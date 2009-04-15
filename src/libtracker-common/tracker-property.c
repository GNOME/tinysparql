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

#include "tracker-property.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_PROPERTY, TrackerPropertyPriv))

typedef struct _TrackerPropertyPriv TrackerPropertyPriv;

struct _TrackerPropertyPriv {
	gchar	      *id;
	gchar	      *name;

	TrackerPropertyType  data_type;
	gchar	      *field_name;
	gint	       weight;
	gboolean       embedded;
	gboolean       multiple_values;
	gboolean       delimited;
	gboolean       filtered;
	gboolean       store_metadata;

	GSList	      *child_ids;
};

static void property_finalize     (GObject      *object);
static void property_get_property (GObject      *object,
				guint	      param_id,
				GValue	     *value,
				GParamSpec   *pspec);
static void property_set_property (GObject      *object,
				guint	      param_id,
				const GValue *value,
				GParamSpec   *pspec);

enum {
	PROP_0,
	PROP_ID,
	PROP_NAME,
	PROP_DATA_TYPE,
	PROP_FIELD_NAME,
	PROP_WEIGHT,
	PROP_EMBEDDED,
	PROP_MULTIPLE_VALUES,
	PROP_DELIMITED,
	PROP_FILTERED,
	PROP_STORE_METADATA,
	PROP_CHILD_IDS
};

