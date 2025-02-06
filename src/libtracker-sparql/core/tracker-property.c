/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
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

#include "core/tracker-namespace.h"
#include "core/tracker-ontologies.h"
#include "core/tracker-property.h"

#define XSD_BOOLEAN  TRACKER_PREFIX_XSD "boolean"
#define XSD_DATE     TRACKER_PREFIX_XSD "date"
#define XSD_DATETIME TRACKER_PREFIX_XSD "dateTime"
#define XSD_DOUBLE   TRACKER_PREFIX_XSD "double"
#define XSD_INTEGER  TRACKER_PREFIX_XSD "integer"
#define XSD_STRING   TRACKER_PREFIX_XSD "string"
#define RDF_LANGSTRING TRACKER_PREFIX_RDF "langString"

const gchar *tracker_property_types[] =
{
        [TRACKER_PROPERTY_TYPE_STRING] = XSD_STRING,
        [TRACKER_PROPERTY_TYPE_BOOLEAN] = XSD_BOOLEAN,
        [TRACKER_PROPERTY_TYPE_INTEGER] = XSD_INTEGER,
        [TRACKER_PROPERTY_TYPE_DOUBLE] = XSD_DOUBLE,
        [TRACKER_PROPERTY_TYPE_DATE] = XSD_DATE,
        [TRACKER_PROPERTY_TYPE_DATETIME] = XSD_DATETIME,
        [TRACKER_PROPERTY_TYPE_LANGSTRING] = RDF_LANGSTRING,
};

static TrackerPropertyType
tracker_uri_to_property_type(const gchar *uri)
{
        for (size_t i = 0; i < G_N_ELEMENTS(tracker_property_types); i++) {
                if (tracker_property_types[i] && (strcmp (uri, tracker_property_types[i]) == 0)) {
                        return i;
                }
        }
        return TRACKER_PROPERTY_TYPE_RESOURCE;
}

struct _TrackerPropertyPrivate {
	gchar         *uri;
	gchar         *name;
	gchar         *table_name;

	TrackerPropertyType  data_type;
	TrackerClass   *domain;
	TrackerClass   *domain_index;
	TrackerClass   *range;
	gint           weight;
	TrackerRowid   id;
	guint          indexed : 1;
	guint          fulltext_indexed : 1;
	guint          multiple_values : 1;
	guint          is_inverse_functional_property : 1;

	gchar         *ontology_path;
	goffset        definition_line_no;
	goffset        definition_column_no;

	TrackerProperty *secondary_index;

	GArray        *super_properties;
	GArray        *domain_indexes;

	TrackerOntologies *ontologies;
};

static void property_finalize     (GObject      *object);

G_DEFINE_TYPE_WITH_PRIVATE (TrackerProperty, tracker_property, G_TYPE_OBJECT)

static void
tracker_property_class_init (TrackerPropertyClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = property_finalize;
}

static void
tracker_property_init (TrackerProperty *property)
{
	TrackerPropertyPrivate *priv;

	priv = tracker_property_get_instance_private (property);

	priv->id = 0;
	priv->weight = 1;
	priv->multiple_values = TRUE;
	priv->super_properties = g_array_new (TRUE, TRUE, sizeof (TrackerProperty *));
	priv->domain_indexes = g_array_new (TRUE, TRUE, sizeof (TrackerClass *));
}

static void
property_finalize (GObject *object)
{
	TrackerPropertyPrivate *priv;

	priv = tracker_property_get_instance_private (TRACKER_PROPERTY (object));

	g_free (priv->uri);
	g_free (priv->name);
	g_free (priv->table_name);

	if (priv->domain) {
		g_object_unref (priv->domain);
	}

	if (priv->range) {
		g_object_unref (priv->range);
	}

	if (priv->ontology_path) {
		g_free (priv->ontology_path);
	}

	if (priv->secondary_index) {
		g_object_unref (priv->secondary_index);
	}

	g_array_free (priv->super_properties, TRUE);
	g_array_free (priv->domain_indexes, TRUE);

	(G_OBJECT_CLASS (tracker_property_parent_class)->finalize) (object);
}

/**
 * tracker_property_new:
 *
 * Creates a new #TrackerProperty instance.
 *
 * Returns: The newly created #TrackerProperty
 **/
TrackerProperty *
tracker_property_new (void)
{
	return g_object_new (TRACKER_TYPE_PROPERTY, NULL);
}

const gchar *
tracker_property_get_uri (TrackerProperty *property)
{
	TrackerPropertyPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (property), NULL);

	priv = tracker_property_get_instance_private (property);

	return priv->uri;
}

const gchar *
tracker_property_get_name (TrackerProperty *property)
{
	TrackerPropertyPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (property), NULL);

	priv = tracker_property_get_instance_private (property);

	return priv->name;
}

const gchar *
tracker_property_get_table_name (TrackerProperty *property)
{
	TrackerPropertyPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (property), NULL);

	priv = tracker_property_get_instance_private (property);

	if (!priv->table_name) {
		if (tracker_property_get_multiple_values (property)) {
			priv->table_name = g_strdup_printf ("%s_%s",
				tracker_class_get_name (tracker_property_get_domain (property)),
				tracker_property_get_name (property));
		} else {
			priv->table_name = g_strdup (tracker_class_get_name (tracker_property_get_domain (property)));
		}
	}

	return priv->table_name;
}

