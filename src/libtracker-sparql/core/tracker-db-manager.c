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

#include <fcntl.h>

#include <glib/gstdio.h>

#include <libtracker-common/tracker-common.h>
#include <libtracker-common/tracker-parser.h>

#include "tracker-db-manager.h"
#include "tracker-db-interface-sqlite.h"
#include "tracker-db-interface.h"
#include "tracker-data-manager.h"
#include "tracker-uuid.h"

#define UNKNOWN_STATUS 0.5

#define MAX_INTERFACES_PER_CPU        16
#define MAX_INTERFACES                (MAX_INTERFACES_PER_CPU * g_get_num_processors ())

/* Required minimum space needed to create databases (5Mb) */
#define TRACKER_DB_MIN_REQUIRED_SPACE 5242880

#define TRACKER_VACUUM_CHECK_SIZE     ((goffset) 4 * 1024 * 1024 * 1024) /* 4GB */

#define IN_USE_FILENAME               ".meta.isrunning"

#define TOSTRING1(x) #x
#define TOSTRING(x) TOSTRING1(x)
#define TRACKER_PARSER_VERSION_STRING TOSTRING(TRACKER_PARSER_VERSION)

#define FTS_FLAGS (TRACKER_DB_MANAGER_FTS_ENABLE_STEMMER |	  \
                   TRACKER_DB_MANAGER_FTS_ENABLE_UNACCENT |	  \
                   TRACKER_DB_MANAGER_FTS_ENABLE_STOP_WORDS |	  \
                   TRACKER_DB_MANAGER_FTS_IGNORE_NUMBERS)

typedef struct {
	TrackerDBInterface *iface;
	const gchar        *file;
	const gchar        *name;
	gchar              *abs_filename;
	gint                cache_size;
	gint                page_size;
} TrackerDBDefinition;

static TrackerDBDefinition db_base = {
	NULL,
	"meta.db",
	"meta",
	NULL,
	TRACKER_DB_CACHE_SIZE_DEFAULT,
	8192,
};

struct _TrackerDBManager {
	GObject parent_instance;
	TrackerDBDefinition db;
	gboolean locations_initialized;
	gchar *data_dir;
	gchar *user_data_dir;
	gchar *in_use_filename;
	GFile *cache_location;
	gchar *shared_cache_key;
	TrackerDBManagerFlags flags;
	guint s_cache_size;
	guint u_cache_size;
	gboolean first_time;
	TrackerDBVersion db_version;

	gpointer vtab_data;

	GWeakRef iface_data;

	GAsyncQueue *interfaces;
};

enum {
	SETUP_INTERFACE,
	UPDATE_INTERFACE,
	N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

G_DEFINE_TYPE (TrackerDBManager, tracker_db_manager, G_TYPE_OBJECT)

static TrackerDBInterface *tracker_db_manager_create_db_interface   (TrackerDBManager    *db_manager,
                                                                     gboolean             readonly,
                                                                     GError             **error);

static TrackerDBInterface * init_writable_db_interface              (TrackerDBManager *db_manager);

gboolean
tracker_db_manager_is_first_time (TrackerDBManager *db_manager)
{
	return db_manager->first_time;
}

TrackerDBManagerFlags
tracker_db_manager_get_flags (TrackerDBManager *db_manager,
                              guint            *select_cache_size,
                              guint            *update_cache_size)
{
	if (select_cache_size)
		*select_cache_size = db_manager->s_cache_size;

	if (update_cache_size)
		*update_cache_size = db_manager->u_cache_size;

	return db_manager->flags;
}

static void
iface_set_params (TrackerDBInterface   *iface,
                  gboolean              readonly,
                  GError              **error)
{
	tracker_db_interface_execute_query (iface, NULL, "PRAGMA encoding = \"UTF-8\"");

	if (readonly) {
		tracker_db_interface_execute_query (iface, NULL, "PRAGMA temp_store = MEMORY;");
	} else {
		tracker_db_interface_execute_query (iface, NULL, "PRAGMA temp_store = FILE;");
	}
}

static void
db_set_params (TrackerDBInterface   *iface,
               const gchar          *database,
               gint                  cache_size,
               gint                  page_size,
               gboolean              enable_wal,
               GError              **error)
{
	GError *internal_error = NULL;
	TrackerDBStatement *stmt;

	TRACKER_NOTE (SQLITE, g_message ("  Setting page size to %d", page_size));
	tracker_db_interface_execute_query (iface, NULL, "PRAGMA \"%s\".page_size = %d", database, page_size);

	tracker_db_interface_execute_query (iface, NULL, "PRAGMA \"%s\".synchronous = NORMAL", database);
	tracker_db_interface_execute_query (iface, NULL, "PRAGMA \"%s\".auto_vacuum = 0", database);

	if (enable_wal) {
		stmt = tracker_db_interface_create_vstatement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE,
		                                               &internal_error,
		                                               "PRAGMA \"%s\".journal_mode = WAL", database);

		if (internal_error) {
			g_debug ("Can't set journal mode to WAL: '%s'",
			         internal_error->message);
			g_propagate_error (error, internal_error);
		} else {
			TrackerDBCursor *cursor;

			cursor = tracker_db_statement_start_cursor (stmt, NULL);
			if (tracker_db_cursor_iter_next (cursor, NULL, NULL)) {
				if (g_ascii_strcasecmp (tracker_db_cursor_get_string (cursor, 0, NULL), "WAL") != 0) {
					g_set_error (error,
					             TRACKER_DB_INTERFACE_ERROR,
					             TRACKER_DB_OPEN_ERROR,
					             "Can't set journal mode to WAL");
				}
			}
			g_object_unref (cursor);
		}

		g_clear_object (&stmt);
	}

