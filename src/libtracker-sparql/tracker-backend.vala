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

[DBus (name = "org.freedesktop.Tracker1.Status")]
interface Tracker.Backend.Status : DBusProxy {
	public abstract void wait () throws DBusError;
	[DBus (name = "Wait")]
	public abstract async void wait_async () throws DBusError;
}

class Tracker.Sparql.Backend : Connection {
	bool direct_only;

	Tracker.Sparql.Connection direct = null;
	Tracker.Sparql.Connection bus = null;
	enum Backend {
		AUTO,
		DIRECT,
		BUS
	}

	[CCode (has_target = false)]
	private delegate Tracker.Sparql.Connection? ModuleInitFunc () throws GLib.Error;

	public Backend (bool direct_only = false) throws Sparql.Error
	requires (Module.supported ()) {
		this.direct_only = direct_only;
	}

	public override void init () throws Sparql.Error, IOError, DBusError {
		Tracker.Backend.Status status = Bus.get_proxy_sync (BusType.SESSION,
		                                                    TRACKER_DBUS_SERVICE,
		                                                    TRACKER_DBUS_OBJECT_STATUS,
		                                                    DBusProxyFlags.DO_NOT_LOAD_PROPERTIES | DBusProxyFlags.DO_NOT_CONNECT_SIGNALS);
		status.set_default_timeout (int.MAX);

		// Makes sure the sevice is available
		debug ("Waiting for service to become available synchronously...");
		status.wait ();
		debug ("Service is ready");

		try {
			debug ("Constructing connection, direct_only=%s", direct_only ? "true" : "false");
			if (load_plugins (direct_only)) {
				debug ("Waiting for backend to become available synchronously...");
				if (direct != null) {
					direct.init ();
				} else {
					bus.init ();
				}
				debug ("Backend is ready");
			}
		} catch (GLib.Error e) {
			throw new Sparql.Error.INTERNAL (e.message);
		}
	}

	public async override void init_async () throws Sparql.Error, IOError, DBusError {
		Tracker.Backend.Status status = Bus.get_proxy_sync (BusType.SESSION,
		                                                    TRACKER_DBUS_SERVICE,
		                                                    TRACKER_DBUS_OBJECT_STATUS,
		                                                    DBusProxyFlags.DO_NOT_LOAD_PROPERTIES | DBusProxyFlags.DO_NOT_CONNECT_SIGNALS);
		status.set_default_timeout (int.MAX);

		// Makes sure the sevice is available
		debug ("Waiting for service to become available asynchronously...");
		yield status.wait_async ();
		debug ("Service is ready");

		try {
			debug ("Constructing connection, direct_only=%s", direct_only ? "true" : "false");
			if (load_plugins (direct_only)) {
				debug ("Waiting for backend to become available asynchronously...");
				if (direct != null) {
					yield direct.init_async ();
				} else {
					yield bus.init_async ();
				}
				debug ("Backend is ready");
			}
		} catch (GLib.Error e) {
			throw new Sparql.Error.INTERNAL (e.message);
		}
	}

	public override Cursor query (string sparql, Cancellable? cancellable = null) throws Sparql.Error, IOError, DBusError
	requires (bus != null || direct != null) {
		debug ("%s(): '%s'", Log.METHOD, sparql);
		if (direct != null) {
			return direct.query (sparql, cancellable);
		} else {
			return bus.query (sparql, cancellable);
		}
	}

	public async override Cursor query_async (string sparql, Cancellable? cancellable = null) throws Sparql.Error, IOError, DBusError
	requires (bus != null || direct != null) {
		debug ("%s(): '%s'", Log.METHOD, sparql);
		if (direct != null) {
			return yield direct.query_async (sparql, cancellable);
		} else {
			return yield bus.query_async (sparql, cancellable);
		}
	}

	public override void update (string sparql, int priority = GLib.Priority.DEFAULT, Cancellable? cancellable = null) throws Sparql.Error, IOError, DBusError
	requires (bus != null) {
		debug ("%s(priority:%d): '%s'", Log.METHOD, priority, sparql);
		bus.update (sparql, priority, cancellable);
	}

	public override GLib.Variant? update_blank (string sparql, int priority = GLib.Priority.DEFAULT, Cancellable? cancellable = null) throws Sparql.Error, IOError, DBusError
	requires (bus != null) {
		debug ("%s(priority:%d): '%s'", Log.METHOD, priority, sparql);
		return bus.update_blank (sparql, priority, cancellable);
	}

	public async override void update_async (string sparql, int priority = GLib.Priority.DEFAULT, Cancellable? cancellable = null) throws Sparql.Error, IOError, DBusError
	requires (bus != null) {
		debug ("%s(priority:%d): '%s'", Log.METHOD, priority, sparql);
		yield bus.update_async (sparql, priority, cancellable);
	}

	public async override GenericArray<Error?>? update_array_async (string[] sparql, int priority = GLib.Priority.DEFAULT, Cancellable? cancellable = null) throws Sparql.Error, IOError, DBusError
	requires (bus != null) {
		return yield bus.update_array_async (sparql, priority, cancellable);
	}