TrackerPropertyType
tracker_property_get_data_type (TrackerProperty *property)
{
	TrackerPropertyPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (property), TRACKER_PROPERTY_TYPE_STRING); //FIXME

	priv = tracker_property_get_instance_private (property);

	return priv->data_type;
}

TrackerClass *
tracker_property_get_domain (TrackerProperty *property)
{
	TrackerPropertyPrivate *priv;

	/* Removed for performance:
	 g_return_val_if_fail (TRACKER_IS_PROPERTY (property), NULL); */

	g_return_val_if_fail (property != NULL, NULL);

	priv = tracker_property_get_instance_private (property);

	return priv->domain;
}

TrackerClass **
tracker_property_get_domain_indexes (TrackerProperty *property)
{
	TrackerPropertyPrivate *priv;

	/* Removed for performance:
	 g_return_val_if_fail (TRACKER_IS_PROPERTY (property), NULL); */

	g_return_val_if_fail (property != NULL, NULL);

	priv = tracker_property_get_instance_private (property);

	return (TrackerClass ** ) priv->domain_indexes->data;
}

TrackerClass *
tracker_property_get_range (TrackerProperty *property)
{
	TrackerPropertyPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (property), NULL);

	priv = tracker_property_get_instance_private (property);

	return priv->range;
}

gint
tracker_property_get_weight (TrackerProperty *property)
{
	TrackerPropertyPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (property), -1);

	priv = tracker_property_get_instance_private (property);

	return priv->weight;
}

TrackerRowid
tracker_property_get_id (TrackerProperty *property)
{
	TrackerPropertyPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (property), 0);

	priv = tracker_property_get_instance_private (property);

	return priv->id;
}

gboolean
tracker_property_get_indexed (TrackerProperty *property)
{
	TrackerPropertyPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (property), FALSE);

	priv = tracker_property_get_instance_private (property);

	return priv->indexed;
}

TrackerProperty *
tracker_property_get_secondary_index (TrackerProperty *property)
{
	TrackerPropertyPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (property), NULL);

	priv = tracker_property_get_instance_private (property);

	return priv->secondary_index;
}

gboolean
tracker_property_get_fulltext_indexed (TrackerProperty *property)
{
	TrackerPropertyPrivate *priv;

	/* Removed for performance:
	 g_return_val_if_fail (TRACKER_IS_PROPERTY (property), NULL); */

	g_return_val_if_fail (property != NULL, FALSE);

	priv = tracker_property_get_instance_private (property);

	return priv->fulltext_indexed;
}

gboolean
tracker_property_get_multiple_values (TrackerProperty *property)
{
	TrackerPropertyPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (property), FALSE);

	priv = tracker_property_get_instance_private (property);

	return priv->multiple_values;
}

const gchar *
tracker_property_get_ontology_path (TrackerProperty *property)
{
	TrackerPropertyPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (property), FALSE);

	priv = tracker_property_get_instance_private (property);

	return priv->ontology_path;
}

goffset
tracker_property_get_definition_line_no (TrackerProperty *property)
{
	TrackerPropertyPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (property), FALSE);

	priv = tracker_property_get_instance_private (property);

	return priv->definition_line_no;
}

goffset
tracker_property_get_definition_column_no (TrackerProperty *property)
{
	TrackerPropertyPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (property), FALSE);

	priv = tracker_property_get_instance_private (property);

	return priv->definition_column_no;
}

gboolean
tracker_property_get_is_inverse_functional_property (TrackerProperty *property)
{
	TrackerPropertyPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (property), FALSE);

	priv = tracker_property_get_instance_private (property);

	return priv->is_inverse_functional_property;
}

TrackerProperty **
tracker_property_get_super_properties (TrackerProperty *property)
{
	TrackerPropertyPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (property), NULL);

	priv = tracker_property_get_instance_private (property);

	return (TrackerProperty **) priv->super_properties->data;
}

void
tracker_property_set_uri (TrackerProperty *property,
                          const gchar     *value)
{
	TrackerPropertyPrivate *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (property));

	priv = tracker_property_get_instance_private (property);

	g_free (priv->uri);
	g_free (priv->name);
	priv->uri = NULL;
	priv->name = NULL;

	if (value) {
		TrackerNamespace *namespace;
		gchar *namespace_uri, *hash;

		priv->uri = g_strdup (value);

		hash = strrchr (priv->uri, '#');
		if (hash == NULL) {
			/* support ontologies whose namespace uri does not end in a hash, e.g. dc */
			hash = strrchr (priv->uri, '/');
		}
		if (hash == NULL) {
			g_critical ("Unknown namespace of property %s", priv->uri);
		} else {
			namespace_uri = g_strndup (priv->uri, hash - priv->uri + 1);
			namespace = tracker_ontologies_get_namespace_by_uri (priv->ontologies, namespace_uri);
			if (namespace == NULL) {
				g_critical ("Unknown namespace %s of property %s", namespace_uri, priv->uri);
			} else {
				priv->name = g_strdup_printf ("%s:%s", tracker_namespace_get_prefix (namespace), hash + 1);
			}
			g_free (namespace_uri);
		}
	}
}

