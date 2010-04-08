/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
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

#include <libtracker-common/tracker-dbus.h>

#include "tracker-db-dbus.h"

gchar **
tracker_dbus_query_result_to_strv (TrackerDBResultSet *result_set,
                                   gint                column,
                                   gint               *count)
{
	gchar **strv = NULL;
	gint    rows = 0;
	gint    i = 0;

	if (result_set) {
		gchar    *str;
		gboolean  valid = TRUE;

		/* Make sure we rewind before iterating the result set */
		tracker_db_result_set_rewind (result_set);

		rows = tracker_db_result_set_get_n_rows (result_set);
		strv = g_new (gchar*, rows + 1);

		while (valid) {
			tracker_db_result_set_get (result_set, column, &str, -1);

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

GPtrArray *
tracker_dbus_query_result_to_ptr_array (TrackerDBResultSet *result_set)
{
	GPtrArray *ptr_array;
	gboolean   valid = FALSE;
	gint       columns;
	gint       i;

	ptr_array = g_ptr_array_new ();

	if (result_set) {
		valid = TRUE;

		/* Make sure we rewind before iterating the result set */
		tracker_db_result_set_rewind (result_set);

		/* Find out how many columns to iterate */
		columns = tracker_db_result_set_get_n_columns (result_set);
	}

	while (valid) {
		gchar  **p;

		p = g_new0 (gchar *, columns + 1);

		/* Append fields to the array */
		for (i = 0; i < columns; i++) {
			GValue  transform = { 0, };
			GValue  value = { 0, };

			g_value_init (&transform, G_TYPE_STRING);

			_tracker_db_result_set_get_value (result_set, i, &value);

			if (G_IS_VALUE (&value) && g_value_transform (&value, &transform)) {
				p[i] = g_value_dup_string (&transform);
			}

			if (!p[i]) {
				p[i] = g_strdup ("");
			}

			if (G_IS_VALUE (&value)) {
				g_value_unset (&value);
			}
			g_value_unset (&transform);
		}

		g_ptr_array_add (ptr_array, p);

		valid = tracker_db_result_set_iter_next (result_set);
	}

	return ptr_array;
}
