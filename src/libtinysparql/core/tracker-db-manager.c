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
#include <locale.h>

#include <tracker-common.h>

#include "tracker-db-manager.h"
#include "tracker-db-interface-sqlite.h"
#include "tracker-db-interface.h"
#include "tracker-data-manager.h"
#include "tracker-uuid.h"

#define MAX_INTERFACES_PER_CPU        16
#define MAX_INTERFACES                (MAX_INTERFACES_PER_CPU * g_get_num_processors ())

/* Required minimum space needed to create databases (5Mb) */
#define TRACKER_DB_MIN_REQUIRED_SPACE 5242880

#define TRACKER_VACUUM_CHECK_SIZE     ((goffset) 4 * 1024 * 1024 * 1024) /* 4GB */

#define DEFAULT_PAGE_SIZE 8192

#define DEFAULT_DATABASE_FILENAME "meta.db"

#define TRACKER_PARSER_VERSION_STRING G_STRINGIFY(TRACKER_PARSER_VERSION)

#define FTS_FLAGS (TRACKER_DB_MANAGER_FTS_ENABLE_STEMMER |	  \
                   TRACKER_DB_MANAGER_FTS_ENABLE_UNACCENT |	  \
                   TRACKER_DB_MANAGER_FTS_ENABLE_STOP_WORDS |	  \
                   TRACKER_DB_MANAGER_FTS_IGNORE_NUMBERS)

struct _TrackerDBManager {
	GObject parent_instance;

	TrackerDBInterface *iface;
	gchar *abs_filename;
	gchar *data_dir;
	gchar *corrupted_filename;
	GFile *cache_location;
	gchar *shared_cache_key;
	TrackerDBManagerFlags flags;
	guint s_cache_size;
	gboolean first_time;
	TrackerDBVersion db_version;

	GWeakRef iface_data;

	GAsyncQueue *interfaces;
};

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
tracker_db_manager_get_flags (TrackerDBManager *db_manager)
{
	return db_manager->flags;
}

