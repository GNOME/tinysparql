/*
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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

#include "config.h"

#include <string.h>
#include <stdlib.h>
#include <regex.h>
#include <zlib.h>
#include <locale.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

#include <glib/gstdio.h>

#include <libtracker-common/tracker-date-time.h>
#include <libtracker-common/tracker-file-utils.h>
#include <libtracker-common/tracker-utils.h>
#include <libtracker-fts/tracker-fts.h>

#include "tracker-db-journal.h"
#include "tracker-db-manager.h"
#include "tracker-db-interface-sqlite.h"
#include "tracker-db-interface.h"

/* ZLib buffer settings */
#define ZLIB_BUF_SIZE                 8192

/* Required minimum space needed to create databases (5Mb) */
#define TRACKER_DB_MIN_REQUIRED_SPACE 5242880

/* Default memory settings for databases */
#define TRACKER_DB_PAGE_SIZE_DONT_SET -1

/* Set current database version we are working with */
#define TRACKER_DB_VERSION_NOW        TRACKER_DB_VERSION_0_9_0
#define TRACKER_DB_VERSION_FILE       "db-version.txt"

#define IN_USE_FILENAME               ".meta.isrunning"

/* Stamp filename to check for first index */
#define FIRST_INDEX_STAMP_FILENAME    ".firstindex"

typedef enum {
	TRACKER_DB_LOCATION_DATA_DIR,
	TRACKER_DB_LOCATION_USER_DATA_DIR,
	TRACKER_DB_LOCATION_SYS_TMP_DIR,
} TrackerDBLocation;

typedef enum {
	TRACKER_DB_VERSION_UNKNOWN, /* Unknown */
	TRACKER_DB_VERSION_0_6_6,   /* before indexer-split */
	TRACKER_DB_VERSION_0_6_90,  /* after  indexer-split */
	TRACKER_DB_VERSION_0_6_91,  /* stable release */
	TRACKER_DB_VERSION_0_6_92,  /* current TRUNK */
	TRACKER_DB_VERSION_0_7_0,   /* vstore branch */
	TRACKER_DB_VERSION_0_7_4,   /* nothing special */
	TRACKER_DB_VERSION_0_7_12,  /* nmo ontology */
	TRACKER_DB_VERSION_0_7_13,  /* coalesce & writeback */
	TRACKER_DB_VERSION_0_7_17,  /* mlo ontology */
	TRACKER_DB_VERSION_0_7_20,  /* nco im ontology */
	TRACKER_DB_VERSION_0_7_21,  /* named graphs/localtime */
	TRACKER_DB_VERSION_0_7_22,  /* fts-limits branch */
	TRACKER_DB_VERSION_0_7_28,  /* RC1 + mto + nco:url */
	TRACKER_DB_VERSION_0_8_0,   /* stable release */
	TRACKER_DB_VERSION_0_9_0    /* unstable release */
} TrackerDBVersion;

typedef struct {
	TrackerDB           db;
	TrackerDBLocation   location;
	TrackerDBInterface *iface;
	const gchar        *file;
	const gchar        *name;
	gchar              *abs_filename;
	gint                cache_size;
	gint                page_size;
	gboolean            attached;
	gboolean            is_index;
	guint64             mtime;
} TrackerDBDefinition;

static TrackerDBDefinition dbs[] = {
	{ TRACKER_DB_UNKNOWN,
	  TRACKER_DB_LOCATION_USER_DATA_DIR,
	  NULL,
	  NULL,
	  NULL,
	  NULL,
	  32,
	  TRACKER_DB_PAGE_SIZE_DONT_SET,
	  FALSE,
	  FALSE,
	  0 },
	{ TRACKER_DB_METADATA,
	  TRACKER_DB_LOCATION_DATA_DIR,
	  NULL,
	  "meta.db",
	  "meta",
	  NULL,
	  2000,
	  TRACKER_DB_PAGE_SIZE_DONT_SET,
	  FALSE,
	  FALSE,
	  0 },
	{ TRACKER_DB_CONTENTS,
	  TRACKER_DB_LOCATION_DATA_DIR,
	  NULL,
	  "contents.db",
	  "contents",
	  NULL,
	  1024,
	  TRACKER_DB_PAGE_SIZE_DONT_SET,
	  FALSE,
	  FALSE,
	  0 },
	{ TRACKER_DB_FULLTEXT,
	  TRACKER_DB_LOCATION_DATA_DIR,
	  NULL,
	  "fulltext.db",
	  "fulltext",
	  NULL,
	  512,
	  TRACKER_DB_PAGE_SIZE_DONT_SET,
	  FALSE,
	  TRUE,
	  0 },
};

static gboolean            db_exec_no_reply    (TrackerDBInterface *iface,
                                                const gchar        *query,
                                                ...);
static TrackerDBInterface *db_interface_create (TrackerDB           db);
static TrackerDBInterface *tracker_db_manager_get_db_interfaces     (gint num, ...);
static TrackerDBInterface *tracker_db_manager_get_db_interfaces_ro  (gint num, ...);

