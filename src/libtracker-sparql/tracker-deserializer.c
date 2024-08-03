/*
 * Copyright (C) 2022, Red Hat, Inc
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

#include "tracker-deserializer.h"
#include "tracker-deserializer-turtle.h"
#include "tracker-deserializer-json.h"
#include "tracker-deserializer-json-ld.h"
#include "tracker-deserializer-xml.h"

#include "tracker-private.h"

enum {
	PROP_0,
	PROP_STREAM,
	PROP_NAMESPACE_MANAGER,
	N_PROPS
};

static GParamSpec *props[N_PROPS];

typedef struct _TrackerDeserializerPrivate TrackerDeserializerPrivate;

struct _TrackerDeserializerPrivate
{
	GInputStream *stream;
	TrackerNamespaceManager *namespaces;
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (TrackerDeserializer, tracker_deserializer,
                                     TRACKER_TYPE_SPARQL_CURSOR)

static void
tracker_deserializer_finalize (GObject *object)
{
	TrackerDeserializer *deserializer = TRACKER_DESERIALIZER (object);
	TrackerDeserializerPrivate *priv =
		tracker_deserializer_get_instance_private (deserializer);

	g_clear_object (&priv->stream);
	g_clear_object (&priv->namespaces);

	G_OBJECT_CLASS (tracker_deserializer_parent_class)->finalize (object);
}
static void
tracker_deserializer_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
	TrackerDeserializer *deserializer = TRACKER_DESERIALIZER (object);
	TrackerDeserializerPrivate *priv =
		tracker_deserializer_get_instance_private (deserializer);

	switch (prop_id) {
	case PROP_STREAM:
		priv->stream = g_value_dup_object (value);
		break;
	case PROP_NAMESPACE_MANAGER:
		priv->namespaces = g_value_dup_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static gboolean
tracker_deserializer_is_bound (TrackerSparqlCursor *cursor,
                               gint                 column)
{
	return tracker_sparql_cursor_get_string (cursor, column, NULL) != NULL;
}

static void
tracker_deserializer_close (TrackerSparqlCursor *cursor)
{
	TrackerDeserializer *deserializer = TRACKER_DESERIALIZER (cursor);
	TrackerDeserializerPrivate *priv =
		tracker_deserializer_get_instance_private (deserializer);

	if (priv->stream)
		g_input_stream_close (priv->stream, NULL, NULL);
}

static void
tracker_deserializer_class_init (TrackerDeserializerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	TrackerSparqlCursorClass *cursor_class = TRACKER_SPARQL_CURSOR_CLASS (klass);

	object_class->finalize = tracker_deserializer_finalize;
	object_class->set_property = tracker_deserializer_set_property;

	cursor_class->is_bound = tracker_deserializer_is_bound;
	cursor_class->close = tracker_deserializer_close;

	props[PROP_STREAM] =
		g_param_spec_object ("stream",
		                     "Stream",
		                     "Stream",
		                     G_TYPE_INPUT_STREAM,
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
tracker_deserializer_init (TrackerDeserializer *deserializer)
{
}

TrackerSparqlCursor *
tracker_deserializer_new (GInputStream            *stream,
                          TrackerNamespaceManager *namespaces,
                          TrackerSerializerFormat  format)
{
	g_return_val_if_fail (G_IS_INPUT_STREAM (stream), NULL);

	switch (format) {
	case TRACKER_SERIALIZER_FORMAT_JSON:
		return tracker_deserializer_json_new (stream, namespaces);
	case TRACKER_SERIALIZER_FORMAT_XML:
		return tracker_deserializer_xml_new (stream, namespaces);
	case TRACKER_SERIALIZER_FORMAT_TTL:
		return tracker_deserializer_turtle_new (stream, namespaces);
	case TRACKER_SERIALIZER_FORMAT_TRIG:
		return tracker_deserializer_trig_new (stream, namespaces);
	case TRACKER_SERIALIZER_FORMAT_JSON_LD:
		return tracker_deserializer_json_ld_new (stream, namespaces);
	default:
		g_warn_if_reached ();
		return NULL;
	}
}

static TrackerSerializerFormat
pick_format_for_file (GFile *file)
{
	TrackerSerializerFormat format = TRACKER_SERIALIZER_FORMAT_TTL;
	TrackerRdfFormat rdf_format = 0;

	if (!tracker_rdf_format_pick_for_file (file, &rdf_format))
		return format;

	switch (rdf_format) {
	case TRACKER_RDF_FORMAT_TURTLE:
		format = TRACKER_SERIALIZER_FORMAT_TTL;
		break;
	case TRACKER_RDF_FORMAT_TRIG:
		format = TRACKER_SERIALIZER_FORMAT_TRIG;
		break;
	case TRACKER_RDF_FORMAT_JSON_LD:
		format = TRACKER_SERIALIZER_FORMAT_JSON_LD;
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	return format;
}

TrackerSparqlCursor *
tracker_deserializer_new_for_file (GFile                    *file,
                                   TrackerNamespaceManager  *namespaces,
                                   GError                  **error)
{
	TrackerSparqlCursor *deserializer;
	GInputStream *istream;
	TrackerSerializerFormat format;

	g_return_val_if_fail (G_IS_FILE (file), NULL);
	g_return_val_if_fail (!error || !*error, NULL);

	istream = G_INPUT_STREAM (g_file_read (file, NULL, error));
	if (!istream)
		return NULL;

	format = pick_format_for_file (file);
	deserializer = tracker_deserializer_new (istream, namespaces, format);
	g_object_unref (istream);

	return TRACKER_SPARQL_CURSOR (deserializer);
}

gboolean
tracker_deserializer_get_parser_location (TrackerDeserializer *deserializer,
                                          goffset             *line_no,
                                          goffset             *column_no)
{
	return TRACKER_DESERIALIZER_GET_CLASS (deserializer)->get_parser_location (deserializer,
	                                                                           line_no,
	                                                                           column_no);
}

GInputStream *
tracker_deserializer_get_stream (TrackerDeserializer *deserializer)
{
	TrackerDeserializerPrivate *priv =
		tracker_deserializer_get_instance_private (deserializer);

	return priv->stream;
}

TrackerNamespaceManager *
tracker_deserializer_get_namespaces (TrackerDeserializer *deserializer)
{
	TrackerDeserializerPrivate *priv =
		tracker_deserializer_get_instance_private (deserializer);

	if (!priv->namespaces)
		priv->namespaces = tracker_namespace_manager_new ();

	return priv->namespaces;
}
