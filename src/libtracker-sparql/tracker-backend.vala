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

[DBus (name = "org.freedesktop.Tracker1.Status", timeout = 2147483647 /* INT_MAX */)]
interface Tracker.Backend.Status : GLib.Object {
	public abstract void wait () throws DBus.Error;
	[DBus (name = "Wait")]
	public abstract async void wait_async () throws DBus.Error;
}

class Tracker.Sparql.Backend : Connection {
	static bool is_constructed = false;
	static bool is_initialized = false;
	static Tracker.Sparql.Connection direct = null;
	static Tracker.Sparql.Connection bus = null;
	static enum Backend {
		AUTO,
		DIRECT,
		BUS
	}

	private delegate Tracker.Sparql.Connection ModuleInitFunc ();

	public Backend (bool direct_only = false) throws Sparql.Error
	requires (Module.supported ()) {
		if (is_constructed) {
			// Don't error or require this, > 1 new Tracker.Sparql.Connection
			// objects can be created and if they are, then we don't need to do
			// anything on subsequent init() calls. We just return the already
			// created direct or bus objects
			return;
		}

		try {
			debug ("Constructing connection, direct_only=%s", direct_only ? "true" : "false");
			load_plugins (direct_only);
		} catch (GLib.Error e) {
			throw new Sparql.Error.INTERNAL (e.message);
		}

		is_constructed = true;
	}

	public override void init () throws Sparql.Error
	requires (is_constructed) {
		try {
			var connection = DBus.Bus.get (DBus.BusType.SESSION);
			var status = (Tracker.Backend.Status) connection.get_object (TRACKER_DBUS_SERVICE,
			                                                             TRACKER_DBUS_OBJECT_STATUS,
			                                                             TRACKER_DBUS_INTERFACE_STATUS);

			// Makes sure the sevice is available
			debug ("Waiting for service to become available synchronously...");
			status.wait ();
			debug ("Service is ready");
		} catch (DBus.Error e) {
			warning ("Could not connect to D-Bus service:'%s': %s", TRACKER_DBUS_INTERFACE_RESOURCES, e.message);
			throw new Sparql.Error.INTERNAL (e.message);
		}

		is_initialized = true;
	}

	public async override void init_async () throws Sparql.Error
	requires (is_constructed) {
		try {
			var connection = DBus.Bus.get (DBus.BusType.SESSION);
			var status = (Tracker.Backend.Status) connection.get_object (TRACKER_DBUS_SERVICE,
			                                                             TRACKER_DBUS_OBJECT_STATUS,
			                                                             TRACKER_DBUS_INTERFACE_STATUS);

			// Makes sure the sevice is available
			debug ("Waiting for service to become available asynchronously...");
			yield status.wait_async ();
			debug ("Service is ready");
		} catch (DBus.Error e) {
			warning ("Could not connect to D-Bus service:'%s': %s", TRACKER_DBUS_INTERFACE_RESOURCES, e.message);
			throw new Sparql.Error.INTERNAL (e.message);
		}

		is_initialized = true;
	}

	public override Cursor query (string sparql, Cancellable? cancellable = null) throws Sparql.Error, IOError
	requires (bus != null || direct != null) {
		debug ("%s(): '%s'", Log.METHOD, sparql);
		if (direct != null) {
			return direct.query (sparql, cancellable);
		} else {
			return bus.query (sparql, cancellable);
		}
	}

	public async override Cursor query_async (string sparql, Cancellable? cancellable = null) throws Sparql.Error, IOError
	requires (bus != null || direct != null) {
		debug ("%s(): '%s'", Log.METHOD, sparql);
		if (direct != null) {
			return yield direct.query_async (sparql, cancellable);
		} else {
			return yield bus.query_async (sparql, cancellable);
		}
	}

	public override void update (string sparql, int priority = GLib.Priority.DEFAULT, Cancellable? cancellable = null) throws Sparql.Error, IOError
	requires (bus != null) {
		debug ("%s(priority:%d): '%s'", Log.METHOD, priority, sparql);
		bus.update (sparql, priority, cancellable);
	}

	public override GLib.Variant? update_blank (string sparql, int priority = GLib.Priority.DEFAULT, Cancellable? cancellable = null) throws Sparql.Error, IOError
	requires (bus != null) {
		debug ("%s(priority:%d): '%s'", Log.METHOD, priority, sparql);
		return bus.update_blank (sparql, priority, cancellable);
	}

	public async override void update_async (string sparql, int priority = GLib.Priority.DEFAULT, Cancellable? cancellable = null) throws Sparql.Error, IOError
	requires (bus != null) {
		debug ("%s(priority:%d): '%s'", Log.METHOD, priority, sparql);
		yield bus.update_async (sparql, priority, cancellable);
	}

	public async override GLib.PtrArray? update_array_async (string[] sparql, int priority = GLib.Priority.DEFAULT, Cancellable? cancellable = null) throws Sparql.Error, IOError
	requires (bus != null) {
		return yield bus.update_array_async (sparql, priority, cancellable);
	}

	public async override GLib.Variant? update_blank_async (string sparql, int priority = GLib.Priority.DEFAULT, Cancellable? cancellable = null) throws Sparql.Error, IOError
	requires (bus != null) {
		debug ("%s(priority:%d): '%s'", Log.METHOD, priority, sparql);
		return yield bus.update_blank_async (sparql, priority, cancellable);
	}

	public override void load (File file, Cancellable? cancellable = null) throws Sparql.Error, IOError
	requires (bus != null) {
		var uri = file.get_uri ();
		debug ("%s(): '%s'", Log.METHOD, uri);
		bus.load (file, cancellable);
	}

	public async override void load_async (File file, Cancellable? cancellable = null) throws Sparql.Error, IOError
	requires (bus != null) {
		var uri = file.get_uri ();
		debug ("%s(): '%s'", Log.METHOD, uri);
		yield bus.load_async (file, cancellable);
	}

	public override Cursor? statistics (Cancellable? cancellable = null) throws Sparql.Error, IOError
	requires (bus != null) {
		debug ("%s()", Log.METHOD);
		return bus.statistics (cancellable);
	}

	public async override Cursor? statistics_async (Cancellable? cancellable = null) throws Sparql.Error, IOError
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
			File file = File.new_for_path (path);
			assert (file != null);

			FileInfo info = null;

			string attributes = FILE_ATTRIBUTE_STANDARD_NAME + "," +
				            FILE_ATTRIBUTE_STANDARD_TYPE + "," +
				            FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE;

			info = file.query_info (attributes,
				                FileQueryInfoFlags.NONE,
				                null);

			string content_type = info.get_content_type ();
			string mime = g_content_type_get_mime_type (content_type);
			string expected_mime = "application/x-sharedlib";
		
			if (mime != expected_mime) {
				throw new IOError.FAILED ("Could not load plugin, mime type was '%s', expected:'%s'",
				                          mime,
				                          expected_mime);
			}

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