static gboolean              initialized;
static gboolean              locations_initialized;
static gchar                *sql_dir;
static gchar                *data_dir = NULL;
static gchar                *user_data_dir = NULL;
static gchar                *sys_tmp_dir = NULL;
static gpointer              db_type_enum_class_pointer;
static TrackerDBManagerFlags old_flags = 0;

static GHashTable           *thread_ifaces = NULL; /* Needed for cross-thread cancellation */
static GStaticMutex          thread_ifaces_mutex = G_STATIC_MUTEX_INIT;

static GStaticPrivate        interface_data_key = G_STATIC_PRIVATE_INIT;

static const gchar *
location_to_directory (TrackerDBLocation location)
{
	switch (location) {
	case TRACKER_DB_LOCATION_DATA_DIR:
		return data_dir;
	case TRACKER_DB_LOCATION_USER_DATA_DIR:
		return user_data_dir;
	case TRACKER_DB_LOCATION_SYS_TMP_DIR:
		return sys_tmp_dir;
	default:
		return NULL;
	};
}

static void
load_sql_file (TrackerDBInterface *iface,
               const gchar        *file,
               const gchar        *delimiter)
{
	gchar *path, *content, **queries;
	gint   count;
	gint   i;

	path = g_build_filename (sql_dir, file, NULL);

	if (!delimiter) {
		delimiter = ";";
	}

	if (!g_file_get_contents (path, &content, NULL, NULL)) {
		g_critical ("Cannot read SQL file:'%s', please reinstall tracker"
		            " or check read permissions on the file if it exists", path);
		g_assert_not_reached ();
	}

	queries = g_strsplit (content, delimiter, -1);

	for (i = 0, count = 0; queries[i]; i++) {
		GError *error = NULL;
		gchar  *sql;

		/* Skip white space, including control characters */
		for (sql = queries[i]; sql && g_ascii_isspace (sql[0]); sql++);

		if (!sql || sql[0] == '\0') {
			continue;
		}

		tracker_db_interface_execute_query (iface, &error, "%s", sql);

		if (error) {
			g_warning ("Error loading query:'%s' #%d, %s", file, i, error->message);
			g_error_free (error);
			continue;
		}

		count++;
	}

	g_message ("  Loaded SQL file:'%s' (%d queries)", file, count);

	g_strfreev (queries);
	g_free (content);
	g_free (path);
}

static gboolean
db_exec_no_reply (TrackerDBInterface *iface,
                  const gchar        *query,
                  ...)
{
	TrackerDBResultSet *result_set;
	va_list                     args;

	va_start (args, query);
	result_set = tracker_db_interface_execute_vquery (iface, NULL, query, args);
	va_end (args);

	if (result_set) {
		g_object_unref (result_set);
	}

	return TRUE;
}

TrackerDBManagerFlags
tracker_db_manager_get_flags (void)
{
	return old_flags;
}

static void
db_set_params (TrackerDBInterface *iface,
               gint                cache_size,
               gint                page_size)
{
	tracker_db_interface_execute_query (iface, NULL, "PRAGMA synchronous = OFF;");
	tracker_db_interface_execute_query (iface, NULL, "PRAGMA count_changes = 0;");
	tracker_db_interface_execute_query (iface, NULL, "PRAGMA temp_store = FILE;");
	tracker_db_interface_execute_query (iface, NULL, "PRAGMA encoding = \"UTF-8\"");
	tracker_db_interface_execute_query (iface, NULL, "PRAGMA auto_vacuum = 0;");

	/* if we add direct access library (or enable running queries in parallel with updates,
	   we need to disable read_uncommitted again
	   however, when read_uncommitted is disabled, we need the custom page cache (wrapper)
	   to avoid locking issues between long running batch transactions and queries */
	tracker_db_interface_execute_query (iface, NULL, "PRAGMA read_uncommitted = True;");

	if (page_size != TRACKER_DB_PAGE_SIZE_DONT_SET) {
		g_message ("  Setting page size to %d", page_size);
		tracker_db_interface_execute_query (iface, NULL, "PRAGMA page_size = %d", page_size);
	}

	tracker_db_interface_execute_query (iface, NULL, "PRAGMA cache_size = %d", cache_size);
	g_message ("  Setting cache size to %d", cache_size);

}


static const gchar *
db_type_to_string (TrackerDB db)
{
	GType       type;
	GEnumClass *enum_class;
	GEnumValue *enum_value;

	type = tracker_db_get_type ();
	enum_class = G_ENUM_CLASS (g_type_class_peek (type));
	enum_value = g_enum_get_value (enum_class, db);

	if (!enum_value) {
		return "unknown";
	}

	return enum_value->value_nick;
}