	tracker_db_interface_execute_query (iface, NULL, "PRAGMA \"%s\".journal_size_limit = 10240000", database);

	tracker_db_interface_execute_query (iface, NULL, "PRAGMA \"%s\".cache_size = %d", database, cache_size);
	TRACKER_NOTE (SQLITE, g_message ("  Setting cache size to %d", cache_size));
}

static gboolean
tracker_db_manager_get_metadata (TrackerDBManager   *db_manager,
                                 const gchar        *key,
                                 GValue             *value)
{
	TrackerDBInterface *iface;
	TrackerDBStatement *stmt;
	TrackerDBCursor *cursor;

	iface = tracker_db_manager_get_writable_db_interface (db_manager);
	stmt = tracker_db_interface_create_statement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE,
	                                              NULL,
	                                              "SELECT value FROM metadata WHERE key = ?");
	if (!stmt)
		return FALSE;

	tracker_db_statement_bind_text (stmt, 0, key);
	cursor = tracker_db_statement_start_cursor (stmt, NULL);
	g_object_unref (stmt);

	if (!cursor || !tracker_db_cursor_iter_next (cursor, NULL, NULL)) {
		g_clear_object (&cursor);
		return FALSE;
	}

	tracker_db_cursor_get_value (cursor, 0, value);
	g_object_unref (cursor);

	return G_VALUE_TYPE (value) != G_TYPE_INVALID;
}

static void
tracker_db_manager_set_metadata (TrackerDBManager   *db_manager,
				 const gchar        *key,
				 GValue             *value)
{
	TrackerDBInterface *iface;
	TrackerDBStatement *stmt;
	GError *error = NULL;

	iface = tracker_db_manager_get_writable_db_interface (db_manager);
	stmt = tracker_db_interface_create_statement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE,
	                                              &error,
	                                              "INSERT OR REPLACE INTO metadata VALUES (?, ?)");
	if (stmt) {
		tracker_db_statement_bind_text (stmt, 0, key);
		tracker_db_statement_bind_value (stmt, 1, value);
		tracker_db_statement_execute (stmt, &error);

		g_object_unref (stmt);
	}

	if (error) {
		g_critical ("Could not store database metadata: %s\n", error->message);
		g_error_free (error);
	}
}

static TrackerDBVersion
db_get_version (TrackerDBManager *db_manager)
{
	TrackerDBInterface *iface;
	TrackerDBStatement *stmt;
	TrackerDBCursor *cursor;
	TrackerDBVersion version;

	iface = tracker_db_manager_get_writable_db_interface (db_manager);
	stmt = tracker_db_interface_create_statement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE,
	                                              NULL, "PRAGMA user_version");
	if (!stmt)
		return TRACKER_DB_VERSION_UNKNOWN;

	cursor = tracker_db_statement_start_cursor (stmt, NULL);
	g_object_unref (stmt);

	if (!cursor || !tracker_db_cursor_iter_next (cursor, NULL, NULL)) {
		g_clear_object (&cursor);
		return TRACKER_DB_VERSION_UNKNOWN;
	}

	version = tracker_db_cursor_get_int (cursor, 0);
	g_object_unref (cursor);

	return version;
}

void
tracker_db_manager_update_version (TrackerDBManager *db_manager)
{
	TrackerDBInterface *iface;
	TrackerDBStatement *stmt;
	GError *error = NULL;

	iface = tracker_db_manager_get_writable_db_interface (db_manager);
	stmt = tracker_db_interface_create_vstatement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE,
	                                               &error, "PRAGMA user_version = %d",
	                                               TRACKER_DB_VERSION_NOW);
	if (stmt) {
		tracker_db_statement_execute (stmt, &error);
		g_object_unref (stmt);
	}

	if (error) {
		g_critical ("Could not set database version: %s\n", error->message);
		g_error_free (error);
	}
}

