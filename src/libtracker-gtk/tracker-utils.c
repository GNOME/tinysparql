/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-
 *
 * libtracker-gtk/tracker-utils.c - Grab bag of functions for manuipulating 
 * tracker results into more Gtk friedly types.
 *
 * Copyright (C) 2007 John Stowers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <stdio.h>
#include <string.h>

#include "tracker-utils.h"

GList *
tracker_keyword_array_to_glist(gchar **array)
{
	GList *list = NULL;
	gchar **meta = NULL;
	gchar * name = NULL;

	if (array == NULL)
		return NULL;

	for (meta = array; *meta; meta++) {
		name = g_strdup(meta[0]);
		printf("keyword_name = %s\n", name);
		list = g_list_prepend(list, name);
	}

	return list;
}

GList *
tracker_get_all_keywords (TrackerClient *tracker_client)
{
	GPtrArray *out_array;
	GList *list = NULL;
	GError *error = NULL;

	out_array = tracker_keywords_get_list (tracker_client, SERVICE_FILES, &error);
	if (!error && out_array) {
		gchar *name = NULL;
		guint i;
		for (i = 0; i < out_array->len; i++) {
			name = ((gchar **)out_array->pdata[i])[0];
			if (strlen (name) > 2)
				list = g_list_prepend(list, name);
		}
		g_ptr_array_free (out_array, TRUE);
	}
	g_clear_error(&error);
	return list;
}

/* Creates a tree model containing the keywords in List 
this simple treemodel has a single column containing the keyword
name*/
GtkTreeModel *
tracker_create_simple_keyword_liststore (const GList *list)
{
    GList *l;
    GtkListStore *store;
    GtkTreeIter iter;

    store = gtk_list_store_new (1, G_TYPE_STRING);

    l=list;
    while (l != NULL) {
        gtk_list_store_insert_with_values (store,
                                           &iter,
                                           0,
                                           0,
                                           (char *)l->data,
                                           -1);
        l = l->next;
    }

    return GTK_TREE_MODEL (store);
}
