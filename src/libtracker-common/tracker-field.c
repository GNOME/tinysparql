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

#include "tracker-field.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_FIELD, TrackerFieldPriv))

typedef struct _TrackerFieldPriv TrackerFieldPriv;

struct _TrackerFieldPriv {
	gchar	      *id;
	gchar	      *name;

	TrackerFieldType  data_type;
	gchar	      *field_name;
	gint	       weight;
	gboolean       embedded;
	gboolean       multiple_values;
	gboolean       delimited;
	gboolean       filtered;
	gboolean       store_metadata;

	GSList	      *child_ids;
};

static void field_finalize     (GObject      *object);
static void field_get_property (GObject      *object,
				guint	      param_id,
				GValue	     *value,
				GParamSpec   *pspec);
static void field_set_property (GObject      *object,
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
tracker_field_type_get_type (void)
{
	static GType etype = 0;

	if (etype == 0) {
		static const GEnumValue values[] = {
			{ TRACKER_FIELD_TYPE_KEYWORD,
			  "TRACKER_FIELD_TYPE_KEYWORD",
			  "keyword" },
			{ TRACKER_FIELD_TYPE_INDEX,
			  "TRACKER_FIELD_TYPE_INDEX",
			  "index" },
			{ TRACKER_FIELD_TYPE_FULLTEXT,
			  "TRACKER_FIELD_TYPE_FULLTEXT",
			  "fulltext" },
			{ TRACKER_FIELD_TYPE_STRING,
			  "TRACKER_FIELD_TYPE_STRING",
			  "string" },
			{ TRACKER_FIELD_TYPE_INTEGER,
			  "TRACKER_FIELD_TYPE_INTEGER",
			  "integer" },
			{ TRACKER_FIELD_TYPE_DOUBLE,
			  "TRACKER_FIELD_TYPE_DOUBLE",
			  "double" },
			{ TRACKER_FIELD_TYPE_DATE,
			  "TRACKER_FIELD_TYPE_DATE",
			  "date" },
			{ TRACKER_FIELD_TYPE_BLOB,
			  "TRACKER_FIELD_TYPE_BLOB",
			  "blob" },
			{ TRACKER_FIELD_TYPE_STRUCT,
			  "TRACKER_FIELD_TYPE_STRUCT",
			  "struct" },
			{ TRACKER_FIELD_TYPE_LINK,
			  "TRACKER_FIELD_TYPE_LINK",
			  "link" },
			{ 0, NULL, NULL }
		};

		etype = g_enum_register_static ("TrackerFieldType", values);
	}

	return etype;
}

G_DEFINE_TYPE (TrackerField, tracker_field, G_TYPE_OBJECT);

const gchar *
tracker_field_type_to_string (TrackerFieldType fieldtype)
{
	GType	    type;
	GEnumClass *enum_class;
	GEnumValue *enum_value;

	type = tracker_field_type_get_type ();
	enum_class = G_ENUM_CLASS (g_type_class_peek (type));
	enum_value = g_enum_get_value (enum_class, fieldtype);

	if (!enum_value) {
		return NULL;
	}

	return enum_value->value_nick;
}

static void
tracker_field_class_init (TrackerFieldClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize	   = field_finalize;
	object_class->get_property = field_get_property;
	object_class->set_property = field_set_property;

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
							    tracker_field_type_get_type (),
							    TRACKER_FIELD_TYPE_INDEX,
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

	g_type_class_add_private (object_class, sizeof (TrackerFieldPriv));
}

static void
tracker_field_init (TrackerField *field)
{
}

static void
field_finalize (GObject *object)
{
	TrackerFieldPriv *priv;

	priv = GET_PRIV (object);

	g_free (priv->id);
	g_free (priv->name);

	if (priv->field_name) {
		g_free (priv->field_name);
	}

	g_slist_foreach (priv->child_ids, (GFunc) g_free, NULL);
	g_slist_free (priv->child_ids);

	(G_OBJECT_CLASS (tracker_field_parent_class)->finalize) (object);
}

static void
field_get_property (GObject    *object,
		    guint	param_id,
		    GValue     *value,
		    GParamSpec *pspec)
{
	TrackerFieldPriv *priv;

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
field_set_property (GObject	 *object,
		    guint	  param_id,
		    const GValue *value,
		    GParamSpec	 *pspec)
{
	switch (param_id) {
	case PROP_ID:
		tracker_field_set_id (TRACKER_FIELD (object),
				      g_value_get_string (value));
		break;
	case PROP_NAME:
		tracker_field_set_name (TRACKER_FIELD (object),
					g_value_get_string (value));
		break;
	case PROP_DATA_TYPE:
		tracker_field_set_data_type (TRACKER_FIELD (object),
					     g_value_get_enum (value));
		break;
	case PROP_FIELD_NAME:
		tracker_field_set_field_name (TRACKER_FIELD (object),
					      g_value_get_string (value));
		break;
	case PROP_WEIGHT:
		tracker_field_set_weight (TRACKER_FIELD (object),
					  g_value_get_int (value));
		break;
	case PROP_EMBEDDED:
		tracker_field_set_embedded (TRACKER_FIELD (object),
					    g_value_get_boolean (value));
		break;
	case PROP_MULTIPLE_VALUES:
		tracker_field_set_multiple_values (TRACKER_FIELD (object),
						   g_value_get_boolean (value));
		break;
	case PROP_DELIMITED:
		tracker_field_set_delimited (TRACKER_FIELD (object),
					     g_value_get_boolean (value));
		break;
	case PROP_FILTERED:
		tracker_field_set_filtered (TRACKER_FIELD (object),
					    g_value_get_boolean (value));
		break;
	case PROP_STORE_METADATA:
		tracker_field_set_store_metadata (TRACKER_FIELD (object),
						  g_value_get_boolean (value));
		break;
	case PROP_CHILD_IDS:
		tracker_field_set_child_ids (TRACKER_FIELD (object),
					     g_value_get_pointer (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static gboolean
field_int_validate (TrackerField *field,
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

TrackerField *
tracker_field_new (void)
{
	TrackerField *field;

	field = g_object_new (TRACKER_TYPE_FIELD, NULL);

	return field;
}

const gchar *
tracker_field_get_id (TrackerField *field)
{
	TrackerFieldPriv *priv;

	g_return_val_if_fail (TRACKER_IS_FIELD (field), NULL);

	priv = GET_PRIV (field);

	return priv->id;
}

const gchar *
tracker_field_get_name (TrackerField *field)
{
	TrackerFieldPriv *priv;

	g_return_val_if_fail (TRACKER_IS_FIELD (field), NULL);

	priv = GET_PRIV (field);

	return priv->name;
}

TrackerFieldType
tracker_field_get_data_type (TrackerField *field)
{
	TrackerFieldPriv *priv;

	g_return_val_if_fail (TRACKER_IS_FIELD (field), TRACKER_FIELD_TYPE_STRING); //FIXME

	priv = GET_PRIV (field);

	return priv->data_type;
}

const gchar *
tracker_field_get_field_name (TrackerField *field)
{
	TrackerFieldPriv *priv;

	g_return_val_if_fail (TRACKER_IS_FIELD (field), NULL);

	priv = GET_PRIV (field);

	return priv->field_name;
}

gint
tracker_field_get_weight (TrackerField *field)
{
	TrackerFieldPriv *priv;

	g_return_val_if_fail (TRACKER_IS_FIELD (field), -1);

	priv = GET_PRIV (field);

	return priv->weight;
}


gboolean
tracker_field_get_embedded (TrackerField *field)
{
	TrackerFieldPriv *priv;

	g_return_val_if_fail (TRACKER_IS_FIELD (field), FALSE);

	priv = GET_PRIV (field);

	return priv->embedded;
}


gboolean
tracker_field_get_multiple_values (TrackerField *field)
{
	TrackerFieldPriv *priv;

	g_return_val_if_fail (TRACKER_IS_FIELD (field), FALSE);

	priv = GET_PRIV (field);

	return priv->multiple_values;
}

gboolean
tracker_field_get_delimited (TrackerField *field)
{
	TrackerFieldPriv *priv;

	g_return_val_if_fail (TRACKER_IS_FIELD (field), FALSE);

	priv = GET_PRIV (field);

	return priv->delimited;
}

gboolean
tracker_field_get_filtered (TrackerField *field)
{
	TrackerFieldPriv *priv;

	g_return_val_if_fail (TRACKER_IS_FIELD (field), FALSE);

	priv = GET_PRIV (field);

	return priv->filtered;
}

gboolean
tracker_field_get_store_metadata (TrackerField *field)
{
	TrackerFieldPriv *priv;

	g_return_val_if_fail (TRACKER_IS_FIELD (field), FALSE);

	priv = GET_PRIV (field);

	return priv->store_metadata;
}


const GSList *
tracker_field_get_child_ids (TrackerField *field)
{
	TrackerFieldPriv *priv;

	g_return_val_if_fail (TRACKER_IS_FIELD (field), NULL);

	priv = GET_PRIV (field);

	return priv->child_ids;
}


void
tracker_field_set_id (TrackerField *field,
		      const gchar  *value)
{
	TrackerFieldPriv *priv;

	g_return_if_fail (TRACKER_IS_FIELD (field));

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
tracker_field_set_name (TrackerField *field,
			const gchar  *value)
{
	TrackerFieldPriv *priv;

	g_return_if_fail (TRACKER_IS_FIELD (field));

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
tracker_field_set_data_type (TrackerField     *field,
			     TrackerFieldType  value)
{
	TrackerFieldPriv *priv;

	g_return_if_fail (TRACKER_IS_FIELD (field));

	priv = GET_PRIV (field);

	priv->data_type = value;
	g_object_notify (G_OBJECT (field), "data-type");
}

void
tracker_field_set_field_name (TrackerField *field,
			      const gchar    *value)
{
	TrackerFieldPriv *priv;

	g_return_if_fail (TRACKER_IS_FIELD (field));

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
tracker_field_set_weight (TrackerField *field,
			  gint		value)
{
	TrackerFieldPriv *priv;
	g_return_if_fail (TRACKER_IS_FIELD (field));

	if (!field_int_validate (field, "weight", value)) {
		return;
	}

	priv = GET_PRIV (field);

	priv->weight = value;
	g_object_notify (G_OBJECT (field), "weight");
}

void
tracker_field_set_embedded (TrackerField *field,
			    gboolean	  value)
{
	TrackerFieldPriv *priv;

	g_return_if_fail (TRACKER_IS_FIELD (field));

	priv = GET_PRIV (field);

	priv->embedded = value;
	g_object_notify (G_OBJECT (field), "embedded");
}

void
tracker_field_set_multiple_values (TrackerField *field,
				   gboolean	 value)
{
	TrackerFieldPriv *priv;

	g_return_if_fail (TRACKER_IS_FIELD (field));

	priv = GET_PRIV (field);

	priv->multiple_values = value;
	g_object_notify (G_OBJECT (field), "multiple-values");
}

void
tracker_field_set_delimited (TrackerField *field,
			     gboolean	   value)
{
	TrackerFieldPriv *priv;

	g_return_if_fail (TRACKER_IS_FIELD (field));

	priv = GET_PRIV (field);

	priv->delimited = value;
	g_object_notify (G_OBJECT (field), "delimited");
}

void
tracker_field_set_filtered (TrackerField *field,
			    gboolean	  value)
{
	TrackerFieldPriv *priv;

	g_return_if_fail (TRACKER_IS_FIELD (field));

	priv = GET_PRIV (field);

	priv->filtered = value;
	g_object_notify (G_OBJECT (field), "filtered");
}

void
tracker_field_set_store_metadata (TrackerField *field,
				  gboolean	value)
{
	TrackerFieldPriv *priv;

	g_return_if_fail (TRACKER_IS_FIELD (field));

	priv = GET_PRIV (field);

	priv->store_metadata = value;
	g_object_notify (G_OBJECT (field), "store-metadata");
}

void
tracker_field_set_child_ids (TrackerField *field,
			     const GSList *value)
{
	TrackerFieldPriv *priv;

	g_return_if_fail (TRACKER_IS_FIELD (field));

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
tracker_field_append_child_id (TrackerField *field,
			       const gchar  *value)
{
	TrackerFieldPriv *priv;

	g_return_if_fail (TRACKER_IS_FIELD (field));

	priv = GET_PRIV (field);

	if (value) {
		priv->child_ids = g_slist_append (priv->child_ids, g_strdup (value));
	}

	g_object_notify (G_OBJECT (field), "child-ids");
}