void
tracker_property_set_domain (TrackerProperty *property,
                             TrackerClass    *value)
{
	TrackerPropertyPrivate *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (property));

	priv = tracker_property_get_instance_private (property);

	if (priv->domain) {
		g_object_unref (priv->domain);
		priv->domain = NULL;
	}

	if (value) {
		priv->domain = g_object_ref (value);
	}
}

void
tracker_property_add_domain_index (TrackerProperty *property,
                                   TrackerClass    *value)
{
	TrackerPropertyPrivate *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (property));
	g_return_if_fail (TRACKER_IS_CLASS (value));

	priv = tracker_property_get_instance_private (property);

	g_array_append_val (priv->domain_indexes, value);
}

void
tracker_property_set_secondary_index (TrackerProperty *property,
                                      TrackerProperty *value)
{
	TrackerPropertyPrivate *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (property));

	priv = tracker_property_get_instance_private (property);

	if (priv->secondary_index) {
		g_object_unref (priv->secondary_index);
		priv->secondary_index = NULL;
	}

	if (value) {
		priv->secondary_index = g_object_ref (value);
	}
}

void
tracker_property_set_range (TrackerProperty *property,
                            TrackerClass     *value)
{
	TrackerPropertyPrivate *priv;
	const gchar *range_uri;

	g_return_if_fail (TRACKER_IS_PROPERTY (property));
	g_return_if_fail (TRACKER_IS_CLASS (value));

	priv = tracker_property_get_instance_private (property);

	if (priv->range) {
		g_object_unref (priv->range);
	}

	priv->range = g_object_ref (value);

	range_uri = tracker_class_get_uri (priv->range);
        priv->data_type = tracker_uri_to_property_type (range_uri);
}

void
tracker_property_set_weight (TrackerProperty *property,
                             gint             value)
{
	TrackerPropertyPrivate *priv;
	g_return_if_fail (TRACKER_IS_PROPERTY (property));

	priv = tracker_property_get_instance_private (property);

	priv->weight = value;
}


void
tracker_property_set_id (TrackerProperty *property,
                         TrackerRowid     value)
{
	TrackerPropertyPrivate *priv;
	g_return_if_fail (TRACKER_IS_PROPERTY (property));

	priv = tracker_property_get_instance_private (property);

	priv->id = value;
}

void
tracker_property_set_indexed (TrackerProperty *property,
                              gboolean         value)
{
	TrackerPropertyPrivate *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (property));

	priv = tracker_property_get_instance_private (property);

	priv->indexed = !!value;
}

void
tracker_property_set_fulltext_indexed (TrackerProperty *property,
                                       gboolean         value)
{
	TrackerPropertyPrivate *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (property));

	priv = tracker_property_get_instance_private (property);

	priv->fulltext_indexed = !!value;
}

void
tracker_property_set_multiple_values (TrackerProperty *property,
                                      gboolean         value)
{
	TrackerPropertyPrivate *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (property));

	priv = tracker_property_get_instance_private (property);

	priv->multiple_values = !!value;
	g_clear_pointer (&priv->table_name, g_free);
}

void
tracker_property_set_ontology_path (TrackerProperty *property,
                                    const gchar     *value)
{
	TrackerPropertyPrivate *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (property));

	priv = tracker_property_get_instance_private (property);

	if (priv->ontology_path)
		g_free (priv->ontology_path);

	priv->ontology_path = g_strdup (value);
}

void
tracker_property_set_definition_line_no (TrackerProperty *property,
                                         goffset          value)
{
	TrackerPropertyPrivate *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (property));

	priv = tracker_property_get_instance_private (property);

	priv->definition_line_no = value;
}

void
tracker_property_set_definition_column_no (TrackerProperty *property,
                                           goffset          value)
{
	TrackerPropertyPrivate *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (property));

	priv = tracker_property_get_instance_private (property);

	priv->definition_column_no = value;
}

void
tracker_property_set_is_inverse_functional_property (TrackerProperty *property,
                                                     gboolean         value)
{
	TrackerPropertyPrivate *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (property));

	priv = tracker_property_get_instance_private (property);

	priv->is_inverse_functional_property = !!value;
}

void
tracker_property_add_super_property (TrackerProperty *property,
                                     TrackerProperty *value)
{
	TrackerPropertyPrivate *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (property));
	g_return_if_fail (TRACKER_IS_PROPERTY (value));

	priv = tracker_property_get_instance_private (property);

	g_array_append_val (priv->super_properties, value);
}

void
tracker_property_set_ontologies (TrackerProperty   *property,
                                 TrackerOntologies *ontologies)
{
	TrackerPropertyPrivate *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (property));
	g_return_if_fail (ontologies != NULL);
	priv = tracker_property_get_instance_private (property);

	priv->ontologies = ontologies;
}