static TrackerDBInterface *
db_interface_get (TrackerDB  type,
                  gboolean  *create)
{
	TrackerDBInterface *iface;
	const gchar        *path;

	path = dbs[type].abs_filename;

	if (!g_file_test (path, G_FILE_TEST_EXISTS)) {
		*create = TRUE;
	} else {
		*create = FALSE;
	}

	g_message ("%s database... '%s' (%s)",
	           *create ? "Creating" : "Loading",
	           path,
	           db_type_to_string (type));

	iface = tracker_db_interface_sqlite_new (path);

	db_set_params (iface,
	               dbs[type].cache_size,
	               dbs[type].page_size);

	return iface;
}

static TrackerDBInterface *
db_interface_get_fulltext (void)
{
	TrackerDBInterface *iface;
	gboolean            create;

	iface = db_interface_get (TRACKER_DB_FULLTEXT, &create);

	return iface;
}

static TrackerDBInterface *
db_interface_get_contents (void)
{
	TrackerDBInterface *iface;
	gboolean            create;

	iface = db_interface_get (TRACKER_DB_CONTENTS, &create);

	if (create) {
		tracker_db_interface_start_transaction (iface);
		load_sql_file (iface, "sqlite-contents.sql", NULL);
		tracker_db_interface_end_db_transaction (iface);
	}

	return iface;
}



static TrackerDBInterface *
db_interface_get_metadata (void)
{
	TrackerDBInterface *iface;
	gboolean            create;

	iface = db_interface_get (TRACKER_DB_METADATA, &create);

	if (create) {
		tracker_db_interface_start_transaction (iface);

		/* Create tables */
		load_sql_file (iface, "sqlite-tracker.sql", NULL);

		tracker_db_interface_end_db_transaction (iface);
	}

	return iface;
}

static TrackerDBInterface *
db_interface_create (TrackerDB db)
{
	switch (db) {
	case TRACKER_DB_UNKNOWN:
		return NULL;

	case TRACKER_DB_METADATA:
		return db_interface_get_metadata ();

	case TRACKER_DB_FULLTEXT:
		return db_interface_get_fulltext ();

	case TRACKER_DB_CONTENTS:
		return db_interface_get_contents ();

	default:
		g_critical ("This TrackerDB type:%d->'%s' has no interface set up yet!!",
		            db,
		            db_type_to_string (db));
		return NULL;
	}
}

static void
db_manager_remove_all (gboolean rm_journal)
{
	guint i;

	g_message ("Removing all database files");

	/* Remove stamp file */
	tracker_db_manager_set_first_index_done (FALSE);

	/* NOTE: We don't have to be initialized for this so we
	 * calculate the absolute directories here.
	 */
	for (i = 1; i < G_N_ELEMENTS (dbs); i++) {

		g_message ("  Removing database:'%s'",
		           dbs[i].abs_filename);
		g_unlink (dbs[i].abs_filename);
	}

	if (rm_journal) {
		const gchar *opath = tracker_db_journal_get_filename ();

		if (opath) {
			GFile *file;
			gchar *cpath;

			cpath = g_strdup (opath);
			tracker_db_journal_shutdown ();
			g_message ("  Removing journal:'%s'",
					   cpath);
			file = g_file_new_for_path (cpath);
			g_file_delete (file, NULL, NULL);
			g_object_unref (file);
			g_free (cpath);
		}
	}
}

static TrackerDBVersion
db_get_version (void)
{
	TrackerDBVersion  version;
	gchar            *filename;

	filename = g_build_filename (data_dir, TRACKER_DB_VERSION_FILE, NULL);

	if (G_LIKELY (g_file_test (filename, G_FILE_TEST_EXISTS))) {
		gchar *contents;

		/* Check version is correct */
		if (G_LIKELY (g_file_get_contents (filename, &contents, NULL, NULL))) {
			if (contents && strlen (contents) <= 2) {
				version = atoi (contents);
			} else {
				g_message ("  Version file content size is either 0 or bigger than expected");

				version = TRACKER_DB_VERSION_UNKNOWN;
			}

			g_free (contents);
		} else {
			g_message ("  Could not get content of file '%s'", filename);

			version = TRACKER_DB_VERSION_UNKNOWN;
		}
	} else {
		g_message ("  Could not find database version file:'%s'", filename);
		g_message ("  Current databases are either old or no databases are set up yet");

		version = TRACKER_DB_VERSION_UNKNOWN;
	}

	g_free (filename);

	return version;
}

static void
db_set_version (void)
{
	GError *error = NULL;
	gchar  *filename;
	gchar  *str;

	filename = g_build_filename (data_dir, TRACKER_DB_VERSION_FILE, NULL);
	g_message ("  Creating version file '%s'", filename);

	str = g_strdup_printf ("%d", TRACKER_DB_VERSION_NOW);

	if (!g_file_set_contents (filename, str, -1, &error)) {
		g_message ("  Could not set file contents, %s",
		           error ? error->message : "no error given");
		g_clear_error (&error);
	}

	g_free (str);
	g_free (filename);
}

static void
db_manager_analyze (TrackerDB db)
{
	guint64             current_mtime;

	current_mtime = tracker_file_get_mtime (dbs[db].abs_filename);

	if (current_mtime > dbs[db].mtime) {
		g_message ("  Analyzing DB:'%s'", dbs[db].name);
		db_exec_no_reply (dbs[db].iface, "ANALYZE %s.Services", dbs[db].name);

		/* Remember current mtime for future */
		dbs[db].mtime = current_mtime;
	} else {
		g_message ("  Not updating DB:'%s', no changes since last optimize", dbs[db].name);
	}
}

