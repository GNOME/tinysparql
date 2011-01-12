/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
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

#include <gvdb/gvdb-builder.h>
#include <gvdb/gvdb-reader.h>
#include <libtracker-common/tracker-ontologies.h>

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

/* rdf:type */
static TrackerProperty *rdf_type = NULL;

static GvdbTable *gvdb_table;
static GvdbTable *gvdb_namespaces_table;
static GvdbTable *gvdb_classes_table;
static GvdbTable *gvdb_properties_table;

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

	if (rdf_type) {
		g_object_unref (rdf_type);
		rdf_type = NULL;
	}

	if (gvdb_table) {
		gvdb_table_unref (gvdb_properties_table);
		gvdb_properties_table = NULL;

		gvdb_table_unref (gvdb_classes_table);
		gvdb_classes_table = NULL;

		gvdb_table_unref (gvdb_namespaces_table);
		gvdb_namespaces_table = NULL;

		gvdb_table_unref (gvdb_table);
		gvdb_table = NULL;
	}

	initialized = FALSE;
}

TrackerProperty *
tracker_ontologies_get_rdf_type (void)
{
	g_return_val_if_fail (rdf_type != NULL, NULL);

	return rdf_type;
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

	const gchar *uri;

	g_return_if_fail (TRACKER_IS_CLASS (service));

	uri = tracker_class_get_uri (service);

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
	TrackerClass *class;

	g_return_val_if_fail (class_uri != NULL, NULL);

	class = g_hash_table_lookup (class_uris, class_uri);

	if (!class && gvdb_table) {
		if (tracker_ontologies_get_class_string_gvdb (class_uri, "name") != NULL) {
			class = tracker_class_new (TRUE);
			tracker_class_set_uri (class, class_uri);

			g_hash_table_insert (class_uris,
				             g_strdup (class_uri),
				             class);
		}
	}

	return class;
}

