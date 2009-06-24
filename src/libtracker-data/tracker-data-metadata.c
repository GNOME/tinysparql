/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia

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

#include "config.h"

#include <glib.h>

#include <libtracker-common/tracker-ontology.h>

#include "tracker-data-metadata.h"

#define GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_DATA_METADATA, TrackerDataMetadataPrivate))

typedef struct TrackerDataMetadataPrivate TrackerDataMetadataPrivate;

struct TrackerDataMetadataPrivate {
	GHashTable *table;
};


static void tracker_data_metadata_finalize (GObject *object);


G_DEFINE_TYPE (TrackerDataMetadata, tracker_data_metadata, G_TYPE_OBJECT)


static void
tracker_data_metadata_class_init (TrackerDataMetadataClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_data_metadata_finalize;

	g_type_class_add_private (object_class,
				  sizeof (TrackerDataMetadataPrivate));
}

static void
tracker_data_metadata_init (TrackerDataMetadata *metadata)
{
	TrackerDataMetadataPrivate *priv;

	priv = GET_PRIVATE (metadata);

	priv->table = g_hash_table_new_full (g_direct_hash,
					     g_direct_equal,
					     (GDestroyNotify) g_object_unref,
					     NULL);
}

static void
free_metadata (gpointer      data,
	       TrackerField *field)
{
	if (tracker_field_get_multiple_values (field)) {
		GList *list;

		list = (GList *) data;
		g_list_foreach (list, (GFunc) g_free, NULL);
		g_list_free (list);
	} else {
		g_free (data);
	}

}

static gboolean
remove_metadata_foreach (gpointer key,
			 gpointer value,
			 gpointer user_data)
{
	TrackerField *field;

	field = (TrackerField *) key;
	free_metadata (value, field);

	return TRUE;
}

static void
tracker_data_metadata_finalize (GObject *object)
{
	TrackerDataMetadataPrivate *priv;

	priv = GET_PRIVATE (object);

	g_hash_table_foreach_remove (priv->table,
				     remove_metadata_foreach,
				     NULL);

	g_hash_table_destroy (priv->table);

	G_OBJECT_CLASS (tracker_data_metadata_parent_class)->finalize (object);
}

/**
 * tracker_data_metadata_new:
 *
 * Creates a new #TrackerDataMetadata with no data in it.
 *
 * Returns: The newly created #TrackerDataMetadata
 **/
TrackerDataMetadata *
tracker_data_metadata_new (void)
{
	return g_object_new (TRACKER_TYPE_DATA_METADATA, NULL);
}

void
tracker_data_metadata_clear_field (TrackerDataMetadata *metadata,
				   const gchar         *field_name)
{
	TrackerDataMetadataPrivate *priv;
	TrackerField *field;
	gpointer data;

	field = tracker_ontology_get_field_by_name (field_name);

	if (!field) {
		g_warning ("Field name '%s' isn't described in the ontology", field_name);
		return;
	}

	priv = GET_PRIVATE (metadata);
	data = g_hash_table_lookup (priv->table, field);

	if (data) {
		free_metadata (data, field);
		g_hash_table_remove (priv->table, field);
	}
}

gboolean
tracker_data_metadata_insert_take_ownership (TrackerDataMetadata *metadata,
					     const gchar         *field_name,
					     gchar               *value)
{
	TrackerDataMetadataPrivate *priv;
	TrackerField *field;
	gchar *old_value;

	g_return_val_if_fail (TRACKER_IS_DATA_METADATA (metadata), FALSE);
	g_return_val_if_fail (field_name != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);

	priv = GET_PRIVATE (metadata);
	field = tracker_ontology_get_field_by_name (field_name);

	g_return_val_if_fail (TRACKER_IS_FIELD (field), FALSE);
	g_return_val_if_fail (tracker_field_get_multiple_values (field) == FALSE, FALSE);

	old_value = g_hash_table_lookup (priv->table, field);
	g_free (old_value);

	g_hash_table_replace (priv->table,
			      g_object_ref (field),
			      value);
	return TRUE;
}

/**
 * tracker_data_metadata_insert:
 * @metadata: A #TrackerDataMetadata
 * @field_name: Field name for the metadata to insert.
 * @value: Value for the metadata to insert.
 *
 * Inserts a new metadata element into @metadata.
 **/
void
tracker_data_metadata_insert (TrackerDataMetadata *metadata,
			      const gchar	  *field_name,
			      const gchar         *value)
{
	TrackerDataMetadataPrivate *priv;
	TrackerField *field;
	gchar *old_value;

	g_return_if_fail (TRACKER_IS_DATA_METADATA (metadata));
	g_return_if_fail (field_name != NULL);
	g_return_if_fail (value != NULL);

	priv = GET_PRIVATE (metadata);
	field = tracker_ontology_get_field_by_name (field_name);

	g_return_if_fail (TRACKER_IS_FIELD (field));
	g_return_if_fail (tracker_field_get_multiple_values (field) == FALSE);

	old_value = g_hash_table_lookup (priv->table, field);
	g_free (old_value);

	g_hash_table_replace (priv->table,
			      g_object_ref (field),
			      g_strdup (value));
}


