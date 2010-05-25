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

#include <libtracker-common/tracker-common.h>
#include <libtracker-fts/tracker-fts.h>

#include "tracker-db-interface-sqlite.h"

#define TRACKER_TYPE_DB_CURSOR_SQLITE         (tracker_db_cursor_sqlite_get_type ())
#define TRACKER_DB_CURSOR_SQLITE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_DB_CURSOR_SQLITE, TrackerDBCursorSqlite))
#define TRACKER_DB_CURSOR_SQLITE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    TRACKER_TYPE_DB_CURSOR_SQLITE, TrackerDBCursorSqliteClass))
#define TRACKER_IS_DB_CURSOR_SQLITE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_DB_CURSOR_SQLITE))
#define TRACKER_IS_DB_CURSOR_SQLITE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((o),    TRACKER_TYPE_DB_CURSOR_SQLITE))
#define TRACKER_DB_CURSOR_SQLITE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  TRACKER_TYPE_DB_CURSOR_SQLITE, TrackerDBCursorSqliteClass))

#define TRACKER_TYPE_DB_STATEMENT_SQLITE         (tracker_db_statement_sqlite_get_type ())
#define TRACKER_DB_STATEMENT_SQLITE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_DB_STATEMENT_SQLITE, TrackerDBStatementSqlite))
#define TRACKER_DB_STATEMENT_SQLITE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    TRACKER_TYPE_DB_STATEMENT_SQLITE, TrackerDBStatementSqliteClass))
#define TRACKER_IS_DB_STATEMENT_SQLITE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_DB_STATEMENT_SQLITE))

#define TRACKER_IS_DB_STATEMENT_SQLITE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((o),    TRACKER_TYPE_DB_STATEMENT_SQLITE))
#define TRACKER_DB_STATEMENT_SQLITE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  TRACKER_TYPE_DB_STATEMENT_SQLITE, TrackerDBStatementSqliteClass))

#define TRACKER_DB_INTERFACE_SQLITE_GET_PRIVATE(o) (((TrackerDBInterfaceSqlite *)o)->priv)
#define TRACKER_DB_INTERFACE_SQLITE_GET_PRIVATE_O(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_DB_INTERFACE_SQLITE, TrackerDBInterfaceSqlitePrivate))

#define TRACKER_DB_STATEMENT_SQLITE_GET_PRIVATE(o) (((TrackerDBStatementSqlite *)o)->priv)
#define TRACKER_DB_STATEMENT_SQLITE_GET_PRIVATE_O(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_DB_STATEMENT_SQLITE, TrackerDBStatementSqlitePrivate))

#define TRACKER_DB_CURSOR_SQLITE_GET_PRIVATE_O(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_DB_CURSOR_SQLITE, TrackerDBCursorSqlitePrivate))
#define TRACKER_DB_CURSOR_SQLITE_GET_PRIVATE(o) (((TrackerDBCursorSqlite *)o)->priv)

/* I have no idea what the 'exact' meaning of nOps is in this API call, but
 * experimentally I noticed that 9 is a relatively good value. It has to do
 * with the frequency of check_interrupt being called. Presumably the amount
 * of SQLite 'ops', or something. The documentation of SQLite isn't very
 * enlightening about this either. I guess the higher you can make it, the
 * fewer overhead you induce. I fear, though, that it might depend on the
 * speed of the platform whether or not this value is actually a good value. */

#define SQLITE_PROGRESS_HANDLER_NOPS_VALUE 9

typedef struct TrackerDBStatementSqlitePrivate TrackerDBStatementSqlitePrivate;
typedef struct TrackerDBCursorSqlitePrivate TrackerDBCursorSqlitePrivate;
typedef struct TrackerDBCursorSqlite TrackerDBCursorSqlite;
typedef struct TrackerDBCursorSqliteClass TrackerDBCursorSqliteClass;
typedef struct TrackerDBStatementSqlite      TrackerDBStatementSqlite;
typedef struct TrackerDBStatementSqliteClass TrackerDBStatementSqliteClass;

struct TrackerDBCursorSqlite {
	GObject parent_instance;
	TrackerDBCursorSqlitePrivate *priv;
};

struct TrackerDBCursorSqliteClass {
	GObjectClass parent_class;
};