TrackerNamespace **
tracker_ontologies_get_namespaces (guint *length)
{
	if (namespaces->len == 0 && gvdb_table) {
		gchar **namespace_uris;
		gint i;

		namespace_uris = gvdb_table_list (gvdb_namespaces_table, "");

		for (i = 0; namespace_uris[i]; i++) {
			TrackerNamespace *namespace;

			namespace = tracker_ontologies_get_namespace_by_uri (namespace_uris[i]);

			g_ptr_array_add (namespaces, g_object_ref (namespace));
		}

		g_strfreev (namespace_uris);
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
	if (classes->len == 0 && gvdb_table) {
		gchar **class_uris;
		gint i;

		class_uris = gvdb_table_list (gvdb_classes_table, "");

		for (i = 0; class_uris[i]; i++) {
			TrackerClass *class;

			class = tracker_ontologies_get_class_by_uri (class_uris[i]);

			g_ptr_array_add (classes, g_object_ref (class));
		}

		g_strfreev (class_uris);
	}

	*length = classes->len;
	return (TrackerClass **) classes->pdata;
}

TrackerProperty **
tracker_ontologies_get_properties (guint *length)
{
	if (properties->len == 0 && gvdb_table) {
		gchar **property_uris;
		gint i;

		property_uris = gvdb_table_list (gvdb_properties_table, "");

		for (i = 0; property_uris[i]; i++) {
			TrackerProperty *property;

			property = tracker_ontologies_get_property_by_uri (property_uris[i]);

			g_ptr_array_add (properties, g_object_ref (property));
		}

		g_strfreev (property_uris);
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

	if (g_strcmp0 (uri, TRACKER_RDF_PREFIX "type") == 0) {
		if (rdf_type) {
			g_object_unref (rdf_type);
		}
		rdf_type = g_object_ref (field);
	}

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
	TrackerProperty *property;

	g_return_val_if_fail (uri != NULL, NULL);

	property = g_hash_table_lookup (property_uris, uri);

	if (!property && gvdb_table) {
		if (tracker_ontologies_get_property_string_gvdb (uri, "name") != NULL) {
			property = tracker_property_new (TRUE);
			tracker_property_set_uri (property, uri);

			g_hash_table_insert (property_uris,
				             g_strdup (uri),
				             property);
		}
	}

	return property;
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
	TrackerNamespace *namespace;

	g_return_val_if_fail (uri != NULL, NULL);

	namespace = g_hash_table_lookup (namespace_uris, uri);

	if (!namespace && gvdb_table) {
		if (tracker_ontologies_get_namespace_string_gvdb (uri, "prefix") != NULL) {
			namespace = tracker_namespace_new (TRUE);
			tracker_namespace_set_uri (namespace, uri);

			g_hash_table_insert (namespace_uris,
					     g_strdup (uri),
					     namespace);
		}
	}

	return namespace;
}


TrackerOntology *
tracker_ontologies_get_ontology_by_uri (const gchar *uri)
{
	g_return_val_if_fail (uri != NULL, NULL);

	return g_hash_table_lookup (ontology_uris, uri);
}

static void
gvdb_hash_table_insert_variant (GHashTable  *table,
                                GvdbItem    *parent,
                                const gchar *uri,
                                const gchar *predicate,
                                GVariant    *value)
{
	gchar *key;
	GvdbItem *item;

	key = g_strdup_printf ("%s#%s", uri, predicate);
	item = gvdb_hash_table_insert (table, key);
	gvdb_item_set_parent (item, parent);
	gvdb_item_set_value (item, value);
	g_free (key);
}

static GvdbItem *
gvdb_hash_table_insert_item (GHashTable *table,
                             GvdbItem   *root,
                             const gchar *uri)
{
	GvdbItem *item;

	item = gvdb_hash_table_insert (table, uri);
	gvdb_item_set_parent (item, root);

	return item;
}

static void
gvdb_hash_table_insert_statement (GHashTable  *table,
                                  GvdbItem    *parent,
                                  const gchar *uri,
                                  const gchar *predicate,
                                  const gchar *value)
{
	gvdb_hash_table_insert_variant (table, parent, uri, predicate, g_variant_new_string (value));
}

void
tracker_ontologies_write_gvdb (const gchar  *filename,
                               GError      **error)
{
	GHashTable *root_table, *table;
	GvdbItem *root, *item;
	const gchar *uri;
	gint i;

	root_table = gvdb_hash_table_new (NULL, NULL);

	table = gvdb_hash_table_new (root_table, "namespaces");
	root = gvdb_hash_table_insert (table, "");
	for (i = 0; i < namespaces->len; i++) {
		TrackerNamespace *namespace;

		namespace = namespaces->pdata[i];
		uri = tracker_namespace_get_uri (namespace);

		item = gvdb_hash_table_insert_item (table, root, uri);

		gvdb_hash_table_insert_statement (table, item, uri, "prefix", tracker_namespace_get_prefix (namespace));
	}
	g_hash_table_unref (table);

	table = gvdb_hash_table_new (root_table, "classes");
	root = gvdb_hash_table_insert (table, "");
	for (i = 0; i < classes->len; i++) {
		TrackerClass *class;
		TrackerClass **super_classes;
		GVariantBuilder builder;

		class = classes->pdata[i];
		uri = tracker_class_get_uri (class);

		item = gvdb_hash_table_insert_item (table, root, uri);

		gvdb_hash_table_insert_statement (table, item, uri, "name", tracker_class_get_name (class));

		super_classes = tracker_class_get_super_classes (class);
		if (super_classes) {
			g_variant_builder_init (&builder, G_VARIANT_TYPE ("as"));

			while (*super_classes) {
				g_variant_builder_add (&builder, "s", tracker_class_get_uri (*super_classes));
				super_classes++;
			}

			gvdb_hash_table_insert_variant (table, item, uri, "super-classes", g_variant_builder_end (&builder));
		}
	}
	g_hash_table_unref (table);

	table = gvdb_hash_table_new (root_table, "properties");
	root = gvdb_hash_table_insert (table, "");
	for (i = 0; i < properties->len; i++) {
		TrackerProperty *property;
		TrackerClass **domain_indexes;
		GVariantBuilder builder;

		property = properties->pdata[i];
		uri = tracker_property_get_uri (property);

		item = gvdb_hash_table_insert_item (table, root, uri);

		gvdb_hash_table_insert_statement (table, item, uri, "name", tracker_property_get_name (property));
		gvdb_hash_table_insert_statement (table, item, uri, "domain", tracker_class_get_uri (tracker_property_get_domain (property)));
		gvdb_hash_table_insert_statement (table, item, uri, "range", tracker_class_get_uri (tracker_property_get_range (property)));

		if (!tracker_property_get_multiple_values (property)) {
			gvdb_hash_table_insert_variant (table, item, uri, "max-cardinality", g_variant_new_int32 (1));
		}

		if (tracker_property_get_is_inverse_functional_property (property)) {
			gvdb_hash_table_insert_variant (table, item, uri, "inverse-functional", g_variant_new_boolean (TRUE));
		}

		domain_indexes = tracker_property_get_domain_indexes (property);
		if (domain_indexes) {
			g_variant_builder_init (&builder, G_VARIANT_TYPE ("as"));

			while (*domain_indexes) {
				g_variant_builder_add (&builder, "s", tracker_class_get_uri (*domain_indexes));
				domain_indexes++;
			}

			gvdb_hash_table_insert_variant (table, item, uri, "domain-indexes", g_variant_builder_end (&builder));
		}
	}
	g_hash_table_unref (table);

	gvdb_table_write_contents (root_table, filename, FALSE, error);

	g_hash_table_unref (root_table);
}

void
tracker_ontologies_load_gvdb (const gchar  *filename,
                              GError      **error)
{
	tracker_ontologies_shutdown ();

	tracker_ontologies_init ();

	gvdb_table = gvdb_table_new (filename, TRUE, error);

	gvdb_namespaces_table = gvdb_table_get_table (gvdb_table, "namespaces");
	gvdb_classes_table = gvdb_table_get_table (gvdb_table, "classes");
	gvdb_properties_table = gvdb_table_get_table (gvdb_table, "properties");
}

GVariant *
tracker_ontologies_get_namespace_value_gvdb (const gchar *uri,
                                             const gchar *predicate)
{
	gchar *key;
	GVariant *value;

	key = g_strdup_printf ("%s#%s", uri, predicate);
	value = gvdb_table_get_value (gvdb_namespaces_table, key);
	g_free (key);

	return value;
}

const gchar *
tracker_ontologies_get_namespace_string_gvdb (const gchar *uri,
                                              const gchar *predicate)
{
	GVariant *value;
	const gchar *result;

	value = tracker_ontologies_get_namespace_value_gvdb (uri, predicate);

	if (value == NULL) {
		return NULL;
	}

	result = g_variant_get_string (value, NULL);
	g_variant_unref (value);

	return result;
}

GVariant *
tracker_ontologies_get_class_value_gvdb (const gchar *uri,
                                         const gchar *predicate)
{
	gchar *key;
	GVariant *value;

	key = g_strdup_printf ("%s#%s", uri, predicate);
	value = gvdb_table_get_value (gvdb_classes_table, key);
	g_free (key);

	return value;
}

const gchar *
tracker_ontologies_get_class_string_gvdb (const gchar *uri,
                                          const gchar *predicate)
{
	GVariant *value;
	const gchar *result;

	value = tracker_ontologies_get_class_value_gvdb (uri, predicate);

	if (value == NULL) {
		return NULL;
	}

	result = g_variant_get_string (value, NULL);
	g_variant_unref (value);

	return result;
}

GVariant *
tracker_ontologies_get_property_value_gvdb (const gchar *uri,
                                            const gchar *predicate)
{
	gchar *key;
	GVariant *value;

	key = g_strdup_printf ("%s#%s", uri, predicate);
	value = gvdb_table_get_value (gvdb_properties_table, key);
	g_free (key);

	return value;
}

const gchar *
tracker_ontologies_get_property_string_gvdb (const gchar *uri,
                                             const gchar *predicate)
{
	GVariant *value;
	const gchar *result;

	value = tracker_ontologies_get_property_value_gvdb (uri, predicate);

	if (value == NULL) {
		return NULL;
	}

	result = g_variant_get_string (value, NULL);
	g_variant_unref (value);

	return result;
}

static gint
class_sort_func (gconstpointer a,
                 gconstpointer b)
{
	return g_strcmp0 (tracker_class_get_name (*((TrackerClass **) a)),
	                  tracker_class_get_name (*((TrackerClass **) b)));
}

void
tracker_ontologies_sort (void)
{
	/* Sort result so it is alphabetical */
	g_ptr_array_sort (classes, class_sort_func);
}
