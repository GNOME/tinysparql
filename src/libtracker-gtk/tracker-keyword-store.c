/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-
 *
 * tracker-gtk/keyword-store.c - A derived GtkListStore that maintians a
 * DBus connection to tracker such that when a new keyword is created it
 * is automatically inserted here.
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

#include <gtk/gtk.h>

#include "tracker-keyword-store.h"
#include "tracker-utils.h"

typedef struct _TrackerKeywordStore TrackerKeywordStore;

static void tracker_keyword_store_tree_drag_source_init (GtkTreeDragSourceIface *iface);

static void  tracker_keyword_store_finalize (GObject *object);


static void tracker_keyword_store_populate_cb (GPtrArray *result, GError *error, gpointer user_data);

G_DEFINE_TYPE_WITH_CODE (TrackerKeywordStore, tracker_keyword_store, GTK_TYPE_LIST_STORE,
			G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_SOURCE,
			tracker_keyword_store_tree_drag_source_init))

#define parent_class tracker_keyword_store_parent_class

static void
tracker_keyword_store_class_init (TrackerKeywordStoreClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = tracker_keyword_store_finalize;
}

static void
tracker_keyword_store_init (TrackerKeywordStore *store)
{
	/* setup the basic class list store properties */
	GType types[TRACKER_KEYWORD_STORE_NUM_COLUMNS];
	types[TRACKER_KEYWORD_STORE_KEYWORD]	= G_TYPE_STRING;
	types[TRACKER_KEYWORD_STORE_IMAGE_URI]	= G_TYPE_STRING;

	gtk_list_store_set_column_types (GTK_LIST_STORE (store),
					TRACKER_KEYWORD_STORE_NUM_COLUMNS, types);

	//setup private members
	store->keywords = g_hash_table_new (g_str_hash, g_str_equal);
	store->tracker_client = tracker_connect (TRUE);

	//populate the liststore asyncronously
	tracker_keywords_get_list_async (store->tracker_client,
					SERVICE_FILES,
					tracker_keyword_store_populate_cb,
					store);
}

static gboolean
tracker_keyword_store_row_draggable (GtkTreeDragSource		*drag_source,
					GtkTreePath		*path)
{
	printf ("ROW DRAGGABLE\n");
	return TRUE;
}

static gboolean
tracker_keyword_store_drag_data_get (GtkTreeDragSource		*drag_source,
				     GtkTreePath		*path,
				     GtkSelectionData		*data)
{
	gchar *keyword;
	GtkTreeIter iter;
	TrackerKeywordStore *store;

	printf ("DRAG DATA GET %s\n",gdk_atom_name (data->target));

	store = TRACKER_KEYWORD_STORE (drag_source);

	gtk_tree_model_get_iter (GTK_TREE_MODEL(store), &iter, path);

	gtk_tree_model_get (GTK_TREE_MODEL(store), &iter, TRACKER_KEYWORD_STORE_KEYWORD, &keyword, -1);

	gtk_selection_data_set_text (data, keyword, strlen (keyword));
	g_free (keyword);
	return TRUE;
}

static gboolean
tracker_keyword_store_drag_data_delete (GtkTreeDragSource	*drag_source,
					GtkTreePath		*path)
{
	printf ("DRAG DATA DELETE\n");
	return FALSE;
}

static void
tracker_keyword_store_tree_drag_source_init (GtkTreeDragSourceIface *iface)
{
	iface->row_draggable = tracker_keyword_store_row_draggable;
	iface->drag_data_get = tracker_keyword_store_drag_data_get;
	iface->drag_data_delete = tracker_keyword_store_drag_data_delete;
}

static void
tracker_keyword_store_populate_cb (GPtrArray *result, GError *error, gpointer user_data) {
	GtkTreeIter iter;
	GtkListStore *list_store = GTK_LIST_STORE (user_data);

	if (!error && result) {
		gchar *name = NULL;
		guint i;
		for (i = 0; i < result->len; i++) {
			name = ((gchar **)result->pdata[i])[0];
			if (strlen (name) > 2) {
				//FIXME: Modify this function when tracker stores emblem images
				gtk_list_store_insert_with_values (list_store, &iter, 0,
								TRACKER_KEYWORD_STORE_KEYWORD, name,
								-1);

			}
		}
		g_ptr_array_free (result, TRUE);
	}
	g_clear_error (&error);
}

static void
tracker_keyword_store_finalize (GObject *object)
{
	TrackerKeywordStore *store = TRACKER_KEYWORD_STORE (object);

	if (store->keywords) {
		g_hash_table_unref (store->keywords);
	}

	if (store->tracker_client) {
		tracker_disconnect (store->tracker_client);
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

/**
 * tracker_keyword_store_new:
 *
 * Creates a #GtkListStore with several columns. This store is especially
 * useful because it also contais a hashtable which retains a gtk_tree_iter
 * indexed by keyword, for O(1) fast lookups of liststore contents.
 *
 * Return value: a new #TrackerKeywordStore.
 *
 **/
GtkListStore *
tracker_keyword_store_new (void)
{
	return g_object_new (TRACKER_TYPE_KEYWORD_STORE, NULL);
}

/**
 * Inserts keyword into the liststore
 * returns true if successful
 **/
gboolean
tracker_keyword_store_insert (	GtkListStore			*store,
				const char			*keyword,
				const char			*stock_id
)
{
	GtkTreeIter *iter;
	TrackerKeywordStore *self;

	g_return_val_if_fail (TRACKER_IS_KEYWORD_STORE(store), FALSE);
	g_return_val_if_fail (keyword != NULL, FALSE);

	self = TRACKER_KEYWORD_STORE (store);

	if (g_hash_table_lookup (self->keywords, keyword) == NULL)
	{
		iter = (GtkTreeIter *)g_new0 (GtkTreeIter, 1);
		gtk_list_store_insert_with_values (store,
						iter, 0,
						TRACKER_KEYWORD_STORE_KEYWORD, keyword,
						TRACKER_KEYWORD_STORE_IMAGE_URI, stock_id,
						-1);
		g_hash_table_insert (self->keywords, g_strdup (keyword), iter);
		return TRUE;

	}
	return FALSE;
}

/**
 * O(1) lookup of items by keyword from the store
 * Returns the GtkTreeIter corresponding to the item with keyword or
 * NULL of it cant be found
 **/
GtkTreeIter *
tracker_keyword_store_lookup (	GtkListStore			*store,
				const char			*keyword)
{
	TrackerKeywordStore *self;

	g_return_val_if_fail (TRACKER_IS_KEYWORD_STORE(store), FALSE);
	g_return_val_if_fail (keyword != NULL, FALSE);

	self = TRACKER_KEYWORD_STORE (store);
	return (GtkTreeIter *)g_hash_table_lookup (self->keywords, keyword);
}

/**
 * O(1) removal of items by keyword
 **/
gboolean
tracker_keyword_store_remove (	GtkListStore			*store,
				const char			*keyword)
{
	GtkTreeIter *iter;
	TrackerKeywordStore *self;
	gboolean a,b;

	iter = tracker_keyword_store_lookup (store, keyword);
	self = TRACKER_KEYWORD_STORE (store);

	if (iter != NULL) {
		a = gtk_list_store_remove (store, iter);
		b = g_hash_table_remove (self->keywords, keyword);
		return a && b;
	}
	return FALSE;
}








