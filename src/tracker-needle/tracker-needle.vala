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

// Added to fix #error for GETTEXT_PACKAGE
private const string a = Config.APPNAME;

[DBus (name = "org.freedesktop.Tracker1.Resources")]
interface Resources : GLib.Object {
	public abstract string[,] SparqlQuery (string query) throws DBus.Error;
}

public class TrackerNeedle {
	private const string UI_FILE = "tracker-needle.ui";
	private Resources tracker;
	private Window window;
	private ToggleToolButton view_list;
	private ToggleToolButton view_icons;
	private ToggleToolButton find_in_contents;
	private ToggleToolButton find_in_titles;
	private Entry search;
	private ToolButton show_stats;
	private ScrolledWindow sw_treeview;
	private TreeView treeview;
	private ScrolledWindow sw_iconview;
	private IconView iconview;
	private uint last_search_id = 0;
	private ListStore store;
    static bool current_view = true;
    static bool current_find_in = true;

	private const int secs_per_day = 60 * 60 * 24;

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

		view_list = builder.get_object ("toolbutton_view_list") as ToggleToolButton;
		view_list.toggled.connect (view_toggled);

		view_icons = builder.get_object ("toolbutton_view_icons") as ToggleToolButton;
		view_icons.toggled.connect (view_toggled);

		find_in_contents = builder.get_object ("toolbutton_find_in_contents") as ToggleToolButton;
		find_in_contents.toggled.connect (find_in_toggled);

		find_in_titles = builder.get_object ("toolbutton_find_in_titles") as ToggleToolButton;
		find_in_titles.toggled.connect (find_in_toggled);

		search = builder.get_object ("entry_search") as Entry;
		search.changed.connect (search_changed);

		show_stats = builder.get_object ("toolbutton_show_stats") as ToolButton;
		show_stats.clicked.connect (show_stats_clicked);

		sw_treeview = builder.get_object ("scrolledwindow_treeview") as ScrolledWindow;
		treeview = builder.get_object ("treeview_results") as TreeView;
		sw_iconview = builder.get_object ("scrolledwindow_iconview") as ScrolledWindow;
		iconview = builder.get_object ("iconview_results") as IconView;
		setup_ui_results (treeview, iconview);