static gchar *
db_get_locale (TrackerDBManager *db_manager)
{
	GValue value = G_VALUE_INIT;
	gchar *locale = NULL;

	if (!tracker_db_manager_get_metadata (db_manager, "locale", &value))
		return NULL;

	locale = g_value_dup_string (&value);
	g_value_unset (&value);

	return locale;
}

static void
db_set_locale (TrackerDBManager *db_manager,
	       const gchar      *locale)
{
	GValue value = G_VALUE_INIT;

	g_value_init (&value, G_TYPE_STRING);
	g_value_set_string (&value, locale);

	tracker_db_manager_set_metadata (db_manager, "locale", &value);
	g_value_unset (&value);
}

gboolean
tracker_db_manager_locale_changed (TrackerDBManager  *db_manager,
                                   GError           **error)
{
	gchar *db_locale;
	gchar *current_locale;
	gboolean changed;

	/* Get current collation locale */
	current_locale = tracker_locale_get (TRACKER_LOCALE_COLLATE);

	/* Get db locale */
	db_locale = db_get_locale (db_manager);

	/* If they are different, recreate indexes. Note that having
	 * both to NULL is actually valid, they would default to
	 * the unicode collation without locale-specific stuff. */
	if (g_strcmp0 (db_locale, current_locale) != 0) {
		g_set_error (error,
		             TRACKER_DB_INTERFACE_ERROR,
		             TRACKER_DB_OPEN_ERROR,
		             "Locale change detected (DB:%s, User/App:%s)",
		             db_locale ? db_locale : "unknown",
		             current_locale);
		changed = TRUE;
	} else {
		g_debug ("Current and DB locales match: '%s'", db_locale);
		changed = FALSE;
	}

	g_free (db_locale);
	g_free (current_locale);

	return changed;
}

void
tracker_db_manager_set_current_locale (TrackerDBManager *db_manager)
{
	gchar *current_locale;

	/* Get current collation locale */
	current_locale = tracker_locale_get (TRACKER_LOCALE_COLLATE);
	g_debug ("Saving DB locale as: '%s'", current_locale);
	db_set_locale (db_manager, current_locale);
	g_free (current_locale);
}

static void
tracker_db_manager_ensure_location (TrackerDBManager *db_manager,
				    GFile            *cache_location)
{
	gchar *dir;

	if ((db_manager->flags & TRACKER_DB_MANAGER_IN_MEMORY) != 0) {
		return;
	}

	if (db_manager->locations_initialized) {
		return;
	}

	db_manager->locations_initialized = TRUE;
	db_manager->data_dir = g_file_get_path (cache_location);

	db_manager->db = db_base;

	dir = g_file_get_path (cache_location);
	db_manager->db.abs_filename = g_build_filename (dir, db_manager->db.file, NULL);
	g_free (dir);
}

gboolean
tracker_db_manager_db_exists (GFile *cache_location)
{
	gchar *dir;
	gchar *filename;
	gboolean db_exists = FALSE;

	dir = g_file_get_path (cache_location);
	filename = g_build_filename (dir, db_base.file, NULL);

	db_exists = g_file_test (filename, G_FILE_TEST_EXISTS);

	g_free (dir);
	g_free (filename);

	return db_exists;
}

void
tracker_db_manager_rollback_db_creation (TrackerDBManager *db_manager)
{
	gchar *dir;
	gchar *filename;

	g_return_if_fail (db_manager->first_time);

	if ((db_manager->flags & TRACKER_DB_MANAGER_IN_MEMORY) != 0)
		return;

	dir = g_file_get_path (db_manager->cache_location);
	filename = g_build_filename (dir, db_base.file, NULL);

	if (g_unlink (filename) < 0) {
		g_warning ("Could not delete database file '%s': %m",
		           db_base.file);
	}

	g_free (dir);
	g_free (filename);
}