	public async override GLib.Variant? update_blank_async (string sparql, int priority = GLib.Priority.DEFAULT, Cancellable? cancellable = null) throws Sparql.Error, IOError, DBusError
	requires (bus != null) {
		debug ("%s(priority:%d): '%s'", Log.METHOD, priority, sparql);
		return yield bus.update_blank_async (sparql, priority, cancellable);
	}

	public override void load (File file, Cancellable? cancellable = null) throws Sparql.Error, IOError, DBusError
	requires (bus != null) {
		var uri = file.get_uri ();
		debug ("%s(): '%s'", Log.METHOD, uri);
		bus.load (file, cancellable);
	}

	public async override void load_async (File file, Cancellable? cancellable = null) throws Sparql.Error, IOError, DBusError
	requires (bus != null) {
		var uri = file.get_uri ();
		debug ("%s(): '%s'", Log.METHOD, uri);
		yield bus.load_async (file, cancellable);
	}

	public override Cursor? statistics (Cancellable? cancellable = null) throws Sparql.Error, IOError, DBusError
	requires (bus != null) {
		debug ("%s()", Log.METHOD);
		return bus.statistics (cancellable);
	}

	public async override Cursor? statistics_async (Cancellable? cancellable = null) throws Sparql.Error, IOError, DBusError
	requires (bus != null) {
		debug ("%s()", Log.METHOD);
		return yield bus.statistics_async (cancellable);
	}

	// Plugin loading functions
	private bool load_plugins (bool direct_only) throws GLib.Error {
		string env_path = Environment.get_variable ("TRACKER_SPARQL_MODULE_PATH");
		string path;
		
		if (env_path != null && env_path.length > 0) {
			path = env_path;
		} else {
			path = Config.SPARQL_MODULES_DIR;
		}

		File dir = File.new_for_path (path);
		string dir_path = dir.get_path ();

		string env_backend = Environment.get_variable ("TRACKER_SPARQL_BACKEND");
		Backend backend = Backend.AUTO;

		if (env_backend != null) {
			if (env_backend.ascii_casecmp ("direct") == 0) {
				backend = Backend.DIRECT;
				debug ("Using backend = 'DIRECT'");
			} else if (env_backend.ascii_casecmp ("bus") == 0) {
				backend = Backend.BUS;
				debug ("Using backend = 'BUS'");
			} else {
				warning ("Environment variable TRACKER_SPARQL_BACKEND set to unknown value '%s'", env_backend);
			}
		}

		if (backend == Backend.AUTO) {
			if (direct_only && backend == Backend.AUTO) {
				backend = Backend.DIRECT;
				debug ("Using backend = 'DIRECT'");
			} else {
				debug ("Using backend = 'AUTO'");
			}
		}

		if (direct_only && backend == Backend.BUS) {
			debug ("Backend set in environment contradicts requested connection type, using environment to override");
		}

		debug ("Searching for modules in folder '%s' ..", dir_path);

		Tracker.Sparql.Connection connection;

		switch (backend) {
		case backend.AUTO:
			string direct_path = Module.build_path (dir_path, "tracker-direct");
			direct = load_plugins_from_path (direct_path, false /* required */);

			string bus_path = Module.build_path (dir_path, "tracker-bus");
			bus = load_plugins_from_path (bus_path, true /* required */);

			connection = bus;
			break;

		case backend.DIRECT:
			string direct_path = Module.build_path (dir_path, "tracker-direct");
			connection = direct = load_plugins_from_path (direct_path, true /* required */);
			break;

		case backend.BUS:
			string bus_path = Module.build_path (dir_path, "tracker-bus");
			connection = bus = load_plugins_from_path (bus_path, true /* required */);
			break;

		default:
			assert_not_reached ();
		}

		debug ("Finished searching for modules");

		return connection != null;
	}

	private Tracker.Sparql.Connection? load_plugins_from_path (string path, bool required) throws GLib.Error {
		try {
			// lazy resolving reduces initialization time
			Module module = Module.open (path, ModuleFlags.BIND_LOCAL | ModuleFlags.BIND_LAZY);
			if (module == null) {
				throw new IOError.FAILED ("Failed to load module from path '%s': %s",
				                          path,
				                          Module.error ());
			}

			void *function;

			if (!module.symbol ("module_init", out function)) {
				throw new IOError.FAILED ("Failed to find entry point function '%s' in '%s': %s",
				                          "module_init",
				                          path,
				                          Module.error ());
			}

			ModuleInitFunc module_init = (ModuleInitFunc) function;
			
			assert (module_init != null);

			// We don't want our modules to ever unload
			module.make_resident ();

			// Call module init function
			Tracker.Sparql.Connection c = module_init ();

			debug ("Loaded module source: '%s'", module.name ());

			return c;
		} catch (GLib.Error e) {
			if (required) {
				// plugin required => error is fatal
				throw e;
			} else {
				return null;
			}
		}
	}
}