GType
tracker_property_type_get_type (void)
{
	static GType etype = 0;

	if (etype == 0) {
		static const GEnumValue values[] = {
			{ TRACKER_PROPERTY_TYPE_KEYWORD,
			  "TRACKER_PROPERTY_TYPE_KEYWORD",
			  "keyword" },
			{ TRACKER_PROPERTY_TYPE_INDEX,
			  "TRACKER_PROPERTY_TYPE_INDEX",
			  "index" },
			{ TRACKER_PROPERTY_TYPE_FULLTEXT,
			  "TRACKER_PROPERTY_TYPE_FULLTEXT",
			  "fulltext" },
			{ TRACKER_PROPERTY_TYPE_STRING,
			  "TRACKER_PROPERTY_TYPE_STRING",
			  "string" },
			{ TRACKER_PROPERTY_TYPE_INTEGER,
			  "TRACKER_PROPERTY_TYPE_INTEGER",
			  "integer" },
			{ TRACKER_PROPERTY_TYPE_DOUBLE,
			  "TRACKER_PROPERTY_TYPE_DOUBLE",
			  "double" },
			{ TRACKER_PROPERTY_TYPE_DATE,
			  "TRACKER_PROPERTY_TYPE_DATE",
			  "date" },
			{ TRACKER_PROPERTY_TYPE_BLOB,
			  "TRACKER_PROPERTY_TYPE_BLOB",
			  "blob" },
			{ TRACKER_PROPERTY_TYPE_STRUCT,
			  "TRACKER_PROPERTY_TYPE_STRUCT",
			  "struct" },
			{ TRACKER_PROPERTY_TYPE_LINK,
			  "TRACKER_PROPERTY_TYPE_LINK",
			  "link" },
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
	GType	    type;
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

	object_class->finalize	   = property_finalize;
	object_class->get_property = property_get_property;
	object_class->set_property = property_set_property;

	g_object_class_install_property (object_class,
					 PROP_ID,
					 g_param_spec_string ("id",
							      "id",
							      "Unique identifier for this field",
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_NAME,
					 g_param_spec_string ("name",
							      "name",
							      "Field name",
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_DATA_TYPE,
					 g_param_spec_enum ("data-type",
							    "data-type",
							    "Field data type",
							    tracker_property_type_get_type (),
							    TRACKER_PROPERTY_TYPE_INDEX,
							    G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_FIELD_NAME,
					 g_param_spec_string ("field-name",
							      "field-name",
							      "Column in services table with the contents of this metadata",
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_WEIGHT,
					 g_param_spec_int ("weight",
							   "weight",
							   "Boost to the score",
							   0,
							   G_MAXINT,
							   0,
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
					 PROP_DELIMITED,
					 g_param_spec_boolean ("delimited",
							       "delimited",
							       "Delimited",
							       FALSE,
							       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_FILTERED,
					 g_param_spec_boolean ("filtered",
							       "filtered",
							       "Filtered",
							       FALSE,
							       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_STORE_METADATA,
					 g_param_spec_boolean ("store-metadata",
							       "store-metadata",
							       "Store metadata",
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_CHILD_IDS,
					 g_param_spec_pointer ("child-ids",
							       "child-ids",
							       "Child ids",
							       G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof (TrackerPropertyPriv));
}

static void
tracker_property_init (TrackerProperty *field)
{
}

static void
property_finalize (GObject *object)
{
	TrackerPropertyPriv *priv;

	priv = GET_PRIV (object);

	g_free (priv->id);
	g_free (priv->name);

	if (priv->field_name) {
		g_free (priv->field_name);
	}

	g_slist_foreach (priv->child_ids, (GFunc) g_free, NULL);
	g_slist_free (priv->child_ids);

	(G_OBJECT_CLASS (tracker_property_parent_class)->finalize) (object);
}

static void
property_get_property (GObject    *object,
		    guint	param_id,
		    GValue     *value,
		    GParamSpec *pspec)
{
	TrackerPropertyPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_ID:
		g_value_set_string (value, priv->id);
		break;
	case PROP_NAME:
		g_value_set_string (value, priv->name);
		break;
	case PROP_DATA_TYPE:
		g_value_set_enum (value, priv->data_type);
		break;
	case PROP_FIELD_NAME:
		g_value_set_string (value, priv->field_name);
		break;
	case PROP_WEIGHT:
		g_value_set_int (value, priv->weight);
		break;
	case PROP_EMBEDDED:
		g_value_set_boolean (value, priv->embedded);
		break;
	case PROP_MULTIPLE_VALUES:
		g_value_set_boolean (value, priv->multiple_values);
		break;
	case PROP_DELIMITED:
		g_value_set_boolean (value, priv->delimited);
		break;
	case PROP_FILTERED:
		g_value_set_boolean (value, priv->filtered);
		break;
	case PROP_STORE_METADATA:
		g_value_set_boolean (value, priv->store_metadata);
		break;
	case PROP_CHILD_IDS:
		g_value_set_pointer (value, priv->child_ids);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
property_set_property (GObject	 *object,
		    guint	  param_id,
		    const GValue *value,
		    GParamSpec	 *pspec)
{
	switch (param_id) {
	case PROP_ID:
		tracker_property_set_id (TRACKER_PROPERTY (object),
				      g_value_get_string (value));
		break;
	case PROP_NAME:
		tracker_property_set_name (TRACKER_PROPERTY (object),
					g_value_get_string (value));
		break;
	case PROP_DATA_TYPE:
		tracker_property_set_data_type (TRACKER_PROPERTY (object),
					     g_value_get_enum (value));
		break;
	case PROP_FIELD_NAME:
		tracker_property_set_field_name (TRACKER_PROPERTY (object),
					      g_value_get_string (value));
		break;
	case PROP_WEIGHT:
		tracker_property_set_weight (TRACKER_PROPERTY (object),
					  g_value_get_int (value));
		break;
	case PROP_EMBEDDED:
		tracker_property_set_embedded (TRACKER_PROPERTY (object),
					    g_value_get_boolean (value));
		break;
	case PROP_MULTIPLE_VALUES:
		tracker_property_set_multiple_values (TRACKER_PROPERTY (object),
						   g_value_get_boolean (value));
		break;
	case PROP_DELIMITED:
		tracker_property_set_delimited (TRACKER_PROPERTY (object),
					     g_value_get_boolean (value));
		break;
	case PROP_FILTERED:
		tracker_property_set_filtered (TRACKER_PROPERTY (object),
					    g_value_get_boolean (value));
		break;
	case PROP_STORE_METADATA:
		tracker_property_set_store_metadata (TRACKER_PROPERTY (object),
						  g_value_get_boolean (value));
		break;
	case PROP_CHILD_IDS:
		tracker_property_set_child_ids (TRACKER_PROPERTY (object),
					     g_value_get_pointer (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static gboolean
field_int_validate (TrackerProperty *field,
		    const gchar   *property,
		    gint	    value)
{
#ifdef G_DISABLE_CHECKS
	GParamSpec *spec;
	GValue	    value = { 0 };
	gboolean    valid;

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
tracker_property_get_id (TrackerProperty *field)
{
	TrackerPropertyPriv *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (field), NULL);

	priv = GET_PRIV (field);

	return priv->id;
}

const gchar *
tracker_property_get_name (TrackerProperty *field)
{
	TrackerPropertyPriv *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (field), NULL);

	priv = GET_PRIV (field);

	return priv->name;
}

TrackerPropertyType
tracker_property_get_data_type (TrackerProperty *field)
{
	TrackerPropertyPriv *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (field), TRACKER_PROPERTY_TYPE_STRING); //FIXME

	priv = GET_PRIV (field);

	return priv->data_type;
}

const gchar *
tracker_property_get_field_name (TrackerProperty *field)
{
	TrackerPropertyPriv *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (field), NULL);

	priv = GET_PRIV (field);

	return priv->field_name;
}

gint
tracker_property_get_weight (TrackerProperty *field)
{
	TrackerPropertyPriv *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (field), -1);

	priv = GET_PRIV (field);

	return priv->weight;
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
tracker_property_get_delimited (TrackerProperty *field)
{
	TrackerPropertyPriv *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (field), FALSE);

	priv = GET_PRIV (field);

	return priv->delimited;
}

gboolean
tracker_property_get_filtered (TrackerProperty *field)
{
	TrackerPropertyPriv *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (field), FALSE);

	priv = GET_PRIV (field);

	return priv->filtered;
}

gboolean
tracker_property_get_store_metadata (TrackerProperty *field)
{
	TrackerPropertyPriv *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (field), FALSE);

	priv = GET_PRIV (field);

	return priv->store_metadata;
}


const GSList *
tracker_property_get_child_ids (TrackerProperty *field)
{
	TrackerPropertyPriv *priv;

	g_return_val_if_fail (TRACKER_IS_PROPERTY (field), NULL);

	priv = GET_PRIV (field);

	return priv->child_ids;
}


void
tracker_property_set_id (TrackerProperty *field,
		      const gchar  *value)
{
	TrackerPropertyPriv *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (field));

	priv = GET_PRIV (field);

	g_free (priv->id);

	if (value) {
		priv->id = g_strdup (value);
	} else {
		priv->id = NULL;
	}

	g_object_notify (G_OBJECT (field), "id");
}

void
tracker_property_set_name (TrackerProperty *field,
			const gchar  *value)
{
	TrackerPropertyPriv *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (field));

	priv = GET_PRIV (field);

	g_free (priv->name);

	if (value) {
		priv->name = g_strdup (value);
	} else {
		priv->name = NULL;
	}

	g_object_notify (G_OBJECT (field), "name");
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
tracker_property_set_field_name (TrackerProperty *field,
			      const gchar    *value)
{
	TrackerPropertyPriv *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (field));

	priv = GET_PRIV (field);

	g_free (priv->field_name);

	if (value) {
		priv->field_name = g_strdup (value);
	} else {
		priv->field_name = NULL;
	}

	g_object_notify (G_OBJECT (field), "field-name");
}

void
tracker_property_set_weight (TrackerProperty *field,
			  gint		value)
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
tracker_property_set_embedded (TrackerProperty *field,
			    gboolean	  value)
{
	TrackerPropertyPriv *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (field));

	priv = GET_PRIV (field);

	priv->embedded = value;
	g_object_notify (G_OBJECT (field), "embedded");
}

void
tracker_property_set_multiple_values (TrackerProperty *field,
				   gboolean	 value)
{
	TrackerPropertyPriv *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (field));

	priv = GET_PRIV (field);

	priv->multiple_values = value;
	g_object_notify (G_OBJECT (field), "multiple-values");
}

void
tracker_property_set_delimited (TrackerProperty *field,
			     gboolean	   value)
{
	TrackerPropertyPriv *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (field));

	priv = GET_PRIV (field);

	priv->delimited = value;
	g_object_notify (G_OBJECT (field), "delimited");
}

void
tracker_property_set_filtered (TrackerProperty *field,
			    gboolean	  value)
{
	TrackerPropertyPriv *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (field));

	priv = GET_PRIV (field);

	priv->filtered = value;
	g_object_notify (G_OBJECT (field), "filtered");
}

void
tracker_property_set_store_metadata (TrackerProperty *field,
				  gboolean	value)
{
	TrackerPropertyPriv *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (field));

	priv = GET_PRIV (field);

	priv->store_metadata = value;
	g_object_notify (G_OBJECT (field), "store-metadata");
}

void
tracker_property_set_child_ids (TrackerProperty *field,
			     const GSList *value)
{
	TrackerPropertyPriv *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (field));

	priv = GET_PRIV (field);

	g_slist_foreach (priv->child_ids, (GFunc) g_free, NULL);
	g_slist_free (priv->child_ids);

	if (value) {
		GSList	     *new_list;
		const GSList *l;

		new_list = NULL;

		for (l = value; l; l = l->next) {
			new_list = g_slist_prepend (new_list, g_strdup (l->data));
		}

		new_list = g_slist_reverse (new_list);
		priv->child_ids = new_list;
	} else {
		priv->child_ids = NULL;
	}

	g_object_notify (G_OBJECT (field), "child-ids");
}

void
tracker_property_append_child_id (TrackerProperty *field,
			       const gchar  *value)
{
	TrackerPropertyPriv *priv;

	g_return_if_fail (TRACKER_IS_PROPERTY (field));

	priv = GET_PRIV (field);

	if (value) {
		priv->child_ids = g_slist_append (priv->child_ids, g_strdup (value));
	}

	g_object_notify (G_OBJECT (field), "child-ids");
}
