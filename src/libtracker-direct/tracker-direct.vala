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

public class Tracker.Direct.Connection : Tracker.Sparql.Connection {
	// only single connection is currently supported per process
	static bool initialized;

	public Connection () throws Sparql.Error
	requires (!initialized) {
		uint select_cache_size = 100;
		string env_cache_size = Environment.get_variable ("TRACKER_SPARQL_CACHE_SIZE");

		if (env_cache_size != null) {
			select_cache_size = env_cache_size.to_int();
		}

		if (!Data.Manager.init (DBManagerFlags.READONLY, null, null, false, select_cache_size, 0, null, null)) {
			throw new Sparql.Error.INTERNAL ("Unable to initialize database");
		}

		initialized = true;
	}

	~Connection () {
		// Clean up connection
		if (initialized) {
			Data.Manager.shutdown ();
			initialized = false;
		}
	}

	public override Sparql.Cursor query (string sparql, Cancellable? cancellable) throws Sparql.Error, IOError, DBusError {
		try {
			var query_object = new Sparql.Query (sparql);
			var cursor = query_object.execute_cursor ();
			cursor.connection = this;
			return cursor;
		} catch (DBInterfaceError e) {
			throw new Sparql.Error.INTERNAL (e.message);
		} catch (DateError e) {
			throw new Sparql.Error.PARSE (e.message);
		}
	}

	public async override Sparql.Cursor query_async (string sparql, Cancellable? cancellable = null) throws Sparql.Error, IOError, DBusError {
		// just creating the cursor won't block
		return query (sparql, cancellable);
	}
}

public Tracker.Sparql.Connection? module_init () {
	try {
		Tracker.Sparql.Connection plugin = new Tracker.Direct.Connection ();
		return plugin;
	} catch (Tracker.Sparql.Error e) {
		return null;
	}
}
