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
#include <libtracker-common/tracker-type-utils.h>
#include <libtracker-data/tracker-data-metadata.h>
#include "tracker-module-metadata-private.h"

struct TrackerModuleMetadata {
	TrackerDataMetadata parent_instance;
};

struct TrackerModuleMetadataClass {
	TrackerDataMetadataClass parent_class;
};


G_DEFINE_TYPE (TrackerModuleMetadata, tracker_module_metadata, TRACKER_TYPE_DATA_METADATA)

static void
tracker_module_metadata_class_init (TrackerModuleMetadataClass *klass)
{
}

static void
tracker_module_metadata_init (TrackerModuleMetadata *metadata)
{
}

/**
 * tracker_module_metadata_add_take_string:
 * @metadata: A #TrackerModuleMetadata
 * @field_name: Field name for the metadata to insert
 * @value: Value for the metadata
 *
 * Adds a new metadata element as a string into @metadata.
 * For ontology fields that can take several values, this
 * function will allow adding several elements to the same
 * field name.
 *
 * If the function returns #TRUE, @metadata will take
 * ownership on @value, else you are responsible of
 * freeing it.
 *
 * Returns: #TRUE if the value was added successfully.
 **/
gboolean
tracker_module_metadata_add_take_string (TrackerModuleMetadata *metadata,
					 const gchar           *field_name,
					 gchar                 *value)
{
	TrackerField *field;

	g_return_val_if_fail (TRACKER_IS_MODULE_METADATA (metadata), FALSE);
	g_return_val_if_fail (field_name != NULL, FALSE);

	field = tracker_ontology_get_field_by_name (field_name);

	if (!field) {
		g_warning ("Field name '%s' isn't described in the ontology", field_name);
		return FALSE;

	}

	if (tracker_field_get_multiple_values (field)) {
		const GList *list;
		GList *copy = NULL;

		list = tracker_data_metadata_lookup_values (TRACKER_DATA_METADATA (metadata),
							    field_name);

		while (list) {
			copy = g_list_prepend (copy, g_strdup (list->data));
			list = list->next;
		}

		copy = g_list_prepend (copy, value);
		copy = g_list_reverse (copy);

		tracker_data_metadata_insert_values (TRACKER_DATA_METADATA (metadata),
						     field_name, copy);

		g_list_foreach (copy, (GFunc) g_free, NULL);
		g_list_free (copy);

		return TRUE;
	} else {
		return tracker_data_metadata_insert_take_ownership (TRACKER_DATA_METADATA (metadata),
								    field_name, value);
	}
}

/**
 * tracker_module_metadata_add_string:
 * @metadata: A #TrackerModuleMetadata
 * @field_name: Field name for the metadata to insert
 * @value: Value for the metadata
 *
 * Adds a new metadata element as a string into @metadata.
 * see tracker_module_metadata_add_take_string().
 **/
void
tracker_module_metadata_add_string (TrackerModuleMetadata *metadata,
				    const gchar           *field_name,
				    const gchar           *value)
{
	TrackerField *field;

	g_return_if_fail (TRACKER_IS_MODULE_METADATA (metadata));
	g_return_if_fail (field_name != NULL);

	field = tracker_ontology_get_field_by_name (field_name);

	if (!field) {
		g_warning ("Field name '%s' isn't described in the ontology", field_name);
	}

	if (tracker_field_get_multiple_values (field)) {
		const GList *list;
		GList *copy = NULL;

		list = tracker_data_metadata_lookup_values (TRACKER_DATA_METADATA (metadata),
							    field_name);

		while (list) {
			copy = g_list_prepend (copy, g_strdup (list->data));
			list = list->next;
		}

		copy = g_list_prepend (copy, g_strdup (value));
		copy = g_list_reverse (copy);

		tracker_data_metadata_insert_values (TRACKER_DATA_METADATA (metadata),
						     field_name, copy);

		g_list_foreach (copy, (GFunc) g_free, NULL);
		g_list_free (copy);
	} else {
		tracker_data_metadata_insert (TRACKER_DATA_METADATA (metadata),
					      field_name, value);
	}
}

/**
 * tracker_module_metadata_add_int:
 * @metadata: A #TrackerModuleMetadata
 * @field_name: Field name for the metadata to insert
 * @value: Value for the metadata
 *
 * Adds a new metadata element as a integer into @metadata.
 * see tracker_module_metadata_add_take_string().
 **/
void
tracker_module_metadata_add_int (TrackerModuleMetadata *metadata,
				 const gchar           *field_name,
				 gint                   value)
{
	gchar *str;

	str = tracker_gint_to_string (value);

	if (!tracker_module_metadata_add_take_string (metadata, field_name, str)) {
		g_free (str);
	}
}