GType
tracker_db_get_type (void)
{
	static GType etype = 0;

	if (etype == 0) {
		static const GEnumValue values[] = {
			{ TRACKER_DB_METADATA,
			  "TRACKER_DB_METADATA",
			  "metadata" },
			{ TRACKER_DB_CONTENTS,
			  "TRACKER_DB_CONTENTS",
			  "contents" },
			{ 0, NULL, NULL }
		};

		etype = g_enum_register_static ("TrackerDB", values);
	}

	return etype;
}

static void
tracker_db_manager_ensure_locale (void)
{
	TrackerDBInterface *common;
	TrackerDBStatement *stmt;
	TrackerDBResultSet *result_set;
	const gchar *current_locale;
	gchar *stored_locale = NULL;

	current_locale = setlocale (LC_COLLATE, NULL);

	common = dbs[TRACKER_DB_METADATA].iface;

	stmt = tracker_db_interface_create_statement (common, NULL, "SELECT OptionValue FROM Options WHERE OptionKey = 'CollationLocale'");

	if (!stmt) {
		return;
	}

	result_set = tracker_db_statement_execute (stmt, NULL);
	g_object_unref (stmt);

	if (result_set) {
		tracker_db_result_set_get (result_set, 0, &stored_locale, -1);
		g_object_unref (result_set);
	}

	if (g_strcmp0 (current_locale, stored_locale) != 0) {
		/* Locales differ, update collate keys */
		g_message ("Updating DB locale dependent data to: %s\n", current_locale);

		stmt = tracker_db_interface_create_statement (common, NULL, "UPDATE Options SET OptionValue = ? WHERE OptionKey = 'CollationLocale'");

		if (stmt) {
			tracker_db_statement_bind_text (stmt, 0, current_locale);
			tracker_db_statement_execute (stmt, NULL);
			g_object_unref (stmt);
		}
	}

	g_free (stored_locale);
}

static void
db_recreate_all (void)
{
	guint i;

	/* We call an internal version of this function here
	 * because at the time 'initialized' = FALSE and that
	 * will cause errors and do nothing.
	 */
	g_message ("Cleaning up database files for reindex");

	db_manager_remove_all (FALSE);

	/* Now create the databases and close them */
	g_message ("Creating database files, this may take a few moments...");

	for (i = 1; i < G_N_ELEMENTS (dbs); i++) {
		dbs[i].iface = db_interface_create (i);
	}

	/* We don't close the dbs in the same loop as before
	 * becase some databases need other databases
	 * attached to be created correctly.
	 */
	for (i = 1; i < G_N_ELEMENTS (dbs); i++) {
		g_object_unref (dbs[i].iface);
		dbs[i].iface = NULL;
	}
}

void
tracker_db_manager_init_locations (void)
{
	const gchar *dir;
	guint i;
	gchar *filename;

	filename = g_strdup_printf ("tracker-%s", g_get_user_name ());
	sys_tmp_dir = g_build_filename (g_get_tmp_dir (), filename, NULL);
	g_free (filename);

	user_data_dir = g_build_filename (g_get_user_data_dir (),
	                                  "tracker",
	                                  "data",
	                                  NULL);

	data_dir = g_build_filename (g_get_user_cache_dir (),
	                             "tracker",
	                             NULL);

	for (i = 1; i < G_N_ELEMENTS (dbs); i++) {
		dir = location_to_directory (dbs[i].location);
		dbs[i].abs_filename = g_build_filename (dir, dbs[i].file, NULL);
	}

	locations_initialized = TRUE;
}

static void
free_thread_interface (gpointer data)
{
	TrackerDBInterface *interface = data;
	GHashTableIter iter;
	gpointer value;

	g_static_mutex_lock (&thread_ifaces_mutex);

	g_hash_table_iter_init (&iter, thread_ifaces);

	while (g_hash_table_iter_next (&iter, NULL, &value)) {
		if (value == data) {
			g_hash_table_iter_remove (&iter);
			break;
		}
	}

	g_static_mutex_unlock (&thread_ifaces_mutex);

	g_object_unref (interface);
}

