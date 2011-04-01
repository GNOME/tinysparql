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
}

class Tracker.Sparql.Backend : Connection {
	Tracker.Sparql.Connection direct = null;
	Tracker.Sparql.Connection bus = null;
	enum Backend {
		AUTO,
		DIRECT,
		BUS
	}

	public override void init () throws Sparql.Error, IOError, DBusError, SpawnError {
		Tracker.Backend.Status status = GLib.Bus.get_proxy_sync (BusType.SESSION,
		                                                         TRACKER_DBUS_SERVICE,
		                                                         TRACKER_DBUS_OBJECT_STATUS,
		                                                         DBusProxyFlags.DO_NOT_LOAD_PROPERTIES | DBusProxyFlags.DO_NOT_CONNECT_SIGNALS);
		status.set_default_timeout (int.MAX);

		// Makes sure the sevice is available
		debug ("Waiting for service to become available...");
		status.wait ();
		debug ("Service is ready");

		try {
			debug ("Constructing connection, direct_only=%s", direct_only ? "true" : "false");
			if (load_plugins (direct_only)) {
				debug ("Waiting for backend to become available...");
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

		Tracker.Sparql.Connection connection;

		switch (backend) {
		case backend.AUTO:
			try {
				direct = new Tracker.Direct.Connection ();
			} catch (Error e) {
				debug ("Unable to initialize direct backend");
			}

			bus = new Tracker.Bus.Connection ();

			connection = bus;
			break;

		case backend.DIRECT:
			connection = direct = new Tracker.Direct.Connection ();
			break;

		case backend.BUS:
			connection = bus = new Tracker.Bus.Connection ();
			break;

		default:
			assert_not_reached ();
		}

		return connection != null;
	}

	static bool direct_only;
	static weak Connection? singleton;
	static bool log_initialized;
	static StaticMutex door;

	public static new Connection get_internal (bool is_direct_only = false, Cancellable? cancellable = null) throws Sparql.Error, IOError, DBusError, SpawnError {
		door.lock ();

		// assign to owned variable to ensure it doesn't get freed between unlock and return
		var result = singleton;
		if (result != null) {
			assert (direct_only == is_direct_only);
			door.unlock ();
			return result;
		}

		log_init ();

		direct_only = is_direct_only;

		result = new Tracker.Sparql.Backend ();
		result.init ();

		if (cancellable != null && cancellable.is_cancelled ()) {
			door.unlock ();
			throw new IOError.CANCELLED ("Operation was cancelled");
		}

		singleton = result;
		result.add_weak_pointer ((void**) (&singleton));

		door.unlock ();

		return singleton;
	}

	public async static new Connection get_internal_async (bool is_direct_only = false, Cancellable? cancellable = null) throws Sparql.Error, IOError, DBusError, SpawnError {
		// fast path: avoid extra thread if connection is already available
		if (door.trylock ()) {
			// assign to owned variable to ensure it doesn't get freed between unlock and return
			var result = singleton;

			door.unlock ();

			if (result != null) {
				assert (direct_only == is_direct_only);
				return result;
			}
		}

		// run in a separate thread
		Sparql.Error sparql_error = null;
		IOError io_error = null;
		DBusError dbus_error = null;
		SpawnError spawn_error = null;
		Connection result = null;
		var context = MainContext.get_thread_default ();

		g_io_scheduler_push_job (job => {
			try {
				result = get_internal (is_direct_only, cancellable);
			} catch (IOError e_io) {
				io_error = e_io;
			} catch (Sparql.Error e_spql) {
				sparql_error = e_spql;
			} catch (DBusError e_dbus) {
				dbus_error = e_dbus;
			} catch (SpawnError e_spawn) {
				spawn_error = e_spawn;
			}

			var source = new IdleSource ();
			source.set_callback (() => {
				get_internal_async.callback ();
				return false;
			});
			source.attach (context);

			return false;
		});
		yield;

		if (sparql_error != null) {
			throw sparql_error;
		} else if (io_error != null) {
			throw io_error;
		} else if (dbus_error != null) {
			throw dbus_error;
		} else if (spawn_error != null) {
			throw spawn_error;
		} else {
			return result;
		}
	}

	private static void log_init () {
		if (log_initialized) {
			return;
		}

		log_initialized = true;

		// Avoid debug messages
		int verbosity = 0;
		string env_verbosity = Environment.get_variable ("TRACKER_VERBOSITY");
		if (env_verbosity != null)
			verbosity = env_verbosity.to_int ();

		LogLevelFlags remove_levels = 0;

		switch (verbosity) {
		// Log level 3: EVERYTHING
		case 3:
			break;

		// Log level 2: CRITICAL/ERROR/WARNING/INFO/MESSAGE only
		case 2:
			remove_levels = LogLevelFlags.LEVEL_DEBUG;
			break;

		// Log level 1: CRITICAL/ERROR/WARNING/INFO only
		case 1:
			remove_levels = LogLevelFlags.LEVEL_DEBUG |
			              LogLevelFlags.LEVEL_MESSAGE;
			break;

		// Log level 0: CRITICAL/ERROR/WARNING only (default)
		default:
		case 0:
			remove_levels = LogLevelFlags.LEVEL_DEBUG |
			              LogLevelFlags.LEVEL_MESSAGE |
			              LogLevelFlags.LEVEL_INFO;
			break;
		}

		if (remove_levels != 0) {
			GLib.Log.set_handler ("Tracker", remove_levels, remove_log_handler);
		}
	}

	private static void remove_log_handler (string? log_domain, LogLevelFlags log_level, string message) {
		/* do nothing */
	}
}

public async static Tracker.Sparql.Connection tracker_sparql_connection_get_async (Cancellable? cancellable = null) throws Tracker.Sparql.Error, IOError, DBusError, SpawnError {
	return yield Tracker.Sparql.Backend.get_internal_async (false, cancellable);
}

public static Tracker.Sparql.Connection tracker_sparql_connection_get (Cancellable? cancellable = null) throws Tracker.Sparql.Error, IOError, DBusError, SpawnError {
	return Tracker.Sparql.Backend.get_internal (false, cancellable);
}

public async static Tracker.Sparql.Connection tracker_sparql_connection_get_direct_async (Cancellable? cancellable = null) throws Tracker.Sparql.Error, IOError, DBusError, SpawnError {
	return yield Tracker.Sparql.Backend.get_internal_async (true, cancellable);
}

public static Tracker.Sparql.Connection tracker_sparql_connection_get_direct (Cancellable? cancellable = null) throws Tracker.Sparql.Error, IOError, DBusError, SpawnError {
	return Tracker.Sparql.Backend.get_internal (true, cancellable);
}
