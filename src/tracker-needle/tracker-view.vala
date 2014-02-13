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

public class Tracker.View : ScrolledWindow {
	public enum Display {
		NO_RESULTS,
		CATEGORIES,
		FILE_LIST,
		FILE_ICONS
	}

	public Display display {
		get;
		private set;
	}

	private ResultStore _store;
	public ResultStore store {
		get {
			return _store;
		}
		set {
			if (_store != null) {
				_store.row_changed.disconnect (store_row_changed);
			}

			_store = value;

			if (_store != null) {
				debug ("using store:%p", _store);
				_store.row_changed.connect (store_row_changed);
			}

			if (display != Display.FILE_ICONS) {
				((TreeView) view).model = _store;
			} else {
				((IconView) view).model = _store;
			}
		}
	}

	private Widget view = null;
	private Gtk.Menu context_menu;

	private void store_row_changed (TreeModel model,
	                                TreePath  path,
	                                TreeIter  iter) {
		int n_children = model.iter_n_children (iter);

		if (n_children > 0) {
			((TreeView) view).expand_row (path, false);
		}
	}

	private bool row_selection_func (TreeSelection selection,
	                                 TreeModel     model,
	                                 TreePath      path,
	                                 bool          path_selected) {
		if (path.get_depth () == 1) {
			// Category row, not selectable
			return false;
		}

		return true;
	}

	public override void unmap () {
		if (store != null) {
			store.cancel_search ();
		}

		base.unmap ();
	}

	public View (Display? _display = Display.NO_RESULTS, ResultStore? store) {
		set_policy (PolicyType.NEVER, PolicyType.AUTOMATIC);

		display = _display;

		if (store != null) {
			_store = store;
			store.row_changed.connect (store_row_changed);
			debug ("using store:%p", store);
		}

		switch (display) {
		case Display.NO_RESULTS:
			Label l;

			l = new Label ("");
			l.set_justify (Justification.CENTER);

			string results = _("No Search Results");
			string help_views = _("Select the view on the toolbar for the content you want, e.g. everything, files or just images");
			string help_search = _("Start to search using the entry box above");
			string markup = @"<big>$results</big>\n\n$help_views\n\n$help_search";
			
			l.set_use_markup (true);
			l.set_markup (markup);

			view = l;
			break;

		case Display.CATEGORIES:
		case Display.FILE_LIST:
			view = new TreeView ();
			break;

		case Display.FILE_ICONS:
			view = new IconView ();
			break;
		}

		if (display == Display.NO_RESULTS) {
			add (view);
		} else {
			add (view);
			setup_model ();
			setup_menus ();
		}

		base.show_all ();
	}

	private void setup_model () {
		switch (display) {
		case Display.FILE_ICONS: {
			IconView iv = (IconView) view;

			iv.set_model (store);
			iv.set_item_width (128);
			iv.set_item_padding (1);
			iv.set_row_spacing (2);
			iv.set_column_spacing (2);
			iv.set_selection_mode (SelectionMode.SINGLE);
			iv.set_pixbuf_column (6);
			iv.set_text_column (-1); // was 2, -1 is for no text
			iv.set_tooltip_column (5);

			break;
		}

		case Display.FILE_LIST: {
			TreeViewColumn col;
			TreeView tv = (TreeView) view;

			tv.set_model (store);
			tv.set_tooltip_column (6);
			tv.set_rules_hint (false);
			tv.set_grid_lines (TreeViewGridLines.VERTICAL);
			tv.set_headers_visible (true);

			var renderer1 = new CellRendererPixbuf ();
			var renderer2 = new Gtk.CellRendererText ();

			col = new TreeViewColumn ();
			col.set_sizing (TreeViewColumnSizing.AUTOSIZE);
			col.pack_start (renderer1, false);
			col.add_attribute (renderer1, "pixbuf", 7);
			renderer1.xpad = 5;
			renderer1.ypad = 5;

			col.pack_start (renderer2, true);
			renderer2.set_fixed_height_from_font (2);
			renderer2.ellipsize = Pango.EllipsizeMode.MIDDLE;

			col.set_title (_("File"));
			col.set_resizable (true);
			col.set_expand (true);
			col.set_cell_data_func (renderer1, background_renderer_func);
			col.set_cell_data_func (renderer2, text_renderer_func);
			tv.append_column (col);

			var renderer3 = new Gtk.CellRendererText ();
			renderer3.set_fixed_height_from_font (2);
			col = new TreeViewColumn ();
			col.set_sizing (TreeViewColumnSizing.AUTOSIZE);
			col.pack_start (renderer3, true);
			col.set_title (_("Last Changed"));
			col.set_cell_data_func (renderer3, file_date_renderer_func);
			tv.append_column (col);

			var renderer4 = new Gtk.CellRendererText ();
			renderer4.set_fixed_height_from_font (2);
			col = new TreeViewColumn ();
			col.set_sizing (TreeViewColumnSizing.AUTOSIZE);
			col.pack_start (renderer4, true);
			col.set_title (_("Size"));
			col.set_cell_data_func (renderer4, file_size_renderer_func);
			tv.append_column (col);

			break;
		}

		case Display.CATEGORIES: {
			TreeViewColumn col;
			TreeView tv = (TreeView) view;
			TreeSelection selection;

			tv.set_model (store);
			tv.set_tooltip_column (5);
			tv.set_rules_hint (false);
			tv.set_grid_lines (TreeViewGridLines.NONE);
			tv.set_headers_visible (false);
			tv.set_show_expanders (false);

			selection = tv.get_selection ();
			selection.set_select_function (row_selection_func);

			col = new TreeViewColumn ();
			col.set_sizing (TreeViewColumnSizing.FIXED);
			col.set_expand (true);

			var renderer1 = new CellRendererPixbuf ();
			col.pack_start (renderer1, false);
			col.add_attribute (renderer1, "pixbuf", 6);
			col.set_cell_data_func (renderer1, background_renderer_func);
			renderer1.xpad = 5;
			renderer1.ypad = 5;

			var renderer2 = new Gtk.CellRendererText ();
			col.pack_start (renderer2, true);
			col.set_cell_data_func (renderer2, text_renderer_func);
			renderer2.set_fixed_height_from_font (2);
			renderer2.ellipsize = Pango.EllipsizeMode.MIDDLE;

			//col.set_resizable (true);
			//col.set_sizing (TreeViewColumnSizing.AUTOSIZE);
			tv.append_column (col);

//			var renderer3 = new Gtk.CellRendererText ();
//			col = new TreeViewColumn ();
//			col.pack_start (renderer3, true);
//			col.add_attribute (renderer3, "text", 3);
//			col.set_title (_("Item Detail"));
//			col.set_cell_data_func (renderer3, cell_renderer_func);
//			tv.append_column (col);

			var renderer4 = new Gtk.CellRendererText ();
			renderer4.set_fixed_height_from_font (2);
			renderer4.alignment = Pango.Alignment.RIGHT;
			renderer4.xalign = 1;

			col = new TreeViewColumn ();
			col.set_min_width (80);
			col.set_sizing (TreeViewColumnSizing.FIXED);
			col.pack_start (renderer4, true);
			col.set_cell_data_func (renderer4, category_detail_renderer_func);
			tv.append_column (col);

			break;
		}
		}
	}

