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
	public abstract string[,] SparqlQuery (string query) throws DBus.Error;
}

// Imported DBus FD API until we have support with Vala
public extern Tracker.Sparql.Cursor tracker_bus_query (DBus.Connection connection, string query) throws GLib.Error;

// Actual class definition
public class Tracker.Bus.Connection : Tracker.Sparql.Connection {
	static DBus.Connection connection;
	static bool initialized;

	public Connection ()
	requires (!initialized) {
		initialized = true;
		
		try {
			connection = DBus.Bus.get (DBus.BusType.SESSION);

			// FIXME: Test for steroids and resources interfaces?			
//			resources = (Resources) c.get_object (TRACKER_DBUS_SERVICE,
//			                                      TRACKER_DBUS_OBJECT_RESOURCES,
//			                                      TRACKER_DBUS_INTERFACE_RESOURCES);
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
		return tracker_bus_query (connection, sparql);
	}

	public async override Sparql.Cursor? query_async (string sparql, Cancellable? cancellable = null) throws GLib.Error {
		// FIXME: Implement
		return null;
	}
}

public Tracker.Sparql.Connection module_init () {
	Tracker.Sparql.Connection plugin = new Tracker.Bus.Connection ();
	return plugin;
}