static gboolean
db_check_integrity (TrackerDBManager *db_manager)
{
	GError *internal_error = NULL;
	TrackerDBStatement *stmt;
	TrackerDBCursor *cursor = NULL;

	stmt = tracker_db_interface_create_statement (db_manager->db.iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE,
	                                              &internal_error,
	                                              "PRAGMA integrity_check(1)");

	if (!stmt) {
		if (internal_error) {
			g_message ("Corrupt database: failed to create integrity_check statement: %s", internal_error->message);
		}

		g_clear_error (&internal_error);
		return FALSE;
	}

	cursor = tracker_db_statement_start_cursor (stmt, NULL);
	g_object_unref (stmt);

	if (cursor) {
		if (tracker_db_cursor_iter_next (cursor, NULL, NULL)) {
			const gchar *check_result;

			check_result = tracker_db_cursor_get_string (cursor, 0, NULL);
			if (g_strcmp0 (check_result, "ok") != 0) {
				g_message ("Corrupt database: sqlite integrity check returned '%s'", check_result);
				return FALSE;
			}
		}
		g_object_unref (cursor);
	}

	/* ensure that database has been initialized by an earlier tracker-store start
	   by checking whether Resource table exists */
	stmt = tracker_db_interface_create_statement (db_manager->db.iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE,
	                                              &internal_error,
	                                              "SELECT 1 FROM Resource");
	if (!stmt) {
		if (internal_error) {
			g_message ("Corrupt database: failed to create resource check statement: %s", internal_error->message);
		}

		g_clear_error (&internal_error);
		return FALSE;
	}

	g_object_unref (stmt);

	return TRUE;
}

