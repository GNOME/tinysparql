/*
 * Copyright (C) 2009, Nokia (urho.konttori@nokia.com)
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

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_PROPERTY, TrackerPropertyPriv))

typedef struct _TrackerPropertyPriv TrackerPropertyPriv;

struct _TrackerPropertyPriv {
	gchar         *uri;
	gchar         *name;
	gchar         *table_name;

	TrackerPropertyType  data_type;
	TrackerClass   *domain;
	TrackerClass   *range;
	gint           weight;
	gint           id;
	gboolean       indexed;
	gboolean       fulltext_indexed;
	gboolean       fulltext_no_limit;
	gboolean       embedded;
	gboolean       multiple_values;
	gboolean       transient;
	gboolean       is_inverse_functional_property;
	gboolean       is_new;

	GArray        *super_properties;
};

static void property_finalize     (GObject      *object);
static void property_get_property (GObject      *object,
                                   guint         param_id,
                                   GValue       *value,
                                   GParamSpec   *pspec);
static void property_set_property (GObject      *object,
                                   guint         param_id,
                                   const GValue *value,
                                   GParamSpec   *pspec);

enum {
	PROP_0,
	PROP_URI,
	PROP_NAME,
	PROP_TABLE_NAME,
	PROP_DATA_TYPE,
	PROP_DOMAIN,
	PROP_RANGE,
	PROP_WEIGHT,
	PROP_INDEXED,
	PROP_FULLTEXT_INDEXED,
	PROP_FULLTEXT_NO_LIMIT,
	PROP_EMBEDDED,
	PROP_MULTIPLE_VALUES,
	PROP_TRANSIENT,
	PROP_IS_INVERSE_FUNCTIONAL_PROPERTY,
	PROP_ID,
	PROP_IS_NEW
};

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

const gchar *
tracker_property_type_to_string (TrackerPropertyType fieldtype)
{
	GType type;
	GEnumClass *enum_class;
	GEnumValue *enum_value;

	type = tracker_property_type_get_type ();
	enum_class = G_ENUM_CLASS (g_type_class_peek (type));
	enum_value = g_enum_get_value (enum_class, fieldtype);

	if (!enum_value) {
		return NULL;
	}

	return enum_value->value_nick;
}

static void
tracker_property_class_init (TrackerPropertyClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize     = property_finalize;
	object_class->get_property = property_get_property;
	object_class->set_property = property_set_property;

	g_object_class_install_property (object_class,
	                                 PROP_URI,
	                                 g_param_spec_string ("uri",
	                                                      "uri",
	                                                      "URI",
	                                                      NULL,
	                                                      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_NAME,
	                                 g_param_spec_string ("name",
	                                                      "name",
	                                                      "Field name",
	                                                      NULL,
	                                                      G_PARAM_READABLE));
	g_object_class_install_property (object_class,
	                                 PROP_TABLE_NAME,
	                                 g_param_spec_string ("table-name",
	                                                      "table-name",
	                                                      "Table name",
	                                                      NULL,
	                                                      G_PARAM_READABLE));
	g_object_class_install_property (object_class,
	                                 PROP_DATA_TYPE,
	                                 g_param_spec_enum ("data-type",
	                                                    "data-type",
	                                                    "Field data type",
	                                                    tracker_property_type_get_type (),
	                                                    TRACKER_PROPERTY_TYPE_STRING,
	                                                    G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_DOMAIN,
	                                 g_param_spec_object ("domain",
	                                                      "domain",
	                                                      "Domain of this property",
	                                                      TRACKER_TYPE_CLASS,
	                                                      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_RANGE,
	                                 g_param_spec_object ("range",
	                                                      "range",
	                                                      "Range of this property",
	                                                      TRACKER_TYPE_CLASS,
	                                                      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_WEIGHT,
	                                 g_param_spec_int ("weight",
	                                                   "weight",
	                                                   "Boost to the score",
	                                                   0,
	                                                   G_MAXINT,
	                                                   1,
	                                                   G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_ID,
	                                 g_param_spec_int ("id",
	                                                   "id",
	                                                   "Id",
	                                                   0,
	                                                   G_MAXINT,
	                                                   0,
	                                                   G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_INDEXED,
	                                 g_param_spec_boolean ("indexed",
	                                                       "indexed",
	                                                       "Indexed",
	                                                       TRUE,
	                                                       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_FULLTEXT_INDEXED,
	                                 g_param_spec_boolean ("fulltext-indexed",
	                                                       "fulltext-indexed",
	                                                       "Full-text indexed",
	                                                       TRUE,
	                                                       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_FULLTEXT_NO_LIMIT,
	                                 g_param_spec_boolean ("fulltext-no-limit",
	                                                       "fulltext-no-limit",
	                                                       "Full-text indexing without word length limits",
	                                                       TRUE,
	                                                       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_EMBEDDED,
	                                 g_param_spec_boolean ("embedded",
	                                                       "embedded",
	                                                       "Embedded",
	                                                       TRUE,
	                                                       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_MULTIPLE_VALUES,
	                                 g_param_spec_boolean ("multiple-values",
	                                                       "multiple-values",
	                                                       "Multiple values",
	                                                       TRUE,
	                                                       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_TRANSIENT,
	                                 g_param_spec_boolean ("transient",
	                                                       "transient",
	                                                       "Transient",
	                                                       FALSE,
	                                                       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_IS_INVERSE_FUNCTIONAL_PROPERTY,
	                                 g_param_spec_boolean ("is-inverse-functional-property",
	                                                       "is-inverse-functional-property",
	                                                       "Is inverse functional property",
	                                                       FALSE,
	                                                       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_IS_NEW,
	                                 g_param_spec_boolean ("is-new",
	                                                       "is-new",
	                                                       "Set to TRUE when a new class or property is to be added to the database ontology",
	                                                       FALSE,
	                                                       G_PARAM_READWRITE));


	g_type_class_add_private (object_class, sizeof (TrackerPropertyPriv));
}

static void
tracker_property_init (TrackerProperty *field)
{
	TrackerPropertyPriv *priv;

	priv = GET_PRIV (field);

	priv->id = 0;
	priv->weight = 1;
	priv->embedded = TRUE;
	priv->transient = FALSE;
	priv->multiple_values = TRUE;
	priv->super_properties = g_array_new (TRUE, TRUE, sizeof (TrackerProperty *));
}

static void
property_finalize (GObject *object)
{
	TrackerPropertyPriv *priv;

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

	g_array_free (priv->super_properties, TRUE);

	(G_OBJECT_CLASS (tracker_property_parent_class)->finalize) (object);
}

static void
property_get_property (GObject    *object,
                       guint       param_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
	TrackerPropertyPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_URI:
		g_value_set_string (value, priv->uri);
		break;
	case PROP_NAME:
		g_value_set_string (value, priv->name);
		break;
	case PROP_TABLE_NAME:
		g_value_set_string (value, tracker_property_get_table_name ((TrackerProperty *) object));
		break;
	case PROP_DATA_TYPE:
		g_value_set_enum (value, priv->data_type);
		break;
	case PROP_DOMAIN:
		g_value_set_object (value, priv->domain);
		break;
	case PROP_RANGE:
		g_value_set_object (value, priv->range);
		break;
	case PROP_WEIGHT:
		g_value_set_int (value, priv->weight);
		break;
	case PROP_ID:
		g_value_set_int (value, priv->id);
		break;
	case PROP_INDEXED:
		g_value_set_boolean (value, priv->indexed);
		break;
	case PROP_FULLTEXT_INDEXED:
		g_value_set_boolean (value, priv->fulltext_indexed);
		break;
	case PROP_FULLTEXT_NO_LIMIT:
		g_value_set_boolean (value, priv->fulltext_no_limit);
		break;
	case PROP_IS_NEW:
		g_value_set_boolean (value, priv->is_new);
		break;
	case PROP_EMBEDDED:
		g_value_set_boolean (value, priv->embedded);
		break;
	case PROP_MULTIPLE_VALUES:
		g_value_set_boolean (value, priv->multiple_values);
		break;
	case PROP_TRANSIENT:
		g_value_set_boolean (value, priv->transient);
		break;
	case PROP_IS_INVERSE_FUNCTIONAL_PROPERTY:
		g_value_set_boolean (value, priv->is_inverse_functional_property);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
property_set_property (GObject      *object,
                       guint         param_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
	switch (param_id) {
	case PROP_URI:
		tracker_property_set_uri (TRACKER_PROPERTY (object),
		                          g_value_get_string (value));
		break;
	case PROP_DATA_TYPE:
		tracker_property_set_data_type (TRACKER_PROPERTY (object),
		                                g_value_get_enum (value));
		break;
	case PROP_DOMAIN:
		tracker_property_set_domain (TRACKER_PROPERTY (object),
		                             g_value_get_object (value));
		break;
	case PROP_RANGE:
		tracker_property_set_range (TRACKER_PROPERTY (object),
		                            g_value_get_object (value));
		break;
	case PROP_WEIGHT:
		tracker_property_set_weight (TRACKER_PROPERTY (object),
		                             g_value_get_int (value));
		break;
	case PROP_ID:
		tracker_property_set_id (TRACKER_PROPERTY (object),
		                         g_value_get_int (value));
		break;
	case PROP_INDEXED:
		tracker_property_set_indexed (TRACKER_PROPERTY (object),
		                              g_value_get_boolean (value));
		break;
	case PROP_IS_NEW:
		tracker_property_set_is_new (TRACKER_PROPERTY (object),
		                             g_value_get_boolean (value));
		break;
	case PROP_FULLTEXT_INDEXED:
		tracker_property_set_fulltext_indexed (TRACKER_PROPERTY (object),
		                                       g_value_get_boolean (value));
		break;
	case PROP_FULLTEXT_NO_LIMIT:
		tracker_property_set_fulltext_no_limit (TRACKER_PROPERTY (object),
							g_value_get_boolean (value));
		break;
	case PROP_EMBEDDED:
		tracker_property_set_embedded (TRACKER_PROPERTY (object),
		                               g_value_get_boolean (value));
		break;
	case PROP_MULTIPLE_VALUES:
		tracker_property_set_multiple_values (TRACKER_PROPERTY (object),
		                                      g_value_get_boolean (value));
		break;
	case PROP_TRANSIENT:
		tracker_property_set_transient (TRACKER_PROPERTY (object),
		                                g_value_get_boolean (value));
		break;
	case PROP_IS_INVERSE_FUNCTIONAL_PROPERTY:
		tracker_property_set_is_inverse_functional_property (TRACKER_PROPERTY (object),
		                                                     g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static gboolean
field_int_validate (TrackerProperty *field,
                    const gchar     *property,
                    gint             value)
{
#ifdef G_DISABLE_CHECKS
	GParamSpec *spec;
	GValue value = { 0 };
	gboolean valid;

	spec = g_object_class_find_property (G_OBJECT_CLASS (field), property);
	g_return_val_if_fail (spec != NULL, FALSE);

	g_value_init (&value, spec->value_type);
	g_value_set_int (&value, verbosity);
	valid = g_param_value_validate (spec, &value);
	g_value_unset (&value);

	g_return_val_if_fail (valid != TRUE, FALSE);
#endif

	return TRUE;
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
	TrackerProperty *field;

	field = g_object_new (TRACKER_TYPE_PROPERTY, NULL);

	return field;
}

const gchar *
tracker_property_get_uri (TrackerProperty *field)
{
	TrackerPropertyPriv *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (field), NULL);

	priv = GET_PRIV (field);

	return priv->uri;
}

gboolean
tracker_property_get_transient (TrackerProperty *field)
{
	TrackerPropertyPriv *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (field), FALSE);

	priv = GET_PRIV (field);

	return priv->transient;
}


const gchar *
tracker_property_get_name (TrackerProperty *field)
{
	TrackerPropertyPriv *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (field), NULL);

	priv = GET_PRIV (field);

	return priv->name;
}

const gchar *
tracker_property_get_table_name (TrackerProperty *field)
{
	TrackerPropertyPriv *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (field), NULL);

	priv = GET_PRIV (field);

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
tracker_property_get_data_type (TrackerProperty *field)
{
	TrackerPropertyPriv *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (field), TRACKER_PROPERTY_TYPE_STRING); //FIXME

	priv = GET_PRIV (field);

	return priv->data_type;
}

TrackerClass *
tracker_property_get_domain (TrackerProperty *field)
{
	TrackerPropertyPriv *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (field), NULL);

	priv = GET_PRIV (field);

	return priv->domain;
}

TrackerClass *
tracker_property_get_range (TrackerProperty *field)
{
	TrackerPropertyPriv *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (field), NULL);

	priv = GET_PRIV (field);

	return priv->range;
}

gint
tracker_property_get_weight (TrackerProperty *field)
{
	TrackerPropertyPriv *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (field), -1);

	priv = GET_PRIV (field);

	return priv->weight;
}

gint
tracker_property_get_id (TrackerProperty *field)
{
	TrackerPropertyPriv *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (field), 0);

	priv = GET_PRIV (field);

	return priv->id;
}

gboolean
tracker_property_get_indexed (TrackerProperty *field)
{
	TrackerPropertyPriv *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (field), FALSE);

	priv = GET_PRIV (field);

	return priv->indexed;
}

gboolean
tracker_property_get_fulltext_indexed (TrackerProperty *field)
{
	TrackerPropertyPriv *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (field), FALSE);

	priv = GET_PRIV (field);

	return priv->fulltext_indexed;
}

gboolean
tracker_property_get_fulltext_no_limit (TrackerProperty *field)
{
	TrackerPropertyPriv *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (field), FALSE);

	priv = GET_PRIV (field);

	return priv->fulltext_no_limit;
}

gboolean
tracker_property_get_is_new (TrackerProperty *field)
{
	TrackerPropertyPriv *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (field), FALSE);

	priv = GET_PRIV (field);

	return priv->is_new;
}

gboolean
tracker_property_get_embedded (TrackerProperty *field)
{
	TrackerPropertyPriv *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (field), FALSE);

	priv = GET_PRIV (field);

	return priv->embedded;
}

gboolean
tracker_property_get_multiple_values (TrackerProperty *field)
{
	TrackerPropertyPriv *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (field), FALSE);

	priv = GET_PRIV (field);

	return priv->multiple_values;
}

gboolean
tracker_property_get_is_inverse_functional_property (TrackerProperty *property)
{
	TrackerPropertyPriv *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (property), FALSE);

	priv = GET_PRIV (property);

	return priv->is_inverse_functional_property;
}

TrackerProperty **
tracker_property_get_super_properties (TrackerProperty *property)
{
	TrackerPropertyPriv *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (property), NULL);

	priv = GET_PRIV (property);

	return (TrackerProperty **) priv->super_properties->data;
}

void
tracker_property_set_uri (TrackerProperty *field,
                          const gchar     *value)
{
	TrackerPropertyPriv *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (field));

	priv = GET_PRIV (field);

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

	g_object_notify (G_OBJECT (field), "uri");
}

void
tracker_property_set_transient (TrackerProperty *field,
                                gboolean         value)
{
	TrackerPropertyPriv *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (field));

	priv = GET_PRIV (field);

	priv->transient = value;
	priv->multiple_values = TRUE;

	g_object_notify (G_OBJECT (field), "transient");
}

void
tracker_property_set_data_type (TrackerProperty     *field,
                                TrackerPropertyType  value)
{
	TrackerPropertyPriv *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (field));

	priv = GET_PRIV (field);

	priv->data_type = value;
	g_object_notify (G_OBJECT (field), "data-type");
}

void
tracker_property_set_domain (TrackerProperty *field,
                             TrackerClass    *value)
{
	TrackerPropertyPriv *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (field));

	priv = GET_PRIV (field);

	if (priv->domain) {
		g_object_unref (priv->domain);
		priv->domain = NULL;
	}

	if (value) {
		priv->domain = g_object_ref (value);
	}

	g_object_notify (G_OBJECT (field), "domain");
}

void
tracker_property_set_range (TrackerProperty *property,
                            TrackerClass     *value)
{
	TrackerPropertyPriv *priv;
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

	g_object_notify (G_OBJECT (property), "range");
}

void
tracker_property_set_weight (TrackerProperty *field,
                             gint             value)
{
	TrackerPropertyPriv *priv;
	g_return_if_fail (TRACKER_IS_PROPERTY (field));

	if (!field_int_validate (field, "weight", value)) {
		return;
	}

	priv = GET_PRIV (field);

	priv->weight = value;
	g_object_notify (G_OBJECT (field), "weight");
}


void
tracker_property_set_id (TrackerProperty *field,
                         gint             value)
{
	TrackerPropertyPriv *priv;
	g_return_if_fail (TRACKER_IS_PROPERTY (field));

	priv = GET_PRIV (field);

	priv->id = value;
}

void
tracker_property_set_indexed (TrackerProperty *field,
                              gboolean         value)
{
	TrackerPropertyPriv *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (field));

	priv = GET_PRIV (field);

	priv->indexed = value;
	g_object_notify (G_OBJECT (field), "indexed");
}

void
tracker_property_set_is_new (TrackerProperty *field,
                             gboolean         value)
{
	TrackerPropertyPriv *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (field));

	priv = GET_PRIV (field);

	priv->is_new = value;
	g_object_notify (G_OBJECT (field), "is-new");
}

void
tracker_property_set_fulltext_indexed (TrackerProperty *field,
                                       gboolean                 value)
{
	TrackerPropertyPriv *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (field));

	priv = GET_PRIV (field);

	priv->fulltext_indexed = value;
	g_object_notify (G_OBJECT (field), "fulltext-indexed");
}

void
tracker_property_set_fulltext_no_limit (TrackerProperty *field,
                                       gboolean                 value)
{
	TrackerPropertyPriv *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (field));

	priv = GET_PRIV (field);

	priv->fulltext_no_limit = value;
	g_object_notify (G_OBJECT (field), "fulltext-no-limit");
}

void
tracker_property_set_embedded (TrackerProperty *field,
                               gboolean                 value)
{
	TrackerPropertyPriv *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (field));

	priv = GET_PRIV (field);

	priv->embedded = value;
	g_object_notify (G_OBJECT (field), "embedded");
}

void
tracker_property_set_multiple_values (TrackerProperty *field,
                                      gboolean         value)
{
	TrackerPropertyPriv *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (field));

	priv = GET_PRIV (field);

	if (priv->transient) {
		priv->multiple_values = TRUE;
	} else {
		priv->multiple_values = value;
	}

	g_object_notify (G_OBJECT (field), "multiple-values");
}

void
tracker_property_set_is_inverse_functional_property (TrackerProperty *property,
                                                     gboolean         value)
{
	TrackerPropertyPriv *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (property));

	priv = GET_PRIV (property);

	priv->is_inverse_functional_property = value;
	g_object_notify (G_OBJECT (property), "is-inverse-functional-property");
}

void
tracker_property_set_super_properties (TrackerProperty  *property,
                                       TrackerProperty **value)
{
	TrackerPropertyPriv *priv;
	TrackerProperty **super_property;

	g_return_if_fail (TRACKER_IS_PROPERTY (property));

	priv = GET_PRIV (property);

	g_array_free (priv->super_properties, TRUE);

	priv->super_properties = g_array_new (TRUE, TRUE, sizeof (TrackerProperty *));
	for (super_property = value; *super_property; super_property++) {
		g_array_append_val (priv->super_properties, *super_property);
	}
}

void
tracker_property_add_super_property (TrackerProperty *property,
                                     TrackerProperty *value)
{
	TrackerPropertyPriv *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (property));
	g_return_if_fail (TRACKER_IS_PROPERTY (value));

	priv = GET_PRIV (property);

	g_array_append_val (priv->super_properties, value);
}

