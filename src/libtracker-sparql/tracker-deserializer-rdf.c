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

#include "tracker-deserializer-rdf.h"

#include "tracker-private.h"

static gchar *col_names[] = {
	"subject",
	"predicate",
	"object",
	"graph",
};

G_STATIC_ASSERT (G_N_ELEMENTS (col_names) == TRACKER_RDF_N_COLS);

enum {
	PROP_0,
	PROP_HAS_GRAPH,
	N_PROPS
};

static GParamSpec *props[N_PROPS];

typedef struct _TrackerDeserializerRdfPrivate TrackerDeserializerRdfPrivate;

struct _TrackerDeserializerRdfPrivate
{
	gboolean has_graph;
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (TrackerDeserializerRdf, tracker_deserializer_rdf,
                                     TRACKER_TYPE_DESERIALIZER)

static void
tracker_deserializer_rdf_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
	TrackerDeserializerRdf *serializer_rdf = TRACKER_DESERIALIZER_RDF (object);
	TrackerDeserializerRdfPrivate *priv =
		tracker_deserializer_rdf_get_instance_private (serializer_rdf);

	switch (prop_id) {
	case PROP_HAS_GRAPH:
		priv->has_graph = g_value_get_boolean (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
tracker_deserializer_rdf_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
	TrackerDeserializerRdf *serializer_rdf = TRACKER_DESERIALIZER_RDF (object);
	TrackerDeserializerRdfPrivate *priv =
		tracker_deserializer_rdf_get_instance_private (serializer_rdf);

	switch (prop_id) {
	case PROP_HAS_GRAPH:
		g_value_set_boolean (value, priv->has_graph);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static const gchar *
tracker_deserializer_rdf_get_variable_name (TrackerSparqlCursor *cursor,
                                            gint                 column)
{
	if (column >= tracker_sparql_cursor_get_n_columns (cursor))
		return NULL;

	g_assert (column < TRACKER_RDF_N_COLS);

	return col_names[column];
}

static gint
tracker_deserializer_rdf_get_n_columns (TrackerSparqlCursor *cursor)
{
	TrackerDeserializerRdf *deserializer_rdf =
		TRACKER_DESERIALIZER_RDF (cursor);
	TrackerDeserializerRdfPrivate *priv =
		tracker_deserializer_rdf_get_instance_private (deserializer_rdf);

	return (priv->has_graph) ? TRACKER_RDF_N_COLS : TRACKER_RDF_N_COLS - 1;
}

static void
tracker_deserializer_rdf_class_init (TrackerDeserializerRdfClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	TrackerSparqlCursorClass *cursor_class = TRACKER_SPARQL_CURSOR_CLASS (klass);

	object_class->set_property = tracker_deserializer_rdf_set_property;
	object_class->get_property = tracker_deserializer_rdf_get_property;

	cursor_class->get_variable_name = tracker_deserializer_rdf_get_variable_name;
	cursor_class->get_n_columns = tracker_deserializer_rdf_get_n_columns;

	props[PROP_HAS_GRAPH] =
		g_param_spec_boolean ("has-graph",
		                      "Has graph",
		                      "Has graph",
		                      FALSE,
		                      G_PARAM_CONSTRUCT_ONLY |
		                      G_PARAM_STATIC_STRINGS |
		                      G_PARAM_READABLE |
		                      G_PARAM_WRITABLE);

	g_object_class_install_properties (object_class, N_PROPS, props);
}

static void
tracker_deserializer_rdf_init (TrackerDeserializerRdf *deserializer_rdf)
{
}