struct TrackerDBInterfaceSqlitePrivate {
	gchar *filename;
	sqlite3 *db;

	GHashTable *dynamic_statements;

	GSList *function_data;

	guint in_transaction : 1;
	guint ro : 1;
	guint fts_initialized : 1;
	gint interrupt;
};

struct TrackerDBStatementSqlitePrivate {
	TrackerDBInterfaceSqlite *db_interface;
	sqlite3_stmt *stmt;
	gboolean stmt_is_sunk;
};

struct TrackerDBCursorSqlitePrivate {
	sqlite3_stmt *stmt;
	TrackerDBStatementSqlite *ref_stmt;
	gboolean finished;
};

struct TrackerDBStatementSqlite {
	GObject parent_instance;
	TrackerDBStatementSqlitePrivate *priv;
};

struct TrackerDBStatementSqliteClass {
	GObjectClass parent_class;
};

static GType tracker_db_cursor_sqlite_get_type (void);
static GType tracker_db_statement_sqlite_get_type (void);

static void tracker_db_interface_sqlite_iface_init (TrackerDBInterfaceIface *iface);
static void tracker_db_statement_sqlite_iface_init (TrackerDBStatementIface *iface);
static void tracker_db_cursor_sqlite_iface_init (TrackerDBCursorIface *iface);

static TrackerDBStatementSqlite * tracker_db_statement_sqlite_new (TrackerDBInterfaceSqlite     *db_interface,
                                                                   sqlite3_stmt                         *sqlite_stmt);
static TrackerDBCursor          * tracker_db_cursor_sqlite_new    (sqlite3_stmt                         *sqlite_stmt,
                                                                   TrackerDBStatementSqlite     *ref_stmt);
static void tracker_db_statement_sqlite_reset (TrackerDBStatementSqlite *stmt);

enum {
	PROP_0,
	PROP_FILENAME,
	PROP_IN_TRANSACTION,
	PROP_RO
};

G_DEFINE_TYPE_WITH_CODE (TrackerDBInterfaceSqlite, tracker_db_interface_sqlite, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (TRACKER_TYPE_DB_INTERFACE,
                                                tracker_db_interface_sqlite_iface_init))

G_DEFINE_TYPE_WITH_CODE (TrackerDBStatementSqlite, tracker_db_statement_sqlite, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (TRACKER_TYPE_DB_STATEMENT,
                                                tracker_db_statement_sqlite_iface_init))

G_DEFINE_TYPE_WITH_CODE (TrackerDBCursorSqlite, tracker_db_cursor_sqlite, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (TRACKER_TYPE_DB_CURSOR,
                                                tracker_db_cursor_sqlite_iface_init))

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

static int
check_interrupt (void *user_data)
{
	TrackerDBInterfaceSqlitePrivate *priv = user_data;
	return g_atomic_int_compare_and_exchange (&priv->interrupt, 1, 0);
}

