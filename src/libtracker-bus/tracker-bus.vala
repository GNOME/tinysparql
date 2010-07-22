/*
 * Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

[DBus (name = "org.freedesktop.Tracker1.Resources")]
private interface Tracker.Bus.Resources : GLib.Object {
	public abstract string[,] sparql_query (string query) throws DBus.Error;
	[DBus (name = "SparqlQuery")]
	public abstract async string[,] sparql_query_async (string query) throws DBus.Error;

	public abstract void sparql_update (string query) throws DBus.Error;
	[DBus (name = "SparqlUpdate")]
	public abstract async void sparql_update_async (string query) throws DBus.Error;

	[DBus (name = "Load")]
	public abstract void import (string uri) throws DBus.Error;
	[DBus (name = "Load")]
	public abstract async void import_async (string uri) throws DBus.Error;
}

[DBus (name = "org.freedesktop.Tracker1.Statistics")]
private interface Tracker.Bus.Statistics : GLib.Object {
	public abstract string[,] Get () throws DBus.Error;
	public async abstract string[,] Get_async () throws DBus.Error;
}

// Imported DBus FD API until we have support with Vala
public extern Tracker.Sparql.Cursor tracker_bus_fd_query (DBus.Connection connection, string query) throws Tracker.Sparql.Error;

// Actual class definition
public class Tracker.Bus.Connection : Tracker.Sparql.Connection {
	static DBus.Connection connection;
	static Resources resources_object;
	static Statistics statistics_object;
	static bool initialized;
	static bool use_steroids;

	public Connection ()
	requires (!initialized) {
		initialized = true;
		
		try {
			if (strcmp (Config.HAVE_DBUS_FD_PASSING_IN_VALA, "1") == 0) {
				use_steroids = true;
			} else {
				use_steroids = false;
			}

			debug ("Using steroids = %s", use_steroids ? "yes" : "no");

			connection = DBus.Bus.get (DBus.BusType.SESSION);

			// FIXME: Ideally we would just get these as and when we need them
			resources_object = (Resources) connection.get_object (TRACKER_DBUS_SERVICE,
			                                                      TRACKER_DBUS_OBJECT_RESOURCES,
			                                                      TRACKER_DBUS_INTERFACE_RESOURCES);
			statistics_object = (Statistics) connection.get_object (TRACKER_DBUS_SERVICE,
			                                                        TRACKER_DBUS_OBJECT_STATISTICS,
			                                                        TRACKER_DBUS_INTERFACE_STATISTICS);
		} catch (DBus.Error e) {
			warning ("Could not connect to D-Bus service:'%s': %s", TRACKER_DBUS_INTERFACE_RESOURCES, e.message);
			initialized = false;
			return;
		}
		
		initialized = true;
	}
 
	~Connection () {
		initialized = false;
	}

	public override Sparql.Cursor? query (string sparql, Cancellable? cancellable) throws Sparql.Error {
		try {
			if (use_steroids) {
				return tracker_bus_fd_query (connection, sparql);
			} else {
				string[,] results = resources_object.sparql_query (sparql);
				return new Tracker.Bus.ArrayCursor ((owned) results, results.length[0], results.length[1]);
			}
		} catch (DBus.Error e) {
			throw new Sparql.Error.INTERNAL (e.message);
		}
	}

	public async override Sparql.Cursor? query_async (string sparql, Cancellable? cancellable = null) throws Sparql.Error {
		try {
			if (use_steroids) {
				return yield tracker_bus_fd_query_async (connection, sparql);
			} else {
				string[,] results = yield resources_object.sparql_query_async (sparql);
				return new Tracker.Bus.ArrayCursor ((owned) results, results.length[0], results.length[1]);
			}
		} catch (DBus.Error e) {
			throw new Sparql.Error.INTERNAL (e.message);
		}
	}

	public override void update (string sparql, Cancellable? cancellable = null) throws Sparql.Error {
		try {
			if (use_steroids) {
				tracker_bus_fd_sparql_update (connection, sparql);
			} else {
				resources_object.sparql_update (sparql);
			}
		} catch (DBus.Error e) {
			throw new Sparql.Error.INTERNAL (e.message);
		}
	}

	public async override void update_async (string sparql, int priority = GLib.Priority.DEFAULT, Cancellable? cancellable = null) throws Sparql.Error {
		try {
			if (use_steroids) {
				yield tracker_bus_fd_sparql_update_async (connection, sparql, cancellable);
			} else {
				yield resources_object.sparql_update_async (sparql);
			}
		} catch (DBus.Error e) {
			throw new Sparql.Error.INTERNAL (e.message);
		}
	}

	public override GLib.Variant? update_blank (string sparql, Cancellable? cancellable = null) throws Sparql.Error {
		GLib.Variant res = null;

		try {
			if (use_steroids) {
				res = tracker_bus_fd_sparql_update_blank (connection, sparql);
			} else {
				res = tracker_bus_array_sparql_update_blank (connection, sparql);
			}
		} catch (DBus.Error e) {
			throw new Sparql.Error.INTERNAL (e.message);
		}

		return res;
	}

	public async override GLib.Variant? update_blank_async (string sparql, int priority = GLib.Priority.DEFAULT, Cancellable? cancellable = null) throws Sparql.Error {
		GLib.Variant res = null;

		try {
			if (use_steroids) {
				res = yield tracker_bus_fd_sparql_update_blank_async (connection, sparql, cancellable);
			} else {
				res = yield tracker_bus_array_sparql_update_blank_async (connection, sparql, cancellable);
			}
		} catch (DBus.Error e) {
			throw new Sparql.Error.INTERNAL (e.message);
		}

		return res;
	}

	public override void import (File file, Cancellable? cancellable = null) throws Sparql.Error {
		try {
			resources_object.import (file.get_uri ());
		} catch (DBus.Error e) {
			throw new Sparql.Error.INTERNAL (e.message);
		}
	}
	public async override void import_async (File file, Cancellable? cancellable = null) throws Sparql.Error {
		try {
			yield resources_object.import_async (file.get_uri ());
		} catch (DBus.Error e) {
			throw new Sparql.Error.INTERNAL (e.message);
		}
	}

	public override Sparql.Cursor? statistics (Cancellable? cancellable = null) throws Sparql.Error {
		try {
			string[,] results = statistics_object.Get ();
			return new Tracker.Bus.ArrayCursor ((owned) results, results.length[0], results.length[1]);
		} catch (DBus.Error e) {
			throw new Sparql.Error.INTERNAL (e.message);
		}
	}

	public async override Sparql.Cursor? statistics_async (Cancellable? cancellable = null) throws Sparql.Error {
		try {
			string[,] results = yield statistics_object.Get_async ();
			return new Tracker.Bus.ArrayCursor ((owned) results, results.length[0], results.length[1]);
		} catch (DBus.Error e) {
			throw new Sparql.Error.INTERNAL (e.message);
		}
	}
}

public Tracker.Sparql.Connection module_init () {
	Tracker.Sparql.Connection plugin = new Tracker.Bus.Connection ();
	return plugin;
}
