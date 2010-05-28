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
	private ToolButton view_list;
	private ToolButton view_icons;
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
		window.destroy += Gtk.main_quit;

		back = builder.get_object ("toolbutton_back") as ToolButton;
		back.clicked += back_clicked;
		back.set_sensitive(false);

		forward = builder.get_object ("toolbutton_forward") as ToolButton;
		forward.clicked += forward_clicked;
		forward.set_sensitive(false);

		view_list = builder.get_object ("toolbutton_view_list") as ToolButton;
		view_list.clicked += view_list_clicked;

		// The default
		view_icons = builder.get_object ("toolbutton_view_icons") as ToolButton;
		view_icons.clicked += view_icons_clicked;

		search = builder.get_object ("entry_search") as Entry;
		search.changed += search_changed;

		sw_treeview = builder.get_object ("scrolledwindow_treeview") as ScrolledWindow;
		treeview = builder.get_object ("treeview_results") as TreeView;
		sw_iconview = builder.get_object ("scrolledwindow_iconview") as ScrolledWindow;
		iconview = builder.get_object ("iconview_results") as IconView;
		setup_ui_results (treeview, iconview);

		sw_iconview.show_all ();
		sw_treeview.hide ();
	}

	private void setup_ui_results (TreeView treeview, IconView iconview) {
		// Setup treeview
		store = new ListStore (5,
							   typeof (Gdk.Pixbuf),  // Icon
							   typeof (string),      // URN
							   typeof (string),      // URL
							   typeof (string),      // Filename
							   typeof (string));     // Description
		treeview.set_model (store);

		// view.insert_column_with_attributes (-1, "URN", new CellRendererText (), "text", 0, null);
		treeview.insert_column_with_attributes (-1, "", new CellRendererPixbuf (), "pixbuf", 0, null);
		treeview.insert_column_with_attributes (-1, "Filename", new CellRendererText (), "text", 3, null);
		treeview.row_activated += view_row_selected;

		// Setup iconview
		iconview.set_model (store);
		iconview.set_item_width (96);
		iconview.set_selection_mode (Gtk.SelectionMode.SINGLE);
		iconview.set_pixbuf_column (0);
		iconview.set_text_column (3);
		//iconview.row_activated += view_row_selected;
	}

	private void search_changed (Entry entry) {
		if (last_search_id != 0) {
			Source.remove (last_search_id);
		}

		last_search_id = Timeout.add_seconds (1, search_run);
	}

	private bool search_run () {
		// Need to escape this string
		string query;

		query = "SELECT ?u nie:url(?u) nfo:fileName(?u) tracker:coalesce(nie:title(?u), \"Unknown\") WHERE { ?u fts:match \"%s\" } ORDER BY DESC(fts:rank(?u)) OFFSET 0 LIMIT 100".printf ((search).text);
		debug ("Query:'%s'", query);

		try {
			var result = tracker.SparqlQuery (query);

			store.clear ();

			var screen = window.get_screen ();
			var theme = IconTheme.get_for_screen (screen);

			for (int i = 0; i < result.length[0]; i++) {
				debug ("--> %s", result[i,0]);
				debug ("  --> %s", result[i,1]);
				debug ("  --> %s", result[i,2]);

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
								//var names = ((ThemedIcon) icon).get_names ();

								// See bug #618574
								// var icon_info = theme.choose_icon (names, 48, IconLookupFlags.USE_BUILTIN);

								// if (icon_info != null) {
									// try {
									// 	pixbuf = icon_info.load_icon ();
									// } catch (GLib.Error e) {
										// Do something
									// }
								// }
							}
						}
					} catch (GLib.Error e) {
						// Do something
					}
				}

				if (pixbuf == null) {
					try {
						// pixbuf = theme.load_icon (theme.get_example_icon_name (), 48, IconLookupFlags.USE_BUILTIN);
						pixbuf = theme.load_icon ("text-x-generic", 48, IconLookupFlags.USE_BUILTIN);
					} catch (GLib.Error e) {
						// Do something
					}
				}

				// Insert into model
				TreeIter iter;

				store.append (out iter);
				store.set (iter, 
						   0, pixbuf,
						   1, result[i,0], 
						   2, result[i,1], 
						   3, result[i,2], 
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

	private void view_list_clicked () {
		sw_iconview.hide ();
		sw_treeview.show_all ();
	}

	private void view_icons_clicked () {
		sw_iconview.show_all ();
		sw_treeview.hide ();
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

