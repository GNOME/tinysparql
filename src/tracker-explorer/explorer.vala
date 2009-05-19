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

public class HistoryItem {
	public string uri;
	public HistoryItem? next = null;
	public HistoryItem? prev = null;
}

public class History {
	private HistoryItem? items = null;
	private HistoryItem? current = null;

	public string? current_uri() {
		if (current) {
			return current.uri;
		} else {
			return null;
		}
	}

	public bool can_go_forward() {
		return (current != null) && (current.next != null);
	}

	public bool can_go_back() {
		return (current != null) && (current.prev != null);
	}

	public void forward() {
		if (can_go_forward()) {
			current = current.next;
		}
	}

	public void back() {
		if (can_go_back()) {
			current = current.prev;
		}
	}

	public void add(string uri) {
		HistoryItem hi = new HistoryItem();
		if (current == null) {
			items = hi;
			current = items;
		} else {
			current.next = hi;
			hi.prev = current;
		}
		current = hi;
	}
}

public class Explorer {

	private const string UI_FILE = "explorer.ui";
	private Resources tracker;
	private History history = new History();
	private ListStore uris;
	private ListStore relationships;
	private Label current_uri_label;
	private Gee.HashMap<string,string> namespaces = new Gee.HashMap<string,string>(str_hash, str_equal, str_equal);
	private Notebook types;


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

		current_uri_label = builder.get_object ("current-object") as Label;

		types = builder.get_object ("types") as Notebook;

		types.switch_page += update_types_page;

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
		relationshipsview.row_activated += (view, path, column) => { row_selected(view, path, column, 1);};
	}

	private TreeView setup_reverserelationships() {
		ListStore reverserelationships = new ListStore (2, typeof(string), typeof(string));
		TreeView reverserelationshipsview = new TreeView.with_model (reverserelationships);
		reverserelationshipsview.set_model(reverserelationships);

		reverserelationshipsview.insert_column_with_attributes (-1, "Subject", new CellRendererText (), "text", 0, null);
		reverserelationshipsview.insert_column_with_attributes (-1, "Relationship", new CellRendererText (), "text", 1, null);
		reverserelationshipsview.row_activated += (view, path, column) => { row_selected(view, path, column, 0);};

		return reverserelationshipsview;
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
			relationship = string.join(":", prefix, parts[1]);
		} else {
			relationship = uri;
		} return relationship;
	}

	private void clear_types() {
		int npages = types.get_n_pages();
		for (int i = 0; i < npages; i++) {
			debug ("removeing page %d", i);
			types.remove_page (0);
		}
	}


	private void update_types_page(void *page, uint _page_num) {

		debug ("update_types_page: %u", _page_num);
		int page_num = (int) _page_num;
		ScrolledWindow sw = types.get_nth_page(page_num) as ScrolledWindow;
		string type = (types.get_tab_label(sw) as Label).get_text();

		ListStore model = (sw.get_child() as TreeView).get_model() as ListStore;
		try {
			var query = "SELECT ?r WHERE { ?r rdfs:range %s }".printf(type);
			var result = tracker.SparqlQuery(query);

			model.clear();
			TreeIter iter;

			for (int i=0; i<result.length[0]; i++) {
				var relation = subst_prefix(result[i,0]);
				var query2 = "SELECT ?s WHERE { ?s %s <%s>}".printf(relation, current_uri);
				var result2 = tracker.SparqlQuery(query2);

				for (int j=0; j<result2.length[0]; j++) {
					var subject = subst_prefix(result2[j,0]);
					model.append (out iter);
					model.set (iter, 0, subject, -1);
					model.set (iter, 1, relation, -1);
				}

			}
		} catch (DBus.Error e) {
		}
	}

	private void add_type(string type) {
		Label tab_label = new Label(type);
		ScrolledWindow child = new ScrolledWindow(null, null);
		TreeView tv = setup_reverserelationships();
		child.add(tv);
		types.prepend_page(child, tab_label);
		child.show_all();
	}

	private void update_pane(string uri) {
		current_uri = uri;
		current_uri_label.set_text (uri);
		try {
			string query = "SELECT ?r ?o  WHERE { <%s> ?r ?o }".printf(uri);
			TreeIter iter;
			var result = tracker.SparqlQuery(query);
			relationships.clear();
			clear_types();

			for (int i=0; i<result.length[0]; i++) {
				var relationship = subst_prefix(result[i,0]);
				var obj = subst_prefix(result[i,1]);
				relationships.append (out iter);
				relationships.set (iter, 0, relationship, -1);
				relationships.set (iter, 1, obj, -1);

				if (relationship == "rdf:type" && obj != "rdfs:Resource") {
					add_type (obj);
				}
			}
			update_types_page(null, 0);

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

	private void row_selected(TreeView view, TreePath path, TreeViewColumn column, int index) {
		TreeIter iter;
		var model = view.get_model();
		model.get_iter(out iter, path);
		weak string uri;
		model.get (iter, index, out uri);
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