		view_list.set_active (true);
	}

	private void setup_ui_results (TreeView treeview, IconView iconview) {
		// Setup treeview
		store = new ListStore (8,
		                       typeof (Gdk.Pixbuf),  // Icon small
		                       typeof (Gdk.Pixbuf),  // Icon big
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
		col.add_attribute (renderer2, "text", 4);

		col.set_title ("File");
		col.set_resizable (true);
		col.set_expand (true);
		col.set_sizing (Gtk.TreeViewColumnSizing.AUTOSIZE);
		treeview.append_column (col);

		treeview.insert_column_with_attributes (-1, "Last Changed", new CellRendererText (), "text", 5, null);
		treeview.insert_column_with_attributes (-1, "Size", new CellRendererText (), "text", 6, null);
		treeview.row_activated.connect (view_row_selected);

		// Setup iconview
		iconview.set_model (store);
		iconview.set_item_width (96);
		iconview.set_selection_mode (Gtk.SelectionMode.SINGLE);
		iconview.set_pixbuf_column (1);
		iconview.set_text_column (4);
		//iconview.row_activated += view_row_selected;
	}

	private void search_changed (Editable editable) {
		if (last_search_id != 0) {
			Source.remove (last_search_id);
		}

		last_search_id = Timeout.add_seconds (1, search_run);
	}

	private Gdk.Pixbuf item_get_pixbuf (IconTheme theme, string filename, int size) {
		// Get Icon
		var file = File.new_for_uri (filename);
		var pixbuf = null as Gdk.Pixbuf;

		if (file.query_exists (null)) {
			try {
				var file_info = file.query_info ("standard::icon",
				                                 FileQueryInfoFlags.NONE,
				                                 null);

				if (file_info != null) {
					var icon = file_info.get_icon ();

					try {
						if (icon is FileIcon) {
							pixbuf = new Gdk.Pixbuf.from_file (((FileIcon) icon).get_file ().get_path ());
						} else if (icon is ThemedIcon) {
							pixbuf = theme.load_icon (((ThemedIcon) icon).get_names ()[0], size, Gtk.IconLookupFlags.USE_BUILTIN);
						}
					} catch (GLib.Error e) {
						warning ("Error loading icon pixbuf: " + e.message);
					}
				}
			} catch (GLib.Error e) {
				warning ("Error looking up file for pixbuf: " + e.message);
			}
		}

		if (pixbuf == null) {
			try {
				// pixbuf = theme.load_icon (theme.get_example_icon_name (), 48, IconLookupFlags.USE_BUILTIN);
				pixbuf = theme.load_icon ("text-x-generic", size, IconLookupFlags.USE_BUILTIN);
			} catch (GLib.Error e) {
				warning ("Could not load default icon pixbuf from theme for 'text-x-generic': " + e.message);
			}
		}

		return pixbuf;
	}

	private string item_get_time (string s) {
		GLib.Time t = GLib.Time ();
		t.strptime (s, "%FT%T");

		var tv_now = GLib.TimeVal ();
		tv_now.get_current_time ();

		var tv_then = GLib.TimeVal ();
		tv_then.from_iso8601 (s);

		var diff_sec = tv_now.tv_sec - tv_then.tv_sec;
		var diff_days = diff_sec / secs_per_day;
		var diff_days_abs = diff_days.abs ();

		// stdout.printf ("timeval now:%ld, then:%ld, diff secs:%ld, diff days:%ld, abs: %ld, seconds per day:%d\n", tv_now.tv_sec, tv_then.tv_sec, diff_sec, diff_days, diff_days_abs, secs_per_day);

		// if it's more than a week, use the default date format
		if (diff_days_abs > 7) {
			return t.format ("%x");
		}

		if (diff_days_abs == 0) {
			return "Today";
		} else {
			bool future = false;

			if (diff_days < 0)
				future = true;

			if (diff_days <= 1) {
				if (future)
					return "Tomorrow";
				else
					return "Yesterday";
			} else {
				if (future) {
					/* Translators: %d is replaced with a number of days. It's always greater than 1 */
					return ngettext ("%ld day from now", "%ld days from now", diff_days_abs).printf (diff_days_abs);
				} else {
					/* Translators: %d is replaced with a number of days. It's always greater than 1 */
					return ngettext ("%ld day ago", "%ld days ago", diff_days_abs).printf (diff_days_abs);
				}
			}
		}
	}

	private bool search_run () {
		// Need to escape this string
		string query;
		string criteria;

		criteria = search.get_text ();

		if (criteria.length < 1) {
			last_search_id = 0;
			return false;
		}

		if (find_in_contents.active) {
			query = @"SELECT ?u nie:url(?u) tracker:coalesce(nie:title(?u), nfo:fileName(?u), \"Unknown\") nfo:fileLastModified(?u) nfo:fileSize(?u) nie:url(?c) WHERE { ?u fts:match \"$criteria\" . ?u nfo:belongsToContainer ?c } ORDER BY DESC(fts:rank(?u)) OFFSET 0 LIMIT 100";
		} else {
			query = @"SELECT ?u nie:url(?u) tracker:coalesce(nfo:fileName(?u), \"Unknown\") nfo:fileLastModified(?u) nfo:fileSize(?u) nie:url(?c) WHERE { ?u a nfo:FileDataObject ; nfo:belongsToContainer ?c . FILTER(fn:contains(nfo:fileName(?u), \"$criteria\")) } ORDER BY DESC(nfo:fileName(?u)) OFFSET 0 LIMIT 100";
		}

		debug ("Query:'%s'", query);

		try {
			var result = tracker.SparqlQuery (query);

			store.clear ();

			var screen = window.get_screen ();
			var theme = IconTheme.get_for_screen (screen);

			var size_small = 0;
			Gtk.icon_size_lookup (Gtk.IconSize.DND, out size_small, null);

			var size_big = 0;
			Gtk.icon_size_lookup (Gtk.IconSize.DIALOG, out size_big, null);

			for (int i = 0; i < result.length[0]; i++) {
				debug ("--> %s", result[i,0]);
				debug ("  --> %s", result[i,1]);
				debug ("  --> %s", result[i,2]);
				debug ("  --> %s", result[i,3]);
				debug ("  --> %s", result[i,4]);

				// Get icon
				Gdk.Pixbuf pixbuf_small = item_get_pixbuf (theme, result[i,1], size_small);
				Gdk.Pixbuf pixbuf_big = item_get_pixbuf (theme, result[i,1], size_big);
				string file_size = GLib.format_size_for_display (result[i,4].to_int());
				string file_time = item_get_time (result[i,3]);

				// Insert into model
				TreeIter iter;
				store.append (out iter);

				// FIXME: should optimise this a bit more, inserting 2 images into a list eek
				store.set (iter,
				           0, pixbuf_small,
				           1, pixbuf_big,
				           2, result[i,0],
				           3, result[i,1],
				           4, result[i,2],
				           5, file_time,
				           6, file_size,
				           7, result[i,5],
				           -1);
			}
		} catch (DBus.Error e) {
			warning ("Could not run SPARQL query: " + e.message);
		}

		last_search_id = 0;

		return false;
	}

	private void view_toggled () {
	    if (current_view == view_list.active) {
	        return;	    
	    }

		if (view_list.active) {
			sw_iconview.hide ();
			sw_treeview.show_all ();
			debug ("View toggled to 'list'");
		} else {
			sw_iconview.show_all ();
			sw_treeview.hide ();
			debug ("View toggled to 'icons'");
		}

		current_view = view_list.active;
	}

	private void find_in_toggled () {    
	    if (current_find_in == find_in_contents.active) {
	        return;	    
	    }
	    
		if (find_in_contents.active) {
			debug ("Find in toggled to 'contents'");
			search_run ();
		} else {
			debug ("Find in toggled to 'titles'");
			search_run ();
		}
		
		current_find_in = find_in_contents.active;
	}

	private void view_row_selected (TreeView view, TreePath path, TreeViewColumn column) {
		TreeIter iter;

		var model = view.get_model ();
		model.get_iter (out iter, path);

		weak string filename;
		model.get (iter, 1, out filename);

		debug ("Selected filename:'%s'", filename);
	}
	
	private void show_stats_clicked () {
	    debug ("Showing stats dialog");
		TrackerStats s = new TrackerStats ();
		s.show ();
	}
}

static int main (string[] args) {
	Gtk.init (ref args);

	Intl.bindtextdomain (Config.GETTEXT_PACKAGE, Config.LOCALEDIR);
	Intl.bind_textdomain_codeset (Config.GETTEXT_PACKAGE, "UTF-8");
	Intl.textdomain (Config.GETTEXT_PACKAGE);

	TrackerNeedle n = new TrackerNeedle ();
	n.show();
	Gtk.main ();

	return 0;
}
