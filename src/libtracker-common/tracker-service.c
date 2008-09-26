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

#include "tracker-service.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_SERVICE, TrackerServicePriv))

typedef struct _TrackerServicePriv TrackerServicePriv;

struct _TrackerServicePriv {
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

static void service_finalize	 (GObject      *object);
static void service_get_property (GObject      *object,
				  guint		param_id,
				  GValue       *value,
				  GParamSpec   *pspec);
static void service_set_property (GObject      *object,
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
			{ TRACKER_DB_TYPE_XESAM,
			  "TRACKER_DB_TYPE_XESAM",
			  "xesam" },
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

G_DEFINE_TYPE (TrackerService, tracker_service, G_TYPE_OBJECT);

static void
tracker_service_class_init (TrackerServiceClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize	   = service_finalize;
	object_class->get_property = service_get_property;
	object_class->set_property = service_set_property;

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

	g_type_class_add_private (object_class, sizeof (TrackerServicePriv));
}

static void
tracker_service_init (TrackerService *service)
{
}

static void
service_finalize (GObject *object)
{
	TrackerServicePriv *priv;

	priv = GET_PRIV (object);

	g_free (priv->name);
	g_free (priv->parent);
	g_free (priv->content_metadata);
	g_free (priv->property_prefix);

	g_slist_foreach (priv->key_metadata, (GFunc) g_free, NULL);
	g_slist_free (priv->key_metadata);

	(G_OBJECT_CLASS (tracker_service_parent_class)->finalize) (object);
}

static void
service_get_property (GObject	 *object,
		      guint	  param_id,
		      GValue	 *value,
		      GParamSpec *pspec)
{
	TrackerServicePriv *priv;

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
service_set_property (GObject	   *object,
		      guint	    param_id,
		      const GValue *value,
		      GParamSpec   *pspec)
{
	TrackerServicePriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_ID:
		tracker_service_set_id (TRACKER_SERVICE (object),
					g_value_get_int (value));
		break;
	case PROP_NAME:
		tracker_service_set_name (TRACKER_SERVICE (object),
					  g_value_get_string (value));
		break;
	case PROP_PROPERTY_PREFIX:
		tracker_service_set_property_prefix (TRACKER_SERVICE (object),
						     g_value_get_string (value));
		break;
	case PROP_PARENT:
		tracker_service_set_parent (TRACKER_SERVICE (object),
					    g_value_get_string (value));
		break;
	case PROP_CONTENT_METADATA:
		tracker_service_set_content_metadata (TRACKER_SERVICE (object),
						      g_value_get_string (value));
		break;
	case PROP_KEY_METADATA:
		tracker_service_set_key_metadata (TRACKER_SERVICE (object),
						  g_value_get_pointer (value));
		break;
	case PROP_DB_TYPE:
		tracker_service_set_db_type (TRACKER_SERVICE (object),
					     g_value_get_enum (value));
		break;
	case PROP_ENABLED:
		tracker_service_set_enabled (TRACKER_SERVICE (object),
					     g_value_get_boolean (value));
		break;
	case PROP_EMBEDDED:
		tracker_service_set_embedded (TRACKER_SERVICE (object),
					      g_value_get_boolean (value));
		break;
	case PROP_HAS_METADATA:
		tracker_service_set_has_metadata (TRACKER_SERVICE (object),
						  g_value_get_boolean (value));
		break;
	case PROP_HAS_FULL_TEXT:
		tracker_service_set_has_full_text (TRACKER_SERVICE (object),
						   g_value_get_boolean (value));
		break;
	case PROP_HAS_THUMBS:
		tracker_service_set_has_thumbs (TRACKER_SERVICE (object),
						g_value_get_boolean (value));
		break;
	case PROP_SHOW_SERVICE_FILES:
		tracker_service_set_show_service_files (TRACKER_SERVICE (object),
							g_value_get_boolean (value));
		break;
	case PROP_SHOW_SERVICE_DIRECTORIES:
		tracker_service_set_show_service_directories (TRACKER_SERVICE (object),
							      g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static gboolean
service_int_validate (TrackerService *service,
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

TrackerService *
tracker_service_new (void)
{
	TrackerService *service;

	service = g_object_new (TRACKER_TYPE_SERVICE, NULL);

	return service;
}

gint
tracker_service_get_id (TrackerService *service)
{
	TrackerServicePriv *priv;

	g_return_val_if_fail (TRACKER_IS_SERVICE (service), -1);

	priv = GET_PRIV (service);

	return priv->id;
}

const gchar *
tracker_service_get_name (TrackerService *service)
{
	TrackerServicePriv *priv;

	g_return_val_if_fail (TRACKER_IS_SERVICE (service), NULL);

	priv = GET_PRIV (service);

	return priv->name;
}

const gchar *
tracker_service_get_parent (TrackerService *service)
{
	TrackerServicePriv *priv;

	g_return_val_if_fail (TRACKER_IS_SERVICE (service), NULL);

	priv = GET_PRIV (service);

	return priv->parent;
}

const gchar *
tracker_service_get_property_prefix (TrackerService *service)
{
	TrackerServicePriv *priv;

	g_return_val_if_fail (TRACKER_IS_SERVICE (service), NULL);

	priv = GET_PRIV (service);

	return priv->property_prefix;
}

const gchar *
tracker_service_get_content_metadata (TrackerService *service)
{
	TrackerServicePriv *priv;

	g_return_val_if_fail (TRACKER_IS_SERVICE (service), NULL);

	priv = GET_PRIV (service);

	return priv->content_metadata;
}

const GSList *
tracker_service_get_key_metadata (TrackerService *service)
{
	TrackerServicePriv *priv;

	g_return_val_if_fail (TRACKER_IS_SERVICE (service), NULL);

	priv = GET_PRIV (service);

	return priv->key_metadata;
}

TrackerDBType
tracker_service_get_db_type (TrackerService *service)
{
	TrackerServicePriv *priv;

	g_return_val_if_fail (TRACKER_IS_SERVICE (service), TRACKER_DB_TYPE_DATA);

	priv = GET_PRIV (service);

	return priv->db_type;
}

gboolean
tracker_service_get_enabled (TrackerService *service)
{
	TrackerServicePriv *priv;

	g_return_val_if_fail (TRACKER_IS_SERVICE (service), FALSE);

	priv = GET_PRIV (service);

	return priv->enabled;
}

gboolean
tracker_service_get_embedded (TrackerService *service)
{
	TrackerServicePriv *priv;

	g_return_val_if_fail (TRACKER_IS_SERVICE (service), FALSE);

	priv = GET_PRIV (service);

	return priv->embedded;
}

gboolean
tracker_service_get_has_metadata (TrackerService *service)
{
	TrackerServicePriv *priv;

	g_return_val_if_fail (TRACKER_IS_SERVICE (service), FALSE);

	priv = GET_PRIV (service);

	return priv->has_metadata;
}

gboolean
tracker_service_get_has_full_text (TrackerService *service)
{
	TrackerServicePriv *priv;

	g_return_val_if_fail (TRACKER_IS_SERVICE (service), FALSE);

	priv = GET_PRIV (service);

	return priv->has_full_text;
}

gboolean
tracker_service_get_has_thumbs (TrackerService *service)
{
	TrackerServicePriv *priv;

	g_return_val_if_fail (TRACKER_IS_SERVICE (service), FALSE);

	priv = GET_PRIV (service);

	return priv->has_thumbs;
}

gboolean
tracker_service_get_show_service_files (TrackerService *service)
{
	TrackerServicePriv *priv;

	g_return_val_if_fail (TRACKER_IS_SERVICE (service), FALSE);

	priv = GET_PRIV (service);

	return priv->show_service_files;
}

gboolean
tracker_service_get_show_service_directories (TrackerService *service)
{
	TrackerServicePriv *priv;

	g_return_val_if_fail (TRACKER_IS_SERVICE (service), FALSE);

	priv = GET_PRIV (service);

	return priv->show_service_directories;
}

void
tracker_service_set_id (TrackerService *service,
			gint		value)
{
	TrackerServicePriv *priv;

	g_return_if_fail (TRACKER_IS_SERVICE (service));

	if (!service_int_validate (service, "id", value)) {
		return;
	}

	priv = GET_PRIV (service);

	priv->id = value;
	g_object_notify (G_OBJECT (service), "id");
}

void
tracker_service_set_name (TrackerService *service,
			  const gchar	 *value)
{
	TrackerServicePriv *priv;

	g_return_if_fail (TRACKER_IS_SERVICE (service));

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
tracker_service_set_parent (TrackerService *service,
			    const gchar    *value)
{
	TrackerServicePriv *priv;

	g_return_if_fail (TRACKER_IS_SERVICE (service));

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
tracker_service_set_property_prefix (TrackerService *service,
				     const gchar    *value)
{
	TrackerServicePriv *priv;

	g_return_if_fail (TRACKER_IS_SERVICE (service));

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
tracker_service_set_content_metadata (TrackerService *service,
				      const gchar    *value)
{
	TrackerServicePriv *priv;

	g_return_if_fail (TRACKER_IS_SERVICE (service));

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
tracker_service_set_key_metadata (TrackerService *service,
				  const GSList	 *value)
{
	TrackerServicePriv *priv;

	g_return_if_fail (TRACKER_IS_SERVICE (service));

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
tracker_service_set_db_type (TrackerService *service,
			     TrackerDBType   value)
{
	TrackerServicePriv *priv;

	g_return_if_fail (TRACKER_IS_SERVICE (service));

	priv = GET_PRIV (service);

	priv->db_type = value;
	g_object_notify (G_OBJECT (service), "db-type");
}

void
tracker_service_set_enabled (TrackerService *service,
			     gboolean	     value)
{
	TrackerServicePriv *priv;

	g_return_if_fail (TRACKER_IS_SERVICE (service));

	priv = GET_PRIV (service);

	priv->enabled = value;
	g_object_notify (G_OBJECT (service), "enabled");
}

void
tracker_service_set_embedded (TrackerService *service,
			      gboolean	      value)
{
	TrackerServicePriv *priv;

	g_return_if_fail (TRACKER_IS_SERVICE (service));

	priv = GET_PRIV (service);

	priv->embedded = value;
	g_object_notify (G_OBJECT (service), "embedded");
}

void
tracker_service_set_has_metadata (TrackerService *service,
				  gboolean	  value)
{
	TrackerServicePriv *priv;

	g_return_if_fail (TRACKER_IS_SERVICE (service));

	priv = GET_PRIV (service);

	priv->has_metadata = value;
	g_object_notify (G_OBJECT (service), "has-metadata");
}

void
tracker_service_set_has_full_text (TrackerService *service,
				   gboolean	   value)
{
	TrackerServicePriv *priv;

	g_return_if_fail (TRACKER_IS_SERVICE (service));

	priv = GET_PRIV (service);

	priv->has_full_text = value;
	g_object_notify (G_OBJECT (service), "has-full-text");
}

void
tracker_service_set_has_thumbs (TrackerService *service,
				gboolean	value)
{
	TrackerServicePriv *priv;

	g_return_if_fail (TRACKER_IS_SERVICE (service));

	priv = GET_PRIV (service);

	priv->has_thumbs = value;
	g_object_notify (G_OBJECT (service), "has-thumbs");
}

void
tracker_service_set_show_service_files (TrackerService *service,
					gboolean	value)
{
	TrackerServicePriv *priv;

	g_return_if_fail (TRACKER_IS_SERVICE (service));

	priv = GET_PRIV (service);

	priv->show_service_files = value;
	g_object_notify (G_OBJECT (service), "show-service-files");
}

void
tracker_service_set_show_service_directories (TrackerService *service,
					      gboolean	      value)
{
	TrackerServicePriv *priv;

	g_return_if_fail (TRACKER_IS_SERVICE (service));

	priv = GET_PRIV (service);

	priv->show_service_directories = value;
	g_object_notify (G_OBJECT (service), "show-service-directories");
}

