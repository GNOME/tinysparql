/*
 * Copyright (C) 2020, Red Hat, Inc
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
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#include "config.h"

#include "tracker-serializer.h"
#include "tracker-serializer-json.h"
#include "tracker-serializer-json-ld.h"
#include "tracker-serializer-trig.h"
#include "tracker-serializer-turtle.h"
#include "tracker-serializer-xml.h"

#include "tracker-private.h"

enum {
	PROP_0,
	PROP_CURSOR,
	PROP_NAMESPACE_MANAGER,
	N_PROPS
};

static GParamSpec *props[N_PROPS];

typedef struct _TrackerSerializerPrivate TrackerSerializerPrivate;

struct _TrackerSerializerPrivate
{
	TrackerSparqlCursor *cursor;
	TrackerNamespaceManager *namespaces;
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (TrackerSerializer, tracker_serializer,
                                     G_TYPE_INPUT_STREAM)

static void
tracker_serializer_finalize (GObject *object)
{
	TrackerSerializer *serializer = TRACKER_SERIALIZER (object);
	TrackerSerializerPrivate *priv =
		tracker_serializer_get_instance_private (serializer);

	g_object_unref (priv->cursor);
	g_object_unref (priv->namespaces);

	G_OBJECT_CLASS (tracker_serializer_parent_class)->finalize (object);
}
static void
tracker_serializer_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
	TrackerSerializer *serializer = TRACKER_SERIALIZER (object);
	TrackerSerializerPrivate *priv =
		tracker_serializer_get_instance_private (serializer);

	switch (prop_id) {
	case PROP_CURSOR:
		priv->cursor = g_value_dup_object (value);
		break;
	case PROP_NAMESPACE_MANAGER:
		priv->namespaces = g_value_dup_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
tracker_serializer_class_init (TrackerSerializerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_serializer_finalize;
	object_class->set_property = tracker_serializer_set_property;

	props[PROP_CURSOR] =
		g_param_spec_object ("cursor",
		                     "cursor",
		                     "cursor",
		                     TRACKER_TYPE_SPARQL_CURSOR,
		                     G_PARAM_CONSTRUCT_ONLY |
		                     G_PARAM_STATIC_STRINGS |
		                     G_PARAM_WRITABLE);
	props[PROP_NAMESPACE_MANAGER] =
		g_param_spec_object ("namespace-manager",
		                     "Namespace Manager",
		                     "Namespace Manager",
		                     TRACKER_TYPE_NAMESPACE_MANAGER,
		                     G_PARAM_CONSTRUCT_ONLY |
		                     G_PARAM_STATIC_STRINGS |
		                     G_PARAM_WRITABLE);

	g_object_class_install_properties (object_class, N_PROPS, props);
}

static void
tracker_serializer_init (TrackerSerializer *serializer)
{
}

GInputStream *
tracker_serializer_new (TrackerSparqlCursor     *cursor,
                        TrackerNamespaceManager *namespaces,
                        TrackerSerializerFormat  format)
{
	GType type;

	g_return_val_if_fail (TRACKER_IS_SPARQL_CURSOR (cursor), NULL);

	switch (format) {
	case TRACKER_SERIALIZER_FORMAT_JSON:
		type = TRACKER_TYPE_SERIALIZER_JSON;
		break;
	case TRACKER_SERIALIZER_FORMAT_XML:
		type = TRACKER_TYPE_SERIALIZER_XML;
		break;
	case TRACKER_SERIALIZER_FORMAT_TTL:
		type = TRACKER_TYPE_SERIALIZER_TURTLE;
		break;
	case TRACKER_SERIALIZER_FORMAT_TRIG:
		type = TRACKER_TYPE_SERIALIZER_TRIG;
		break;
	case TRACKER_SERIALIZER_FORMAT_JSON_LD:
		type = TRACKER_TYPE_SERIALIZER_JSON_LD;
		break;
	default:
		g_warn_if_reached ();
		return NULL;
	}

	return g_object_new (type,
	                     "cursor", cursor,
	                     "namespace-manager", namespaces,
	                     NULL);
}

TrackerSparqlCursor *
tracker_serializer_get_cursor (TrackerSerializer *serializer)
{
	TrackerSerializerPrivate *priv =
		tracker_serializer_get_instance_private (serializer);

	g_return_val_if_fail (TRACKER_IS_SERIALIZER (serializer), NULL);

	return priv->cursor;
}

TrackerNamespaceManager *
tracker_serializer_get_namespaces (TrackerSerializer *serializer)
{
	TrackerSerializerPrivate *priv =
		tracker_serializer_get_instance_private (serializer);

	g_return_val_if_fail (TRACKER_IS_SERIALIZER (serializer), NULL);

	return priv->namespaces;
}