/**
 * tracker_module_metadata_add_uint:
 * @metadata: A #TrackerModuleMetadata
 * @field_name: Field name for the metadata to insert
 * @value: Value for the metadata
 *
 * Adds a new metadata element as an unsigned integer into @metadata.
 * see tracker_module_metadata_add_take_string().
 **/
void
tracker_module_metadata_add_uint (TrackerModuleMetadata *metadata,
				  const gchar           *field_name,
				  guint                  value)
{
	gchar *str;

	str = tracker_guint_to_string (value);

	if (!tracker_module_metadata_add_take_string (metadata, field_name, str)) {
		g_free (str);
	}
}

void
tracker_module_metadata_add_int64 (TrackerModuleMetadata *metadata,
				    const gchar           *field_name,
				    gint64                value)
{
	gchar *str;

	str = tracker_gint64_to_string (value);

	if (!tracker_module_metadata_add_take_string (metadata, field_name, str)) {
		g_free (str);
	}
}

void
tracker_module_metadata_add_uint64 (TrackerModuleMetadata *metadata,
				    const gchar           *field_name,
				    guint64                value)
{
	gchar *str;

	str = tracker_guint64_to_string (value);

	if (!tracker_module_metadata_add_take_string (metadata, field_name, str)) {
		g_free (str);
	}
}

void
tracker_module_metadata_add_offset (TrackerModuleMetadata *metadata,
				    const gchar           *field_name,
				    goffset                value)
{
	gchar *str;

	str = tracker_goffset_to_string (value);

	if (!tracker_module_metadata_add_take_string (metadata, field_name, str)) {
		g_free (str);
	}
}

/**
 * tracker_module_metadata_add_double:
 * @metadata: A #TrackerModuleMetadata
 * @field_name: Field name for the metadata to insert
 * @value: Value for the metadata
 *
 * Adds a new metadata element as a double into @metadata.
 * see tracker_module_metadata_add_take_string().
 **/
void
tracker_module_metadata_add_double (TrackerModuleMetadata *metadata,
				    const gchar           *field_name,
				    gdouble                value)
{
	gchar *str;

	str = g_strdup_printf ("%f", value);

	if (!tracker_module_metadata_add_take_string (metadata, field_name, str)) {
		g_free (str);
	}
}

/**
 * tracker_module_metadata_add_float:
 * @metadata: A #TrackerModuleMetadata
 * @field_name: Field name for the metadata to insert
 * @value: Value for the metadata
 *
 * Adds a new metadata element as a float into @metadata.
 * see tracker_module_metadata_add_take_string().
 **/
void
tracker_module_metadata_add_float (TrackerModuleMetadata *metadata,
				   const gchar           *field_name,
				   gfloat                 value)
{
	gchar *str;

	str = g_strdup_printf ("%f", value);

	if (!tracker_module_metadata_add_take_string (metadata, field_name, str)) {
		g_free (str);
	}
}

/**
 * tracker_module_metadata_add_date:
 * @metadata: A #TrackerModuleMetadata
 * @field_name: Field name for the metadata to insert
 * @value: Value for the metadata
 *
 * Adds a new metadata element as a time_t into @metadata.
 * see tracker_module_metadata_add_take_string().
 **/
void
tracker_module_metadata_add_date (TrackerModuleMetadata *metadata,
				  const gchar           *field_name,
				  time_t                 value)
{
	gchar *str;

	if (sizeof (time_t) == 8) {
		str = g_strdup_printf ("%" G_GINT64_FORMAT, (gint64) value);
	} else {
		str = g_strdup_printf ("%d", (gint32) value);
	}

	if (!tracker_module_metadata_add_take_string (metadata, field_name, str)) {
		g_free (str);
	}
}

static void
get_hash_table_foreach (TrackerField *field,
			gpointer      value,
			gpointer      user_data)
{
	GHashTable *table;

	table = user_data;

	g_hash_table_insert (table,
			     (gpointer) tracker_field_get_name (field),
			     value);
}

GHashTable *
tracker_module_metadata_get_hash_table (TrackerModuleMetadata *metadata)
{
	GHashTable *table;

	table = g_hash_table_new (g_str_hash, g_str_equal);

	tracker_data_metadata_foreach (TRACKER_DATA_METADATA (metadata),
				       get_hash_table_foreach,
				       table);
	return table;
}

/**
 * tracker_module_metadata_new:
 *
 * Creates a new #TrackerModuleMetadata
 *
 * Returns: A newly created #TrackerModuleMetadata
 **/
TrackerModuleMetadata *
tracker_module_metadata_new (void)
{
	return g_object_new (TRACKER_TYPE_MODULE_METADATA, NULL);
}
