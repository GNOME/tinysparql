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

public class Tracker.Needle {
	private const string UI_FILE = "tracker-needle.ui";
	private History history;
	private Window window;
	private ToggleToolButton view_categories;
	private ToggleToolButton view_filelist;
	private ToggleToolButton view_icons;
	private SeparatorToolItem separator_secondary;
	private ToggleToolButton find_in_contents;
	private ToggleToolButton find_in_titles;
	private ComboBoxEntry search_list;
	private Entry search;
	private Spinner spinner;
	private ToolItem spinner_shell;
	private ToggleToolButton show_tags;
	private ToolButton show_stats;
	private HBox view;
	private Tracker.View sw_noresults;
	private Tracker.View sw_categories;
	private Tracker.View sw_filelist;
	private Tracker.View sw_icons;
	private Tracker.TagList taglist;
	private uint last_search_id = 0;
	private int size_small = 0;
	private int size_medium = 0;
	private int size_big = 0;
	static bool current_view = true;
	static bool current_find_in = true;

	public Needle () {
		history = new Tracker.History ();
	}

	public void show () {
		setup_ui ();

		window.show ();
	}

	public void set_search (string[]? args) {
		if (args != null) {
			string text = "";

			foreach (string s in args) {
				if (text.length > 1)
					text += " ";

				text += s;
			}

			debug ("Setting search criteria to: '%s'\n", text);
			search.set_text (text);
		}
	}

	private void store_state_changed (GLib.Object object,
	                                  ParamSpec   p) {
		ResultStore store = (ResultStore) object;

		if (store.active) {
			spinner_shell.show_all ();
			spinner.start ();
		} else {
			spinner_shell.hide ();
			spinner.stop ();
		}
	}

	private void setup_ui () {
		var builder = new Gtk.Builder ();

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

		Gtk.icon_size_lookup (Gtk.IconSize.MENU, out size_small, null);
		Gtk.icon_size_lookup (Gtk.IconSize.DND, out size_medium, null);
		Gtk.icon_size_lookup (Gtk.IconSize.DIALOG, out size_big, null);

		window = builder.get_object ("window_needle") as Window;
		window.destroy.connect (Gtk.main_quit);
		window.key_press_event.connect (window_key_press_event);

		view_filelist = builder.get_object ("toolbutton_view_filelist") as ToggleToolButton;
		view_filelist.toggled.connect (view_toggled);

		view_icons = builder.get_object ("toolbutton_view_icons") as ToggleToolButton;
		view_icons.toggled.connect (view_toggled);

		view_categories = builder.get_object ("toolbutton_view_categories") as ToggleToolButton;
		view_categories.toggled.connect (view_toggled);

		separator_secondary = builder.get_object ("separator_secondary") as SeparatorToolItem;

		find_in_contents = builder.get_object ("toolbutton_find_in_contents") as ToggleToolButton;
		find_in_contents.toggled.connect (find_in_toggled);

		find_in_titles = builder.get_object ("toolbutton_find_in_titles") as ToggleToolButton;
		find_in_titles.toggled.connect (find_in_toggled);

		search_list = builder.get_object ("comboboxentry_search") as ComboBoxEntry;
		search = search_list.get_child () as Entry;
		search.changed.connect (search_changed);
		search.activate.connect (search_activated);
		search_history_insert (history.get ());

		spinner = new Spinner ();
		spinner_shell = builder.get_object ("toolcustom_spinner") as ToolItem;
		spinner_shell.add (spinner);

		show_tags = builder.get_object ("toolbutton_show_tags") as ToggleToolButton;
		show_tags.clicked.connect (show_tags_clicked);

		show_stats = builder.get_object ("toolbutton_show_stats") as ToolButton;
		show_stats.clicked.connect (show_stats_clicked);

		view = builder.get_object ("hbox_view") as HBox;

		// Set up views
		TreeView treeview;
		IconView iconview;

		sw_noresults = new Tracker.View (Tracker.View.Display.NO_RESULTS, null);
		view.pack_start (sw_noresults, true, true, 0);

		sw_categories = new Tracker.View (Tracker.View.Display.CATEGORIES, null);
		treeview = (TreeView) sw_categories.get_child ();
		treeview.row_activated.connect (view_row_selected);
		sw_categories.store.notify["active"].connect (store_state_changed);
		view.pack_start (sw_categories, true, true, 0);

		// sw_filelist = new Tracker.View (Tracker.View.Display.FILE_LIST, null);
		// treeview = (TreeView) sw_filelist.get_child ();
		// treeview.row_activated.connect (view_row_selected);
		// view.pack_start (sw_filelist, true, true, 0);

		// sw_icons = new Tracker.View (Tracker.View.Display.FILE_ICONS, null);
		// iconview = (IconView) sw_icons.get_child ();
		// iconview.item_activated.connect (icon_item_selected);
		// view.pack_start (sw_icons, true, true, 0);

		// Set up taglist
		taglist = new Tracker.TagList ();
		taglist.hide ();
		view.pack_end (taglist, false, true, 0);

		view_categories.set_active (true);
	}

