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
using Tracker.Sparql;
using Tracker.Config;
using Tracker.Stats;
using Tracker.TagList;
using Tracker.View;

[CCode (cname = "TRACKER_UI_DIR")]
extern static const string UIDIR;

[CCode (cname = "SRCDIR")]
extern static const string SRCDIR;

public class Tracker.Needle {
	private const string UI_FILE = "tracker-needle.ui";
	private Tracker.Config config;
	private Window window;
	private ToggleToolButton view_list;
	private ToggleToolButton view_icons;
	private ToggleToolButton view_details;
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
	private Tracker.View sw_treeview;
	private TreeView treeview;
	private Tracker.View sw_iconview;
	private IconView iconview;
	private Tracker.TagList taglist;
	private uint last_search_id = 0;
	private ListStore store;
	private int size_small = 0;
	private int size_medium = 0;
	private int size_big = 0;
	static bool current_view = true;
	static bool current_find_in = true;

	public Needle () {
		config = new Tracker.Config ();
	}

	public void show () {
		setup_ui ();

		window.show ();
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

		view_list = builder.get_object ("toolbutton_view_list") as ToggleToolButton;
		view_list.toggled.connect (view_toggled);

		view_icons = builder.get_object ("toolbutton_view_icons") as ToggleToolButton;
		view_icons.toggled.connect (view_toggled);

		view_details = builder.get_object ("toolbutton_view_details") as ToggleToolButton;
		view_details.toggled.connect (view_toggled);

		separator_secondary = builder.get_object ("separator_secondary") as SeparatorToolItem;

		find_in_contents = builder.get_object ("toolbutton_find_in_contents") as ToggleToolButton;
		find_in_contents.toggled.connect (find_in_toggled);

		find_in_titles = builder.get_object ("toolbutton_find_in_titles") as ToggleToolButton;
		find_in_titles.toggled.connect (find_in_toggled);

		search_list = builder.get_object ("comboboxentry_search") as ComboBoxEntry;
		search = search_list.get_child () as Entry;
		search.changed.connect (search_changed);
		search_history_insert (config.get_history ());

		spinner = new Spinner ();
		spinner_shell = builder.get_object ("toolcustom_spinner") as ToolItem;
		spinner_shell.add (spinner);

		show_tags = builder.get_object ("toolbutton_show_tags") as ToggleToolButton;
		show_tags.clicked.connect (show_tags_clicked);

		show_stats = builder.get_object ("toolbutton_show_stats") as ToolButton;
		show_stats.clicked.connect (show_stats_clicked);

		view = builder.get_object ("hbox_view") as HBox;

		// Set up views
		sw_noresults = new Tracker.View (Tracker.View.Display.NO_RESULTS);
		view.pack_start (sw_noresults, true, true, 0);

		sw_treeview = new Tracker.View (Tracker.View.Display.CATEGORIES);
		treeview = (TreeView) sw_treeview.get_child ();
		view.pack_start (sw_treeview, true, true, 0);

		sw_iconview = new Tracker.View (Tracker.View.Display.FILE_ICONS);
		iconview = (IconView) sw_iconview.get_child ();
		view.pack_start (sw_iconview, true, true, 0);

		// Set up view models
		setup_ui_results (treeview, iconview);
		
		// Set up taglist
		taglist = new Tracker.TagList ();
		taglist.hide ();
		view.pack_end (taglist, false, true, 0);

		view_details.set_active (true);
	}