gboolean
tracker_db_manager_init (TrackerDBManagerFlags  flags,
                         gboolean              *first_time,
                         gboolean               shared_cache)
{
	GType               etype;
	TrackerDBVersion    version;
	gchar              *filename;
	const gchar        *dir;
	const gchar        *env_path;
	gboolean            need_reindex;
	guint               i;
	gchar              *in_use_filename;
	int                 in_use_file;
	gboolean            loaded = FALSE;
	TrackerDBInterface *resources_iface;

	/* First set defaults for return values */
	if (first_time) {
		*first_time = FALSE;
	}

	if (initialized) {
		return TRUE;
	}

	need_reindex = FALSE;

	/* Since we don't reference this enum anywhere, we do
	 * it here to make sure it exists when we call
	 * g_type_class_peek(). This wouldn't be necessary if
	 * it was a param in a GObject for example.
	 *
	 * This does mean that we are leaking by 1 reference
	 * here and should clean it up, but it doesn't grow so
	 * this is acceptable.
	 */
	etype = tracker_db_get_type ();
	db_type_enum_class_pointer = g_type_class_ref (etype);

	/* Set up locations */
	g_message ("Setting database locations");

	old_flags = flags;

	filename = g_strdup_printf ("tracker-%s", g_get_user_name ());
	if (sys_tmp_dir)
		g_free (sys_tmp_dir);
	sys_tmp_dir = g_build_filename (g_get_tmp_dir (), filename, NULL);
	g_free (filename);

	env_path = g_getenv ("TRACKER_DB_SQL_DIR");

	if (G_UNLIKELY (!env_path)) {
		sql_dir = g_build_filename (SHAREDIR,
		                            "tracker",
		                            NULL);
	} else {
		sql_dir = g_strdup (env_path);
	}

	if (user_data_dir)
		g_free (user_data_dir);

	user_data_dir = g_build_filename (g_get_user_data_dir (),
	                                  "tracker",
	                                  "data",
	                                  NULL);

	if (data_dir)
		g_free (data_dir);

	data_dir = g_build_filename (g_get_user_cache_dir (),
	                             "tracker",
	                             NULL);

	/* Make sure the directories exist */
	g_message ("Checking database directories exist");

	g_mkdir_with_parents (data_dir, 00755);
	g_mkdir_with_parents (user_data_dir, 00755);
	g_mkdir_with_parents (sys_tmp_dir, 00755);

	g_message ("Checking database version");

	version = db_get_version ();

	if (version < TRACKER_DB_VERSION_NOW) {
		g_message ("  A reindex will be forced");
		need_reindex = TRUE;
	}

	if (need_reindex) {
		db_set_version ();
	}

	g_message ("Checking database files exist");

	for (i = 1; i < G_N_ELEMENTS (dbs); i++) {
		/* Fill absolute path for the database */
		dir = location_to_directory (dbs[i].location);
		if (dbs[i].abs_filename)
			g_free (dbs[i].abs_filename);
		dbs[i].abs_filename = g_build_filename (dir, dbs[i].file, NULL);

		/* Check we have each database in place, if one is
		 * missing, we reindex.
		 */

		/* No need to check for other files not existing (for
		 * reindex) if one is already missing.
		 */
		if (need_reindex) {
			continue;
		}

		if (!g_file_test (dbs[i].abs_filename, G_FILE_TEST_EXISTS)) {
			g_message ("Could not find database file:'%s'", dbs[i].abs_filename);
			g_message ("One or more database files are missing, a reindex will be forced");
			need_reindex = TRUE;
		}
	}

	locations_initialized = TRUE;

	/* If we are just initializing to remove the databases,
	 * return here.
	 */
	if ((flags & TRACKER_DB_MANAGER_REMOVE_ALL) != 0) {
		initialized = TRUE;
		return TRUE;
	}

	/* Set general database options */
	if (shared_cache) {
		g_message ("Enabling database shared cache");
		tracker_db_interface_sqlite_enable_shared_cache ();
	}

	in_use_filename = g_build_filename (g_get_user_data_dir (),
	                                    "tracker",
	                                    "data",
	                                    IN_USE_FILENAME,
	                                    NULL);

	/* Should we reindex? If so, just remove all databases files,
	 * NOT the paths, note, that these paths are also used for
	 * other things like the nfs lock file.
	 */
	if (flags & TRACKER_DB_MANAGER_FORCE_REINDEX || need_reindex) {
		if (first_time) {
			*first_time = TRUE;
		}

		if (!tracker_file_system_has_enough_space (data_dir, TRACKER_DB_MIN_REQUIRED_SPACE, TRUE)) {
			return FALSE;
		}

		/* Clear the first-index stamp file */
		tracker_db_manager_set_first_index_done (FALSE);

		db_recreate_all ();

		/* Load databases */
		g_message ("Loading databases files...");

	} else {
		gboolean must_recreate;

		/* Load databases */
		g_message ("Loading databases files...");

		must_recreate = !tracker_db_journal_reader_verify_last (NULL);

		if (!must_recreate && g_file_test (in_use_filename, G_FILE_TEST_EXISTS)) {
			gsize size = 0;

			g_message ("Didn't shut down cleanly last time, doing integrity checks");

			for (i = 1; i < G_N_ELEMENTS (dbs) && !must_recreate; i++) {
				TrackerDBCursor *cursor = NULL;
				TrackerDBStatement *stmt;
				struct stat st;

				if (g_stat (dbs[i].abs_filename, &st) == 0) {
					size = st.st_size;
				}

				/* Size is 1 when using echo > file.db, none of our databases
				 * are only one byte in size even initually. */

				if (size <= 1) {
					must_recreate = TRUE;
					continue;
				}

				dbs[i].iface = db_interface_create (i);
				dbs[i].mtime = tracker_file_get_mtime (dbs[i].abs_filename);

				loaded = TRUE;

				stmt = tracker_db_interface_create_statement (dbs[i].iface, NULL,
				                                              "PRAGMA integrity_check(1)");

				if (stmt) {
					cursor = tracker_db_statement_start_cursor (stmt, NULL);
					g_object_unref (stmt);
				}

				if (cursor) {
					if (tracker_db_cursor_iter_next (cursor, NULL)) {
						if (g_strcmp0 (tracker_db_cursor_get_string (cursor, 0), "ok") != 0) {
							must_recreate = TRUE;
						}
					}
					g_object_unref (cursor);
				}
			}
		}

		if (must_recreate) {

			if (first_time) {
				*first_time = TRUE;
			}

			for (i = 1; i < G_N_ELEMENTS (dbs); i++) {
				if (dbs[i].iface) {
					g_object_unref (dbs[i].iface);
					dbs[i].iface = NULL;
				}
			}

			if (!tracker_file_system_has_enough_space (data_dir, TRACKER_DB_MIN_REQUIRED_SPACE, TRUE)) {
				return FALSE;
			}

			db_recreate_all ();
			loaded = FALSE;
		}

	}

	if (!loaded) {
		for (i = 1; i < G_N_ELEMENTS (dbs); i++) {
			dbs[i].iface = db_interface_create (i);
			dbs[i].mtime = tracker_file_get_mtime (dbs[i].abs_filename);
		}
	}

	in_use_file = g_open (in_use_filename,
	                      O_WRONLY | O_APPEND | O_CREAT | O_SYNC,
	                      S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);

        if (in_use_file >= 0) {
                fsync (in_use_file);
                close (in_use_file);
        }

	g_free (in_use_filename);

	tracker_db_manager_ensure_locale ();

	initialized = TRUE;

	thread_ifaces = g_hash_table_new (NULL, NULL);

	if (flags & TRACKER_DB_MANAGER_READONLY) {
		resources_iface = tracker_db_manager_get_db_interfaces_ro (3,
		                                                           TRACKER_DB_METADATA,
		                                                           TRACKER_DB_FULLTEXT,
		                                                           TRACKER_DB_CONTENTS);
	} else {
		resources_iface = tracker_db_manager_get_db_interfaces (3,
		                                                        TRACKER_DB_METADATA,
		                                                        TRACKER_DB_FULLTEXT,
		                                                        TRACKER_DB_CONTENTS);
	}

	g_static_private_set (&interface_data_key, resources_iface, free_thread_interface);

	g_static_mutex_lock (&thread_ifaces_mutex);
	g_hash_table_insert (thread_ifaces, g_thread_self (), resources_iface);
	g_static_mutex_unlock (&thread_ifaces_mutex);

	return TRUE;
}

