//
// Copyright 2010, Martyn Russell <martyn@lanedo.com>
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

public class Needle {
	private const string UI_FILE = "needle.ui";
	private Resources tracker;
	private Window window;
	private ToolButton back;
	private ToolButton forward;
	private ToggleToolButton view_list;
	private ToggleToolButton view_icons;
	private Entry search;
	private ScrolledWindow sw_treeview;
	private TreeView treeview;
	private ScrolledWindow sw_iconview;
	private IconView iconview;
	private uint last_search_id = 0;
	private ListStore store;

	public void show () {
		setup_dbus ();
		setup_ui ();

		window.show ();
	}

	private void setup_dbus () {
		try {
			var conn = DBus.Bus.get (DBus.BusType.SESSION);
			tracker = (Resources) conn.get_object ("org.freedesktop.Tracker1",
												   "/org/freedesktop/Tracker1/Resources",
												   "org.freedesktop.Tracker1.Resources");
		} catch (DBus.Error e) {
			var msg = new MessageDialog (null,
										 DialogFlags.MODAL,
										 MessageType.ERROR,
										 ButtonsType.CANCEL,
										 "Error connecting to D-Bus session bus, %s", 
										 e.message);
			msg.run ();
			Gtk.main_quit ();
		}
	}

	private void setup_ui () {
		var builder = new Builder ();

		try {
			//try load from source tree first.
			builder.add_from_file (SRCDIR + UI_FILE);
		} catch (GLib.Error e) {
			//now the install location
			try {
				builder.add_from_file (UIDIR + UI_FILE);
			} catch (GLib.Error e) {
				var msg = new MessageDialog (null, 
											 DialogFlags.MODAL,
											 MessageType.ERROR,
											 ButtonsType.CANCEL,
											 "Failed to load UI file, %s\n",
											 e.message);
				msg.run ();
				Gtk.main_quit();
			}
		}

		window = builder.get_object ("window_needle") as Window;
		window.destroy.connect (Gtk.main_quit);

		back = builder.get_object ("toolbutton_back") as ToolButton;
		back.clicked.connect (back_clicked);
		back.set_sensitive (false);

		forward = builder.get_object ("toolbutton_forward") as ToolButton;
		forward.clicked.connect (forward_clicked);
		forward.set_sensitive (false);

		view_list = builder.get_object ("toolbutton_view_list") as ToggleToolButton;
		view_list.toggled.connect (view_toggled);

		view_icons = builder.get_object ("toolbutton_view_icons") as ToggleToolButton;
		view_icons.toggled.connect (view_toggled);

		search = builder.get_object ("entry_search") as Entry;
		search.changed.connect (search_changed);

		sw_treeview = builder.get_object ("scrolledwindow_treeview") as ScrolledWindow;
		treeview = builder.get_object ("treeview_results") as TreeView;
		sw_iconview = builder.get_object ("scrolledwindow_iconview") as ScrolledWindow;
		iconview = builder.get_object ("iconview_results") as IconView;
		setup_ui_results (treeview, iconview);

		view_list.set_active (true);
	}

	private void setup_ui_results (TreeView treeview, IconView iconview) {
		// Setup treeview
		store = new ListStore (7,
							   typeof (Gdk.Pixbuf),  // Icon
							   typeof (string),      // URN
							   typeof (string),      // URL
							   typeof (string),      // File name
							   typeof (string),      // File last changed
							   typeof (string),      // File size
							   typeof (string));     // Tooltip
		treeview.set_model (store);

	    var col = new Gtk.TreeViewColumn ();

		var renderer1 = new CellRendererPixbuf ();
    	col.pack_start (renderer1, false);
	    col.add_attribute (renderer1, "pixbuf", 0);

		var renderer2 = new CellRendererText ();
    	col.pack_start (renderer2, true);
	    col.add_attribute (renderer2, "text", 3);

	    col.set_title ("File");
	    col.set_resizable (true);
	    col.set_expand (true);
	    col.set_sizing (Gtk.TreeViewColumnSizing.AUTOSIZE);
	    treeview.append_column (col);   
	    
		treeview.insert_column_with_attributes (-1, "Last Changed", new CellRendererText (), "text", 4, null);
		treeview.insert_column_with_attributes (-1, "Size", new CellRendererText (), "text", 5, null);
		treeview.row_activated.connect (view_row_selected);

		// Setup iconview
		iconview.set_model (store);
		iconview.set_item_width (96);
		iconview.set_selection_mode (Gtk.SelectionMode.SINGLE);
		iconview.set_pixbuf_column (0);
		iconview.set_text_column (3);
		//iconview.row_activated += view_row_selected;
	}