TrackerDBManager *
tracker_db_manager_new (TrackerDBManagerFlags   flags,
                        GFile                  *cache_location,
                        gboolean                shared_cache,
                        guint                   select_cache_size,
                        guint                   update_cache_size,
                        TrackerBusyCallback     busy_callback,
                        gpointer                busy_user_data,
                        GObject                *iface_data,
                        gpointer                vtab_data,
                        GError                **error)
{
	TrackerDBManager *db_manager;
	int in_use_file;
	TrackerDBInterface *resources_iface;
	GError *internal_error = NULL;
	gboolean need_to_create = FALSE;

	db_manager = g_object_new (TRACKER_TYPE_DB_MANAGER, NULL);
	db_manager->vtab_data = vtab_data;

	/* Set default value for first_time */
	db_manager->first_time = FALSE;

	/* Set up locations */
	db_manager->flags = flags;
	db_manager->s_cache_size = select_cache_size;
	db_manager->u_cache_size = update_cache_size;
	db_manager->interfaces = g_async_queue_new_full (g_object_unref);

	g_set_object (&db_manager->cache_location, cache_location);
	g_weak_ref_init (&db_manager->iface_data, iface_data);

	if ((db_manager->flags & TRACKER_DB_MANAGER_IN_MEMORY) == 0) {
		tracker_db_manager_ensure_location (db_manager, cache_location);
		db_manager->in_use_filename = g_build_filename (db_manager->data_dir,
		                                                IN_USE_FILENAME,
		                                                NULL);

		/* Don't do need_reindex checks for readonly (direct-access) */
		if ((flags & TRACKER_DB_MANAGER_READONLY) == 0) {

			/* Make sure the directories exist */
			if (g_mkdir_with_parents (db_manager->data_dir, 00755) < 0) {
				g_set_error (error,
				             TRACKER_DB_INTERFACE_ERROR,
				             TRACKER_DB_OPEN_ERROR,
				             "Could not create database directory");
				g_object_unref (db_manager);
				return NULL;
			}
		}
	} else {
		db_manager->shared_cache_key = tracker_generate_uuid (NULL);
	}

	if ((db_manager->flags & TRACKER_DB_MANAGER_IN_MEMORY) != 0) {
		need_to_create = TRUE;
	} else if (!g_file_test (db_manager->db.abs_filename, G_FILE_TEST_EXISTS)) {
		if ((flags & TRACKER_DB_MANAGER_READONLY) == 0) {
			TRACKER_NOTE (SQLITE, g_message ("Could not find database file:'%s', will create it.", db_manager->db.abs_filename));
			need_to_create = TRUE;
		} else {
			g_set_error (error,
			             TRACKER_DB_INTERFACE_ERROR,
			             TRACKER_DB_OPEN_ERROR,
			             "Could not find database file:'%s'.", db_manager->db.abs_filename);

			g_object_unref (db_manager);
			return NULL;
		}
	} else if ((flags & TRACKER_DB_MANAGER_SKIP_VERSION_CHECK) == 0) {
		db_manager->db_version = db_get_version (db_manager);

		if (db_manager->db_version < TRACKER_DB_VERSION_3_0 ||
		    ((flags & TRACKER_DB_MANAGER_READONLY) != 0 &&
		     db_manager->db_version < TRACKER_DB_VERSION_NOW)) {
			g_set_error (error,
			             TRACKER_DB_INTERFACE_ERROR,
			             TRACKER_DB_OPEN_ERROR,
			             "Database version is too old: got version %i, but %i is needed",
			             db_manager->db_version, TRACKER_DB_VERSION_NOW);

			g_object_unref (db_manager);
			return NULL;
		} else if (db_manager->db_version > TRACKER_DB_VERSION_NOW) {
			g_set_error (error,
			             TRACKER_DB_INTERFACE_ERROR,
			             TRACKER_DB_OPEN_ERROR,
			             "Database version is too new: got version %i, but %i is needed",
			             db_manager->db_version, TRACKER_DB_VERSION_NOW);

			g_object_unref (db_manager);
			return NULL;
		}
	}

	db_manager->locations_initialized = TRUE;

	if ((flags & TRACKER_DB_MANAGER_READONLY) != 0) {
		GValue value = G_VALUE_INIT;
		TrackerDBManagerFlags fts_flags = 0;

		if (tracker_db_manager_get_metadata (db_manager, "fts-flags", &value)) {
			fts_flags = g_ascii_strtoll (g_value_get_string (&value), NULL, 10);
			g_value_unset (&value);
		}

		/* Readonly connections should go with the FTS flags as stored
		 * in metadata.
		 */
		db_manager->flags = (db_manager->flags & ~(FTS_FLAGS)) | fts_flags;
	}

	/* Set general database options */
	if (shared_cache) {
		TRACKER_NOTE (SQLITE, g_message ("Enabling database shared cache"));
		tracker_db_interface_sqlite_enable_shared_cache ();
	}

	if (need_to_create) {
		db_manager->first_time = TRUE;

		if ((db_manager->flags & TRACKER_DB_MANAGER_IN_MEMORY) == 0 &&
		     !tracker_file_system_has_enough_space (db_manager->data_dir, TRACKER_DB_MIN_REQUIRED_SPACE, TRUE)) {
			g_set_error (error,
			             TRACKER_DB_INTERFACE_ERROR,
			             TRACKER_DB_OPEN_ERROR,
			             "Filesystem does not have enough space");
			return FALSE;
		}

		TRACKER_NOTE (SQLITE, g_message ("Creating database files for %s...", db_manager->db.abs_filename));

		db_manager->db.iface = tracker_db_manager_create_db_interface (db_manager, FALSE, &internal_error);
		if (internal_error) {
			g_propagate_error (error, internal_error);
			g_object_unref (db_manager);
			return FALSE;
		}

		g_clear_object (&db_manager->db.iface);
	} else {
		TRACKER_NOTE (SQLITE, g_message ("Loading files for database %s...", db_manager->db.abs_filename));

		if ((flags & TRACKER_DB_MANAGER_READONLY) == 0) {
			/* Check that the database was closed cleanly and do a deeper integrity
			 * check if it wasn't, raising an error if we detect corruption. */

			if (g_file_test (db_manager->in_use_filename, G_FILE_TEST_EXISTS)) {
				gsize size = 0;
				struct stat st;

				TRACKER_NOTE (SQLITE, g_message ("Didn't shut down cleanly last time, doing integrity checks"));

				if (g_stat (db_manager->db.abs_filename, &st) == 0) {
					size = st.st_size;
				}

				/* Size is 1 when using echo > file.db, none of our databases
				 * are only one byte in size even initually. */
				if (size <= 1) {
					g_debug ("Database is corrupt: size is 1 byte or less.");
					return FALSE;
				}

				db_manager->db.iface = tracker_db_manager_create_db_interface (db_manager, FALSE, &internal_error);

				if (internal_error) {
					/* If this already doesn't succeed, then surely the file is
					 * corrupt. No need to check for integrity anymore. */
					g_propagate_error (error, internal_error);
					g_object_unref (db_manager);
					return NULL;
				}

				busy_callback ("Integrity checking", 0, busy_user_data);

				if (db_check_integrity (db_manager) == FALSE) {
					g_set_error (error,
					             TRACKER_DB_INTERFACE_ERROR,
					             TRACKER_DB_OPEN_ERROR,
					             "Corrupt db file");
					g_object_unref (db_manager);
					return NULL;
				}
			}
		}
	}

	if ((flags & (TRACKER_DB_MANAGER_READONLY | TRACKER_DB_MANAGER_IN_MEMORY)) == 0) {
		/* do not create in-use file for read-only mode (direct access) */
		in_use_file = g_open (db_manager->in_use_filename,
			              O_WRONLY | O_APPEND | O_CREAT | O_SYNC,
			              S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);

		if (in_use_file >= 0) {
		        fsync (in_use_file);
		        close (in_use_file);
		}
	}

	resources_iface = tracker_db_manager_create_db_interface (db_manager,
	                                                          TRUE, &internal_error);

	if (internal_error) {
		g_propagate_error (error, internal_error);
		g_object_unref (db_manager);
		return NULL;
	}

	g_clear_object (&resources_iface);

	return db_manager;
}