void
tracker_db_manager_shutdown (void)
{
	guint i;
	gchar *in_use_filename;

	if (!initialized) {
		return;
	}

	for (i = 1; i < G_N_ELEMENTS (dbs); i++) {
		if (dbs[i].abs_filename) {
			g_free (dbs[i].abs_filename);
			dbs[i].abs_filename = NULL;

			if (dbs[i].iface) {
				g_object_unref (dbs[i].iface);
				dbs[i].iface = NULL;
			}
		}
	}

	g_free (data_dir);
	data_dir = NULL;
	g_free (user_data_dir);
	user_data_dir = NULL;
	g_free (sys_tmp_dir);
	sys_tmp_dir = NULL;
	g_free (sql_dir);

	/* shutdown fts in all threads
	   needs to be done before shutting down all db interfaces as
	   shutdown does not happen in thread where interface was created */
	tracker_fts_shutdown_all ();
	/* shutdown db interfaces in all threads */
	g_static_private_free (&interface_data_key);

	if (thread_ifaces) {
		g_hash_table_destroy (thread_ifaces);
		thread_ifaces = NULL;
	}

	/* Since we don't reference this enum anywhere, we do
	 * it here to make sure it exists when we call
	 * g_type_class_peek(). This wouldn't be necessary if
	 * it was a param in a GObject for example.
	 *
	 * This does mean that we are leaking by 1 reference
	 * here and should clean it up, but it doesn't grow so
	 * this is acceptable.
	 */
	g_type_class_unref (db_type_enum_class_pointer);
	db_type_enum_class_pointer = NULL;

	initialized = FALSE;
	locations_initialized = FALSE;

	in_use_filename = g_build_filename (g_get_user_data_dir (),
	                                    "tracker",
	                                    "data",
	                                    IN_USE_FILENAME,
	                                    NULL);

	g_unlink (in_use_filename);

	g_free (in_use_filename);
}

void
tracker_db_manager_remove_all (gboolean rm_journal)
{
	g_return_if_fail (initialized != FALSE);

	db_manager_remove_all (rm_journal);
}


