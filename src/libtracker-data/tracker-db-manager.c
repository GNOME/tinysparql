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

#include <libtracker-common/tracker-common.h>
#include <libtracker-common/tracker-parser-sha1.h>

#if HAVE_TRACKER_FTS
#include <libtracker-fts/tracker-fts.h>
#endif

#include "tracker-db-journal.h"
#include "tracker-db-manager.h"
#include "tracker-db-interface-sqlite.h"
#include "tracker-db-interface.h"
#include "tracker-data-manager.h"

/* ZLib buffer settings */
#define ZLIB_BUF_SIZE                 8192

/* Required minimum space needed to create databases (5Mb) */
#define TRACKER_DB_MIN_REQUIRED_SPACE 5242880

/* Default memory settings for databases */
#define TRACKER_DB_PAGE_SIZE_DONT_SET -1

/* Set current database version we are working with */
#define TRACKER_DB_VERSION_NOW        TRACKER_DB_VERSION_0_15_2
#define TRACKER_DB_VERSION_FILE       "db-version.txt"
#define TRACKER_DB_LOCALE_FILE        "db-locale.txt"

#define IN_USE_FILENAME               ".meta.isrunning"

/* Stamp files to know crawling/indexing state */
#define FIRST_INDEX_FILENAME          "first-index.txt"
#define LAST_CRAWL_FILENAME           "last-crawl.txt"
#define NEED_MTIME_CHECK_FILENAME     "no-need-mtime-check.txt"
#define PARSER_SHA1_FILENAME          "parser-sha1.txt"

typedef enum {
	TRACKER_DB_LOCATION_DATA_DIR,
	TRACKER_DB_LOCATION_USER_DATA_DIR,
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
	TRACKER_DB_VERSION_0_9_0,   /* unstable release */
	TRACKER_DB_VERSION_0_9_8,   /* affiliation cardinality + volumes */
	TRACKER_DB_VERSION_0_9_15,  /* mtp:hidden */
	TRACKER_DB_VERSION_0_9_16,  /* Fix for NB#184823 */
	TRACKER_DB_VERSION_0_9_19,  /* collation */
	TRACKER_DB_VERSION_0_9_21,  /* Fix for NB#186055 */
	TRACKER_DB_VERSION_0_9_24,  /* nmo:PhoneMessage class */
	TRACKER_DB_VERSION_0_9_34,  /* ontology cache */
	TRACKER_DB_VERSION_0_9_38,  /* nie:url an inverse functional property */
	TRACKER_DB_VERSION_0_15_2   /* fts4 */
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
	  TRACKER_DB_CACHE_SIZE_DEFAULT,
	  8192,
	  FALSE,
	  FALSE,
	  0 },
};

static gboolean            db_exec_no_reply                        (TrackerDBInterface   *iface,
                                                                    const gchar          *query,
                                                                    ...);
static TrackerDBInterface *db_interface_create                      (TrackerDB            db,
                                                                     GError             **error);
static TrackerDBInterface *tracker_db_manager_get_db_interfaces     (GError             **error,
                                                                     gint                 num, ...);
static TrackerDBInterface *tracker_db_manager_get_db_interfaces_ro  (GError             **error,
                                                                     gint                 num, ...);
static void                db_remove_locale_file                    (void);

static gboolean              initialized;
static gboolean              locations_initialized;
static gchar                *data_dir = NULL;
static gchar                *user_data_dir = NULL;
static gchar                *in_use_filename = NULL;
static gpointer              db_type_enum_class_pointer;
static TrackerDBManagerFlags old_flags = 0;
static guint                 s_cache_size;
static guint                 u_cache_size;

static GPrivate              interface_data_key = G_PRIVATE_INIT ((GDestroyNotify)g_object_unref);

/* mutex used by singleton connection in libtracker-direct, not used by tracker-store */
static GMutex                global_mutex;

static TrackerDBInterface   *global_iface;

static const gchar *
location_to_directory (TrackerDBLocation location)
{
	switch (location) {
	case TRACKER_DB_LOCATION_DATA_DIR:
		return data_dir;
	case TRACKER_DB_LOCATION_USER_DATA_DIR:
		return user_data_dir;
	default:
		return NULL;
	};
}

static gboolean
db_exec_no_reply (TrackerDBInterface *iface,
                  const gchar        *query,
                  ...)
{
	va_list                     args;

	va_start (args, query);
	tracker_db_interface_execute_vquery (iface, NULL, query, args);
	va_end (args);

	return TRUE;
}

TrackerDBManagerFlags
tracker_db_manager_get_flags (guint *select_cache_size, guint *update_cache_size)
{
	if (select_cache_size)
		*select_cache_size = s_cache_size;

	if (update_cache_size)
		*update_cache_size = u_cache_size;

	return old_flags;
}

