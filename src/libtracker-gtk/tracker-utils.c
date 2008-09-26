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

#include <string.h>

#include "tracker-utils.h"


GList *
tracker_keyword_array_to_glist (gchar **array)
{
	GList *list = NULL;
	gchar **meta = NULL;

	if (!array) {
		return NULL;
	}

	for (meta = array; *meta; meta++) {
		gchar *name = g_strdup (*meta);
		list = g_list_prepend (list, name);
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
		guint i;
		for (i = 0; i < out_array->len; i++) {
			gchar **names = out_array->pdata[i];
			if (names) {
				gchar *name = names[0];
				if (strlen (name) > 2) {
					list = g_list_prepend(list, name);
				}
			}
		}
		g_ptr_array_free (out_array, TRUE);
	}

	g_clear_error (&error);

	return list;
}

/* Creates a tree model containing the keywords in List
this simple treemodel has a single column containing the keyword name*/
GtkTreeModel *
tracker_create_simple_keyword_liststore (const GList *list)
{
    GtkListStore *store;
    const GList *tmp;

    store = gtk_list_store_new (1, G_TYPE_STRING);

    for (tmp = list; tmp; tmp = tmp->next) {
	    gchar *keyword = keyword = tmp->data;

	    gtk_list_store_insert_with_values (store,
					       NULL,
					       0,
					       0,
					       keyword,
					       -1);
    }

    return GTK_TREE_MODEL (store);
}

void
tracker_set_atk_relationship(GtkWidget *obj1, int relation_type,
			     GtkWidget *obj2)
{
	AtkObject *atk_obj1, *atk_obj2, *targets[1];
	AtkRelationSet *atk_rel_set;
	AtkRelation *atk_rel;

	atk_obj1 = gtk_widget_get_accessible (GTK_WIDGET (obj1));
	atk_obj2 = gtk_widget_get_accessible (GTK_WIDGET (obj2));
	atk_rel_set = atk_object_ref_relation_set (atk_obj1);
	targets[0] = atk_obj2;
	atk_rel = atk_relation_new (targets, 1, relation_type);
	atk_relation_set_add (atk_rel_set, atk_rel);
	g_object_unref (G_OBJECT (atk_rel));
}
