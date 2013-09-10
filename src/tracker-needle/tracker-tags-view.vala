/*
 * Copyright (C) 2011, Martyn Russell <martyn@lanedo.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

using Gtk;
using Tracker;

private class TagData {
	public TrackerTagsView tv;
	public Cancellable cancellable;
	public string tag_id;
	public TreeIter iter;
	public int items;
	public bool update;
	public bool selected;

	public TagData (string? _tag_id, TreeIter? _iter, bool _update, bool _selected, int _items, TrackerTagsView _tv) {
		debug ("Creating tag data");

		tv = _tv;
		cancellable = new Cancellable ();
		tag_id = _tag_id;

		if (_iter != null) {
			iter = _iter;
		}

		items = _items;
		update = _update;
		selected = _selected;
	}

	~TagData () {
		if (cancellable != null) {
			cancellable.cancel ();
			cancellable = null;
		}
	}
}

public class TrackerTagsView : VBox {
	private Sparql.Connection connection;
	private Cancellable cancellable;

	private List<TagData> tag_data_requests;
	private List<string> files;

	private ListStore store;

	private const string UI_FILE = "tracker-needle.ui";

	private VBox vbox;
	private Label label;
	private Entry entry;
	private Button button_add;
	private Button button_remove;
	private ScrolledWindow scrolled_window;
	private TreeView view;

	enum Col {
		SELECTION,
		TAG_ID,
		TAG_NAME,
		TAG_COUNT,
		TAG_COUNT_VALUE,
		N_COLUMNS
	}

	enum Selection {
		INCONSISTENT = -1,
		FALSE = 0,
		TRUE = 1
	}

	public TrackerTagsView (owned List<string>? _files) {
		try {
			connection = Sparql.Connection.get ();
		} catch (GLib.Error e) {
			warning ("Could not get Sparql connection: %s", e.message);
		}

		files = (owned) _files;

		cancellable = new Cancellable ();

		store = new ListStore (Col.N_COLUMNS,
		                       typeof (int),      /* Selection type */
		                       typeof (string),   /* Tag ID */
		                       typeof (string),   /* Tag Name */
		                       typeof (string),   /* Tag Count String */
		                       typeof (int));     /* Tag Count */

		create_ui ();
	}

	~TrackerTagsView () {
		if (cancellable != null) {
			cancellable.cancel ();
			cancellable = null;
		}

		if (files != null) {
			foreach (string url in files) {
				url = null;
			}

			files = null;
		}

		if (tag_data_requests != null) {
			foreach (TagData td in tag_data_requests) {
				td = null;
			};

			tag_data_requests = null;
		}
	}

	public void hide_label () {
		label.hide ();
	}

	public void set_files (List<string>? _files) {
		if (files != null) {
			foreach (string url in files) {
				url = null;
			}

			files = null;
		}

		if (_files != null) {
			foreach (string url in _files) {
				files.prepend (url);
			}

			files.reverse ();
		}

		update_for_files ();
	}

	private void show_error_dialog (string action, Error e) {
		string str = e.message != null ? e.message : _("No error given");

		var msg = new MessageDialog (null,
		                             DialogFlags.MODAL,
		                             MessageType.ERROR,
		                             ButtonsType.CLOSE,
		                             "%s",
		                             action);
		msg.format_secondary_text (str);
		msg.response.connect (() => {
			msg.destroy ();
		});

		msg.run ();
	}

	[CCode (instance_pos = -1)]
	public void button_remove_clicked_cb (Button source) {
		debug ("Remove clicked");

		TreeIter iter;
		TreeModel model;

		TreeSelection selection = view.get_selection ();

		if (selection.get_selected (out model, out iter)) {
			string id;

			model.get (iter, Col.TAG_ID, out id, -1);

			TagData td = new TagData (id, iter, false, true, 1, this);
			tag_data_requests.prepend (td);

			remove_tag.begin (td);
		}
	}

	[CCode (instance_pos = -1)]
	public void button_add_clicked_cb (Button source) {
		debug ("Add clicked");
		unowned string tag = entry.get_text ();
		add_tag.begin (tag);
	}

	[CCode (instance_pos = -1)]
	public void entry_tag_activated_cb (Entry source) {
		debug ("Entry activated");
		((Widget) button_add).activate ();
	}

	[CCode (instance_pos = -1)]
	public void entry_tag_changed_cb (Editable source) {
		debug ("Entry changed");

		unowned string tag = entry.get_text ();
		TreeIter iter;

		if (find_tag (tag, out iter)) {
			((Widget) button_add).set_sensitive (false);
		} else {
			((Widget) button_add).set_sensitive ((tag != null && tag != ""));
		}
	}

	[CCode (instance_pos = -1)]
	public void treeview_tags_cell_toggled_cb (CellRendererToggle source, string path_string) {
		debug ("Treeview row cell toggled");
		TreePath path = new TreePath.from_string (path_string);
		model_toggle_row.begin (path);
	}

	[CCode (instance_pos = -1)]
	public void treeview_tags_row_selected_cb (TreeSelection selection) {
		debug ("Treeview row selected");

		TreeIter iter;
		TreeModel model;

		if (selection.get_selected (out model, out iter)) {
			button_remove.set_sensitive (true);
		} else {
			button_remove.set_sensitive (false);
		}
	}

	[CCode (instance_pos = -1)]
	public void treeview_tags_row_activated_cb (TreeView source, TreePath path, TreeViewColumn column) {
		debug ("Treeview row activated");
		model_toggle_row.begin (path);
	}

	[CCode (instance_pos = -1)]
	private void treeview_tags_toggle_cell_data_func (Gtk.CellLayout layout, Gtk.CellRenderer cell, Gtk.TreeModel model, Gtk.TreeIter iter) {
		int selection;

		model.get (iter, Col.SELECTION, out selection, -1);
		((Gtk.CellRendererToggle) cell).set_active (selection == Selection.TRUE);
		((Gtk.CellRendererToggle) cell).inconsistent = (selection == Selection.INCONSISTENT);
	}

	private void create_ui () {
		var builder = new Gtk.Builder ();

		try {
			debug ("Trying to use UI file:'%s'", SRCDIR + UI_FILE);
			builder.add_from_file (SRCDIR + UI_FILE);
		} catch (GLib.Error e) {
			//now the install location
			try {
				debug ("Trying to use UI file:'%s'", UIDIR + UI_FILE);
				builder.add_from_file (UIDIR + UI_FILE);
			} catch (GLib.Error e) {
				var msg = new MessageDialog (null,
				                             DialogFlags.MODAL,
				                             MessageType.ERROR,
				                             ButtonsType.CANCEL,
				                             "Failed to load UI file, %s\n",
				                             e.message);
				msg.run ();
				Gtk.main_quit ();
			}
		}

		// Get widgets from .ui file
		vbox = builder.get_object ("vbox_tags") as VBox;
		label = builder.get_object ("label_tag") as Label;
		entry = builder.get_object ("entry_tag") as Entry;
		button_add = builder.get_object ("button_add") as Button;
		button_remove = builder.get_object ("button_remove") as Button;
		scrolled_window = builder.get_object ("scrolled_window_tags") as ScrolledWindow;
		view = builder.get_object ("treeview_tags") as TreeView;

		// Set up signal handlers (didn't work from glade)
		((Editable) entry).changed.connect (entry_tag_changed_cb);
		button_add.clicked.connect (button_add_clicked_cb);
		button_remove.clicked.connect (button_remove_clicked_cb);

		// Set up treeview
		Gtk.TreeViewColumn col;
		Gtk.CellRenderer renderer;

		// List column: Tag
		renderer = new CellRendererToggle ();
		renderer.xpad = 5;
		renderer.ypad = 5;
		((CellRendererToggle) renderer).toggled.connect (treeview_tags_cell_toggled_cb);
		((CellRendererToggle) renderer).set_radio (false);

		col = new Gtk.TreeViewColumn ();
		col.set_title ("-");
		col.set_resizable (false);
		col.set_sizing (Gtk.TreeViewColumnSizing.FIXED);
		col.set_fixed_width (25);
		col.pack_start (renderer, false);
		col.set_cell_data_func (renderer, treeview_tags_toggle_cell_data_func);
		view.append_column (col);

		// List column: Name
		renderer = new CellRendererText ();
		renderer.xpad = 5;
		renderer.ypad = 5;
		((CellRendererText) renderer).ellipsize = Pango.EllipsizeMode.END;
		((CellRendererText) renderer).ellipsize_set = true;

		col = new Gtk.TreeViewColumn ();
		col.set_title (_("Name"));
		col.set_resizable (true);
		col.set_sizing (Gtk.TreeViewColumnSizing.AUTOSIZE);
		col.set_expand (true);
		col.pack_start (renderer, true);
		col.add_attribute (renderer, "text", Col.TAG_NAME);

		view.append_column (col);

		// List coumnn: Count
		renderer = new CellRendererText ();
		renderer.xpad = 5;
		renderer.ypad = 5;

		col = new Gtk.TreeViewColumn ();
		col.set_title ("-");
		col.set_resizable (false);
		col.set_sizing (Gtk.TreeViewColumnSizing.FIXED);
		col.set_fixed_width (50);
		col.pack_end (renderer, false);
		col.add_attribute (renderer, "text", Col.TAG_COUNT);

		view.append_column (col);

		// Selection signals
		var selection = view.get_selection ();
		selection.changed.connect (treeview_tags_row_selected_cb);

		// Model details
		TreeModel model = store as TreeModel;
		TreeSortable sortable = model as TreeSortable;

		view.set_model (model);
		view.row_activated.connect (treeview_tags_row_activated_cb);
		sortable.set_sort_column_id (2, SortType.ASCENDING);

		// Add vbox to this widget's vbox
		vbox.unparent ();
		base.pack_start (vbox, true, true, 0);

		// Set controls up based on selected file(s)
		// NOTE: This can't occur before the view is created
		update_for_files ();

		query_tags.begin ();
	}

	private void update_for_files () {
		if (files != null) {
			string str = dngettext (null,
			                        "_Set the tags you want to associate with the %d selected item:",
			                        "_Set the tags you want to associate with the %d selected items:",
			                        files.length ()).printf (files.length ());
			label.set_text_with_mnemonic (str);
			vbox.sensitive = true;
		} else {
			label.set_text (_("No items currently selected"));
			vbox.sensitive = false;
		}

		query_tags_for_files.begin ();
	}

	private async void model_toggle_row (TreePath path) {
		TreeModel model;
		TreeIter iter;
		string id, tag;
		int selection;

		model = view.get_model ();

		if (model.get_iter (out iter, path) == false) {
			return;
		}

		model.get (iter,
		           Col.SELECTION, out selection,
		           Col.TAG_ID, out id,
		           Col.TAG_NAME, out tag,
		           -1);

		int new_value = selection == Selection.FALSE ? Selection.TRUE : Selection.FALSE;

		string tag_escaped = sparql_get_escaped_string (tag);
		string filter = sparql_get_filter_string (null);
		string query = null;

		TagData td;

		// NOTE: Was if (selection) ...
		if (new_value != Selection.FALSE) {
			// NB: ?f is used in filter.
			query = "INSERT {
			           ?urn nao:hasTag ?label
			         } WHERE {
			           ?urn nie:url ?f .
			           ?label nao:prefLabel %s .
			           %s
			         }".printf (tag_escaped, filter);
		} else {
			// NB: ?f is used in filter.
			query = "DELETE {
			           ?urn nao:hasTag ?label
			         } WHERE {
			           ?urn nie:url ?f .
			           ?label nao:prefLabel %s .
			           %s
			         }".printf (tag_escaped, filter);

			/* Check if there are any files left with this tag and
			 * remove tag if not.
			 */
			td = new TagData (id, iter, false, true, 1, this);
			tag_data_requests.prepend (td);

			query_files_for_tag_id.begin (td);
		}

		filter = null;
		tag_escaped = null;

		if (connection == null) {
			warning ("Can't update tags, no SPARQL connection available");
			return;
		}

		debug ("Updating tags for uris");

		entry.set_sensitive (false);

		td = new TagData (id, iter, true, (new_value != Selection.FALSE), 1, this);
		tag_data_requests.prepend (td);

		try {
			yield connection.update_async (query, Priority.DEFAULT, td.cancellable);

			debug ("Updated tags");
			update_tag_data (td);

			entry.set_text ("");
		} catch (GLib.Error e) {
			warning ("Could not run Sparql update query: %s", e.message);
			show_error_dialog (_("Could not update tags"), e);
		}

		tag_data_requests.remove (td);
		td = null;

		entry.set_sensitive (true);
	}

	private bool find_tag (string tag, out TreeIter iter) {
		TreeIter found_iter = { 0 };

		iter = found_iter;

		if (tag == null || tag == "") {
			return false;
		}

		TreeModel model = view.get_model ();
		bool found = false;

		model.foreach ((model, path, foreach_iter) => {
			string foreach_tag;

			model.get (foreach_iter, Col.TAG_NAME, out foreach_tag, -1);

			if (foreach_tag != null && foreach_tag == tag) {
				found = true;
				found_iter = foreach_iter;
				return true;
			}

			return false;
		});

		if (found == true) {
			iter = found_iter;
			return true;
		}

		return false;
	}

	private async void remove_tag (TagData td) {
		if (connection == null) {
			warning ("Can't remove tag '%s', no SPARQL connection available", td.tag_id);
			tag_data_requests.remove (td);
			td = null;
			return;
		}

		string query = "DELETE { <%s> a rdfs:Resource }".printf (td.tag_id);

		try {
			yield connection.update_async (query, Priority.DEFAULT, td.cancellable);

			debug ("Tag removed");
			store.remove (td.iter);
		} catch (GLib.Error e) {
			warning ("Could not run Sparql update query: %s", e.message);
			show_error_dialog (_("Could not remove tag"), e);
		}

		tag_data_requests.remove (td);
		td = null;
	}

	private async void add_tag (string tag) {
		string query = null;

		if (connection == null) {
			warning ("Can't add tag '%s', no SPARQL connection available", tag);
			return;
		}

		entry.set_sensitive (false);

		if (files != null && files.length () > 0) {
			query = "";

			string filter = sparql_get_filter_string (null);
			string tag_escaped = sparql_get_escaped_string (tag);

			foreach (string url in files) {
				query += "INSERT {
				            _:file a nie:DataObject ;
				             nie:url '%s'
				          } WHERE {
				            OPTIONAL {
				               ?file a nie:DataObject ;
				               nie:url '%s'
				            } .
				            FILTER (!bound(?file))
				          }".printf (url, url);
			}

			query += "INSERT {
			            _:tag a nao:Tag;
			            nao:prefLabel %s .
			          } WHERE {
			            OPTIONAL {
			              ?tag a nao:Tag ;
			              nao:prefLabel %s
			            } .
			            FILTER (!bound(?tag))
			          }
			          INSERT {
			            ?urn nao:hasTag ?label
			          } WHERE {
			            ?urn nie:url ?f .
			            ?label nao:prefLabel %s
			            %s
			          }".printf (tag_escaped, tag_escaped, tag_escaped, filter);
		} else {
			string tag_label_escaped = sparql_get_escaped_string (tag);

			query = "INSERT {
			           _:tag a nao:Tag ;
			           nao:prefLabel %s .
			         } WHERE {
			           OPTIONAL {
			             ?tag a nao:Tag ;
			             nao:prefLabel %s
			           } .
			           FILTER (!bound(?tag))
			         }".printf (tag_label_escaped, tag_label_escaped);
		}

		TagData td = new TagData (null, null, false, true, (int) files.length (), this);
		tag_data_requests.prepend (td);

		try {
			yield connection.update_async (query, Priority.DEFAULT, td.cancellable);

			debug ("Updated tags");
			update_tag_data (td);

			// Only do this on success
			entry.set_text ("");
		} catch (GLib.Error e) {
			warning ("Could not run Sparql update query: %s", e.message);
			show_error_dialog (_("Could not update tags"), e);
		}

		tag_data_requests.remove (td);
		td = null;

		entry.set_sensitive (true);
	}

	private void update_tag_data (TagData td) {
		unowned string tag = entry.get_text ();

		if (td.update == false) {
			TreeIter iter;

			debug ("Setting tag selection state to ON (new)");

			store.append (out iter);
			store.set (iter,
			           Col.TAG_ID, td.tag_id,
			           Col.TAG_NAME, tag,
			           Col.TAG_COUNT, "%d".printf (td.items),
			           Col.TAG_COUNT_VALUE, td.items,
			           Col.SELECTION, Selection.TRUE,
			           -1);
		} else if (td.selected == true) {
			debug ("Setting tag selection state to ON");

			store.set (td.iter, Col.SELECTION, Selection.TRUE, -1);

			tag_data_requests.prepend (td);
			query_files_for_tag_id.begin (td);
		} else {
			debug ("Setting tag selection state to FALSE");

			store.set (td.iter, Col.SELECTION, Selection.FALSE, -1);

			tag_data_requests.prepend (td);
			query_files_for_tag_id.begin (td);
		}
	}

	private void untoggle_all () {
		TreeModel model = view.get_model ();
		ListStore store = (ListStore) model;

		model.foreach ((model, path, foreach_iter) => {
			store.set (foreach_iter, Col.SELECTION, Selection.FALSE, -1);
			return false;
		});
	}

	private async void query_tags_for_files () {
		untoggle_all ();

		if (files == null) {
			return;
		}

		// Get tags for files only and make sure we toggle the list
		string files_filter = "";

		foreach (string url in files) {
			if (files_filter.length > 0) {
				files_filter += ",";
			}

			files_filter += "'%s'".printf (url);
		}

		string query = "select ?tag nao:prefLabel(?tag) WHERE { ?urn nao:hasTag ?tag . FILTER(nie:url(?urn) IN (%s)) } ORDER BY (?tag)".printf (files_filter);

		debug ("Getting tags for files selected...");

		try {
			Sparql.Cursor cursor = yield connection.query_async (query, null);

			while (yield cursor.next_async ()) {
				debug ("Toggling tags...");

				unowned string id = cursor.get_string (0);
				unowned string label = cursor.get_string (1);

				debug ("  Enabling tag:'%s', label:'%s'", id, label);

				TreeIter iter;
				if (find_tag (label, out iter)) {
					store.set (iter,
					           Col.SELECTION, Selection.TRUE,
					           -1);
				}
			}
		} catch (GLib.Error e) {
			warning ("Could not run Sparql query: %s", e.message);
			show_error_dialog (_("Could not retrieve tags for the current selection"), e);
		}
	}

	private async void query_tags () {
		// Get all tags
		string query = "SELECT ?urn ?label WHERE { ?urn a nao:Tag ; nao:prefLabel ?label . } ORDER BY ?label";

		debug ("Clearing tags in store");
		store.clear ();

		try {
			Sparql.Cursor cursor = yield connection.query_async (query, null);

			while (yield cursor.next_async ()) {

				debug ("Adding all tags...");

				unowned string id = cursor.get_string (0);
				unowned string label = cursor.get_string (1);

				debug ("  Adding tag id:'%s' with label:'%s' to store", id, label);

				TreeIter iter;
				store.append (out iter);

				store.set (iter,
				           Col.TAG_ID, id,
				           Col.TAG_NAME, label,
				           Col.SELECTION, Selection.FALSE,
				           -1);

				TagData td = new TagData (id, iter, false, true, 1, this);
				tag_data_requests.prepend (td);

				query_files_for_tag_id.begin (td);
			}
		} catch (GLib.Error e) {
			warning ("Could not run Sparql query: %s", e.message);
			show_error_dialog (_("Could not add tag"), e);
		}
	}

	private async void query_files_for_tag_id (TagData td) {
		if (connection == null) {
			warning ("Can't query files for tag id '%s', no SPARQL connection available", td.tag_id);
			tag_data_requests.remove (td);
			td = null;
			return;
		}

		string query = "SELECT ?url WHERE { ?urn a rdfs:Resource ; nie:url ?url ; nao:hasTag <%s> . }".printf (td.tag_id);

		try {
			Sparql.Cursor cursor = yield connection.query_async (query, td.cancellable);

			uint has_tag_in_selection = 0;
			uint files_with_tag = 0;
			uint files_selected = files.length ();

			while (yield cursor.next_async ()) {
				files_with_tag++;

				foreach (string url in files) {
					unowned string url_returned = cursor.get_string (0);

					debug ("--> '%s' vs '%s'", url, url_returned);

					if (url_returned == null) {
						continue;
					}

					if (url_returned == url) {
						has_tag_in_selection++;
						break;
					}
				}
			}

			debug ("Querying files with tag, in selection:%ld, in total:%ld, selected:%ld",
			       has_tag_in_selection, files_with_tag, files_selected);

			if (has_tag_in_selection == 0) {
				store.set (td.iter, Col.SELECTION, Selection.FALSE, -1);
			} else if (files_selected != has_tag_in_selection) {
				store.set (td.iter, Col.SELECTION, Selection.INCONSISTENT, -1);
			} else {
				store.set (td.iter, Col.SELECTION, Selection.TRUE, -1);
			}

			string str = "%ld".printf (files_with_tag);
			store.set (td.iter, Col.TAG_COUNT, str, Col.TAG_COUNT_VALUE, files_with_tag, -1);

			debug ("Tags for file updated");
		} catch (GLib.Error e) {
			warning ("Could not run Sparql query: %s", e.message);
			show_error_dialog (_("Could not update tags for file"), e);
		}

		tag_data_requests.remove (td);
		td = null;
	}

	private string sparql_get_filter_string (string? tag) requires (files != null && files.length () > 0) {
		string filter = "FILTER (";

		if (tag != null && tag != "") {
			filter += "(";
		}

		bool first = true;

		foreach (string url in files) {
			if (!first) {
				filter += " || ";
			}

			filter += "?f = \"%s\"".printf (url);
			first = false;
		}

		if (tag != null && tag != "") {
			filter += ") && ?t = <%s>".printf (tag);
		}

		filter += ")";

		return filter;
	}

	private string sparql_get_escaped_string (string str) {
		string escaped = Sparql.escape_string (str);
		return "\"%s\"".printf (escaped);
	}
}

