/*
 * Copyright (C) 2008 Nokia
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


#define TRACKER_DB_INTERFACE_SQLITE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_DB_INTERFACE_SQLITE, TrackerDBInterfaceSqlitePrivate))

#define TRACKER_DB_STATEMENT_SQLITE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_DB_STATEMENT_SQLITE, TrackerDBStatementSqlitePrivate))

#define TRACKER_DB_CURSOR_SQLITE_GET_PRIVATE_O(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_DB_CURSOR_SQLITE, TrackerDBCursorSqlitePrivate))
#define TRACKER_DB_CURSOR_SQLITE_GET_PRIVATE(o) (((TrackerDBCursorSqlite *)o)->priv)

typedef struct TrackerDBInterfaceSqlitePrivate TrackerDBInterfaceSqlitePrivate;
typedef struct TrackerDBStatementSqlitePrivate TrackerDBStatementSqlitePrivate;
typedef struct TrackerDBCursorSqlitePrivate TrackerDBCursorSqlitePrivate;
typedef struct SqliteFunctionData SqliteFunctionData;
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
	GHashTable *statements;

	GSList *function_data;

	guint in_transaction : 1;
	guint ro : 1;
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

struct SqliteFunctionData {
	TrackerDBInterface *interface;
	TrackerDBFunc func;
};


struct TrackerDBStatementSqlite {
	GObject parent_instance;
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
	sqlite3_enable_shared_cache (1);
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
close_database (TrackerDBInterfaceSqlitePrivate *priv)
{
	g_hash_table_unref (priv->dynamic_statements);
	priv->dynamic_statements = NULL;

	g_hash_table_unref (priv->statements);
	priv->statements = NULL;

	g_slist_foreach (priv->function_data, (GFunc) g_free, NULL);
	g_slist_free (priv->function_data);
	priv->function_data = NULL;

	sqlite3_close (priv->db);
}

void
tracker_db_interface_sqlite_fts_init (TrackerDBInterfaceSqlite *interface,
                                      gboolean                  create)
{
	TrackerDBInterfaceSqlitePrivate *priv;

	priv = TRACKER_DB_INTERFACE_SQLITE_GET_PRIVATE (interface);

	tracker_fts_init (priv->db, create);
}

static void
tracker_db_interface_sqlite_finalize (GObject *object)
{
	TrackerDBInterfaceSqlitePrivate *priv;

	priv = TRACKER_DB_INTERFACE_SQLITE_GET_PRIVATE (object);

	close_database (priv);

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
	priv->statements = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                          (GDestroyNotify) g_free,
	                                          (GDestroyNotify) sqlite3_finalize);

}

static void
tracker_db_interface_sqlite_init (TrackerDBInterfaceSqlite *db_interface)
{
	TrackerDBInterfaceSqlitePrivate *priv;

	priv = TRACKER_DB_INTERFACE_SQLITE_GET_PRIVATE (db_interface);

	priv->ro = FALSE;
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

static void
internal_sqlite3_function (sqlite3_context *context,
                           int              argc,
                           sqlite3_value   *argv[])
{
	SqliteFunctionData *data;
	GValue *values, result;
	gint i;

	data = (SqliteFunctionData *) sqlite3_user_data (context);
	values = g_new0 (GValue, argc);

	/* Transform the arguments */
	for (i = 0; i < argc; i++) {
		switch (sqlite3_value_type (argv[i])) {
		case SQLITE_TEXT:
			g_value_init (&values[i], G_TYPE_STRING);
			g_value_set_string (&values[i], (gchar *) sqlite3_value_text (argv[i]));
			break;
		case SQLITE_INTEGER:
			g_value_init (&values[i], G_TYPE_INT);
			g_value_set_int (&values[i], sqlite3_value_int (argv[i]));
			break;
		case SQLITE_FLOAT:
			g_value_init (&values[i], G_TYPE_DOUBLE);
			g_value_set_double (&values[i], sqlite3_value_double (argv[i]));
			break;
		case SQLITE_NULL:
			/* Unset GValues as NULLs */
			break;
		default:
			g_critical ("Unknown sqlite3 database value type:%d",
			            sqlite3_value_type (argv[i]));
		}
	}

	/* Call the function */
	result = data->func (data->interface, argc, values);

	/* And return something appropriate to the context */
	if (G_VALUE_HOLDS_INT (&result)) {
		sqlite3_result_int (context, g_value_get_int (&result));
	} else if (G_VALUE_HOLDS_DOUBLE (&result)) {
		sqlite3_result_double (context, g_value_get_double (&result));
	} else if (G_VALUE_HOLDS_STRING (&result)) {
		sqlite3_result_text (context,
		                     g_value_dup_string (&result),
		                     -1, g_free);
	} else if (G_VALUE_HOLDS (&result, G_TYPE_INVALID)) {
		sqlite3_result_null (context);
	} else {
		g_critical ("Sqlite3 returned type not managed:'%s'",
		            G_VALUE_TYPE_NAME (&result));
		sqlite3_result_null (context);
	}

	/* Now free all this mess */
	for (i = 0; i < argc; i++) {
		if (G_IS_VALUE (&values[i])) {
			g_value_unset (&values[i]);
		}
	}

	if (! G_VALUE_HOLDS (&result, G_TYPE_INVALID)) {
		g_value_unset (&result);
	}

	g_free (values);
}

