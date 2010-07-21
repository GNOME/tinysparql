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

// Convenience
public const string TRACKER_DBUS_SERVICE = "org.freedesktop.Tracker1";
public const string TRACKER_DBUS_INTERFACE_RESOURCES = TRACKER_DBUS_SERVICE + ".Resources";
public const string TRACKER_DBUS_OBJECT_RESOURCES = "/org/freedesktop/Tracker1/Resources";
public const string TRACKER_DBUS_INTERFACE_STATISTICS = TRACKER_DBUS_SERVICE + ".Statistics";
public const string TRACKER_DBUS_OBJECT_STATISTICS = "/org/freedesktop/Tracker1/Statistics";
public const string TRACKER_DBUS_INTERFACE_STEROIDS = TRACKER_DBUS_SERVICE + ".Steroids";
public const string TRACKER_DBUS_OBJECT_STEROIDS = "/org/freedesktop/Tracker1/Steroids";

public abstract class Tracker.Sparql.Connection : Object {
	static bool direct_only;
	static weak Connection? singleton;
	static int verbosity = 0;

	public static Connection get () throws GLib.Error {
		if (singleton != null) {
			assert (!direct_only);
			return singleton;
		} else {
			log_init ();
			
			var result = new PluginLoader ();
			singleton = result;
			result.add_weak_pointer ((void**) (&singleton));
			return result;
		}
	}

	public static Connection get_direct () throws GLib.Error {
		if (singleton != null) {
			assert (direct_only);
			return singleton;
		} else {
			log_init ();
			
			var result = new PluginLoader (true /* direct_only */);
			direct_only = true;
			singleton = result;
			result.add_weak_pointer ((void**) (&singleton));
			return result;
		}
	}

	private static void log_init () {
		// Avoid debug messages
		string env_verbosity = Environment.get_variable ("TRACKER_SPARQL_VERBOSITY");
		if (env_verbosity != null)
			verbosity = env_verbosity.to_int ();
	
		GLib.Log.set_handler (null, LogLevelFlags.LEVEL_MASK | LogLevelFlags.FLAG_FATAL, log_handler);
		GLib.Log.set_default_handler (log_handler);
	}

	private static bool log_should_handle (LogLevelFlags log_level) {
		switch (verbosity) {
		// Log level 3: EVERYTHING
		case 3:
			break;

		// Log level 2: CRITICAL/ERROR/WARNING/INFO/MESSAGE only
		case 2:
			if (!(LogLevelFlags.LEVEL_MESSAGE in log_level) &&
				!(LogLevelFlags.LEVEL_INFO in log_level) &&
				!(LogLevelFlags.LEVEL_WARNING in log_level) &&
				!(LogLevelFlags.LEVEL_ERROR in log_level) &&
				!(LogLevelFlags.LEVEL_CRITICAL in log_level)) {
				return false;
			}

			break;

		// Log level 1: CRITICAL/ERROR/WARNING/INFO only
		case 1:
			if (!(LogLevelFlags.LEVEL_INFO in log_level) &&
				!(LogLevelFlags.LEVEL_WARNING in log_level) &&
				!(LogLevelFlags.LEVEL_ERROR in log_level) &&
				!(LogLevelFlags.LEVEL_CRITICAL in log_level)) {
				return false;
			}

			break;

		// Log level 0: CRITICAL/ERROR/WARNING only (default)
		default:
		case 0:
			if (!(LogLevelFlags.LEVEL_WARNING in log_level) &&
				!(LogLevelFlags.LEVEL_ERROR in log_level) &&
				!(LogLevelFlags.LEVEL_CRITICAL in log_level)) {
				return false;
			}

			break;
		}

		return true;
	}

	private static void log_handler (string? log_domain, LogLevelFlags log_level, string message) {
		if (!log_should_handle (log_level)) {
			return;
		}

		GLib.Log.default_handler (log_domain, log_level, message);
	}

	// Query
	public abstract Cursor query (string sparql, Cancellable? cancellable = null) throws GLib.Error;
	public async abstract Cursor query_async (string sparql, Cancellable? cancellable = null) throws GLib.Error;

	// Update
	public virtual void update (string sparql, Cancellable? cancellable = null) throws GLib.Error {
		warning ("Interface 'update' not implemented");
	}
	public async virtual void update_async (string sparql, int? priority = GLib.Priority.DEFAULT, Cancellable? cancellable = null) throws GLib.Error {
		warning ("Interface 'update_async' not implemented");
	}

	// Only applies to update_async with the right priority. 
	// Priority is used to identify batch updates.
	public virtual void update_commit (Cancellable? cancellable = null) throws GLib.Error {
		warning ("Interface 'update_commit' not implemented");
	}
	public async virtual void update_commit_async (Cancellable? cancellable = null) throws GLib.Error {
		warning ("Interface 'update_commit_async' not implemented");
	}
	
	// Import
	public virtual void import (File file, Cancellable? cancellable = null) throws GLib.Error {
		warning ("Interface 'import' not implemented");
	}
	public async virtual void import_async (File file, Cancellable? cancellable = null) throws GLib.Error {
		warning ("Interface 'import_async' not implemented");
	}
	
	// Statistics
	public virtual Cursor? statistics (Cancellable? cancellable = null) throws GLib.Error {
		warning ("Interface 'statistics' not implemented");
		return null;
	}
	
	public async virtual Cursor? statistics_async (Cancellable? cancellable = null) throws GLib.Error {
		warning ("Interface 'statistics_async' not implemented");
		return null;
	}
}
