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

public class Tracker.TagList : ScrolledWindow {
	static Sparql.Connection connection;
	private TreeView treeview;
	private ListStore store;
	private int offset;
	private int limit;

	public TagList () {
		limit = 100;

		// Set scrolling
		set_policy (PolicyType.NEVER, PolicyType.AUTOMATIC);
		set_size_request (175, -1);

		// Add widgets
		store = new ListStore (4,
		                       typeof (bool),    // Enabled
		                       typeof (string),  // Name
		                       typeof (string),  // Description
		                       typeof (string)); // Count
		treeview = new TreeView.with_model (store);

		treeview.set_headers_visible (true);

		Gtk.TreeViewColumn col;
		Gtk.CellRenderer renderer;

		col = new Gtk.TreeViewColumn ();
		col.set_title (_("Tags"));
		col.set_resizable (true);
		col.set_expand (true);
		col.set_sizing (Gtk.TreeViewColumnSizing.AUTOSIZE);

		// Do this later when we have more time
//		renderer = new CellRendererToggle ();
//		renderer.xpad = 5;
//		renderer.ypad = 5;
//		col.pack_start (renderer, false);
//		col.add_attribute (renderer, "active", 0);

		renderer = new Tracker.CellRendererText ();
		col.pack_start (renderer, true);
		col.add_attribute (renderer, "text", 1);
		col.add_attribute (renderer, "subtext", 2);

		renderer = new CellRendererText ();
		renderer.xpad = 5;
		renderer.ypad = 5;
		col.pack_end (renderer, false);
		col.add_attribute (renderer, "text", 3);
		treeview.append_column (col);

		add (treeview);
		base.show_all ();

		// Add data
		try {
			connection = Sparql.Connection.get ();
		} catch (Sparql.Error ea) {
			warning ("Could not get Sparql connection: %s", ea.message);
			return;
		} catch (GLib.IOError eb) {
			warning ("Could not get Sparql connection: %s", eb.message);
			return;
		}

		get_tags.begin ();
	}

	private async void get_tags () {
		string query = @"
		               SELECT 
		                 ?tag 
		                 ?label
		                 nao:description(?tag)
		                 COUNT(?urns) AS urns
		               WHERE {
		                 ?tag a nao:Tag ;
		                 nao:prefLabel ?label .
		                 OPTIONAL {
		                   ?urns nao:hasTag ?tag
		                 }
		               } 
		               GROUP BY ?tag 
		               ORDER BY ASC(?label) 
		               OFFSET $offset 
		               LIMIT $limit
		               ";
		
		debug ("Getting tags");

		Sparql.Cursor cursor = null;

		try {
			cursor = yield connection.query_async (query, null);

			while (yield cursor.next_async ()) {
				for (int i = 0; i < cursor.n_columns; i++) {
					if (i == 0) {
						debug ("--> %s", cursor.get_string (i));
					} else {
						debug ("  --> %s", cursor.get_string (i));
					}
				}

				// Insert into model
				TreeIter iter;
				store.append (out iter);

				store.set (iter,
				           0, false,                      // Enabled
				           1, cursor.get_string (1),      // Name
				           2, cursor.get_string (2),      // Description
				           3, cursor.get_string (3),      // Count
				           -1);
			}
		} catch (Sparql.Error ea) {
			warning ("Could not run Sparql query: %s", ea.message);
		} catch (GLib.IOError eb) {
			warning ("Could not run Sparql query: %s", eb.message);
		} catch (GLib.Error ec) {
			warning ("Could not run Sparql query: %s", ec.message);
		}

		debug ("  Done");
	}
}

