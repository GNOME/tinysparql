//
// Copyright 2010, Carlos Garnacho <carlos@lanedo.com>
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

public class Tracker.ResultStore : Gtk.TreeModel, GLib.Object {
	private GLib.Cancellable cancellable;

	private struct ResultNode {
		string [] values;
	}
	private struct CategoryNode {
		Tracker.Query.Type type;
		string [] args;
		ResultNode [] results;
		Gdk.Pixbuf pixbuf;
		int count;
	}
	private CategoryNode [] categories;

	private class Operation : GLib.Object {
		public CategoryNode *node;
		public int offset;
	}
	private GenericArray<Operation> running_operations;
	private GenericArray<Operation> delayed_operations;

	private int n_columns;
	private int timestamp;

	private Operation * find_operation (GenericArray<Operation>  array,
					    CategoryNode            *node,
					    int                      offset) {
		Operation op;
		int i;

		for (i = 0; i < array.length; i++) {
			op = array[i];

			if (op.node == node &&
			    op.offset == offset) {
				return op;
			}
		}

		return null;
	}

	async void load_operation (Operation    op,
				   Cancellable? cancellable) {
		Tracker.Query query;
		Sparql.Cursor cursor = null;
		int i;

		try {
			cancellable.set_error_if_cancelled ();

			query = new Tracker.Query ();
			query.criteria = _search_term;
			query.limit = 100;
			query.offset = op.offset;

			cursor = yield query.perform_async (op.node.type, op.node.args);

			for (i = op.offset; i < op.offset + 100; i++) {
				ResultNode *result;
				TreeIter iter;
				TreePath path;
				bool b = false;
				int j;

				try {
					b = yield cursor.next_async ();
				} catch (GLib.Error ge) {
					warning ("Could not fetch row: %s\n", ge.message);
				}

				if (!b) {
					break;
				}

				result = &op.node.results[i];

				for (j = 0; j < n_columns - 1; j++) {
					result.values[j] = cursor.get_string (j);
				}

				// Emit row-changed
				iter = TreeIter ();
				iter.stamp = this.timestamp;
				iter.user_data = op.node;
				iter.user_data2 = result;
				iter.user_data3 = i.to_pointer ();

				path = this.get_path (iter);
				row_changed (path, iter);
			}

			running_operations.remove (op);
		} catch (GLib.IOError ie) {
			warning ("Could not load items: %s\n", ie.message);
			return;
		}

		if (delayed_operations.length > 0) {
			Operation next_to_start;

			// Take last added task from delayed queue and start it
			next_to_start = delayed_operations[delayed_operations.length - 1];
			delayed_operations.remove (next_to_start);
			running_operations.add (next_to_start);

			load_operation.begin (next_to_start, cancellable);
		} else if (running_operations.length == 0) {
			// finished processing
			this.active = false;
		}
	}

	private void add_operation (CategoryNode *cat,
				    int           offset) {
		Operation op = new Operation ();
		Operation old;

		op.node = cat;
		op.offset = offset;

		if (find_operation (running_operations, cat, offset) != null) {
			// Operation already running
			return;
		}

		// If the task is delayed, it will be either pushed
	        // to the running queue, or reordered to be processed
		// next in the delayed queue.
		old = find_operation (delayed_operations, cat, offset);

		if (old != null) {
			delayed_operations.remove (old);
		}

		this.active = true;

		// Running queue is limited to 2 simultaneous queries,
		// anything after that will be added to a different queue.
		if (running_operations.length < 2) {
			running_operations.add (op);

			// Start the operation right away
			load_operation.begin (op, cancellable);
		} else {
			// Reorder the operation if it was already there, else just add
			delayed_operations.add (op);
		}
	}