	private void background_renderer_func (CellLayout   cell_layout,
	                                       CellRenderer cell,
	                                       TreeModel    tree_model,
	                                       TreeIter     iter) {
		Gdk.Color color;
		Style style;
		TreePath path;

		style = view.get_style ();

		color = style.base[StateType.SELECTED];
		int sum_normal = color.red + color.green + color.blue;
		color = style.base[StateType.NORMAL];
		int sum_selected = color.red + color.green + color.blue;
		color = style.text_aa[StateType.INSENSITIVE];

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

		path = tree_model.get_path (iter);

		// Set odd/even colours
		if (path.get_indices()[0] % 2 != 0) {
			cell.set ("cell-background-gdk", color);
		} else {
			cell.set ("cell-background-gdk", null);
		}
	}

	private void text_renderer_func (CellLayout   cell_layout,
	                                 CellRenderer cell,
	                                 TreeModel    tree_model,
	                                 TreeIter     iter) {
		string text, subtext;
		string markup = null;
		int n_children;

		background_renderer_func (cell_layout, cell, tree_model, iter);
		n_children = tree_model.iter_n_children (iter);

		if (n_children > 0) {
			// Category row
			Tracker.Query.Type type;
			string cat = null;

			tree_model.get (iter, 7, out type, -1);
			switch (type) {
			case Tracker.Query.Type.APPLICATIONS:
				cat = _("Applications");
				break;
			case Tracker.Query.Type.MUSIC:
				cat = _("Music");
				break;
			case Tracker.Query.Type.IMAGES:
				cat = _("Images");
				break;
			case Tracker.Query.Type.VIDEOS:
				cat = _("Videos");
				break;
			case Tracker.Query.Type.DOCUMENTS:
				cat = _("Documents");
				break;
			case Tracker.Query.Type.MAIL:
				cat = _("Mail");
				break;
			case Tracker.Query.Type.FOLDERS:
				cat = _("Folders");
				break;
			case Tracker.Query.Type.BOOKMARKS:
				cat = _("Bookmarks");
				break;
			}

			markup = "<b><big>%s</big></b> <small>(%d %s)</small>".printf (cat, n_children, _("Items"));
		} else {
			// Result row
			tree_model.get (iter, 2, out text, 3, out subtext, -1);

			if (text != null) {
				markup = Markup.escape_text (text);

				if (subtext != null) {
					subtext = subtext.replace ("\n", " ");
					markup += "\n<small><span color='grey'>%s</span></small>".printf (Markup.escape_text (subtext));
				}
			} else {
				markup = "<span color='grey'>%s</span>\n".printf (_("Loading…"));
			}
		}

		cell.set ("markup", markup);
	}

