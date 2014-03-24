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

class Tracker.Sparql.Backend : Connection {
	bool initialized;
	Tracker.Sparql.Connection direct = null;
	Tracker.Sparql.Connection bus = null;
	enum Backend {
		AUTO,
		DIRECT,
		BUS
	}
	GLib.BusType bus_type = BusType.SESSION;

	public Backend () throws Sparql.Error, IOError, DBusError, SpawnError {
		try {
			// Important to make sure we check the right bus for the store
			load_env ();

			// Makes sure the sevice is available
			debug ("Waiting for service to become available...");

			// do not use proxy to work around race condition in GDBus
			// NB#259760
			var bus = GLib.Bus.get_sync (bus_type);
			var msg = new DBusMessage.method_call (TRACKER_DBUS_SERVICE, TRACKER_DBUS_OBJECT_STATUS, TRACKER_DBUS_INTERFACE_STATUS, "Wait");
			bus.send_message_with_reply_sync (msg, 0, /* timeout */ int.MAX, null).to_gerror ();

			debug ("Service is ready");

			debug ("Constructing connection");
			load_plugins ();
			debug ("Backend is ready");
		} catch (GLib.Error e) {
			throw new Sparql.Error.INTERNAL (e.message);
		}

		initialized = true;
	}

	private void load_env () {
		string env_bus_type = Environment.get_variable ("TRACKER_BUS_TYPE");

		if (env_bus_type != null) {
			if (env_bus_type.ascii_casecmp ("system") == 0) {
				bus_type = BusType.SYSTEM;
				debug ("Using bus = 'SYSTEM'");
			} else if (env_bus_type.ascii_casecmp ("session") == 0) {
				bus_type = BusType.SESSION;
				debug ("Using bus = 'SESSION'");
			} else {
				warning ("Environment variable TRACKER_BUS_TYPE set to unknown value '%s'", env_bus_type);
			}
		}
	}

	public override void dispose () {
		// trying to lock on partially initialized instances will deadlock
		if (initialized) {
			door.lock ();

			try {
				// Ensure this instance is not used for any new calls to Tracker.Sparql.Connection.get.
				// However, a call to Tracker.Sparql.Connection.get between g_object_unref and the
				// above lock might have increased the reference count of this instance to 2 (or more).
				// Therefore, we must not clean up direct/bus connection in dispose.
				if (singleton == this) {
					singleton = null;
				}
			} finally {
				door.unlock ();
			}
		}

		base.dispose ();
	}

	public override Cursor query (string sparql, Cancellable? cancellable = null) throws Sparql.Error, IOError, DBusError {
		debug ("%s(): '%s'", Log.METHOD, sparql);
		if (direct != null) {
			return direct.query (sparql, cancellable);
		} else {
			return bus.query (sparql, cancellable);
		}
	}

	public async override Cursor query_async (string sparql, Cancellable? cancellable = null) throws Sparql.Error, IOError, DBusError {
		debug ("%s(): '%s'", Log.METHOD, sparql);
		if (direct != null) {
			return yield direct.query_async (sparql, cancellable);
		} else {
			return yield bus.query_async (sparql, cancellable);
		}
	}

	public override void update (string sparql, int priority = GLib.Priority.DEFAULT, Cancellable? cancellable = null) throws Sparql.Error, IOError, DBusError {
		debug ("%s(priority:%d): '%s'", Log.METHOD, priority, sparql);
		if (bus == null) {
			throw new Sparql.Error.UNSUPPORTED ("Update support not available for direct-only connection");
		}
		bus.update (sparql, priority, cancellable);
	}

	public override GLib.Variant? update_blank (string sparql, int priority = GLib.Priority.DEFAULT, Cancellable? cancellable = null) throws Sparql.Error, IOError, DBusError {
		debug ("%s(priority:%d): '%s'", Log.METHOD, priority, sparql);
		if (bus == null) {
			throw new Sparql.Error.UNSUPPORTED ("Update support not available for direct-only connection");
		}
		return bus.update_blank (sparql, priority, cancellable);
	}

	public async override void update_async (string sparql, int priority = GLib.Priority.DEFAULT, Cancellable? cancellable = null) throws Sparql.Error, IOError, DBusError {
		debug ("%s(priority:%d): '%s'", Log.METHOD, priority, sparql);
		if (bus == null) {
			throw new Sparql.Error.UNSUPPORTED ("Update support not available for direct-only connection");
		}
		yield bus.update_async (sparql, priority, cancellable);
	}

	public async override GenericArray<Error?>? update_array_async (string[] sparql, int priority = GLib.Priority.DEFAULT, Cancellable? cancellable = null) throws Sparql.Error, IOError, DBusError {
		if (bus == null) {
			throw new Sparql.Error.UNSUPPORTED ("Update support not available for direct-only connection");
		}
		return yield bus.update_array_async (sparql, priority, cancellable);
	}

	public async override GLib.Variant? update_blank_async (string sparql, int priority = GLib.Priority.DEFAULT, Cancellable? cancellable = null) throws Sparql.Error, IOError, DBusError {
		debug ("%s(priority:%d): '%s'", Log.METHOD, priority, sparql);
		if (bus == null) {
			throw new Sparql.Error.UNSUPPORTED ("Update support not available for direct-only connection");
		}
		return yield bus.update_blank_async (sparql, priority, cancellable);
	}