	async void load_category (CategoryNode *cat,
				  Cancellable?  cancellable) {
		uint count = 0;

		try {
			cancellable.set_error_if_cancelled ();

			Tracker.Query query = new Tracker.Query ();
			query.criteria = _search_term;

			count = yield query.get_count_async (cat.type);
		} catch (GLib.IOError ie) {
			warning ("Could not get count: %s\n", ie.message);
			return;
		}

		if (count != 0) {
			ResultNode *res;
			int i;

			Gtk.TreeIter iter;
			Gtk.TreePath path;

			cat.results.resize ((int) count);

			iter = TreeIter ();
			iter.stamp = this.timestamp;
			iter.user_data = cat;

			for (i = 0; i < count; i++) {
				res = &cat.results[i];
				res.values = new string[n_columns];

				iter.user_data2 = res;
				iter.user_data3 = i.to_pointer ();
				path = this.get_path (iter);

				cat.count++;
				row_inserted (path, iter);
			}

			iter.user_data2 = null;
			iter.user_data3 = null;
			path = get_path (iter);

			row_changed (path, iter);
		}

		if (running_operations.length == 0) {
			this.active = false;
		}
	}

	private void clear_results () {
		int i, j;

		for (i = 0; i < categories.length; i++) {
			CategoryNode *cat = &categories[i];
			TreeIter iter;
			TreePath path;

			if (cat.results.length == 0) {
				continue;
			}

			iter = TreeIter ();
			iter.stamp = this.timestamp;
			iter.user_data = cat;

			for (j = cat.count - 1; j >= 0; j--) {
				iter.user_data2 = &cat.results[j];
				iter.user_data3 = j.to_pointer ();
				path = get_path (iter);

				row_deleted (path);
				cat.count--;
			}

			iter.user_data2 = null;
			iter.user_data3 = null;
			path = get_path (iter);

			row_changed (path, iter);

			cat.results.resize (0);
			cat.count = 0;
		}
	}

	private string _search_term;
	public string search_term {
		get {
			return _search_term;
		}
		set {
			int i;

			_search_term = value;

			if (cancellable != null) {
				cancellable.cancel ();
			}

			clear_results ();
			this.active = true;

			this.timestamp++;

			for (i = 0; i < categories.length; i++) {
				load_category.begin (&categories[i], cancellable);
			}
		}
	}

	public bool active {
		get;
		private set;
	}

	private int find_nth_category_index (CategoryNode *node,
	                                     int           n) {
		int i;

		if (node == null) {
			// Count from the first one
			return n;
		}

		for (i = 0; i < categories.length; i++) {
			CategoryNode *cat;

			cat = &categories[i];

			if (cat == node) {
				return i + n;
			}
		}

		return -1;
	}

	private int filled_categories_count () {
		int i, n = 0;

		for (i = 0; i < categories.length; i++) {
			CategoryNode *cat;

			cat = &categories[i];

			if (cat.count > 0) {
				n++;
			}
		}

		return n;
	}

	public GLib.Type get_column_type (int index_) {
		if (index_ == n_columns - 1) {
			return typeof (Gdk.Pixbuf);
		} else {
			return typeof (string);
		}
	}

	public Gtk.TreeModelFlags get_flags () {
		return Gtk.TreeModelFlags.ITERS_PERSIST;
	}

	public bool get_iter (out Gtk.TreeIter iter,
	                      Gtk.TreePath     path) {
		unowned int [] indices = path.get_indices ();
		CategoryNode *cat;

		if (indices[0] >= categories.length) {
			iter.stamp = 0;
			return false;
		}

		cat = &categories[indices[0]];
		iter.stamp = this.timestamp;
		iter.user_data = cat;

		if (path.get_depth () == 2) {
			// it's a result
			if (indices[1] >= cat.count) {
				iter.stamp = 0;
				return false;
			}

			iter.user_data2 = &cat.results[indices[1]];
			iter.user_data3 = indices[1].to_pointer ();
		}

		return true;
	}

	public int get_n_columns () {
		return n_columns;
	}

