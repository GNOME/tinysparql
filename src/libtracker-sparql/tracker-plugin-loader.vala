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

private class Tracker.Sparql.PluginLoader : Object {
	static bool initialized = false;
	static Tracker.Sparql.Connection direct = null;
	static Tracker.Sparql.Connection bus = null;

	private delegate Tracker.Sparql.Connection ModuleInitFunc ();

	public PluginLoader ()
	requires (!initialized) {
		if (!Module.supported ()) {
		    return;
		}

		if (!load_plugins ()) {
			initialized = false;
			return;
		}

		initialized = true;
	}

	// Plugin loading functions
	private bool load_plugins () {
		string env_path = Environment.get_variable ("TRACKER_SPARQL_MODULE_PATH");
		string path;
		
		if (env_path != null && env_path.length > 0) {
			path = env_path;
		} else {
			// FIXME: Get from config
			path = "/tmp";
		}

		File dir = File.new_for_path (path);
		string dir_path = dir.get_path ();
		
		debug ("Searching for modules in folder '%s' ..", dir_path);

		// First get direct library details
		string direct_path = Module.build_path (dir_path, "tracker-direct-0.9");
		direct = load_plugins_from_path (direct_path);

		// Second get bus library details
		string bus_path = Module.build_path (dir_path, "tracker-bus-0.9");
		bus = load_plugins_from_path (bus_path);

		debug ("Finished searching for modules in folder '%s'", dir_path);

		// FIXME: Finish this by checking for bus too!
		return direct != null;
	}

	private Tracker.Sparql.Connection? load_plugins_from_path (string path) {
		File file = File.new_for_path (path);
		assert (file != null);

		FileInfo info = null;

		try {
			string attributes = FILE_ATTRIBUTE_STANDARD_NAME + "," +
			                    FILE_ATTRIBUTE_STANDARD_TYPE + "," +
			                    FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE;

			info = file.query_info (attributes,
			                        FileQueryInfoFlags.NONE,
			                        null);
		} catch (Error e) {
			warning ("Could not get GFileInfo for '%s'", path);
			return null;
		}

		string content_type = info.get_content_type ();
		string mime = g_content_type_get_mime_type (content_type);
		string expected_mime = "application/x-sharedlib";
		
		if (mime != expected_mime) {
			warning ("Could not load plugin, mime type was '%s', expected:'%s'", 
			         mime,
			         expected_mime);
			return null;
		}

		Module module = Module.open (path, ModuleFlags.BIND_LOCAL);
		if (module == null) {
			warning ("Failed to load module from path '%s': %s",
			         path,
			         Module.error ());
			return null;
		}

		void *function;

		if (!module.symbol ("module_init", out function)) {
			warning ("Failed to find entry point function '%s' in '%s': %s",
			         "module_init",
			         path,
			         Module.error ());

			return null;
		}

		ModuleInitFunc module_init = (ModuleInitFunc) function;
		assert (module_init != null);

		// We don't want our modules to ever unload
		module.make_resident ();

		// Call module init function
		Tracker.Sparql.Connection c = module_init ();

		debug ("Loaded module source: '%s'", module.name ());

		return c;
	}
}

