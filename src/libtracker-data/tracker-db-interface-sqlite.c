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

#include <glib/gstdio.h>

#include <sqlite3.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>

#include <libtracker-common/tracker-common.h>

#if HAVE_TRACKER_FTS
#include <libtracker-fts/tracker-fts.h>
#endif

#include "tracker-db-interface-sqlite.h"

struct TrackerDBInterface {
	GObject parent_instance;

	gchar *filename;
	sqlite3 *db;

	GHashTable *dynamic_statements;

	GSList *function_data;

	guint ro : 1;
#if HAVE_TRACKER_FTS
	guint fts_initialized : 1;
#endif
	GCancellable *cancellable;
};

struct TrackerDBInterfaceClass {
	GObjectClass parent_class;
};

struct TrackerDBCursor {
	GObject parent_instance;
	sqlite3_stmt *stmt;
	TrackerDBStatement *ref_stmt;
	gboolean finished;
};

struct TrackerDBCursorClass {
	GObjectClass parent_class;
};

struct TrackerDBStatement {
	GObject parent_instance;
	TrackerDBInterface *db_interface;
	sqlite3_stmt *stmt;
	gboolean stmt_is_sunk;
};

struct TrackerDBStatementClass {
	GObjectClass parent_class;
};

static TrackerDBStatement       * tracker_db_statement_sqlite_new (TrackerDBInterface     *db_interface,
                                                                   sqlite3_stmt           *sqlite_stmt);
static TrackerDBCursor          * tracker_db_cursor_sqlite_new    (sqlite3_stmt           *sqlite_stmt,
                                                                   TrackerDBStatement     *ref_stmt);
static void tracker_db_statement_sqlite_reset (TrackerDBStatement *stmt);

enum {
	PROP_0,
	PROP_FILENAME,
	PROP_RO
};

G_DEFINE_TYPE (TrackerDBInterface, tracker_db_interface, G_TYPE_OBJECT)

G_DEFINE_TYPE (TrackerDBStatement, tracker_db_statement, G_TYPE_OBJECT)

G_DEFINE_TYPE (TrackerDBCursor, tracker_db_cursor, G_TYPE_OBJECT)

void
tracker_db_interface_sqlite_enable_shared_cache (void)
{
	sqlite3_config (SQLITE_CONFIG_MULTITHREAD);
	sqlite3_enable_shared_cache (1);
}

static void
function_sparql_string_join (sqlite3_context *context,
                             int              argc,
                             sqlite3_value   *argv[])
{
	GString *str = NULL;
	const gchar *separator;
	gint i;

	/* fn:string-join (str1, str2, ..., separator) */

	if (sqlite3_value_type (argv[argc-1]) != SQLITE_TEXT) {
		sqlite3_result_error (context, "Invalid separator", -1);
		return;
	}

	separator = sqlite3_value_text (argv[argc-1]);

	for (i = 0;i < argc-1; i++) {
		if (sqlite3_value_type (argv[argc-1]) == SQLITE_TEXT) {
			const gchar *text = sqlite3_value_text (argv[i]);

			if (text != NULL) {
				if (!str) {
					str = g_string_new (text);
				} else {
					g_string_append_printf (str, "%s%s", separator, text);
				}
			}
		}
	}

	if (str) {
		sqlite3_result_text (context, str->str, str->len, g_free);
		g_string_free (str, FALSE);
	} else {
		sqlite3_result_null (context);
	}

	return;
}

/* Create a title-type string from the filename for replacing missing ones */
static void
function_sparql_string_from_filename (sqlite3_context *context,
                                      int              argc,
                                      sqlite3_value   *argv[])
{
	gchar  *name = NULL;
	gchar  *suffix = NULL;

	if (argc != 1) {
		sqlite3_result_error (context, "Invalid argument count", -1);
		return;
	}

	/* "/home/user/path/title_of_the_movie.movie" -> "title of the movie"
	 * Only for local files currently, do we need to change? */

	name = g_filename_display_basename (sqlite3_value_text (argv[0]));

	if (!name) {
		sqlite3_result_null (context);
		return;
	}

	suffix = g_strrstr (name, ".");

	if (suffix) {
		*suffix = '\0';
	}

	g_strdelimit (name, "._", ' ');

	sqlite3_result_text (context, name, -1, g_free);
}