void
tracker_db_manager_finalize (GObject *object)
{
	TrackerDBManager *db_manager = TRACKER_DB_MANAGER (object);
	gboolean readonly = (db_manager->flags & TRACKER_DB_MANAGER_READONLY) != 0;

	g_async_queue_unref (db_manager->interfaces);
	g_free (db_manager->db.abs_filename);

	if (db_manager->db.iface) {
		if (!readonly)
			tracker_db_interface_sqlite_wal_checkpoint (db_manager->db.iface, TRUE, NULL);
		g_object_unref (db_manager->db.iface);
	}

	g_weak_ref_clear (&db_manager->iface_data);

	g_free (db_manager->data_dir);

	if (db_manager->in_use_filename && !readonly) {
		/* do not delete in-use file for read-only mode (direct access) */
		if (g_unlink (db_manager->in_use_filename) < 0)
			g_warning ("Could not delete '" IN_USE_FILENAME "': %m");
	}

	g_free (db_manager->in_use_filename);
	g_free (db_manager->shared_cache_key);
	g_clear_object (&db_manager->cache_location);

	G_OBJECT_CLASS (tracker_db_manager_parent_class)->finalize (object);
}

static TrackerDBInterface *
tracker_db_manager_create_db_interface (TrackerDBManager  *db_manager,
                                        gboolean           readonly,
                                        GError           **error)
{
	TrackerDBInterface *connection;
	GError *internal_error = NULL;
	TrackerDBInterfaceFlags flags = 0;

	if (readonly)
		flags |= TRACKER_DB_INTERFACE_READONLY;
	if (db_manager->flags & TRACKER_DB_MANAGER_ENABLE_MUTEXES)
		flags |= TRACKER_DB_INTERFACE_USE_MUTEX;
	if (db_manager->flags & TRACKER_DB_MANAGER_IN_MEMORY)
		flags |= TRACKER_DB_INTERFACE_IN_MEMORY;

	connection = tracker_db_interface_sqlite_new (db_manager->db.abs_filename,
	                                              db_manager->shared_cache_key,
	                                              flags,
	                                              &internal_error);
	if (internal_error) {
		g_propagate_error (error, internal_error);
		return NULL;
	}

	tracker_db_interface_set_user_data (connection,
	                                    g_weak_ref_get (&db_manager->iface_data),
	                                    g_object_unref);

	if (db_manager->vtab_data)
		tracker_db_interface_init_vtabs (connection, db_manager->vtab_data);

	iface_set_params (connection,
	                  readonly,
	                  &internal_error);
	db_set_params (connection, "main",
	               db_manager->db.cache_size,
	               db_manager->db.page_size,
	               !(db_manager->flags & TRACKER_DB_MANAGER_IN_MEMORY),
	               &internal_error);

	if (internal_error) {
		g_propagate_error (error, internal_error);
		g_object_unref (connection);
		return NULL;
	}

	tracker_db_interface_set_max_stmt_cache_size (connection,
	                                              TRACKER_DB_STATEMENT_CACHE_TYPE_SELECT,
	                                              db_manager->s_cache_size);

	if (!readonly) {
		tracker_db_interface_set_max_stmt_cache_size (connection,
		                                              TRACKER_DB_STATEMENT_CACHE_TYPE_UPDATE,
		                                              db_manager->u_cache_size);
	}

	return connection;
}

/**
 * tracker_db_manager_get_db_interface:
 *
 * Request a database connection to the database
 *
 * The caller must NOT g_object_unref the result
 *
 * returns: (callee-owns): a database connection
 **/
