/*
 * Copyright (C) 2022, Red Hat Inc.
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

/* Deserialization of a (tree of) TrackerResource into a cursor */

#include "config.h"

#include <tracker-common.h>

#include "tracker-deserializer-resource.h"

#include "tracker-private.h"

#include "tracker-uri.h"

enum {
	PROP_0,
	PROP_RESOURCE,
	PROP_GRAPH,
	N_PROPS,
};

static GParamSpec *props[N_PROPS];

typedef struct {
	TrackerResource *resource;
	TrackerResourceIterator iter;
	const gchar *cur_property;
	const GValue *cur_value;
	gchar *expanded_subject;
	gchar *expanded_property;
	gchar *value_as_string;
} ResourceStack;

struct _TrackerDeserializerResource {
	TrackerDeserializerRdf parent;
	TrackerResource *resource;
	GArray *iterators;
	GHashTable *visited;
	gchar *graph;
	gchar *expanded_graph;
};

G_DEFINE_TYPE (TrackerDeserializerResource, tracker_deserializer_resource,
	       TRACKER_TYPE_DESERIALIZER_RDF)

static void
tracker_deserializer_resource_set_property (GObject      *object,
                                            guint         prop_id,
                                            const GValue *value,
                                            GParamSpec   *pspec)
{
	TrackerDeserializerResource *deserializer =
		TRACKER_DESERIALIZER_RESOURCE (object);

	switch (prop_id) {
	case PROP_RESOURCE:
		g_clear_object (&deserializer->resource);
		deserializer->resource = g_value_dup_object (value);
		break;
	case PROP_GRAPH:
		g_clear_object (&deserializer->graph);
		deserializer->graph = g_value_dup_string (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
tracker_deserializer_resource_finalize (GObject *object)
{
	TrackerDeserializerResource *deserializer =
		TRACKER_DESERIALIZER_RESOURCE (object);

	g_clear_object (&deserializer->resource);
	g_hash_table_unref (deserializer->visited);
	g_array_unref (deserializer->iterators);
	g_clear_pointer (&deserializer->graph, g_free);
	g_clear_pointer (&deserializer->expanded_graph, g_free);

	G_OBJECT_CLASS (tracker_deserializer_resource_parent_class)->finalize (object);
}

static gchar *
expand_uri (TrackerDeserializerResource *deserializer,
            const gchar                 *uri)
{
	TrackerNamespaceManager *namespaces;

	if (strncmp (uri, "_:", 2) == 0)
		return g_strdup (uri);

	namespaces = tracker_deserializer_get_namespaces (TRACKER_DESERIALIZER (deserializer));

	return tracker_namespace_manager_expand_uri (namespaces, uri);
}

static void
push_stack (TrackerDeserializerResource *deserializer,
            TrackerResource             *resource)
{
	ResourceStack item = { 0, };

	item.resource = resource;
	item.expanded_subject =
		expand_uri (deserializer, tracker_resource_get_identifier (resource));
	tracker_resource_iterator_init (&item.iter, item.resource);
	g_array_append_val (deserializer->iterators, item);
	g_hash_table_add (deserializer->visited, item.resource);
}

static ResourceStack *
peek_stack (TrackerDeserializerResource *deserializer)
{
	if (deserializer->iterators->len == 0)
		return NULL;

	return &g_array_index (deserializer->iterators, ResourceStack,
	                       deserializer->iterators->len - 1);
}

static void
pop_stack (TrackerDeserializerResource *deserializer)
{
	if (deserializer->iterators->len == 0)
		return;

	g_array_set_size (deserializer->iterators,
	                  deserializer->iterators->len - 1);
}

static void
tracker_deserializer_resource_constructed (GObject *object)
{
	TrackerDeserializerResource *deserializer =
		TRACKER_DESERIALIZER_RESOURCE (object);

	G_OBJECT_CLASS (tracker_deserializer_resource_parent_class)->constructed (object);

	if (deserializer->graph)
		deserializer->expanded_graph = expand_uri (deserializer, deserializer->graph);

	push_stack (deserializer, deserializer->resource);
}

static TrackerSparqlValueType
value_type_from_gtype (const GValue *value)
{
	if (G_VALUE_HOLDS (value, TRACKER_TYPE_RESOURCE)) {
		TrackerResource *resource;

		resource = g_value_get_object (value);

		if (strncmp (tracker_resource_get_identifier (resource),
		             "_:", 2) == 0)
			return TRACKER_SPARQL_VALUE_TYPE_BLANK_NODE;
		else
			return TRACKER_SPARQL_VALUE_TYPE_URI;
	} else if (G_VALUE_HOLDS (value, TRACKER_TYPE_URI)) {
		const gchar *uri = g_value_get_string (value);
		if (g_str_has_prefix (uri, "_:"))
			return TRACKER_SPARQL_VALUE_TYPE_BLANK_NODE;
		else
			return TRACKER_SPARQL_VALUE_TYPE_URI;
	} else if (G_VALUE_HOLDS (value, G_TYPE_STRING)) {
		return TRACKER_SPARQL_VALUE_TYPE_STRING;
	} else if (G_VALUE_HOLDS (value, G_TYPE_BOOLEAN)) {
		return TRACKER_SPARQL_VALUE_TYPE_BOOLEAN;
	} else if (G_VALUE_HOLDS (value, G_TYPE_INT) ||
	           G_VALUE_HOLDS (value, G_TYPE_UINT) ||
	           G_VALUE_HOLDS (value, G_TYPE_INT64)) {
		return TRACKER_SPARQL_VALUE_TYPE_INTEGER;
	} else if (G_VALUE_HOLDS (value, G_TYPE_DOUBLE)) {
		return TRACKER_SPARQL_VALUE_TYPE_DOUBLE;
	} else if (G_VALUE_HOLDS (value, G_TYPE_DATE_TIME)) {
		return TRACKER_SPARQL_VALUE_TYPE_DATETIME;
	}

	return TRACKER_SPARQL_VALUE_TYPE_UNBOUND;
}

static TrackerSparqlValueType
tracker_deserializer_resource_get_value_type (TrackerSparqlCursor *cursor,
                                              gint                 col)
{
	TrackerDeserializerResource *deserializer =
		TRACKER_DESERIALIZER_RESOURCE (cursor);
	ResourceStack *item;

	item = peek_stack (deserializer);
	if (!item)
		return TRACKER_SPARQL_VALUE_TYPE_UNBOUND;

	switch (col) {
	case TRACKER_RDF_COL_SUBJECT:
		if (strncmp (tracker_resource_get_identifier (item->resource),
		             "_:", 2) == 0)
			return TRACKER_SPARQL_VALUE_TYPE_BLANK_NODE;
		else
			return TRACKER_SPARQL_VALUE_TYPE_URI;
		break;
	case TRACKER_RDF_COL_PREDICATE:
		return TRACKER_SPARQL_VALUE_TYPE_URI;
		break;
	case TRACKER_RDF_COL_OBJECT:
		return value_type_from_gtype (item->cur_value);
		break;
	case TRACKER_RDF_COL_GRAPH:
		return deserializer->graph ?
			TRACKER_SPARQL_VALUE_TYPE_URI : TRACKER_SPARQL_VALUE_TYPE_UNBOUND;
		break;
	default:
		return TRACKER_SPARQL_VALUE_TYPE_UNBOUND;
		break;
	}
}

static const gchar *
tracker_deserializer_resource_get_string (TrackerSparqlCursor  *cursor,
                                          gint                  col,
                                          const gchar         **langtag,
                                          glong                *length)
{
	TrackerDeserializerResource *deserializer =
		TRACKER_DESERIALIZER_RESOURCE (cursor);
	const gchar *str = NULL;
	ResourceStack *item;

	if (length)
		*length = 0;
	if (langtag)
		*langtag = NULL;

	item = peek_stack (deserializer);
	if (!item)
		return NULL;

	switch (col) {
	case TRACKER_RDF_COL_SUBJECT:
		str = item->expanded_subject;
		break;
	case TRACKER_RDF_COL_PREDICATE:
		str = item->expanded_property;
		break;
	case TRACKER_RDF_COL_OBJECT:
		str = item->value_as_string;
		break;
	case TRACKER_RDF_COL_GRAPH:
		str = deserializer->expanded_graph;
		break;
	default:
		break;
	}

	if (!str)
		return NULL;

	if (length)
		*length = strlen (str);

	return str;
}

static gchar *
convert_gvalue_to_string (TrackerDeserializerResource *deserializer,
                          const GValue                *value)
{
	if (G_VALUE_HOLDS (value, TRACKER_TYPE_RESOURCE)) {
		TrackerResource *resource;

		resource = g_value_get_object (value);
		return expand_uri (deserializer,
		                   tracker_resource_get_identifier (resource));
	} else if (G_VALUE_HOLDS (value, G_TYPE_STRING) ||
	           G_VALUE_HOLDS (value, TRACKER_TYPE_URI)) {
		return expand_uri (deserializer, g_value_get_string (value));
	} else if (G_VALUE_HOLDS (value, G_TYPE_BOOLEAN)) {
		gboolean val;

		val = g_value_get_boolean (value);
		return g_strdup (val ? "true" : "false");
	} else if (G_VALUE_HOLDS (value, G_TYPE_INT)) {
		gint val;

		val = g_value_get_int (value);
		return g_strdup_printf ("%d", val);
	} else if (G_VALUE_HOLDS (value, G_TYPE_UINT)) {
		guint val;

		val = g_value_get_uint (value);
		return g_strdup_printf ("%u", val);
	} else if (G_VALUE_HOLDS (value, G_TYPE_INT64)) {
		gint64 val;

		val = g_value_get_int64 (value);
		return g_strdup_printf ("%" G_GINT64_FORMAT, val);
	} else if (G_VALUE_HOLDS (value, G_TYPE_DOUBLE)) {
		gchar *buf;
		gdouble val;

		buf = g_new0 (gchar, G_ASCII_DTOSTR_BUF_SIZE);
		val = g_value_get_double (value);
		return g_ascii_dtostr (buf, G_ASCII_DTOSTR_BUF_SIZE, val);
	} else if (G_VALUE_HOLDS (value, G_TYPE_DATE_TIME)) {
		GDateTime *val;

		val = g_value_get_boxed (value);
		return tracker_date_format_iso8601 (val);
	}

	return NULL;
}

static gboolean
tracker_deserializer_resource_next (TrackerSparqlCursor  *cursor,
                                    GCancellable         *cancellable,
                                    GError              **error)
{
	TrackerDeserializerResource *deserializer =
		TRACKER_DESERIALIZER_RESOURCE (cursor);
	ResourceStack *item;

 retry:
	item = peek_stack (deserializer);
	if (!item)
		return FALSE;

	g_clear_pointer (&item->expanded_property, g_free);
	g_clear_pointer (&item->value_as_string, g_free);

	if (tracker_resource_iterator_next (&item->iter,
	                                    &item->cur_property,
	                                    &item->cur_value)) {
		item->expanded_property =
			expand_uri (deserializer, item->cur_property);
		item->value_as_string =
			convert_gvalue_to_string (deserializer, item->cur_value);

		if (G_VALUE_HOLDS (item->cur_value, TRACKER_TYPE_RESOURCE)) {
			TrackerResource *child;

			/* Since the value extracted is a new resource, iterate
			 * through it first before handling this property/value
			 * on the current resource.
			 */
			child = g_value_get_object (item->cur_value);
			if (!g_hash_table_contains (deserializer->visited, child)) {
				push_stack (deserializer, child);
				goto retry;
			}
		}

		return TRUE;
	} else {
		pop_stack (deserializer);
		item = peek_stack (deserializer);
		/* We already fetched a property/value before pushing */
		return item != NULL;
	}
}

static void
tracker_deserializer_resource_class_init (TrackerDeserializerResourceClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	TrackerSparqlCursorClass *cursor_class =
		TRACKER_SPARQL_CURSOR_CLASS (klass);

	object_class->set_property = tracker_deserializer_resource_set_property;
	object_class->finalize = tracker_deserializer_resource_finalize;
	object_class->constructed = tracker_deserializer_resource_constructed;

	cursor_class->get_value_type = tracker_deserializer_resource_get_value_type;
	cursor_class->get_string = tracker_deserializer_resource_get_string;
	cursor_class->next = tracker_deserializer_resource_next;

	props[PROP_RESOURCE] =
		g_param_spec_object ("resource",
		                     "Resource",
		                     "Resource",
		                     TRACKER_TYPE_RESOURCE,
		                     G_PARAM_CONSTRUCT_ONLY |
		                     G_PARAM_STATIC_STRINGS |
		                     G_PARAM_WRITABLE);
	props[PROP_GRAPH] =
		g_param_spec_string ("graph",
		                     "Graph",
		                     "Graph",
		                     NULL,
		                     G_PARAM_CONSTRUCT_ONLY |
		                     G_PARAM_STATIC_STRINGS |
		                     G_PARAM_WRITABLE);

	g_object_class_install_properties (object_class, N_PROPS, props);
}

static void
clear_stack (gpointer user_data)
{
	ResourceStack *item = user_data;

	g_clear_pointer (&item->expanded_subject, g_free);
	g_clear_pointer (&item->expanded_property, g_free);
	g_clear_pointer (&item->value_as_string, g_free);
}

static void
tracker_deserializer_resource_init (TrackerDeserializerResource *deserializer)
{
	deserializer->iterators = g_array_new (FALSE, FALSE, sizeof (ResourceStack));
	g_array_set_clear_func (deserializer->iterators, clear_stack);
	deserializer->visited = g_hash_table_new (NULL, NULL);
}

TrackerSparqlCursor *
tracker_deserializer_resource_new (TrackerResource         *resource,
                                   TrackerNamespaceManager *namespaces,
                                   const gchar             *graph)
{
	return g_object_new (TRACKER_TYPE_DESERIALIZER_RESOURCE,
	                     "resource", resource,
	                     "namespace-manager", namespaces,
	                     "has-graph", graph != NULL,
	                     "graph", graph,
	                     NULL);
}
