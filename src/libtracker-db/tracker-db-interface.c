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

struct TrackerDBResultSet {
	GObject parent_class;
	GType *col_types;
	GPtrArray *array;
	guint columns;
	guint current_row;
};

struct TrackerDBResultSetClass {
	GObjectClass parent_class;
};

G_DEFINE_TYPE (TrackerDBResultSet, tracker_db_result_set, G_TYPE_OBJECT)

GQuark
tracker_db_interface_error_quark (void)
{
	return g_quark_from_static_string ("tracker-db-interface-error-quark");
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
	TrackerDBResultSet *result_set;

	result_set = TRACKER_DB_RESULT_SET (object);

	if (result_set->array) {
		g_ptr_array_foreach (result_set->array, (GFunc) free_row,
		                     GUINT_TO_POINTER (result_set->columns));
		g_ptr_array_free (result_set->array, TRUE);
	}

	g_free (result_set->col_types);

	G_OBJECT_CLASS (tracker_db_result_set_parent_class)->finalize (object);
}

static void
tracker_db_result_set_class_init (TrackerDBResultSetClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->finalize = tracker_db_result_set_finalize;
}

static void
tracker_db_result_set_init (TrackerDBResultSet *result_set)
{
}

TrackerDBResultSet *
tracker_db_interface_execute_query (TrackerDBInterface  *interface,
                                    GError             **error,
                                    const gchar         *query,
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

	return TRUE;
}

gboolean
tracker_db_interface_end_db_transaction (TrackerDBInterface *interface)
{
	GError *error = NULL;

	tracker_db_interface_execute_query (interface, &error, "COMMIT");

	if (error) {
		g_warning ("%s", error->message);
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
	TrackerDBResultSet *result_set;

	result_set = g_object_new (TRACKER_TYPE_DB_RESULT_SET, NULL);

	result_set->columns = columns;
	result_set->col_types = g_new0 (GType, result_set->columns);

	return result_set;
}

void
_tracker_db_result_set_append (TrackerDBResultSet *result_set)
{
	g_return_if_fail (TRACKER_IS_DB_RESULT_SET (result_set));

	if (G_UNLIKELY (!result_set->array)) {
		result_set->array = g_ptr_array_sized_new (100);
	}

	g_ptr_array_add (result_set->array, NULL);
	result_set->current_row = result_set->array->len - 1;
}

void
_tracker_db_result_set_set_value (TrackerDBResultSet *result_set,
                                  guint               column,
                                  const GValue       *value)
{
	gpointer *row = NULL;

	g_return_if_fail (TRACKER_IS_DB_RESULT_SET (result_set));

	/* just return if the value doesn't contain anything */
	if (G_VALUE_TYPE (value) == 0)
		return;

	g_return_if_fail (column < result_set->columns);

	/* Assign a GType if it didn't have any */
	/* if (G_UNLIKELY (result_set->col_types[column] == 0)) */
	result_set->col_types[column] = G_VALUE_TYPE (value);

	row = g_ptr_array_index (result_set->array, result_set->current_row);

	/* Allocate space for the row, if it wasn't allocated previously */
	if (G_UNLIKELY (!row)) {
		row = g_new0 (gpointer, result_set->columns);
		g_ptr_array_index (result_set->array, result_set->current_row) = row;
	}

	switch (result_set->col_types [column]) {
	case G_TYPE_INT64: {
		gint64 *val;

		val = g_new (gint64, 1);
		*val = g_value_get_int64 (value);
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
	case G_TYPE_INT64:
		g_value_set_int64 (value, *((gint64*) data));
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
                                  guint               column,
                                  GValue             *value)
{
	gpointer *row;

	g_return_if_fail (TRACKER_IS_DB_RESULT_SET (result_set));

	row = g_ptr_array_index (result_set->array, result_set->current_row);

	if (result_set->col_types[column] != G_TYPE_INVALID && row && row[column]) {
		g_value_init (value, result_set->col_types[column]);
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
	va_list args;
	gint n_col;
	GValue value = { 0, };
	gpointer *row;
	gchar *error = NULL;

	g_return_if_fail (TRACKER_IS_DB_RESULT_SET (result_set));

	g_return_if_fail (result_set->array != NULL);

	row = g_ptr_array_index (result_set->array, result_set->current_row);
	va_start (args, result_set);

	while ((n_col = va_arg (args, gint)) >= 0) {
		if ((guint) n_col >= result_set->columns) {
			g_critical ("Result set has %d columns, trying to access column %d, "
			            "maybe -1 is missing at the end of the arguments?",
			            result_set->columns, n_col);
			break;
		}

		if (result_set->col_types[n_col] != G_TYPE_INVALID) {
			g_value_init (&value, result_set->col_types[n_col]);
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
	g_return_if_fail (TRACKER_IS_DB_RESULT_SET (result_set));

	result_set->current_row = 0;
}

gboolean
tracker_db_result_set_iter_next (TrackerDBResultSet *result_set)
{
	g_return_val_if_fail (TRACKER_IS_DB_RESULT_SET (result_set), FALSE);

	if (result_set->current_row + 1 >= result_set->array->len)
		return FALSE;

	result_set->current_row++;
	return TRUE;
}

guint
tracker_db_result_set_get_n_columns (TrackerDBResultSet *result_set)
{
	g_return_val_if_fail (TRACKER_IS_DB_RESULT_SET (result_set), 0);

	return result_set->columns;
}

guint
tracker_db_result_set_get_n_rows (TrackerDBResultSet *result_set)
{
	g_return_val_if_fail (TRACKER_IS_DB_RESULT_SET (result_set), 0);

	if (!result_set->array)
		return 0;

	return result_set->array->len;
}