static void
function_sparql_uri_is_parent (sqlite3_context *context,
                               int              argc,
                               sqlite3_value   *argv[])
{
	const gchar *uri, *parent, *remaining;
	gboolean match = FALSE;
	guint parent_len;

	if (argc != 2) {
		sqlite3_result_error (context, "Invalid argument count", -1);
		return;
	}

	parent = sqlite3_value_text (argv[0]);
	uri = sqlite3_value_text (argv[1]);

	if (!parent || !uri) {
		sqlite3_result_error (context, "Invalid arguments", -1);
		return;
	}

	parent_len = sqlite3_value_bytes (argv[0]);

	/* Check only one argument, it's going to
	 * be compared with the other anyway.
	 */

	if (!(parent_len >= 7 && (parent[4] == ':' && parent[5] == '/' && parent[6] == '/'))) {
		if (strstr (parent, "://") == NULL) {
			sqlite3_result_int (context, FALSE);
			return;
		}
	}

	/* Remove trailing '/', will
	 * be checked later on uri.
	 */
	while (parent[parent_len - 1] == '/') {
		parent_len--;
	}

	if (strncmp (uri, parent, parent_len) == 0 && uri[parent_len] == '/') {
		const gchar *slash;

		while (uri[parent_len] == '/') {
			parent_len++;
		}

		remaining = &uri[parent_len];

		if (*remaining == '\0') {
			/* Exact match, not a child */
			match = FALSE;
		} else if ((slash = strchr (remaining, '/')) == NULL) {
			/* Remaining doesn't have uri
			 * separator, it's a direct child.
			 */
			match = TRUE;
		} else {
			/* Check it's not trailing slashes */
			while (*slash == '/') {
				slash++;
			}

			match = (*slash == '\0');
		}
	}

	sqlite3_result_int (context, match);
}

static void
function_sparql_uri_is_descendant (sqlite3_context *context,
                                   int              argc,
                                   sqlite3_value   *argv[])
{
	const gchar *uri, *parent, *remaining;
	gboolean match = FALSE;
	guint parent_len;

	if (argc != 2) {
		sqlite3_result_error (context, "Invalid argument count", -1);
		return;
	}

	parent = sqlite3_value_text (argv[0]);
	uri = sqlite3_value_text (argv[1]);

	if (!parent || !uri) {
		sqlite3_result_error (context, "Invalid arguments", -1);
		return;
	}

	parent_len = sqlite3_value_bytes (argv[0]);

	/* Check only one argument, it's going to
	 * be compared with the other anyway.
	 */

	if (!(parent_len >= 7 && (parent[4] == ':' && parent[5] == '/' && parent[6] == '/'))) {
		if (strstr (parent, "://") == NULL) {
			sqlite3_result_int (context, FALSE);
			return;
		}
	}

	/* Remove trailing '/', will
	 * be checked later on uri.
	 */
	while (parent[parent_len - 1] == '/') {
		parent_len--;
	}

	if (strncmp (uri, parent, parent_len) == 0 && uri[parent_len] == '/') {
		while (uri[parent_len] == '/') {
			parent_len++;
		}

		remaining = &uri[parent_len];

		if (remaining && *remaining) {
			match = TRUE;
		}
	}

	sqlite3_result_int (context, match);
}

static void
function_sparql_cartesian_distance (sqlite3_context *context,
                                    int              argc,
                                    sqlite3_value   *argv[])
{
	gdouble lat1;
	gdouble lat2;
	gdouble lon1;
	gdouble lon2;

	gdouble R;
	gdouble a;
	gdouble b;
	gdouble c;
	gdouble d;

	if (argc != 4) {
		sqlite3_result_error (context, "Invalid argument count", -1);
		return;
	}

	lat1 = sqlite3_value_double (argv[0])*M_PI/180;
	lat2 = sqlite3_value_double (argv[1])*M_PI/180;
	lon1 = sqlite3_value_double (argv[2])*M_PI/180;
	lon2 = sqlite3_value_double (argv[3])*M_PI/180;

	R = 6371000;
	a = M_PI/2 - lat1;
	b = M_PI/2 - lat2;
	c = sqrt(a*a + b*b - 2*a*b*cos(lon2 - lon1));
	d = R*c;

	sqlite3_result_double (context, d);
}

