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

#include <libtracker-common/tracker-ontologies.h>

#include "tracker-namespace.h"
#include "tracker-ontologies.h"
#include "tracker-property.h"

#define XSD_BOOLEAN  TRACKER_XSD_PREFIX "boolean"
#define XSD_DATE     TRACKER_XSD_PREFIX "date"
#define XSD_DATETIME TRACKER_XSD_PREFIX "dateTime"
#define XSD_DOUBLE   TRACKER_XSD_PREFIX "double"
#define XSD_INTEGER  TRACKER_XSD_PREFIX "integer"
#define XSD_STRING   TRACKER_XSD_PREFIX "string"

#define GET_PRIV(obj) (((TrackerProperty*) obj)->priv)
#define TRACKER_PROPERTY_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_PROPERTY, TrackerPropertyPrivate))

struct _TrackerPropertyPrivate {
	gchar         *uri;
	gchar         *name;
	gchar         *table_name;

	TrackerPropertyType  data_type;
	TrackerClass   *domain;
	TrackerClass   *domain_index;
	TrackerClass   *range;
	gint           weight;
	gint           id;
	gboolean       indexed;
	TrackerProperty *secondary_index;
	gboolean       fulltext_indexed;
	gboolean       fulltext_no_limit;
	gboolean       embedded;
	gboolean       multiple_values;
	gboolean       transient;
	gboolean       is_inverse_functional_property;
	gboolean       is_new;
	gboolean       db_schema_changed;
	gboolean       writeback;
	gchar         *default_value;
	gboolean       is_new_domain_index;

	GArray        *super_properties;
	GArray        *domain_indexes;
};

static void property_finalize     (GObject      *object);

GType
tracker_property_type_get_type (void)
{
	static GType etype = 0;

	if (etype == 0) {
		static const GEnumValue values[] = {
			{ TRACKER_PROPERTY_TYPE_UNKNOWN,
			  "TRACKER_PROPERTY_TYPE_UNKNOWN",
			  "unknown" },
			{ TRACKER_PROPERTY_TYPE_STRING,
			  "TRACKER_PROPERTY_TYPE_STRING",
			  "string" },
			{ TRACKER_PROPERTY_TYPE_BOOLEAN,
			  "TRACKER_PROPERTY_TYPE_BOOLEAN",
			  "boolean" },
			{ TRACKER_PROPERTY_TYPE_INTEGER,
			  "TRACKER_PROPERTY_TYPE_INTEGER",
			  "integer" },
			{ TRACKER_PROPERTY_TYPE_DOUBLE,
			  "TRACKER_PROPERTY_TYPE_DOUBLE",
			  "double" },
			{ TRACKER_PROPERTY_TYPE_DATE,
			  "TRACKER_PROPERTY_TYPE_DATE",
			  "date" },
			{ TRACKER_PROPERTY_TYPE_DATETIME,
			  "TRACKER_PROPERTY_TYPE_DATETIME",
			  "datetime" },
			{ TRACKER_PROPERTY_TYPE_RESOURCE,
			  "TRACKER_PROPERTY_TYPE_RESOURCE",
			  "resource" },
			{ 0, NULL, NULL }
		};

		etype = g_enum_register_static ("TrackerPropertyType", values);
	}

	return etype;
}

G_DEFINE_TYPE (TrackerProperty, tracker_property, G_TYPE_OBJECT);

static void
tracker_property_class_init (TrackerPropertyClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize     = property_finalize;

	g_type_class_add_private (object_class, sizeof (TrackerPropertyPrivate));
}

static void
tracker_property_init (TrackerProperty *property)
{
	TrackerPropertyPrivate *priv;

	priv = TRACKER_PROPERTY_GET_PRIVATE (property);

	priv->id = 0;
	priv->weight = 1;
	priv->embedded = TRUE;
	priv->transient = FALSE;
	priv->multiple_values = TRUE;
	priv->super_properties = g_array_new (TRUE, TRUE, sizeof (TrackerProperty *));
	priv->domain_indexes = g_array_new (TRUE, TRUE, sizeof (TrackerClass *));

	/* Make GET_PRIV working */
	property->priv = priv;
}