	private void cell_renderer_func (Gtk.CellLayout   cell_layout,
	                                 Gtk.CellRenderer cell,
	                                 Gtk.TreeModel    tree_model,
	                                 Gtk.TreeIter     iter) {
		Gdk.Color color;
		Gtk.Style style;
		bool show_row_hint;

		tree_model.get (iter, 9, out show_row_hint, -1);

		style = ((Widget) treeview).get_style ();

		color = style.base[Gtk.StateType.SELECTED];
		int sum_normal = color.red + color.green + color.blue;
		color = style.base[Gtk.StateType.NORMAL];
		int sum_selected = color.red + color.green + color.blue;
		color = style.text_aa[Gtk.StateType.INSENSITIVE];

		if (sum_normal < sum_selected) {
			/* Found a light theme */
			color.red = (color.red + (style.white).red) / 2;
			color.green = (color.green + (style.white).green) / 2;
			color.blue = (color.blue + (style.white).blue) / 2;
		} else {
			/* Found a dark theme */
			color.red = (color.red + (style.black).red) / 2;
			color.green = (color.green + (style.black).green) / 2;
			color.blue = (color.blue + (style.black).blue) / 2;
		}

		// Set odd/even colours
		if (show_row_hint) {
//			((Widget) treeview).style_get ("odd-row-color", out color, null);
			cell.set ("cell-background-gdk", &color);
		} else {
//			((Widget) treeview).style_get ("even-row-color", out color, null);
			cell.set ("cell-background-gdk", null);
		}
	}

