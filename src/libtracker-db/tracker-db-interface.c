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

#include <string.h>

#include <gobject/gvaluecollector.h>

#include "tracker-db-interface.h"

#define TRACKER_DB_RESULT_SET_GET_PRIVATE_O(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_DB_RESULT_SET, TrackerDBResultSetPrivate))
#define TRACKER_DB_RESULT_SET_GET_PRIVATE(o) (((TrackerDBResultSet*)o)->priv)

struct TrackerDBResultSetPrivate {
	GType *col_types;
	GPtrArray *array;
	guint columns;
	guint current_row;
};

enum {
	PROP_0,
	PROP_COLUMNS
};

G_DEFINE_TYPE (TrackerDBResultSet, tracker_db_result_set, G_TYPE_OBJECT)

GQuark
tracker_db_interface_error_quark (void)
{
	return g_quark_from_static_string ("tracker-db-interface-error-quark");
}

static void
tracker_db_interface_class_init (gpointer iface)
{
	g_object_interface_install_property (iface,
	                                     g_param_spec_boolean ("in-transaction",
	                                                           "In transaction",
	                                                           "Whether the connection has a transaction opened",
	                                                           FALSE,
	                                                           G_PARAM_READWRITE));
}

GType
tracker_db_interface_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		type = g_type_register_static_simple (G_TYPE_INTERFACE,
		                                      "TrackerDBInterface",
		                                      sizeof (TrackerDBInterfaceIface),
		                                      (GClassInitFunc) tracker_db_interface_class_init,
		                                      0, NULL, 0);

		g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
	}

	return type;
}

GType
tracker_db_statement_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		type = g_type_register_static_simple (G_TYPE_INTERFACE,
		                                      "TrackerDBStatement",
		                                      sizeof (TrackerDBStatementIface),
		                                      NULL,
		                                      0, NULL, 0);

		g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
	}

	return type;
}

GType
tracker_db_cursor_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		type = g_type_register_static_simple (G_TYPE_INTERFACE,
		                                      "TrackerDBCursor",
		                                      sizeof (TrackerDBCursorIface),
		                                      NULL,
		                                      0, NULL, 0);

		g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
	}

	return type;
}