	public Gtk.TreePath get_path (Gtk.TreeIter iter) {
		TreePath path = new TreePath ();
		CategoryNode *cat;
		int i;

		for (i = 0; i < categories.length; i++) {
			cat = &categories[i];

			if (cat == iter.user_data) {
				path.append_index (i);
				break;
			}
		}

		if (iter.user_data2 != null) {
			path.append_index ((int) iter.user_data3);
		}

		return path;
	}

	public void get_value (Gtk.TreeIter   iter,
	                       int            column,
	                       out GLib.Value value) {
		CategoryNode *cat;

		if (column > n_columns) {
			value.init (typeof (string));
			return;
		}

		cat = iter.user_data;
		value.init (this.get_column_type (column));

		if (iter.user_data2 == null) {
			if (column == 2) {
				switch (cat.type) {
				case Tracker.Query.Type.APPLICATIONS:
					value.set_string (_("Applications"));
					break;
				case Tracker.Query.Type.MUSIC:
					value.set_string (_("Music"));
					break;
				case Tracker.Query.Type.IMAGES:
					value.set_string (_("Images"));
					break;
				case Tracker.Query.Type.VIDEOS:
					value.set_string (_("Videos"));
					break;
				case Tracker.Query.Type.DOCUMENTS:
					value.set_string (_("Documents"));
					break;
				case Tracker.Query.Type.MAIL:
					value.set_string (_("Mail"));
					break;
				case Tracker.Query.Type.FOLDERS:
					value.set_string (_("Folders"));
					break;
				}
			} else if (column == n_columns - 1) {
				Gdk.Pixbuf pixbuf;

				pixbuf = cat.pixbuf;

				if (pixbuf == null) {
					var theme = IconTheme.get_for_screen (Gdk.Screen.get_default ());
					int size = 24;

					switch (cat.type) {
					case Tracker.Query.Type.APPLICATIONS:
						pixbuf = tracker_pixbuf_new_from_name (theme, "package-x-generic", size);
						break;
					case Tracker.Query.Type.MUSIC:
						pixbuf = tracker_pixbuf_new_from_name (theme, "audio-x-generic", size);
						break;
					case Tracker.Query.Type.IMAGES:
						pixbuf = tracker_pixbuf_new_from_name (theme, "image-x-generic", size);
						break;
					case Tracker.Query.Type.VIDEOS:
						pixbuf = tracker_pixbuf_new_from_name (theme, "video-x-generic", size);
						break;
					case Tracker.Query.Type.DOCUMENTS:
						pixbuf = tracker_pixbuf_new_from_name (theme, "x-office-presentation", size);
						break;
					case Tracker.Query.Type.MAIL:
						pixbuf = tracker_pixbuf_new_from_name (theme, "emblem-mail", size);
						break;
					case Tracker.Query.Type.FOLDERS:
						pixbuf = tracker_pixbuf_new_from_name (theme, "folder", size);
						break;
					}
				}

				value.set_object (pixbuf);
			}
		} else {
			ResultNode *result;
			int n_node;

			result = iter.user_data2;
			n_node = (int) iter.user_data3;

			if (result.values[0] != null) {
				if (column == n_columns - 1) {
					// No pixbuf ATM
					//value.set_object (null);
				} else {
					value.set_string (result.values[column]);
				}
			} else {
				n_node /= 100;
				n_node *= 100;

				add_operation (cat, n_node);
			}
		}
	}

	public bool iter_children (out Gtk.TreeIter iter,
	                           Gtk.TreeIter?    parent) {
		CategoryNode *cat;

		if (parent == null) {
			int i;

			if (categories.length == 0) {
				iter.stamp = 0;
				return false;
			}

			i = find_nth_category_index (null, 0);
			cat = &categories[i];
			iter.stamp = this.timestamp;
			iter.user_data = cat;
			return true;
		}

		if (parent.user_data2 != null) {
			iter.stamp = 0;
			return false;
		}

		cat = parent.user_data;

		if (cat.results.length <= 0) {
			iter.stamp = 0;
			return false;
		}

		iter.stamp = this.timestamp;
		iter.user_data = cat;
		iter.user_data2 = &cat.results[0];
		iter.user_data3 = 0.to_pointer ();

		return true;
	}