	private void setup_ui_results (TreeView treeview, IconView iconview) {
		// Setup treeview
		store = new ListStore (10,
		                       typeof (Gdk.Pixbuf),  // Icon small
		                       typeof (Gdk.Pixbuf),  // Icon big
		                       typeof (string),      // URN
		                       typeof (string),      // URL
		                       typeof (string),      // Title
		                       typeof (string),      // Subtitle
		                       typeof (string),      // Column 2
		                       typeof (string),      // Column 3
		                       typeof (string),      // Tooltip
		                       typeof (bool));       // Category hint
		treeview.set_model (store);
		treeview.set_tooltip_column (8);
		treeview.set_rules_hint (false);

		Gtk.TreeViewColumn col;

		var renderer1 = new CellRendererPixbuf ();
		var renderer2 = new Tracker.CellRendererText ();

		col = new Gtk.TreeViewColumn ();
		col.pack_start (renderer1, false);
		col.add_attribute (renderer1, "pixbuf", 0);
		renderer1.xpad = 5;
		renderer1.ypad = 5;

		col.pack_start (renderer2, true);
		col.add_attribute (renderer2, "text", 4);
		col.add_attribute (renderer2, "subtext", 5);
		renderer2.ellipsize = Pango.EllipsizeMode.MIDDLE;

		col.set_title (_("File"));
		col.set_resizable (true);
		col.set_expand (true);
		col.set_sizing (Gtk.TreeViewColumnSizing.AUTOSIZE);
		col.set_cell_data_func (renderer1, cell_renderer_func);
		col.set_cell_data_func (renderer2, cell_renderer_func);
		treeview.append_column (col);

		var renderer3 = new Tracker.CellRendererText ();
		col = new Gtk.TreeViewColumn ();
		col.pack_start (renderer3, true);
		col.add_attribute (renderer3, "text", 6);
		col.set_title (_("Last Changed"));
		col.set_cell_data_func (renderer3, cell_renderer_func);
		treeview.append_column (col);

		var renderer4 = new Tracker.CellRendererText ();
		col = new Gtk.TreeViewColumn ();
		col.pack_start (renderer4, true);
		col.add_attribute (renderer4, "text", 7);
		col.set_title (_("Size"));
		col.set_cell_data_func (renderer4, cell_renderer_func);
		treeview.append_column (col);

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

	private async void search_simple () {
		Tracker.Query query = new Tracker.Query ();
		Tracker.Sparql.Cursor cursor = null;

		query.limit = 1000;
		query.criteria = search.get_text ();

		try {
			if (find_in_contents.active) {
				cursor = yield query.perform_async (query.Type.ALL);
			} else {
				cursor = yield query.perform_async (query.Type.ALL_ONLY_IN_TITLES);
			}

			if (cursor == null) {
				search_finished ();
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
				store.append (out iter);

				// FIXME: should optimise this a bit more, inserting 2 images into a list eek
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
			search_finished ();
			return;
		}

		search_finished ();
	}

	private async void search_detailed () {
		Tracker.Query.Type[] categories = { 
			Tracker.Query.Type.APPLICATIONS,
			Tracker.Query.Type.MUSIC,
			Tracker.Query.Type.VIDEOS,
			Tracker.Query.Type.DOCUMENTS,
			Tracker.Query.Type.MAIL,
			Tracker.Query.Type.IMAGES
		};
		Tracker.Query query = new Tracker.Query ();

		store.clear ();

		var screen = window.get_screen ();
		var theme = IconTheme.get_for_screen (screen);
		bool odd = false;

		foreach (Tracker.Query.Type type in categories) {
			int count = 0;

			Tracker.Sparql.Cursor cursor;

			query.limit = 1000;
			query.criteria = search.get_text ();

			try {
				cursor = yield query.perform_async (type);

				if (cursor == null) {
					search_finished ();
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
					store.append (out iter);

					// FIXME: should optimise this a bit more, inserting 2 images into a list eek
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
				search_finished ();
				return;
			}

			if (count > 0) {
				odd = !odd;
			}
		}

		search_finished ();
	}

	private void search_finished () {
		// Hide spinner
		spinner.stop ();
		spinner_shell.hide ();

		// Check if we have any results, if we don't change the view
		if (store.iter_n_children (null) < 1) {
			sw_noresults.show ();
			sw_iconview.hide ();
			sw_treeview.hide ();
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

			config.add_history (criteria);
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

		if (criteria.length < 1) {
			search_finished ();

			return false;
		}

		search_history_find_or_insert (criteria, true);

		// Show correct window
		bool rows = view_list.active || view_details.active;
		
		if (rows) {
			sw_noresults.hide ();
			sw_iconview.hide ();
			sw_treeview.show ();
		} else {
			sw_noresults.hide ();
			sw_iconview.show ();
			sw_treeview.hide ();
		}

		// Show spinner
		spinner_shell.show_all ();
		spinner.start ();

		if (view_details.active) {
			search_detailed ();
		} else {
			search_simple ();
		}

		return false;
	}

	private void view_toggled () {
		bool rows;
		bool show_find_in;

		rows = view_list.active || view_details.active;

		if (current_view == rows) {
			return;
		}

		if (rows) {
			// FIXME: if list/details changes, re-run query

			// Was: sw_treeview.show_all ();

			debug ("View toggled to 'list' or 'details'");
			
			if (view_details.active) {
				treeview.set_grid_lines (Gtk.TreeViewGridLines.NONE);
				treeview.get_column (2).visible = false;
				treeview.set_headers_visible (false);
				show_find_in = false;
			} else {
				treeview.set_grid_lines (Gtk.TreeViewGridLines.VERTICAL);
				treeview.get_column (2).visible = true;
				treeview.set_headers_visible (true);
				show_find_in = true;
			}
		} else {
			// Was: sw_iconview.show_all ();
			show_find_in = true;
			debug ("View toggled to 'icons'");
		}

		// Show no results Window when switching
		sw_noresults.show ();
		sw_iconview.hide ();
		sw_treeview.hide ();

		// Show/Hide secondary widgets
		separator_secondary.visible = show_find_in;
		find_in_contents.visible = show_find_in;
		find_in_titles.visible = show_find_in;

		search_run ();
		current_view = rows;
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

		weak string uri;
		model.get (iter, 3, out uri);

		debug ("Selected uri:'%s'", uri);

		try {
			AppInfo.launch_default_for_uri (uri, null);
		} catch (GLib.Error e) {
			warning ("Could not launch application: " + e.message);
		}
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

static int main (string[] args) {
	Gtk.init (ref args);

	Intl.bindtextdomain (Config.GETTEXT_PACKAGE, Config.LOCALEDIR);
	Intl.bind_textdomain_codeset (Config.GETTEXT_PACKAGE, "UTF-8");
	Intl.textdomain (Config.GETTEXT_PACKAGE);

	Tracker.Needle n = new Tracker.Needle ();
	n.show();
	Gtk.main ();

	return 0;
}
