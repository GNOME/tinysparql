using Gtk;

[DBus (name = "org.freedesktop.Tracker.Resources")]
interface Resources : GLib.Object {
	public abstract void Delete (string subject, string predicate, string object_) throws DBus.Error;
	public abstract void Insert (string subject, string predicate, string object_) throws DBus.Error;
	public abstract void Load (string url) throws DBus.Error;
	public abstract string[,] SparqlQuery (string query) throws DBus.Error;
	public abstract void SparqlUpdate (string query) throws DBus.Error;
}

public class Explorer {

	private Resources tracker;
	private ListStore listmodel;

	public Explorer() {
		var conn = DBus.Bus.get (DBus.BusType.SESSION);
		tracker = (Resources) conn.get_object ("org.freedesktop.Tracker",
						       "/org/freedesktop/Tracker/Resources",
						       "org.freedesktop.Tracker.Resources");
	}

	public void setup() {
		var window = new Window (WindowType.TOPLEVEL);
		window.title = "Tracker Explorer";
		window.set_size_request (300, 400);
		window.position = WindowPosition.CENTER;
		window.destroy += Gtk.main_quit;

		var vbox = new VBox(false, 0);
		window.add(vbox);

		var entry = new Entry();
		entry.set_text ("Test");
		entry.changed += entry_changed;
		vbox.pack_start(entry, false, false, 0);

		var treeview = new TreeView();
		setup_treeview(treeview);

		var scrolled_window = new ScrolledWindow(null,null);
		scrolled_window.set_policy (PolicyType.AUTOMATIC, PolicyType.AUTOMATIC);
		scrolled_window.add_with_viewport(treeview);

		vbox.pack_start(scrolled_window, true, true, 0);

		window.show_all ();
		window.destroy += Gtk.main_quit;
	}

	private void setup_treeview (TreeView view) {
		listmodel = new ListStore (1, typeof (string));
		view.set_model (listmodel);

		view.insert_column_with_attributes (-1, "URI", new CellRendererText (), "text", 0, null);
/*
		var cell = new CellRendererText ();
		cell.set ("foreground_set", true, null);
		view.insert_column_with_attributes (-1, "Balance", cell, "text", 2, "foreground", 3, null);

		TreeIter iter;
		listmodel.append (out iter);
		listmodel.set (iter, 0, "My Visacard", 1, "card", 2, "102,10", 3, "red", -1);

		listmodel.append (out iter);
		listmodel.set (iter, 0, "My Mastercard", 1, "card", 2, "10,20", 3, "red", -1);
*/
	}


	private void entry_changed (Editable editable) {
		debug ("changed");
		string query = "SELECT ?s WHERE { ?s fts:match \"%s*\" }".printf(((Entry)editable).text);
		debug ("Query: %s", query);

		try {
			var result = tracker.SparqlQuery(query);
			listmodel.clear();
			foreach ( var s in result) {
				debug ("%s", s);
				TreeIter iter;
				listmodel.append (out iter);
				listmodel.set (iter, 0, s, -1);
			}

		} catch (DBus.Error e) {
		}
	}


	static int main (string[] args) {
		Gtk.init (ref args);

		Explorer e = new Explorer();
		e.setup();
		Gtk.main ();
		return 0;
	}
}
