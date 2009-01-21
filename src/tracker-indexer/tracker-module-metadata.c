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
#include "tracker-module-metadata-private.h"

struct TrackerModuleMetadata {
	GObject parent_instance;
	GHashTable *table;
};

struct TrackerModuleMetadataClass {
	GObjectClass parent_class;
};


static void   tracker_module_metadata_finalize   (GObject *object);


G_DEFINE_TYPE (TrackerModuleMetadata, tracker_module_metadata, G_TYPE_OBJECT)

static void
tracker_module_metadata_class_init (TrackerModuleMetadataClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_module_metadata_finalize;
}

static void
tracker_module_metadata_init (TrackerModuleMetadata *metadata)
{
	metadata->table = g_hash_table_new_full (g_direct_hash,
						 g_direct_equal,
						 (GDestroyNotify) g_object_unref,
						 NULL);
}

static void
free_metadata (TrackerField *field,
	       gpointer      data)
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
	free_metadata (field, value);

	return TRUE;
}

static void
tracker_module_metadata_finalize (GObject *object)
{
	TrackerModuleMetadata *metadata;

	metadata = TRACKER_MODULE_METADATA (object);

	g_hash_table_foreach_remove (metadata->table,
				     remove_metadata_foreach,
				     NULL);

	g_hash_table_destroy (metadata->table);

	G_OBJECT_CLASS (tracker_module_metadata_parent_class)->finalize (object);
}

gconstpointer
tracker_module_metadata_lookup (TrackerModuleMetadata *metadata,
				const gchar           *field_name,
				gboolean              *multiple_values)
{
	TrackerField *field;

	field = tracker_ontology_get_field_by_name (field_name);

	if (multiple_values) {
		*multiple_values = tracker_field_get_multiple_values (field);
	}

	return g_hash_table_lookup (metadata->table, field);
}

/**
 * tracker_module_metadata_clear_field:
 * @metadata: A #TrackerModuleMetadata
 * @field_name: Field name for the metadata to clear
 *
 * Clears any content for the given field name.
 **/
void
tracker_module_metadata_clear_field (TrackerModuleMetadata *metadata,
				     const gchar           *field_name)
{
	TrackerField *field;

	gpointer data;

	field = tracker_ontology_get_field_by_name (field_name);

	if (!field) {
		g_warning ("Field name '%s' isn't described in the ontology", field_name);
		return;
	}

	data = g_hash_table_lookup (metadata->table, field);

	if (data) {
		free_metadata (field, data);
		g_hash_table_remove (metadata->table, field);
	}
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
	gpointer data;

	g_return_val_if_fail (metadata != NULL, FALSE);
	g_return_val_if_fail (field_name != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);

	field = tracker_ontology_get_field_by_name (field_name);

	if (!field) {
		g_warning ("Field name '%s' isn't described in the ontology", field_name);
		return FALSE;
	}

	if (tracker_field_get_multiple_values (field)) {
		GList *list;

		list = g_hash_table_lookup (metadata->table, field);
		list = g_list_prepend (list, value);
		data = list;
	} else {
		data = g_hash_table_lookup (metadata->table, field);
		g_free (data);
		data = value;
	}

	g_hash_table_replace (metadata->table,
			      g_object_ref (field),
			      data);

	return TRUE;
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
	gchar *str;

	str = g_strdup (value);

	if (!tracker_module_metadata_add_take_string (metadata, field_name, str)) {
		g_free (str);
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

/**
 * tracker_module_metadata_foreach:
 * @metadata: A #TrackerModuleMetadata.
 * @func: The function to call with each metadata.
 * @user_data: user data to pass to the function.
 *
 * Calls a function for each element in @metadata.
 **/
void
tracker_module_metadata_foreach (TrackerModuleMetadata        *metadata,
				 TrackerModuleMetadataForeach  func,
				 gpointer		       user_data)
{
	g_hash_table_foreach (metadata->table,
			      (GHFunc) func,
			      user_data);
}

void
tracker_module_metadata_foreach_remove (TrackerModuleMetadata       *metadata,
					TrackerModuleMetadataRemove  func,
					gpointer                     user_data)
{
	g_hash_table_foreach_remove (metadata->table,
				     (GHRFunc) func,
				     user_data);
}

static void
get_hash_table_foreach (gpointer key,
			gpointer value,
			gpointer user_data)
{
	TrackerField *field;
	GHashTable *table;

	field = TRACKER_FIELD (key);
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
	g_hash_table_foreach (metadata->table, (GHFunc) get_hash_table_foreach, table);

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
