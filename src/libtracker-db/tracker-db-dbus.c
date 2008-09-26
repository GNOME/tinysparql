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

#include <libtracker-common/tracker-dbus.h>

#include "tracker-db-dbus.h"

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
					   gint offset_column,
					   gint until_column,
					   gboolean rewind)
{
	gchar **strv = NULL;
	gint	i = 0;
	gint	columns;

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
	} else if (offset_column == -1)
		offset_column = 0;

	if (until_column == -1)
		until_column = columns;

	strv = g_new (gchar*, until_column + 1);


	for (i = offset_column ; i < until_column; i++) {
		GValue value = {0, };
		GValue	transform = {0, };

		g_value_init (&transform, G_TYPE_STRING);

		_tracker_db_result_set_get_value (result_set, i, &value);
		if (g_value_transform (&value, &transform)) {
			strv[i] = g_value_dup_string (&transform);
		}
		g_value_unset (&value);
		g_value_unset (&transform);
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
		GSList	*list = NULL;
		gchar  **p;

		/* Append fields to the array */
		for (i = 0; i < columns; i++) {
			GValue	 transform = { 0, };
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

