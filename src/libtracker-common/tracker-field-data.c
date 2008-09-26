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

#include "tracker-field-data.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_FIELD_DATA, TrackerFieldDataPriv))

typedef struct _TrackerFieldDataPriv TrackerFieldDataPriv;

struct _TrackerFieldDataPriv {
	gchar		 *alias;

	gchar		 *table_name;
	gchar		 *field_name;

	gchar		 *select_field;
	gchar		 *where_field;
	gchar		 *id_field;

	TrackerFieldType  data_type;

	gboolean	  multiple_values;
	gboolean	  is_select;
	gboolean	  is_condition;
	gboolean	  needs_join;
};

static void field_data_finalize     (GObject	  *object);
static void field_data_get_property (GObject	  *object,
				     guint	   param_id,
				     GValue	  *value,
				     GParamSpec   *pspec);
static void field_data_set_property (GObject	  *object,
				     guint	   param_id,
				     const GValue *value,
				     GParamSpec   *pspec);

enum {
	PROP_0,
	PROP_ALIAS,
	PROP_TABLE_NAME,
	PROP_FIELD_NAME,
	PROP_SELECT_FIELD,
	PROP_WHERE_FIELD,
	PROP_ID_FIELD,
	PROP_DATA_TYPE,
	PROP_MULTIPLE_VALUES,
	PROP_IS_SELECT,
	PROP_IS_CONDITION,
	PROP_NEEDS_JOIN
};

G_DEFINE_TYPE (TrackerFieldData, tracker_field_data, G_TYPE_OBJECT);