TrackerDBInterface *
tracker_db_manager_get_db_interface (TrackerDBManager  *db_manager,
                                     GError           **error)
{
	GError *internal_error = NULL;
	TrackerDBInterface *interface = NULL;
	guint len, i;

	/* The interfaces never actually leave the async queue,
	 * we use it as a thread synchronized LRU, which doesn't
	 * mean the interface found has no other active cursors,
	 * in which case we either optionally create a new
	 * TrackerDBInterface, or resign to sharing the obtained
	 * one with other threads (thus getting increased contention
	 * in the interface lock).
	 */
	g_async_queue_lock (db_manager->interfaces);
	len = g_async_queue_length_unlocked (db_manager->interfaces);

	/* 1st. Find a free interface */
	for (i = 0; i < len; i++) {
		interface = g_async_queue_try_pop_unlocked (db_manager->interfaces);

		if (!interface)
			break;
		if (!tracker_db_interface_get_is_used (interface))
			break;

		g_async_queue_push_unlocked (db_manager->interfaces, interface);
		interface = NULL;
	}

	/* 2nd. If no more interfaces can be created, pick one */
	if (!interface && len >= MAX_INTERFACES) {
		interface = g_async_queue_try_pop_unlocked (db_manager->interfaces);
	}

	if (interface) {
		g_signal_emit (db_manager, signals[UPDATE_INTERFACE], 0, interface);
	} else {
		/* 3rd. Create a new interface to satisfy the request */
		interface = tracker_db_manager_create_db_interface (db_manager,
		                                                    TRUE, &internal_error);

		if (interface) {
			g_signal_emit (db_manager, signals[SETUP_INTERFACE], 0, interface);
		} else {
			if (g_async_queue_length_unlocked (db_manager->interfaces) == 0) {
				g_propagate_prefixed_error (error, internal_error, "Error opening database: ");
				g_async_queue_unlock (db_manager->interfaces);
				return NULL;
			} else {
				g_error_free (internal_error);
				/* Fetch the first interface back. Oh well */
				interface = g_async_queue_try_pop_unlocked (db_manager->interfaces);
			}
		}
	}

	tracker_db_interface_ref_use (interface);

	g_async_queue_push_unlocked (db_manager->interfaces, interface);
	g_async_queue_unlock (db_manager->interfaces);

	return interface;
}

static void
tracker_db_manager_init (TrackerDBManager *manager)
{
}

static void
tracker_db_manager_class_init (TrackerDBManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_db_manager_finalize;

	signals[SETUP_INTERFACE] =
               g_signal_new ("setup-interface",
                             G_TYPE_FROM_CLASS (klass),
                             G_SIGNAL_RUN_LAST, 0,
                             NULL, NULL,
                             g_cclosure_marshal_VOID__OBJECT,
                             G_TYPE_NONE,
                             1, TRACKER_TYPE_DB_INTERFACE);
	signals[UPDATE_INTERFACE] =
               g_signal_new ("update-interface",
                             G_TYPE_FROM_CLASS (klass),
                             G_SIGNAL_RUN_LAST, 0,
                             NULL, NULL,
                             g_cclosure_marshal_VOID__OBJECT,
                             G_TYPE_NONE,
                             1, TRACKER_TYPE_DB_INTERFACE);
}

static TrackerDBInterface *
init_writable_db_interface (TrackerDBManager *db_manager)
{
	TrackerDBInterface *iface;
	GError *error = NULL;
	gboolean readonly;

	/* Honor anyway the DBManager readonly flag */
	readonly = (db_manager->flags & TRACKER_DB_MANAGER_READONLY) != 0;
	iface = tracker_db_manager_create_db_interface (db_manager, readonly, &error);
	if (error) {
		g_critical ("Error opening readwrite database: %s", error->message);
		g_error_free (error);
	}

	return iface;
}