	private void file_size_renderer_func (CellLayout   cell_layout,
	                                      CellRenderer cell,
	                                      TreeModel    tree_model,
	                                      TreeIter     iter) {
		string size;

		background_renderer_func (cell_layout, cell, tree_model, iter);
		tree_model.get (iter, 4, out size, -1);

		if (size != null) {
			size = GLib.format_size (int.parse (size));
		}

		cell.set ("text", size);
	}

	private void file_date_renderer_func (CellLayout   cell_layout,
	                                      CellRenderer cell,
	                                      TreeModel    tree_model,
	                                      TreeIter     iter) {
		string date;

		background_renderer_func (cell_layout, cell, tree_model, iter);
		tree_model.get (iter, 5, out date, -1);

		if (date != null) {
			date = tracker_time_format_from_iso8601 (date);
		}

		cell.set ("text", date);
	}

	private void category_detail_renderer_func (CellLayout   cell_layout,
	                                            CellRenderer cell,
	                                            TreeModel    tree_model,
	                                            TreeIter     iter) {
		Tracker.Query.Type category;
		string markup = null;
		string detail;

		background_renderer_func (cell_layout, cell, tree_model, iter);
		tree_model.get (iter, 4, out detail, 7, out category, -1);

		if (detail == null) {
			cell.set ("markup", null);
			return;
		}

		switch (category) {
		case Tracker.Query.Type.FOLDERS:
		case Tracker.Query.Type.MAIL:
		case Tracker.Query.Type.BOOKMARKS:
			detail = tracker_time_format_from_iso8601 (detail);
			break;
		case Tracker.Query.Type.MUSIC:
		case Tracker.Query.Type.VIDEOS:
			detail = tracker_time_format_from_seconds (detail);
			break;
		case Tracker.Query.Type.DOCUMENTS:
			detail = ngettext ("%d Page", "%d Pages", int.parse (detail)).printf (int.parse (detail));
			break;
		case Tracker.Query.Type.IMAGES:
			detail = GLib.format_size (int.parse (detail));
			break;
		}

		markup = "<span color='grey'><small>%s</small></span>".printf (Markup.escape_text (detail));
		cell.set ("markup", markup);
	}

	private void setup_menus () {
		// Set up context menu
		view.button_press_event.connect (view_button_press_event);

		context_menu = new Gtk.Menu ();

		var item = new Gtk.MenuItem.with_mnemonic (_("_Show Parent Directory"));
		item.activate.connect (context_menu_directory_clicked);
		context_menu.append (item);

		var separator = new SeparatorMenuItem ();
		context_menu.append (separator);

		item = new Gtk.MenuItem.with_mnemonic (_("_Tags…"));
		item.activate.connect (context_menu_tags_clicked);
		context_menu.append (item);

		context_menu.show_all ();
	}

	private bool view_button_press_event (Gtk.Widget widget, Gdk.EventButton event) {
		if (event.button == 3) {
			if (get_selected_path () != null) {
				context_menu.popup (null, null, null, event.button, event.time);
			}
		}

		return false;
	}

	private TreeModel? get_model () {
		switch (display) {
		case Display.CATEGORIES:
		case Display.FILE_LIST:
			TreeView v = (TreeView) view;
			return v.get_model ();

		case Display.FILE_ICONS:
			IconView v = (IconView) view;
			return v.get_model ();
		default:
			break;
		}

		return null;
	}

	private TreePath? get_selected_path () {
		switch (display) {
		case Display.CATEGORIES:
		case Display.FILE_LIST:
			TreeView v = (TreeView) view;
			TreeSelection s = v.get_selection ();
			List<TreePath> selected = s.get_selected_rows (null);

			return selected.nth_data (0);

		case Display.FILE_ICONS:
			IconView v = (IconView) view;
			List<TreePath> selected = v.get_selected_items ();

			return selected.nth_data (0);

		default:
			break;
		}

		return null;
	}

	private void context_menu_directory_clicked () {
		TreeModel model = get_model ();
		TreePath path = get_selected_path ();

		tracker_model_launch_selected_parent_dir (model, path, 1);
	}

	private void context_menu_tags_clicked () {
		TreeModel model = get_model ();
		TreePath path = get_selected_path ();
		TreeIter iter;
		model.get_iter (out iter, path);

		weak string uri;
		model.get (iter, 1, out uri);

		if (uri == null) {
			return;
		}

		debug ("Showing tags dialog for uri:'%s'", uri);

		// Create dialog and embed vbox.
		Dialog dialog = new Dialog.with_buttons (_("Tags"),
		                                         (Window) this.get_toplevel (),
		                                         DialogFlags.MODAL | DialogFlags.DESTROY_WITH_PARENT,
		                                         Stock.CLOSE, ResponseType.CLOSE,
		                                         null);
		dialog.set_default_size (400, 300);
		dialog.border_width = 12;
		dialog.response.connect (() => {
			dialog.destroy ();
		});

		List<string> files = null;
		files.prepend (uri);
		VBox vbox = new TrackerTagsView ((owned) files);

		var content = dialog.get_content_area () as Box;
		content.pack_start (vbox, true, true, 6);
		content.spacing = 10;

		((Widget) dialog).show_all ();
		dialog.run ();
	}
}