	private bool window_key_press_event (Gtk.Widget   widget,
	                                     Gdk.EventKey event) {
		// Add Ctrl+W close window semantics
		if (Gdk.ModifierType.CONTROL_MASK in event.state && Gdk.keyval_name (event.keyval) == "w") {
			widget.destroy();
		}

		return false;
	}

	private ListStore? get_store_for_active_view () {
       		// if (view_icons.active) {
		// 	return sw_icons.store;
		// } else if (view_filelist.active) {
		// 	return sw_filelist.store;
		// } else if (view_categories.active) {
		// 	return sw_categories.store;
		// }

		debug ("No views active to get store?!?!");
		return null;
	}

	private void search_changed (Editable editable) {
		if (last_search_id != 0) {
			Source.remove (last_search_id);
		}

		last_search_id = Timeout.add_seconds (1, search_run);
	}

	private void search_activated (Entry entry) {
		if (last_search_id != 0) {
			Source.remove (last_search_id);
			last_search_id = 0;
		}

		search_run ();
	}

	private async void search_simple (ListStore store) requires (store != null) {
		/*
		Tracker.Query query = new Tracker.Query ();
		Tracker.Sparql.Cursor cursor = null;

		query.limit = 1000;
		query.criteria = search.get_text ();

		debug ("Doing simple search using store:%p", store);

		try {
			if (find_in_contents.active) {
				cursor = yield query.perform_async (query.Type.ALL, null);
			} else {
				cursor = yield query.perform_async (query.Type.ALL_ONLY_IN_TITLES, null);
			}

			if (cursor == null) {
				search_finished (store);
				return;
			}

			store.clear ();

			var screen = window.get_screen ();
			var theme = IconTheme.get_for_screen (screen);

			while (true) {
				bool b = yield cursor.next_async ();
				if (!b) {
					break;
				}

				for (int i = 0; i < cursor.n_columns; i++) {
					if (i == 0) {
						debug ("--> %s", cursor.get_string (i));
					} else {
						debug ("  --> %s", cursor.get_string (i));
					}
				}

				// Get icon
				string urn = cursor.get_string (0);
				string _file = cursor.get_string (1);
				string title = cursor.get_string (2);
				string _file_time = cursor.get_string (3);
				string _file_size = cursor.get_string (4);
				string tooltip = cursor.get_string (7);
				Gdk.Pixbuf pixbuf_small = tracker_pixbuf_new_from_file (theme, _file, size_small, false);
				Gdk.Pixbuf pixbuf_big = tracker_pixbuf_new_from_file (theme, _file, size_big, false);
				string file_size = GLib.format_size_for_display (_file_size.to_int());
				string file_time = tracker_time_format_from_iso8601 (_file_time);

				// Insert into model
				TreeIter iter;

				// FIXME: should optimise this a bit more, inserting 2 images into a list eek
				store.append (out iter);
				store.set (iter,
					       0, pixbuf_small, // Small Image
					       1, pixbuf_big,   // Large Image
					       2, urn,          // URN
					       3, _file,        // URL
					       4, title,        // Title
					       5, null,         // Subtitle
					       6, file_time,    // Column2: Time
					       7, file_size,    // Column3: Size
					       8, tooltip,      // Tooltip
					       -1);
			}
		} catch (GLib.Error e) {
			warning ("Could not iterate query results: %s", e.message);
			search_finished (store);
			return;
		}

		search_finished (store);
		*/
	}

