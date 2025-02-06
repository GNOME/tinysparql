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

#include "tracker-ontologies.h"

#include "core/tracker-ontologies.h"

typedef struct _TrackerOntologiesPrivate TrackerOntologiesPrivate;

struct _TrackerOntologiesPrivate {
	/* List of TrackerNamespace objects */
	GPtrArray  *namespaces;

	/* Namespace uris */
	GHashTable *namespace_uris;

	/* List of TrackerOntology objects */
	GPtrArray  *ontologies;

	/* Ontology uris */
	GHashTable *ontology_uris;

	/* List of TrackerClass objects */
	GPtrArray  *classes;

	/* Hash (gchar *class_uri, TrackerClass *service) */
	GHashTable *class_uris;

	/* List of TrackerProperty objects */
	GPtrArray  *properties;

	/* Field uris */
	GHashTable *property_uris;

	/* FieldType enum class */
	gpointer property_type_enum_class;

	/* Hash (int id, const gchar *uri) */
	GHashTable *id_uri_pairs;

	/* Some fast paths for frequent properties */
	TrackerProperty *rdf_type;
	TrackerProperty *nrl_added;
	TrackerProperty *nrl_modified;
};

G_DEFINE_TYPE_WITH_PRIVATE (TrackerOntologies, tracker_ontologies, G_TYPE_OBJECT)

static void
tracker_ontologies_init (TrackerOntologies *ontologies)
{
	TrackerOntologiesPrivate *priv = tracker_ontologies_get_instance_private (ontologies);

	priv->namespaces = g_ptr_array_new_with_free_func (g_object_unref);

	priv->ontologies = g_ptr_array_new_with_free_func (g_object_unref);

	priv->namespace_uris = g_hash_table_new_full (g_str_hash,
	                                              g_str_equal,
	                                              g_free,
	                                              g_object_unref);

	priv->ontology_uris = g_hash_table_new_full (g_str_hash,
	                                             g_str_equal,
	                                             g_free,
	                                             g_object_unref);

	priv->classes = g_ptr_array_new_with_free_func (g_object_unref);

	priv->class_uris = g_hash_table_new_full (g_str_hash,
	                                          g_str_equal,
	                                          g_free,
	                                          g_object_unref);

	priv->id_uri_pairs = g_hash_table_new_full (tracker_rowid_hash, tracker_rowid_equal,
	                                            (GDestroyNotify) tracker_rowid_free,
	                                            g_free);

	priv->properties = g_ptr_array_new_with_free_func (g_object_unref);

	priv->property_uris = g_hash_table_new_full (g_str_hash,
	                                             g_str_equal,
	                                             g_free,
	                                             g_object_unref);
}

static void
tracker_ontologies_finalize (GObject *object)
{
	TrackerOntologies *ontologies = TRACKER_ONTOLOGIES (object);
	TrackerOntologiesPrivate *priv = tracker_ontologies_get_instance_private (ontologies);

	g_ptr_array_free (priv->namespaces, TRUE);
	g_hash_table_unref (priv->namespace_uris);

	g_ptr_array_free (priv->ontologies, TRUE);
	g_hash_table_unref (priv->ontology_uris);

	g_ptr_array_free (priv->classes, TRUE);
	g_hash_table_unref (priv->class_uris);

	g_hash_table_unref (priv->id_uri_pairs);

	g_ptr_array_free (priv->properties, TRUE);
	g_hash_table_unref (priv->property_uris);

	g_clear_object (&priv->rdf_type);
	g_clear_object (&priv->nrl_added);
	g_clear_object (&priv->nrl_modified);

	G_OBJECT_CLASS (tracker_ontologies_parent_class)->finalize (object);
}

static void
tracker_ontologies_class_init (TrackerOntologiesClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_ontologies_finalize;
}

TrackerProperty *
tracker_ontologies_get_rdf_type (TrackerOntologies *ontologies)
{
	TrackerOntologiesPrivate *priv = tracker_ontologies_get_instance_private (ontologies);

	g_return_val_if_fail (priv->rdf_type != NULL, NULL);

	return priv->rdf_type;
}

TrackerProperty *
tracker_ontologies_get_nrl_added (TrackerOntologies *ontologies)
{
	TrackerOntologiesPrivate *priv = tracker_ontologies_get_instance_private (ontologies);

	g_return_val_if_fail (priv->nrl_added != NULL, NULL);

	return priv->nrl_added;
}

TrackerProperty *
tracker_ontologies_get_nrl_modified (TrackerOntologies *ontologies)
{
	TrackerOntologiesPrivate *priv = tracker_ontologies_get_instance_private (ontologies);

	g_return_val_if_fail (priv->nrl_modified != NULL, NULL);

	return priv->nrl_modified;
}

const gchar*
tracker_ontologies_get_uri_by_id (TrackerOntologies *ontologies,
                                  TrackerRowid       id)
{
	TrackerOntologiesPrivate *priv = tracker_ontologies_get_instance_private (ontologies);

	g_return_val_if_fail (id != -1, NULL);

	return g_hash_table_lookup (priv->id_uri_pairs, &id);
}

void
tracker_ontologies_add_class (TrackerOntologies *ontologies,
                              TrackerClass      *service)
{
	TrackerOntologiesPrivate *priv = tracker_ontologies_get_instance_private (ontologies);
	const gchar *uri;

	g_return_if_fail (TRACKER_IS_CLASS (service));

	uri = tracker_class_get_uri (service);

	g_ptr_array_add (priv->classes, g_object_ref (service));
	tracker_class_set_ontologies (service, ontologies);

	if (uri) {
		g_hash_table_insert (priv->class_uris,
		                     g_strdup (uri),
		                     g_object_ref (service));
	}
}

