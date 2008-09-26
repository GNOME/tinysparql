/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
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

#include <string.h>

#include <gobject/gvaluecollector.h>

#include "tracker-db-interface.h"

#define TRACKER_DB_RESULT_SET_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_DB_RESULT_SET, TrackerDBResultSetPrivate))

typedef struct TrackerDBResultSetPrivate TrackerDBResultSetPrivate;

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

/* Boxed type for blobs */
static gpointer
blob_copy (gpointer boxed)
{
	GByteArray *array, *copy;

	array = (GByteArray *) boxed;
	copy = g_byte_array_sized_new (array->len);
	g_byte_array_append (copy, array->data, array->len);

	return copy;
}

static void
blob_free (gpointer boxed)
{
	GByteArray *array;

	array = (GByteArray *) boxed;
	g_byte_array_free (array, TRUE);
}

GType
tracker_db_blob_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		type = g_boxed_type_register_static ("TrackerDBBlob",
						     blob_copy,
						     blob_free);
	}

	return type;
}

/* TrackerDBResultSet */
static void
tracker_db_result_set_set_property (GObject	  *object,
				    guint	   prop_id,
				    const GValue  *value,
				    GParamSpec	  *pspec)
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
				    guint	prop_id,
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

TrackerDBResultSet *
tracker_db_interface_execute_vquery (TrackerDBInterface  *interface,
				     GError		**error,
				     const gchar	 *query,
				     va_list		  args)
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
tracker_db_interface_execute_query (TrackerDBInterface	*interface,
				    GError	       **error,
				    const gchar		*query,
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

void
tracker_db_interface_set_procedure_table (TrackerDBInterface *interface,
					  GHashTable	     *procedure_table)
{
	g_return_if_fail (TRACKER_IS_DB_INTERFACE (interface));
	g_return_if_fail (procedure_table != NULL);

	if (!TRACKER_DB_INTERFACE_GET_IFACE (interface)->set_procedure_table) {
		g_critical ("Database abstraction %s doesn't implement "
			    "the method set_procedure_table()",
			    G_OBJECT_TYPE_NAME (interface));
		return;
	}

	TRACKER_DB_INTERFACE_GET_IFACE (interface)->set_procedure_table (interface,
									 procedure_table);
}

TrackerDBResultSet *
tracker_db_interface_execute_vprocedure (TrackerDBInterface  *interface,
					 GError		    **error,
					 const gchar	     *procedure,
					 va_list	      args)
{
	TrackerDBResultSet *result_set;

	g_return_val_if_fail (TRACKER_IS_DB_INTERFACE (interface), NULL);
	g_return_val_if_fail (procedure != NULL, NULL);

	if (!TRACKER_DB_INTERFACE_GET_IFACE (interface)->execute_procedure) {
		g_critical ("Database abstraction %s doesn't implement "
			    "the method execute_procedure()",
			    G_OBJECT_TYPE_NAME (interface));
		return NULL;
	}

	result_set = TRACKER_DB_INTERFACE_GET_IFACE (interface)->execute_procedure (interface,
										    error,
										    procedure,
										    args);

	return ensure_result_set_state (result_set);
}



TrackerDBResultSet *
tracker_db_interface_execute_vprocedure_len (TrackerDBInterface  *interface,
					     GError		**error,
					     const gchar	 *procedure,
					     va_list		  args)
{
	TrackerDBResultSet *result_set;

	g_return_val_if_fail (TRACKER_IS_DB_INTERFACE (interface), NULL);
	g_return_val_if_fail (procedure != NULL, NULL);

	if (!TRACKER_DB_INTERFACE_GET_IFACE (interface)->execute_procedure_len) {
		g_critical ("Database abstraction %s doesn't implement "
			    "the method execute_procedure_len()",
			    G_OBJECT_TYPE_NAME (interface));
		return NULL;
	}

	result_set = TRACKER_DB_INTERFACE_GET_IFACE (interface)->execute_procedure_len (interface,
											error,
											procedure,
											args);

	return ensure_result_set_state (result_set);
}

TrackerDBResultSet *
tracker_db_interface_execute_procedure (TrackerDBInterface  *interface,
					GError		   **error,
					const gchar	    *procedure,
					...)
{
	TrackerDBResultSet *result_set;
	va_list args;

	va_start (args, procedure);
	result_set = tracker_db_interface_execute_vprocedure (interface,
							      error,
							      procedure,
							      args);
	va_end (args);

	return result_set;
}

TrackerDBResultSet *
tracker_db_interface_execute_procedure_len (TrackerDBInterface	*interface,
					    GError	       **error,
					    const gchar		*procedure,
					    ...)
{
	TrackerDBResultSet *result_set;
	va_list args;

	va_start (args, procedure);
	result_set = tracker_db_interface_execute_vprocedure_len (interface,
								  error,
								  procedure,
								  args);
	va_end (args);

	return result_set;
}

gboolean
tracker_db_interface_start_transaction (TrackerDBInterface *interface)
{
	GError *error = NULL;

	tracker_db_interface_execute_query (interface,
					    &error,
					    "BEGIN TRANSACTION");

	if (error) {
		g_warning (error->message);
		g_error_free (error);
		return FALSE;
	}

	g_object_set (interface, "in-transaction", TRUE, NULL);

	return TRUE;
}

gboolean
tracker_db_interface_end_transaction (TrackerDBInterface *interface)
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
		g_warning (error->message);
		g_error_free (error);

		tracker_db_interface_execute_query (interface, NULL, "ROLLBACK");

		return FALSE;
	}

	return TRUE;
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
				  guint		      column,
				  const GValue	     *value)
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
fill_in_value (GValue	*value,
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
	}
}

void
_tracker_db_result_set_get_value (TrackerDBResultSet *result_set,
				  guint		      column,
				  GValue	     *value)
{
	TrackerDBResultSetPrivate *priv;
	gpointer *row;

	g_return_if_fail (TRACKER_IS_DB_RESULT_SET (result_set));

	priv = TRACKER_DB_RESULT_SET_GET_PRIVATE (result_set);
	row = g_ptr_array_index (priv->array, priv->current_row);

	if (priv->col_types[column] != G_TYPE_INVALID) {
		g_value_init (value, priv->col_types[column]);
		if (row && row[column]) {
			fill_in_value (value, row[column]);
		} else {
			/* Make up some empty value. */
			switch (G_VALUE_TYPE (value)) {
			case G_TYPE_INT:
				g_value_set_int (value, 0);
				break;
			case G_TYPE_DOUBLE:
				g_value_set_double (value, 0.0);
				break;
			case G_TYPE_STRING:
				g_value_set_string (value, "");
				break;
			}
		}
	} else {
		/* Make up some empty value */
		g_value_init (value, G_TYPE_STRING);
		g_value_set_string (value, "");
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
			g_warning (error);
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