static void
iface_set_params (TrackerDBInterface   *iface,
                  gboolean              readonly,
                  GError              **error)
{
	int32_t application_id = ('S' << 24 | 'p' << 16 | 'q' << 8 | 'l' << 0);

	tracker_db_interface_execute_query (iface, NULL, "PRAGMA encoding = 'UTF-8'");
	tracker_db_interface_execute_query (iface, NULL, "PRAGMA application_id = %d", application_id);

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
	tracker_db_interface_execute_query (iface, NULL, "PRAGMA page_size = %d", page_size);

	tracker_db_interface_execute_query (iface, NULL, "PRAGMA synchronous = NORMAL");
	tracker_db_interface_execute_query (iface, NULL, "PRAGMA auto_vacuum = 0");

	if (enable_wal) {
		stmt = tracker_db_interface_create_vstatement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE,
		                                               &internal_error,
		                                               "PRAGMA journal_mode = WAL");

		if (internal_error) {
			g_debug ("Can't set journal mode to WAL: '%s'",
			         internal_error->message);
			g_propagate_error (error, internal_error);
		} else {
			TrackerSparqlCursor *cursor;

			cursor = TRACKER_SPARQL_CURSOR (tracker_db_statement_start_cursor (stmt, NULL));
			if (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
				if (g_ascii_strcasecmp (tracker_sparql_cursor_get_string (cursor, 0, NULL), "WAL") != 0) {
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

	tracker_db_interface_execute_query (iface, NULL, "PRAGMA journal_size_limit = 10240000");

	tracker_db_interface_execute_query (iface, NULL, "PRAGMA cache_size = %d", cache_size);
	TRACKER_NOTE (SQLITE, g_message ("  Setting cache size to %d", cache_size));
}

static gboolean
tracker_db_manager_get_metadata (TrackerDBManager   *db_manager,
                                 const gchar        *key,
                                 GValue             *value)
{
	TrackerDBInterface *iface;
	TrackerDBStatement *stmt;
	TrackerSparqlCursor *cursor;

	iface = tracker_db_manager_get_writable_db_interface (db_manager);
	stmt = tracker_db_interface_create_statement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE,
	                                              NULL,
	                                              "SELECT value FROM metadata WHERE key = ?");
	if (!stmt)
		return FALSE;

	tracker_db_statement_bind_text (stmt, 0, key);
	cursor = TRACKER_SPARQL_CURSOR (tracker_db_statement_start_cursor (stmt, NULL));
	g_object_unref (stmt);

	if (!cursor || !tracker_sparql_cursor_next (cursor, NULL, NULL)) {
		g_clear_object (&cursor);
		return FALSE;
	}

	tracker_db_cursor_get_value (TRACKER_DB_CURSOR (cursor), 0, value);
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
	TrackerSparqlCursor *cursor;
	TrackerDBVersion version;

	iface = tracker_db_manager_get_writable_db_interface (db_manager);
	stmt = tracker_db_interface_create_statement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE,
	                                              NULL, "PRAGMA user_version");
	if (!stmt)
		return TRACKER_DB_VERSION_UNKNOWN;

	cursor = TRACKER_SPARQL_CURSOR (tracker_db_statement_start_cursor (stmt, NULL));
	g_object_unref (stmt);

	if (!cursor || !tracker_sparql_cursor_next (cursor, NULL, NULL)) {
		g_clear_object (&cursor);
		return TRACKER_DB_VERSION_UNKNOWN;
	}

	version = tracker_sparql_cursor_get_integer (cursor, 0);
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
	const gchar *current_locale;
	gboolean changed;

	/* Get current collation locale */
	current_locale = setlocale (LC_COLLATE, NULL);

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

	return changed;
}

void
tracker_db_manager_set_current_locale (TrackerDBManager *db_manager)
{
	const gchar *current_locale;

	/* Get current collation locale */
	current_locale = setlocale (LC_COLLATE, NULL);
	g_debug ("Saving DB locale as: '%s'", current_locale);
	db_set_locale (db_manager, current_locale);
}

gboolean
tracker_db_manager_ontology_checksum_changed (TrackerDBManager *db_manager,
                                              const gchar      *checksum)
{
	GValue value = G_VALUE_INIT;
	gboolean equal;

	if (!tracker_db_manager_get_metadata (db_manager, "ontology-checksum", &value))
		return TRUE;

	equal = g_str_equal (g_value_get_string (&value), checksum);
	g_value_unset (&value);

	return !equal;
}

void
tracker_db_manager_set_ontology_checksum (TrackerDBManager *db_manager,
                                          const gchar      *checksum)
{
	GValue value = G_VALUE_INIT;

	g_value_init (&value, G_TYPE_STRING);
	g_value_set_string (&value, checksum);
	tracker_db_manager_set_metadata (db_manager, "ontology-checksum", &value);
	g_value_unset (&value);
}

static void
tracker_db_manager_ensure_location (TrackerDBManager *db_manager,
				    GFile            *cache_location)
{
	gchar *path, *basename;

	if ((db_manager->flags & TRACKER_DB_MANAGER_IN_MEMORY) != 0) {
		return;
	}

	path = g_file_get_path (cache_location);
	basename = g_file_get_basename (cache_location);

	/* If the basename has an extension, we consider the cache location
	 * to describe a database file name. We however ensure backwards
	 * compatible behavior by checking that the path does not exist already
	 * as a directory.
	 *
	 * Otherwise (no extension, path is an existing directory), we use
	 * the path as the dir, with a default database filename.
	 */
	if (!g_file_test (path, G_FILE_TEST_IS_DIR) && strstr (basename, ".")) {
		db_manager->data_dir = g_path_get_dirname (path);
		db_manager->abs_filename = path;
	} else {
		db_manager->data_dir = path;
		db_manager->abs_filename = g_build_filename (path, DEFAULT_DATABASE_FILENAME, NULL);
	}

	g_free (basename);
}

void
tracker_db_manager_rollback_db_creation (TrackerDBManager *db_manager)
{
	g_return_if_fail (db_manager->first_time);

	if ((db_manager->flags & TRACKER_DB_MANAGER_IN_MEMORY) != 0)
		return;

	if (g_unlink (db_manager->abs_filename) < 0)
		g_warning ("Could not delete database file: %m");
}

gboolean
tracker_db_manager_check_integrity (TrackerDBManager  *db_manager,
                                    GError           **error)
{
	TrackerDBStatement *stmt;
	TrackerSparqlCursor *cursor = NULL;

	stmt = tracker_db_interface_create_statement (db_manager->iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE,
	                                              error,
	                                              "PRAGMA integrity_check(1)");
	if (!stmt)
		return FALSE;

	cursor = TRACKER_SPARQL_CURSOR (tracker_db_statement_start_cursor (stmt, error));
	g_object_unref (stmt);

	if (!cursor)
		return FALSE;

	if (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
		const gchar *check_result;

		check_result = tracker_sparql_cursor_get_string (cursor, 0, NULL);
		if (g_strcmp0 (check_result, "ok") != 0) {
			GError *inner_error = NULL;

			if (!g_file_set_contents (db_manager->corrupted_filename, "", -1, &inner_error))
				g_warning ("Could not mark database as corrupted: %s", inner_error->message);

			g_clear_error (&inner_error);
			g_clear_object (&cursor);
			g_set_error (error,
			             TRACKER_DB_INTERFACE_ERROR,
			             TRACKER_DB_CORRUPT,
			             "Integrity check failed: %s", check_result);
			return FALSE;
		}
	}

	g_object_unref (cursor);

	return TRUE;
}

TrackerDBManager *
tracker_db_manager_new (TrackerDBManagerFlags   flags,
                        GFile                  *cache_location,
                        guint                   select_cache_size,
                        GObject                *iface_data,
                        GError                **error)
{
	TrackerDBManager *db_manager;
	TrackerDBInterface *resources_iface;
	GError *internal_error = NULL;
	gboolean need_to_create = FALSE;

	db_manager = g_object_new (TRACKER_TYPE_DB_MANAGER, NULL);

	/* Set default value for first_time */
	db_manager->first_time = FALSE;

	/* Set up locations */
	db_manager->flags = flags;
	db_manager->s_cache_size = select_cache_size;
	db_manager->interfaces = g_async_queue_new_full (g_object_unref);

	g_set_object (&db_manager->cache_location, cache_location);
	g_weak_ref_init (&db_manager->iface_data, iface_data);

	if ((db_manager->flags & TRACKER_DB_MANAGER_IN_MEMORY) == 0) {
		gchar *basename, *corrupted_basename;

		tracker_db_manager_ensure_location (db_manager, cache_location);

		basename = g_path_get_basename (db_manager->abs_filename);
		corrupted_basename = g_strdup_printf (".%s.corrupted", basename);
		g_free (basename);

		db_manager->corrupted_filename = g_build_filename (db_manager->data_dir,
		                                                   corrupted_basename,
		                                                   NULL);
		g_free (corrupted_basename);

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
		db_manager->shared_cache_key = tracker_generate_uuid ("file");
	}

	if ((db_manager->flags & TRACKER_DB_MANAGER_IN_MEMORY) != 0) {
		need_to_create = TRUE;
	} else if (!g_file_test (db_manager->abs_filename, G_FILE_TEST_EXISTS)) {
		if ((flags & TRACKER_DB_MANAGER_READONLY) == 0) {
			TRACKER_NOTE (SQLITE, g_message ("Could not find database file:'%s', will create it.", db_manager->abs_filename));
			need_to_create = TRUE;
		} else {
			g_set_error (error,
			             TRACKER_DB_INTERFACE_ERROR,
			             TRACKER_DB_OPEN_ERROR,
			             "Could not find database file:'%s'.", db_manager->abs_filename);

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

	if (need_to_create) {
		db_manager->first_time = TRUE;

		if (!tracker_db_manager_has_enough_space (db_manager)) {
			g_set_error (error,
			             TRACKER_DB_INTERFACE_ERROR,
			             TRACKER_DB_OPEN_ERROR,
			             "Filesystem does not have enough space");
			return FALSE;
		}

		TRACKER_NOTE (SQLITE, g_message ("Creating database files for %s...", db_manager->abs_filename));

		db_manager->iface = tracker_db_manager_create_db_interface (db_manager, FALSE, &internal_error);
		if (internal_error) {
			g_propagate_error (error, internal_error);
			g_object_unref (db_manager);
			return FALSE;
		}

		g_clear_object (&db_manager->iface);
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

	tracker_db_manager_release_memory (db_manager);

	g_async_queue_unref (db_manager->interfaces);
	g_free (db_manager->abs_filename);

	if (db_manager->iface) {
		if (!readonly)
			tracker_db_interface_sqlite_wal_checkpoint (db_manager->iface, TRUE, NULL);
		g_object_unref (db_manager->iface);
	}

	g_weak_ref_clear (&db_manager->iface_data);

	g_free (db_manager->data_dir);

	g_free (db_manager->corrupted_filename);
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
	const gchar *path_or_handle = NULL;
	GObject *user_data;

	if (readonly)
		flags |= TRACKER_DB_INTERFACE_READONLY;
	if (db_manager->flags & TRACKER_DB_MANAGER_IN_MEMORY)
		flags |= TRACKER_DB_INTERFACE_IN_MEMORY;

	path_or_handle = db_manager->abs_filename ?
		db_manager->abs_filename : db_manager->shared_cache_key;

	connection = tracker_db_interface_sqlite_new (path_or_handle,
	                                              flags,
	                                              &internal_error);
	if (internal_error) {
		g_propagate_error (error, internal_error);
		return NULL;
	}

	user_data = g_weak_ref_get (&db_manager->iface_data);
	tracker_db_interface_set_user_data (connection, user_data);
	g_object_unref (user_data);

	tracker_db_interface_init_vtabs (connection);

	iface_set_params (connection,
	                  readonly,
	                  &internal_error);
	db_set_params (connection, "main",
	               TRACKER_DB_CACHE_SIZE_DEFAULT,
	               DEFAULT_PAGE_SIZE,
	               !(db_manager->flags & TRACKER_DB_MANAGER_IN_MEMORY),
	               &internal_error);

	if (internal_error) {
		g_propagate_error (error, internal_error);
		g_object_unref (connection);
		return NULL;
	}

	if (!tracker_db_interface_sqlite_fts_init (connection,
	                                           db_manager->flags,
	                                           error)) {
		g_object_unref (connection);
		return NULL;
	}

	tracker_db_interface_set_max_stmt_cache_size (connection,
	                                              TRACKER_DB_STATEMENT_CACHE_TYPE_SELECT,
	                                              db_manager->s_cache_size);

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

	if (!interface) {
		/* 3rd. Create a new interface to satisfy the request */
		interface = tracker_db_manager_create_db_interface (db_manager,
		                                                    TRUE, &internal_error);

		if (!interface) {
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
	if (db_manager->iface == NULL)
		db_manager->iface = init_writable_db_interface (db_manager);

	return db_manager->iface;
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
	if (tracker_file_get_size (db_manager->abs_filename) < TRACKER_VACUUM_CHECK_SIZE)
		return;

	iface = tracker_db_manager_get_writable_db_interface (db_manager);
	tracker_db_interface_execute_query (iface, NULL, "VACUUM");
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

		if (tracker_db_interface_get_is_used (iface)) {
			g_async_queue_push_unlocked (db_manager->interfaces, iface);
		} else {
			if (tracker_db_interface_found_corruption (iface)) {
				GError *error = NULL;

				if (!g_file_set_contents (db_manager->corrupted_filename, "", -1, &error))
					g_warning ("Could not mark database as corrupted: %s", error->message);

				g_clear_error (&error);
			}

			g_object_unref (iface);
		}
	}

	if (g_async_queue_length_unlocked (db_manager->interfaces) < len) {
		g_debug ("Freed %d readonly interfaces",
		         len - g_async_queue_length_unlocked (db_manager->interfaces));
	}

	if (db_manager->iface) {
		gssize bytes;

		bytes = tracker_db_interface_sqlite_release_memory (db_manager->iface);

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

gboolean
tracker_db_manager_needs_repair (TrackerDBManager *db_manager)
{
	if (g_file_test (db_manager->corrupted_filename, G_FILE_TEST_EXISTS)) {
		if (g_unlink (db_manager->corrupted_filename) < 0)
			g_warning ("Could not unmark database as corrupted: %m");

		return TRUE;
	}

	return FALSE;
}
