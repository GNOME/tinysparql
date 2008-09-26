/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia (urho.konttori@nokia.com)
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

#include <string.h>
#include <stdlib.h>

#include <glib.h>

#include "tracker-ontology.h"

typedef struct {
	gchar *prefix;
	gint   service;
} ServiceMimePrefixes;

static gboolean    initialized;

/* Hash (gint service_type_id, TrackerService *service) */
static GHashTable *service_ids;

/* Hash (gchar *service_name, TrackerService *service) */
static GHashTable *service_names;

/* Hash (gchar *mime, gint service_type_id) */
static GHashTable *mimes_to_service_ids;

/* List of ServiceMimePrefixes */
static GSList	  *service_mime_prefixes;

/* Field descriptions */
static GHashTable *field_names;

/* FieldType enum class */
static gpointer    field_type_enum_class;

static void
ontology_mime_prefix_foreach (gpointer data,
			      gpointer user_data)
{
	ServiceMimePrefixes *mime_prefix;

	mime_prefix = (ServiceMimePrefixes*) data;

	g_free (mime_prefix->prefix);
	g_free (mime_prefix);
}

#if 0

/* NOTE: This function *USED* to be for the service_names and
 * field_names hash tables so they could collate strings before
 * comparing them. But we have stopped using this because all service
 * names and field names are quite specific and defined in code so
 * they shouldn't ever need to be collated or case insensitively
 * compared.
 *
 * If this breaks things, we can reinstate it.
 */
static gpointer
ontology_hash_lookup_by_collated_str (GHashTable  *hash_table,
				      const gchar *str)
{
	gpointer  data;
	gchar	 *str_lower;

	str_lower = g_utf8_collate_key (str, -1);
	if (!str_lower) {
		return NULL;
	}

	data = g_hash_table_lookup (hash_table, str_lower);
	g_free (str_lower);

	return data;
}

#endif

static gpointer
ontology_hash_lookup_by_id (GHashTable	*hash_table,
			    gint	 id)
{
	gpointer  data;
	gchar	 *str;

	str = g_strdup_printf ("%d", id);
	if (!str) {
		return NULL;
	}

	data = g_hash_table_lookup (hash_table, str);
	g_free (str);

	return data;
}

void
tracker_ontology_init (void)
{
	if (initialized) {
		return;
	}

	service_ids = g_hash_table_new_full (g_str_hash,
					     g_str_equal,
					     g_free,
					     g_object_unref);

	service_names = g_hash_table_new_full (g_str_hash,
					       g_str_equal,
					       g_free,
					       g_object_unref);

	mimes_to_service_ids = g_hash_table_new_full (g_str_hash,
						      g_str_equal,
						      g_free,
						      NULL);

	field_names = g_hash_table_new_full (g_str_hash,
					     g_str_equal,
					     g_free,
					     g_object_unref);

	/* We will need the class later in order to match strings to enum values
	 * when inserting metadata types in the DB, so the enum class needs to be
	 * created beforehand.
	 */
	field_type_enum_class = g_type_class_ref (TRACKER_TYPE_FIELD_TYPE);

	initialized = TRUE;
}

void
tracker_ontology_shutdown (void)
{
	if (!initialized) {
		return;
	}

	g_hash_table_unref (service_ids);
	service_ids = NULL;

	g_hash_table_unref (service_names);
	service_names = NULL;

	g_hash_table_unref (mimes_to_service_ids);
	mimes_to_service_ids = NULL;

	g_hash_table_unref (field_names);
	field_names = NULL;

	if (service_mime_prefixes) {
		g_slist_foreach (service_mime_prefixes,
				 ontology_mime_prefix_foreach,
				 NULL);
		g_slist_free (service_mime_prefixes);
		service_mime_prefixes = NULL;
	}

	g_type_class_unref (field_type_enum_class);
	field_type_enum_class = NULL;

	initialized = FALSE;
}