	public override void load (File file, Cancellable? cancellable = null) throws Sparql.Error, IOError, DBusError {
		var uri = file.get_uri ();
		debug ("%s(): '%s'", Log.METHOD, uri);
		if (bus == null) {
			throw new Sparql.Error.UNSUPPORTED ("Update support not available for direct-only connection");
		}
		bus.load (file, cancellable);
	}

	public async override void load_async (File file, Cancellable? cancellable = null) throws Sparql.Error, IOError, DBusError {
		var uri = file.get_uri ();
		debug ("%s(): '%s'", Log.METHOD, uri);
		if (bus == null) {
			throw new Sparql.Error.UNSUPPORTED ("Update support not available for direct-only connection");
		}
		yield bus.load_async (file, cancellable);
	}

	public override Cursor? statistics (Cancellable? cancellable = null) throws Sparql.Error, IOError, DBusError {
		debug ("%s()", Log.METHOD);
		if (bus == null) {
			throw new Sparql.Error.UNSUPPORTED ("Statistics support not available for direct-only connection");
		}
		return bus.statistics (cancellable);
	}

	public async override Cursor? statistics_async (Cancellable? cancellable = null) throws Sparql.Error, IOError, DBusError {
		debug ("%s()", Log.METHOD);
		if (bus == null) {
			throw new Sparql.Error.UNSUPPORTED ("Statistics support not available for direct-only connection");
		}
		return yield bus.statistics_async (cancellable);
	}

	// Plugin loading functions
	private void load_plugins () throws GLib.Error {
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
			debug ("Using backend = 'AUTO'");
		}

		switch (backend) {
		case Backend.AUTO:
			try {
				direct = new Tracker.Direct.Connection ();
			} catch (Error e) {
				debug ("Unable to initialize direct backend: " + e.message);
			}

			bus = new Tracker.Bus.Connection ();
			break;

		case Backend.DIRECT:
			direct = new Tracker.Direct.Connection ();
			break;

		case Backend.BUS:
			bus = new Tracker.Bus.Connection ();
			break;

		default:
			assert_not_reached ();
		}
	}

	static weak Connection? singleton;
	static bool log_initialized;
	static Mutex door;

	static new Connection get (Cancellable? cancellable = null) throws Sparql.Error, IOError, DBusError, SpawnError {
		door.lock ();

		try {
			// assign to owned variable to ensure it doesn't get freed between check and return
			var result = singleton;

			if (result == null) {
				log_init ();

				result = new Tracker.Sparql.Backend ();

				if (cancellable != null && cancellable.is_cancelled ()) {
					throw new IOError.CANCELLED ("Operation was cancelled");
				}

				singleton = result;
			}

			return result;
		} finally {
			door.unlock ();
		}
	}

	public static new Connection get_internal (Cancellable? cancellable = null) throws Sparql.Error, IOError, DBusError, SpawnError {
		if (MainContext.get_thread_default () == null) {
			// ok to initialize without extra thread
			return get (cancellable);
		}

		// run with separate main context to be able to wait for async method
		var context = new MainContext ();
		var loop = new MainLoop (context);
		AsyncResult async_result = null;

		context.push_thread_default ();

		get_internal_async.begin (cancellable, (obj, res) => {
			async_result = res;
			loop.quit ();
		});

		loop.run ();

		context.pop_thread_default ();

		return get_internal_async.end (async_result);
	}

	public async static new Connection get_internal_async (Cancellable? cancellable = null) throws Sparql.Error, IOError, DBusError, SpawnError {
		// fast path: avoid extra thread if connection is already available
		if (door.trylock ()) {
			// assign to owned variable to ensure it doesn't get freed between unlock and return
			var result = singleton;

			door.unlock ();

			if (result != null) {
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
				result = get (cancellable);
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
			verbosity = int.parse (env_verbosity);

		LogLevelFlags remove_levels = 0;

		// If we have debug enabled, we imply G_MESSAGES_DEBUG or we
		// see nothing, this came in since GLib 2.32.
		if (verbosity > 2)
			Environment.set_variable ("G_MESSAGES_DEBUG", "all", true);

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
	return yield Tracker.Sparql.Backend.get_internal_async (cancellable);
}

public static Tracker.Sparql.Connection tracker_sparql_connection_get (Cancellable? cancellable = null) throws Tracker.Sparql.Error, IOError, DBusError, SpawnError {
	return Tracker.Sparql.Backend.get_internal (cancellable);
}

public async static Tracker.Sparql.Connection tracker_sparql_connection_get_direct_async (Cancellable? cancellable = null) throws Tracker.Sparql.Error, IOError, DBusError, SpawnError {
	return yield Tracker.Sparql.Backend.get_internal_async (cancellable);
}

public static Tracker.Sparql.Connection tracker_sparql_connection_get_direct (Cancellable? cancellable = null) throws Tracker.Sparql.Error, IOError, DBusError, SpawnError {
	return Tracker.Sparql.Backend.get_internal (cancellable);
}
