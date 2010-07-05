/*
 * Copyright (C) 2008-2010, Nokia
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
	[CCode (cheader_filename = "libtracker-data/tracker-db-manager.h")]
	public enum DB {
		UNKNOWN,
		COMMON,
		CACHE,
		METADATA,
		CONTENTS
	}

	[CCode (cprefix = "TRACKER_DB_", cheader_filename = "libtracker-data/tracker-db-interface.h")]
	public errordomain DBInterfaceError {
		QUERY_ERROR,
		CORRUPT,
		INTERRUPTED
	}

	[CCode (cheader_filename = "libtracker-data/tracker-db-interface.h")]
	public interface DBInterface : GLib.Object {
		[PrintfFormat]
		public abstract DBStatement create_statement (...) throws DBInterfaceError;
	}

	[CCode (cheader_filename = "libtracker-data/tracker-db-manager.h")]
	namespace DBManager {
		public unowned DBInterface get_db_interface ();
	}

	[CCode (cheader_filename = "libtracker-data/tracker-db-interface.h")]
	public class DBResultSet : GLib.Object {
		public void _get_value (uint column, out GLib.Value value);
		public bool iter_next ();
	}

	[CCode (cheader_filename = "libtracker-data/tracker-db-interface.h")]
	public class DBCursor : GLib.Object {
	}

	[CCode (cheader_filename = "libtracker-data/tracker-db-interface.h")]
	public interface DBStatement : GLib.Object {
		public abstract void bind_double (int index, double value);
		public abstract void bind_int (int index, int value);
		public abstract void bind_text (int index, string value);
		public abstract DBResultSet execute () throws DBInterfaceError;
		public abstract DBCursor start_cursor () throws DBInterfaceError;
	}

	[CCode (cheader_filename = "libtracker-data/tracker-class.h")]
	public class Class : GLib.Object {
		public string name { get; set; }
		public string uri { get; set; }
		[CCode (array_length = false, array_null_terminated = true)]
		public unowned Class[] get_super_classes ();
	}

	[CCode (cheader_filename = "libtracker-data/tracker-namespace.h")]
	public class Namespace : GLib.Object {
		public string prefix { get; set; }
		public string uri { get; set; }
	}

	[CCode (cheader_filename = "libtracker-data/tracker-property.h")]
	public class Property : GLib.Object {
		public string name { get; }
		public string table_name { get; }
		public string uri { get; set; }
		public PropertyType data_type { get; set; }
		public Class domain { get; set; }
		public Class range { get; set; }
		public bool multiple_values { get; set; }
		public bool is_inverse_functional_property { get; set; }
		[CCode (array_length = false, array_null_terminated = true)]
		public unowned Class[] get_domain_indexes ();
	}

	[CCode (cheader_filename = "libtracker-data/tracker-property.h")]
	public enum PropertyType {
		UNKNOWN,
		STRING,
		BOOLEAN,
		INTEGER,
		DOUBLE,
		DATE,
		DATETIME,
		RESOURCE
	}

	[CCode (cheader_filename = "libtracker-data/tracker-ontologies.h")]
	namespace Ontologies {
		public unowned Class get_class_by_uri (string class_uri);
		public unowned Property get_property_by_uri (string property_uri);
		public unowned Namespace[] get_namespaces ();
		public unowned Class[] get_classes ();
		public unowned Property[] get_properties ();
	}

	[CCode (cheader_filename = "libtracker-data/tracker-data-update.h")]
	public errordomain DataError {
		UNKNOWN_CLASS,
		UNKNOWN_PROPERTY,
		INVALID_TYPE
	}

	[CCode (cheader_filename = "libtracker-data/tracker-data-query.h,libtracker-data/tracker-data-update.h")]
	namespace Data {
		public int query_resource_id (string uri);
		public void begin_db_transaction ();
		public void commit_db_transaction ();
		public void begin_transaction () throws DBInterfaceError;
		public void commit_transaction () throws DBInterfaceError;
		public void rollback_transaction ();
		public void delete_statement (string graph, string subject, string predicate, string object) throws DataError, DateError;
		public void insert_statement (string graph, string subject, string predicate, string object) throws DataError, DateError;
		public void insert_statement_with_uri (string graph, string subject, string predicate, string object) throws DataError;
		public void insert_statement_with_string (string graph, string subject, string predicate, string object) throws DataError, DateError;
		public void delete_resource_description (string graph, string uri) throws DataError;
		public void update_buffer_flush () throws DBInterfaceError;
		public void update_buffer_might_flush () throws DBInterfaceError;
	}
}