void
tracker_ontology_service_add (TrackerService *service,
			      GSList	     *mimes,
			      GSList	     *mime_prefixes)
{

	GSList		    *l;
	ServiceMimePrefixes *service_mime_prefix;
	gint		     id;
	const gchar	    *name;

	g_return_if_fail (TRACKER_IS_SERVICE (service));

	id = tracker_service_get_id (service);
	name = tracker_service_get_name (service);

	g_hash_table_insert (service_names,
			     /* g_utf8_collate_key (name, -1),	*/
			     g_strdup (name),
			     g_object_ref (service));
	g_hash_table_insert (service_ids,
			     g_strdup_printf ("%d", id),
			     g_object_ref (service));

	for (l = mimes; l && l->data; l = l->next) {
		g_hash_table_insert (mimes_to_service_ids,
				     l->data,
				     GINT_TO_POINTER (id));
	}

	for (l = mime_prefixes; l; l = l->next) {
		service_mime_prefix = g_new0 (ServiceMimePrefixes, 1);
		service_mime_prefix->prefix = l->data;
		service_mime_prefix->service = id;

		service_mime_prefixes = g_slist_prepend (service_mime_prefixes,
						       service_mime_prefix);
	}
}

TrackerService *
tracker_ontology_get_service_by_name (const gchar *service_str)
{
	g_return_val_if_fail (service_str != NULL, NULL);

	return g_hash_table_lookup (service_names, service_str);
}

gchar *
tracker_ontology_get_service_by_id (gint id)
{
	TrackerService *service;

	service = ontology_hash_lookup_by_id (service_ids, id);

	if (!service) {
		return NULL;
	}

	return g_strdup (tracker_service_get_name (service));
}

gchar *
tracker_ontology_get_service_by_mime (const gchar *mime)
{
	gpointer	     id;
	ServiceMimePrefixes *item;
	GSList		    *prefix_service;

	g_return_val_if_fail (mime != NULL, g_strdup ("Other"));

	/* Try a complete mime */
	id = g_hash_table_lookup (mimes_to_service_ids, mime);
	if (id) {
		return tracker_ontology_get_service_by_id (GPOINTER_TO_INT (id));
	}

	/* Try in prefixes */
	for (prefix_service = service_mime_prefixes;
	     prefix_service != NULL;
	     prefix_service = prefix_service->next) {
		item = prefix_service->data;
		if (g_str_has_prefix (mime, item->prefix)) {
			return tracker_ontology_get_service_by_id (item->service);
		}
	}

	/* Default option */
	return g_strdup ("Other");
}

gint
tracker_ontology_get_service_id_by_name (const char *service_str)
{
	TrackerService *service;

	g_return_val_if_fail (service_str != NULL, -1);

	service = g_hash_table_lookup (service_names, service_str);

	if (!service) {
		return -1;
	}

	return tracker_service_get_id (service);
}

gchar *
tracker_ontology_get_service_parent (const gchar *service_str)
{
	TrackerService *service;
	const gchar    *parent = NULL;

	g_return_val_if_fail (service_str != NULL, NULL);

	service = g_hash_table_lookup (service_names, service_str);

	if (service) {
		parent = tracker_service_get_parent (service);
	}

	return g_strdup (parent);
}

gchar *
tracker_ontology_get_service_parent_by_id (gint id)
{
	TrackerService *service;

	service = ontology_hash_lookup_by_id (service_ids, id);

	if (!service) {
		return NULL;
	}

	return g_strdup (tracker_service_get_parent (service));
}

gint
tracker_ontology_get_service_parent_id_by_id (gint id)
{
	TrackerService *service;
	const gchar    *parent = NULL;

	service = ontology_hash_lookup_by_id (service_ids, id);

	if (service) {
		parent = tracker_service_get_parent (service);
	}

	if (!parent) {
		return -1;
	}

	service = g_hash_table_lookup (service_names, parent);

	if (!service) {
		return -1;
	}

	return tracker_service_get_id (service);
}

TrackerDBType
tracker_ontology_get_service_db_by_name (const gchar *service_str)
{
	TrackerDBType  type;
	gchar	      *str;

	g_return_val_if_fail (service_str != NULL, TRACKER_DB_TYPE_FILES);

	str = g_utf8_strdown (service_str, -1);

	if (g_str_has_suffix (str, "emails") ||
	    g_str_has_suffix (str, "attachments")) {
		type = TRACKER_DB_TYPE_EMAIL;
	} else if (g_str_has_prefix (str, "files")) {
		type = TRACKER_DB_TYPE_FILES;
	} else if (g_str_has_prefix (str, "xesam")) {
		type = TRACKER_DB_TYPE_XESAM;
	} else {
		type = TRACKER_DB_TYPE_FILES;
	}

	g_free (str);

	return type;
}