static void
function_sparql_haversine_distance (sqlite3_context *context,
                                    int              argc,
                                    sqlite3_value   *argv[])
{
	gdouble lat1;
	gdouble lat2;
	gdouble lon1;
	gdouble lon2;

	gdouble R;
	gdouble dLat;
	gdouble dLon;
	gdouble a;
	gdouble c;
	gdouble d;

	if (argc != 4) {
		sqlite3_result_error (context, "Invalid argument count", -1);
		return;
	}

	lat1 = sqlite3_value_double (argv[0])*M_PI/180;
	lat2 = sqlite3_value_double (argv[1])*M_PI/180;
	lon1 = sqlite3_value_double (argv[2])*M_PI/180;
	lon2 = sqlite3_value_double (argv[3])*M_PI/180;

	R = 6371000;
	dLat = (lat2-lat1);
	dLon = (lon2-lon1);
	a = sin(dLat/2) * sin(dLat/2) + cos(lat1) * cos(lat2) *  sin(dLon/2) * sin(dLon/2);
	c = 2 * atan2(sqrt(a), sqrt(1-a));
	d = R * c;

	sqlite3_result_double (context, d);
}

static void
function_sparql_regex (sqlite3_context *context,
                       int              argc,
                       sqlite3_value   *argv[])
{
	gboolean ret;
	const gchar *text, *pattern, *flags;
	GRegexCompileFlags regex_flags;
	GRegex *regex;

	if (argc != 3) {
		sqlite3_result_error (context, "Invalid argument count", -1);
		return;
	}

	regex = sqlite3_get_auxdata (context, 1);

	text = sqlite3_value_text (argv[0]);
	flags = sqlite3_value_text (argv[2]);

	if (regex == NULL) {
		gchar *err_str;
		GError *error = NULL;

		pattern = sqlite3_value_text (argv[1]);

		regex_flags = 0;
		while (*flags) {
			switch (*flags) {
			case 's':
				regex_flags |= G_REGEX_DOTALL;
				break;
			case 'm':
				regex_flags |= G_REGEX_MULTILINE;
				break;
			case 'i':
				regex_flags |= G_REGEX_CASELESS;
				break;
			case 'x':
				regex_flags |= G_REGEX_EXTENDED;
				break;
			default:
				err_str = g_strdup_printf ("Invalid SPARQL regex flag '%c'", *flags);
				sqlite3_result_error (context, err_str, -1);
				g_free (err_str);
				return;
			}
			flags++;
		}

		regex = g_regex_new (pattern, regex_flags, 0, &error);

		if (error) {
			sqlite3_result_error (context, error->message, -1);
			g_clear_error (&error);
			return;
		}

		sqlite3_set_auxdata (context, 1, regex, (void (*) (void*)) g_regex_unref);
	}

	ret = g_regex_match (regex, text, 0, NULL);

	sqlite3_result_int (context, ret);
}

static inline int
stmt_step (sqlite3_stmt *stmt)
{
	int result;

	result = sqlite3_step (stmt);

	/* If the statement expired between preparing it and executing
	 * sqlite3_step(), we are supposed to get SQLITE_SCHEMA error in
	 * sqlite3_errcode(), BUT there seems to be a bug in sqlite and
	 * SQLITE_ABORT is being returned instead for that case. So, the
	 * only way to see if a given statement was expired is to use
	 * sqlite3_expired(stmt), which is marked as DEPRECATED in sqlite.
	 * If found that the statement is expired, we need to reset it
	 * and retry the sqlite3_step().
	 * NOTE, that this expiration may only happen between preparing
	 * the statement and step-ing it, NOT between steps. */
	if ((result == SQLITE_ABORT || result == SQLITE_SCHEMA) &&
	    sqlite3_expired (stmt)) {
		sqlite3_reset (stmt);
		result = sqlite3_step (stmt);
	}

	return result;
}

static int
check_interrupt (void *user_data)
{
	TrackerDBInterface *db_interface = user_data;
	return g_cancellable_is_cancelled (db_interface->cancellable) ? 1 : 0;
}