	private async void search_detailed (ResultStore store) requires (store != null) {
		/*
		Tracker.Query.Type[] categories = { 
			Tracker.Query.Type.APPLICATIONS,
			Tracker.Query.Type.MUSIC,
			Tracker.Query.Type.VIDEOS,
			Tracker.Query.Type.DOCUMENTS,
			Tracker.Query.Type.MAIL,
			Tracker.Query.Type.IMAGES,
			Tracker.Query.Type.FOLDERS
		};

		Tracker.Query query = new Tracker.Query ();


		store.clear ();

		debug ("Doing detailed search using store:%p", store);

		var screen = window.get_screen ();
		var theme = IconTheme.get_for_screen (screen);
		bool odd = false;

		foreach (Tracker.Query.Type type in categories) {
			int count = 0;

			Tracker.Sparql.Cursor cursor;

			query.limit = 1000;
			query.criteria = search.get_text ();

			try {
				cursor = yield query.perform_async (type, null);

				if (cursor == null) {
					search_finished (store);
					return;
				}

				while (true) {
					bool b = yield cursor.next_async ();
					if (!b) {
						break;
					}

					for (int i = 0; i < cursor.n_columns; i++) {
						if (i == 0) {
							debug ("--> %s", cursor.get_string (i));
						} else {
							debug ("  --> %s", cursor.get_string (i));
						}
					}

					string urn = cursor.get_string (0);
					string _file = cursor.get_string (1);
					string title = cursor.get_string (2);
					string subtitle = null;
					string column2 = null;
					string column3 = null;
					string tooltip = cursor.get_string (5);
					Gdk.Pixbuf pixbuf_small = null; 

					// Special cases
					switch (type) {
					case Tracker.Query.Type.APPLICATIONS:
						if (count == 0) {
							pixbuf_small = tracker_pixbuf_new_from_name (theme, "package-x-generic", size_medium);
						}
						break;
					case Tracker.Query.Type.MUSIC:
						if (count == 0) {
							pixbuf_small = tracker_pixbuf_new_from_name (theme, "audio-x-generic", size_medium);
						}
						column2 = tracker_time_format_from_seconds (cursor.get_string (4));
						break;
					case Tracker.Query.Type.IMAGES:
						if (count == 0) {
							pixbuf_small = tracker_pixbuf_new_from_name (theme, "image-x-generic", size_medium);
						}
						column2 = GLib.format_size_for_display (cursor.get_string (4).to_int ());
						break;
					case Tracker.Query.Type.VIDEOS:
						if (count == 0) {
							pixbuf_small = tracker_pixbuf_new_from_name (theme, "video-x-generic", size_medium);
						}
						column2 = tracker_time_format_from_seconds (cursor.get_string (4));
						break;
					case Tracker.Query.Type.DOCUMENTS:
						if (count == 0) {
							pixbuf_small = tracker_pixbuf_new_from_name (theme, "x-office-presentation", size_medium);
						}
						break;
					case Tracker.Query.Type.MAIL:
						if (count == 0) {
							pixbuf_small = tracker_pixbuf_new_from_name (theme, "emblem-mail", size_medium);
						}
						column2 = tracker_time_format_from_iso8601 (cursor.get_string (4));
						break;
					case Tracker.Query.Type.FOLDERS:
						if (count == 0) {
							pixbuf_small = tracker_pixbuf_new_from_name (theme, "folder", size_medium);
						}
						column2 = tracker_time_format_from_iso8601 (cursor.get_string (4));
						break;

					default:
						break;
					}

					if (subtitle == null) {
						subtitle = cursor.get_string (3);
					}

					if (column2 == null) {
						column2 = cursor.get_string (4);
					}

					// Insert into model
					TreeIter iter;

					// FIXME: should optimise this a bit more, inserting 2 images into a list eek
					store.append (out iter);
					store.set (iter,
							   0, pixbuf_small, // Small Image
							   1, null,         // Large Image
							   2, urn,          // URN
							   3, _file,        // URL
							   4, title,        // Title
							   5, subtitle,     // Subtitle
							   6, column2,      // Column2
							   7, column3,      // Column3
							   8, tooltip,      // Tooltip
							   9, odd,          // Category hint
							   -1);

					count++;
				}
			} catch (GLib.Error e) {
				warning ("Could not iterate query results: %s", e.message);
				search_finished (store);
				return;
			}

			if (count > 0) {
				odd = !odd;
			}
		}

		search_finished (store);
		*/
	}