static void
property_finalize (GObject *object)
{
	TrackerPropertyPrivate *priv;

	priv = GET_PRIV (object);

	g_free (priv->uri);
	g_free (priv->name);
	g_free (priv->table_name);

	if (priv->domain) {
		g_object_unref (priv->domain);
	}

	if (priv->range) {
		g_object_unref (priv->range);
	}

	if (priv->secondary_index) {
		g_object_unref (priv->secondary_index);
	}

	g_array_free (priv->super_properties, TRUE);
	g_array_free (priv->domain_indexes, TRUE);

	g_free (priv->default_value);

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
	TrackerProperty *property;

	property = g_object_new (TRACKER_TYPE_PROPERTY, NULL);

	return property;
}

const gchar *
tracker_property_get_uri (TrackerProperty *property)
{
	TrackerPropertyPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (property), NULL);

	priv = GET_PRIV (property);

	return priv->uri;
}

gboolean
tracker_property_get_transient (TrackerProperty *property)
{
	TrackerPropertyPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (property), FALSE);

	priv = GET_PRIV (property);

	return priv->transient;
}


const gchar *
tracker_property_get_name (TrackerProperty *property)
{
	TrackerPropertyPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (property), NULL);

	priv = GET_PRIV (property);

	return priv->name;
}

const gchar *
tracker_property_get_table_name (TrackerProperty *property)
{
	TrackerPropertyPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (property), NULL);

	priv = GET_PRIV (property);

	if (!priv->table_name) {
		if (priv->multiple_values) {
			priv->table_name = g_strdup_printf ("%s_%s",
				tracker_class_get_name (priv->domain),
				priv->name);
		} else {
			priv->table_name = g_strdup (tracker_class_get_name (priv->domain));
		}
	}

	return priv->table_name;
}

TrackerPropertyType
tracker_property_get_data_type (TrackerProperty *property)
{
	TrackerPropertyPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (property), TRACKER_PROPERTY_TYPE_STRING); //FIXME

	priv = GET_PRIV (property);

	return priv->data_type;
}

TrackerClass *
tracker_property_get_domain (TrackerProperty *property)
{
	TrackerPropertyPrivate *priv;

	/* Removed for performance:
	 g_return_val_if_fail (TRACKER_IS_PROPERTY (property), NULL); */

	g_return_val_if_fail (property != NULL, NULL);

	priv = GET_PRIV (property);

	return priv->domain;
}

TrackerClass **
tracker_property_get_domain_indexes (TrackerProperty *property)
{
	TrackerPropertyPrivate *priv;

	/* Removed for performance:
	 g_return_val_if_fail (TRACKER_IS_PROPERTY (property), NULL); */

	g_return_val_if_fail (property != NULL, NULL);

	priv = GET_PRIV (property);

	return priv->domain_indexes->data;
}

TrackerClass *
tracker_property_get_range (TrackerProperty *property)
{
	TrackerPropertyPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (property), NULL);

	priv = GET_PRIV (property);

	return priv->range;
}

gint
tracker_property_get_weight (TrackerProperty *property)
{
	TrackerPropertyPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (property), -1);

	priv = GET_PRIV (property);

	return priv->weight;
}

gint
tracker_property_get_id (TrackerProperty *property)
{
	TrackerPropertyPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (property), 0);

	priv = GET_PRIV (property);

	return priv->id;
}

gboolean
tracker_property_get_indexed (TrackerProperty *property)
{
	TrackerPropertyPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (property), FALSE);

	priv = GET_PRIV (property);

	return priv->indexed;
}

TrackerProperty *
tracker_property_get_secondary_index (TrackerProperty *property)
{
	TrackerPropertyPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (property), NULL);

	priv = GET_PRIV (property);

	return priv->secondary_index;
}