static void
db_set_params (TrackerDBInterface   *iface,
               gint                  cache_size,
               gint                  page_size,
               GError              **error)
{
	gchar *queries = NULL;
	const gchar *pragmas_file;

	pragmas_file = g_getenv ("TRACKER_PRAGMAS_FILE");

	if (pragmas_file && g_file_get_contents (pragmas_file, &queries, NULL, NULL)) {
		gchar *query = strtok (queries, "\n");
		g_debug ("PRAGMA's from file: %s", pragmas_file);
		while (query) {
			g_debug ("  INIT query: %s", query);
			tracker_db_interface_execute_query (iface, NULL, "%s", query);
			query = strtok (NULL, "\n");
		}
		g_free (queries);
	} else {
		GError *internal_error = NULL;
		TrackerDBStatement *stmt;

#ifdef DISABLE_JOURNAL
		tracker_db_interface_execute_query (iface, NULL, "PRAGMA synchronous = NORMAL;");
#else
		tracker_db_interface_execute_query (iface, NULL, "PRAGMA synchronous = OFF;");
#endif /* DISABLE_JOURNAL */
		tracker_db_interface_execute_query (iface, NULL, "PRAGMA temp_store = FILE;");
		tracker_db_interface_execute_query (iface, NULL, "PRAGMA encoding = \"UTF-8\"");
		tracker_db_interface_execute_query (iface, NULL, "PRAGMA auto_vacuum = 0;");

		stmt = tracker_db_interface_create_statement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE,
		                                              &internal_error,
		                                              "PRAGMA journal_mode = WAL;");

		if (internal_error) {
			g_message ("Can't set journal mode to WAL: '%s'",
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

		if (stmt) {
			g_object_unref (stmt);
		}

		/* disable autocheckpoint */
		tracker_db_interface_execute_query (iface, NULL, "PRAGMA wal_autocheckpoint = 0");

		tracker_db_interface_execute_query (iface, NULL, "PRAGMA journal_size_limit = 10240000");

		if (page_size != TRACKER_DB_PAGE_SIZE_DONT_SET) {
			g_message ("  Setting page size to %d", page_size);
			tracker_db_interface_execute_query (iface, NULL, "PRAGMA page_size = %d", page_size);
		}

		tracker_db_interface_execute_query (iface, NULL, "PRAGMA cache_size = %d", cache_size);
		g_message ("  Setting cache size to %d", cache_size);
	}
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
db_interface_get (TrackerDB   type,
                  gboolean   *create,
                  GError    **error)
{
	TrackerDBInterface *iface;
	const gchar *path;
	GError *internal_error = NULL;

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

	iface = tracker_db_interface_sqlite_new (path,
	                                         &internal_error);

	if (internal_error) {
		g_propagate_error (error, internal_error);
		return NULL;
	}

	db_set_params (iface,
	               dbs[type].cache_size,
	               dbs[type].page_size,
	               &internal_error);

	if (internal_error) {
		g_propagate_error (error, internal_error);
		return NULL;
	}

	return iface;
}

static TrackerDBInterface *
db_interface_get_metadata (GError **error)
{
	TrackerDBInterface *iface;
	gboolean create;
	GError *internal_error = NULL;

	iface = db_interface_get (TRACKER_DB_METADATA, &create, &internal_error);

	if (internal_error) {
		g_propagate_error (error, internal_error);
		return NULL;
	}

	return iface;
}

static TrackerDBInterface *
db_interface_create (TrackerDB db,
                     GError **error)
{
	TrackerDBInterface *iface;
	GError *internal_error = NULL;

	switch (db) {
	case TRACKER_DB_UNKNOWN:
		return NULL;

	case TRACKER_DB_METADATA:
		iface = db_interface_get_metadata (&internal_error);
		if (internal_error) {
			g_propagate_error (error, internal_error);
			return NULL;
		}
		return iface;

	default:
		g_critical ("This TrackerDB type:%d->'%s' has no interface set up yet!!",
		            db,
		            db_type_to_string (db));
		return NULL;
	}
}

static void
db_manager_remove_journal (void)
{
#ifndef DISABLE_JOURNAL
	gchar *path;
	gchar *directory, *rotate_to = NULL;
	gsize chunk_size;
	gboolean do_rotate = FALSE;
	const gchar *dirs[3] = { NULL, NULL, NULL };
	guint i;
	GError *error = NULL;

	/* We duplicate the path here because later we shutdown the
	 * journal which frees this data. We want to survive that.
	 */
	path = g_strdup (tracker_db_journal_get_filename ());
	if (!path) {
		return;
	}

	g_message ("  Removing journal:'%s'", path);

	directory = g_path_get_dirname (path);

	tracker_db_journal_get_rotating (&do_rotate, &chunk_size, &rotate_to);
	tracker_db_journal_shutdown (&error);

	if (error) {
		/* TODO: propagate error */
		g_message ("Ignored error while shutting down journal during remove: %s",
		           error->message ? error->message : "No error given");
		g_error_free (error);
	}

	dirs[0] = directory;
	dirs[1] = do_rotate ? rotate_to : NULL;

	for (i = 0; dirs[i] != NULL; i++) {
		GDir *journal_dir;
		const gchar *f;

		journal_dir = g_dir_open (dirs[i], 0, NULL);
		if (!journal_dir) {
			continue;
		}

		/* Remove rotated chunks */
		while ((f = g_dir_read_name (journal_dir)) != NULL) {
			gchar *fullpath;

			if (!g_str_has_prefix (f, TRACKER_DB_JOURNAL_FILENAME ".")) {
				continue;
			}

			fullpath = g_build_filename (dirs[i], f, NULL);
			if (g_unlink (fullpath) == -1) {
				g_message ("%s", g_strerror (errno));
			}
			g_free (fullpath);
		}

		g_dir_close (journal_dir);
	}

	g_free (rotate_to);
	g_free (directory);

	/* Remove active journal */
	if (g_unlink (path) == -1) {
		g_message ("%s", g_strerror (errno));
	}
	g_free (path);
#endif /* DISABLE_JOURNAL */
}

static void
db_manager_remove_all (gboolean rm_journal)
{
	guint i;

	g_message ("Removing all database/storage files");

	/* Remove stamp files */
	tracker_db_manager_set_first_index_done (FALSE);
	tracker_db_manager_set_last_crawl_done (FALSE);
	tracker_db_manager_set_need_mtime_check (TRUE);

	/* NOTE: We don't have to be initialized for this so we
	 * calculate the absolute directories here.
	 */
	for (i = 1; i < G_N_ELEMENTS (dbs); i++) {
		gchar *filename;

		g_message ("  Removing database:'%s'", dbs[i].abs_filename);
		g_unlink (dbs[i].abs_filename);

		/* also delete shm and wal helper files */
		filename = g_strdup_printf ("%s-shm", dbs[i].abs_filename);
		g_unlink (filename);
		g_free (filename);

		filename = g_strdup_printf ("%s-wal", dbs[i].abs_filename);
		g_unlink (filename);
		g_free (filename);
	}

	if (rm_journal) {
		db_manager_remove_journal ();

		/* If also the journal is gone, we can also remove db-version.txt, it
		 * would have no more relevance whatsoever. */
		tracker_db_manager_remove_version_file ();
	}

	/* Remove locale file also */
	db_remove_locale_file ();
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

void
tracker_db_manager_create_version_file (void)
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

void
tracker_db_manager_remove_version_file (void)
{
	gchar *filename;

	filename = g_build_filename (data_dir, TRACKER_DB_VERSION_FILE, NULL);
	g_message ("  Removing db-version file:'%s'", filename);
	g_unlink (filename);
	g_free (filename);
}

static void
db_remove_locale_file (void)
{
	gchar *filename;

	filename = g_build_filename (data_dir, TRACKER_DB_LOCALE_FILE, NULL);
	g_message ("  Removing db-locale file:'%s'", filename);
	g_unlink (filename);
	g_free (filename);
}

static gchar *
db_get_locale (void)
{
	gchar *locale = NULL;
	gchar *filename;

	filename = g_build_filename (data_dir, TRACKER_DB_LOCALE_FILE, NULL);

	if (G_LIKELY (g_file_test (filename, G_FILE_TEST_EXISTS))) {
		gchar *contents;

		/* Check locale is correct */
		if (G_LIKELY (g_file_get_contents (filename, &contents, NULL, NULL))) {
			if (contents && strlen (contents) == 0) {
				g_critical ("  Empty locale file found at '%s'", filename);
				g_free (contents);
			} else {
				/* Re-use contents */
				locale = contents;
			}
		} else {
			g_critical ("  Could not get content of file '%s'", filename);
		}
	} else {
		/* expected when restoring from backup, always recreate indices */
		g_message ("  Could not find database locale file:'%s'", filename);
		locale = g_strdup ("unknown");
	}

	g_free (filename);

	return locale;
}

static void
db_set_locale (const gchar *locale)
{
	GError *error = NULL;
	gchar  *filename;
	gchar  *str;

	filename = g_build_filename (data_dir, TRACKER_DB_LOCALE_FILE, NULL);
	g_message ("  Creating locale file '%s'", filename);

	str = g_strdup_printf ("%s", locale ? locale : "");

	if (!g_file_set_contents (filename, str, -1, &error)) {
		g_message ("  Could not set file contents, %s",
		           error ? error->message : "no error given");
		g_clear_error (&error);
	}

	g_free (str);
	g_free (filename);
}

gboolean
tracker_db_manager_locale_changed (void)
{
	gchar *db_locale;
	gchar *current_locale;
	gboolean changed;

	/* As a special case, we allow calling this API function before
	 * tracker_data_manager_init() has been called, so it can be used
	 * to check for locale mismatches for initializing the database.
	 */
	if (!locations_initialized) {
		tracker_db_manager_init_locations ();
	}

	/* Get current collation locale */
	current_locale = tracker_locale_get (TRACKER_LOCALE_COLLATE);

	/* Get db locale */
	db_locale = db_get_locale ();

	/* If they are different, recreate indexes. Note that having
	 * both to NULL is actually valid, they would default to
	 * the unicode collation without locale-specific stuff. */
	if (g_strcmp0 (db_locale, current_locale) != 0) {
		g_message ("Locale change detected from '%s' to '%s'...",
		           db_locale, current_locale);
		changed = TRUE;
	} else {
		g_message ("Current and DB locales match: '%s'", db_locale);
		changed = FALSE;
	}

	g_free (db_locale);
	g_free (current_locale);

	return changed;
}

void
tracker_db_manager_set_current_locale (void)
{
	gchar *current_locale;

	/* Get current collation locale */
	current_locale = tracker_locale_get (TRACKER_LOCALE_COLLATE);
	g_message ("Saving DB locale as: '%s'", current_locale);
	db_set_locale (current_locale);
	g_free (current_locale);
}

static void
db_manager_analyze (TrackerDB           db,
                    TrackerDBInterface *iface)
{
	guint64             current_mtime;

	current_mtime = tracker_file_get_mtime (dbs[db].abs_filename);

	if (current_mtime > dbs[db].mtime) {
		g_message ("  Analyzing DB:'%s'", dbs[db].name);
		db_exec_no_reply (iface, "ANALYZE %s.Services", dbs[db].name);

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
		};

		etype = g_enum_register_static ("TrackerDB", values);
	}

	return etype;
}

static void
db_recreate_all (GError **error)
{
	guint i;
	gchar *locale;
	GError *internal_error = NULL;

	/* We call an internal version of this function here
	 * because at the time 'initialized' = FALSE and that
	 * will cause errors and do nothing.
	 */
	g_message ("Cleaning up database files for reindex");

	db_manager_remove_all (FALSE);

	/* Now create the databases and close them */
	g_message ("Creating database files, this may take a few moments...");

	for (i = 1; i < G_N_ELEMENTS (dbs); i++) {
		dbs[i].iface = db_interface_create (i, &internal_error);
		if (internal_error) {
			guint y;

			for (y = 1; y < i; y++) {
				g_object_unref (dbs[y].iface);
				dbs[y].iface = NULL;
			}

			g_propagate_error (error, internal_error);

			return;
		}
	}

	/* We don't close the dbs in the same loop as before
	 * becase some databases need other databases
	 * attached to be created correctly.
	 */
	for (i = 1; i < G_N_ELEMENTS (dbs); i++) {
		g_object_unref (dbs[i].iface);
		dbs[i].iface = NULL;
	}

	locale = tracker_locale_get (TRACKER_LOCALE_COLLATE);
	/* Initialize locale file */
	db_set_locale (locale);
	g_free (locale);
}

void
tracker_db_manager_init_locations (void)
{
	const gchar *dir;
	guint i;

	if (locations_initialized) {
		return;
	}

	user_data_dir = g_build_filename (g_get_user_data_dir (),
	                                  "tracker",
	                                  "data",
	                                  NULL);

	/* For DISABLE_JOURNAL case we should use g_get_user_data_dir here. For now
	 * keeping this as-is */

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
perform_recreate (gboolean *first_time, GError **error)
{
	GError *internal_error = NULL;
	guint i;

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
		g_set_error (error,
		             TRACKER_DB_INTERFACE_ERROR,
		             TRACKER_DB_OPEN_ERROR,
		             "Filesystem has not enough space");
		return;
	}

	db_recreate_all (&internal_error);

	if (internal_error) {
		g_propagate_error (error, internal_error);
	}
}

gboolean
tracker_db_manager_init (TrackerDBManagerFlags   flags,
                         gboolean               *first_time,
                         gboolean                restoring_backup,
                         gboolean                shared_cache,
                         guint                   select_cache_size,
                         guint                   update_cache_size,
                         TrackerBusyCallback     busy_callback,
                         gpointer                busy_user_data,
                         const gchar            *busy_operation,
                         GError                **error)
{
	GType etype;
	TrackerDBVersion version;
	const gchar *dir;
	gboolean need_reindex;
	guint i;
	int in_use_file;
	gboolean loaded = FALSE;
	TrackerDBInterface *resources_iface;
	GError *internal_error = NULL;

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

	tracker_db_manager_init_locations ();

	g_free (in_use_filename);
	in_use_filename = g_build_filename (g_get_user_data_dir (),
	                                    "tracker",
	                                    "data",
	                                    IN_USE_FILENAME,
	                                    NULL);

	/* Don't do need_reindex checks for readonly (direct-access) */
	if ((flags & TRACKER_DB_MANAGER_READONLY) == 0) {

		/* Make sure the directories exist */
		g_message ("Checking database directories exist");

		g_mkdir_with_parents (data_dir, 00755);
		g_mkdir_with_parents (user_data_dir, 00755);

		g_message ("Checking database version");

		version = db_get_version ();

		if (version < TRACKER_DB_VERSION_NOW) {
			g_message ("  A reindex will be forced");
			need_reindex = TRUE;
		}

		if (need_reindex) {
			tracker_db_manager_create_version_file ();
			tracker_db_manager_set_need_mtime_check (TRUE);
		}
	}

	g_message ("Checking database files exist");

	for (i = 1; i < G_N_ELEMENTS (dbs); i++) {
		/* Check we have each database in place, if one is
		 * missing, we reindex.
		 */

		if ((flags & TRACKER_DB_MANAGER_READONLY) == 0) {
			/* No need to check for other files not existing (for
			 * reindex) if one is already missing.
			 */
			if (need_reindex) {
				continue;
			}
		}

		if (!g_file_test (dbs[i].abs_filename, G_FILE_TEST_EXISTS)) {
			if ((flags & TRACKER_DB_MANAGER_READONLY) == 0) {
				g_message ("Could not find database file:'%s'", dbs[i].abs_filename);
				g_message ("One or more database files are missing, a reindex will be forced");
				need_reindex = TRUE;
			} else {
				guint y;

				g_set_error (error,
				             TRACKER_DB_INTERFACE_ERROR,
				             TRACKER_DB_OPEN_ERROR,
				             "Could not find database file:'%s'. One or more database files are missing", dbs[i].abs_filename);

				for (y = 1; y <= i; y++) {
					g_free (dbs[y].abs_filename);
					dbs[y].abs_filename = NULL;
				}

				return FALSE;
			}
		}
	}

	locations_initialized = TRUE;

	/* Don't do remove-dbs for readonly (direct-access) */
	if ((flags & TRACKER_DB_MANAGER_READONLY) == 0) {

		/* If we are just initializing to remove the databases,
		 * return here.
		 */
		if ((flags & TRACKER_DB_MANAGER_REMOVE_ALL) != 0) {
			initialized = TRUE;
			return TRUE;
		}
	}

	/* Set general database options */
	if (shared_cache) {
		g_message ("Enabling database shared cache");
		tracker_db_interface_sqlite_enable_shared_cache ();
	}

	/* Should we reindex? If so, just remove all databases files,
	 * NOT the paths, note, that these paths are also used for
	 * other things like the nfs lock file.
	 */
	if (flags & TRACKER_DB_MANAGER_FORCE_REINDEX || need_reindex) {

		if (flags & TRACKER_DB_MANAGER_READONLY) {
			/* no reindexing supported in read-only mode (direct access) */

			g_set_error (error,
			             TRACKER_DB_INTERFACE_ERROR,
			             TRACKER_DB_OPEN_ERROR,
			             "No reindexing supported in read-only mode (direct access)");

			return FALSE;
		}

		/* Clear the first-index stamp file */
		tracker_db_manager_set_first_index_done (FALSE);

		perform_recreate (first_time, &internal_error);

		if (internal_error) {
			g_propagate_error (error, internal_error);
			return FALSE;
		}

		/* Load databases */
		g_message ("Loading databases files...");

	} else if ((flags & TRACKER_DB_MANAGER_READONLY) == 0) {
		/* do not do shutdown check for read-only mode (direct access) */
		gboolean must_recreate = FALSE;
#ifndef DISABLE_JOURNAL
		gchar *journal_filename;
#endif /* DISABLE_JOURNAL */

		/* Load databases */
		g_message ("Loading databases files...");

#ifndef DISABLE_JOURNAL
		journal_filename = g_build_filename (g_get_user_data_dir (),
		                                     "tracker",
		                                     "data",
		                                     TRACKER_DB_JOURNAL_FILENAME,
		                                     NULL);

		must_recreate = !tracker_db_journal_reader_verify_last (journal_filename,
		                                                        NULL);

		g_free (journal_filename);
#endif /* DISABLE_JOURNAL */

		if (!must_recreate && g_file_test (in_use_filename, G_FILE_TEST_EXISTS)) {
			gsize size = 0;

			g_message ("Didn't shut down cleanly last time, doing integrity checks");

			for (i = 1; i < G_N_ELEMENTS (dbs) && !must_recreate; i++) {
				struct stat st;
				TrackerDBStatement *stmt;
#ifndef DISABLE_JOURNAL
				gchar *busy_status;
#endif /* DISABLE_JOURNAL */

				if (g_stat (dbs[i].abs_filename, &st) == 0) {
					size = st.st_size;
				}

				/* Size is 1 when using echo > file.db, none of our databases
				 * are only one byte in size even initually. */

				if (size <= 1) {
					if (!restoring_backup) {
						must_recreate = TRUE;
					} else {
						g_set_error (&internal_error,
						             TRACKER_DB_INTERFACE_ERROR,
						             TRACKER_DB_OPEN_ERROR,
						             "Corrupt db file");
					}
					continue;
				}

				dbs[i].iface = db_interface_create (i, &internal_error);

				if (internal_error) {
					/* If this already doesn't succeed, then surely the file is
					 * corrupt. No need to check for integrity anymore. */
					if (!restoring_backup) {
						g_clear_error (&internal_error);
						must_recreate = TRUE;
					}
					continue;
				}

				dbs[i].mtime = tracker_file_get_mtime (dbs[i].abs_filename);

				loaded = TRUE;

#ifndef DISABLE_JOURNAL
				/* Report OPERATION - STATUS */
				busy_status = g_strdup_printf ("%s - %s",
				                               busy_operation,
				                               "Integrity checking");
				tracker_db_interface_set_busy_handler (dbs[i].iface,
				                                       busy_callback,
				                                       busy_status,
				                                       busy_user_data);
				g_free (busy_status);

				stmt = tracker_db_interface_create_statement (dbs[i].iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE,
				                                              &internal_error,
				                                              "PRAGMA integrity_check(1)");

				if (internal_error != NULL) {
					if (internal_error->domain == TRACKER_DB_INTERFACE_ERROR &&
					    internal_error->code == TRACKER_DB_QUERY_ERROR) {
						must_recreate = TRUE;
					} else {
						g_critical ("%s", internal_error->message);
					}
					g_error_free (internal_error);
					internal_error = NULL;
				} else {
					TrackerDBCursor *cursor = NULL;

					if (stmt) {
						cursor = tracker_db_statement_start_cursor (stmt, NULL);
						g_object_unref (stmt);
					} else {
						g_critical ("Can't create stmt for integrity_check, no error given");
					}

					if (cursor) {
						if (tracker_db_cursor_iter_next (cursor, NULL, NULL)) {
							if (g_strcmp0 (tracker_db_cursor_get_string (cursor, 0, NULL), "ok") != 0) {
								must_recreate = TRUE;
							}
						}
						g_object_unref (cursor);
					}
				}
#endif /* DISABLE_JOURNAL */

				/* ensure that database has been initialized by an earlier tracker-store start
				   by checking whether Resource table exists */
				stmt = tracker_db_interface_create_statement (dbs[i].iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE,
				                                              &internal_error,
				                                              "SELECT 1 FROM Resource");
				if (internal_error != NULL) {
					if (!restoring_backup) {
						must_recreate = TRUE;
						g_error_free (internal_error);
						internal_error = NULL;
					} else {
						continue;
					}
				} else {
					g_object_unref (stmt);
				}

				tracker_db_interface_set_busy_handler (dbs[i].iface, NULL, NULL, NULL);
			}
		}

		if (must_recreate) {
			g_message ("Database severely damaged. We will recreate it"
#ifndef DISABLE_JOURNAL
			           " and replay the journal if available.");
#else
			           ".");
#endif /* DISABLE_JOURNAL */

			perform_recreate (first_time, &internal_error);
			if (internal_error) {
				g_propagate_error (error, internal_error);
				return FALSE;
			}
			loaded = FALSE;
		} else {
			if (internal_error) {
				g_propagate_error (error, internal_error);
				return FALSE;
			}
		}
	}

	if (!loaded) {
		for (i = 1; i < G_N_ELEMENTS (dbs); i++) {
			dbs[i].mtime = tracker_file_get_mtime (dbs[i].abs_filename);
		}
	}

	if ((flags & TRACKER_DB_MANAGER_READONLY) == 0) {
		/* do not create in-use file for read-only mode (direct access) */
		in_use_file = g_open (in_use_filename,
			              O_WRONLY | O_APPEND | O_CREAT | O_SYNC,
			              S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);

		if (in_use_file >= 0) {
		        fsync (in_use_file);
		        close (in_use_file);
		}
	}

	initialized = TRUE;

	if (flags & TRACKER_DB_MANAGER_READONLY) {
		resources_iface = tracker_db_manager_get_db_interfaces_ro (&internal_error, 1,
		                                                           TRACKER_DB_METADATA);
		/* libtracker-direct does not use per-thread interfaces */
		global_iface = resources_iface;
	} else {
		resources_iface = tracker_db_manager_get_db_interfaces (&internal_error, 1,
		                                                        TRACKER_DB_METADATA);
	}

	if (internal_error) {
		if ((!restoring_backup) && (flags & TRACKER_DB_MANAGER_READONLY) == 0) {
			GError *new_error = NULL;

			perform_recreate (first_time, &new_error);
			if (!new_error) {
				resources_iface = tracker_db_manager_get_db_interfaces (&internal_error, 1,
				                                                        TRACKER_DB_METADATA);
			} else {
				/* Most serious error is the recreate one here */
				g_clear_error (&internal_error);
				g_propagate_error (error, new_error);
				initialized = FALSE;
				return FALSE;
			}
		} else {
			g_propagate_error (error, internal_error);
			initialized = FALSE;
			return FALSE;
		}
	}

	tracker_db_interface_set_max_stmt_cache_size (resources_iface,
	                                              TRACKER_DB_STATEMENT_CACHE_TYPE_SELECT,
	                                              select_cache_size);

	tracker_db_interface_set_max_stmt_cache_size (resources_iface,
	                                              TRACKER_DB_STATEMENT_CACHE_TYPE_UPDATE,
	                                              update_cache_size);

	s_cache_size = select_cache_size;
	u_cache_size = update_cache_size;

	if ((flags & TRACKER_DB_MANAGER_READONLY) == 0)
		g_private_replace (&interface_data_key, resources_iface);

	return TRUE;
}

void
tracker_db_manager_shutdown (void)
{
	guint i;

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

	if (global_iface) {
		/* libtracker-direct */
		g_object_unref (global_iface);
		global_iface = NULL;
	}

	/* shutdown db interface in all threads */
	g_private_replace (&interface_data_key, NULL);

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

	if ((tracker_db_manager_get_flags (NULL, NULL) & TRACKER_DB_MANAGER_READONLY) == 0) {
		/* do not delete in-use file for read-only mode (direct access) */
		g_unlink (in_use_filename);
	}

	g_free (in_use_filename);
	in_use_filename = NULL;
}

void
tracker_db_manager_remove_all (gboolean rm_journal)
{
	g_return_if_fail (initialized != FALSE);

	db_manager_remove_all (rm_journal);
}

void
tracker_db_manager_optimize (void)
{
	gboolean dbs_are_open = FALSE;
	TrackerDBInterface *iface;

	g_return_if_fail (initialized != FALSE);

	g_message ("Optimizing database...");

	g_message ("  Checking database is not in use");

	iface = tracker_db_manager_get_db_interface ();

	/* Check if any connections are open? */
	if (G_OBJECT (iface)->ref_count > 1) {
		g_message ("  database is still in use with %d references!",
		           G_OBJECT (iface)->ref_count);

		dbs_are_open = TRUE;
	}

	if (dbs_are_open) {
		g_message ("  Not optimizing database, still in use with > 1 reference");
		return;
	}

	/* Optimize the metadata database */
	db_manager_analyze (TRACKER_DB_METADATA, iface);
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
tracker_db_manager_get_db_interfaces (GError **error,
                                      gint num, ...)
{
	gint n_args;
	va_list args;
	TrackerDBInterface *connection = NULL;
	GError *internal_error = NULL;

	g_return_val_if_fail (initialized != FALSE, NULL);

	va_start (args, num);
	for (n_args = 1; n_args <= num; n_args++) {
		TrackerDB db = va_arg (args, TrackerDB);

		if (!connection) {
			connection = tracker_db_interface_sqlite_new (dbs[db].abs_filename,
			                                              &internal_error);

			if (internal_error) {
				g_propagate_error (error, internal_error);
				connection = NULL;
				goto end_on_error;
			}

			db_set_params (connection,
			               dbs[db].cache_size,
			               dbs[db].page_size,
			               &internal_error);

			if (internal_error) {
				g_propagate_error (error, internal_error);
				connection = NULL;
				goto end_on_error;
			}

		} else {
			db_exec_no_reply (connection,
			                  "ATTACH '%s' as '%s'",
			                  dbs[db].abs_filename,
			                  dbs[db].name);
		}

	}

	end_on_error:

	va_end (args);

	return connection;
}

static TrackerDBInterface *
tracker_db_manager_get_db_interfaces_ro (GError **error,
                                         gint num, ...)
{
	gint n_args;
	va_list args;
	TrackerDBInterface *connection = NULL;
	GError *internal_error = NULL;

	g_return_val_if_fail (initialized != FALSE, NULL);

	va_start (args, num);
	for (n_args = 1; n_args <= num; n_args++) {
		TrackerDB db = va_arg (args, TrackerDB);

		if (!connection) {
			connection = tracker_db_interface_sqlite_new_ro (dbs[db].abs_filename,
			                                                 &internal_error);

			if (internal_error) {
				g_propagate_error (error, internal_error);
				connection = NULL;
				goto end_on_error;
			}

			db_set_params (connection,
			               dbs[db].cache_size,
			               dbs[db].page_size,
			               &internal_error);

			if (internal_error) {
				g_propagate_error (error, internal_error);
				connection = NULL;
				goto end_on_error;
			}

		} else {
			db_exec_no_reply (connection,
			                  "ATTACH '%s' as '%s'",
			                  dbs[db].abs_filename,
			                  dbs[db].name);
		}
	}

	end_on_error:

	va_end (args);

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
tracker_db_manager_get_db_interface (void)
{
	GError *internal_error = NULL;
	TrackerDBInterface *interface;

	g_return_val_if_fail (initialized != FALSE, NULL);

	if (global_iface) {
		/* libtracker-direct */
		return global_iface;
	}

	interface = g_private_get (&interface_data_key);

	/* Ensure the interface is there */
	if (!interface) {
		interface = tracker_db_manager_get_db_interfaces (&internal_error, 1,
		                                                  TRACKER_DB_METADATA);

		if (internal_error) {
			g_critical ("Error opening database: %s", internal_error->message);
			g_error_free (internal_error);
			return NULL;
		}

		tracker_data_manager_init_fts (interface, FALSE);

		tracker_db_interface_set_max_stmt_cache_size (interface,
		                                              TRACKER_DB_STATEMENT_CACHE_TYPE_SELECT,
		                                              s_cache_size);

		tracker_db_interface_set_max_stmt_cache_size (interface,
		                                              TRACKER_DB_STATEMENT_CACHE_TYPE_UPDATE,
		                                              u_cache_size);

		g_private_set (&interface_data_key, interface);
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


inline static gchar *
get_first_index_filename (void)
{
	return g_build_filename (g_get_user_cache_dir (),
	                         "tracker",
	                         FIRST_INDEX_FILENAME,
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
	gchar *filename;

	filename = get_first_index_filename ();
	exists = g_file_test (filename, G_FILE_TEST_EXISTS);
	g_free (filename);

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
	gchar *filename;

	filename = get_first_index_filename ();
	already_exists = g_file_test (filename, G_FILE_TEST_EXISTS);

	if (done && !already_exists) {
		GError *error = NULL;

		/* If done, create stamp file if not already there */
		if (!g_file_set_contents (filename, PACKAGE_VERSION, -1, &error)) {
			g_warning ("  Could not create file:'%s' failed, %s",
			           filename,
			           error->message);
			g_error_free (error);
		} else {
			g_message ("  First index file:'%s' created",
			           filename);
		}
	} else if (!done && already_exists) {
		/* If NOT done, remove stamp file */
		g_message ("  Removing first index file:'%s'", filename);

		if (g_remove (filename)) {
			g_warning ("    Could not remove file:'%s', %s",
			           filename,
			           g_strerror (errno));
		}
	}

	g_free (filename);
}

inline static gchar *
get_last_crawl_filename (void)
{
	return g_build_filename (g_get_user_cache_dir (),
	                         "tracker",
	                         LAST_CRAWL_FILENAME,
	                         NULL);
}

/**
 * tracker_db_manager_get_last_crawl_done:
 *
 * Check when last crawl was performed.
 *
 * Returns: time_t() value when last crawl occurred, otherwise 0.
 **/
guint64
tracker_db_manager_get_last_crawl_done (void)
{
	gchar *filename;
	gchar *content;
	guint64 then;

	filename = get_last_crawl_filename ();

	if (!g_file_get_contents (filename, &content, NULL, NULL)) {
		g_message ("  No previous timestamp, crawling forced");
		return 0;
	}

	then = g_ascii_strtoull (content, NULL, 10);
	g_free (content);

	return then;
}

/**
 * tracker_db_manager_set_last_crawl_done:
 *
 * Set the status of the first full index of files. Should be set to
 *  %FALSE if the index was never done or if a reindex is needed. When
 *  the index is completed, should be set to %TRUE.
 **/
void
tracker_db_manager_set_last_crawl_done (gboolean done)
{
	gboolean already_exists;
	gchar *filename;

	filename = get_last_crawl_filename ();
	already_exists = g_file_test (filename, G_FILE_TEST_EXISTS);

	if (done && !already_exists) {
		GError *error = NULL;
		gchar *content;

		content = g_strdup_printf ("%" G_GUINT64_FORMAT, (guint64) time (NULL));

		/* If done, create stamp file if not already there */
		if (!g_file_set_contents (filename, content, -1, &error)) {
			g_warning ("  Could not create file:'%s' failed, %s",
			           filename,
			           error->message);
			g_error_free (error);
		} else {
			g_message ("  Last crawl file:'%s' created",
			           filename);
		}

		g_free (content);
	} else if (!done && already_exists) {
		/* If NOT done, remove stamp file */
		g_message ("  Removing last crawl file:'%s'", filename);

		if (g_remove (filename)) {
			g_warning ("    Could not remove file:'%s', %s",
			           filename,
			           g_strerror (errno));
		}
	}

	g_free (filename);
}

inline static gchar *
get_need_mtime_check_filename (void)
{
	return g_build_filename (g_get_user_cache_dir (),
	                         "tracker",
	                         NEED_MTIME_CHECK_FILENAME,
	                         NULL);
}

/**
 * tracker_db_manager_get_need_mtime_check:
 *
 * Check if the miner-fs was cleanly shutdown or not.
 *
 * Returns: %TRUE if we need to check mtimes for directories against
 * the database on the next start for the miner-fs, %FALSE otherwise.
 *
 * Since: 0.10
 **/
gboolean
tracker_db_manager_get_need_mtime_check (void)
{
	gboolean exists;
	gchar *filename;

	filename = get_need_mtime_check_filename ();
	exists = g_file_test (filename, G_FILE_TEST_EXISTS);
	g_free (filename);

	/* Existence of the file means we cleanly shutdown before and
	 * don't need to do the mtime check again on this start.
	 */
	return !exists;
}

/**
 * tracker_db_manager_set_need_mtime_check:
 * @needed: a #gboolean
 *
 * If the next start of miner-fs should perform a full mtime check
 * against each directory found and those in the database (for
 * complete synchronisation), then @needed should be #TRUE, otherwise
 * #FALSE.
 *
 * Creates a file in $HOME/.cache/tracker/ if an mtime check is not
 * needed. The idea behind this is that a check is forced if the file
 * is not cleaned up properly on shutdown (i.e. due to a crash or any
 * other uncontrolled shutdown reason).
 *
 * Since: 0.10
 **/
void
tracker_db_manager_set_need_mtime_check (gboolean needed)
{
	gboolean already_exists;
	gchar *filename;

	filename = get_need_mtime_check_filename ();
	already_exists = g_file_test (filename, G_FILE_TEST_EXISTS);

	/* !needed = add file
	 *  needed = remove file
	 */
	if (!needed && !already_exists) {
		GError *error = NULL;

		/* Create stamp file if not already there */
		if (!g_file_set_contents (filename, PACKAGE_VERSION, -1, &error)) {
			g_warning ("  Could not create file:'%s' failed, %s",
			           filename,
			           error->message);
			g_error_free (error);
		} else {
			g_message ("  Need mtime check file:'%s' created",
			           filename);
		}
	} else if (needed && already_exists) {
		/* Remove stamp file */
		g_message ("  Removing need mtime check file:'%s'", filename);

		if (g_remove (filename)) {
			g_warning ("    Could not remove file:'%s', %s",
			           filename,
			           g_strerror (errno));
		}
	}

	g_free (filename);
}

void
tracker_db_manager_lock (void)
{
	g_mutex_lock (&global_mutex);
}

gboolean
tracker_db_manager_trylock (void)
{
	return g_mutex_trylock (&global_mutex);
}

void
tracker_db_manager_unlock (void)
{
	g_mutex_unlock (&global_mutex);
}

inline static gchar *
get_parser_sha1_filename (void)
{
	return g_build_filename (g_get_user_cache_dir (),
	                         "tracker",
	                         PARSER_SHA1_FILENAME,
	                         NULL);
}


gboolean
tracker_db_manager_get_tokenizer_changed (void)
{
	gchar *filename, *sha1;
	gboolean changed = TRUE;

	filename = get_parser_sha1_filename ();

	if (g_file_get_contents (filename, &sha1, NULL, NULL)) {
		changed = strcmp (sha1, TRACKER_PARSER_SHA1) != 0;
		g_free (sha1);
	}

	g_free (filename);

	return changed;
}

void
tracker_db_manager_tokenizer_update (void)
{
	GError *error = NULL;
	gchar *filename;

	filename = get_parser_sha1_filename ();

	if (!g_file_set_contents (filename, TRACKER_PARSER_SHA1, -1, &error)) {
		g_warning ("The file '%s' could not be rewritten by Tracker and "
		           "should be deleted manually. Not doing so will result "
		           "in Tracker rebuilding its FTS tokens on every startup. "
		           "The error received was: '%s'", filename, error->message);
		g_error_free (error);
	}

	g_free (filename);
}
