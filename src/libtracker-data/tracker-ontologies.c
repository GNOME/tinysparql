/*
 * Copyright (C) 2006, Jamie McCracken (jamiemcc@gnome.org)
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

#include <string.h>
#include <stdlib.h>

#include <glib.h>

#include "tracker-ontologies.h"

static gboolean    initialized;

/* List of TrackerNamespace objects */
static GPtrArray  *namespaces;

/* Namespace uris */
static GHashTable *namespace_uris;

/* List of TrackerOntology objects */
static GPtrArray  *ontologies;

/* Ontology uris */
static GHashTable *ontology_uris;

/* List of TrackerClass objects */
static GPtrArray  *classes;

/* Hash (gchar *class_uri, TrackerClass *service) */
static GHashTable *class_uris;

/* List of TrackerProperty objects */
static GPtrArray  *properties;

/* Field uris */
static GHashTable *property_uris;

/* FieldType enum class */
static gpointer    property_type_enum_class;

/* Hash (int id, const gchar *uri) */
static GHashTable *id_uri_pairs;

void
tracker_ontologies_init (void)
{
	if (initialized) {
		return;
	}

	namespaces = g_ptr_array_new ();

	ontologies = g_ptr_array_new ();

	namespace_uris = g_hash_table_new_full (g_str_hash,
	                                        g_str_equal,
	                                        g_free,
	                                        g_object_unref);

	ontology_uris = g_hash_table_new_full (g_str_hash,
	                                       g_str_equal,
	                                       g_free,
	                                       g_object_unref);

	classes = g_ptr_array_new ();

	class_uris = g_hash_table_new_full (g_str_hash,
	                                    g_str_equal,
	                                    g_free,
	                                    g_object_unref);

	id_uri_pairs = g_hash_table_new_full (g_direct_hash, g_direct_equal,
	                                      NULL,
	                                      g_free);

	properties = g_ptr_array_new ();

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
tracker_ontologies_shutdown (void)
{
	if (!initialized) {
		return;
	}

	g_ptr_array_foreach (namespaces, (GFunc) g_object_unref, NULL);
	g_ptr_array_free (namespaces, TRUE);

	g_hash_table_unref (namespace_uris);
	namespace_uris = NULL;

	g_ptr_array_foreach (ontologies, (GFunc) g_object_unref, NULL);
	g_ptr_array_free (ontologies, TRUE);

	g_hash_table_unref (ontology_uris);
	ontology_uris = NULL;

	g_ptr_array_foreach (classes, (GFunc) g_object_unref, NULL);
	g_ptr_array_free (classes, TRUE);

	g_hash_table_unref (class_uris);
	class_uris = NULL;

	g_hash_table_unref (id_uri_pairs);
	id_uri_pairs = NULL;

	g_ptr_array_foreach (properties, (GFunc) g_object_unref, NULL);
	g_ptr_array_free (properties, TRUE);

	g_hash_table_unref (property_uris);
	property_uris = NULL;

	g_type_class_unref (property_type_enum_class);
	property_type_enum_class = NULL;

	initialized = FALSE;
}

const gchar*
tracker_ontologies_get_uri_by_id (gint id)
{
	g_return_val_if_fail (id != -1, NULL);

	return g_hash_table_lookup (id_uri_pairs, GINT_TO_POINTER (id));
}

void
tracker_ontologies_add_class (TrackerClass *service)
{

	const gchar         *uri, *name;

	g_return_if_fail (TRACKER_IS_CLASS (service));

	uri = tracker_class_get_uri (service);
	name = tracker_class_get_name (service);

	g_ptr_array_add (classes, g_object_ref (service));

	if (uri) {
		g_hash_table_insert (class_uris,
		                     g_strdup (uri),
		                     g_object_ref (service));
	}
}

TrackerClass *
tracker_ontologies_get_class_by_uri (const gchar *class_uri)
{
	g_return_val_if_fail (class_uri != NULL, NULL);

	return g_hash_table_lookup (class_uris, class_uri);
}

TrackerNamespace **
tracker_ontologies_get_namespaces (guint *length)
{
	if (G_UNLIKELY (!namespaces)) {
		*length = 0;
		return NULL;
	}

	*length = namespaces->len;
	return (TrackerNamespace **) namespaces->pdata;
}

TrackerOntology **
tracker_ontologies_get_ontologies (guint *length)
{
	if (G_UNLIKELY (!ontologies)) {
		*length = 0;
		return NULL;
	}

	*length = ontologies->len;
	return (TrackerOntology **) ontologies->pdata;
}

TrackerClass **
tracker_ontologies_get_classes (guint *length)
{
	if (G_UNLIKELY (!classes)) {
		*length = 0;
		return NULL;
	}

	*length = classes->len;
	return (TrackerClass **) classes->pdata;
}

TrackerProperty **
tracker_ontologies_get_properties (guint *length)
{
	if (G_UNLIKELY (!properties)) {
		*length = 0;
		return NULL;
	}

	*length = properties->len;
	return (TrackerProperty **) properties->pdata;
}

/* Field mechanics */
void
tracker_ontologies_add_property (TrackerProperty *field)
{
	const gchar *uri;

	g_return_if_fail (TRACKER_IS_PROPERTY (field));

	uri = tracker_property_get_uri (field);

	g_ptr_array_add (properties, g_object_ref (field));

	g_hash_table_insert (property_uris,
	                     g_strdup (uri),
	                     g_object_ref (field));
}

void
tracker_ontologies_add_id_uri_pair (gint id, const gchar *uri)
{
	g_hash_table_insert (id_uri_pairs,
	                     GINT_TO_POINTER (id),
	                     g_strdup (uri));
}

TrackerProperty *
tracker_ontologies_get_property_by_uri (const gchar *uri)
{
	g_return_val_if_fail (uri != NULL, NULL);

	return g_hash_table_lookup (property_uris, uri);
}

void
tracker_ontologies_add_namespace (TrackerNamespace *namespace)
{
	const gchar *uri;

	g_return_if_fail (TRACKER_IS_NAMESPACE (namespace));

	uri = tracker_namespace_get_uri (namespace);

	g_ptr_array_add (namespaces, g_object_ref (namespace));

	g_hash_table_insert (namespace_uris,
	                     g_strdup (uri),
	                     g_object_ref (namespace));
}

void
tracker_ontologies_add_ontology (TrackerOntology *ontology)
{
	const gchar *uri;

	g_return_if_fail (TRACKER_IS_ONTOLOGY (ontology));

	uri = tracker_ontology_get_uri (ontology);

	g_ptr_array_add (ontologies, g_object_ref (ontology));

	g_hash_table_insert (ontology_uris,
	                     g_strdup (uri),
	                     g_object_ref (ontology));
}

TrackerNamespace *
tracker_ontologies_get_namespace_by_uri (const gchar *uri)
{
	g_return_val_if_fail (uri != NULL, NULL);

	return g_hash_table_lookup (namespace_uris, uri);
}


TrackerOntology *
tracker_ontologies_get_ontology_by_uri (const gchar *uri)
{
	g_return_val_if_fail (uri != NULL, NULL);

	return g_hash_table_lookup (ontology_uris, uri);
}