static TrackerDBStatement *
tracker_db_interface_sqlite_create_statement (TrackerDBInterface *db_interface,
                                              const gchar        *query)
{
	TrackerDBInterfaceSqlitePrivate *priv;
	TrackerDBStatementSqlite *stmt;

	priv = TRACKER_DB_INTERFACE_SQLITE_GET_PRIVATE (db_interface);

	stmt = g_hash_table_lookup (priv->dynamic_statements, query);

	if (!stmt) {
		sqlite3_stmt *sqlite_stmt;

		g_debug ("Preparing query: '%s'", query);

		if (sqlite3_prepare_v2 (priv->db, query, -1, &sqlite_stmt, NULL) != SQLITE_OK) {
			g_critical ("Unable to prepare query '%s': %s", query, sqlite3_errmsg (priv->db));
			return NULL;
		}

		stmt = tracker_db_statement_sqlite_new (TRACKER_DB_INTERFACE_SQLITE (db_interface), sqlite_stmt);
		g_hash_table_insert (priv->dynamic_statements, g_strdup (query), stmt);
	} else {
		tracker_db_statement_sqlite_reset (stmt);
	}

	return g_object_ref (stmt);
}

static void
foreach_print_error (gpointer key, gpointer value, gpointer stmt)
{
	if (value == stmt)
		g_print ("In %s\n", (char*) key);
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
		g_hash_table_foreach (priv->statements, foreach_print_error, stmt);

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

static void
tracker_db_interface_sqlite_iface_init (TrackerDBInterfaceIface *iface)
{
	iface->create_statement = tracker_db_interface_sqlite_create_statement;
	iface->execute_query = tracker_db_interface_sqlite_execute_query;
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

void
tracker_db_interface_sqlite_create_function (TrackerDBInterface *interface,
                                             const gchar        *name,
                                             TrackerDBFunc       func,
                                             gint                n_args)
{
	SqliteFunctionData *data;
	TrackerDBInterfaceSqlitePrivate *priv;

	priv = TRACKER_DB_INTERFACE_SQLITE_GET_PRIVATE (interface);

	data = g_new0 (SqliteFunctionData, 1);
	data->interface = interface;
	data->func = func;

	priv->function_data = g_slist_prepend (priv->function_data, data);

	sqlite3_create_function (priv->db, name, n_args, SQLITE_ANY, data, &internal_sqlite3_function, NULL, NULL);
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
tracker_db_cursor_sqlite_iter_next (TrackerDBCursor *cursor)
{
	TrackerDBCursorSqlitePrivate *priv;
	priv = TRACKER_DB_CURSOR_SQLITE_GET_PRIVATE (cursor);

	if (!priv->finished) {
		guint result;

		result = sqlite3_step (priv->stmt);

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

