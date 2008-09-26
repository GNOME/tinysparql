/* Tracker - Sqlite implementation
 * Copyright (C) 2008 Nokia
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <sqlite3.h>
#include "tracker-db-interface-sqlite.h"

#define TRACKER_DB_INTERFACE_SQLITE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_DB_INTERFACE_SQLITE, TrackerDBInterfaceSqlitePrivate))

typedef struct TrackerDBInterfaceSqlitePrivate TrackerDBInterfaceSqlitePrivate;
typedef struct SqliteFunctionData SqliteFunctionData;

struct TrackerDBInterfaceSqlitePrivate {
	gchar *filename;
	sqlite3 *db;

	GHashTable *statements;
	GHashTable *procedures;

	GSList *function_data;

	guint in_transaction : 1;
};

struct SqliteFunctionData {
	TrackerDBInterface *interface;
	TrackerDBFunc func;
};

static void tracker_db_interface_sqlite_iface_init (TrackerDBInterfaceIface *iface);

enum {
	PROP_0,
	PROP_FILENAME,
	PROP_IN_TRANSACTION
};

G_DEFINE_TYPE_WITH_CODE (TrackerDBInterfaceSqlite, tracker_db_interface_sqlite, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (TRACKER_TYPE_DB_INTERFACE,
						tracker_db_interface_sqlite_iface_init))

static GObject *
tracker_db_interface_sqlite_constructor (GType			type,
					 guint			n_construct_properties,
					 GObjectConstructParam *construct_params)
{
	GObject *object;
	TrackerDBInterfaceSqlitePrivate *priv;

	object = (* G_OBJECT_CLASS (tracker_db_interface_sqlite_parent_class)->constructor) (type,
											     n_construct_properties,
											     construct_params);
	priv = TRACKER_DB_INTERFACE_SQLITE_GET_PRIVATE (object);
	g_assert (priv->filename != NULL);

	if (sqlite3_open (priv->filename, &priv->db) != SQLITE_OK) {
		g_critical ("Could not open sqlite3 database:'%s'", priv->filename);
	} else {
		g_message ("Opened sqlite3 database:'%s'", priv->filename);
	}

	sqlite3_extended_result_codes (priv->db, 0);
	sqlite3_busy_timeout (priv->db, 10000000);

	return object;
}

static void
tracker_db_interface_sqlite_set_property (GObject	*object,
					  guint		 prop_id,
					  const GValue	*value,
					  GParamSpec	*pspec)
{
	TrackerDBInterfaceSqlitePrivate *priv;

	priv = TRACKER_DB_INTERFACE_SQLITE_GET_PRIVATE (object);

	switch (prop_id) {
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
tracker_db_interface_sqlite_finalize (GObject *object)
{
	TrackerDBInterfaceSqlitePrivate *priv;

	priv = TRACKER_DB_INTERFACE_SQLITE_GET_PRIVATE (object);

	g_hash_table_destroy (priv->statements);

	if (priv->procedures) {
		g_hash_table_unref (priv->procedures);
	}

	g_slist_foreach (priv->function_data, (GFunc) g_free, NULL);
	g_slist_free (priv->function_data);

	sqlite3_close (priv->db);
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

	g_type_class_add_private (object_class,
				  sizeof (TrackerDBInterfaceSqlitePrivate));
}

static void
tracker_db_interface_sqlite_init (TrackerDBInterfaceSqlite *db_interface)
{
	TrackerDBInterfaceSqlitePrivate *priv;

	priv = TRACKER_DB_INTERFACE_SQLITE_GET_PRIVATE (db_interface);

	priv->statements = g_hash_table_new_full (g_str_hash, g_str_equal,
						  (GDestroyNotify) g_free,
						  (GDestroyNotify) sqlite3_finalize);
}

static void
add_row (TrackerDBResultSet *result_set,
	 sqlite3_stmt	    *stmt)
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
			   int		    argc,
			   sqlite3_value   *argv[])
{
	SqliteFunctionData *data;
	GValue *values, result;
	GByteArray *blob_array;
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
		case SQLITE_BLOB: {
			gconstpointer blob;
			gint size;

			blob = sqlite3_value_blob (argv[i]);
			size = sqlite3_value_bytes (argv[i]);

			blob_array = g_byte_array_sized_new (size);
			g_byte_array_append (blob_array, blob, size);

			g_value_init (&values[i], TRACKER_TYPE_DB_BLOB);
			g_value_take_boxed (&values[i], blob_array);

			break;
		}
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
	} else if (G_VALUE_HOLDS (&result, TRACKER_TYPE_DB_BLOB)) {
		blob_array = g_value_get_boxed (&result);
		sqlite3_result_blob (context,
				     g_memdup (blob_array->data, blob_array->len),
				     blob_array->len,
				     g_free);
	} else if (G_VALUE_HOLDS (&result, G_TYPE_INVALID)) {
		sqlite3_result_null (context);
	} else {
		g_critical ("Sqlite3 returned type not managed:'%s'",
			    G_VALUE_TYPE_NAME (&result));
		sqlite3_result_null (context);
	}

	/* Now free all this mess */
	for (i = 0; i < argc; i++) {
		g_value_unset (&values[i]);
	}

	if (! G_VALUE_HOLDS (&result, G_TYPE_INVALID)) {
		g_value_unset (&result);
	}

	g_free (values);
}