	private void search_changed (Editable editable) {
		if (last_search_id != 0) {
			Source.remove (last_search_id);
		}

		last_search_id = Timeout.add_seconds (1, search_run);
	}

	private bool search_run () {
		// Need to escape this string
		string query;

		query = "SELECT ?u nie:url(?u) tracker:coalesce(nie:title(?u), nfo:fileName(?u), \"Unknown\") nfo:fileLastModified(?u) nfo:fileSize(?u) nie:url(?c) WHERE { ?u fts:match \"%s\" . ?u nfo:belongsToContainer ?c } ORDER BY DESC(fts:rank(?u)) OFFSET 0 LIMIT 100".printf ((search).text);
		debug ("Query:'%s'", query);

		try {
			var result = tracker.SparqlQuery (query);

			store.clear ();

			var screen = window.get_screen ();
			var theme = IconTheme.get_for_screen (screen);
			var size = 24;

			for (int i = 0; i < result.length[0]; i++) {
				debug ("--> %s", result[i,0]);
				debug ("  --> %s", result[i,1]);
				debug ("  --> %s", result[i,2]);
				debug ("  --> %s", result[i,3]);
				debug ("  --> %s", result[i,4]);

				// Get Icon
				var file = File.new_for_uri (result[i,1]);
				var pixbuf = null as Gdk.Pixbuf;

				if (file.query_exists (null)) {
					try {
						var file_info = file.query_info ("standard::icon", 
														 FileQueryInfoFlags.NONE, 
														 null);

						if (file_info != null) {
							var icon = file_info.get_icon ();

							if (icon != null) {
								var names = ((ThemedIcon) icon).get_names ();
								
								// See bug #618574
								var icon_info = theme.choose_icon (names, size, IconLookupFlags.USE_BUILTIN);

								if (icon_info != null) {
									try {
										pixbuf = icon_info.load_icon ();
									} catch (GLib.Error e) {
										// Do something
									}
								}
							}
						}
					} catch (GLib.Error e) {
						// Do something
					}
				}

				if (pixbuf == null) {
					try {
						// pixbuf = theme.load_icon (theme.get_example_icon_name (), 48, IconLookupFlags.USE_BUILTIN);
						pixbuf = theme.load_icon ("text-x-generic", size, IconLookupFlags.USE_BUILTIN);
					} catch (GLib.Error e) {
						// Do something
					}
				}

				// Insert into model
				TreeIter iter;
				string file_size = GLib.format_size_for_display (result[i,4].to_int());
								
				store.append (out iter);
				store.set (iter, 
						   0, pixbuf,
						   1, result[i,0],
						   2, result[i,1],
						   3, result[i,2],
						   4, result[i,3],
						   5, file_size, 
						   6, result[i,5],
						   -1);
			}
		} catch (DBus.Error e) {
			// Do nothing
		}

		last_search_id = 0;

		return false;
	}

	private void forward_clicked () {
		// Do nothing
	}

	private void back_clicked () {
		// Do nothing
	}

	private void view_toggled () {
		if (view_list.active) {
			sw_iconview.hide ();
			sw_treeview.show_all ();
		} else {
			sw_iconview.show_all ();
			sw_treeview.hide ();
		}
	}

	private void view_row_selected (TreeView view, TreePath path, TreeViewColumn column) {
		TreeIter iter;

		var model = view.get_model ();
		model.get_iter (out iter, path);

		weak string filename;
		model.get (iter, 1, out filename);
		
		debug ("Selected filename:'%s'", filename);
	}
}

static int main (string[] args) {
	Gtk.init (ref args);

	Needle n = new Needle();
	n.show();
	Gtk.main ();

	return 0;
}