GSList *
tracker_ontology_get_service_names_registered (void)
{
	TrackerService *service;
	GList	       *services, *l;
	GSList	       *names = NULL;

	services = g_hash_table_get_values (service_names);

	for (l = services; l; l = l->next) {
		service = l->data;
		names = g_slist_prepend (names, g_strdup (tracker_service_get_name (service)));
	}

	return names;
}

GSList *
tracker_ontology_get_field_names_registered (const gchar *service_str)
{
	GList	       *fields;
	GList	       *l;
	GSList	       *names;
	const gchar    *prefix;
	const gchar    *parent_prefix;

	parent_prefix = NULL;

	if (service_str) {
		TrackerService *service;
		TrackerService *parent;
		const gchar    *parent_name;

		service = tracker_ontology_get_service_by_name (service_str);
		if (!service) {
			return NULL;
		}

		/* Prefix for properties of the category */
		prefix = tracker_service_get_property_prefix (service);

		if (!prefix || g_strcmp0 (prefix, " ") == 0) {
			prefix = service_str;
		}

		/* Prefix for properties of the parent */
		parent_name = tracker_ontology_get_service_parent (service_str);

		if (parent_name && g_strcmp0 (parent_name, " ") != 0) {
			parent = tracker_ontology_get_service_by_name (parent_name);

			if (parent) {
				parent_prefix = tracker_service_get_property_prefix (parent);

				if (!parent_prefix || g_strcmp0 (parent_prefix, " ") == 0) {
					parent_prefix = parent_name;
				}
			}
		}
	}

	names = NULL;
	fields = g_hash_table_get_values (field_names);

	for (l = fields; l; l = l->next) {
		TrackerField *field;
		const gchar  *name;

		field = l->data;
		name = tracker_field_get_name (field);

		if (service_str == NULL ||
		    (prefix && g_str_has_prefix (name, prefix)) ||
		    (parent_prefix && g_str_has_prefix (name, parent_prefix))) {
			names = g_slist_prepend (names, g_strdup (name));
		}
	}

	g_list_free (fields);

	return names;
}

/*
 * Service data
 */
gboolean
tracker_ontology_service_is_valid (const gchar *service_str)
{
	g_return_val_if_fail (service_str != NULL, FALSE);

	return tracker_ontology_get_service_id_by_name (service_str) != -1;
}

gboolean
tracker_ontology_service_has_embedded (const gchar *service_str)
{
	TrackerService *service;

	g_return_val_if_fail (service_str != NULL, FALSE);

	service = g_hash_table_lookup (service_names, service_str);

	if (!service) {
		return FALSE;
	}

	return tracker_service_get_embedded (service);
}

gboolean
tracker_ontology_service_has_metadata (const gchar *service_str)
{
	TrackerService *service;

	g_return_val_if_fail (service_str != NULL, FALSE);

	service = g_hash_table_lookup (service_names, service_str);

	if (!service) {
		return FALSE;
	}

	return tracker_service_get_has_metadata (service);
}

gboolean
tracker_ontology_service_has_thumbnails (const gchar *service_str)
{
	TrackerService *service;

	g_return_val_if_fail (service_str != NULL, FALSE);

	service = g_hash_table_lookup (service_names, service_str);

	if (!service) {
		return FALSE;
	}

	return tracker_service_get_has_thumbs (service);
}

gboolean
tracker_ontology_service_has_text (const char *service_str)
{
	TrackerService *service;

	g_return_val_if_fail (service_str != NULL, FALSE);

	service = g_hash_table_lookup (service_names, service_str);

	if (!service) {
		return FALSE;
	}

	return tracker_service_get_has_full_text (service);
}

gint
tracker_ontology_service_get_key_metadata (const gchar *service_str,
					   const gchar *meta_name)
{
	TrackerService *service;
	gint		i;
	const GSList   *l;

	g_return_val_if_fail (service_str != NULL, 0);
	g_return_val_if_fail (meta_name != NULL, 0);

	service = g_hash_table_lookup (service_names, service_str);

	if (!service) {
		return 0;
	}

	for (l = tracker_service_get_key_metadata (service), i = 1;
	     l;
	     l = l->next, i++) {
		if (!l->data) {
			continue;
		}

		if (strcasecmp (l->data, meta_name) == 0) {
			return i;
		}
	}

	return 0;
}

