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

[DBus (name = "org.freedesktop.Tracker1.Resources", timeout = 2147483647 /* INT_MAX */)]
private interface Tracker.Bus.Resources : GLib.Object {
	public abstract void load (string uri) throws Sparql.Error, DBus.Error;
	[DBus (name = "Load")]
	public abstract async void load_async (string uri) throws Sparql.Error, DBus.Error;
}

[DBus (name = "org.freedesktop.Tracker1.Statistics")]
private interface Tracker.Bus.Statistics : GLib.Object {
	public abstract string[,] Get () throws DBus.Error;
	public async abstract string[,] Get_async () throws DBus.Error;
}

// Imported DBus FD API until we have support with Vala
public extern Tracker.Sparql.Cursor tracker_bus_fd_query (DBus.Connection connection, string query, Cancellable? cancellable) throws Tracker.Sparql.Error, DBus.Error, GLib.IOError;

// Actual class definition
public class Tracker.Bus.Connection : Tracker.Sparql.Connection {
	static DBus.Connection connection;
	static Resources resources_object;
	static Statistics statistics_object;
	static bool initialized;

	public Connection ()
	requires (!initialized) {
		initialized = true;
		
		try {
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

	public override Sparql.Cursor query (string sparql, Cancellable? cancellable) throws Sparql.Error, IOError {
		try {
			return tracker_bus_fd_query (connection, sparql, cancellable);
		} catch (DBus.Error e) {
			throw new Sparql.Error.INTERNAL (e.message);
		}
	}

	public async override Sparql.Cursor query_async (string sparql, Cancellable? cancellable = null) throws Sparql.Error, IOError {
		try {
			return yield tracker_bus_fd_query_async (connection, sparql, cancellable);
		} catch (DBus.Error e) {
			throw new Sparql.Error.INTERNAL (e.message);
		}
	}

	public override void update (string sparql, int priority = GLib.Priority.DEFAULT, Cancellable? cancellable = null) throws Sparql.Error, IOError {
		try {
			if (priority >= GLib.Priority.DEFAULT) {
				tracker_bus_fd_sparql_update (connection, sparql);
			} else {
				tracker_bus_fd_sparql_batch_update (connection, sparql);
			}
		} catch (DBus.Error e) {
			throw new Sparql.Error.INTERNAL (e.message);
		}
	}

	public async override void update_async (string sparql, int priority = GLib.Priority.DEFAULT, Cancellable? cancellable = null) throws Sparql.Error, IOError {
		try {
			if (priority >= GLib.Priority.DEFAULT) {
				yield tracker_bus_fd_sparql_update_async (connection, sparql, cancellable);
			} else {
				yield tracker_bus_fd_sparql_batch_update_async (connection, sparql, cancellable);
			}
		} catch (DBus.Error e) {
			throw new Sparql.Error.INTERNAL (e.message);
		}
	}

	public async override GLib.PtrArray? update_array_async (string[] sparql, int priority = GLib.Priority.DEFAULT, Cancellable? cancellable = null) throws Sparql.Error, IOError {
		try {
			// helper variable necessary to work around bug in vala < 0.11
			PtrArray result;
			if (priority >= GLib.Priority.DEFAULT) {
				result = yield tracker_bus_fd_sparql_update_array_async (connection, sparql, cancellable);
			} else {
				result = yield tracker_bus_fd_sparql_batch_update_array_async (connection, sparql, cancellable);
			}
			return result;
		} catch (DBus.Error e) {
			throw new Sparql.Error.INTERNAL (e.message);
		}
	}

	public override GLib.Variant? update_blank (string sparql, int priority = GLib.Priority.DEFAULT, Cancellable? cancellable = null) throws Sparql.Error, IOError {
		try {
			GLib.Variant res = null;
			res = tracker_bus_fd_sparql_update_blank (connection, sparql);
			return res;
		} catch (DBus.Error e) {
			throw new Sparql.Error.INTERNAL (e.message);
		}
	}

	public async override GLib.Variant? update_blank_async (string sparql, int priority = GLib.Priority.DEFAULT, Cancellable? cancellable = null) throws Sparql.Error, IOError {
		try {
			GLib.Variant res = null;
			res = yield tracker_bus_fd_sparql_update_blank_async (connection, sparql, cancellable);
			return res;
		} catch (DBus.Error e) {
			throw new Sparql.Error.INTERNAL (e.message);
		}
	}

	public override void load (File file, Cancellable? cancellable = null) throws Sparql.Error, IOError {
		try {
			resources_object.load (file.get_uri ());

			if (cancellable != null && cancellable.is_cancelled ()) {
				throw new IOError.CANCELLED ("Operation was cancelled");
			}
		} catch (DBus.Error e) {
			throw new Sparql.Error.INTERNAL (e.message);
		}
	}
	public async override void load_async (File file, Cancellable? cancellable = null) throws Sparql.Error, IOError {
		try {
			yield resources_object.load_async (file.get_uri ());

			if (cancellable != null && cancellable.is_cancelled ()) {
				throw new IOError.CANCELLED ("Operation was cancelled");
			}
		} catch (DBus.Error e) {
			throw new Sparql.Error.INTERNAL (e.message);
		}
	}

	public override Sparql.Cursor? statistics (Cancellable? cancellable = null) throws Sparql.Error, IOError {
		try {
			string[,] results = statistics_object.Get ();
			Sparql.ValueType[] types = new Sparql.ValueType[2];
			string[] var_names = new string[2];

			if (cancellable != null && cancellable.is_cancelled ()) {
				throw new IOError.CANCELLED ("Operation was cancelled");
			}

			var_names[0] = "class";
			var_names[1] = "count";
			types[0] = Sparql.ValueType.STRING;
			types[1] = Sparql.ValueType.INTEGER;

			return new Tracker.Bus.ArrayCursor ((owned) results,
			                                    results.length[0],
			                                    results.length[1],
			                                    var_names,
			                                    types);
		} catch (DBus.Error e) {
			throw new Sparql.Error.INTERNAL (e.message);
		}
	}

	public async override Sparql.Cursor? statistics_async (Cancellable? cancellable = null) throws Sparql.Error, IOError {
		try {
			string[,] results = yield statistics_object.Get_async ();
			Sparql.ValueType[] types = new Sparql.ValueType[2];
			string[] var_names = new string[2];

			if (cancellable != null && cancellable.is_cancelled ()) {
				throw new IOError.CANCELLED ("Operation was cancelled");
			}

			var_names[0] = "class";
			var_names[1] = "count";
			types[0] = Sparql.ValueType.STRING;
			types[1] = Sparql.ValueType.INTEGER;

			return new Tracker.Bus.ArrayCursor ((owned) results,
			                                    results.length[0],
			                                    results.length[1],
			                                    var_names,
			                                    types);
		} catch (DBus.Error e) {
			throw new Sparql.Error.INTERNAL (e.message);
		}
	}
}

public Tracker.Sparql.Connection module_init () {
	Tracker.Sparql.Connection plugin = new Tracker.Bus.Connection ();
	return plugin;
}