	private void search_finished (ResultStore? store) {
		// Check if we have any results, if we don't change the view
		if (store == null || !store.has_results ()) {
			sw_noresults.show ();
			sw_icons.hide ();
			sw_categories.hide ();
			sw_filelist.hide ();
		}
	}

	private TreeIter? search_history_find_or_insert (string criteria, bool? add_to_model = false) {
		if (criteria.length < 1) {
			return null;
		}

		ComboBox combo = search_list as ComboBox;
		TreeModel model = combo.get_model ();
		string criteria_folded = criteria.casefold ();

		TreeIter iter;
		bool valid = model.iter_children (out iter, null);

		while (valid) {
			string text;

			model.get (iter, 0, out text, -1);

			string text_folded = text.casefold ();

			if (text_folded == criteria_folded) {
				return iter;
			}

			valid = model.iter_next (ref iter);
		}

		if (add_to_model) {
			TreeIter new_iter;

			ListStore store = (ListStore) model;
			store.prepend (out new_iter);
			store.set (new_iter, 0, criteria, -1);

			history.add (criteria);
		}

		return null;
	}

	private void search_history_insert (string[] history) {
		foreach (string criteria in history) {
			search_history_find_or_insert (criteria, true);
		}
	}

	private bool search_run () {
		last_search_id = 0;

		string str = search.get_text ();
		string criteria = str.strip ();
		ResultStore store = null;

		if (criteria.length < 3) {
			search_finished (store);
			return false;
		}

		search_history_find_or_insert (criteria, true);

		// Show correct window
		sw_noresults.hide ();

		if (view_icons.active) {
			sw_icons.show ();
			store = sw_icons.store;
		} else {
			sw_icons.hide ();
		}

		if (view_categories.active) {
			sw_categories.show ();
			store = sw_categories.store;
		} else {
			sw_categories.hide ();
		}

		if (view_filelist.active) {
			sw_filelist.show ();
			store = sw_filelist.store;
		} else {
			sw_filelist.hide ();
		}

		if (store != null) {
			store.search_term = search.get_text ();
		}

		return false;
	}

