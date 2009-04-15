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

#include "tracker-class.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_CLASS, TrackerClassPriv))

typedef struct _TrackerClassPriv TrackerClassPriv;

struct _TrackerClassPriv {
	gint	       id;

	gchar	      *name;
	gchar	      *parent;

	gchar	      *property_prefix;
	gchar	      *content_metadata;
	GSList	      *key_metadata;

	TrackerDBType  db_type;

	gboolean       enabled;
	gboolean       embedded;

	gboolean       has_metadata;
	gboolean       has_full_text;
	gboolean       has_thumbs;

	gboolean       show_service_files;
	gboolean       show_service_directories;
};

static void class_finalize	 (GObject      *object);
static void class_get_property (GObject      *object,
				  guint		param_id,
				  GValue       *value,
				  GParamSpec   *pspec);
static void class_set_property (GObject      *object,
				  guint		param_id,
				  const GValue *value,
				  GParamSpec   *pspec);

enum {
	PROP_0,
	PROP_ID,
	PROP_NAME,
	PROP_PARENT,
	PROP_PROPERTY_PREFIX,
	PROP_CONTENT_METADATA,
	PROP_KEY_METADATA,
	PROP_DB_TYPE,
	PROP_ENABLED,
	PROP_EMBEDDED,
	PROP_HAS_METADATA,
	PROP_HAS_FULL_TEXT,
	PROP_HAS_THUMBS,
	PROP_SHOW_SERVICE_FILES,
	PROP_SHOW_SERVICE_DIRECTORIES
};

GType
tracker_db_type_get_type (void)
{
	static GType etype = 0;

	if (etype == 0) {
		static const GEnumValue values[] = {
			{ TRACKER_DB_TYPE_UNKNOWN,
			  "TRACKER_DB_TYPE_UNKNOWN",
			  "unknown" },
			{ TRACKER_DB_TYPE_DATA,
			  "TRACKER_DB_TYPE_DATA",
			  "data" },
			{ TRACKER_DB_TYPE_INDEX,
			  "TRACKER_DB_TYPE_INDEX",
			  "index" },
			{ TRACKER_DB_TYPE_COMMON,
			  "TRACKER_DB_TYPE_COMMON",
			  "common" },
			{ TRACKER_DB_TYPE_CONTENT,
			  "TRACKER_DB_TYPE_CONTENT",
			  "content" },
			{ TRACKER_DB_TYPE_EMAIL,
			  "TRACKER_DB_TYPE_EMAIL",
			  "email" },
			{ TRACKER_DB_TYPE_FILES,
			  "TRACKER_DB_TYPE_FILES",
			  "files" },
			{ TRACKER_DB_TYPE_CACHE,
			  "TRACKER_DB_TYPE_CACHE",
			  "cache" },
			{ TRACKER_DB_TYPE_USER,
			  "TRACKER_DB_TYPE_USER",
			  "user" },
			{ 0, NULL, NULL }
		};

		etype = g_enum_register_static ("TrackerDBType", values);
	}

	return etype;
}

G_DEFINE_TYPE (TrackerClass, tracker_class, G_TYPE_OBJECT);

