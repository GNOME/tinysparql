/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia
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

#include <libtracker-common/tracker-dbus.h>

#include "tracker-db-dbus.h"

typedef struct {
	gint     key;
	gpointer value;
} OneRow;

typedef struct {
	gpointer value;
} OneElem;

static inline void
row_add (GPtrArray *row, 
	 gchar     *value)
{
	OneElem *elem;
	GSList  *list = NULL;

	elem = g_slice_new (OneElem);
	list = NULL;

	list = g_slist_prepend (list, value);
	elem->value = list;
	g_ptr_array_add (row, elem);
}

static inline void
row_insert (GPtrArray *row, 
	    gchar     *value, 
	    guint      lindex)
{
	OneElem *elem;
	GSList  *list;
	GSList  *iter;

	elem = g_ptr_array_index (row, lindex);
	list = elem->value;

	/* We check for duplicate values here so that
	 * we can have several multivalued fields in
	 * the same query.
	 */
	for (iter = list; iter; iter=iter->next) {
		if (strcmp (iter->data, value) == 0) {
			return;
		}
	}

	list = g_slist_prepend (list, value);
	elem->value = list;
}

static inline void
row_destroy (GPtrArray *row)
{
	guint i;

	for (i = 0; i < row->len; i++) {
		OneElem *elem;
		GSList  *list;

		elem = g_ptr_array_index (row, i);
		list = elem->value;
		g_slist_foreach (list,
				 (GFunc) g_free,
				 NULL);
		g_slist_free (list);
		g_slice_free (OneElem, elem);
	}

	g_ptr_array_free (row, TRUE);
}

static inline gpointer
rows_lookup (GPtrArray *rows,
	     gint       key)
{
	guint	 i;
	gpointer value = NULL;

	for (i = 0; i < rows->len; i++) {
		OneRow *row;

		row = g_ptr_array_index (rows, i);

		if (row->key == key) {
			value = row->value;
			break;
		}
	}

	return value;
}

static inline void
rows_destroy (GPtrArray *rows)
{
	guint i;

	for (i = 0; i < rows->len; i++) {
		OneRow *row;
		row = g_ptr_array_index (rows, i);
		row_destroy (row->value);
		g_slice_free (OneRow, row);
	}

	g_ptr_array_free (rows, TRUE);
}

static inline void
rows_add (GPtrArray *rows, 
	  gint       key, 
	  gpointer   value)
{
	OneRow *row;

	row = g_slice_new (OneRow);

	row->key = key;
	row->value = value;

	g_ptr_array_add (rows, row);
}

static inline void
rows_migrate (GPtrArray *rows,
	      GPtrArray *result)
{
	guint i, j;

	/* Go thought the lists and join with | separator */
	for (i = 0; i < rows->len; i++) {
		OneRow     *row;
		GPtrArray  *array;
		gchar     **strv;

		row   = g_ptr_array_index (rows, i);
		array = row->value;

		strv = g_new0 (gchar*, array->len + 1);

		for (j = 0; j < array->len; j++) {
			OneElem *elem;
			GSList  *list;
			GSList  *iter;
			GString *string;

			elem   = g_ptr_array_index (array, j);
			list   = elem->value;
			string = g_string_new((gchar *)list->data);

			for (iter = list->next; iter; iter = iter->next) {
				g_string_append_printf (string, "|%s", (gchar *)iter->data);
			}

			strv[j] = string->str;

			g_string_free (string, FALSE);
		}

		strv[array->len] = NULL;

		g_ptr_array_add (result, strv);
	}
}

static gchar **
dbus_query_result_to_strv (TrackerDBResultSet *result_set,
			   gint		       column,
			   gint		      *count,
			   gboolean	       numeric)

{
	gchar **strv = NULL;
	gint	rows = 0;
	gint	i = 0;

	if (result_set) {
		gchar	 *str;
		gboolean  valid = TRUE;
		gint	  value;

		/* Make sure we rewind before iterating the result set */
		tracker_db_result_set_rewind (result_set);

		rows = tracker_db_result_set_get_n_rows (result_set);
		strv = g_new (gchar*, rows + 1);

		while (valid) {
			if (numeric) {
				tracker_db_result_set_get (result_set, column, &value, -1);
				str = g_strdup_printf ("%d", value);
			} else {
				tracker_db_result_set_get (result_set, column, &str, -1);
			}

			if (!str) {
				valid = tracker_db_result_set_iter_next (result_set);
				continue;
			}

			if (!g_utf8_validate (str, -1, NULL)) {
				g_warning ("Could not add string:'%s' to GStrv, invalid UTF-8", str);
				g_free (str);
				str = g_strdup ("");
			}

			strv[i++] = str;
			valid = tracker_db_result_set_iter_next (result_set);
		}

		strv[i] = NULL;
	}

	if (count) {
		*count = i;
	}

	return strv;
}

