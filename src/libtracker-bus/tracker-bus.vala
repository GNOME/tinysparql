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
	public abstract void sparql_update (string query) throws DBus.Error;
	[DBus (name = "SparqlUpdate")]
	public abstract async void sparql_update_async (string query) throws DBus.Error;
}

// Imported DBus FD API until we have support with Vala
public extern Tracker.Sparql.Cursor tracker_bus_fd_query (DBus.Connection connection, string query) throws GLib.Error;

// Actual class definition
public class Tracker.Bus.Connection : Tracker.Sparql.Connection {
	static DBus.Connection connection;
	static Resources resources;
	static bool initialized;
	static bool use_steroids;
		
	public Connection ()
	requires (!initialized) {
		initialized = true;
		
		try {
			connection = DBus.Bus.get (DBus.BusType.SESSION);

			// FIXME: Test for steroids and resources interfaces?			
			use_steroids = false;

			resources = (Resources) connection.get_object (TRACKER_DBUS_SERVICE,
			                                               TRACKER_DBUS_OBJECT_RESOURCES,
			                                               TRACKER_DBUS_INTERFACE_RESOURCES);
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

	public override Sparql.Cursor? query (string sparql, Cancellable? cancellable) throws GLib.Error {
		// FIXME: Decide between FD passing and DBus-Glib, we need to do this
		// here because otherwise we need to do the whole set up of their
		// DBus interface again in the .c file, which is pointless when
		// one call can do it here just fine.
		//
		// Really we need #ifdef here, unsupported in vala AFAIK

		
		if (use_steroids) {
			return tracker_bus_fd_query (connection, sparql);
		} else {
			string[,] results = resources.sparql_query (sparql);
			return new Tracker.Bus.ArrayCursor ((owned) results, results.length[0], results.length[1]);
		}
	}

	public async override Sparql.Cursor? query_async (string sparql, Cancellable? cancellable = null) throws GLib.Error {
		// FIXME: Implement
		return null;
	}

	public override void update (string sparql, Cancellable? cancellable = null) throws GLib.Error {
		resources.sparql_update (sparql);
	}

	public async override void update_async (string sparql, int priority = GLib.Priority.DEFAULT, Cancellable? cancellable = null) throws GLib.Error {
		yield resources.sparql_update_async (sparql);
	}
}

public Tracker.Sparql.Connection module_init () {
	Tracker.Sparql.Connection plugin = new Tracker.Bus.Connection ();
	return plugin;
}
