using Gtk;

[CCode (cname = "TRACKER_UI_DIR")]
extern static const string UIDIR;

[CCode (cname = "SRCDIR")]
extern static const string SRCDIR;

[DBus (name = "org.freedesktop.Tracker.Resources")]
interface Resources : GLib.Object {
	public abstract void Delete (string subject, string predicate, string object_) throws DBus.Error;
	public abstract void Insert (string subject, string predicate, string object_) throws DBus.Error;
	public abstract void Load (string url) throws DBus.Error;
	public abstract string[,] SparqlQuery (string query) throws DBus.Error;
	public abstract void SparqlUpdate (string query) throws DBus.Error;
}

public class Explorer {

	private const string UI_FILE = "explorer.ui";
	private Resources tracker;
	private ListStore uris;
	private ListStore relationships;
	private Label current_object;
	private ListStore reverserelationships;
	private Label current_object_reverse;
	private Gee.HashMap<string,string> namespaces = new Gee.HashMap<string,string>(str_hash, str_equal, str_equal);

	public void show() {

		try {
			var conn = DBus.Bus.get (DBus.BusType.SESSION);
			tracker = (Resources) conn.get_object ("org.freedesktop.Tracker",
							       "/org/freedesktop/Tracker/Resources",
							       "org.freedesktop.Tracker.Resources");
		} catch (DBus.Error e) {
			var msg = new MessageDialog (null, DialogFlags.MODAL,
					 MessageType.ERROR, ButtonsType.CANCEL,
					 "Error connecting to D-Bus session bus\n%s", e.message);
			msg.run ();
			Gtk.main_quit();
		}


		var builder = new Builder ();

		try {
			//try load from source tree first.
			builder.add_from_file (SRCDIR + UI_FILE);
		} catch (GLib.Error e) {
			//now the install location
			try {
				builder.add_from_file (UIDIR + UI_FILE);
			} catch (GLib.Error e) {
				var msg = new MessageDialog (null, DialogFlags.MODAL,
						 MessageType.ERROR, ButtonsType.CANCEL,
						 "Failed to load UI\n%s", e.message);
				msg.run ();
				Gtk.main_quit();
			}
		}

		var window = builder.get_object ("explorer") as Window;
		window.destroy += Gtk.main_quit;

		var entry = builder.get_object ("text-search") as Entry;
		entry.changed += entry_changed;

		var urisview = builder.get_object ("uris") as TreeView;
		setup_uris(urisview);

		var relationshipsview = builder.get_object ("relationshipsview") as TreeView;
		setup_relationships(relationshipsview);

		current_object = builder.get_object ("current-object") as Label;
		var reverserelationshipsview = builder.get_object ("reverserelationshipsview") as TreeView;
		setup_reverserelationships(reverserelationshipsview);

		current_object_reverse = builder.get_object ("current-object-reverse") as Label;

		fetch_prefixes();

		window.show_all();
	}

	private void setup_uris (TreeView urisview) {
		uris = new ListStore (1, typeof (string));
		urisview.set_model (uris);

		urisview.insert_column_with_attributes (-1, "URI", new CellRendererText (), "text", 0, null);
		urisview.row_activated += uri_selected;
	}

	private void setup_relationships(TreeView relationshipsview) {
		relationships = new ListStore (2, typeof(string), typeof(string));
		relationshipsview.set_model(relationships);

		relationshipsview.insert_column_with_attributes (-1, "Relationship", new CellRendererText (), "text", 0, null);
		relationshipsview.insert_column_with_attributes (-1, "Object", new CellRendererText (), "text", 1, null);
		relationshipsview.row_activated += object_selected;
	}

	private void setup_reverserelationships(TreeView reverserelationshipsview) {
		reverserelationships = new ListStore (2, typeof(string), typeof(string));
		reverserelationshipsview.set_model(reverserelationships);

		reverserelationshipsview.insert_column_with_attributes (-1, "Subject", new CellRendererText (), "text", 1, null);
		reverserelationshipsview.insert_column_with_attributes (-1, "Relationship", new CellRendererText (), "text", 0, null);
		reverserelationshipsview.row_activated += object_selected;
	}


	private void fetch_prefixes () {
		string query = "SELECT ?s ?prefix WHERE { ?s a tracker:Namespace ; tracker:prefix ?prefix }";
		try {
			var result = tracker.SparqlQuery(query);
			for (int i=0; i<result.length[0]; i++) {
				string _namespace = result[i,0];
				_namespace = _namespace.substring(0, _namespace.len() -1);
				namespaces[_namespace] = result[i,1];
			}
		} catch (DBus.Error e) {
		}
	}

	private void entry_changed (Editable editable) {
		string query = "SELECT ?s WHERE { ?s fts:match \"%s*\" }".printf(((Entry)editable).text);
		//debug ("Query: %s", query);

		try {
			var result = tracker.SparqlQuery(query);
			uris.clear();
			foreach ( var s in result) {
//				//debug ("%s", s);
				TreeIter iter;
				uris.append (out iter);
				uris.set (iter, 0, s, -1);
			}

		} catch (DBus.Error e) {
		}
	}

	//split at '#' and look up to see if we have
	//a human-readable prefix
	private string subst_prefix(string uri) {
		string[] parts = uri.split("#");
		string? prefix = namespaces[parts[0]];
		string relationship;

		if (prefix != null) {
			relationship = string.join("#", prefix, parts[1]);
		} else {
			relationship = uri;
		}
		return relationship;
	}

	private void update_pane(string uri) {
		//debug ("updating pane: %s", uri);
		current_object.set_text (uri);
		current_object_reverse.set_text (uri);

		try {
			string query = "SELECT ?r ?o  WHERE { <%s> ?r ?o }".printf(uri);
			TreeIter iter;
			var result = tracker.SparqlQuery(query);
			relationships.clear();

			for (int i=0; i<result.length[0]; i++) {
				var relationship = subst_prefix(result[i,0]);
				relationships.append (out iter);
				relationships.set (iter, 0, relationship, -1);
				relationships.set (iter, 1, result[i,1], -1);
			}
/*
			query = "SELECT ?s ?r WHERE { ?s ?r  <%s> }".printf(uri);
			result = tracker.SparqlQuery(query);
			reverserelationships.clear();

			for (int i=0; i<result.length[0]; i++) {
				var relationship = subst_prefix(result[i,1]);
				reverserelationships.append (out iter);
				reverserelationships.set (iter, 0, result[i,0], -1);
				reverserelationships.set (iter, 1, relationship, -1);
			}

*/
		} catch (DBus.Error e) {
		}
	}

	private void uri_selected(TreeView view, TreePath path, TreeViewColumn column) {
		TreeIter iter;
		var model = view.get_model();
		model.get_iter(out iter, path);
		weak string uri;
		model.get (iter, 0, out uri);
		//debug ("uri selected: %s", uri);
		update_pane(uri);
	}

	private void object_selected(TreeView view, TreePath path, TreeViewColumn column) {
		TreeIter iter;
		var model = view.get_model();
		model.get_iter(out iter, path);
		weak string uri;
		model.get (iter, 1, out uri);
		//debug ("object selected: %s", uri);
		update_pane(uri);
	}

}



static int main (string[] args) {
	Gtk.init (ref args);

	Explorer s = new Explorer();
	s.show();
	Gtk.main ();
	return 0;
}