	private void view_toggled () {
		bool show_find_in;

		if (!view_icons.active &&
			!view_filelist.active &&
			!view_categories.active) {
				return;
		}

		show_find_in = view_filelist.active || view_icons.active;

		// Show no results Window when switching
		sw_noresults.show ();
		sw_icons.hide ();
		sw_filelist.hide ();
		sw_categories.hide ();

		// Show/Hide secondary widgets
		separator_secondary.visible = show_find_in;
		find_in_contents.visible = show_find_in;
		find_in_titles.visible = show_find_in;

		search_run ();
		//current_view = rows;
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

	private void launch_selected (TreeModel model, TreePath path, int col) {
		TreeIter iter;
		model.get_iter (out iter, path);

		weak string uri;
		model.get (iter, col, out uri);

                if (uri == null) {
	                return;
                }

                debug ("Selected uri:'%s'", uri);

		// Bit of a hack for now if there is no URI scheme, we assume that
		// the uri is actually a command line to launch.
		if (uri.index_of ("://") < 1) {
			var command = uri.split (" ");
			debug ("Attempting to spawn_async() '%s'", command[0]);

			Pid child_pid;
			string[] argv = new string[1];
			argv[0] = command[0];

			try {
				Process.spawn_async ("/usr/bin",
				                     argv,
				                     null, // environment
				                     SpawnFlags.SEARCH_PATH,
				                     null, // child_setup
				                     out child_pid);
			} catch (Error e) {
				warning ("Could not launch '%s', %d->%s", command[0], e.code, GLib.strerror (e.code));
				return;
			}

			debug ("Launched application with PID:%d", child_pid);
			return;
		}

		try {
			debug ("Attempting to launch application for uri:'%s'", uri);
			AppInfo.launch_default_for_uri (uri, null);
		} catch (GLib.Error e) {
			warning ("Could not launch application: " + e.message);
		}
	}

	private void view_row_selected (TreeView view, TreePath path, TreeViewColumn column) {
		var model = view.get_model ();
		launch_selected (model, path, 1);
	}

	private void icon_item_selected (IconView view, TreePath path) {
		var model = view.get_model ();
		launch_selected (model, path, 1);
	}

	private void show_tags_clicked () {
		if (show_tags.active) {
			debug ("Showing tags");
			taglist.show ();
		} else {
			debug ("Hiding tags");
			taglist.hide ();
		}
	}

	private void show_stats_clicked () {
		debug ("Showing stats dialog");
		Tracker.Stats s = new Tracker.Stats ();
		s.show ();
	}
}

static bool print_version = false;
[CCode (array_length = false, array_null_terminated = true)]
static string[] search_criteria = null;

const OptionEntry[] options = {
	{ "version",
	  'V',
	  0,
	  OptionArg.NONE,
	  ref print_version,
	  N_("Print version"),
	  null },
	{ "", // G_OPTION_REMAINING
	  0,
	  0,
	  OptionArg.STRING_ARRAY,
	  ref search_criteria,
	  N_("[SEARCH-CRITERIA]"),
	  N_("[SEARCH-CRITERIA]") },
	{ null }
};

static int main (string[] args) {
	OptionContext context = new OptionContext (_("Desktop Search user interface using Tracker"));

	try {
		context.set_help_enabled (true);
		context.add_main_entries (options, null);
		context.add_group (Gtk.get_option_group (true));
		context.parse (ref args);
	} catch (Error e) {
		printerr (e.message + "\n\n");
		printerr (context.get_help (true, null));
		return 1;
	}

	if (print_version) {
		string about = "";
		string license = "";

		about   += "Tracker " + Config.PACKAGE_VERSION + "\n";

		license += "This program is free software and comes without any warranty.\n";
		license += "It is licensed under version 2 or later of the General Public ";
		license += "License which can be viewed at:\n";
		license += "\n";
		license += "  http://www.gnu.org/licenses/gpl.txt\n";

		print ("\n" + about + "\n" + license + "\n");
		return 0;
	}

	Gtk.init (ref args);

	Intl.bindtextdomain (Config.GETTEXT_PACKAGE, Config.LOCALEDIR);
	Intl.bind_textdomain_codeset (Config.GETTEXT_PACKAGE, "UTF-8");
	Intl.textdomain (Config.GETTEXT_PACKAGE);

	Tracker.Needle n = new Tracker.Needle ();
	n.show();

	n.set_search (search_criteria);

	Gtk.main ();

	return 0;
}
