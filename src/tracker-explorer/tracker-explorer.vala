//
// Copyright 2009, Rob Taylor <rob.taylor@codethink.co.uk>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
// 02110-1301, USA.
//

using Gtk;

[CCode (cname = "TRACKER_UI_DIR")]
extern static const string UIDIR;

[CCode (cname = "SRCDIR")]
extern static const string SRCDIR;

[DBus (name = "org.freedesktop.Tracker1.Resources")]
interface Resources : GLib.Object {
	public abstract string[,] SparqlQuery (string query) throws DBus.Error;
}

public class HistoryItem {
	public HistoryItem (string uri) {
		this.uri = uri;
	}
	public string uri;
	public HistoryItem? next = null;
	public HistoryItem? prev = null;
}

public class History {
	private HistoryItem? items = null;
	private HistoryItem? current = null;

	public string? current_uri() {
		if (current != null) {
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

	public bool forward() {
		if (can_go_forward()) {
			current = current.next;
			return true;
		}
		return false;
	}

	public bool back() {
		if (can_go_back()) {
			current = current.prev;
			return true;
		}
		return false;
	}

	public void add(string uri) {
		HistoryItem hi = new HistoryItem(uri);
		if (current == null) {
			items = hi;
			current = items;
		} else {
			current.next = hi;
			hi.prev = current;
		}
		current = hi;

		//debug ("history.add current=%p, next=%p, prev=%p, uri = %s", current, current.next, current.prev, current.uri);
	}
}

public class Explorer {

	private const string UI_FILE = "tracker-explorer.ui";
	private Resources tracker;
	private History history = new History();
	private ListStore uris;
	private ListStore relationships;
	private Label current_uri_label;
	private Gee.HashMap<string,string> namespaces = new Gee.HashMap<string,string>(str_hash, str_equal, str_equal);
	private Notebook types;
	private Button forward;
	private Button back;


	public void show() {

		try {
			var conn = DBus.Bus.get (DBus.BusType.SESSION);
			tracker = (Resources) conn.get_object ("org.freedesktop.Tracker1",
							       "/org/freedesktop/Tracker1/Resources",
							       "org.freedesktop.Tracker1.Resources");
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
		window.destroy.connect (Gtk.main_quit);

		var entry = builder.get_object ("text-search") as Entry;
		entry.changed.connect (entry_changed);

		var urisview = builder.get_object ("uris") as TreeView;
		setup_uris(urisview);

		var relationshipsview = builder.get_object ("relationshipsview") as TreeView;
		setup_relationships(relationshipsview);

		current_uri_label = builder.get_object ("current-object") as Label;

		types = builder.get_object ("types") as Notebook;

		types.set_focus_child.connect (update_types_page);

		forward = builder.get_object("forward") as Button;
		forward.clicked.connect (forward_clicked);
		forward.set_sensitive(false);

		back = builder.get_object("back") as Button;
		back.clicked.connect (back_clicked);
		back.set_sensitive(false);

		fetch_prefixes();

		window.show_all();
	}

	private void setup_uris (TreeView urisview) {
		uris = new ListStore (1, typeof (string));
		urisview.set_model (uris);

		urisview.insert_column_with_attributes (-1, "URI", new CellRendererText (), "text", 0, null);
		urisview.row_activated.connect (row_selected);
	}

	private void setup_relationships(TreeView relationshipsview) {
		relationships = new ListStore (3, typeof(string), typeof(string), typeof(string)); //select uri, relationship, object
		relationshipsview.set_model(relationships);

		relationshipsview.insert_column_with_attributes (-1, "Relationship", new CellRendererText (), "text", 1, null);
		relationshipsview.insert_column_with_attributes (-1, "Object", new CellRendererText (), "text", 2, null);
		relationshipsview.row_activated.connect (row_selected);
	}

	private TreeView setup_reverserelationships() {
		// select uri, subject, relationship
		ListStore reverserelationships = new ListStore (3, typeof(string), typeof(string), typeof(string));

		TreeView reverserelationshipsview = new TreeView.with_model (reverserelationships);
		reverserelationshipsview.set_model(reverserelationships);

		reverserelationshipsview.insert_column_with_attributes (-1, "Subject", new CellRendererText (), "text", 1, null);
		reverserelationshipsview.insert_column_with_attributes (-1, "Relationship", new CellRendererText (), "text", 2, null);
		reverserelationshipsview.row_activated.connect (row_selected);

		return reverserelationshipsview;
	}


	private void fetch_prefixes () {
		string query = "SELECT ?s ?prefix WHERE { ?s a tracker:Namespace ; tracker:prefix ?prefix }";
		try {
			var result = tracker.SparqlQuery(query);
			for (int i=0; i<result.length[0]; i++) {
				string _namespace = result[i,0];
				_namespace = _namespace.substring(0, _namespace.length -1);
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
			//debug ("removing page %d", i);
			types.remove_page (0);
		}
	}


	private void update_types_page(Widget ?w) {
		int page_num = types.get_current_page();
		if (page_num < 0) {
			return;
		}
		//debug ("update_types_page: %d", page_num);
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
				var query2 = "SELECT ?s WHERE { ?s %s <%s>}".printf(relation, history.current_uri());
				var result2 = tracker.SparqlQuery(query2);

				for (int j=0; j<result2.length[0]; j++) {
					var subject = subst_prefix(result2[j,0]);
					model.append (out iter);
					model.set (iter, 0, result2[j,0], -1);
					model.set (iter, 1, subject, -1);
					model.set (iter, 2, relation, -1);
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
		types.append_page(child, tab_label);
		child.show_all();
	}

	private void set_current_uri(string uri) {
		history.add(uri);
		update_pane();
	}

	private void forward_clicked() {
		if (history.forward()) {
			update_pane();
		}
	}

	private void back_clicked() {
		if (history.back()) {
			update_pane();
		}
	}

	private void update_pane() {
		forward.set_sensitive(history.can_go_forward());
		back.set_sensitive(history.can_go_back());
		current_uri_label.set_text (subst_prefix(history.current_uri()));
		try {
			var query = "SELECT ?r ?o  WHERE { <%s> ?r ?o }".printf(history.current_uri());
			TreeIter iter;
			var result = tracker.SparqlQuery(query);
			relationships.clear();
			clear_types();

			for (int i=0; i<result.length[0]; i++) {
				var relationship = subst_prefix(result[i,0]);
				var obj = subst_prefix(result[i,1]);
				relationships.append (out iter);
				relationships.set (iter, 0, result[i,1], -1);
				relationships.set (iter, 1, relationship, -1);
				relationships.set (iter, 2, obj, -1);

				if (relationship == "rdf:type" && obj != "rdfs:Resource") {
					add_type (obj);
				}
			}
			types.set_current_page(types.get_n_pages() - 1);
			update_types_page(null);

		} catch (DBus.Error e) {
		}
	}

	private void row_selected(TreeView view, TreePath path, TreeViewColumn column) {
		TreeIter iter;
		var model = view.get_model();
		model.get_iter(out iter, path);
		weak string uri;
		model.get (iter, 0, out uri);
		set_current_uri(uri);
	}


}



static int main (string[] args) {
	Gtk.init (ref args);

	Explorer s = new Explorer();
	s.show();
	Gtk.main ();
	return 0;
}