/* TrackerDBResultSet */
static void
tracker_db_result_set_set_property (GObject       *object,
                                    guint          prop_id,
                                    const GValue  *value,
                                    GParamSpec    *pspec)
{
	TrackerDBResultSetPrivate *priv;

	priv = TRACKER_DB_RESULT_SET_GET_PRIVATE (object);

	switch (prop_id) {
	case PROP_COLUMNS:
		priv->columns = g_value_get_uint (value);
		priv->col_types = g_new0 (GType, priv->columns);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
tracker_db_result_set_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
	TrackerDBResultSetPrivate *priv;

	priv = TRACKER_DB_RESULT_SET_GET_PRIVATE (object);

	switch (prop_id) {
	case PROP_COLUMNS:
		g_value_set_uint (value, priv->columns);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
free_row (gpointer *row,
          gpointer  data)
{
	guint columns = GPOINTER_TO_UINT (data);
	guint i;

	if (!row)
		return;

	for (i = 0; i < columns; i++) {
		g_free (row[i]);
	}

	g_free (row);
}

static void
tracker_db_result_set_finalize (GObject *object)
{
	TrackerDBResultSetPrivate *priv;

	priv = TRACKER_DB_RESULT_SET_GET_PRIVATE (object);

	if (priv->array) {
		g_ptr_array_foreach (priv->array, (GFunc) free_row,
		                     GUINT_TO_POINTER (priv->columns));
		g_ptr_array_free (priv->array, TRUE);
	}

	g_free (priv->col_types);

	G_OBJECT_CLASS (tracker_db_result_set_parent_class)->finalize (object);
}

static void
tracker_db_result_set_class_init (TrackerDBResultSetClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->set_property = tracker_db_result_set_set_property;
	object_class->get_property = tracker_db_result_set_get_property;
	object_class->finalize = tracker_db_result_set_finalize;

	g_object_class_install_property (object_class,
	                                 PROP_COLUMNS,
	                                 g_param_spec_uint ("columns",
	                                                    "Columns",
	                                                    "Resultset columns",
	                                                    0, G_MAXUINT, 0,
	                                                    G_PARAM_READWRITE |
	                                                    G_PARAM_CONSTRUCT_ONLY));


	g_type_class_add_private (object_class,
	                          sizeof (TrackerDBResultSetPrivate));
}

static void
tracker_db_result_set_init (TrackerDBResultSet *result_set)
{
	result_set->priv = TRACKER_DB_RESULT_SET_GET_PRIVATE_O (result_set);
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

TrackerDBStatement *
tracker_db_interface_create_statement (TrackerDBInterface  *interface,
                                       GError             **error,
                                       const gchar         *query,
                                       ...)
{
	TrackerDBStatement *stmt;
	TrackerDBInterfaceIface *iface;
	va_list args;
	gchar *str;

	/* Removed for performance 
	g_return_val_if_fail (TRACKER_IS_DB_INTERFACE (interface), NULL); */

	g_return_val_if_fail (interface != NULL, NULL);
	g_return_val_if_fail (query != NULL, NULL);

	va_start (args, query);
	str = g_strdup_vprintf (query, args);
	va_end (args);

	iface = TRACKER_DB_INTERFACE_GET_IFACE (interface);
	stmt = iface->create_statement (interface, error, str);
	g_free (str);

	return stmt;
}


TrackerDBResultSet *
tracker_db_interface_execute_vquery (TrackerDBInterface  *interface,
                                     GError             **error,
                                     const gchar         *query,
                                     va_list              args)
{
	TrackerDBResultSet *result_set = NULL;
	gchar *str;

	g_return_val_if_fail (TRACKER_IS_DB_INTERFACE (interface), NULL);
	g_return_val_if_fail (query != NULL, NULL);

	if (!TRACKER_DB_INTERFACE_GET_IFACE (interface)->execute_query) {
		g_critical ("Database abstraction %s doesn't implement "
		            "the method execute_vquery()",
		            G_OBJECT_TYPE_NAME (interface));
		return NULL;
	}

	str = g_strdup_vprintf (query, args);
	result_set = TRACKER_DB_INTERFACE_GET_IFACE (interface)->execute_query (interface,
		  error,
		  str);
	g_free (str);

	return ensure_result_set_state (result_set);
}



TrackerDBResultSet *
tracker_db_interface_execute_query (TrackerDBInterface  *interface,
                                    GError             **error,
                                    const gchar                 *query,
                                    ...)
{
	TrackerDBResultSet *result_set;
	va_list args;

	va_start (args, query);
	result_set = tracker_db_interface_execute_vquery (interface,
	                                                  error,
	                                                  query,
	                                                  args);
	va_end (args);

	return result_set;
}

gboolean
tracker_db_interface_interrupt (TrackerDBInterface *interface)
{
	g_return_val_if_fail (TRACKER_IS_DB_INTERFACE (interface), FALSE);

	if (!TRACKER_DB_INTERFACE_GET_IFACE (interface)->interrupt) {
		g_critical ("Database abstraction %s doesn't implement "
		            "the method interrupt()",
		            G_OBJECT_TYPE_NAME (interface));
		return FALSE;
	}

	return TRACKER_DB_INTERFACE_GET_IFACE (interface)->interrupt (interface);
}

gboolean
tracker_db_interface_start_transaction (TrackerDBInterface *interface)
{
	GError *error = NULL;

	tracker_db_interface_execute_query (interface,
	                                    &error,
	                                    "BEGIN TRANSACTION");

	if (error) {
		g_warning ("%s", error->message);
		g_error_free (error);
		return FALSE;
	}

	g_object_set (interface, "in-transaction", TRUE, NULL);

	return TRUE;
}

gboolean
tracker_db_interface_end_db_transaction (TrackerDBInterface *interface)
{
	gboolean in_transaction;
	GError *error = NULL;

	g_object_get (interface, "in-transaction", &in_transaction, NULL);

	if (!in_transaction) {
		return FALSE;
	}

	g_object_set (interface, "in-transaction", FALSE, NULL);
	tracker_db_interface_execute_query (interface, &error, "COMMIT");

	if (error) {
		g_warning ("%s", error->message);
		g_error_free (error);

		tracker_db_interface_execute_query (interface, NULL, "ROLLBACK");

		return FALSE;
	}

	return TRUE;
}


void
tracker_db_statement_bind_double (TrackerDBStatement    *stmt,
                                  int                    idx,
                                  double                 value)
{
	/* Removed for performance 
	g_return_val_if_fail (TRACKER_IS_DB_STATEMENT (stmt), NULL); */

	g_return_if_fail (stmt != NULL);

	TRACKER_DB_STATEMENT_GET_IFACE (stmt)->bind_double (stmt, idx, value);
}

void
tracker_db_statement_bind_int (TrackerDBStatement       *stmt,
                               int                       idx,
                               int                       value)
{
	/* Removed for performance:
	g_return_if_fail (TRACKER_IS_DB_STATEMENT (stmt)); */

	g_return_if_fail (stmt != NULL);

	TRACKER_DB_STATEMENT_GET_IFACE (stmt)->bind_int (stmt, idx, value);
}

void
tracker_db_statement_bind_int64 (TrackerDBStatement     *stmt,
                                 int                     idx,
                                 gint64                          value)
{
	/* Removed for performance 
	g_return_val_if_fail (TRACKER_IS_DB_STATEMENT (stmt), NULL); */

	g_return_if_fail (stmt != NULL);

	TRACKER_DB_STATEMENT_GET_IFACE (stmt)->bind_int64 (stmt, idx, value);
}

void
tracker_db_statement_bind_null (TrackerDBStatement      *stmt,
                                int                      idx)
{
	/* Removed for performance 
	g_return_val_if_fail (TRACKER_IS_DB_STATEMENT (stmt), NULL); */

	g_return_if_fail (stmt != NULL);

	TRACKER_DB_STATEMENT_GET_IFACE (stmt)->bind_null (stmt, idx);
}

void
tracker_db_statement_bind_text (TrackerDBStatement      *stmt,
                                int                      idx,
                                const gchar             *value)
{
	/* Removed for performance 
	g_return_val_if_fail (TRACKER_IS_DB_STATEMENT (stmt), NULL); */

	g_return_if_fail (stmt != NULL);

	TRACKER_DB_STATEMENT_GET_IFACE (stmt)->bind_text (stmt, idx, value);
}

TrackerDBResultSet *
tracker_db_statement_execute (TrackerDBStatement         *stmt,
                              GError                    **error)
{
	TrackerDBResultSet *result_set;

	/* Removed for performance 
	g_return_val_if_fail (TRACKER_IS_DB_STATEMENT (stmt), NULL); */

	g_return_val_if_fail (stmt != NULL, NULL); 

	result_set = TRACKER_DB_STATEMENT_GET_IFACE (stmt)->execute (stmt, error);

	return ensure_result_set_state (result_set);
}

TrackerDBCursor *
tracker_db_statement_start_cursor (TrackerDBStatement    *stmt,
                                   GError               **error)
{
	/* Removed for performance
	g_return_val_if_fail (TRACKER_IS_DB_STATEMENT (stmt), NULL); */

	g_return_val_if_fail (stmt != NULL, NULL);

	return TRACKER_DB_STATEMENT_GET_IFACE (stmt)->start_cursor (stmt, error);
}

/* TrackerDBCursor API */

void
tracker_db_cursor_rewind (TrackerDBCursor *cursor)
{
	g_return_if_fail (TRACKER_IS_DB_CURSOR (cursor));

	TRACKER_DB_CURSOR_GET_IFACE (cursor)->rewind (cursor);
}

gboolean
tracker_db_cursor_iter_next (TrackerDBCursor *cursor,
                             GError         **error)
{
	/* Removed for performance 
	g_return_val_if_fail (TRACKER_IS_DB_CURSOR (cursor), FALSE); */

	g_return_val_if_fail (cursor != NULL, FALSE);

	return TRACKER_DB_CURSOR_GET_IFACE (cursor)->iter_next (cursor, error);
}

guint
tracker_db_cursor_get_n_columns (TrackerDBCursor *cursor)
{
	g_return_val_if_fail (TRACKER_IS_DB_CURSOR (cursor), 0);

	return TRACKER_DB_CURSOR_GET_IFACE (cursor)->get_n_columns (cursor);
}

void
tracker_db_cursor_get_value (TrackerDBCursor *cursor,  guint column, GValue *value)
{
	/* Removed for performance 
	g_return_if_fail (TRACKER_IS_DB_CURSOR (cursor)); */

	g_return_if_fail (cursor != NULL);

	TRACKER_DB_CURSOR_GET_IFACE (cursor)->get_value (cursor, column, value);
}

const gchar*
tracker_db_cursor_get_string (TrackerDBCursor *cursor, guint            column)
{
	/* Removed for performance 
	g_return_val_if_fail (TRACKER_IS_DB_CURSOR (cursor), NULL); */

	g_return_val_if_fail (cursor != NULL, NULL);

	return TRACKER_DB_CURSOR_GET_IFACE (cursor)->get_string (cursor, column);
}

gint
tracker_db_cursor_get_int (TrackerDBCursor *cursor, guint            column)
{
	/* Removed for performance
	g_return_val_if_fail (TRACKER_IS_DB_CURSOR (cursor), -1); */

	g_return_val_if_fail (cursor != NULL, -1);

	return TRACKER_DB_CURSOR_GET_IFACE (cursor)->get_int (cursor, column);
}

gdouble
tracker_db_cursor_get_double (TrackerDBCursor *cursor, guint            column)
{
	/* Removed for performance
	g_return_val_if_fail (TRACKER_IS_DB_CURSOR (cursor), -1); */

	g_return_val_if_fail (cursor != NULL, -1);

	return TRACKER_DB_CURSOR_GET_IFACE (cursor)->get_double (cursor, column);
}



/* TrackerDBResultSet semiprivate API */
TrackerDBResultSet *
_tracker_db_result_set_new (guint columns)
{
	return g_object_new (TRACKER_TYPE_DB_RESULT_SET,
	                     "columns", columns,
	                     NULL);
}

void
_tracker_db_result_set_append (TrackerDBResultSet *result_set)
{
	TrackerDBResultSetPrivate *priv;

	g_return_if_fail (TRACKER_IS_DB_RESULT_SET (result_set));

	priv = TRACKER_DB_RESULT_SET_GET_PRIVATE (result_set);

	if (G_UNLIKELY (!priv->array)) {
		priv->array = g_ptr_array_sized_new (100);
	}

	g_ptr_array_add (priv->array, NULL);
	priv->current_row = priv->array->len - 1;
}

void
_tracker_db_result_set_set_value (TrackerDBResultSet *result_set,
                                  guint                       column,
                                  const GValue       *value)
{
	TrackerDBResultSetPrivate *priv;
	gpointer *row = NULL;

	g_return_if_fail (TRACKER_IS_DB_RESULT_SET (result_set));

	/* just return if the value doesn't contain anything */
	if (G_VALUE_TYPE (value) == 0)
		return;

	priv = TRACKER_DB_RESULT_SET_GET_PRIVATE (result_set);

	g_return_if_fail (column < priv->columns);

	/* Assign a GType if it didn't have any */
	/* if (G_UNLIKELY (priv->col_types[column] == 0)) */
	priv->col_types[column] = G_VALUE_TYPE (value);

	row = g_ptr_array_index (priv->array, priv->current_row);

	/* Allocate space for the row, if it wasn't allocated previously */
	if (G_UNLIKELY (!row)) {
		row = g_new0 (gpointer, priv->columns);
		g_ptr_array_index (priv->array, priv->current_row) = row;
	}

	switch (priv->col_types [column]) {
	case G_TYPE_INT: {
		gint *val;

		val = g_new (gint, 1);
		*val = g_value_get_int (value);
		row[column] = val;
		break;
	}
	case G_TYPE_DOUBLE: {
		gdouble *val;

		val = g_new (gdouble, 1);
		*val = g_value_get_double (value);
		row[column] = val;
		break;
	}
	case G_TYPE_STRING:
		row[column] = (gpointer) g_value_dup_string (value);
		break;
	default:
		g_warning ("Unknown type for resultset: %s\n", G_VALUE_TYPE_NAME (value));
	}
}

static void
fill_in_value (GValue   *value,
               gpointer  data)
{
	switch (G_VALUE_TYPE (value)) {
	case G_TYPE_INT:
		g_value_set_int (value, *((gint*) data));
		break;
	case G_TYPE_DOUBLE:
		g_value_set_double (value, *((gdouble *) data));
		break;
	case G_TYPE_STRING:
		g_value_set_string (value, data);
		break;
	default:
		g_warning ("Unknown type for resultset: %s\n", G_VALUE_TYPE_NAME (value));
		break;
	}
}

void
_tracker_db_result_set_get_value (TrackerDBResultSet *result_set,
                                  guint                       column,
                                  GValue             *value)
{
	TrackerDBResultSetPrivate *priv;
	gpointer *row;

	g_return_if_fail (TRACKER_IS_DB_RESULT_SET (result_set));

	priv = TRACKER_DB_RESULT_SET_GET_PRIVATE (result_set);
	row = g_ptr_array_index (priv->array, priv->current_row);

	if (priv->col_types[column] != G_TYPE_INVALID && row && row[column]) {
		g_value_init (value, priv->col_types[column]);
		fill_in_value (value, row[column]);
	} else {
		/* NULL, keep value unset */
	}
}

/* TrackerDBResultSet API */
void
tracker_db_result_set_get (TrackerDBResultSet *result_set,
                           ...)
{
	TrackerDBResultSetPrivate *priv;
	va_list args;
	gint n_col;
	GValue value = { 0, };
	gpointer *row;
	gchar *error = NULL;

	g_return_if_fail (TRACKER_IS_DB_RESULT_SET (result_set));

	priv = TRACKER_DB_RESULT_SET_GET_PRIVATE (result_set);
	g_return_if_fail (priv->array != NULL);

	row = g_ptr_array_index (priv->array, priv->current_row);
	va_start (args, result_set);

	while ((n_col = va_arg (args, gint)) >= 0) {
		if ((guint) n_col >= priv->columns) {
			g_critical ("Result set has %d columns, trying to access column %d, "
			            "maybe -1 is missing at the end of the arguments?",
			            priv->columns, n_col);
			break;
		}

		if (priv->col_types[n_col] != G_TYPE_INVALID) {
			g_value_init (&value, priv->col_types[n_col]);
			fill_in_value (&value, row[n_col]);
			G_VALUE_LCOPY (&value, args, 0, &error);
			g_value_unset (&value);
		} else {
			gpointer *pointer;

			/* No valid type, set to NULL/0 */
			pointer = va_arg (args, gpointer *);
			*pointer = NULL;
		}

		if (error) {
			g_warning ("%s", error);
			g_free (error);
		}
	}

	va_end (args);
}

void
tracker_db_result_set_rewind (TrackerDBResultSet *result_set)
{
	TrackerDBResultSetPrivate *priv;

	g_return_if_fail (TRACKER_IS_DB_RESULT_SET (result_set));

	priv = TRACKER_DB_RESULT_SET_GET_PRIVATE (result_set);
	priv->current_row = 0;
}

gboolean
tracker_db_result_set_iter_next (TrackerDBResultSet *result_set)
{
	TrackerDBResultSetPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_DB_RESULT_SET (result_set), FALSE);

	priv = TRACKER_DB_RESULT_SET_GET_PRIVATE (result_set);

	if (priv->current_row + 1 >= priv->array->len)
		return FALSE;

	priv->current_row++;
	return TRUE;
}

guint
tracker_db_result_set_get_n_columns (TrackerDBResultSet *result_set)
{
	TrackerDBResultSetPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_DB_RESULT_SET (result_set), 0);

	priv = TRACKER_DB_RESULT_SET_GET_PRIVATE (result_set);

	return priv->columns;
}

guint
tracker_db_result_set_get_n_rows (TrackerDBResultSet *result_set)
{
	TrackerDBResultSetPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_DB_RESULT_SET (result_set), 0);

	priv = TRACKER_DB_RESULT_SET_GET_PRIVATE (result_set);

	if (!priv->array)
		return 0;

	return priv->array->len;
}