gboolean
tracker_property_get_fulltext_indexed (TrackerProperty *property)
{
	TrackerPropertyPrivate *priv;

	/* Removed for performance:
	 g_return_val_if_fail (TRACKER_IS_PROPERTY (property), NULL); */

	g_return_val_if_fail (property != NULL, FALSE);

	priv = GET_PRIV (property);

	return priv->fulltext_indexed;
}

gboolean
tracker_property_get_fulltext_no_limit (TrackerProperty *property)
{
	TrackerPropertyPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (property), FALSE);

	priv = GET_PRIV (property);

	return priv->fulltext_no_limit;
}

gboolean
tracker_property_get_is_new (TrackerProperty *property)
{
	TrackerPropertyPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (property), FALSE);

	priv = GET_PRIV (property);

	return priv->is_new;
}

gboolean
tracker_property_get_is_new_domain_index (TrackerProperty *property)
{
	TrackerPropertyPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (property), FALSE);

	priv = GET_PRIV (property);

	return priv->is_new_domain_index;
}

gboolean
tracker_property_get_writeback (TrackerProperty *property)
{
	TrackerPropertyPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (property), FALSE);

	priv = GET_PRIV (property);

	return priv->writeback;
}

gboolean
tracker_property_get_db_schema_changed (TrackerProperty *property)
{
	TrackerPropertyPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (property), FALSE);

	priv = GET_PRIV (property);

	return priv->db_schema_changed;
}

gboolean
tracker_property_get_embedded (TrackerProperty *property)
{
	TrackerPropertyPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (property), FALSE);

	priv = GET_PRIV (property);

	return priv->embedded;
}

gboolean
tracker_property_get_multiple_values (TrackerProperty *property)
{
	TrackerPropertyPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (property), FALSE);

	priv = GET_PRIV (property);

	return priv->multiple_values;
}

gboolean
tracker_property_get_is_inverse_functional_property (TrackerProperty *property)
{
	TrackerPropertyPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (property), FALSE);

	priv = GET_PRIV (property);

	return priv->is_inverse_functional_property;
}

TrackerProperty **
tracker_property_get_super_properties (TrackerProperty *property)
{
	TrackerPropertyPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (property), NULL);

	priv = GET_PRIV (property);

	return (TrackerProperty **) priv->super_properties->data;
}

const gchar *
tracker_property_get_default_value (TrackerProperty *property)
{
	TrackerPropertyPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (property), NULL);

	priv = GET_PRIV (property);

	return priv->default_value;
}

void
tracker_property_set_uri (TrackerProperty *property,
                          const gchar     *value)
{
	TrackerPropertyPrivate *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (property));

	priv = GET_PRIV (property);

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
			namespace = tracker_ontologies_get_namespace_by_uri (namespace_uri);
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
tracker_property_set_transient (TrackerProperty *property,
                                gboolean         value)
{
	TrackerPropertyPrivate *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (property));

	priv = GET_PRIV (property);

	priv->transient = value;
	priv->multiple_values = TRUE;
}

void
tracker_property_set_domain (TrackerProperty *property,
                             TrackerClass    *value)
{
	TrackerPropertyPrivate *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (property));

	priv = GET_PRIV (property);

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

	priv = GET_PRIV (property);

	g_array_append_val (priv->domain_indexes, value);
}

void
tracker_property_set_secondary_index (TrackerProperty *property,
                                      TrackerProperty *value)
{
	TrackerPropertyPrivate *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (property));

	priv = GET_PRIV (property);

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

	priv = GET_PRIV (property);

	if (priv->range) {
		g_object_unref (priv->range);
	}

	priv->range = g_object_ref (value);

	range_uri = tracker_class_get_uri (priv->range);
	if (strcmp (range_uri, XSD_STRING) == 0) {
		priv->data_type = TRACKER_PROPERTY_TYPE_STRING;
	} else if (strcmp (range_uri, XSD_BOOLEAN) == 0) {
		priv->data_type = TRACKER_PROPERTY_TYPE_BOOLEAN;
	} else if (strcmp (range_uri, XSD_INTEGER) == 0) {
		priv->data_type = TRACKER_PROPERTY_TYPE_INTEGER;
	} else if (strcmp (range_uri, XSD_DOUBLE) == 0) {
		priv->data_type = TRACKER_PROPERTY_TYPE_DOUBLE;
	} else if (strcmp (range_uri, XSD_DATE) == 0) {
		priv->data_type = TRACKER_PROPERTY_TYPE_DATE;
	} else if (strcmp (range_uri, XSD_DATETIME) == 0) {
		priv->data_type = TRACKER_PROPERTY_TYPE_DATETIME;
	} else {
		priv->data_type = TRACKER_PROPERTY_TYPE_RESOURCE;
	}
}