gboolean
tracker_ontology_service_get_show_directories (const gchar *service_str)
{
	TrackerService *service;

	service = g_hash_table_lookup (service_names, service_str);

	if (!service) {
		return FALSE;
	}

	return tracker_service_get_show_service_directories (service);
}

gboolean
tracker_ontology_service_get_show_files (const gchar *service_str)
{
	TrackerService *service;

	service = g_hash_table_lookup (service_names, service_str);

	if (!service) {
		return FALSE;
	}

	return tracker_service_get_show_service_files (service);
}

/* Field mechanics */
void
tracker_ontology_field_add (TrackerField *field)
{
	const gchar *name;

	g_return_if_fail (TRACKER_IS_FIELD (field));

	name = tracker_field_get_name (field);
	g_return_if_fail (name != NULL);

	g_hash_table_insert (field_names,
			     /* g_utf8_collate_key (tracker_field_get_name (field), -1), */
			     g_strdup (name),
			     g_object_ref (field));
}

TrackerField *
tracker_ontology_get_field_by_name (const gchar *name)
{
	g_return_val_if_fail (name != NULL, NULL);

	return g_hash_table_lookup (field_names, name);
}

TrackerField *
tracker_ontology_get_field_by_id (gint id)
{
	TrackerField *field = NULL;
	GList	     *values;
	GList	     *l;

	/* TODO Create a hashtable with id -> field def. More efficient */

	values = g_hash_table_get_values (field_names);

	for (l = values; l && !field; l = l->next) {
		if (atoi (tracker_field_get_id (l->data)) == id) {
			field = l->data;
		}
	}

	g_list_free (values);

	return field;
}

gchar *
tracker_ontology_get_field_name_by_service_name (TrackerField *field,
						 const gchar  *service_str)
{
	const gchar *field_name;
	const gchar *meta_name;
	gint	     key_field;

	g_return_val_if_fail (TRACKER_IS_FIELD (field), NULL);
	g_return_val_if_fail (service_str != NULL, NULL);

	meta_name = tracker_field_get_name (field);
	key_field = tracker_ontology_service_get_key_metadata (service_str,
							       meta_name);

	if (key_field > 0) {
		return g_strdup_printf ("KeyMetadata%d", key_field);

	}

	/* TODO do it using field_name in TrackerField! */
	field_name = tracker_field_get_field_name (field);
	if (field_name) {
		return g_strdup (field_name);
	} else {
		return NULL;
	}
}

/*
 * Field data
 */
gchar *
tracker_ontology_field_get_display_name (TrackerField *field)
{
	TrackerFieldType type;

	g_return_val_if_fail (TRACKER_IS_FIELD (field), NULL);

	type = tracker_field_get_data_type (field);

	if (type == TRACKER_FIELD_TYPE_INDEX ||
	    type == TRACKER_FIELD_TYPE_STRING ||
	    type == TRACKER_FIELD_TYPE_DOUBLE) {
		return g_strdup ("MetaDataDisplay");
	}

	return g_strdup ("MetaDataValue");
}

const gchar *
tracker_ontology_field_get_id (const gchar *name)
{
	TrackerField *field;

	g_return_val_if_fail (name != NULL, NULL);

	field = tracker_ontology_get_field_by_name (name);

	if (field) {
		return tracker_field_get_id (field);
	}

	return NULL;
}

gboolean
tracker_ontology_field_is_child_of (const gchar *field_str_child,
				    const gchar *field_str_parent)
{
	TrackerField *field_child;
	TrackerField *field_parent;
	const GSList *l;

	g_return_val_if_fail (field_str_child != NULL, FALSE);
	g_return_val_if_fail (field_str_parent != NULL, FALSE);

	field_child = tracker_ontology_get_field_by_name (field_str_child);

	if (!field_child) {
		return FALSE;
	}

	field_parent = tracker_ontology_get_field_by_name (field_str_parent);

	if (!field_parent) {
		return FALSE;
	}

	for (l = tracker_field_get_child_ids (field_parent); l; l = l->next) {
		if (!l->data) {
			return FALSE;
		}

		if (strcmp (tracker_field_get_id (field_child), l->data) == 0) {
			return TRUE;
		}
	}

	return FALSE;
}
