/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia (urho.konttori@nokia.com)
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

#include <string.h>
#include <stdlib.h>

#include <glib.h>

#include "tracker-ontology.h"

typedef struct {
	gchar  *name;
	GArray *subcategories;
} CalculateSubcategoriesForEach;

static gboolean    initialized;

/* List of TrackerNamespace objects */
static GArray     *namespaces;

/* Namespace uris */
static GHashTable *namespace_uris;

/* List of TrackerClass objects */
static GArray     *classes;

/* Hash (gchar *class_uri, TrackerClass *service) */
static GHashTable *class_uris;

/* List of TrackerProperty objects */
static GArray     *properties;

/* Field uris */
static GHashTable *property_uris;

/* FieldType enum class */
static gpointer    property_type_enum_class;

void
tracker_ontology_init (void)
{
	if (initialized) {
		return;
	}

	namespaces = g_array_new (TRUE, TRUE, sizeof (TrackerNamespace *));

	namespace_uris = g_hash_table_new_full (g_str_hash,
					      g_str_equal,
					      g_free,
					      g_object_unref);

	classes = g_array_new (TRUE, TRUE, sizeof (TrackerClass *));

	class_uris = g_hash_table_new_full (g_str_hash,
					      g_str_equal,
					      g_free,
					      g_object_unref);

	properties = g_array_new (TRUE, TRUE, sizeof (TrackerProperty *));

	property_uris = g_hash_table_new_full (g_str_hash,
					    g_str_equal,
					    g_free,
					    g_object_unref);

	/* We will need the class later in order to match strings to enum values
	 * when inserting metadata types in the DB, so the enum class needs to be
	 * created beforehand.
	 */
	property_type_enum_class = g_type_class_ref (TRACKER_TYPE_PROPERTY_TYPE);

	initialized = TRUE;
}

void
tracker_ontology_shutdown (void)
{
	gint i;

	if (!initialized) {
		return;
	}

	for (i = 0; i < namespaces->len; i++) {
		g_object_unref (g_array_index (namespaces, TrackerNamespace *, i));
	}
	g_array_free (namespaces, TRUE);

	g_hash_table_unref (namespace_uris);
	namespace_uris = NULL;

	for (i = 0; i < classes->len; i++) {
		g_object_unref (g_array_index (classes, TrackerClass *, i));
	}
	g_array_free (classes, TRUE);

	g_hash_table_unref (class_uris);
	class_uris = NULL;

	for (i = 0; i < properties->len; i++) {
		g_object_unref (g_array_index (properties, TrackerProperty *, i));
	}
	g_array_free (properties, TRUE);

	g_hash_table_unref (property_uris);
	property_uris = NULL;

	g_type_class_unref (property_type_enum_class);
	property_type_enum_class = NULL;

	initialized = FALSE;
}

void
tracker_ontology_add_class (TrackerClass *service)
{

	const gchar	    *uri, *name;

	g_return_if_fail (TRACKER_IS_CLASS (service));

	uri = tracker_class_get_uri (service);
	name = tracker_class_get_name (service);

	g_object_ref (service);
	g_array_append_val (classes, service);

	if (uri) {
		g_hash_table_insert (class_uris,
				     g_strdup (uri),
				     g_object_ref (service));
	}
}

TrackerClass *
tracker_ontology_get_class_by_uri (const gchar *class_uri)
{
	g_return_val_if_fail (class_uri != NULL, NULL);

	return g_hash_table_lookup (class_uris, class_uri);
}

TrackerNamespace **
tracker_ontology_get_namespaces (void)
{
	/* copy len + 1 elements to include NULL terminator */
	return g_memdup (namespaces->data, sizeof (TrackerNamespace *) * (namespaces->len + 1));
}

TrackerClass **
tracker_ontology_get_classes (void)
{
	/* copy len + 1 elements to include NULL terminator */
	return g_memdup (classes->data, sizeof (TrackerClass *) * (classes->len + 1));
}

TrackerProperty **
tracker_ontology_get_properties (void)
{
	/* copy len + 1 elements to include NULL terminator */
	return g_memdup (properties->data, sizeof (TrackerProperty *) * (properties->len + 1));
}

/* Field mechanics */
void
tracker_ontology_add_property (TrackerProperty *field)
{
	const gchar *uri;

	g_return_if_fail (TRACKER_IS_PROPERTY (field));

	uri = tracker_property_get_uri (field);

	g_object_ref (field);
	g_array_append_val (properties, field);

	g_hash_table_insert (property_uris,
			     g_strdup (uri),
			     g_object_ref (field));
}

TrackerProperty *
tracker_ontology_get_property_by_uri (const gchar *uri)
{
	g_return_val_if_fail (uri != NULL, NULL);

	return g_hash_table_lookup (property_uris, uri);
}

void
tracker_ontology_add_namespace (TrackerNamespace *namespace)
{
	const gchar *uri;

	g_return_if_fail (TRACKER_IS_NAMESPACE (namespace));

	uri = tracker_namespace_get_uri (namespace);

	g_object_ref (namespace);
	g_array_append_val (namespaces, namespace);

	g_hash_table_insert (namespace_uris,
			     g_strdup (uri),
			     g_object_ref (namespace));
}

TrackerNamespace *
tracker_ontology_get_namespace_by_uri (const gchar *uri)
{
	g_return_val_if_fail (uri != NULL, NULL);

	return g_hash_table_lookup (namespace_uris, uri);
}



