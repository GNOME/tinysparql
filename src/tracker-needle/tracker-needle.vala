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
	private ToggleToolButton find_in_all;
	private ToolItem search_entry;
	private ComboBox search_list;
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
	private InfoBar info_bar;
	private Label info_bar_label;
	private TrackerTagsView tags_view;
	private uint last_search_id = 0;
	private int size_small = 0;
	private int size_medium = 0;
	private int size_big = 0;
	private uint limit = 500;
	static bool current_find_in_filelist = true;
	static bool current_find_in_icons = true;
	private Widget current_view;

	private ResultStore categories_model;
	private ResultStore files_model;
	private ResultStore files_in_title_model;
	private ResultStore images_model;
	private ResultStore images_in_title_model;

	private void result_overflow () {
		string str = "%s\n<small>%s</small>".printf (_("Search criteria was too generic"),
		                                             _("Only the first 500 items will be displayed"));
		show_info_message (str, MessageType.INFO);
	}

	private void create_models () {
		// Categories model
		categories_model = new ResultStore (6);
		categories_model.limit = limit;
		categories_model.result_overflow.connect (result_overflow);
		categories_model.add_query (Tracker.Query.Type.APPLICATIONS,
		                            Tracker.Query.Match.FTS,
		                            "?urn",
		                            "tracker:coalesce(nfo:softwareCmdLine(?urn), ?urn)",
		                            "tracker:coalesce(nie:title(?urn), nfo:fileName(?urn))",
		                            "nie:comment(?urn)",
		                            "\"\"",
		                            "\"\"");

		categories_model.add_query (Tracker.Query.Type.IMAGES,
		                            Tracker.Query.Match.FTS,
		                            "?urn",
		                            "nie:url(?urn)",
		                            "tracker:coalesce(nie:title(?urn), nfo:fileName(?urn))",
		                            "fn:string-join((nfo:height(?urn), nfo:width(?urn)), \" x \")",
		                            "nfo:fileSize(?urn)",
		                            "nie:url(?urn)");
		categories_model.add_query (Tracker.Query.Type.MUSIC,
		                            Tracker.Query.Match.FTS_INDIRECT,
		                            "?urn",
		                            "nie:url(?urn)",
		                            "tracker:coalesce(nie:title(?urn), nfo:fileName(?urn))",
		                            "fn:string-join((?performer, ?album), \" - \")",
		                            "nfo:duration(?urn)",
		                            "nie:url(?urn)");
		categories_model.add_query (Tracker.Query.Type.VIDEOS,
		                            Tracker.Query.Match.FTS,
		                            "?urn",
		                            "nie:url(?urn)",
		                            "tracker:coalesce(fts:snippet(?urn),nie:title(?urn), nfo:fileName(?urn))",
		                            "\"\"",
		                            "nfo:duration(?urn)",
		                            "nie:url(?urn)");
		categories_model.add_query (Tracker.Query.Type.DOCUMENTS,
		                            Tracker.Query.Match.FTS_INDIRECT,
		                            "?urn",
		                            "nie:url(?urn)",
		                            "tracker:coalesce(nie:title(?urn), nfo:fileName(?urn))",
		                            "tracker:coalesce(fts:snippet(?urn),nco:fullname(?creator), nco:fullname(?publisher))",
		                            "nfo:pageCount(?urn)",
		                            "nie:url(?urn)");
		categories_model.add_query (Tracker.Query.Type.MAIL,
		                            Tracker.Query.Match.FTS,
		                            "?urn",
		                            "nie:url(?urn)",
		                            "nmo:messageSubject(?urn)",
		                            "tracker:coalesce(fts:snippet(?urn),nco:fullname(?sender), nco:nickname(?sender), nco:emailAddress(?sender))",
		                            "nmo:receivedDate(?urn)",
		                            "fn:concat(\"To: \", tracker:coalesce(nco:fullname(?to), nco:nickname(?to), nco:emailAddress(?to)))");
		categories_model.add_query (Tracker.Query.Type.FOLDERS,
		                            Tracker.Query.Match.FTS,
		                            "?urn",
		                            "nie:url(?urn)",
		                            "tracker:coalesce(fts:snippet(?urn),nie:title(?urn), nfo:fileName(?urn))",
		                            "nie:url(?parent)",
		                            "nfo:fileLastModified(?urn)",
		                            "?tooltip");
		categories_model.add_query (Tracker.Query.Type.BOOKMARKS,
		                            Tracker.Query.Match.FTS,
		                            "?urn",
		                            "nie:url(?bookmark)",
		                            "nie:title(?urn)",
		                            "nie:url(?bookmark)",
		                            "nie:contentLastModified(?urn)",
		                            "nie:url(?bookmark)");

		// Files model
		files_model = new ResultStore (7);
		files_model.limit = limit;
		files_model.result_overflow.connect (result_overflow);
		files_model.add_query (Tracker.Query.Type.ALL,
		                       Tracker.Query.Match.FTS,
		                       "?urn",
		                       "nie:url(?urn)",
		                       "tracker:coalesce(nie:title(?urn), nfo:fileName(?urn))",
		                       "nie:url(?urn)",
		                       "nfo:fileSize(?urn)",
		                       "nfo:fileLastModified(?urn)",
		                       "nie:url(?urn)");

		files_in_title_model = new ResultStore (7);
		files_in_title_model.limit = limit;
		files_in_title_model.result_overflow.connect (result_overflow);
		files_in_title_model.add_query (Tracker.Query.Type.ALL,
		                                Tracker.Query.Match.TITLES,
		                                "?urn",
		                                "nie:url(?urn)",
		                                "tracker:coalesce(nie:title(?urn), nfo:fileName(?urn))",
		                                "nie:url(?urn)",
		                                "nfo:fileSize(?urn)",
		                                "nfo:fileLastModified(?urn)",
		                                "nie:url(?urn)");

		// Images model
		images_model = new ResultStore (6);
		images_model.limit = limit;
		images_model.result_overflow.connect (result_overflow);
		images_model.icon_size = 128;
		images_model.add_query (Tracker.Query.Type.IMAGES,
		                        Tracker.Query.Match.NONE,
		                        "?urn",
		                        "nie:url(?urn)",
		                        "tracker:coalesce(nie:title(?urn), nfo:fileName(?urn))",
//		                        "nie:url(?urn)",
		                        "nfo:fileSize(?urn)",
		                        "nfo:fileLastModified(?urn)",
		                        "nie:url(?urn)");

		images_in_title_model = new ResultStore (6);
		images_in_title_model.limit = limit;
		images_in_title_model.result_overflow.connect (result_overflow);
		images_in_title_model.icon_size = 128;
		images_in_title_model.add_query (Tracker.Query.Type.IMAGES,
		                                 Tracker.Query.Match.TITLES,
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
		Button info_bar_button;
		Toolbar toolbar;
		Paned paned;

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

		toolbar = builder.get_object ("toolbar_main") as Toolbar;
		toolbar.get_style_context().add_class (STYLE_CLASS_PRIMARY_TOOLBAR);

		info_bar = builder.get_object ("info_bar") as InfoBar;
		info_bar_label = builder.get_object ("info_bar_label") as Label;
		info_bar_button = builder.get_object ("info_bar_button") as Button;
		info_bar_button.clicked.connect (info_bar_closed);

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

		find_in_all = builder.get_object ("toolbutton_find_in_all") as ToggleToolButton;
		find_in_all.toggled.connect (find_in_toggled);

		search_entry = builder.get_object ("toolitem_search_entry") as ToolItem;
		search_list = builder.get_object ("combobox_search") as ComboBox;
		search = search_list.get_child () as Entry;
		search.changed.connect (search_changed);
		search.activate.connect (search_activated);
		search.key_press_event.connect (search_key_press_event);
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
		TreeSelection treeselection;

		sw_noresults = new Tracker.View (Tracker.View.Display.NO_RESULTS, null);
		view.pack_start (sw_noresults, true, true, 0);

		sw_categories = new Tracker.View (Tracker.View.Display.CATEGORIES, categories_model);
		sw_categories.store.notify["active"].connect (store_state_changed);
		treeview = (TreeView) sw_categories.get_child ();
		treeview.row_activated.connect (view_row_activated);
		treeselection = treeview.get_selection ();
		treeselection.changed.connect (view_row_selected);
		view.pack_start (sw_categories, true, true, 0);

		sw_filelist = new Tracker.View (Tracker.View.Display.FILE_LIST, null);
		treeview = (TreeView) sw_filelist.get_child ();
		treeview.row_activated.connect (view_row_activated);
		treeselection = treeview.get_selection ();
		treeselection.changed.connect (view_row_selected);
		view.pack_start (sw_filelist, true, true, 0);

		sw_icons = new Tracker.View (Tracker.View.Display.FILE_ICONS, null);
		iconview = (IconView) sw_icons.get_child ();
		iconview.item_activated.connect (icon_item_activated);
		iconview.selection_changed.connect (icon_view_selection_changed);
		view.pack_start (sw_icons, true, true, 0);

		// Set up tags widget
		paned = builder.get_object ("hpaned") as Paned;
		tags_view = new TrackerTagsView (null);
		tags_view.hide ();
		tags_view.hide_label ();
		paned.pack2 (tags_view, false, false);

		view_categories.set_active (true);
	}

	private bool window_key_press_event (Gtk.Widget widget, Gdk.EventKey event) {
		// Add Ctrl+W close window semantics
		if (Gdk.ModifierType.CONTROL_MASK in event.state && Gdk.keyval_name (event.keyval) == "w") {
			widget.destroy();
		}

		return false;
	}

	private bool search_key_press_event (Gtk.Widget widget, Gdk.EventKey event) {
		if (Gdk.keyval_name (event.keyval) == "Down" ||
		    Gdk.keyval_name (event.keyval) == "KP_Down") {
			var child = ((ScrolledWindow) current_view).get_child ();

			if (child != null) {
				child.grab_focus();
			}
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

		if (!show_tags.active) {
			if (criteria.length < 3) {
				// Allow empty search criteria for finding all
				if (!view_icons.active || !find_in_all.active) {
					search_finished (store);
					return false;
				}
			}

			search_history_find_or_insert (criteria, true);
		}

		// Show correct window
		sw_noresults.hide ();
		current_view = sw_noresults;

		if (view_icons.active) {
			sw_icons.show ();
			current_view = sw_icons;

			if (find_in_all.active) {
				store = images_model;
			} else {
				store = images_in_title_model;
			}

			sw_icons.store = store;
		} else {
			sw_icons.hide ();
		}

		if (view_categories.active) {
			sw_categories.show ();
			current_view = sw_categories;
			store = categories_model;
		} else {
			sw_categories.hide ();
		}

		if (view_filelist.active) {
			sw_filelist.show ();
			current_view = sw_filelist;

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
			// We can set tags to search by but we don't anymore
			store.search_tags = null;
			store.search_term = search.get_text ();
		}

		return false;
	}

	private void view_toggled () {
		if (!view_icons.active &&
			!view_filelist.active &&
			!view_categories.active) {
				return;
		}

		if (view_categories.active || view_filelist.active) {
			if (current_find_in_filelist) {
				find_in_contents.active = true;
			} else {
				find_in_titles.active = true;
			}
		} else if (view_icons.active) {
			if (current_find_in_icons) {
				find_in_titles.active = true;
			} else {
				find_in_all.active = true;
			}
		}

		// Show no results Window when switching
		sw_noresults.show ();
		sw_icons.hide ();
		sw_filelist.hide ();
		sw_categories.hide ();

		// Show/Hide secondary widgets
		separator_secondary.visible = view_filelist.active || view_icons.active;
		find_in_contents.visible = view_filelist.active;
		find_in_titles.visible = view_filelist.active || view_icons.active;
		find_in_all.visible = view_icons.active; // only show this in one view

		search_run ();
		//current_view = rows;
	}

	private void find_in_toggled () {
		if (!find_in_contents.active &&
		    !find_in_titles.active &&
		    !find_in_all.active) {
		    return;
		}

		if (find_in_contents.active) {
			debug ("Find in toggled to 'contents'");

			if (show_tags.active != true) {
				search_entry.sensitive = true;
			}

			search_run ();
		} else if (find_in_titles.active) {
			debug ("Find in toggled to 'titles'");

			if (show_tags.active != true) {
				search_entry.sensitive = true;
			}

			search_run ();
		} else if (find_in_all.active) {
			debug ("Find in toggled to 'all'");

			// We hide the entry in this case, which is special
			search_entry.sensitive = false;

			search_run ();
		}

		if (view_filelist.active) {
			current_find_in_filelist = find_in_contents.active;
		} else if (view_icons.active) {
			current_find_in_icons = find_in_titles.active;
		}
	}

	private void view_row_activated (TreeView view, TreePath path, TreeViewColumn column) {
		var model = view.get_model ();
		tracker_model_launch_selected (model, path, 1);
	}

	private void icon_item_activated (IconView view, TreePath path) {
		var model = view.get_model ();
		tracker_model_launch_selected (model, path, 1);
	}

	private void view_row_selected (TreeSelection selection) {
		TreeIter iter;
		TreeModel model = null;
		debug ("Row selection changed");

		List<TreePath> rows = selection.get_selected_rows (out model);
		List<string> uris = null;

		if (rows == null) {
			return;
		}

		foreach (TreePath path in rows) {
			if (model.get_iter (out iter, path)) {
				string uri;

				model.get (iter, 1, out uri, -1);
				debug ("--> %s", uri);

				if (uri != null) {
					uris.prepend (uri);
				}
			}
		}

		tags_view.set_files (uris);
	}

	private void icon_view_selection_changed () {
		IconView iconview;
		TreeModel model = null;
		debug ("Icon selection changed");

		iconview = (IconView) sw_icons.get_child ();
		model = iconview.get_model ();
		List<string> uris = null;

		iconview.selected_foreach ((iconview, path) => {
			TreeIter iter;

			if (model.get_iter (out iter, path)) {
				string uri;

				model.get (iter, 1, out uri, -1);
				debug ("--> %s", uri);

				if (uri != null) {
					uris.prepend (uri);
				}
			}
		});

		tags_view.set_files (uris);
	}

	private void show_tags_clicked () {
		if (show_tags.active) {
			debug ("Showing tags");
			tags_view.show ();
			//search_entry.sensitive = false;
		} else {
			debug ("Hiding tags");
			tags_view.hide ();
			//search_entry.sensitive = true;
		}

		// Re-run search to filter with or without tags
		// search_run ();
	}

	private void show_stats_clicked () {
		debug ("Showing stats dialog");
		Tracker.Stats s = new Tracker.Stats ();
		s.show ();
	}

	public void show_info_message (string          message,
	                               Gtk.MessageType type) {
		info_bar.set_message_type (type);
		info_bar_label.set_markup (message);
		info_bar.show ();
	}

	private void info_bar_closed (Button source) {
		info_bar.hide ();
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