TrackerDBInterface *
tracker_db_manager_get_writable_db_interface (TrackerDBManager *db_manager)
{
	if (db_manager->db.iface == NULL)
		db_manager->db.iface = init_writable_db_interface (db_manager);

	return db_manager->db.iface;
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
tracker_db_manager_has_enough_space (TrackerDBManager *db_manager)
{
	if ((db_manager->flags & TRACKER_DB_MANAGER_IN_MEMORY) != 0)
		return TRUE;
	return tracker_file_system_has_enough_space (db_manager->data_dir, TRACKER_DB_MIN_REQUIRED_SPACE, FALSE);
}

gboolean
tracker_db_manager_get_tokenizer_changed (TrackerDBManager *db_manager)
{
	GValue value = G_VALUE_INIT;
	const gchar *version;
	TrackerDBManagerFlags flags;
	gboolean changed;

	if (!tracker_db_manager_get_metadata (db_manager, "fts-flags", &value))
		return TRUE;

	flags = g_ascii_strtoll (g_value_get_string (&value), NULL, 10);
	g_value_unset (&value);

	if ((db_manager->flags & TRACKER_DB_MANAGER_READONLY) == 0 &&
	    flags != (db_manager->flags & FTS_FLAGS)) {
		return TRUE;
	}

	if (!tracker_db_manager_get_metadata (db_manager, "parser-version", &value))
		return TRUE;

	version = g_value_get_string (&value);
	changed = strcmp (version, TRACKER_PARSER_VERSION_STRING) != 0;
	g_value_unset (&value);

	return changed;
}

void
tracker_db_manager_tokenizer_update (TrackerDBManager *db_manager)
{
	GValue value = G_VALUE_INIT;

	g_value_init (&value, G_TYPE_STRING);
	g_value_set_string (&value, TRACKER_PARSER_VERSION_STRING);
	tracker_db_manager_set_metadata (db_manager, "parser-version", &value);
	g_value_unset (&value);

	g_value_init (&value, G_TYPE_INT64);
	g_value_set_int64 (&value, (db_manager->flags & FTS_FLAGS));
	tracker_db_manager_set_metadata (db_manager, "fts-flags", &value);
	g_value_unset (&value);
}

void
tracker_db_manager_check_perform_vacuum (TrackerDBManager *db_manager)
{
	TrackerDBInterface *iface;

	if ((db_manager->flags & TRACKER_DB_MANAGER_IN_MEMORY) != 0)
		return;
	if (tracker_file_get_size (db_manager->db.abs_filename) < TRACKER_VACUUM_CHECK_SIZE)
		return;

	iface = tracker_db_manager_get_writable_db_interface (db_manager);
	tracker_db_interface_execute_query (iface, NULL, "VACUUM");
}

static gboolean
ensure_create_database_file (TrackerDBManager  *db_manager,
                             GFile             *file,
                             GError           **error)
{
	TrackerDBInterface *iface;
	GError *file_error = NULL;
	gchar *path;

	/* Ensure the database is created from scratch */
	if (!g_file_delete (file, NULL, &file_error)) {
		if (!g_error_matches (file_error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND)) {
			g_propagate_error (error, file_error);
			return FALSE;
		}

		g_clear_error (&file_error);
	}

	/* Create the database file in a separate interface, so we can
	 * configure page_size and journal_mode outside a transaction, so
	 * they apply throughout the whole lifetime.
	 */
	path = g_file_get_path (file);
	iface = tracker_db_interface_sqlite_new (path,
	                                         db_manager->shared_cache_key,
	                                         0, error);
	g_free (path);

	if (!iface)
		return FALSE;

	tracker_db_interface_execute_query (iface, NULL, "PRAGMA cache_size = %d",
	                                    db_manager->db.page_size);
	tracker_db_interface_execute_query (iface, NULL, "PRAGMA journal_mode = WAL");

	g_object_unref (iface);
	return TRUE;
}

gboolean
tracker_db_manager_attach_database (TrackerDBManager    *db_manager,
                                    TrackerDBInterface  *iface,
                                    const gchar         *name,
                                    gboolean             create,
                                    GError             **error)
{
	gchar *filename, *escaped;
	GFile *file = NULL;

	if (db_manager->cache_location) {
		filename = g_strdup_printf ("%s.db", name);
		escaped = g_uri_escape_string (filename, NULL, FALSE);
		file = g_file_get_child (db_manager->cache_location, escaped);
		g_free (filename);
		g_free (escaped);

		if (create) {
			if (!ensure_create_database_file (db_manager, file, error)) {
				g_object_unref (file);
				return FALSE;
			}
		}
	}

	if (!tracker_db_interface_attach_database (iface, file, name, error)) {
		g_clear_object (&file);
		return FALSE;
	}

	g_clear_object (&file);
	db_set_params (iface, name,
	               db_manager->db.cache_size,
	               db_manager->db.page_size,
	               !(db_manager->flags & TRACKER_DB_MANAGER_IN_MEMORY),
	               error);
	return TRUE;
}

gboolean
tracker_db_manager_detach_database (TrackerDBManager    *db_manager,
                                    TrackerDBInterface  *iface,
                                    const gchar         *name,
                                    GError             **error)
{
	return tracker_db_interface_detach_database (iface, name, error);
}

void
tracker_db_manager_release_memory (TrackerDBManager *db_manager)
{
	TrackerDBInterface *iface;
	gint i, len;

	g_async_queue_lock (db_manager->interfaces);
	len = g_async_queue_length_unlocked (db_manager->interfaces);

	for (i = 0; i < len; i++) {
		iface = g_async_queue_try_pop_unlocked (db_manager->interfaces);
		if (!iface)
			break;

		if (tracker_db_interface_get_is_used (iface))
			g_async_queue_push_unlocked (db_manager->interfaces, iface);
		else
			g_object_unref (iface);
	}

	if (g_async_queue_length_unlocked (db_manager->interfaces) < len) {
		g_debug ("Freed %d readonly interfaces",
		         len - g_async_queue_length_unlocked (db_manager->interfaces));
	}

	if (db_manager->db.iface) {
		gssize bytes;

		bytes = tracker_db_interface_sqlite_release_memory (db_manager->db.iface);

		if (bytes > 0) {
			g_debug ("Freed %" G_GSSIZE_MODIFIER "d bytes from writable interface",
			         bytes);
		}
	}

	g_async_queue_unlock (db_manager->interfaces);
}

TrackerDBVersion
tracker_db_manager_get_version (TrackerDBManager *db_manager)
{
	return db_manager->db_version;
}