void
tracker_db_manager_move_to_temp (void)
{
	guint i;
	gchar *cpath, *new_filename;

	g_return_if_fail (initialized != FALSE);

	g_message ("Moving all database files");

	for (i = 1; i < G_N_ELEMENTS (dbs); i++) {
		new_filename = g_strdup_printf ("%s.tmp", dbs[i].abs_filename);
		g_message ("  Renaming database:'%s' -> '%s'",
		           dbs[i].abs_filename, new_filename);
		g_rename (dbs[i].abs_filename, new_filename);
		g_free (new_filename);
	}

	cpath = g_strdup (tracker_db_journal_get_filename ());
	new_filename = g_strdup_printf ("%s.tmp", cpath);
	g_message ("  Renaming journal:'%s' -> '%s'",
	           cpath, new_filename);
	g_rename (cpath, new_filename);
	g_free (cpath);
	g_free (new_filename);
}


void
tracker_db_manager_restore_from_temp (void)
{
	guint i;
	gchar *cpath, *new_filename;

	g_return_if_fail (locations_initialized != FALSE);

	g_message ("Moving all database files");

	for (i = 1; i < G_N_ELEMENTS (dbs); i++) {
		new_filename = g_strdup_printf ("%s.tmp", dbs[i].abs_filename);
		g_message ("  Renaming database:'%s' -> '%s'",
		           dbs[i].abs_filename, new_filename);
		g_rename (dbs[i].abs_filename, new_filename);
		g_free (new_filename);
	}

	cpath = g_strdup (tracker_db_journal_get_filename ());
	new_filename = g_strdup_printf ("%s.tmp", cpath);
	g_message ("  Renaming journal:'%s' -> '%s'",
	           cpath, new_filename);
	g_rename (cpath, new_filename);
	g_free (cpath);
	g_free (new_filename);
}

void
tracker_db_manager_remove_temp (void)
{
	guint i;
	gchar *cpath, *new_filename;

	g_return_if_fail (locations_initialized != FALSE);

	g_message ("Removing all temp database files");

	for (i = 1; i < G_N_ELEMENTS (dbs); i++) {
		new_filename = g_strdup_printf ("%s.tmp", dbs[i].abs_filename);
		g_message ("  Removing temp database:'%s'",
		           new_filename);
		g_unlink (new_filename);
		g_free (new_filename);
	}

	cpath = g_strdup (tracker_db_journal_get_filename ());
	new_filename = g_strdup_printf ("%s.tmp", cpath);
	g_message ("  Removing temp journal:'%s'",
	           new_filename);
	g_unlink (new_filename);
	g_free (cpath);
	g_free (new_filename);
}

void
tracker_db_manager_optimize (void)
{
	gboolean dbs_are_open = FALSE;
	guint    i;

	g_return_if_fail (initialized != FALSE);

	g_message ("Optimizing databases...");

	g_message ("  Checking DBs are not open");

	/* Check if any connections are open? */
	for (i = 1; i < G_N_ELEMENTS (dbs); i++) {
		if (G_OBJECT (dbs[i].iface)->ref_count > 1) {
			g_message ("  DB:'%s' is still open with %d references!",
			           dbs[i].name,
			           G_OBJECT (dbs[i].iface)->ref_count);

			dbs_are_open = TRUE;
		}
	}

	if (dbs_are_open) {
		g_message ("  Not optimizing DBs, some are still open with > 1 reference");
		return;
	}

	/* Optimize the metadata database */
	db_manager_analyze (TRACKER_DB_METADATA);
}

const gchar *
tracker_db_manager_get_file (TrackerDB db)
{
	g_return_val_if_fail (initialized != FALSE, NULL);

	return dbs[db].abs_filename;
}

/**
 * tracker_db_manager_get_db_interfaces:
 * @num: amount of TrackerDB files wanted
 * @...: All the files that you want in the connection as TrackerDB items
 *
 * Request a database connection where the first requested file gets connected
 * to and the subsequent requsted files get attached to the connection.
 *
 * The caller must g_object_unref the result when finished using it.
 *
 * returns: (caller-owns): a database connection
 **/
static TrackerDBInterface *
tracker_db_manager_get_db_interfaces (gint num, ...)
{
	gint                n_args;
	va_list                     args;
	TrackerDBInterface *connection = NULL;

	g_return_val_if_fail (initialized != FALSE, NULL);

	va_start (args, num);
	for (n_args = 1; n_args <= num; n_args++) {
		TrackerDB db = va_arg (args, TrackerDB);

		if (!connection) {
			connection = tracker_db_interface_sqlite_new (dbs[db].abs_filename);

			db_set_params (connection,
			               dbs[db].cache_size,
			               dbs[db].page_size);

		} else {
			db_exec_no_reply (connection,
			                  "ATTACH '%s' as '%s'",
			                  dbs[db].abs_filename,
			                  dbs[db].name);
		}

	}
	va_end (args);

	return connection;
}

