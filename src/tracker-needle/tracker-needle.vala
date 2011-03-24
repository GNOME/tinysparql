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
	static bool current_find_in = true;

	private ResultStore categories_model;
	private ResultStore files_model;
	private ResultStore files_in_title_model;
	private ResultStore images_model;

	private void create_models () {
		// Categories model
		categories_model = new ResultStore (6);
		categories_model.add_query (Tracker.Query.Type.APPLICATIONS,
		                            "?urn",
		                            "tracker:coalesce(nfo:softwareCmdLine(?urn), ?urn)",
		                            "tracker:coalesce(nie:title(?urn), nfo:fileName(?urn))",
		                            "nie:comment(?urn)",
		                            "\"\"",
		                            "\"\"");

		categories_model.add_query (Tracker.Query.Type.IMAGES,
		                            "?urn",
		                            "nie:url(?urn)",
		                            "tracker:coalesce(nie:title(?urn), nfo:fileName(?urn))",
		                            "fn:string-join((nfo:height(?urn), nfo:width(?urn)), \" x \")",
		                            "nfo:fileSize(?urn)",
		                            "nie:url(?urn)");
		categories_model.add_query (Tracker.Query.Type.MUSIC,
		                            "?urn",
		                            "nie:url(?urn)",
		                            "tracker:coalesce(nie:title(?urn), nfo:fileName(?urn))",
		                            "fn:string-join((?performer, ?album), \" - \")",
		                            "nfo:duration(?urn)",
		                            "nie:url(?urn)");
		categories_model.add_query (Tracker.Query.Type.VIDEOS,
		                            "?urn",
		                            "nie:url(?urn)",
		                            "tracker:coalesce(nie:title(?urn), nfo:fileName(?urn))",
		                            "\"\"",
		                            "nfo:duration(?urn)",
		                            "nie:url(?urn)");
		categories_model.add_query (Tracker.Query.Type.DOCUMENTS,
		                            "?urn",
		                            "nie:url(?urn)",
		                            "tracker:coalesce(nie:title(?urn), nfo:fileName(?urn))",
		                            "tracker:coalesce(nco:fullname(?creator), nco:fullname(?publisher))",
		                            "nfo:pageCount(?urn)",
		                            "nie:url(?urn)");
		categories_model.add_query (Tracker.Query.Type.MAIL,
		                            "?urn",
		                            "nie:url(?urn)",
		                            "tracker:coalesce(nco:fullname(?sender), nco:nickname(?sender), nco:emailAddress(?sender))",
		                            "tracker:coalesce(nmo:messageSubject(?urn))",
		                            "nmo:receivedDate(?urn)",
		                            "fn:concat(\"To: \", tracker:coalesce(nco:fullname(?to), nco:nickname(?to), nco:emailAddress(?to)))");
		categories_model.add_query (Tracker.Query.Type.FOLDERS,
		                            "?urn",
		                            "nie:url(?urn)",
		                            "tracker:coalesce(nie:title(?urn), nfo:fileName(?urn))",
		                            "tracker:coalesce(nie:url(?parent), \"\")",
		                            "nfo:fileLastModified(?urn)",
		                            "?tooltip");

		// Files model
		files_model = new ResultStore (7);
		files_model.add_query (Tracker.Query.Type.ALL,
		                       "?urn",
		                       "nie:url(?urn)",
		                       "tracker:coalesce(nie:title(?urn), nfo:fileName(?urn))",
		                       "nie:url(?urn)",
		                       "nfo:fileSize(?urn)",
		                       "nfo:fileLastModified(?urn)",
		                       "nie:url(?urn)");

		// Files model, search in titles
		files_in_title_model = new ResultStore (7);
		files_in_title_model.add_query (Tracker.Query.Type.ALL_ONLY_IN_TITLES,
		                                "?urn",
		                                "nie:url(?urn)",
		                                "tracker:coalesce(nie:title(?urn), nfo:fileName(?urn))",
		                                "nie:url(?urn)",
		                                "nfo:fileSize(?urn)",
		                                "nfo:fileLastModified(?urn)",
		                                "nie:url(?urn)");

		// Images model
		images_model = new ResultStore (6);
		images_model.icon_size = 48;
		images_model.add_query (Tracker.Query.Type.IMAGES,
		                        "?urn",
		                        "nie:url(?urn)",
		                        "tracker:coalesce(nie:title(?urn), nfo:fileName(?urn))",
		                        "nfo:fileSize(?urn)",
		                        "nfo:fileLastModified(?urn)",
		                        "nie:url(?urn)");
	}

	public Needle () {
		create_models ();
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

		sw_categories = new Tracker.View (Tracker.View.Display.CATEGORIES, categories_model);
		treeview = (TreeView) sw_categories.get_child ();
		treeview.row_activated.connect (view_row_selected);
		sw_categories.store.notify["active"].connect (store_state_changed);
		view.pack_start (sw_categories, true, true, 0);

		sw_filelist = new Tracker.View (Tracker.View.Display.FILE_LIST, null);
		treeview = (TreeView) sw_filelist.get_child ();
		treeview.row_activated.connect (view_row_selected);
		view.pack_start (sw_filelist, true, true, 0);

		sw_icons = new Tracker.View (Tracker.View.Display.FILE_ICONS, images_model);
		iconview = (IconView) sw_icons.get_child ();
		iconview.item_activated.connect (icon_item_selected);
		view.pack_start (sw_icons, true, true, 0);

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
			store = images_model;
		} else {
			sw_icons.hide ();
		}

		if (view_categories.active) {
			sw_categories.show ();
			store = categories_model;
		} else {
			sw_categories.hide ();
		}

		if (view_filelist.active) {
			sw_filelist.show ();

			if (find_in_contents.active) {
				store = files_model;
			} else {
				store = files_in_title_model;
			}

			sw_filelist.store = store;
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
