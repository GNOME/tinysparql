/*
 * Copyright (C) 2008-2009, Nokia
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

namespace Tracker {
	[CCode (cheader_filename = "libtracker-db/tracker-db-manager.h")]
	public enum DB {
		UNKNOWN,
		COMMON,
		CACHE,
		METADATA,
		CONTENTS
	}

	[CCode (cprefix = "TRACKER_DB_", cheader_filename = "libtracker-db/tracker-db-interface.h")]
	public errordomain DBInterfaceError {
		QUERY_ERROR,
		CORRUPT,
		INTERRUPTED
	}

	[CCode (cheader_filename = "libtracker-db/tracker-db-interface.h")]
	public interface DBInterface : GLib.Object {
		[PrintfFormat]
		public abstract DBStatement create_statement (...) throws DBInterfaceError;
	}

	[CCode (cheader_filename = "libtracker-db/tracker-db-manager.h")]
	namespace DBManager {
		public unowned DBInterface get_db_interface ();
	}

	[CCode (cheader_filename = "libtracker-db/tracker-db-interface.h")]
	public class DBResultSet : GLib.Object {
		public void _get_value (uint column, out GLib.Value value);
		public bool iter_next ();
	}

	[CCode (cheader_filename = "libtracker-db/tracker-db-interface.h")]
	public class DBCursor : GLib.Object {
	}

	[CCode (cheader_filename = "libtracker-db/tracker-db-interface.h")]
	public interface DBStatement : GLib.Object {
		public abstract void bind_double (int index, double value);
		public abstract void bind_int (int index, int value);
		public abstract void bind_text (int index, string value);
		public abstract DBResultSet execute () throws DBInterfaceError;
		public abstract DBCursor start_cursor () throws DBInterfaceError;
	}
}