static void
open_database (TrackerDBInterface *db_interface)
{
	g_assert (db_interface->filename != NULL);

	if (!db_interface->ro) {
		if (sqlite3_open (db_interface->filename, &db_interface->db) != SQLITE_OK) {
			g_critical ("Could not open sqlite3 database:'%s'", db_interface->filename);
		} else {
			g_message ("Opened sqlite3 database:'%s'", db_interface->filename);
		}
	} else {
		if (sqlite3_open_v2 (db_interface->filename, &db_interface->db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
			g_critical ("Could not open sqlite3 database:'%s'", db_interface->filename);
		} else {
			g_message ("Opened sqlite3 database:'%s'", db_interface->filename);
		}
	}

	sqlite3_progress_handler (db_interface->db, 100,
	                          check_interrupt, db_interface);

	sqlite3_create_function (db_interface->db, "SparqlRegex", 3, SQLITE_ANY,
	                         db_interface, &function_sparql_regex,
	                         NULL, NULL);

	sqlite3_create_function (db_interface->db, "SparqlHaversineDistance", 4, SQLITE_ANY,
	                         db_interface, &function_sparql_haversine_distance,
	                         NULL, NULL);

	sqlite3_create_function (db_interface->db, "SparqlCartesianDistance", 4, SQLITE_ANY,
	                         db_interface, &function_sparql_cartesian_distance,
	                         NULL, NULL);

	sqlite3_create_function (db_interface->db, "SparqlStringFromFilename", 1, SQLITE_ANY,
	                         db_interface, &function_sparql_string_from_filename,
	                         NULL, NULL);

	sqlite3_create_function (db_interface->db, "SparqlStringJoin", -1, SQLITE_ANY,
	                         db_interface, &function_sparql_string_join,
	                         NULL, NULL);

	sqlite3_create_function (db_interface->db, "SparqlUriIsParent", 2, SQLITE_ANY,
	                         db_interface, &function_sparql_uri_is_parent,
	                         NULL, NULL);

	sqlite3_create_function (db_interface->db, "SparqlUriIsDescendant", 2, SQLITE_ANY,
	                         db_interface, &function_sparql_uri_is_descendant,
	                         NULL, NULL);

	sqlite3_extended_result_codes (db_interface->db, 0);
	sqlite3_busy_timeout (db_interface->db, 100000);
}

static GObject *
tracker_db_interface_sqlite_constructor (GType                  type,
                                         guint                  n_construct_properties,
                                         GObjectConstructParam *construct_params)
{
	GObject *object;
	TrackerDBInterface *db_iface;

	object = (* G_OBJECT_CLASS (tracker_db_interface_parent_class)->constructor) (type,
		               n_construct_properties,
		               construct_params);
	db_iface = TRACKER_DB_INTERFACE (object);

	open_database (db_iface);

	return object;
}

static void
tracker_db_interface_sqlite_set_property (GObject       *object,
                                          guint          prop_id,
                                          const GValue  *value,
                                          GParamSpec    *pspec)
{
	TrackerDBInterface *db_iface;

	db_iface = TRACKER_DB_INTERFACE (object);

	switch (prop_id) {
	case PROP_RO:
		db_iface->ro = g_value_get_boolean (value);
		break;
	case PROP_FILENAME:
		db_iface->filename = g_value_dup_string (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
tracker_db_interface_sqlite_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
	TrackerDBInterface *db_iface;

	db_iface = TRACKER_DB_INTERFACE (object);

	switch (prop_id) {
	case PROP_RO:
		g_value_set_boolean (value, db_iface->ro);
		break;
	case PROP_FILENAME:
		g_value_set_string (value, db_iface->filename);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
close_database (TrackerDBInterface *db_interface)
{
	gint rc;

	g_hash_table_unref (db_interface->dynamic_statements);
	db_interface->dynamic_statements = NULL;

	g_slist_foreach (db_interface->function_data, (GFunc) g_free, NULL);
	g_slist_free (db_interface->function_data);
	db_interface->function_data = NULL;

#if HAVE_TRACKER_FTS
	if (db_interface->fts_initialized) {
		tracker_fts_shutdown (G_OBJECT (db_interface));
	}
#endif

	rc = sqlite3_close (db_interface->db);
	g_warn_if_fail (rc == SQLITE_OK);
}

void
tracker_db_interface_sqlite_fts_init (TrackerDBInterface *db_interface,
                                      gboolean            create)
{
#if HAVE_TRACKER_FTS
	tracker_fts_init (db_interface->db, create, G_OBJECT (db_interface));
	db_interface->fts_initialized = TRUE;
#else
	g_message ("FTS support is disabled");
#endif
}

static void
tracker_db_interface_sqlite_finalize (GObject *object)
{
	TrackerDBInterface *db_interface;

	db_interface = TRACKER_DB_INTERFACE (object);

	close_database (db_interface);

	g_message ("Closed sqlite3 database:'%s'", db_interface->filename);

	g_free (db_interface->filename);

	G_OBJECT_CLASS (tracker_db_interface_parent_class)->finalize (object);
}

static void
tracker_db_interface_class_init (TrackerDBInterfaceClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->constructor = tracker_db_interface_sqlite_constructor;
	object_class->set_property = tracker_db_interface_sqlite_set_property;
	object_class->get_property = tracker_db_interface_sqlite_get_property;
	object_class->finalize = tracker_db_interface_sqlite_finalize;

	g_object_class_install_property (object_class,
	                                 PROP_FILENAME,
	                                 g_param_spec_string ("filename",
	                                                      "DB filename",
	                                                      "DB filename",
	                                                      NULL,
	                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (object_class,
	                                 PROP_RO,
	                                 g_param_spec_boolean ("read-only",
	                                                       "Read only",
	                                                       "Whether the connection is read only",
	                                                       FALSE,
	                                                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
prepare_database (TrackerDBInterface *db_interface)
{
	db_interface->dynamic_statements = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                                          NULL,
	                                                          (GDestroyNotify) g_object_unref);
}

static void
tracker_db_interface_init (TrackerDBInterface *db_interface)
{
	db_interface->ro = FALSE;

	prepare_database (db_interface);
}

static TrackerDBResultSet *
ensure_result_set_state (TrackerDBResultSet *result_set)
{
	if (!result_set)
		return NULL;

	if (tracker_db_result_set_get_n_rows (result_set) == 0) {
		g_object_unref (result_set);
		return NULL;
	}

	/* ensure that it's at the first item */
	tracker_db_result_set_rewind (result_set);

	return result_set;
}

static void
add_row (TrackerDBResultSet *result_set,
         sqlite3_stmt       *stmt)
{
	gint cols, i;

	cols = sqlite3_column_count (stmt);
	_tracker_db_result_set_append (result_set);

	for (i = 0; i < cols; i++) {
		GValue value = { 0, };
		gint col_type;

		col_type = sqlite3_column_type (stmt, i);

		switch (col_type) {
		case SQLITE_TEXT:
			g_value_init (&value, G_TYPE_STRING);
			g_value_set_string (&value, (gchar *) sqlite3_column_text (stmt, i));
			break;
		case SQLITE_INTEGER:
			g_value_init (&value, G_TYPE_INT64);
			g_value_set_int64 (&value, sqlite3_column_int64 (stmt, i));
			break;
		case SQLITE_FLOAT:
			g_value_init (&value, G_TYPE_DOUBLE);
			g_value_set_double (&value, sqlite3_column_double (stmt, i));
			break;
		case SQLITE_NULL:
			/* just ignore NULLs */
			break;
		default:
			g_critical ("Unknown sqlite3 database column type:%d", col_type);
		}

		if (G_VALUE_TYPE (&value) != G_TYPE_INVALID) {
			_tracker_db_result_set_set_value (result_set, i, &value);
			g_value_unset (&value);
		}
	}
}


TrackerDBStatement *
tracker_db_interface_create_statement (TrackerDBInterface  *db_interface,
                                       GError             **error,
                                       const gchar         *query,
                                       ...)
{
	TrackerDBStatement *stmt;
	va_list args;
	gchar *full_query;

	g_return_val_if_fail (TRACKER_IS_DB_INTERFACE (db_interface), NULL);

	va_start (args, query);
	full_query = g_strdup_vprintf (query, args);
	va_end (args);

	stmt = g_hash_table_lookup (db_interface->dynamic_statements, full_query);

	if (!stmt) {
		sqlite3_stmt *sqlite_stmt;
		int retval;

		g_debug ("Preparing query: '%s'", full_query);

		retval = sqlite3_prepare_v2 (db_interface->db, full_query, -1, &sqlite_stmt, NULL);

		if (retval != SQLITE_OK) {

			if (retval == SQLITE_INTERRUPT) {
				g_set_error (error,
				             TRACKER_DB_INTERFACE_ERROR,
				             TRACKER_DB_INTERRUPTED,
				             "Interrupted");
			} else {
				g_set_error (error,
				             TRACKER_DB_INTERFACE_ERROR,
				             TRACKER_DB_QUERY_ERROR,
				             "%s",
				             sqlite3_errmsg (db_interface->db));
			}

			g_free (full_query);

			return NULL;
		}

		stmt = tracker_db_statement_sqlite_new (db_interface, sqlite_stmt);

		g_hash_table_insert (db_interface->dynamic_statements,
		                     (gpointer) sqlite3_sql (sqlite_stmt),
		                     stmt);
	} else {
		tracker_db_statement_sqlite_reset (stmt);
	}

	g_free (full_query);

	return g_object_ref (stmt);
}

static TrackerDBResultSet *
create_result_set_from_stmt (TrackerDBInterface  *interface,
                             sqlite3_stmt        *stmt,
                             GCancellable        *cancellable,
                             GError             **error)
{
	TrackerDBResultSet *result_set = NULL;
	gint columns, result;

	columns = sqlite3_column_count (stmt);
	result = SQLITE_OK;

	while (result == SQLITE_OK  ||
	       result == SQLITE_ROW) {

		if (g_cancellable_is_cancelled (cancellable)) {
			result = SQLITE_INTERRUPT;
			sqlite3_reset (stmt);
		} else {
			/* only one statement can be active at the same time per interface */
			interface->cancellable = cancellable;
			result = stmt_step (stmt);

			interface->cancellable = NULL;
		}

		switch (result) {
		case SQLITE_ERROR:
			sqlite3_reset (stmt);
			break;
		case SQLITE_ROW:
			if (G_UNLIKELY (!result_set)) {
				result_set = _tracker_db_result_set_new (columns);
			}

			add_row (result_set, stmt);
			break;
		default:
			break;
		}
	}

	if (result != SQLITE_DONE) {

		/* This is rather fatal */
		if (sqlite3_errcode (interface->db) == SQLITE_IOERR ||
		    sqlite3_errcode (interface->db) == SQLITE_CORRUPT ||
		    sqlite3_errcode (interface->db) == SQLITE_NOTADB) {

			g_critical ("SQLite error: %s (errno: %s)",
			            sqlite3_errmsg (interface->db),
			            g_strerror (errno));

			sqlite3_finalize (stmt);
			sqlite3_close (interface->db);

			g_unlink (interface->filename);

			g_error ("SQLite experienced an error with file:'%s'. "
			         "It is either NOT a SQLite database or it is "
			         "corrupt or there was an IO error accessing the data. "
			         "This file has now been removed and will be recreated on the next start. "
			         "Shutting down now.",
			         interface->filename);

			return NULL;
		}

		if (!error) {
			g_warning ("Could not perform SQLite operation, error:%d->'%s'",
			           sqlite3_errcode (interface->db),
			           sqlite3_errmsg (interface->db));
		} else {
			if (result == SQLITE_INTERRUPT) {
				g_set_error (error,
				             TRACKER_DB_INTERFACE_ERROR,
				             TRACKER_DB_INTERRUPTED,
				             "Interrupted");
			} else {
				g_set_error (error,
				             TRACKER_DB_INTERFACE_ERROR,
				             TRACKER_DB_QUERY_ERROR,
				             "%s",
				             sqlite3_errmsg (interface->db));
			}
		}

		/* If there was an error, result set may be invalid or incomplete */
		if (result_set) {
			g_object_unref (result_set);
		}

		return NULL;
	}

	return ensure_result_set_state (result_set);
}

TrackerDBResultSet *
tracker_db_interface_execute_vquery (TrackerDBInterface  *db_interface,
                                     GError             **error,
                                     const gchar         *query,
                                     va_list              args)
{
	gchar *full_query;
	TrackerDBResultSet *result_set;
	sqlite3_stmt *stmt;
	int retval;

	full_query = g_strdup_vprintf (query, args);

	/* g_debug ("Running query: '%s'", full_query); */
	retval = sqlite3_prepare_v2 (db_interface->db, full_query, -1, &stmt, NULL);

	if (retval != SQLITE_OK) {
		g_set_error (error,
		             TRACKER_DB_INTERFACE_ERROR,
		             TRACKER_DB_QUERY_ERROR,
		             "%s",
		             sqlite3_errmsg (db_interface->db));
		g_free (full_query);
		return NULL;
	} else if (stmt == NULL) {
		g_set_error (error,
		             TRACKER_DB_INTERFACE_ERROR,
		             TRACKER_DB_QUERY_ERROR,
		             "Could not prepare SQL statement:'%s'",
		             full_query);

		g_free (full_query);
		return NULL;
	}

	result_set = create_result_set_from_stmt (db_interface, stmt, NULL, error);
	sqlite3_finalize (stmt);

	g_free (full_query);
	return result_set;
}

TrackerDBInterface *
tracker_db_interface_sqlite_new (const gchar *filename)
{
	return g_object_new (TRACKER_TYPE_DB_INTERFACE,
	                     "filename", filename,
	                     NULL);
}

TrackerDBInterface *
tracker_db_interface_sqlite_new_ro (const gchar *filename)
{
	return g_object_new (TRACKER_TYPE_DB_INTERFACE,
	                     "filename", filename,
	                     "read-only", TRUE,
	                     NULL);
}

static gint
collation_function (gpointer      data,
                    int           len1,
                    gconstpointer str1,
                    int           len2,
                    gconstpointer str2)
{
	TrackerDBCollationFunc func;

	func = (TrackerDBCollationFunc) data;

	return (func) ((gchar *) str1, len1, (gchar *) str2, len2);
}

gboolean
tracker_db_interface_sqlite_set_collation_function (TrackerDBInterface       *interface,
                                                    const gchar              *name,
                                                    TrackerDBCollationFunc    func)
{
	gint result;

	g_return_val_if_fail (TRACKER_IS_DB_INTERFACE (interface), FALSE);

	result = sqlite3_create_collation (interface->db, name, SQLITE_UTF8, func, &collation_function);

	return (result == SQLITE_OK);
}

gint64
tracker_db_interface_sqlite_get_last_insert_id (TrackerDBInterface *interface)
{
	g_return_val_if_fail (TRACKER_IS_DB_INTERFACE (interface), 0);

	return (gint64) sqlite3_last_insert_rowid (interface->db);
}

static void
tracker_db_statement_finalize (GObject *object)
{
	TrackerDBStatement *stmt;

	stmt = TRACKER_DB_STATEMENT (object);

	/* A cursor was still open while we're being finalized, because a cursor
	 * holds its own reference, this means that somebody is unreffing a stmt
	 * too often. We mustn't sqlite3_finalize the priv->stmt in this case,
	 * though. It would crash&burn the cursor. */

	g_assert (!stmt->stmt_is_sunk);

	sqlite3_finalize (stmt->stmt);

	G_OBJECT_CLASS (tracker_db_statement_parent_class)->finalize (object);
}

static void
tracker_db_statement_class_init (TrackerDBStatementClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->finalize = tracker_db_statement_finalize;
}

static TrackerDBStatement *
tracker_db_statement_sqlite_new (TrackerDBInterface       *db_interface,
                                 sqlite3_stmt             *sqlite_stmt)
{
	TrackerDBStatement *stmt;

	stmt = g_object_new (TRACKER_TYPE_DB_STATEMENT, NULL);

	stmt->db_interface = db_interface;
	stmt->stmt = sqlite_stmt;
	stmt->stmt_is_sunk = FALSE;

	return stmt;
}

static void
tracker_db_cursor_finalize (GObject *object)
{
	TrackerDBCursor *cursor;

	cursor = TRACKER_DB_CURSOR (object);

	if (cursor->ref_stmt) {
		cursor->ref_stmt->stmt_is_sunk = FALSE;
		tracker_db_statement_sqlite_reset (cursor->ref_stmt);
		g_object_unref (cursor->ref_stmt);
	} else {
		sqlite3_finalize (cursor->stmt);
	}

	G_OBJECT_CLASS (tracker_db_cursor_parent_class)->finalize (object);
}

static void
tracker_db_cursor_class_init (TrackerDBCursorClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->finalize = tracker_db_cursor_finalize;
}

static TrackerDBCursor *
tracker_db_cursor_sqlite_new (sqlite3_stmt              *sqlite_stmt,
                              TrackerDBStatement        *ref_stmt)
{
	TrackerDBCursor *cursor;

	cursor = g_object_new (TRACKER_TYPE_DB_CURSOR, NULL);

	cursor->stmt = sqlite_stmt;
	cursor->finished = FALSE;

	if (ref_stmt) {
		ref_stmt->stmt_is_sunk = TRUE;
		cursor->ref_stmt = g_object_ref (ref_stmt);
	} else {
		cursor->ref_stmt = NULL;
	}

	return cursor;
}

void
tracker_db_statement_bind_double (TrackerDBStatement      *stmt,
                                  int                      index,
                                  double                   value)
{
	g_return_if_fail (TRACKER_IS_DB_STATEMENT (stmt));

	g_assert (!stmt->stmt_is_sunk);

	sqlite3_bind_double (stmt->stmt, index + 1, value);
}

void
tracker_db_statement_bind_int (TrackerDBStatement       *stmt,
                               int                       index,
                               gint64                    value)
{
	g_return_if_fail (TRACKER_IS_DB_STATEMENT (stmt));

	g_assert (!stmt->stmt_is_sunk);

	sqlite3_bind_int64 (stmt->stmt, index + 1, value);
}

void
tracker_db_statement_bind_null (TrackerDBStatement        *stmt,
                                int                        index)
{
	g_return_if_fail (TRACKER_IS_DB_STATEMENT (stmt));

	g_assert (!stmt->stmt_is_sunk);

	sqlite3_bind_null (stmt->stmt, index + 1);
}

void
tracker_db_statement_bind_text (TrackerDBStatement        *stmt,
                                int                        index,
                                const gchar               *value)
{
	g_return_if_fail (TRACKER_IS_DB_STATEMENT (stmt));

	g_assert (!stmt->stmt_is_sunk);

	sqlite3_bind_text (stmt->stmt, index + 1, value, -1, SQLITE_TRANSIENT);
}

void
tracker_db_cursor_rewind (TrackerDBCursor *cursor)
{
	g_return_if_fail (TRACKER_IS_DB_CURSOR (cursor));

	sqlite3_reset (cursor->stmt);
}

gboolean
tracker_db_cursor_iter_next (TrackerDBCursor *cursor,
                             GCancellable    *cancellable,
                             GError         **error)
{
	TrackerDBStatement *stmt = cursor->ref_stmt;
	TrackerDBInterface *iface = stmt->db_interface;

	if (!cursor->finished) {
		guint result;

		if (g_cancellable_is_cancelled (cancellable)) {
			result = SQLITE_INTERRUPT;
			sqlite3_reset (cursor->stmt);
		} else {
			/* only one statement can be active at the same time per interface */
			iface->cancellable = cancellable;
			result = stmt_step (cursor->stmt);
			iface->cancellable = NULL;
		}

		if (result == SQLITE_INTERRUPT) {
			g_set_error (error,
			             TRACKER_DB_INTERFACE_ERROR,
			             TRACKER_DB_INTERRUPTED,
			             "Interrupted");
		} else if (result != SQLITE_ROW && result != SQLITE_DONE) {

			g_set_error (error,
			             TRACKER_DB_INTERFACE_ERROR,
			             TRACKER_DB_QUERY_ERROR,
			             "%s", sqlite3_errmsg (iface->db));
		}

		cursor->finished = (result != SQLITE_ROW);
	}

	return (!cursor->finished);
}

guint
tracker_db_cursor_get_n_columns (TrackerDBCursor *cursor)
{
	return sqlite3_column_count (cursor->stmt);
}

void
tracker_db_cursor_get_value (TrackerDBCursor *cursor,  guint column, GValue *value)
{
	gint col_type;

	col_type = sqlite3_column_type (cursor->stmt, column);

	switch (col_type) {
	case SQLITE_TEXT:
		g_value_init (value, G_TYPE_STRING);
		g_value_set_string (value, (gchar *) sqlite3_column_text (cursor->stmt, column));
		break;
	case SQLITE_INTEGER:
		g_value_init (value, G_TYPE_INT64);
		g_value_set_int64 (value, sqlite3_column_int64 (cursor->stmt, column));
		break;
	case SQLITE_FLOAT:
		g_value_init (value, G_TYPE_DOUBLE);
		g_value_set_double (value, sqlite3_column_double (cursor->stmt, column));
		break;
	case SQLITE_NULL:
		/* just ignore NULLs */
		break;
	default:
		g_critical ("Unknown sqlite3 database column type:%d", col_type);
	}

}

gint64
tracker_db_cursor_get_int (TrackerDBCursor *cursor,  guint column)
{
	return (gint64) sqlite3_column_int64 (cursor->stmt, column);
}

gdouble
tracker_db_cursor_get_double (TrackerDBCursor *cursor,  guint column)
{
	return (gdouble) sqlite3_column_double (cursor->stmt, column);
}


const gchar*
tracker_db_cursor_get_string (TrackerDBCursor *cursor,  guint column, gint *length)
{
	if (length) {
		sqlite3_value *val = sqlite3_column_value (cursor->stmt, column);

		*length = sqlite3_value_bytes (val);
		return (const gchar *) sqlite3_value_text (val);
	} else {
		return (const gchar *) sqlite3_column_text (cursor->stmt, column);
	}
}


TrackerDBResultSet *
tracker_db_statement_execute (TrackerDBStatement          *stmt,
                              GError                     **error)
{
	g_return_val_if_fail (!stmt->stmt_is_sunk, NULL);

	return create_result_set_from_stmt (stmt->db_interface, stmt->stmt, NULL, error);
}

TrackerDBCursor *
tracker_db_statement_start_cursor (TrackerDBStatement             *stmt,
                                   GError                        **error)
{
	g_return_val_if_fail (!stmt->stmt_is_sunk, NULL);

	return tracker_db_cursor_sqlite_new (stmt->stmt, stmt);
}


static void
tracker_db_statement_init (TrackerDBStatement *stmt)
{
}

static void
tracker_db_cursor_init (TrackerDBCursor *cursor)
{
}

static void
tracker_db_statement_sqlite_reset (TrackerDBStatement *stmt)
{
	g_assert (!stmt->stmt_is_sunk);

	sqlite3_reset (stmt->stmt);
	sqlite3_clear_bindings (stmt->stmt);
}