void
tracker_property_set_weight (TrackerProperty *property,
                             gint             value)
{
	TrackerPropertyPrivate *priv;
	g_return_if_fail (TRACKER_IS_PROPERTY (property));

	priv = GET_PRIV (property);

	priv->weight = value;
}


void
tracker_property_set_id (TrackerProperty *property,
                         gint             value)
{
	TrackerPropertyPrivate *priv;
	g_return_if_fail (TRACKER_IS_PROPERTY (property));

	priv = GET_PRIV (property);

	priv->id = value;
}

void
tracker_property_set_indexed (TrackerProperty *property,
                              gboolean         value)
{
	TrackerPropertyPrivate *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (property));

	priv = GET_PRIV (property);

	priv->indexed = value;
}

void
tracker_property_set_is_new (TrackerProperty *property,
                             gboolean         value)
{
	TrackerPropertyPrivate *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (property));

	priv = GET_PRIV (property);

	priv->is_new = value;
}

void
tracker_property_set_is_new_domain_index (TrackerProperty *property,
                                          gboolean         value)
{
	TrackerPropertyPrivate *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (property));

	priv = GET_PRIV (property);

	priv->is_new_domain_index = value;
}

void
tracker_property_set_writeback (TrackerProperty *property,
                                gboolean         value)
{
	TrackerPropertyPrivate *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (property));

	priv = GET_PRIV (property);

	priv->writeback = value;
}

void
tracker_property_set_db_schema_changed (TrackerProperty *property,
                                        gboolean         value)
{
	TrackerPropertyPrivate *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (property));

	priv = GET_PRIV (property);

	priv->db_schema_changed = value;
}

void
tracker_property_set_fulltext_indexed (TrackerProperty *property,
                                       gboolean         value)
{
	TrackerPropertyPrivate *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (property));

	priv = GET_PRIV (property);

	priv->fulltext_indexed = value;
}

void
tracker_property_set_fulltext_no_limit (TrackerProperty *property,
                                       gboolean          value)
{
	TrackerPropertyPrivate *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (property));

	priv = GET_PRIV (property);

	priv->fulltext_no_limit = value;
}

void
tracker_property_set_embedded (TrackerProperty *property,
                               gboolean         value)
{
	TrackerPropertyPrivate *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (property));

	priv = GET_PRIV (property);

	priv->embedded = value;
}

void
tracker_property_set_multiple_values (TrackerProperty *property,
                                      gboolean         value)
{
	TrackerPropertyPrivate *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (property));

	priv = GET_PRIV (property);

	if (priv->transient) {
		priv->multiple_values = TRUE;
	} else {
		priv->multiple_values = value;
	}
}

void
tracker_property_set_is_inverse_functional_property (TrackerProperty *property,
                                                     gboolean         value)
{
	TrackerPropertyPrivate *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (property));

	priv = GET_PRIV (property);

	priv->is_inverse_functional_property = value;
}

void
tracker_property_add_super_property (TrackerProperty *property,
                                     TrackerProperty *value)
{
	TrackerPropertyPrivate *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (property));
	g_return_if_fail (TRACKER_IS_PROPERTY (value));

	priv = GET_PRIV (property);

	g_array_append_val (priv->super_properties, value);
}

void
tracker_property_set_default_value (TrackerProperty *property,
                                    const gchar     *value)
{
	TrackerPropertyPrivate *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (property));

	priv = GET_PRIV (property);

	g_free (priv->default_value);
	priv->default_value = g_strdup (value);
}