static TrackerDBInterface *
tracker_db_manager_get_db_interfaces_ro (gint num, ...)
{
	gint                n_args;
	va_list                     args;
	TrackerDBInterface *connection = NULL;

	g_return_val_if_fail (initialized != FALSE, NULL);

	va_start (args, num);
	for (n_args = 1; n_args <= num; n_args++) {
		TrackerDB db = va_arg (args, TrackerDB);

		if (!connection) {
			connection = tracker_db_interface_sqlite_new_ro (dbs[db].abs_filename);
			db_set_params (connection,
			               dbs[db].cache_size,
			               dbs[db].page_size);
		} else {
			db_exec_no_reply (connection,
			                  "ATTACH '%s' as '%s'",
			                  dbs[db].abs_filename,
			                  dbs[db].name);
		}

	}
	va_end (args);

	return connection;
}


/**
 * tracker_db_manager_get_db_interface:
 * @db: the database file wanted
 *
 * Request a database connection to the database file @db.
 *
 * The caller must NOT g_object_unref the result
 *
 * returns: (callee-owns): a database connection
 **/
TrackerDBInterface *
tracker_db_manager_get_db_interface (void)
{
	TrackerDBInterface *interface;

	g_return_val_if_fail (initialized != FALSE, NULL);
	interface = g_static_private_get (&interface_data_key);

	/* Ensure the interface is there */
	if (!interface) {
		interface = tracker_db_manager_get_db_interfaces (3,
			                                          TRACKER_DB_METADATA,
			                                          TRACKER_DB_FULLTEXT,
			                                          TRACKER_DB_CONTENTS);

		tracker_db_interface_sqlite_fts_init (TRACKER_DB_INTERFACE_SQLITE (interface), FALSE);

		g_static_private_set (&interface_data_key, interface, free_thread_interface);

		g_static_mutex_lock (&thread_ifaces_mutex);
		g_hash_table_insert (thread_ifaces, g_thread_self (), interface);
		g_static_mutex_unlock (&thread_ifaces_mutex);
	}

	return interface;
}

/**
 * tracker_db_manager_has_enough_space:
 *
 * Checks whether the file system, where the database files are stored,
 * has enough free space to allow modifications.
 *
 * returns: TRUE if there is enough space, FALSE otherwise
 **/
gboolean
tracker_db_manager_has_enough_space  (void)
{
	return tracker_file_system_has_enough_space (data_dir, TRACKER_DB_MIN_REQUIRED_SPACE, FALSE);
}

/**
 * tracker_db_manager_interrupt_thread:
 * @thread: a #GThread to be interrupted
 *
 * Interrupts any ongoing DB operation going on on @thread.
 *
 * Returns: %TRUE if DB operations were interrupted, %FALSE otherwise.
 **/
gboolean
tracker_db_manager_interrupt_thread (GThread *thread)
{
	TrackerDBInterface *interface;

	g_static_mutex_lock (&thread_ifaces_mutex);
	interface = g_hash_table_lookup (thread_ifaces, thread);
	g_static_mutex_unlock (&thread_ifaces_mutex);

	if (!interface) {
		return FALSE;
	}

	return tracker_db_interface_interrupt (interface);
}

static gchar *
get_first_index_stamp_path (void)
{
	return g_build_filename (g_get_user_cache_dir (),
	                         "tracker",
	                         FIRST_INDEX_STAMP_FILENAME,
	                         NULL);
}

/**
 * tracker_db_manager_get_first_index_done:
 *
 * Check if first full index of files was already done.
 *
 * Returns: %TRUE if a first full index have been done, %FALSE otherwise.
 **/
gboolean
tracker_db_manager_get_first_index_done (void)
{
	gboolean exists;
	gchar *stamp;

	stamp = get_first_index_stamp_path();
	exists = g_file_test (stamp, G_FILE_TEST_EXISTS);
	g_free (stamp);

	return exists;
}

/**
 * tracker_db_manager_set_first_index_done:
 *
 * Set the status of the first full index of files. Should be set to
 *  %FALSE if the index was never done or if a reindex is needed. When
 *  the index is completed, should be set to %TRUE.
 **/
void
tracker_db_manager_set_first_index_done (gboolean done)
{
	gboolean already_exists;
	gchar *stamp;

	stamp = get_first_index_stamp_path ();

	already_exists = g_file_test (stamp, G_FILE_TEST_EXISTS);

	if (done && !already_exists) {
		GError *error = NULL;

		/* If done, create stamp file if not already there */
		if (!g_file_set_contents (stamp, "", -1, &error)) {
			g_warning ("  Creating first-index stamp in "
			           "'%s' failed: '%s'",
			           stamp,
			           error->message);
			g_error_free (error);
		} else {
			g_message ("  First-index stamp created in '%s'",
			           stamp);
		}
	} else if (!done && already_exists) {
		/* If NOT done, remove stamp file */
		if (g_remove (stamp)) {
			g_warning ("  Removing first-index stamp from '%s' "
			           "failed: '%s'",
			           stamp,
			           g_strerror (errno));
		} else {
			g_message ("  First-index stamp removed from '%s'",
			           stamp);
		}
	}

	g_free (stamp);
}