	public bool iter_has_child (Gtk.TreeIter iter) {
		if (iter.user_data2 == null) {
			CategoryNode *cat;

			cat = iter.user_data;
			return (cat.count > 0);
		}

		return false;
	}

	public int iter_n_children (Gtk.TreeIter? iter) {
		if (iter == null) {
			return categories.length - 1;
		}

		if (iter.user_data2 != null) {
			// a result doesn't have children
			return -1;
		}

		CategoryNode *cat = iter.user_data;

		return cat.count;
	}

	public bool iter_next (ref Gtk.TreeIter iter) {
		CategoryNode *cat;
		int i;

		cat = iter.user_data;

		if (iter.user_data2 == null) {
			i = find_nth_category_index (cat, 1);

			if (i < 0 || i >= categories.length) {
				iter.stamp = 0;
				return false;
			}

			iter.stamp = this.timestamp;
			iter.user_data = &categories[i];

			return true;
		} else {
			// Result node
			i = (int) iter.user_data3;
			i++;

			if (i >= cat.count) {
				iter.stamp = 0;
				return false;
			}

			iter.user_data2 = &cat.results[i];
			iter.user_data3 = i.to_pointer ();

			return true;
		}
	}

	public bool iter_nth_child (out Gtk.TreeIter iter,
	                            Gtk.TreeIter?    parent,
	                            int              n) {
		CategoryNode *cat;

		if (parent != null) {
			cat = parent.user_data;

			if (n >= cat.count) {
				iter.stamp = 0;
				return false;
			}

			iter.stamp = this.timestamp;
			iter.user_data = cat;
			iter.user_data2 = &cat.results[n];
			iter.user_data3 = n.to_pointer ();
			return true;
		} else {
			int index;

			index = find_nth_category_index (null, n);

			if (index < 0 ||
			    index >= categories.length) {
				iter.stamp = 0;
				return false;
			}

			cat = &categories[index];
			iter.stamp = this.timestamp;
			iter.user_data = cat;

			return true;
		}
	}

	public bool iter_parent (out Gtk.TreeIter iter,
	                         Gtk.TreeIter     child) {
		if (child.user_data2 != null) {
			// child within a category
			iter.stamp = this.timestamp;
			iter.user_data = child.user_data;
			iter.user_data2 = null;
			iter.user_data3 = null;
			return true;
		}

		iter.stamp = 0;
		return false;
	}

	public void ref_node (Gtk.TreeIter iter) {
	}

	public void unref_node (Gtk.TreeIter iter) {
	}

	public ResultStore (int _n_columns) {
		running_operations = new GenericArray<Operation?> ();
		delayed_operations = new GenericArray<Operation?> ();

		// Add an extra one for the pixbuf
		n_columns = _n_columns + 1;
		timestamp = 1;
	}

	public void add_query (Tracker.Query.Type type, ...) {
		var l = va_list ();
		string str = null;
		string [] args = null;
		CategoryNode cat;
		TreeIter iter;
		TreePath path;

		do {
			str = l.arg ();

			if (str != null) {
				args += str;
			}
		} while (str != null);

		if (args.length != n_columns - 1) {
			warning ("Arguments and number of columns doesn't match");
			return;
		}

		cat = CategoryNode ();
		cat.type = type;
		cat.args = args;
		cat.results = new ResultNode[0];

		categories += cat;

		iter = TreeIter ();
		iter.stamp = this.timestamp;
		iter.user_data = &categories[categories.length - 1];
		path = this.get_path (iter);

		row_inserted (path, iter);
	}

	public bool has_results () {
		return filled_categories_count () > 0;
	}
}
