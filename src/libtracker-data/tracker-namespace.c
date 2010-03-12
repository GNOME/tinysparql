/*
 * Copyright (C) 2008, Nokia (urho.konttori@nokia.com)
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

#include "tracker-namespace.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_NAMESPACE, TrackerNamespacePriv))

typedef struct _TrackerNamespacePriv TrackerNamespacePriv;

struct _TrackerNamespacePriv {
	gchar *uri;
	gchar *prefix;
	gboolean is_new;
};

static void namespace_finalize     (GObject      *object);
static void namespace_get_property (GObject      *object,
                                    guint         param_id,
                                    GValue       *value,
                                    GParamSpec   *pspec);
static void namespace_set_property (GObject      *object,
                                    guint         param_id,
                                    const GValue *value,
                                    GParamSpec   *pspec);

enum {
	PROP_0,
	PROP_URI,
	PROP_PREFIX,
	PROP_IS_NEW
};

G_DEFINE_TYPE (TrackerNamespace, tracker_namespace, G_TYPE_OBJECT);

static void
tracker_namespace_class_init (TrackerNamespaceClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize     = namespace_finalize;
	object_class->get_property = namespace_get_property;
	object_class->set_property = namespace_set_property;

	g_object_class_install_property (object_class,
	                                 PROP_URI,
	                                 g_param_spec_string ("uri",
	                                                      "uri",
	                                                      "URI",
	                                                      NULL,
	                                                      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_PREFIX,
	                                 g_param_spec_string ("prefix",
	                                                      "prefix",
	                                                      "Prefix",
	                                                      NULL,
	                                                      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_IS_NEW,
	                                 g_param_spec_boolean ("is-new",
	                                                       "is-new",
	                                                       "Set to TRUE when a new class or property is to be added to the database ontology",
	                                                       FALSE,
	                                                       G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof (TrackerNamespacePriv));
}

static void
tracker_namespace_init (TrackerNamespace *service)
{
}

static void
namespace_finalize (GObject *object)
{
	TrackerNamespacePriv *priv;

	priv = GET_PRIV (object);

	g_free (priv->uri);
	g_free (priv->prefix);

	(G_OBJECT_CLASS (tracker_namespace_parent_class)->finalize) (object);
}

static void
namespace_get_property (GObject          *object,
                        guint       param_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
	TrackerNamespacePriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_URI:
		g_value_set_string (value, priv->uri);
		break;
	case PROP_PREFIX:
		g_value_set_string (value, priv->prefix);
		break;
	case PROP_IS_NEW:
		g_value_set_boolean (value, priv->is_new);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
namespace_set_property (GObject            *object,
                        guint         param_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
	TrackerNamespacePriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_URI:
		tracker_namespace_set_uri (TRACKER_NAMESPACE (object),
		                           g_value_get_string (value));
		break;
	case PROP_PREFIX:
		tracker_namespace_set_prefix (TRACKER_NAMESPACE (object),
		                              g_value_get_string (value));
		break;
	case PROP_IS_NEW:
		tracker_namespace_set_is_new (TRACKER_NAMESPACE (object),
		                              g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

TrackerNamespace *
tracker_namespace_new (void)
{
	TrackerNamespace *namespace;

	namespace = g_object_new (TRACKER_TYPE_NAMESPACE, NULL);

	return namespace;
}

const gchar *
tracker_namespace_get_uri (TrackerNamespace *namespace)
{
	TrackerNamespacePriv *priv;

	g_return_val_if_fail (TRACKER_IS_NAMESPACE (namespace), NULL);

	priv = GET_PRIV (namespace);

	return priv->uri;
}

const gchar *
tracker_namespace_get_prefix (TrackerNamespace *namespace)
{
	TrackerNamespacePriv *priv;

	g_return_val_if_fail (TRACKER_IS_NAMESPACE (namespace), NULL);

	priv = GET_PRIV (namespace);

	return priv->prefix;
}

gboolean
tracker_namespace_get_is_new (TrackerNamespace *namespace)
{
	TrackerNamespacePriv *priv;

	g_return_val_if_fail (TRACKER_IS_NAMESPACE (namespace), FALSE);

	priv = GET_PRIV (namespace);

	return priv->is_new;
}

void
tracker_namespace_set_uri (TrackerNamespace *namespace,
                           const gchar    *value)
{
	TrackerNamespacePriv *priv;

	g_return_if_fail (TRACKER_IS_NAMESPACE (namespace));

	priv = GET_PRIV (namespace);

	g_free (priv->uri);

	if (value) {
		priv->uri = g_strdup (value);
	} else {
		priv->uri = NULL;
	}

	g_object_notify (G_OBJECT (namespace), "uri");
}

void
tracker_namespace_set_prefix (TrackerNamespace *namespace,
                              const gchar    *value)
{
	TrackerNamespacePriv *priv;

	g_return_if_fail (TRACKER_IS_NAMESPACE (namespace));

	priv = GET_PRIV (namespace);

	g_free (priv->prefix);

	if (value) {
		priv->prefix = g_strdup (value);
	} else {
		priv->prefix = NULL;
	}

	g_object_notify (G_OBJECT (namespace), "prefix");
}

void
tracker_namespace_set_is_new (TrackerNamespace *namespace,
                              gboolean          value)
{
	TrackerNamespacePriv *priv;

	g_return_if_fail (TRACKER_IS_NAMESPACE (namespace));

	priv = GET_PRIV (namespace);

	priv->is_new = value;
	g_object_notify (G_OBJECT (namespace), "is-new");
}
