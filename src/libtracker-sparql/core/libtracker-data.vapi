/*
 * Copyright (C) 2008-2011, Nokia
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
	public enum DB {
		UNKNOWN,
		COMMON,
		CACHE,
		METADATA,
		CONTENTS
	}

	public errordomain DBInterfaceError {
		QUERY_ERROR,
		CORRUPT,
		INTERRUPTED,
		OPEN_ERROR,
		NO_SPACE
	}

	public errordomain DBJournalError {
		UNKNOWN,
		DAMAGED_JOURNAL_ENTRY,
		COULD_NOT_WRITE,
		COULD_NOT_CLOSE,
		BEGIN_OF_JOURNAL
	}

	public enum DBStatementCacheType {
		SELECT,
		UPDATE,
		NONE
	}

	public delegate void DBWalCallback (DBInterface iface, int n_pages);

	public interface DBInterface : GLib.Object {
		[PrintfFormat]
		public DBStatement create_statement (DBStatementCacheType cache_type, ...) throws DBInterfaceError;
		[PrintfFormat]
		public void execute_query (...) throws DBInterfaceError;
		public void sqlite_wal_hook (DBWalCallback callback);
		public void sqlite_wal_checkpoint (bool blocking) throws DBInterfaceError;
		public unowned GLib.Object get_user_data ();
	}

	public delegate void BusyCallback (string status, double progress);

	public enum DBManagerFlags {
		FORCE_REINDEX,
		REMOVE_ALL,
		READONLY,
		DO_NOT_CHECK_ONTOLOGY,
		ENABLE_MUTEXES,
	}

	namespace DBManager {
		public void lock ();
		public bool trylock ();
		public void unlock ();
		public bool locale_changed () throws DBInterfaceError;
	}

	public class DBCursor : Sparql.Cursor {
	}

	public interface DBStatement : GLib.InitiallyUnowned {
		public abstract void bind_double (int index, double value);
		public abstract void bind_int (int index, int value);
		public abstract void bind_text (int index, string value);
		public abstract DBCursor start_cursor () throws DBInterfaceError;
		public abstract DBCursor start_sparql_cursor (PropertyType[] types, string[] variable_names) throws DBInterfaceError;
	}

	public class DBConfig : ConfigFile {
		public DBConfig ();
		public bool save ();
		public int journal_chunk_size { get; set; }
		public string journal_rotate_destination { owned get; set; }
	}

	namespace DBJournal {
		public void set_rotating (bool do_rotating, size_t chunk_size, string? rotate_to);
	}

	public class Class : GLib.Object {
		public string name { get; set; }
		public string uri { get; set; }
		public unowned Class[] get_super_classes ();
		public void transact_events ();
		public bool notify { get; set; }

		public bool has_insert_events ();
		public bool has_delete_events ();
		public void foreach_delete_event (EventsForeach func);
		public void foreach_insert_event (EventsForeach func);
		public void reset_ready_events ();
	}

	public delegate void EventsForeach (int graph_id, int subject_id, int pred_id, int object_id);

	public class Namespace : GLib.Object {
		public string prefix { get; set; }
		public string uri { get; set; }
	}

	public class Property : GLib.Object {
		public string name { get; }
		public string table_name { get; }
		public string uri { get; set; }
		public PropertyType data_type { get; set; }
		public Class domain { get; set; }
		public Class range { get; set; }
		public bool multiple_values { get; set; }
		public bool is_inverse_functional_property { get; set; }
		public unowned Class[] get_domain_indexes ();
	}

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

	public class Ontologies : GLib.Object {
		public unowned Class get_class_by_uri (string class_uri);
		public unowned Property get_property_by_uri (string property_uri);
		public unowned Namespace[] get_namespaces ();
		public unowned Class[] get_classes ();
		public unowned Property[] get_properties ();
	}

	public delegate void StatementCallback (int graph_id, string? graph, int subject_id, string subject, int predicate_id, int object_id, string object, GLib.GenericArray<unowned Class> rdf_types);
	public delegate void CommitCallback ();

	public class Data.Update : GLib.Object {
		public void begin_db_transaction ();
		public void commit_db_transaction ();
		public void begin_transaction () throws DBInterfaceError;
		public void commit_transaction () throws DBInterfaceError;
		public void rollback_transaction ();
		public void update_sparql (string update) throws Sparql.Error;
		public GLib.Variant update_sparql_blank (string update) throws Sparql.Error;
		public void load_turtle_file (GLib.File file) throws Sparql.Error;
		public void delete_statement (string? graph, string subject, string predicate, GLib.Bytes object) throws Sparql.Error, DateError;
		public void update_statement (string? graph, string subject, string predicate, GLib.Bytes? object) throws Sparql.Error, DateError;
		public void insert_statement (string? graph, string subject, string predicate, GLib.Bytes object) throws Sparql.Error, DateError;
		public void insert_statement_with_uri (string? graph, string subject, string predicate, GLib.Bytes object) throws Sparql.Error;
		public void insert_statement_with_string (string? graph, string subject, string predicate, GLib.Bytes object) throws Sparql.Error, DateError;
		public void update_buffer_flush () throws DBInterfaceError;
		public void update_buffer_might_flush () throws DBInterfaceError;
		public void sync ();

		public void add_insert_statement_callback (StatementCallback callback);
		public void add_delete_statement_callback (StatementCallback callback);
		public void add_commit_statement_callback (CommitCallback callback);
		public void add_rollback_statement_callback (CommitCallback callback);
		public void remove_insert_statement_callback (StatementCallback callback);
		public void remove_delete_statement_callback (StatementCallback callback);
		public void remove_commit_statement_callback (CommitCallback callback);
		public void remove_rollback_statement_callback (CommitCallback callback);
	}

	namespace Data {
		public int query_resource_id (Data.Manager manager, DBInterface iface, string uri);
		public DBCursor query_sparql_cursor (Data.Manager manager, string query) throws Sparql.Error;
	}

	public class Data.Manager : GLib.Object, GLib.Initable {
		public Manager (DBManagerFlags flags, GLib.File cache_location, GLib.File data_location, GLib.File ontology_location, bool journal_check, bool restoring_backup, uint select_cache_size, uint update_cache_size);
                public unowned Ontologies get_ontologies ();
		public unowned DBInterface get_db_interface ();
		public unowned DBInterface get_writable_db_interface ();
		public unowned Data.Update get_data ();
		public void shutdown ();
		public GLib.HashTable<string,string> get_namespaces ();
	}

	public const string COLLATION_NAME;

	public const string TITLE_COLLATION_NAME;

	public const unichar COLLATION_LAST_CHAR;
}
