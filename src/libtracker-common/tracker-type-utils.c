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

#include "config.h"

#define _XOPEN_SOURCE
#include <time.h>

#include <strings.h>
#include <string.h>
#include <stdlib.h>

#include <glib.h>

#include "tracker-log.h"
#include "tracker-utils.h"
#include "tracker-type-utils.h"

gchar *
tracker_glong_to_string (glong i)
{
	return g_strdup_printf ("%ld", i);
}

gchar *
tracker_gint_to_string (gint i)
{
	return g_strdup_printf ("%d", i);
}

gchar *
tracker_guint_to_string (guint i)
{
	return g_strdup_printf ("%u", i);
}

gchar *
tracker_gint32_to_string (gint32 i)
{
	return g_strdup_printf ("%" G_GINT32_FORMAT, i);
}

gchar *
tracker_guint32_to_string (guint32 i)
{
	return g_strdup_printf ("%" G_GUINT32_FORMAT, i);
}

gboolean
tracker_string_to_uint (const gchar *s,
                        guint       *value)
{
	unsigned long int  n;
	gchar             *end;

	g_return_val_if_fail (s != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);

	n = (guint) strtoul (s, &end, 10);

	if (end == s) {
		*value = 0;
		return FALSE;
	}

	if (n > G_MAXUINT) {
		*value = 0;
		return FALSE;

	} else {
		*value = (guint) n;
		return TRUE;
	}
}

gint
tracker_string_in_string_list (const gchar  *str,
                               gchar       **strv)
{
	gchar **p;
	gint    i;

	g_return_val_if_fail (str != NULL, -1);

	if (!strv) {
		return -1;
	}

	for (p = strv, i = 0; *p; p++, i++) {
		if (strcasecmp (*p, str) == 0) {
			return i;
		}
	}

	return -1;
}

gboolean
tracker_string_in_gslist (const gchar *str,
                          GSList      *list)
{
	GSList *l;

	g_return_val_if_fail (str != NULL, FALSE);

	for (l = list; l; l = l->next) {
		if (g_strcmp0 (l->data, str) == 0) {
			return TRUE;
		}
	}

	return FALSE;
}

GSList *
tracker_string_list_to_gslist (gchar **strv,
                               gsize   size)
{
	GSList *list;
	gsize   i;
	gsize   size_used;

	g_return_val_if_fail (strv != NULL, NULL);

	if (size < 1) {
		size_used = g_strv_length (strv);
	} else {
		size_used = size;
	}

	list = NULL;

	for (i = 0; i < size; i++) {
		if (strv[i]) {
			list = g_slist_prepend (list, g_strdup (strv[i]));
		} else {
			break;
		}
	}

	return g_slist_reverse (list);
}

gchar *
tracker_string_list_to_string (gchar **strv,
                               gsize   size,
                               gchar   sep)
{
	GString *string;
	gsize    i;
	gsize    size_used;

	g_return_val_if_fail (strv != NULL, NULL);

	if (size < 1) {
		size_used = g_strv_length (strv);
	} else {
		size_used = size;
	}

	string = g_string_new ("");

	for (i = 0; i < size; i++) {
		if (strv[i]) {
			if (i > 0) {
				g_string_append_c (string, sep);
			}

			string = g_string_append (string, strv[i]);
		} else {
			break;
		}
	}

	return g_string_free (string, FALSE);
}

gchar **
tracker_string_to_string_list (const gchar *str)
{
	gchar **result;

	result = g_new0 (gchar *, 2);

	result [0] = g_strdup (str);
	result [1] = NULL;

	return result;
}

gchar **
tracker_gslist_to_string_list (GSList *list)
{
	GSList  *l;
	gchar  **strv;
	gint     i;

	strv = g_new0 (gchar*, g_slist_length (list) + 1);

	for (l = list, i = 0; l; l = l->next) {
		if (!l->data) {
			continue;
		}

		strv[i++] = g_strdup (l->data);
	}

	strv[i] = NULL;

	return strv;
}

gboolean
tracker_gslist_with_string_data_equal (GSList *list1,
                                       GSList *list2)
{
        GSList *sl;

        if (list1 == list2) {
                return TRUE;
        }

        if (g_slist_length (list1) != g_slist_length (list2)) {
                return FALSE;
        }

        /* NOTE: This is probably not the most efficient way to do
         * this, but we don't want to order the list first since that
         * would involve creating new memory. This would make sense
         * for large list operations I think. We don't expect to be
         * doing much if any of that.
         */
        for (sl = list1; sl; sl = sl->next) {
                const gchar *str;

                str = sl->data;

                /* If we are not still in the list, remove the dir */
                if (!tracker_string_in_gslist (str, list2)) {
                        return FALSE;
                }
        }

        for (sl = list2; sl; sl = sl->next) {
                const gchar *str;

                str = sl->data;

                /* If we are now in the list, add the dir */
                if (!tracker_string_in_gslist (str, list1)) {
                        return FALSE;
                }
        }

        return TRUE;
}

GSList *
tracker_gslist_copy_with_string_data (GSList *list)
{
	GSList *l;
	GSList *new_list;

	if (!list) {
		return NULL;
	}

	new_list = NULL;

	for (l = list; l; l = l->next) {
		new_list = g_slist_prepend (new_list, g_strdup (l->data));
	}

	new_list = g_slist_reverse (new_list);

	return new_list;
}

GList *
tracker_glist_copy_with_string_data (GList *list)
{
	GList *l;
	GList *new_list;

	if (!list) {
		return NULL;
	}

	new_list = NULL;

	for (l = list; l; l = l->next) {
		new_list = g_list_prepend (new_list, g_strdup (l->data));
	}

	new_list = g_list_reverse (new_list);

	return new_list;
}

gchar *
tracker_string_boolean_to_string_gint (const gchar *value)
{
	g_return_val_if_fail (value != NULL, NULL);

	if (g_ascii_strcasecmp (value, "true") == 0) {
		return g_strdup ("1");
	} else if (g_ascii_strcasecmp (value, "false") == 0) {
		return g_strdup ("0");
	} else {
		return g_strdup (value);
	}
}
