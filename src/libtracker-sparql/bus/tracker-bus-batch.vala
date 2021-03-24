/*
 * Copyright (C) 2021, Red Hat Inc.
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
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

public class Tracker.Bus.Batch : Tracker.Batch {
	private DBusConnection bus;
	private string dbus_name;
	private string object_path;
	private string[] updates;

	public Batch (DBusConnection bus, string dbus_name, string object_path) {
		Object ();
		this.bus = bus;
		this.dbus_name = dbus_name;
		this.object_path = object_path;
	}

	public override void add_sparql (string sparql) {
		updates += sparql;
	}

	public override void add_resource (string? graph, Resource resource) {
		var namespaces = this.connection.get_namespace_manager ();
		var sparql = resource.print_sparql_update (namespaces, graph);
		updates += sparql;
	}

	public override bool execute (GLib.Cancellable? cancellable) throws Sparql.Error, GLib.Error, GLib.IOError, GLib.DBusError {
		// use separate main context for sync operation
		var context = new MainContext ();
		var loop = new MainLoop (context, false);
		context.push_thread_default ();
		AsyncResult async_res = null;
		execute_async.begin (cancellable, (o, res) => {
			async_res = res;
			loop.quit ();
		});
		loop.run ();
		context.pop_thread_default ();
		return execute_async.end (async_res);
	}

	public async override bool execute_async (GLib.Cancellable? cancellable) throws Sparql.Error, GLib.Error, GLib.IOError, GLib.DBusError {
		return yield Tracker.Bus.Connection.perform_update_array (bus, dbus_name, object_path, updates, cancellable);
	}
}
