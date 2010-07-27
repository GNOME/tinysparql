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

// To run this test:
// -===-------------
//
// tracker-control -k
// rm ~/.cache/tracker/meta.db
// export TRACKER_DEBUG_MAKE_JOURNAL_READER_GO_VERY_SLOW=yes
// tracker-store
// ./busy-handling-test

[DBus (name = "org.freedesktop.Tracker1.Resources")]
private interface Resources : GLib.Object {
	[DBus (name = "SparqlQuery", timeout = 99999999999)]
	public abstract async string[,] sparql_query_async (string query) throws DBus.Error;
}

[DBus (name = "org.freedesktop.Tracker1.Status")]
private interface Status: GLib.Object {
	public signal void progress (string status, double progress);
}

public class TestApp {
	static DBus.Connection connection;
	static Resources resources_object;
	static Status status_object;
	int res = -1;
	MainLoop loop;
	bool initialized = false;

	public TestApp ()
	requires (!initialized) {
		try {
			connection = DBus.Bus.get (DBus.BusType.SESSION);
			resources_object = (Resources) connection.get_object ("org.freedesktop.Tracker1",
			                                                      "/org/freedesktop/Tracker1/Resources",
			                                                      "org.freedesktop.Tracker1.Resources");
			status_object = (Status) connection.get_object ("org.freedesktop.Tracker1",
			                                                "/org/freedesktop/Tracker1/Status",
			                                                "org.freedesktop.Tracker1.Status");

			status_object.progress.connect (on_status_cb);

		} catch (DBus.Error e) {
			warning ("Could not connect to D-Bus service: %s", e.message);
			initialized = false;
			res = -1;
			return;
		}
		initialized = true;
	}


	void on_status_cb (string status, double progress) {
		print ("%s: %f\n", status, progress);
	}

	async void do_query_tests_async () {
		try {
			string[,] results = yield resources_object.sparql_query_async ("SELECT ?u { ?u a rdfs:Resource }");
			foreach (string res in results) {
				print ("%s\n", res);
			}
		} catch (GLib.Error e) {
			print ("Fail: %s\n", e.message);
			res = -1;
		}
	}

	async void do_async_query_tests () {
		yield do_query_tests_async ();

		print ("Async tests done, now I can quit the mainloop\n");
		loop.quit ();
	}

	bool in_mainloop () {
		do_async_query_tests ();

		return false;
	}

	public int run () {
		loop = new MainLoop (null, false);

		Idle.add (in_mainloop);

		loop.run ();

		return res;
	}
}

int main (string[] args) {
	TestApp app = new TestApp ();

	return app.run ();
}