static void
tracker_field_data_class_init (TrackerFieldDataClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize	   = field_data_finalize;
	object_class->get_property = field_data_get_property;
	object_class->set_property = field_data_set_property;

	g_object_class_install_property (object_class,
					 PROP_ALIAS,
					 g_param_spec_string ("alias",
							      "alias",
							      "A name for this field data",
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_TABLE_NAME,
					 g_param_spec_string ("table-name",
							      "Table name",
							      "Table name",
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_FIELD_NAME,
					 g_param_spec_string ("field-name",
							      "Field name",
							      "Field name",
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_SELECT_FIELD,
					 g_param_spec_string ("select-field",
							      "Select field",
							      "Select field",
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_WHERE_FIELD,
					 g_param_spec_string ("where-field",
							      "Where field",
							      "Where field",
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_ID_FIELD,
					 g_param_spec_string ("id-field",
							      "ID field",
							      "ID field",
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_DATA_TYPE,
					 g_param_spec_enum ("data-type",
							    "Data type",
							    "TrackerField type",
							    tracker_field_type_get_type (),
							    TRACKER_FIELD_TYPE_INDEX,
							    G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_MULTIPLE_VALUES,
					 g_param_spec_boolean ("multiple-values",
							       "Multiple values",
							       "Multiple values",
							       FALSE,
							       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_IS_SELECT,
					 g_param_spec_boolean ("is-select",
							       "Is select",
							       "Is select",
							       FALSE,
							       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_IS_CONDITION,
					 g_param_spec_boolean ("is-condition",
							       "Is condition",
							       "Is condition",
							       FALSE,
							       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_NEEDS_JOIN,
					 g_param_spec_boolean ("needs-join",
							       "Needs join",
							       "Needs join",
							       FALSE,
							       G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof (TrackerFieldDataPriv));
}

static void
tracker_field_data_init (TrackerFieldData *field_data)
{
}

static void
field_data_finalize (GObject *object)
{
	TrackerFieldDataPriv *priv;

	priv = GET_PRIV (object);

	g_free (priv->alias);

	g_free (priv->table_name);
	g_free (priv->field_name);

	g_free (priv->select_field);
	g_free (priv->where_field);
	g_free (priv->id_field);

	(G_OBJECT_CLASS (tracker_field_data_parent_class)->finalize) (object);
}

static void
field_data_get_property (GObject    *object,
			 guint	     param_id,
			 GValue     *value,
			 GParamSpec *pspec)
{
	TrackerFieldDataPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_ALIAS:
		g_value_set_string (value, priv->alias);
		break;
	case PROP_TABLE_NAME:
		g_value_set_string (value, priv->table_name);
		break;
	case PROP_FIELD_NAME:
		g_value_set_string (value, priv->field_name);
		break;
	case PROP_SELECT_FIELD:
		g_value_set_string (value, priv->select_field);
		break;
	case PROP_WHERE_FIELD:
		g_value_set_string (value, priv->where_field);
		break;
	case PROP_ID_FIELD:
		g_value_set_string (value, priv->id_field);
		break;
	case PROP_DATA_TYPE:
		g_value_set_enum (value, priv->data_type);
		break;
	case PROP_MULTIPLE_VALUES:
		g_value_set_boolean (value, priv->multiple_values);
		break;
	case PROP_IS_SELECT:
		g_value_set_boolean (value, priv->is_select);
		break;
	case PROP_IS_CONDITION:
		g_value_set_boolean (value, priv->is_condition);
		break;
	case PROP_NEEDS_JOIN:
		g_value_set_boolean (value, priv->needs_join);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
field_data_set_property (GObject      *object,
			 guint	       param_id,
			 const GValue *value,
			 GParamSpec   *pspec)
{
	switch (param_id) {
	case PROP_ALIAS:
		tracker_field_data_set_alias (TRACKER_FIELD_DATA (object),
					      g_value_get_string (value));
		break;
	case PROP_TABLE_NAME:
		tracker_field_data_set_table_name (TRACKER_FIELD_DATA (object),
						   g_value_get_string (value));
		break;
	case PROP_FIELD_NAME:
		tracker_field_data_set_field_name (TRACKER_FIELD_DATA (object),
						   g_value_get_string (value));
		break;
	case PROP_SELECT_FIELD:
		tracker_field_data_set_select_field (TRACKER_FIELD_DATA (object),
						     g_value_get_string (value));
		break;
	case PROP_WHERE_FIELD:
		tracker_field_data_set_where_field (TRACKER_FIELD_DATA (object),
						    g_value_get_string (value));
		break;
	case PROP_ID_FIELD:
		tracker_field_data_set_id_field (TRACKER_FIELD_DATA (object),
						 g_value_get_string (value));
		break;
	case PROP_DATA_TYPE:
		tracker_field_data_set_data_type (TRACKER_FIELD_DATA (object),
						 g_value_get_enum (value));
		break;
	case PROP_MULTIPLE_VALUES:
		tracker_field_data_set_multiple_values (TRACKER_FIELD_DATA (object),
							g_value_get_boolean (value));
		break;
	case PROP_IS_SELECT:
		tracker_field_data_set_is_select (TRACKER_FIELD_DATA (object),
						  g_value_get_boolean (value));
		break;
	case PROP_IS_CONDITION:
		tracker_field_data_set_is_condition (TRACKER_FIELD_DATA (object),
						     g_value_get_boolean (value));
		break;
	case PROP_NEEDS_JOIN:
		tracker_field_data_set_needs_join (TRACKER_FIELD_DATA (object),
						   g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

TrackerFieldData *
tracker_field_data_new (void)
{
	TrackerFieldData *field_data;

	field_data = g_object_new (TRACKER_TYPE_FIELD_DATA, NULL);

	return field_data;
}

const gchar *
tracker_field_data_get_alias (TrackerFieldData *field_data)
{
	TrackerFieldDataPriv *priv;

	g_return_val_if_fail (TRACKER_IS_FIELD_DATA (field_data), NULL);

	priv = GET_PRIV (field_data);

	return priv->alias;
}

const gchar *
tracker_field_data_get_table_name (TrackerFieldData *field_data)
{
	TrackerFieldDataPriv *priv;

	g_return_val_if_fail (TRACKER_IS_FIELD_DATA (field_data), NULL);

	priv = GET_PRIV (field_data);

	return priv->table_name;
}

const gchar *
tracker_field_data_get_field_name (TrackerFieldData *field_data)
{
	TrackerFieldDataPriv *priv;

	g_return_val_if_fail (TRACKER_IS_FIELD_DATA (field_data), NULL);

	priv = GET_PRIV (field_data);

	return priv->field_name;
}

const gchar *
tracker_field_data_get_select_field (TrackerFieldData *field_data)
{
	TrackerFieldDataPriv *priv;

	g_return_val_if_fail (TRACKER_IS_FIELD_DATA (field_data), NULL);

	priv = GET_PRIV (field_data);

	return priv->select_field;
}

const gchar *
tracker_field_data_get_where_field (TrackerFieldData *field_data)
{
	TrackerFieldDataPriv *priv;

	g_return_val_if_fail (TRACKER_IS_FIELD_DATA (field_data), NULL);

	priv = GET_PRIV (field_data);

	return priv->where_field;
}

const gchar *
tracker_field_data_get_id_field (TrackerFieldData *field_data)
{
	TrackerFieldDataPriv *priv;

	g_return_val_if_fail (TRACKER_IS_FIELD_DATA (field_data), NULL);

	priv = GET_PRIV (field_data);

	return priv->id_field;
}

TrackerFieldType
tracker_field_data_get_data_type (TrackerFieldData *field_data)
{
	TrackerFieldDataPriv *priv;

	g_return_val_if_fail (TRACKER_IS_FIELD_DATA (field_data), TRACKER_FIELD_TYPE_INDEX);

	priv = GET_PRIV (field_data);

	return priv->data_type;
}

gboolean
tracker_field_data_get_multiple_values (TrackerFieldData *field_data)
{
	TrackerFieldDataPriv *priv;

	g_return_val_if_fail (TRACKER_IS_FIELD_DATA (field_data), FALSE);

	priv = GET_PRIV (field_data);

	return priv->multiple_values;
}


gboolean
tracker_field_data_get_is_select (TrackerFieldData *field_data)
{
	TrackerFieldDataPriv *priv;

	g_return_val_if_fail (TRACKER_IS_FIELD_DATA (field_data), FALSE);

	priv = GET_PRIV (field_data);

	return priv->is_select;
}

gboolean
tracker_field_data_get_is_condition (TrackerFieldData *field_data)
{
	TrackerFieldDataPriv *priv;

	g_return_val_if_fail (TRACKER_IS_FIELD_DATA (field_data), FALSE);

	priv = GET_PRIV (field_data);

	return priv->is_condition;
}

gboolean
tracker_field_data_get_needs_join (TrackerFieldData *field_data)
{
	TrackerFieldDataPriv *priv;

	g_return_val_if_fail (TRACKER_IS_FIELD_DATA (field_data), FALSE);

	priv = GET_PRIV (field_data);

	return priv->needs_join;
}

void
tracker_field_data_set_alias (TrackerFieldData *field_data,
			      const gchar      *value)
{
	TrackerFieldDataPriv *priv;

	g_return_if_fail (TRACKER_IS_FIELD_DATA (field_data));

	priv = GET_PRIV (field_data);

	g_free (priv->alias);

	if (value) {
		priv->alias = g_strdup (value);
	} else {
		priv->alias = NULL;
	}

	g_object_notify (G_OBJECT (field_data), "alias");
}

void
tracker_field_data_set_table_name (TrackerFieldData *field_data,
				   const gchar	    *value)
{
	TrackerFieldDataPriv *priv;

	g_return_if_fail (TRACKER_IS_FIELD_DATA (field_data));

	priv = GET_PRIV (field_data);

	g_free (priv->table_name);

	if (value) {
		priv->table_name = g_strdup (value);
	} else {
		priv->table_name = NULL;
	}

	g_object_notify (G_OBJECT (field_data), "table-name");
}

void
tracker_field_data_set_field_name (TrackerFieldData *field_data,
				   const gchar	    *value)
{
	TrackerFieldDataPriv *priv;

	g_return_if_fail (TRACKER_IS_FIELD_DATA (field_data));

	priv = GET_PRIV (field_data);

	g_free (priv->field_name);

	if (value) {
		priv->field_name = g_strdup (value);
	} else {
		priv->field_name = NULL;
	}

	g_object_notify (G_OBJECT (field_data), "field-name");
}

void
tracker_field_data_set_select_field (TrackerFieldData *field_data,
				     const gchar      *value)
{
	TrackerFieldDataPriv *priv;

	g_return_if_fail (TRACKER_IS_FIELD_DATA (field_data));

	priv = GET_PRIV (field_data);

	g_free (priv->select_field);

	if (value) {
		priv->select_field = g_strdup (value);
	} else {
		priv->select_field = NULL;
	}

	g_object_notify (G_OBJECT (field_data), "select-field");
}

void
tracker_field_data_set_where_field (TrackerFieldData *field_data,
				    const gchar      *value)
{
	TrackerFieldDataPriv *priv;

	g_return_if_fail (TRACKER_IS_FIELD_DATA (field_data));

	priv = GET_PRIV (field_data);

	g_free (priv->where_field);

	if (value) {
		priv->where_field = g_strdup (value);
	} else {
		priv->where_field = NULL;
	}

	g_object_notify (G_OBJECT (field_data), "where-field");
}

void
tracker_field_data_set_id_field (TrackerFieldData *field_data,
				 const gchar	  *value)
{
	TrackerFieldDataPriv *priv;

	g_return_if_fail (TRACKER_IS_FIELD_DATA (field_data));

	priv = GET_PRIV (field_data);

	g_free (priv->id_field);

	if (value) {
		priv->id_field = g_strdup (value);
	} else {
		priv->id_field = NULL;
	}

	g_object_notify (G_OBJECT (field_data), "id-field");
}

void
tracker_field_data_set_data_type (TrackerFieldData *field_data,
				  TrackerFieldType  value)
{
	TrackerFieldDataPriv *priv;

	g_return_if_fail (TRACKER_IS_FIELD_DATA (field_data));

	priv = GET_PRIV (field_data);

	priv->data_type = value;
	g_object_notify (G_OBJECT (field_data), "data-type");
}

void
tracker_field_data_set_multiple_values (TrackerFieldData *field_data,
					gboolean	  value)
{
	TrackerFieldDataPriv *priv;

	g_return_if_fail (TRACKER_IS_FIELD_DATA (field_data));

	priv = GET_PRIV (field_data);

	priv->multiple_values = value;
	g_object_notify (G_OBJECT (field_data), "multiple-values");
}

void
tracker_field_data_set_is_select (TrackerFieldData *field_data,
				  gboolean	    value)
{
	TrackerFieldDataPriv *priv;

	g_return_if_fail (TRACKER_IS_FIELD_DATA (field_data));

	priv = GET_PRIV (field_data);

	priv->is_select = value;
	g_object_notify (G_OBJECT (field_data), "is-select");
}

void
tracker_field_data_set_is_condition (TrackerFieldData *field_data,
				     gboolean	       value)
{
	TrackerFieldDataPriv *priv;

	g_return_if_fail (TRACKER_IS_FIELD_DATA (field_data));

	priv = GET_PRIV (field_data);

	priv->is_condition = value;
	g_object_notify (G_OBJECT (field_data), "is-condition");
}

void
tracker_field_data_set_needs_join (TrackerFieldData *field_data,
				   gboolean	     value)
{
	TrackerFieldDataPriv *priv;

	g_return_if_fail (TRACKER_IS_FIELD_DATA (field_data));

	priv = GET_PRIV (field_data);

	priv->needs_join = value;
	g_object_notify (G_OBJECT (field_data), "needs-join");
}
