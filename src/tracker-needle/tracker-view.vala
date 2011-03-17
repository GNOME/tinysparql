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

	public ResultStore store {
		get;
		private set;
	}

	private Widget view = null;

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

	public View (Display? _display = Display.NO_RESULTS, ResultStore? _store) {
		set_policy (PolicyType.NEVER, PolicyType.AUTOMATIC);

		display = _display;

		if (_store != null) {
			store = _store;
			debug ("using store:%p", store);
		} else {
			// Setup treeview
			store = new ResultStore (6);
			store.row_changed.connect (store_row_changed);

			store.add_query (Tracker.Query.Type.APPLICATIONS,
			                 "?urn",
			                 "tracker:coalesce(nfo:softwareCmdLine(?urn), ?urn)",
			                 "tracker:coalesce(nie:title(?urn), nfo:fileName(?urn))",
			                 "nie:comment(?urn)",
			                 "\"\"",
			                 "\"\"");

			store.add_query (Tracker.Query.Type.IMAGES,
			                 "?urn",
			                 "nie:url(?urn)",
			                 "tracker:coalesce(nie:title(?urn), nfo:fileName(?urn))",
			                 "fn:string-join((nfo:height(?urn), nfo:width(?urn)), \" x \")",
			                 "nfo:fileSize(?urn)",
			                 "nie:url(?urn)");
			store.add_query (Tracker.Query.Type.MUSIC,
			                 "?urn",
			                 "nie:url(?urn)",
			                 "tracker:coalesce(nie:title(?urn), nfo:fileName(?urn))",
			                 "fn:string-join((?performer, ?album), \" - \")",
			                 "nfo:duration(?urn)",
			                 "nie:url(?urn)");
			store.add_query (Tracker.Query.Type.VIDEOS,
			                 "?urn",
			                 "nie:url(?urn)",
			                 "tracker:coalesce(nie:title(?urn), nfo:fileName(?urn))",
			                 "\"\"",
			                 "nfo:duration(?urn)",
			                 "nie:url(?urn)");
			store.add_query (Tracker.Query.Type.DOCUMENTS,
			                 "?urn",
			                 "nie:url(?urn)",
			                 "tracker:coalesce(nie:title(?urn), nfo:fileName(?urn))",
			                 "tracker:coalesce(nco:fullname(?creator), nco:fullname(?publisher))",
			                 "fn:concat(nfo:pageCount(?urn), \" Pages\")",
			                 "nie:url(?urn)");
			store.add_query (Tracker.Query.Type.MAIL,
			                 "?urn",
			                 "nie:url(?urn)",
			                 "tracker:coalesce(nco:fullname(?sender), nco:nickname(?sender), nco:emailAddress(?sender))",
			                 "tracker:coalesce(nmo:messageSubject(?urn))",
			                 "nmo:receivedDate(?urn)",
			                 "fn:concat(\"To: \", tracker:coalesce(nco:fullname(?to), nco:nickname(?to), nco:emailAddress(?to)))");
			store.add_query (Tracker.Query.Type.FOLDERS,
			                 "?urn",
			                 "nie:url(?urn)",
			                 "tracker:coalesce(nie:title(?urn), nfo:fileName(?urn))",
			                 "tracker:coalesce(nie:url(?parent), \"\")",
			                 "nfo:fileLastModified(?urn)",
			                 "?tooltip");

			debug ("Creating store:%p", store);
		}

		switch (display) {
		case Display.NO_RESULTS:
			Label l;

			l = new Label ("");

			string message = _("No Search Results");
			string markup = @"<big>$message</big>";
			
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
			add_with_viewport (view);
		} else {
			add (view);
			setup_model ();
		}

		base.show_all ();
	}

	private void setup_model () {
		switch (display) {
		case Display.FILE_ICONS: {
			IconView iv = (IconView) view;

			iv.set_model (store);
			iv.set_item_width (96);
			iv.set_selection_mode (SelectionMode.SINGLE);
			iv.set_pixbuf_column (1);
			iv.set_text_column (4);

			break;
		}

		case Display.FILE_LIST: {
			TreeViewColumn col;
			TreeView tv = (TreeView) view;

			tv.set_model (store);
			tv.set_tooltip_column (8);
			tv.set_rules_hint (false);
			tv.set_grid_lines (TreeViewGridLines.VERTICAL);
			tv.set_headers_visible (true);
			tv.set_fixed_height_mode (true);

			var renderer1 = new CellRendererPixbuf ();
			var renderer2 = new Tracker.CellRendererText ();

			col = new TreeViewColumn ();
			col.set_sizing (TreeViewColumnSizing.FIXED);
			col.pack_start (renderer1, false);
			col.add_attribute (renderer1, "pixbuf", 0);
			renderer1.xpad = 5;
			renderer1.ypad = 5;

			col.pack_start (renderer2, true);
			col.add_attribute (renderer2, "text", 4);
			renderer2.ellipsize = Pango.EllipsizeMode.MIDDLE;
			renderer2.show_fixed_height = false;

			col.set_title (_("File"));
			col.set_resizable (true);
			col.set_expand (true);
			col.set_sizing (TreeViewColumnSizing.AUTOSIZE);
			col.set_cell_data_func (renderer1, renderer_background_func);
			col.set_cell_data_func (renderer2, renderer_background_func);
			tv.append_column (col);

			var renderer3 = new Tracker.CellRendererText ();
			col = new TreeViewColumn ();
			col.set_sizing (TreeViewColumnSizing.FIXED);
			col.pack_start (renderer3, true);
			col.add_attribute (renderer3, "text", 6);
			col.set_title (_("Last Changed"));
			col.set_cell_data_func (renderer3, renderer_background_func);
			tv.append_column (col);

			var renderer4 = new Tracker.CellRendererText ();
			col = new TreeViewColumn ();
			col.set_sizing (TreeViewColumnSizing.FIXED);
			col.pack_start (renderer4, true);
			col.add_attribute (renderer4, "text", 7);
			col.set_title (_("Size"));
			col.set_cell_data_func (renderer4, renderer_background_func);
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

			var renderer1 = new CellRendererPixbuf ();
			var renderer2 = new Gtk.CellRendererText ();

			col = new TreeViewColumn ();
			col.set_sizing (TreeViewColumnSizing.FIXED);
			col.pack_start (renderer1, false);
			col.add_attribute (renderer1, "pixbuf", 6);
			renderer1.xpad = 5;
			renderer1.ypad = 5;

			col.pack_start (renderer2, true);
			col.set_cell_data_func (renderer2, text_renderer_func);
//			col.add_attribute (renderer2, "text", 1); //4);
//			col.add_attribute (renderer2, "subtext", 5);
                        renderer2.set_fixed_height_from_font (2);
			renderer2.ellipsize = Pango.EllipsizeMode.MIDDLE;
//			renderer2.show_fixed_height = true;

			col.set_title (_("Item"));
			col.set_resizable (true);
			col.set_expand (true);
			col.set_sizing (TreeViewColumnSizing.AUTOSIZE);
			col.set_cell_data_func (renderer1, renderer_background_func);
			tv.append_column (col);

//			var renderer3 = new Gtk.CellRendererText ();
//			col = new TreeViewColumn ();
//			col.pack_start (renderer3, true);
//			col.add_attribute (renderer3, "text", 3);
//			col.set_title (_("Item Detail"));
//			col.set_cell_data_func (renderer3, cell_renderer_func);
//			tv.append_column (col);

// 			var renderer4 = new Tracker.CellRendererText ();
// 			col = new TreeViewColumn ();
// 			col.set_sizing (TreeViewColumnSizing.FIXED);
// 			col.pack_start (renderer4, true);
// 			col.add_attribute (renderer4, "text", 4);
// 			col.set_title (_("Size"));
//			col.set_cell_data_func (renderer4, cell_renderer_func);
// 			tv.append_column (col);

			break;
		}
		}
	}

	private void renderer_background_func (CellLayout   cell_layout,
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

		renderer_background_func (cell_layout, cell, tree_model, iter);
		tree_model.get (iter, 2, out text, 3, out subtext, -1);

		if (text != null) {
			markup = Markup.escape_text (text);
		}

		if (subtext != null) {
			markup += "\n<small><span color='grey'>%s</span></small>".printf (Markup.escape_text (subtext));
		}

		if (markup == null) {
			markup = "<span color='grey'>%s</span>".printf (_("Loading..."));
		}

		cell.set ("markup", markup);
	}
}

