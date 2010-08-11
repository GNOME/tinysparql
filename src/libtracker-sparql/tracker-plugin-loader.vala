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

class Tracker.Sparql.PluginLoader : Connection {
	static bool initialized = false;
	static Tracker.Sparql.Connection direct = null;
	static Tracker.Sparql.Connection bus = null;

	private delegate Tracker.Sparql.Connection ModuleInitFunc ();

	public PluginLoader (bool direct_only = false) throws Sparql.Error
	requires (!initialized) {
		if (!Module.supported ()) {
		    return;
		}

		try {
			load_plugins (direct_only);
		} catch (GLib.Error e) {
			throw new Sparql.Error.INTERNAL (e.message);
		}

		initialized = true;
	}

	public override Cursor query (string sparql, Cancellable? cancellable = null) throws Sparql.Error, IOError {
		if (direct != null) {
			return direct.query (sparql, cancellable);
		} else {
			return bus.query (sparql, cancellable);
		}
	}

	public async override Cursor query_async (string sparql, Cancellable? cancellable = null) throws Sparql.Error, IOError {
		if (direct != null) {
			return yield direct.query_async (sparql, cancellable);
		} else {
			return yield bus.query_async (sparql, cancellable);
		}
	}

	public override void update (string sparql, int priority = GLib.Priority.DEFAULT, Cancellable? cancellable = null) throws Sparql.Error, IOError {
		bus.update (sparql, priority, cancellable);
	}

	public override GLib.Variant? update_blank (string sparql, int priority = GLib.Priority.DEFAULT, Cancellable? cancellable = null) throws Sparql.Error, IOError {
		return bus.update_blank (sparql, priority, cancellable);
	}

	public async override void update_async (string sparql, int priority = GLib.Priority.DEFAULT, Cancellable? cancellable = null) throws Sparql.Error, IOError {
		yield bus.update_async (sparql, priority, cancellable);
	}

	public async override GLib.Variant? update_blank_async (string sparql, int priority = GLib.Priority.DEFAULT, Cancellable? cancellable = null) throws Sparql.Error, IOError {
		return yield bus.update_blank_async (sparql, priority, cancellable);
	}

	public override void load (File file, Cancellable? cancellable = null) throws Sparql.Error, IOError {
		bus.load (file, cancellable);
	}

	public async override void load_async (File file, Cancellable? cancellable = null) throws Sparql.Error, IOError {
		yield bus.load_async (file, cancellable);
	}

	public override Cursor? statistics (Cancellable? cancellable = null) throws Sparql.Error, IOError {
		return bus.statistics (cancellable);
	}

	public async override Cursor? statistics_async (Cancellable? cancellable = null) throws Sparql.Error, IOError {
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
		
		debug ("Searching for modules in folder '%s' ..", dir_path);

		// First get direct library details
		string direct_path = Module.build_path (dir_path, "tracker-direct");
		direct = load_plugins_from_path (direct_path, direct_only /* required */);

		if (!direct_only) {
			// Second get bus library details
			string bus_path = Module.build_path (dir_path, "tracker-bus");
			bus = load_plugins_from_path (bus_path, true /* required */);
		}

		debug ("Finished searching for modules in folder '%s'", dir_path);

		if (direct_only) {
			return direct != null;
		} else {
			return bus != null;
		}
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

			Module module = Module.open (path, ModuleFlags.BIND_LOCAL);
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