static void
tracker_db_interface_sqlite_set_procedure_table (TrackerDBInterface *db_interface,
						 GHashTable	    *procedure_table)
{
	TrackerDBInterfaceSqlitePrivate *priv;

	priv = TRACKER_DB_INTERFACE_SQLITE_GET_PRIVATE (db_interface);

	if (priv->procedures) {
		g_hash_table_unref (priv->procedures);
		priv->procedures = NULL;
	}

	if (procedure_table) {
		priv->procedures = g_hash_table_ref (procedure_table);
	}
}

static void
foreach_print_error (gpointer key, gpointer value, gpointer stmt)
{
	if (value == stmt)
		g_print ("In %s\n", (char*) key);
}

static TrackerDBResultSet *
create_result_set_from_stmt (TrackerDBInterfaceSqlite  *interface,
			     sqlite3_stmt	       *stmt,
			     GError		      **error)
{
	TrackerDBInterfaceSqlitePrivate *priv;
	TrackerDBResultSet *result_set = NULL;
	gint columns, result, busy_count;

	priv = TRACKER_DB_INTERFACE_SQLITE_GET_PRIVATE (interface);
	columns = sqlite3_column_count (stmt);
	result = SQLITE_OK;
	busy_count = 0;

	while (result == SQLITE_OK  ||
	       result == SQLITE_ROW ||
	       result == SQLITE_BUSY) {

		result = sqlite3_step (stmt);

		switch (result) {
		case SQLITE_ERROR:
			sqlite3_reset (stmt);
			break;
		case SQLITE_BUSY:
			busy_count++;

			if (busy_count > 100000) {
				/* tracker_error ("ERROR: excessive busy count in query %s", query); */
				busy_count = 0;
			}

			if (busy_count > 50) {
				g_usleep (g_random_int_range (1000, busy_count * 200));
			} else {
				g_usleep (100);
			}

			break;
		case SQLITE_ROW:
			if (G_UNLIKELY (!result_set)) {
				result_set = _tracker_db_result_set_new (columns);
			}

			add_row (result_set, stmt);
			break;
		}
	}

	if (result != SQLITE_DONE) {

		g_hash_table_foreach (priv->statements, foreach_print_error, stmt);

		if (result == SQLITE_CORRUPT) {
			g_critical ("Sqlite3 database:'%s' is corrupt, can not live without it",
				    priv->filename);
			g_assert_not_reached ();
		}

		if (!error) {
			g_warning (sqlite3_errmsg (priv->db));
		} else {
			g_set_error (error,
				     TRACKER_DB_INTERFACE_ERROR,
				     TRACKER_DB_QUERY_ERROR,
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

static sqlite3_stmt *
get_stored_stmt (TrackerDBInterfaceSqlite *db_interface,
		 const gchar		  *procedure_name)
{
	TrackerDBInterfaceSqlitePrivate *priv;
	sqlite3_stmt *stmt;
	gint result;

	priv = TRACKER_DB_INTERFACE_SQLITE_GET_PRIVATE (db_interface);
	stmt = g_hash_table_lookup (priv->statements, procedure_name);

	if (!stmt || sqlite3_expired (stmt) != 0) {
		const gchar *procedure;

		procedure = g_hash_table_lookup (priv->procedures, procedure_name);

		if (!procedure) {
			g_critical ("Sqlite3 prepared query:'%s' was not found",
				    procedure_name);
			return NULL;
		}

		result = sqlite3_prepare_v2 (priv->db, procedure, -1, &stmt, NULL);

		if (result == SQLITE_OK && stmt) {
			g_hash_table_insert (priv->statements,
					     g_strdup (procedure_name),
					     stmt);
		}
	} else {
		sqlite3_reset (stmt);
	}

	return stmt;
}

static TrackerDBResultSet *
tracker_db_interface_sqlite_execute_procedure (TrackerDBInterface  *db_interface,
					       GError		  **error,
					       const gchar	   *procedure_name,
					       va_list		    args)
{
	TrackerDBInterfaceSqlitePrivate *priv;
	sqlite3_stmt *stmt;
	gint stmt_args, n_args;
	gchar *str;

	priv = TRACKER_DB_INTERFACE_SQLITE_GET_PRIVATE (db_interface);
	stmt = get_stored_stmt (TRACKER_DB_INTERFACE_SQLITE (db_interface), procedure_name);
	stmt_args = sqlite3_bind_parameter_count (stmt);

	for (n_args = 1; n_args <= stmt_args; n_args++) {
		str = va_arg (args, gchar *);
		sqlite3_bind_text (stmt, n_args, str, -1, SQLITE_STATIC);
	}

	return create_result_set_from_stmt (TRACKER_DB_INTERFACE_SQLITE (db_interface), stmt, error);
}

static TrackerDBResultSet *
tracker_db_interface_sqlite_execute_procedure_len (TrackerDBInterface  *db_interface,
						   GError	      **error,
						   const gchar	       *procedure_name,
						   va_list		args)
{
	TrackerDBInterfaceSqlitePrivate *priv;
	sqlite3_stmt *stmt;
	gint stmt_args, n_args, len;
	gchar *str;

	priv = TRACKER_DB_INTERFACE_SQLITE_GET_PRIVATE (db_interface);
	stmt = get_stored_stmt (TRACKER_DB_INTERFACE_SQLITE (db_interface), procedure_name);
	stmt_args = sqlite3_bind_parameter_count (stmt);

	for (n_args = 1; n_args <= stmt_args; n_args++) {
		str = va_arg (args, gchar *);
		len = va_arg (args, gint);

		if (len == -1) {
			/* Assume we're dealing with strings */
			sqlite3_bind_text (stmt, n_args, str, len, SQLITE_STATIC);
		} else {
			/* Deal with it as a blob */
			sqlite3_bind_blob (stmt, n_args, str, len, SQLITE_STATIC);
		}
	}

	return create_result_set_from_stmt (TRACKER_DB_INTERFACE_SQLITE (db_interface), stmt, error);
}

static TrackerDBResultSet *
tracker_db_interface_sqlite_execute_query (TrackerDBInterface  *db_interface,
					   GError	      **error,
					   const gchar	       *query)
{
	TrackerDBInterfaceSqlitePrivate *priv;
	TrackerDBResultSet *result_set;
	sqlite3_stmt *stmt;
	int retval;

	priv = TRACKER_DB_INTERFACE_SQLITE_GET_PRIVATE (db_interface);

	retval = sqlite3_prepare_v2 (priv->db, query, -1, &stmt, NULL);

	if (retval != SQLITE_OK) {
		g_set_error (error,
			     TRACKER_DB_INTERFACE_ERROR,
			     TRACKER_DB_QUERY_ERROR,
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
	iface->set_procedure_table = tracker_db_interface_sqlite_set_procedure_table;
	iface->execute_procedure = tracker_db_interface_sqlite_execute_procedure;
	iface->execute_procedure_len = tracker_db_interface_sqlite_execute_procedure_len;
	iface->execute_query = tracker_db_interface_sqlite_execute_query;
}

TrackerDBInterface *
tracker_db_interface_sqlite_new (const gchar *filename)
{
	return g_object_new (TRACKER_TYPE_DB_INTERFACE_SQLITE,
			     "filename", filename,
			     NULL);
}

static gint
collation_function (gpointer	  data,
		    int		  len1,
		    gconstpointer str1,
		    int		  len2,
		    gconstpointer str2)
{
	TrackerDBCollationFunc func;

	func = (TrackerDBCollationFunc) data;

	return (func) ((gchar *) str1, len1, (gchar *) str2, len2);
}

void
tracker_db_interface_sqlite_create_function (TrackerDBInterface *interface,
					     const gchar	*name,
					     TrackerDBFunc	 func,
					     gint		 n_args)
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
						    const gchar		     *name,
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
