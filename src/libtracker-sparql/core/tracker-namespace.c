/*
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

#include "tracker-namespace.h"
#include "tracker-ontologies.h"

typedef struct _TrackerNamespacePrivate TrackerNamespacePrivate;

struct _TrackerNamespacePrivate {
	gchar *uri;

	gchar *prefix;
	TrackerOntologies *ontologies;
};

static void namespace_finalize     (GObject      *object);

G_DEFINE_TYPE_WITH_PRIVATE (TrackerNamespace, tracker_namespace, G_TYPE_OBJECT)

static void
tracker_namespace_class_init (TrackerNamespaceClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = namespace_finalize;
}

static void
tracker_namespace_init (TrackerNamespace *service)
{
}

static void
namespace_finalize (GObject *object)
{
	TrackerNamespacePrivate *priv;

	priv = tracker_namespace_get_instance_private (TRACKER_NAMESPACE (object));

	g_free (priv->uri);
	g_free (priv->prefix);

	(G_OBJECT_CLASS (tracker_namespace_parent_class)->finalize) (object);
}

TrackerNamespace *
tracker_namespace_new (gboolean use_gvdb)
{
	return g_object_new (TRACKER_TYPE_NAMESPACE, NULL);
}

const gchar *
tracker_namespace_get_uri (TrackerNamespace *namespace)
{
	TrackerNamespacePrivate *priv;

	g_return_val_if_fail (TRACKER_IS_NAMESPACE (namespace), NULL);

	priv = tracker_namespace_get_instance_private (namespace);

	return priv->uri;
}

const gchar *
tracker_namespace_get_prefix (TrackerNamespace *namespace)
{
	TrackerNamespacePrivate *priv;

	g_return_val_if_fail (TRACKER_IS_NAMESPACE (namespace), NULL);

	priv = tracker_namespace_get_instance_private (namespace);

	return priv->prefix;
}

void
tracker_namespace_set_uri (TrackerNamespace *namespace,
                           const gchar    *value)
{
	TrackerNamespacePrivate *priv;

	g_return_if_fail (TRACKER_IS_NAMESPACE (namespace));

	priv = tracker_namespace_get_instance_private (namespace);

	g_free (priv->uri);

	if (value) {
		priv->uri = g_strdup (value);
	} else {
		priv->uri = NULL;
	}
}

void
tracker_namespace_set_prefix (TrackerNamespace *namespace,
                              const gchar    *value)
{
	TrackerNamespacePrivate *priv;

	g_return_if_fail (TRACKER_IS_NAMESPACE (namespace));

	priv = tracker_namespace_get_instance_private (namespace);

	g_free (priv->prefix);

	if (value) {
		priv->prefix = g_strdup (value);
	} else {
		priv->prefix = NULL;
	}
}

void
tracker_namespace_set_ontologies (TrackerNamespace  *namespace,
                                  TrackerOntologies *ontologies)
{
	TrackerNamespacePrivate *priv;

	g_return_if_fail (TRACKER_IS_NAMESPACE (namespace));
	g_return_if_fail (ontologies != NULL);

	priv = tracker_namespace_get_instance_private (namespace);
	priv->ontologies = ontologies;
}