static void
open_database (TrackerDBInterfaceSqlitePrivate *priv)
{
	g_assert (priv->filename != NULL);

	if (!priv->ro) {
		if (sqlite3_open (priv->filename, &priv->db) != SQLITE_OK) {
			g_critical ("Could not open sqlite3 database:'%s'", priv->filename);
		} else {
			g_message ("Opened sqlite3 database:'%s'", priv->filename);
		}
	} else {
		if (sqlite3_open_v2 (priv->filename, &priv->db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
			g_critical ("Could not open sqlite3 database:'%s'", priv->filename);
		} else {
			g_message ("Opened sqlite3 database:'%s'", priv->filename);
		}
	}

	sqlite3_progress_handler (priv->db, SQLITE_PROGRESS_HANDLER_NOPS_VALUE,
	                          check_interrupt, priv);

	sqlite3_create_function (priv->db, "SparqlRegex", 3, SQLITE_ANY,
	                         priv, &function_sparql_regex,
	                         NULL, NULL);

	sqlite3_create_function (priv->db, "SparqlHaversineDistance", 4, SQLITE_ANY,
	                         priv, &function_sparql_haversine_distance,
	                         NULL, NULL);

	sqlite3_create_function (priv->db, "SparqlCartesianDistance", 4, SQLITE_ANY,
	                         priv, &function_sparql_cartesian_distance,
	                         NULL, NULL);

	sqlite3_create_function (priv->db, "SparqlStringFromFilename", 1, SQLITE_ANY,
	                         priv, &function_sparql_string_from_filename,
	                         NULL, NULL);

	sqlite3_create_function (priv->db, "SparqlStringJoin", -1, SQLITE_ANY,
	                         priv, &function_sparql_string_join,
	                         NULL, NULL);

	sqlite3_create_function (priv->db, "SparqlUriIsParent", 2, SQLITE_ANY,
	                         priv, &function_sparql_uri_is_parent,
	                         NULL, NULL);

	sqlite3_create_function (priv->db, "SparqlUriIsDescendant", 2, SQLITE_ANY,
	                         priv, &function_sparql_uri_is_descendant,
	                         NULL, NULL);

	sqlite3_extended_result_codes (priv->db, 0);
	sqlite3_busy_timeout (priv->db, 100000);
}

static GObject *
tracker_db_interface_sqlite_constructor (GType                  type,
                                         guint                  n_construct_properties,
                                         GObjectConstructParam *construct_params)
{
	GObject *object;
	TrackerDBInterfaceSqlitePrivate *priv;

	object = (* G_OBJECT_CLASS (tracker_db_interface_sqlite_parent_class)->constructor) (type,
		               n_construct_properties,
		               construct_params);
	priv = TRACKER_DB_INTERFACE_SQLITE_GET_PRIVATE (object);

	open_database (priv);

	return object;
}

static void
tracker_db_interface_sqlite_set_property (GObject       *object,
                                          guint                  prop_id,
                                          const GValue  *value,
                                          GParamSpec    *pspec)
{
	TrackerDBInterfaceSqlitePrivate *priv;

	priv = TRACKER_DB_INTERFACE_SQLITE_GET_PRIVATE (object);

	switch (prop_id) {
	case PROP_RO:
		priv->ro = g_value_get_boolean (value);
		break;
	case PROP_FILENAME:
		priv->filename = g_value_dup_string (value);
		break;
	case PROP_IN_TRANSACTION:
		priv->in_transaction = g_value_get_boolean (value);
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
	TrackerDBInterfaceSqlitePrivate *priv;

	priv = TRACKER_DB_INTERFACE_SQLITE_GET_PRIVATE (object);

	switch (prop_id) {
	case PROP_RO:
		g_value_set_boolean (value, priv->ro);
		break;
	case PROP_FILENAME:
		g_value_set_string (value, priv->filename);
		break;
	case PROP_IN_TRANSACTION:
		g_value_set_boolean (value, priv->in_transaction);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
close_database (GObject                         *object,
                TrackerDBInterfaceSqlitePrivate *priv)
{
	gint rc;

	g_hash_table_unref (priv->dynamic_statements);
	priv->dynamic_statements = NULL;

	g_slist_foreach (priv->function_data, (GFunc) g_free, NULL);
	g_slist_free (priv->function_data);
	priv->function_data = NULL;

	if (priv->fts_initialized) {
		tracker_fts_shutdown (object);
	}

	rc = sqlite3_close (priv->db);
	g_warn_if_fail (rc == SQLITE_OK);
}

void
tracker_db_interface_sqlite_fts_init (TrackerDBInterfaceSqlite *interface,
                                      gboolean                  create)
{
	TrackerDBInterfaceSqlitePrivate *priv;

	priv = TRACKER_DB_INTERFACE_SQLITE_GET_PRIVATE (interface);

	tracker_fts_init (priv->db, create, G_OBJECT (interface));
	priv->fts_initialized = TRUE;
}

static void
tracker_db_interface_sqlite_finalize (GObject *object)
{
	TrackerDBInterfaceSqlitePrivate *priv;

	priv = TRACKER_DB_INTERFACE_SQLITE_GET_PRIVATE (object);

	close_database (object, priv);

	g_message ("Closed sqlite3 database:'%s'", priv->filename);


	g_free (priv->filename);

	G_OBJECT_CLASS (tracker_db_interface_sqlite_parent_class)->finalize (object);
}

static void
tracker_db_interface_sqlite_class_init (TrackerDBInterfaceSqliteClass *class)
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
	/* Override properties from interface */
	g_object_class_override_property (object_class,
	                                  PROP_IN_TRANSACTION,
	                                  "in-transaction");

	g_object_class_install_property (object_class,
	                                 PROP_RO,
	                                 g_param_spec_boolean ("read-only",
	                                                       "Read only",
	                                                       "Whether the connection is read only",
	                                                       FALSE,
	                                                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (object_class,
	                          sizeof (TrackerDBInterfaceSqlitePrivate));
}

static void
prepare_database (TrackerDBInterfaceSqlitePrivate *priv)
{
	priv->dynamic_statements = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                                  (GDestroyNotify) g_free,
	                                                  (GDestroyNotify) g_object_unref);
}

static void
tracker_db_interface_sqlite_init (TrackerDBInterfaceSqlite *db_interface)
{
	TrackerDBInterfaceSqlitePrivate *priv;

	priv = TRACKER_DB_INTERFACE_SQLITE_GET_PRIVATE_O (db_interface);

	priv->ro = FALSE;
	db_interface->priv = priv;

	prepare_database (priv);
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
			g_value_init (&value, G_TYPE_INT);
			g_value_set_int (&value, sqlite3_column_int (stmt, i));
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


static TrackerDBStatement *
tracker_db_interface_sqlite_create_statement (TrackerDBInterface  *db_interface,
                                              GError             **error,
                                              const gchar         *query)
{
	TrackerDBInterfaceSqlitePrivate *priv;
	TrackerDBStatementSqlite *stmt;

	priv = TRACKER_DB_INTERFACE_SQLITE_GET_PRIVATE (db_interface);

	stmt = g_hash_table_lookup (priv->dynamic_statements, query);

	if (!stmt) {
		sqlite3_stmt *sqlite_stmt;
		int retval;

		g_debug ("Preparing query: '%s'", query);

		retval = sqlite3_prepare_v2 (priv->db, query, -1, &sqlite_stmt, NULL);

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
				             sqlite3_errmsg (priv->db));
			}

			return NULL;
		}

		stmt = tracker_db_statement_sqlite_new (TRACKER_DB_INTERFACE_SQLITE (db_interface), sqlite_stmt);
		g_hash_table_insert (priv->dynamic_statements, g_strdup (query), stmt);
	} else {
		tracker_db_statement_sqlite_reset (stmt);
	}

	return g_object_ref (stmt);
}

static TrackerDBResultSet *
create_result_set_from_stmt (TrackerDBInterfaceSqlite  *interface,
                             sqlite3_stmt              *stmt,
                             GError                   **error)
{
	TrackerDBInterfaceSqlitePrivate *priv;
	TrackerDBResultSet *result_set = NULL;
	gint columns, result;

	priv = TRACKER_DB_INTERFACE_SQLITE_GET_PRIVATE (interface);
	columns = sqlite3_column_count (stmt);
	result = SQLITE_OK;

	while (result == SQLITE_OK  ||
	       result == SQLITE_ROW) {

		result = sqlite3_step (stmt);

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
		if (sqlite3_errcode (priv->db) == SQLITE_IOERR ||
		    sqlite3_errcode (priv->db) == SQLITE_CORRUPT ||
		    sqlite3_errcode (priv->db) == SQLITE_NOTADB) {

			sqlite3_finalize (stmt);
			sqlite3_close (priv->db);

			g_unlink (priv->filename);

			g_error ("SQLite experienced an error with file:'%s'. "
			         "It is either NOT a SQLite database or it is "
			         "corrupt or there was an IO error accessing the data. "
			         "This file has now been removed and will be recreated on the next start. "
			         "Shutting down now.",
			         priv->filename);

			return NULL;
		}

		if (!error) {
			g_warning ("Could not perform SQLite operation, error:%d->'%s'",
			           sqlite3_errcode (priv->db),
			           sqlite3_errmsg (priv->db));
		} else {
			g_set_error (error,
			             TRACKER_DB_INTERFACE_ERROR,
			             TRACKER_DB_QUERY_ERROR,
			             "%s",
			             sqlite3_errmsg (priv->db));
		}

		/* If there was an error, result set may be invalid or incomplete */
		if (result_set) {
			g_object_unref (result_set);
		}

		return NULL;
	}

	return result_set;
}


static TrackerDBResultSet *
tracker_db_interface_sqlite_execute_query (TrackerDBInterface  *db_interface,
                                           GError             **error,
                                           const gchar         *query)
{
	TrackerDBInterfaceSqlitePrivate *priv;
	TrackerDBResultSet *result_set;
	sqlite3_stmt *stmt;
	int retval;

	priv = TRACKER_DB_INTERFACE_SQLITE_GET_PRIVATE (db_interface);

	/* g_debug ("Running query: '%s'", query); */
	retval = sqlite3_prepare_v2 (priv->db, query, -1, &stmt, NULL);

	if (retval != SQLITE_OK) {
		g_set_error (error,
		             TRACKER_DB_INTERFACE_ERROR,
		             TRACKER_DB_QUERY_ERROR,
		             "%s",
		             sqlite3_errmsg (priv->db));
		return NULL;
	} else if (stmt == NULL) {
		g_set_error (error,
		             TRACKER_DB_INTERFACE_ERROR,
		             TRACKER_DB_QUERY_ERROR,
		             "Could not prepare SQL statement:'%s'",
		             query);

		return NULL;
	}

	result_set = create_result_set_from_stmt (TRACKER_DB_INTERFACE_SQLITE (db_interface), stmt, error);
	sqlite3_finalize (stmt);

	return result_set;
}

static gboolean
tracker_db_interface_sqlite_interrupt (TrackerDBInterface *iface)
{
	TrackerDBInterfaceSqlitePrivate *priv;

	priv = TRACKER_DB_INTERFACE_SQLITE_GET_PRIVATE (iface);

	g_atomic_int_set (&priv->interrupt, 1);

	return TRUE;
}

static void
tracker_db_interface_sqlite_iface_init (TrackerDBInterfaceIface *iface)
{
	iface->create_statement = tracker_db_interface_sqlite_create_statement;
	iface->execute_query = tracker_db_interface_sqlite_execute_query;
	iface->interrupt = tracker_db_interface_sqlite_interrupt;
}

TrackerDBInterface *
tracker_db_interface_sqlite_new (const gchar *filename)
{
	return g_object_new (TRACKER_TYPE_DB_INTERFACE_SQLITE,
	                     "filename", filename,
	                     NULL);
}

TrackerDBInterface *
tracker_db_interface_sqlite_new_ro (const gchar *filename)
{
	return g_object_new (TRACKER_TYPE_DB_INTERFACE_SQLITE,
	                     "filename", filename,
	                     "read-only", TRUE,
	                     NULL);
}

static gint
collation_function (gpointer      data,
                    int                   len1,
                    gconstpointer str1,
                    int                   len2,
                    gconstpointer str2)
{
	TrackerDBCollationFunc func;

	func = (TrackerDBCollationFunc) data;

	return (func) ((gchar *) str1, len1, (gchar *) str2, len2);
}

gboolean
tracker_db_interface_sqlite_set_collation_function (TrackerDBInterfaceSqlite *interface,
                                                    const gchar                      *name,
                                                    TrackerDBCollationFunc    func)
{
	TrackerDBInterfaceSqlitePrivate *priv;
	gint result;

	g_return_val_if_fail (TRACKER_IS_DB_INTERFACE_SQLITE (interface), FALSE);

	priv = TRACKER_DB_INTERFACE_SQLITE_GET_PRIVATE (interface);

	result = sqlite3_create_collation (priv->db, name, SQLITE_UTF8, func, &collation_function);

	return (result == SQLITE_OK);
}

gint64
tracker_db_interface_sqlite_get_last_insert_id (TrackerDBInterfaceSqlite *interface)
{
	TrackerDBInterfaceSqlitePrivate *priv;

	g_return_val_if_fail (TRACKER_IS_DB_INTERFACE_SQLITE (interface), 0);

	priv = TRACKER_DB_INTERFACE_SQLITE_GET_PRIVATE (interface);

	return (gint64) sqlite3_last_insert_rowid (priv->db);
}

static void
tracker_db_statement_sqlite_finalize (GObject *object)
{
	TrackerDBStatementSqlitePrivate *priv;

	priv = TRACKER_DB_STATEMENT_SQLITE_GET_PRIVATE (object);

	/* A cursor was still open while we're being finalized, because a cursor
	 * holds its own reference, this means that somebody is unreffing a stmt
	 * too often. We mustn't sqlite3_finalize the priv->stmt in this case,
	 * though. It would crash&burn the cursor. */

	g_assert (!priv->stmt_is_sunk);

	sqlite3_finalize (priv->stmt);

	G_OBJECT_CLASS (tracker_db_statement_sqlite_parent_class)->finalize (object);
}

static void
tracker_db_statement_sqlite_class_init (TrackerDBStatementSqliteClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->finalize = tracker_db_statement_sqlite_finalize;

	g_type_class_add_private (object_class,
	                          sizeof (TrackerDBStatementSqlitePrivate));
}

static TrackerDBStatementSqlite *
tracker_db_statement_sqlite_new (TrackerDBInterfaceSqlite       *db_interface,
                                 sqlite3_stmt                   *sqlite_stmt)
{
	TrackerDBStatementSqlite *stmt;
	TrackerDBStatementSqlitePrivate *priv;

	stmt = g_object_new (TRACKER_TYPE_DB_STATEMENT_SQLITE, NULL);

	priv = TRACKER_DB_STATEMENT_SQLITE_GET_PRIVATE (stmt);

	priv->db_interface = db_interface;
	priv->stmt = sqlite_stmt;
	priv->stmt_is_sunk = FALSE;

	return stmt;
}

static void
tracker_db_cursor_sqlite_finalize (GObject *object)
{
	TrackerDBCursorSqlitePrivate *priv;

	priv = TRACKER_DB_CURSOR_SQLITE_GET_PRIVATE (object);

	if (priv->ref_stmt) {
		TrackerDBStatementSqlitePrivate *stmt_priv;
		stmt_priv = TRACKER_DB_STATEMENT_SQLITE_GET_PRIVATE (priv->ref_stmt);
		stmt_priv->stmt_is_sunk = FALSE;
		tracker_db_statement_sqlite_reset (priv->ref_stmt);
		g_object_unref (priv->ref_stmt);
	} else {
		sqlite3_finalize (priv->stmt);
	}

	G_OBJECT_CLASS (tracker_db_cursor_sqlite_parent_class)->finalize (object);
}

static void
tracker_db_cursor_sqlite_class_init (TrackerDBCursorSqliteClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->finalize = tracker_db_cursor_sqlite_finalize;

	g_type_class_add_private (object_class,
	                          sizeof (TrackerDBCursorSqlitePrivate));
}

static TrackerDBCursor *
tracker_db_cursor_sqlite_new (sqlite3_stmt              *sqlite_stmt,
                              TrackerDBStatementSqlite  *ref_stmt)
{
	TrackerDBCursor *cursor;
	TrackerDBCursorSqlitePrivate *priv;

	cursor = g_object_new (TRACKER_TYPE_DB_CURSOR_SQLITE, NULL);

	priv = TRACKER_DB_CURSOR_SQLITE_GET_PRIVATE (cursor);

	priv->stmt = sqlite_stmt;
	priv->finished = FALSE;

	if (ref_stmt) {
		TrackerDBStatementSqlitePrivate *stmt_priv;
		stmt_priv = TRACKER_DB_STATEMENT_SQLITE_GET_PRIVATE (ref_stmt);
		stmt_priv->stmt_is_sunk = TRUE;
		priv->ref_stmt = g_object_ref (ref_stmt);
	} else {
		priv->ref_stmt = NULL;
	}

	return cursor;
}

static void
tracker_db_statement_sqlite_bind_double (TrackerDBStatement      *stmt,
                                         int                      index,
                                         double                           value)
{
	TrackerDBStatementSqlitePrivate *priv;

	priv = TRACKER_DB_STATEMENT_SQLITE_GET_PRIVATE (stmt);

	g_assert (!priv->stmt_is_sunk);

	sqlite3_bind_double (priv->stmt, index + 1, value);
}

static void
tracker_db_statement_sqlite_bind_int (TrackerDBStatement         *stmt,
                                      int                         index,
                                      int                         value)
{
	TrackerDBStatementSqlitePrivate *priv;

	priv = TRACKER_DB_STATEMENT_SQLITE_GET_PRIVATE (stmt);

	g_assert (!priv->stmt_is_sunk);

	sqlite3_bind_int (priv->stmt, index + 1, value);
}

static void
tracker_db_statement_sqlite_bind_int64 (TrackerDBStatement       *stmt,
                                        int                      index,
                                        gint64                           value)
{
	TrackerDBStatementSqlitePrivate *priv;

	priv = TRACKER_DB_STATEMENT_SQLITE_GET_PRIVATE (stmt);

	g_assert (!priv->stmt_is_sunk);

	sqlite3_bind_int64 (priv->stmt, index + 1, value);
}

static void
tracker_db_statement_sqlite_bind_null (TrackerDBStatement        *stmt,
                                       int                        index)
{
	TrackerDBStatementSqlitePrivate *priv;

	priv = TRACKER_DB_STATEMENT_SQLITE_GET_PRIVATE (stmt);

	g_assert (!priv->stmt_is_sunk);

	sqlite3_bind_null (priv->stmt, index + 1);
}

static void
tracker_db_statement_sqlite_bind_text (TrackerDBStatement        *stmt,
                                       int                        index,
                                       const gchar               *value)
{
	TrackerDBStatementSqlitePrivate *priv;

	priv = TRACKER_DB_STATEMENT_SQLITE_GET_PRIVATE (stmt);

	g_assert (!priv->stmt_is_sunk);

	sqlite3_bind_text (priv->stmt, index + 1, value, -1, SQLITE_TRANSIENT);
}

static void
tracker_db_cursor_sqlite_rewind (TrackerDBCursor *cursor)
{
	TrackerDBCursorSqlitePrivate *priv;

	priv = TRACKER_DB_CURSOR_SQLITE_GET_PRIVATE (cursor);

	sqlite3_reset (priv->stmt);
}

static gboolean
tracker_db_cursor_sqlite_iter_next (TrackerDBCursor *cursor,
                                    GError         **error)
{
	TrackerDBCursorSqlitePrivate *priv;
	priv = TRACKER_DB_CURSOR_SQLITE_GET_PRIVATE (cursor);

	if (!priv->finished) {
		guint result;

		result = sqlite3_step (priv->stmt);

		if (result == SQLITE_INTERRUPT) {
			g_set_error (error,
			             TRACKER_DB_INTERFACE_ERROR,
			             TRACKER_DB_INTERRUPTED,
			             "Interrupted");
		} else if (result != SQLITE_ROW && result != SQLITE_DONE) {
			TrackerDBStatementSqlite *stmt = priv->ref_stmt;
			TrackerDBStatementSqlitePrivate *stmt_priv = TRACKER_DB_STATEMENT_SQLITE_GET_PRIVATE (stmt);
			TrackerDBInterfaceSqlite *iface = stmt_priv->db_interface;
			TrackerDBInterfaceSqlitePrivate *iface_priv = TRACKER_DB_INTERFACE_SQLITE_GET_PRIVATE (iface);

			g_set_error (error,
			             TRACKER_DB_INTERFACE_ERROR,
			             TRACKER_DB_QUERY_ERROR,
			             "%s", sqlite3_errmsg (iface_priv->db));
		}

		priv->finished = (result != SQLITE_ROW);
	}

	return (!priv->finished);
}

static guint
tracker_db_cursor_sqlite_get_n_columns (TrackerDBCursor *cursor)
{
	TrackerDBCursorSqlitePrivate *priv;

	priv = TRACKER_DB_CURSOR_SQLITE_GET_PRIVATE (cursor);

	return sqlite3_column_count (priv->stmt);
}

static void
tracker_db_cursor_sqlite_get_value (TrackerDBCursor *cursor,  guint column, GValue *value)
{
	TrackerDBCursorSqlitePrivate *priv;
	gint col_type;

	priv = TRACKER_DB_CURSOR_SQLITE_GET_PRIVATE (cursor);

	col_type = sqlite3_column_type (priv->stmt, column);

	switch (col_type) {
	case SQLITE_TEXT:
		g_value_init (value, G_TYPE_STRING);
		g_value_set_string (value, (gchar *) sqlite3_column_text (priv->stmt, column));
		break;
	case SQLITE_INTEGER:
		g_value_init (value, G_TYPE_INT);
		g_value_set_int (value, sqlite3_column_int (priv->stmt, column));
		break;
	case SQLITE_FLOAT:
		g_value_init (value, G_TYPE_DOUBLE);
		g_value_set_double (value, sqlite3_column_double (priv->stmt, column));
		break;
	case SQLITE_NULL:
		/* just ignore NULLs */
		break;
	default:
		g_critical ("Unknown sqlite3 database column type:%d", col_type);
	}

}

static gint
tracker_db_cursor_sqlite_get_int (TrackerDBCursor *cursor,  guint column)
{
	TrackerDBCursorSqlitePrivate *priv;
	priv = TRACKER_DB_CURSOR_SQLITE_GET_PRIVATE (cursor);
	return (gint) sqlite3_column_int (priv->stmt, column);
}

static gdouble
tracker_db_cursor_sqlite_get_double (TrackerDBCursor *cursor,  guint column)
{
	TrackerDBCursorSqlitePrivate *priv;
	priv = TRACKER_DB_CURSOR_SQLITE_GET_PRIVATE (cursor);
	return (gdouble) sqlite3_column_double (priv->stmt, column);
}


static const gchar*
tracker_db_cursor_sqlite_get_string (TrackerDBCursor *cursor,  guint column)
{
	TrackerDBCursorSqlitePrivate *priv;
	priv = TRACKER_DB_CURSOR_SQLITE_GET_PRIVATE (cursor);
	return (const gchar *) sqlite3_column_text (priv->stmt, column);
}


static TrackerDBResultSet *
tracker_db_statement_sqlite_execute (TrackerDBStatement                  *stmt,
                                     GError                     **error)
{
	TrackerDBStatementSqlitePrivate *priv;

	priv = TRACKER_DB_STATEMENT_SQLITE_GET_PRIVATE (stmt);

	g_return_val_if_fail (!priv->stmt_is_sunk, NULL);

	return create_result_set_from_stmt (priv->db_interface, priv->stmt, error);
}

static TrackerDBCursor *
tracker_db_statement_sqlite_start_cursor (TrackerDBStatement             *stmt,
                                          GError                        **error)
{
	TrackerDBStatementSqlitePrivate *priv;

	priv = TRACKER_DB_STATEMENT_SQLITE_GET_PRIVATE (stmt);

	g_return_val_if_fail (!priv->stmt_is_sunk, NULL);

	return tracker_db_cursor_sqlite_new (priv->stmt, TRACKER_DB_STATEMENT_SQLITE (stmt));
}


static void
tracker_db_statement_sqlite_iface_init (TrackerDBStatementIface *iface)
{
	iface->bind_double = tracker_db_statement_sqlite_bind_double;
	iface->bind_int = tracker_db_statement_sqlite_bind_int;
	iface->bind_int64 = tracker_db_statement_sqlite_bind_int64;
	iface->bind_null = tracker_db_statement_sqlite_bind_null;
	iface->bind_text = tracker_db_statement_sqlite_bind_text;
	iface->execute = tracker_db_statement_sqlite_execute;
	iface->start_cursor = tracker_db_statement_sqlite_start_cursor;
}


static void
tracker_db_cursor_sqlite_iface_init (TrackerDBCursorIface *iface)
{
	iface->rewind = tracker_db_cursor_sqlite_rewind;
	iface->iter_next = tracker_db_cursor_sqlite_iter_next;
	iface->get_n_columns = tracker_db_cursor_sqlite_get_n_columns;
	iface->get_value = tracker_db_cursor_sqlite_get_value;
	iface->get_int = tracker_db_cursor_sqlite_get_int;
	iface->get_double = tracker_db_cursor_sqlite_get_double;
	iface->get_string = tracker_db_cursor_sqlite_get_string;
}

static void
tracker_db_statement_sqlite_init (TrackerDBStatementSqlite *stmt)
{
	stmt->priv = TRACKER_DB_STATEMENT_SQLITE_GET_PRIVATE_O(stmt);
}

static void
tracker_db_cursor_sqlite_init (TrackerDBCursorSqlite *cursor)
{
	TrackerDBCursorSqlitePrivate *priv = TRACKER_DB_CURSOR_SQLITE_GET_PRIVATE_O (cursor);
	cursor->priv = priv;
}

static void
tracker_db_statement_sqlite_reset (TrackerDBStatementSqlite *stmt)
{
	TrackerDBStatementSqlitePrivate *priv;

	priv = TRACKER_DB_STATEMENT_SQLITE_GET_PRIVATE (stmt);

	g_assert (!priv->stmt_is_sunk);

	sqlite3_reset (priv->stmt);
	sqlite3_clear_bindings (priv->stmt);
}

