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

#include <glib.h>
#include <libtracker-common/tracker-ontology.h>
#include "tracker-data-metadata.h"

struct TrackerDataMetadata {
	GHashTable *table;
};

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
	TrackerDataMetadata *metadata;

	metadata = g_slice_new (TrackerDataMetadata);
	metadata->table = g_hash_table_new_full (g_direct_hash,
						 g_direct_equal,
						 (GDestroyNotify) g_object_unref,
						 NULL);
	return metadata;
}

static gboolean
remove_metadata_foreach (gpointer key,
			 gpointer value,
			 gpointer user_data)
{
	TrackerField *field;

	field = (TrackerField *) key;

	if (tracker_field_get_multiple_values (field)) {
		GList *list;

		list = (GList *) value;
		g_list_foreach (list, (GFunc) g_free, NULL);
		g_list_free (list);
	} else {
		g_free (value);
	}

	return TRUE;
}

/**
 * tracker_data_metadata_free:
 * @metadata: A #TrackerDataMetadata
 *
 * Frees the #TrackerDataMetadata and any contained data.
 **/
void
tracker_data_metadata_free (TrackerDataMetadata *metadata)
{
	g_hash_table_foreach_remove (metadata->table,
				     remove_metadata_foreach,
				     NULL);

	g_hash_table_destroy (metadata->table);
	g_slice_free (TrackerDataMetadata, metadata);
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
			      gchar		  *value)
{
	TrackerField *field;

	field = tracker_ontology_get_field_by_name (field_name);

	if (!field) {
		g_warning ("Field name '%s' has isn't described in the ontology", field_name);
		g_free (value);
		return;
	}

        g_return_if_fail (tracker_field_get_multiple_values (field) == FALSE);

	g_hash_table_insert (metadata->table,
			     g_object_ref (field),
			     value);
}

/**
 * tracker_data_metadata_insert_values:
 * @metadata: A #TrackerDataMetadata
 * @field_name: Field name for the metadata to insert
 * @list: Value list for the metadata to insert
 *
 * Inserts a list of values into @metadata for the given @field_name.
 * The ontology has to specify that @field_name allows multiple values.
 **/
void
tracker_data_metadata_insert_values (TrackerDataMetadata *metadata,
				     const gchar	  *field_name,
				     GList		  *list)
{
	TrackerField *field;

	field = tracker_ontology_get_field_by_name (field_name);

	g_return_if_fail (TRACKER_IS_FIELD (field));
	g_return_if_fail (tracker_field_get_multiple_values (field) == TRUE);

	g_hash_table_insert (metadata->table,
			     g_object_ref (field),
			     list);
}

/**
 * tracker_data_metadata_lookup:
 * @metadata: A #TrackerDataMetadata
 * @field_name: Field name to look up
 *
 * Returns the value corresponding to the metadata specified by @field_name.
 *
 * Returns: The value. This string is owned by @metadata and must not be modified or freed.
 **/
G_CONST_RETURN gchar *
tracker_data_metadata_lookup (TrackerDataMetadata *metadata,
			      const gchar	  *field_name)
{
	TrackerField *field;

	field = tracker_ontology_get_field_by_name (field_name);

	g_return_val_if_fail (TRACKER_IS_FIELD (field), NULL);
	g_return_val_if_fail (tracker_field_get_multiple_values (field) == FALSE, NULL);

	return g_hash_table_lookup (metadata->table, field);
}

/**
 * tracker_data_metadata_lookup_values:
 * @metadata: A #TrackerDataMetadata
 * @field_name: Field name to look up
 *
 * Returns the value list corresponding to the metadata specified by @field_name.
 *
 * Returns: A List containing strings. Both the list and the contained strings
 *          are owned by @metadata and must not be modified or freed.
 **/
G_CONST_RETURN GList *
tracker_data_metadata_lookup_values (TrackerDataMetadata *metadata,
				     const gchar	  *field_name)
{
	TrackerField *field;

	field = tracker_ontology_get_field_by_name (field_name);

	g_return_val_if_fail (TRACKER_IS_FIELD (field), NULL);
	g_return_val_if_fail (tracker_field_get_multiple_values (field) == TRUE, NULL);

	return g_hash_table_lookup (metadata->table, field);
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
	g_hash_table_foreach (metadata->table,
			      (GHFunc) func,
			      user_data);
}