gchar **
tracker_dbus_query_result_to_strv (TrackerDBResultSet *result_set,
				   gint		       column,
				   gint		      *count)
{
	return dbus_query_result_to_strv (result_set, column, count, FALSE);
}

gchar **
tracker_dbus_query_result_numeric_to_strv (TrackerDBResultSet *result_set,
					   gint		       column,
					   gint		      *count)
{
	return dbus_query_result_to_strv (result_set, column, count, TRUE);
}

gchar **
tracker_dbus_query_result_columns_to_strv (TrackerDBResultSet *result_set,
					   gint                offset_column,
					   gint                until_column,
					   gboolean            rewind)
{
	gchar    **strv = NULL;
	gint	   i = 0;
	gint	   columns;
	gint       row_counter = 0;
	gboolean   valid = TRUE;

	if (result_set) {
		columns = tracker_db_result_set_get_n_columns (result_set);
		if (rewind) {
			 /* Make sure we rewind before iterating the result set */
			tracker_db_result_set_rewind (result_set);
		}
	}

	if (!result_set || offset_column > columns) {
		strv = g_new (gchar*, 1);
		strv[0] = NULL;
		return strv;
	} else if (offset_column == -1) {
		offset_column = 0;
	}

	if (until_column == -1) {
		until_column = columns;
	}

	strv = g_new (gchar*, until_column + 1);

	while (valid) {

		for (i = offset_column ; i < until_column; i++) {
			GValue value = {0, };
			GValue transform = {0, };
			
			g_value_init (&transform, G_TYPE_STRING);
			
			_tracker_db_result_set_get_value (result_set, i, &value);
			if (g_value_transform (&value, &transform)) {
				if (row_counter == 0) {
					strv[i] = g_value_dup_string (&transform);
				} else {
					gchar *new_value, *old_value;

					new_value = g_value_dup_string (&transform);
					if (new_value != NULL && strlen (new_value) > 0) {
						old_value = strv [i];

						strv[i] = g_strconcat (old_value, "|", new_value, NULL);
						
						g_free (old_value);
					}

					if (new_value) {
						g_free (new_value);
					}
					
				}
			}
			g_value_unset (&value);
			g_value_unset (&transform);
		}

		row_counter += 1;
		valid = tracker_db_result_set_iter_next (result_set);
	}
	strv[i] = NULL;

	return strv;
}


GHashTable *
tracker_dbus_query_result_to_hash_table (TrackerDBResultSet *result_set)
{
	GHashTable *hash_table;
	gint	    field_count;
	gboolean    valid = FALSE;

	hash_table = g_hash_table_new_full (g_str_hash,
					    g_str_equal,
					    (GDestroyNotify) g_free,
					    (GDestroyNotify) tracker_dbus_gvalue_slice_free);

	if (result_set) {
		valid = TRUE;

		/* Make sure we rewind before iterating the result set */
		tracker_db_result_set_rewind (result_set);

		/* Find out how many columns to iterate */
		field_count = tracker_db_result_set_get_n_columns (result_set);
	}

	while (valid) {
		GValue	 transform = { 0, };
		GValue	*values;
		gchar  **p;
		gint	 i = 0;
		gchar	*key;
		GSList	*list = NULL;

		g_value_init (&transform, G_TYPE_STRING);

		tracker_db_result_set_get (result_set, 0, &key, -1);
		values = tracker_dbus_gvalue_slice_new (G_TYPE_STRV);

		for (i = 1; i < field_count; i++) {
			GValue	value = { 0, };
			gchar  *str;

			_tracker_db_result_set_get_value (result_set, i, &value);

			if (g_value_transform (&value, &transform)) {
				str = g_value_dup_string (&transform);

				if (!g_utf8_validate (str, -1, NULL)) {
					g_warning ("Could not add string:'%s' to GStrv, invalid UTF-8", str);
					g_free (str);
					str = g_strdup ("");
				}
			} else {
				str = g_strdup ("");
			}

			list = g_slist_prepend (list, (gchar*) str);
		}

		list = g_slist_reverse (list);
		p = tracker_dbus_slist_to_strv (list);
		g_slist_free (list);
		g_value_take_boxed (values, p);
		g_hash_table_insert (hash_table, key, values);

		valid = tracker_db_result_set_iter_next (result_set);
	}

	return hash_table;
}