static void
tracker_class_class_init (TrackerClassClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize	   = class_finalize;
	object_class->get_property = class_get_property;
	object_class->set_property = class_set_property;

	g_object_class_install_property (object_class,
					 PROP_ID,
					 g_param_spec_int ("id",
							   "id",
							   "Unique identifier for this service",
							   0,
							   G_MAXINT,
							   0,
							   G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_NAME,
					 g_param_spec_string ("name",
							      "name",
							      "Service name",
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_PARENT,
					 g_param_spec_string ("parent",
							      "parent",
							      "Service name of parent",
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_PROPERTY_PREFIX,
					 g_param_spec_string ("property-prefix",
							      "property-prefix",
							      "The properties of this category are prefix:name",
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_CONTENT_METADATA,
					 g_param_spec_string ("content-metadata",
							      "content-metadata",
							      "Content metadata",
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_KEY_METADATA,
					 g_param_spec_pointer ("key-metadata",
							       "key-metadata",
							       "Key metadata",
							       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_DB_TYPE,
					 g_param_spec_enum ("db-type",
							    "db-type",
							    "Database type",
							    tracker_db_type_get_type (),
							    TRACKER_DB_TYPE_DATA,
							    G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_ENABLED,
					 g_param_spec_boolean ("enabled",
							       "enabled",
							       "Enabled",
							       TRUE,
							       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_EMBEDDED,
					 g_param_spec_boolean ("embedded",
							       "embedded",
							       "Embedded",
							       FALSE,
							       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_HAS_METADATA,
					 g_param_spec_boolean ("has-metadata",
							       "has-metadata",
							       "Has metadata",
							       FALSE,
							       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_HAS_FULL_TEXT,
					 g_param_spec_boolean ("has-full-text",
							       "has-full-text",
							       "Has full text",
							       FALSE,
							       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_HAS_THUMBS,
					 g_param_spec_boolean ("has-thumbs",
							       "has-thumbs",
							       "Has thumbnails",
							       FALSE,
							       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_SHOW_SERVICE_FILES,
					 g_param_spec_boolean ("show-service-files",
							       "show-service-files",
							       "Show service files",
							       FALSE,
							       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_SHOW_SERVICE_DIRECTORIES,
					 g_param_spec_boolean ("show-service-directories",
							       "show-service-directories",
							       "Show service directories",
							       FALSE,
							       G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof (TrackerClassPriv));
}

static void
tracker_class_init (TrackerClass *service)
{
}

static void
class_finalize (GObject *object)
{
	TrackerClassPriv *priv;

	priv = GET_PRIV (object);

	g_free (priv->name);
	g_free (priv->parent);
	g_free (priv->content_metadata);
	g_free (priv->property_prefix);

	g_slist_foreach (priv->key_metadata, (GFunc) g_free, NULL);
	g_slist_free (priv->key_metadata);

	(G_OBJECT_CLASS (tracker_class_parent_class)->finalize) (object);
}

static void
class_get_property (GObject	 *object,
		      guint	  param_id,
		      GValue	 *value,
		      GParamSpec *pspec)
{
	TrackerClassPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_ID:
		g_value_set_int (value, priv->id);
		break;
	case PROP_NAME:
		g_value_set_string (value, priv->name);
		break;
	case PROP_PARENT:
		g_value_set_string (value, priv->parent);
		break;
	case PROP_PROPERTY_PREFIX:
		g_value_set_string (value, priv->property_prefix);
		break;
	case PROP_CONTENT_METADATA:
		g_value_set_string (value, priv->content_metadata);
		break;
	case PROP_KEY_METADATA:
		g_value_set_pointer (value, priv->key_metadata);
		break;
	case PROP_DB_TYPE:
		g_value_set_enum (value, priv->db_type);
		break;
	case PROP_ENABLED:
		g_value_set_boolean (value, priv->enabled);
		break;
	case PROP_EMBEDDED:
		g_value_set_boolean (value, priv->embedded);
		break;
	case PROP_HAS_METADATA:
		g_value_set_boolean (value, priv->has_metadata);
		break;
	case PROP_HAS_FULL_TEXT:
		g_value_set_boolean (value, priv->has_full_text);
		break;
	case PROP_HAS_THUMBS:
		g_value_set_boolean (value, priv->has_thumbs);
		break;
	case PROP_SHOW_SERVICE_FILES:
		g_value_set_boolean (value, priv->show_service_files);
		break;
	case PROP_SHOW_SERVICE_DIRECTORIES:
		g_value_set_boolean (value, priv->show_service_directories);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
class_set_property (GObject	   *object,
		      guint	    param_id,
		      const GValue *value,
		      GParamSpec   *pspec)
{
	TrackerClassPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_ID:
		tracker_class_set_id (TRACKER_CLASS (object),
					g_value_get_int (value));
		break;
	case PROP_NAME:
		tracker_class_set_name (TRACKER_CLASS (object),
					  g_value_get_string (value));
		break;
	case PROP_PROPERTY_PREFIX:
		tracker_class_set_property_prefix (TRACKER_CLASS (object),
						     g_value_get_string (value));
		break;
	case PROP_PARENT:
		tracker_class_set_parent (TRACKER_CLASS (object),
					    g_value_get_string (value));
		break;
	case PROP_CONTENT_METADATA:
		tracker_class_set_content_metadata (TRACKER_CLASS (object),
						      g_value_get_string (value));
		break;
	case PROP_KEY_METADATA:
		tracker_class_set_key_metadata (TRACKER_CLASS (object),
						  g_value_get_pointer (value));
		break;
	case PROP_DB_TYPE:
		tracker_class_set_db_type (TRACKER_CLASS (object),
					     g_value_get_enum (value));
		break;
	case PROP_ENABLED:
		tracker_class_set_enabled (TRACKER_CLASS (object),
					     g_value_get_boolean (value));
		break;
	case PROP_EMBEDDED:
		tracker_class_set_embedded (TRACKER_CLASS (object),
					      g_value_get_boolean (value));
		break;
	case PROP_HAS_METADATA:
		tracker_class_set_has_metadata (TRACKER_CLASS (object),
						  g_value_get_boolean (value));
		break;
	case PROP_HAS_FULL_TEXT:
		tracker_class_set_has_full_text (TRACKER_CLASS (object),
						   g_value_get_boolean (value));
		break;
	case PROP_HAS_THUMBS:
		tracker_class_set_has_thumbs (TRACKER_CLASS (object),
						g_value_get_boolean (value));
		break;
	case PROP_SHOW_SERVICE_FILES:
		tracker_class_set_show_service_files (TRACKER_CLASS (object),
							g_value_get_boolean (value));
		break;
	case PROP_SHOW_SERVICE_DIRECTORIES:
		tracker_class_set_show_service_directories (TRACKER_CLASS (object),
							      g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static gboolean
class_int_validate (TrackerClass *service,
		      const gchar   *property,
		      gint	    value)
{
#ifdef G_DISABLE_CHECKS
	GParamSpec *spec;
	GValue	    value = { 0 };
	gboolean    valid;

	spec = g_object_class_find_property (G_OBJECT_CLASS (service), property);
	g_return_val_if_fail (spec != NULL, FALSE);

	g_value_init (&value, spec->value_type);
	g_value_set_int (&value, verbosity);
	valid = g_param_value_validate (spec, &value);
	g_value_unset (&value);

	g_return_val_if_fail (valid != TRUE, FALSE);
#endif

	return TRUE;
}

TrackerClass *
tracker_class_new (void)
{
	TrackerClass *service;

	service = g_object_new (TRACKER_TYPE_CLASS, NULL);

	return service;
}

gint
tracker_class_get_id (TrackerClass *service)
{
	TrackerClassPriv *priv;

	g_return_val_if_fail (TRACKER_IS_CLASS (service), -1);

	priv = GET_PRIV (service);

	return priv->id;
}

const gchar *
tracker_class_get_name (TrackerClass *service)
{
	TrackerClassPriv *priv;

	g_return_val_if_fail (TRACKER_IS_CLASS (service), NULL);

	priv = GET_PRIV (service);

	return priv->name;
}

const gchar *
tracker_class_get_parent (TrackerClass *service)
{
	TrackerClassPriv *priv;

	g_return_val_if_fail (TRACKER_IS_CLASS (service), NULL);

	priv = GET_PRIV (service);

	return priv->parent;
}

const gchar *
tracker_class_get_property_prefix (TrackerClass *service)
{
	TrackerClassPriv *priv;

	g_return_val_if_fail (TRACKER_IS_CLASS (service), NULL);

	priv = GET_PRIV (service);

	return priv->property_prefix;
}

const gchar *
tracker_class_get_content_metadata (TrackerClass *service)
{
	TrackerClassPriv *priv;

	g_return_val_if_fail (TRACKER_IS_CLASS (service), NULL);

	priv = GET_PRIV (service);

	return priv->content_metadata;
}

const GSList *
tracker_class_get_key_metadata (TrackerClass *service)
{
	TrackerClassPriv *priv;

	g_return_val_if_fail (TRACKER_IS_CLASS (service), NULL);

	priv = GET_PRIV (service);

	return priv->key_metadata;
}

TrackerDBType
tracker_class_get_db_type (TrackerClass *service)
{
	TrackerClassPriv *priv;

	g_return_val_if_fail (TRACKER_IS_CLASS (service), TRACKER_DB_TYPE_DATA);

	priv = GET_PRIV (service);

	return priv->db_type;
}

gboolean
tracker_class_get_enabled (TrackerClass *service)
{
	TrackerClassPriv *priv;

	g_return_val_if_fail (TRACKER_IS_CLASS (service), FALSE);

	priv = GET_PRIV (service);

	return priv->enabled;
}

gboolean
tracker_class_get_embedded (TrackerClass *service)
{
	TrackerClassPriv *priv;

	g_return_val_if_fail (TRACKER_IS_CLASS (service), FALSE);

	priv = GET_PRIV (service);

	return priv->embedded;
}

gboolean
tracker_class_get_has_metadata (TrackerClass *service)
{
	TrackerClassPriv *priv;

	g_return_val_if_fail (TRACKER_IS_CLASS (service), FALSE);

	priv = GET_PRIV (service);

	return priv->has_metadata;
}

gboolean
tracker_class_get_has_full_text (TrackerClass *service)
{
	TrackerClassPriv *priv;

	g_return_val_if_fail (TRACKER_IS_CLASS (service), FALSE);

	priv = GET_PRIV (service);

	return priv->has_full_text;
}

gboolean
tracker_class_get_has_thumbs (TrackerClass *service)
{
	TrackerClassPriv *priv;

	g_return_val_if_fail (TRACKER_IS_CLASS (service), FALSE);

	priv = GET_PRIV (service);

	return priv->has_thumbs;
}

gboolean
tracker_class_get_show_service_files (TrackerClass *service)
{
	TrackerClassPriv *priv;

	g_return_val_if_fail (TRACKER_IS_CLASS (service), FALSE);

	priv = GET_PRIV (service);

	return priv->show_service_files;
}

gboolean
tracker_class_get_show_service_directories (TrackerClass *service)
{
	TrackerClassPriv *priv;

	g_return_val_if_fail (TRACKER_IS_CLASS (service), FALSE);

	priv = GET_PRIV (service);

	return priv->show_service_directories;
}

void
tracker_class_set_id (TrackerClass *service,
			gint		value)
{
	TrackerClassPriv *priv;

	g_return_if_fail (TRACKER_IS_CLASS (service));

	if (!class_int_validate (service, "id", value)) {
		return;
	}

	priv = GET_PRIV (service);

	priv->id = value;
	g_object_notify (G_OBJECT (service), "id");
}

void
tracker_class_set_name (TrackerClass *service,
			  const gchar	 *value)
{
	TrackerClassPriv *priv;

	g_return_if_fail (TRACKER_IS_CLASS (service));

	priv = GET_PRIV (service);

	g_free (priv->name);

	if (value) {
		priv->name = g_strdup (value);
	} else {
		priv->name = NULL;
	}

	g_object_notify (G_OBJECT (service), "name");
}

void
tracker_class_set_parent (TrackerClass *service,
			    const gchar    *value)
{
	TrackerClassPriv *priv;

	g_return_if_fail (TRACKER_IS_CLASS (service));

	priv = GET_PRIV (service);

	g_free (priv->parent);

	if (value) {
		priv->parent = g_strdup (value);
	} else {
		priv->parent = NULL;
	}

	g_object_notify (G_OBJECT (service), "parent");
}

void
tracker_class_set_property_prefix (TrackerClass *service,
				     const gchar    *value)
{
	TrackerClassPriv *priv;

	g_return_if_fail (TRACKER_IS_CLASS (service));

	priv = GET_PRIV (service);

	g_free (priv->property_prefix);

	if (value) {
		priv->property_prefix = g_strdup (value);
	} else {
		priv->property_prefix = NULL;
	}

	g_object_notify (G_OBJECT (service), "property-prefix");
}

void
tracker_class_set_content_metadata (TrackerClass *service,
				      const gchar    *value)
{
	TrackerClassPriv *priv;

	g_return_if_fail (TRACKER_IS_CLASS (service));

	priv = GET_PRIV (service);

	g_free (priv->content_metadata);

	if (value) {
		priv->content_metadata = g_strdup (value);
	} else {
		priv->content_metadata = NULL;
	}

	g_object_notify (G_OBJECT (service), "content-metadata");
}

void
tracker_class_set_key_metadata (TrackerClass *service,
				  const GSList	 *value)
{
	TrackerClassPriv *priv;

	g_return_if_fail (TRACKER_IS_CLASS (service));

	priv = GET_PRIV (service);

	g_slist_foreach (priv->key_metadata, (GFunc) g_free, NULL);
	g_slist_free (priv->key_metadata);

	if (value) {
		GSList	     *new_list;
		const GSList *l;

		new_list = NULL;

		for (l = value; l; l = l->next) {
			new_list = g_slist_prepend (new_list, g_strdup (l->data));
		}

		new_list = g_slist_reverse (new_list);
		priv->key_metadata = new_list;
	} else {
		priv->key_metadata = NULL;
	}

	g_object_notify (G_OBJECT (service), "key-metadata");
}

void
tracker_class_set_db_type (TrackerClass *service,
			     TrackerDBType   value)
{
	TrackerClassPriv *priv;

	g_return_if_fail (TRACKER_IS_CLASS (service));

	priv = GET_PRIV (service);

	priv->db_type = value;
	g_object_notify (G_OBJECT (service), "db-type");
}

void
tracker_class_set_enabled (TrackerClass *service,
			     gboolean	     value)
{
	TrackerClassPriv *priv;

	g_return_if_fail (TRACKER_IS_CLASS (service));

	priv = GET_PRIV (service);

	priv->enabled = value;
	g_object_notify (G_OBJECT (service), "enabled");
}

void
tracker_class_set_embedded (TrackerClass *service,
			      gboolean	      value)
{
	TrackerClassPriv *priv;

	g_return_if_fail (TRACKER_IS_CLASS (service));

	priv = GET_PRIV (service);

	priv->embedded = value;
	g_object_notify (G_OBJECT (service), "embedded");
}

void
tracker_class_set_has_metadata (TrackerClass *service,
				  gboolean	  value)
{
	TrackerClassPriv *priv;

	g_return_if_fail (TRACKER_IS_CLASS (service));

	priv = GET_PRIV (service);

	priv->has_metadata = value;
	g_object_notify (G_OBJECT (service), "has-metadata");
}

void
tracker_class_set_has_full_text (TrackerClass *service,
				   gboolean	   value)
{
	TrackerClassPriv *priv;

	g_return_if_fail (TRACKER_IS_CLASS (service));

	priv = GET_PRIV (service);

	priv->has_full_text = value;
	g_object_notify (G_OBJECT (service), "has-full-text");
}

void
tracker_class_set_has_thumbs (TrackerClass *service,
				gboolean	value)
{
	TrackerClassPriv *priv;

	g_return_if_fail (TRACKER_IS_CLASS (service));

	priv = GET_PRIV (service);

	priv->has_thumbs = value;
	g_object_notify (G_OBJECT (service), "has-thumbs");
}

void
tracker_class_set_show_service_files (TrackerClass *service,
					gboolean	value)
{
	TrackerClassPriv *priv;

	g_return_if_fail (TRACKER_IS_CLASS (service));

	priv = GET_PRIV (service);

	priv->show_service_files = value;
	g_object_notify (G_OBJECT (service), "show-service-files");
}

void
tracker_class_set_show_service_directories (TrackerClass *service,
					      gboolean	      value)
{
	TrackerClassPriv *priv;

	g_return_if_fail (TRACKER_IS_CLASS (service));

	priv = GET_PRIV (service);

	priv->show_service_directories = value;
	g_object_notify (G_OBJECT (service), "show-service-directories");
}

