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

	public Connection ()
	requires (!initialized) {
		initialized = true;
		Data.Manager.init (DBManagerFlags.READONLY, null, null, false, null, null);
	}

	~Connection () {
		// Clean up connection
		Data.Manager.shutdown ();
		initialized = false;
	}

	public override Sparql.Cursor query (string sparql, Cancellable? cancellable) throws GLib.Error {
		var query_object = new Sparql.Query (sparql);
		var cursor = query_object.execute_cursor ();
		cursor.connection = this;
		return cursor;
	}

	public async override Sparql.Cursor query_async (string sparql, Cancellable? cancellable = null) throws GLib.Error {
		// just creating the cursor won't block
		return query (sparql, cancellable);
	}
}

public Tracker.Sparql.Connection module_init () {
	Tracker.Sparql.Connection plugin = new Tracker.Direct.Connection ();
	return plugin;
}
