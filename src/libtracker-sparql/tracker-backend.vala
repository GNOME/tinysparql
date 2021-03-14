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

/* All apps using libtracker-sparql will call one of these constructors, so
 * we take the opportunity to call tracker_get_debug_flags(). This has the
 * effect of printing the 'help' message if TRACKER_DEBUG=help is set.
 */

public static Tracker.Sparql.Connection tracker_sparql_connection_remote_new (string url_base) {
	Tracker.get_debug_flags ();
	return new Tracker.Remote.Connection (url_base);
}

public static Tracker.Sparql.Connection tracker_sparql_connection_bus_new (string service, string? object_path, DBusConnection? conn) throws Tracker.Sparql.Error, IOError, DBusError, GLib.Error {
	Tracker.get_debug_flags ();

	var context = new GLib.MainContext ();
	var loop = new GLib.MainLoop(context);
	GLib.Error? error = null;
	Tracker.Sparql.Connection? sparql_conn = null;

	context.push_thread_default ();

	tracker_sparql_connection_bus_new_async.begin(service, object_path, conn, null, (o, res) => {
		try {
			sparql_conn = tracker_sparql_connection_bus_new_async.end(res);
		} catch (Error e) {
			error = e;
		}
		loop.quit();
	});
	loop.run ();

	context.pop_thread_default ();

	if (error != null)
		throw error;

	return sparql_conn;
}

public static async Tracker.Sparql.Connection tracker_sparql_connection_bus_new_async (string service, string? object_path, DBusConnection? conn, Cancellable? cancellable) throws Tracker.Sparql.Error, IOError, DBusError, GLib.Error {
	GLib.DBusConnection dbus_conn;
	string path;

	Tracker.get_debug_flags ();

	if (conn != null)
		dbus_conn = conn;
	else
		dbus_conn = yield GLib.Bus.get (GLib.BusType.SESSION, cancellable);

	if (object_path != null)
		path = object_path;
	else
		path = "/org/freedesktop/Tracker3/Endpoint";

	return yield new Tracker.Bus.Connection (service, path, dbus_conn, cancellable);
}

public static Tracker.Sparql.Connection tracker_sparql_connection_new (Tracker.Sparql.ConnectionFlags flags, File? store, File? ontology, Cancellable? cancellable = null) throws GLib.Error, Tracker.Sparql.Error, IOError {
	Tracker.get_debug_flags ();
	var conn = new Tracker.Direct.Connection (flags, store, ontology);
	conn.init (cancellable);
	return conn;
}

public static async Tracker.Sparql.Connection tracker_sparql_connection_new_async (Tracker.Sparql.ConnectionFlags flags, File? store, File ontology, Cancellable? cancellable = null) throws GLib.Error, Tracker.Sparql.Error, IOError {
	Tracker.get_debug_flags ();
	var conn = new Tracker.Direct.Connection (flags, store, ontology);
	yield conn.init_async (Priority.DEFAULT, cancellable);
	return conn;
}