/**
 * tracker_data_metadata_insert_values:
 * @metadata: A #TrackerDataMetadata
 * @field_name: Field name for the metadata to insert
 * @list: Value list for the metadata to insert
 *
 * Inserts a list of values into @metadata for the given @field_name.
 * The ontology has to specify that @field_name allows multiple values.
 * 
 * The values in @list and the list itself are copied, the caller is
 * still responsible for the memory @list uses after calling this
 * function. 
 **/
void
tracker_data_metadata_insert_values (TrackerDataMetadata *metadata,
				     const gchar         *field_name,
				     const GList	 *list)
{
	TrackerDataMetadataPrivate *priv;
	TrackerField *field;
	GList        *old_values, *copy;

	g_return_if_fail (TRACKER_IS_DATA_METADATA (metadata));
	g_return_if_fail (field_name != NULL);

	if (!list) {
		return;
	}

	priv = GET_PRIVATE (metadata);
	field = tracker_ontology_get_field_by_name (field_name);

	if (!field) {
		g_warning ("Field name '%s' has isn't described in the ontology", field_name);
		return;
	}

	g_return_if_fail (TRACKER_IS_FIELD (field));
	g_return_if_fail (tracker_field_get_multiple_values (field) == TRUE);

	copy = tracker_glist_copy_with_string_data ((GList *)list);

	old_values = g_hash_table_lookup (priv->table, field);

	if (old_values) {
		g_list_foreach (old_values, (GFunc) g_free, NULL);
		g_list_free (old_values);
	}

	g_hash_table_replace (priv->table,
			      g_object_ref (field),
			      copy);
}

/**
 * tracker_data_metadata_lookup:
 * @metadata: A #TrackerDataMetadata
 * @field_name: Field name to look up
 *
 * Returns the value corresponding to the metadata specified by
 * @field_name. 
 *
 * Returns: The value. This string is owned by @metadata and must not
 * be modified or freed. 
 **/
G_CONST_RETURN gchar *
tracker_data_metadata_lookup (TrackerDataMetadata *metadata,
			      const gchar	  *field_name)
{
	TrackerDataMetadataPrivate *priv;
	TrackerField *field;

	g_return_val_if_fail (TRACKER_IS_DATA_METADATA (metadata), NULL);
	g_return_val_if_fail (field_name != NULL, NULL);

	priv = GET_PRIVATE (metadata);
	field = tracker_ontology_get_field_by_name (field_name);

	g_return_val_if_fail (TRACKER_IS_FIELD (field), NULL);
	g_return_val_if_fail (tracker_field_get_multiple_values (field) == FALSE, NULL);

	return g_hash_table_lookup (priv->table, field);
}

/**
 * tracker_data_metadata_lookup_values:
 * @metadata: A #TrackerDataMetadata
 * @field_name: Field name to look up
 *
 * Returns the value list corresponding to the metadata specified by
 * @field_name. 
 *
 * Returns: A List containing strings. Both the list and the contained
 *          strings are owned by @metadata and must not be modified or
 *          freed. 
 **/
G_CONST_RETURN GList *
tracker_data_metadata_lookup_values (TrackerDataMetadata *metadata,
				     const gchar         *field_name)
{
	TrackerDataMetadataPrivate *priv;
	TrackerField *field;

	g_return_val_if_fail (TRACKER_IS_DATA_METADATA (metadata), NULL);
	g_return_val_if_fail (field_name != NULL, NULL);

	priv = GET_PRIVATE (metadata);
	field = tracker_ontology_get_field_by_name (field_name);

	g_return_val_if_fail (TRACKER_IS_FIELD (field), NULL);
	g_return_val_if_fail (tracker_field_get_multiple_values (field) == TRUE, NULL);

	return g_hash_table_lookup (priv->table, field);
}

/**
 * tracker_data_metadata_foreach:
 * @metadata: A #TrackerDataMetadata.
 * @func: The function to call with each metadata.
 * @user_data: user data to pass to the function.
 *
 * Calls a function for each element in @metadata.
 **/
void
tracker_data_metadata_foreach (TrackerDataMetadata	  *metadata,
			       TrackerDataMetadataForeach  func,
			       gpointer			   user_data)
{
	TrackerDataMetadataPrivate *priv;

	g_return_if_fail (TRACKER_IS_DATA_METADATA (metadata));
	g_return_if_fail (func != NULL);

	priv = GET_PRIVATE (metadata);

	g_hash_table_foreach (priv->table,
			      (GHFunc) func,
			      user_data);
}

/**
 * tracker_data_metadata_foreach_remove:
 * @metadata: A #TrackerDataMetadata.
 * @func: The function to call with each metadata. 
 * @user_data: user data to pass to the function.
 *
 * Calls a function for each element in @metadata and remove the
 * element if @func returns %TRUE. 
 **/
void
tracker_data_metadata_foreach_remove (TrackerDataMetadata       *metadata,
				      TrackerDataMetadataRemove  func,
				      gpointer		         user_data)
{
	TrackerDataMetadataPrivate *priv;

	g_return_if_fail (TRACKER_IS_DATA_METADATA (metadata));
	g_return_if_fail (func != NULL);

	priv = GET_PRIVATE (metadata);

	g_hash_table_foreach_remove (priv->table,
				     (GHRFunc) func,
				     user_data);
}