GPtrArray *
tracker_dbus_query_result_to_ptr_array (TrackerDBResultSet *result_set)
{
	GPtrArray *ptr_array;
	gboolean   valid = FALSE;
	gint	   columns;
	gint	   i;

	ptr_array = g_ptr_array_new ();

	if (result_set) {
		valid = TRUE;

		/* Make sure we rewind before iterating the result set */
		tracker_db_result_set_rewind (result_set);

		/* Find out how many columns to iterate */
		columns = tracker_db_result_set_get_n_columns (result_set);
	}

	while (valid) {
		GSList  *list = NULL;
		gchar  **p;

		/* Append fields to the array */
		for (i = 0; i < columns; i++) {
			GValue	transform = { 0, };
			GValue	value = { 0, };
			gchar  *str;

			g_value_init (&transform, G_TYPE_STRING);

			_tracker_db_result_set_get_value (result_set, i, &value);

			if (g_value_transform (&value, &transform)) {
				str = g_value_dup_string (&transform);

				if (!str) {
					str = g_strdup ("");
				} else if (!g_utf8_validate (str, -1, NULL)) {
					g_warning ("Could not add string:'%s' to GStrv, invalid UTF-8", str);
					g_free (str);
					str = g_strdup ("");
				}
			} else {
				str = g_strdup ("");
			}

			list = g_slist_prepend (list, (gchar*) str);

			g_value_unset (&value);
			g_value_unset (&transform);
		}

		list = g_slist_reverse (list);
		p = tracker_dbus_slist_to_strv (list);

		g_slist_foreach (list, (GFunc)g_free, NULL);
		g_slist_free (list);

		g_ptr_array_add (ptr_array, p);

		valid = tracker_db_result_set_iter_next (result_set);
	}

	return ptr_array;
}

GPtrArray *
tracker_dbus_query_result_multi_to_ptr_array (TrackerDBResultSet *result_set)
{
	GPtrArray *result;
	GPtrArray *rows;
	gboolean   valid = FALSE;
	gint	   columns;

	rows = g_ptr_array_new ();

	if (result_set) {
		valid = TRUE;

		/* Make sure we rewind before iterating the result set */
		tracker_db_result_set_rewind (result_set);

		/* Find out how many columns to iterate */
		columns = tracker_db_result_set_get_n_columns (result_set);
	}

	while (valid) {
		GPtrArray *row;
		GValue	   value_in = {0, };
		gint	   key;		
		gint       column;
		gboolean   add = FALSE;

		/* Get the key and the matching row if exists */
		_tracker_db_result_set_get_value (result_set, 0, &value_in);
		key = g_value_get_int (&value_in);		
		row = rows_lookup (rows, key);				
		if (!row) {
			row = g_ptr_array_new ();
			add = TRUE;
		}

		/* Append fields or values to the array */
		for (column = 1; column < columns; column++) {
			GValue  transform = { 0, };
			GValue  value = { 0, };
			gchar  *str;

			g_value_init (&transform, G_TYPE_STRING);

			_tracker_db_result_set_get_value (result_set,
							  column,
							  &value);

			if (g_value_transform (&value, &transform)) {
				str = g_value_dup_string (&transform);
				
				if (!str) {
					str = g_strdup ("");
				} else if (!g_utf8_validate (str, -1, NULL)) {
					g_warning ("Could not add string:'%s' to GStrv, invalid UTF-8", str);
					g_free (str);
					str = g_strdup ("");
				}
			} else {
				str = g_strdup ("");				
			}

			if (add) {
				row_add (row, str);
			} else {				
				row_insert (row, str, column-1);
			}		       

			g_value_unset (&value);
			g_value_unset (&transform);
		}


		if (add) {
			rows_add (rows, key, row);
		}

		valid = tracker_db_result_set_iter_next (result_set);
	}

	result = g_ptr_array_new();

	rows_migrate (rows, result);
	rows_destroy (rows);
	
	return result;
}