TrackerClass *
tracker_ontologies_get_class_by_uri (TrackerOntologies *ontologies,
                                     const gchar       *class_uri)
{
	TrackerOntologiesPrivate *priv = tracker_ontologies_get_instance_private (ontologies);

	g_return_val_if_fail (class_uri != NULL, NULL);

	return g_hash_table_lookup (priv->class_uris, class_uri);
}

TrackerNamespace **
tracker_ontologies_get_namespaces (TrackerOntologies *ontologies,
                                   guint             *length)
{
	TrackerOntologiesPrivate *priv = tracker_ontologies_get_instance_private (ontologies);

	*length = priv->namespaces->len;
	return (TrackerNamespace **) priv->namespaces->pdata;
}

TrackerClass **
tracker_ontologies_get_classes (TrackerOntologies *ontologies,
                                guint             *length)
{
	TrackerOntologiesPrivate *priv = tracker_ontologies_get_instance_private (ontologies);

	*length = priv->classes->len;
	return (TrackerClass **) priv->classes->pdata;
}

TrackerProperty **
tracker_ontologies_get_properties (TrackerOntologies *ontologies,
                                   guint             *length)
{
	TrackerOntologiesPrivate *priv = tracker_ontologies_get_instance_private (ontologies);

	*length = priv->properties->len;
	return (TrackerProperty **) priv->properties->pdata;
}

/* Field mechanics */
void
tracker_ontologies_add_property (TrackerOntologies *ontologies,
                                 TrackerProperty   *field)
{
	TrackerOntologiesPrivate *priv = tracker_ontologies_get_instance_private (ontologies);
	const gchar *uri;

	g_return_if_fail (TRACKER_IS_PROPERTY (field));

	uri = tracker_property_get_uri (field);

	if (g_strcmp0 (uri, TRACKER_PREFIX_RDF "type") == 0) {
		g_set_object (&priv->rdf_type, field);
	} else if (g_strcmp0 (uri, TRACKER_PREFIX_NRL "added") == 0) {
		g_set_object (&priv->nrl_added, field);
	} else if (g_strcmp0 (uri, TRACKER_PREFIX_NRL "modified") == 0) {
		g_set_object (&priv->nrl_modified, field);
	}

	g_ptr_array_add (priv->properties, g_object_ref (field));
	tracker_property_set_ontologies (field, ontologies);

	g_hash_table_insert (priv->property_uris,
	                     g_strdup (uri),
	                     g_object_ref (field));
	g_hash_table_insert (priv->property_uris,
	                     g_strdup (tracker_property_get_name (field)),
	                     g_object_ref (field));
}

void
tracker_ontologies_add_id_uri_pair (TrackerOntologies *ontologies,
                                    TrackerRowid       id,
                                    const gchar       *uri)
{
	TrackerOntologiesPrivate *priv = tracker_ontologies_get_instance_private (ontologies);

	g_hash_table_insert (priv->id_uri_pairs,
	                     tracker_rowid_copy (&id),
	                     g_strdup (uri));
}

TrackerProperty *
tracker_ontologies_get_property_by_uri (TrackerOntologies *ontologies,
                                        const gchar       *uri)
{
	TrackerOntologiesPrivate *priv = tracker_ontologies_get_instance_private (ontologies);

	g_return_val_if_fail (uri != NULL, NULL);

	return g_hash_table_lookup (priv->property_uris, uri);
}

void
tracker_ontologies_add_namespace (TrackerOntologies *ontologies,
                                  TrackerNamespace  *namespace)
{
	TrackerOntologiesPrivate *priv = tracker_ontologies_get_instance_private (ontologies);
	const gchar *uri;

	g_return_if_fail (TRACKER_IS_NAMESPACE (namespace));

	uri = tracker_namespace_get_uri (namespace);

	g_ptr_array_add (priv->namespaces, g_object_ref (namespace));
	tracker_namespace_set_ontologies (namespace, ontologies);

	g_hash_table_insert (priv->namespace_uris,
	                     g_strdup (uri),
	                     g_object_ref (namespace));
}

void
tracker_ontologies_add_ontology (TrackerOntologies *ontologies,
                                 TrackerOntology   *ontology)
{
	TrackerOntologiesPrivate *priv = tracker_ontologies_get_instance_private (ontologies);
	const gchar *uri;

	g_return_if_fail (TRACKER_IS_ONTOLOGY (ontology));

	uri = tracker_ontology_get_uri (ontology);

	g_ptr_array_add (priv->ontologies, g_object_ref (ontology));
	tracker_ontology_set_ontologies (ontology, ontologies);

	g_hash_table_insert (priv->ontology_uris,
	                     g_strdup (uri),
	                     g_object_ref (ontology));
}

TrackerNamespace *
tracker_ontologies_get_namespace_by_uri (TrackerOntologies *ontologies,
                                         const gchar       *uri)
{
	TrackerOntologiesPrivate *priv = tracker_ontologies_get_instance_private (ontologies);

	g_return_val_if_fail (uri != NULL, NULL);

	return g_hash_table_lookup (priv->namespace_uris, uri);
}


TrackerOntology *
tracker_ontologies_get_ontology_by_uri (TrackerOntologies *ontologies,
                                        const gchar       *uri)
{
	TrackerOntologiesPrivate *priv = tracker_ontologies_get_instance_private (ontologies);

	g_return_val_if_fail (uri != NULL, NULL);

	return g_hash_table_lookup (priv->ontology_uris, uri);
}

TrackerOntologies *
tracker_ontologies_new (void)
{
	return g_object_new (TRACKER_TYPE_ONTOLOGIES, NULL);
}
