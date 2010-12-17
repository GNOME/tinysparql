/*
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

using Tracker;
using Tracker.Sparql;

const int max_signals = 1000;
const string title_data = "title";

struct Event {
	int graph_id;
	int subject_id;
	int pred_id;
	int object_id;
}

[DBus (name = "org.freedesktop.Tracker1.Resources")]
private interface Resources : GLib.Object {
	[DBus (name = "GraphUpdated")]
	public signal void graph_updated (string class_name, Event[] deletes, Event[] inserts);
	[DBus (name = "BatchSparqlUpdate")]
	public abstract async void batch_sparql_update_async (string query) throws Sparql.Error, DBus.Error;
	[DBus (name = "SparqlUpdate")]
	public abstract async void sparql_update_async (string query) throws Sparql.Error, DBus.Error;
}

public class TestApp {
	static DBus.Connection dbus_connection;
	static Resources resources_object;
	MainLoop loop;
	bool initialized = false;
	Sparql.Connection con;
	int count = 0;
	GLib.Timer t;

	public TestApp ()
	requires (!initialized) {
		try {
			con = Tracker.Sparql.Connection.get();
			dbus_connection = DBus.Bus.get (DBus.BusType.SESSION);
			resources_object = (Resources) dbus_connection.get_object ("org.freedesktop.Tracker1",
			                                                           "/org/freedesktop/Tracker1/Resources",
			                                                           "org.freedesktop.Tracker1.Resources");


			resources_object.graph_updated.connect (on_graph_updated_received);
			t = new GLib.Timer ();
			
		} catch (GLib.Error e) {
			warning ("Could not connect to D-Bus service: %s", e.message);
			initialized = false;
			return;
		}
		initialized = true;
	}

	private void on_graph_updated_received (string class_name, Event[] deletes, Event[] inserts) {
		foreach (Event insert in inserts)
			count++;
		print ("New class signal count=%d time=%lf\n", count, t.elapsed ());
	}

	private void insert_data () {
		int i;
		for (i = 0; i <= max_signals; i++) {
			string upqry = "DELETE { <%d> a rdfs:Resource }".printf(i);
			resources_object.sparql_update_async (upqry);
		}

		t.start();
		for (i = 0; i <= max_signals; i++) {
			string upqry = "INSERT { <%d> a nmm:MusicPiece ; nie:title '%s %d' }".printf(i, title_data, i);

			if (i == max_signals / 2) {
				resources_object.sparql_update_async (upqry);
			} else {
				resources_object.batch_sparql_update_async (upqry);
			}
		}
	}

	private bool in_mainloop () {
		insert_data ();
		return false;
	}

	public int run () {
		loop = new MainLoop (null, false);
		Idle.add (in_mainloop);
		loop.run ();
		return 0;
	}
}

int main (string[] args) {
	TestApp app = new TestApp ();

	return app.run ();
}
