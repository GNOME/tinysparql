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

/* Serialization of cursors to the JSON-LD format defined at:
 *  https://www.w3.org/TR/json-ld/
 */

#include "config.h"

#include "tracker-serializer-json-ld.h"

#include <json-glib/json-glib.h>

struct _TrackerSerializerJsonLD
{
	TrackerSerializer parent_instance;
	JsonGenerator *generator;
	JsonBuilder *builder;
	GHashTable *resources;
	GList *recent_resources;
	GString *data;
	GPtrArray *vars;
	gchar *cur_graph;
	gchar *cur_subject;
	JsonObject *cur_resource;
	guint stream_closed : 1;
	guint cursor_started : 1;
	guint cursor_finished : 1;
	guint context_printed : 1;
	guint needs_separator : 1;
};

G_DEFINE_TYPE (TrackerSerializerJsonLD, tracker_serializer_json_ld,
               TRACKER_TYPE_SERIALIZER)

#define MAX_CACHED_RESOURCES 50

static void
tracker_serializer_json_ld_finalize (GObject *object)
{
	TrackerSerializerJsonLD *serializer_json_ld =
		TRACKER_SERIALIZER_JSON_LD (object);

	g_input_stream_close (G_INPUT_STREAM (object), NULL, NULL);
	g_clear_pointer (&serializer_json_ld->cur_graph, g_free);
	g_clear_pointer (&serializer_json_ld->cur_subject, g_free);

	G_OBJECT_CLASS (tracker_serializer_json_ld_parent_class)->finalize (object);
}

static void
generate_namespace_foreach (gpointer key,
                            gpointer value,
                            gpointer user_data)
{
	JsonBuilder *builder = user_data;;

	json_builder_set_member_name (builder, key);
	json_builder_add_string_value (builder, value);
}

static void
finish_objects (TrackerSerializerJsonLD *serializer_json_ld)
{
	GHashTableIter iter;
	JsonNode *node;

	g_hash_table_iter_init (&iter, serializer_json_ld->resources);

	while (g_hash_table_iter_next (&iter, NULL, (gpointer*) &node)) {
		if (g_list_find (serializer_json_ld->recent_resources, node)) {
			if (serializer_json_ld->needs_separator)
				g_string_append (serializer_json_ld->data, ",\n");

			json_generator_set_root (serializer_json_ld->generator, node);
			json_generator_to_gstring (serializer_json_ld->generator,
			                           serializer_json_ld->data);
			serializer_json_ld->needs_separator = TRUE;
		}

		g_hash_table_iter_remove (&iter);
	}

	g_clear_list (&serializer_json_ld->recent_resources, NULL);
}

static JsonNode *
create_node (const gchar             *id,
             TrackerSparqlValueType   type,
             TrackerNamespaceManager *namespaces)
{
	JsonNode *node;
	JsonObject *object;

	node = json_node_new (JSON_NODE_OBJECT);
	object = json_object_new ();

	if (type == TRACKER_SPARQL_VALUE_TYPE_BLANK_NODE) {
		gchar *bnode_label;

		bnode_label = g_strconcat ("_:", id, NULL);
		g_strdelimit (&bnode_label[2], ":", '_');
		json_object_set_string_member (object, "@id", bnode_label);
		g_free (bnode_label);
	} else {
		gchar *compressed;

		compressed = tracker_namespace_manager_compress_uri (namespaces, id);

		if (compressed) {
			json_object_set_string_member (object, "@id", compressed);
			g_free (compressed);
		} else {
			json_object_set_string_member (object, "@id", id);
		}
	}

	json_node_set_object (node, object);
	json_object_unref (object);

	return node;
}

static JsonNode *
create_value_object (const gchar *value,
                     const gchar *langtag,
                     const gchar *datatype)
{
	JsonNode *node;
	JsonObject *object;

	node = json_node_new (JSON_NODE_OBJECT);
	object = json_object_new ();
	json_object_set_string_member (object, "@value", value);

	if (langtag)
		json_object_set_string_member (object, "@language", langtag);
	if (datatype)
		json_object_set_string_member (object, "@type", datatype);

	json_node_set_object (node, object);
	json_object_unref (object);

	return node;
}

static gboolean
serialize_up_to_position (TrackerSerializerJsonLD  *serializer_json_ld,
                          gsize                     pos,
                          GCancellable             *cancellable,
                          GError                  **error)
{
	TrackerSparqlCursor *cursor;
	TrackerNamespaceManager *namespaces;
	GError *inner_error = NULL;
	JsonNode *node;
	gboolean check_finish = FALSE;

	if (!serializer_json_ld->data)
		serializer_json_ld->data = g_string_new (NULL);
	if (!serializer_json_ld->generator)
		serializer_json_ld->generator = json_generator_new ();
	if (!serializer_json_ld->builder)
		serializer_json_ld->builder = json_builder_new ();
	if (!serializer_json_ld->vars)
		serializer_json_ld->vars = g_ptr_array_new_with_free_func (g_free);

	if (!serializer_json_ld->resources) {
		serializer_json_ld->resources = g_hash_table_new_full (g_str_hash, g_str_equal,
		                                                       g_free, (GDestroyNotify) json_node_unref);
	}

	if (pos < serializer_json_ld->data->len)
		return TRUE;

	cursor = tracker_serializer_get_cursor (TRACKER_SERIALIZER (serializer_json_ld));
	namespaces = tracker_serializer_get_namespaces (TRACKER_SERIALIZER (serializer_json_ld));

	if (!serializer_json_ld->context_printed) {
		JsonBuilder *builder;

		g_string_append (serializer_json_ld->data, "{");

		builder = json_builder_new ();
		json_builder_begin_object (builder);
		tracker_namespace_manager_foreach (namespaces,
		                                   generate_namespace_foreach,
		                                   builder);
		json_builder_end_object (builder);

		node = json_builder_get_root (builder);
		json_generator_set_root (serializer_json_ld->generator, node);
		g_object_unref (builder);
		json_node_unref (node);

		g_string_append (serializer_json_ld->data, "\"@context\":");
		json_generator_to_gstring (serializer_json_ld->generator,
		                           serializer_json_ld->data);

		g_string_append (serializer_json_ld->data,
		                 ",\"@graph\":[");

		serializer_json_ld->context_printed = TRUE;
	}

	while (!serializer_json_ld->cursor_finished) {
		const gchar *graph, *subject, *predicate, *langtag;
		gboolean graph_changed, subject_changed;
		TrackerSparqlValueType subject_type, object_type;
		JsonNode *value = NULL;
		gchar *prop = NULL;

		if (!tracker_sparql_cursor_next (cursor, cancellable, &inner_error)) {
			if (inner_error) {
				g_propagate_error (error, inner_error);
				return FALSE;
			} else {
				finish_objects (serializer_json_ld);
				serializer_json_ld->cursor_finished = TRUE;
				if (serializer_json_ld->cur_graph)
					g_string_append (serializer_json_ld->data, "]}");

				g_string_append (serializer_json_ld->data, "]}");

				return TRUE;
			}
		}

		subject = tracker_sparql_cursor_get_string (cursor, 0, NULL);
		subject_type = tracker_sparql_cursor_get_value_type (cursor, 0);
		predicate = tracker_sparql_cursor_get_string (cursor, 1, NULL);
		object_type = tracker_sparql_cursor_get_value_type (cursor, 2);
		graph = tracker_sparql_cursor_get_string (cursor, 3, NULL);

		graph_changed = g_strcmp0 (graph, serializer_json_ld->cur_graph) != 0;
		subject_changed = g_strcmp0 (subject, serializer_json_ld->cur_subject) != 0;

		if (graph_changed) {
			/* New/different graph */
			if (serializer_json_ld->cursor_started && graph_changed) {
				finish_objects (serializer_json_ld);
				if (serializer_json_ld->cur_graph)
					g_string_append (serializer_json_ld->data, "]}");

				g_string_append (serializer_json_ld->data, ",\n");
			}

			if (graph) {
				g_string_append_printf (serializer_json_ld->data,
				                        "{\"@id\":\"%s\",\"@graph\":[",
				                        graph);
			}

			g_clear_pointer (&serializer_json_ld->cur_graph, g_free);
			serializer_json_ld->cur_graph = g_strdup (graph);
			serializer_json_ld->needs_separator = FALSE;
		}

		if (subject_changed || graph_changed) {
			JsonObject *object;

			/* New/different subject */
			if (serializer_json_ld->cursor_started && subject_changed)
				check_finish = TRUE;

			if (g_hash_table_size (serializer_json_ld->resources) > MAX_CACHED_RESOURCES)
				finish_objects (serializer_json_ld);

			node = g_hash_table_lookup (serializer_json_ld->resources, subject);

			if (!node) {
				node = create_node (subject, subject_type, namespaces);

				g_hash_table_insert (serializer_json_ld->resources, g_strdup (subject), node);
				serializer_json_ld->recent_resources =
					g_list_prepend (serializer_json_ld->recent_resources, node);
			}

			object = json_node_get_object (node);
			serializer_json_ld->cur_resource = object;

			g_clear_pointer (&serializer_json_ld->cur_subject, g_free);
			serializer_json_ld->cur_subject = g_strdup (subject);
		}

		if (g_strcmp0 (predicate, TRACKER_PREFIX_RDF "type") == 0) {
			const gchar *type;
			gchar *compressed;

			type = tracker_sparql_cursor_get_string (cursor, 2, NULL);
			compressed = tracker_namespace_manager_compress_uri (namespaces, type);

			prop = g_strdup ("@type");

			value = json_node_alloc ();
			json_node_init_string (value, compressed);
			g_free (compressed);
		} else {
			TrackerNamespaceManager *namespaces;
			const gchar *res;

			namespaces = tracker_serializer_get_namespaces (TRACKER_SERIALIZER (serializer_json_ld));
			prop = tracker_namespace_manager_compress_uri (namespaces, predicate);

			switch (object_type) {
			case TRACKER_SPARQL_VALUE_TYPE_URI:
			case TRACKER_SPARQL_VALUE_TYPE_BLANK_NODE:
				res = tracker_sparql_cursor_get_string (cursor, 2, NULL);

				node = g_hash_table_lookup (serializer_json_ld->resources,
				                            res);

				if (node &&
				    serializer_json_ld->recent_resources->next != NULL &&
				    serializer_json_ld->cur_resource != json_node_get_object (node) &&
				    g_list_find (serializer_json_ld->recent_resources, node)) {
					/* This is still a "root" node, make it part of this tree */
					serializer_json_ld->recent_resources =
						g_list_remove (serializer_json_ld->recent_resources, node);
					value = json_node_ref (node);
				} else {
					/* Unknown object, or one already referenced elsewhere */
					value = create_node (res, object_type, namespaces);
				}
				break;
			case TRACKER_SPARQL_VALUE_TYPE_DATETIME:
				res = tracker_sparql_cursor_get_string (cursor, 2, NULL);
				value = create_value_object (res, NULL, TRACKER_PREFIX_XSD "dateTime");
				break;
			case TRACKER_SPARQL_VALUE_TYPE_STRING:
				res = tracker_sparql_cursor_get_langstring (cursor, 2, &langtag, NULL);
				if (langtag) {
					value = create_value_object (res, langtag, TRACKER_PREFIX_RDF "langString");
				} else {
					value = json_node_alloc ();
					json_node_init_string (value, res);
				}
				break;
			case TRACKER_SPARQL_VALUE_TYPE_INTEGER:
				value = json_node_alloc ();
				json_node_init_int (value, tracker_sparql_cursor_get_integer (cursor, 2));
				break;
			case TRACKER_SPARQL_VALUE_TYPE_DOUBLE:
				value = json_node_alloc ();
				json_node_init_double (value, tracker_sparql_cursor_get_double (cursor, 2));
				break;
			case TRACKER_SPARQL_VALUE_TYPE_BOOLEAN:
				value = json_node_alloc ();
				json_node_init_boolean (value, tracker_sparql_cursor_get_boolean (cursor, 2));
				break;
			case TRACKER_SPARQL_VALUE_TYPE_UNBOUND:
				break;
			}
		}

		if (prop && value) {
			JsonNode *prev;
			JsonArray *array;

			prev = json_object_get_member (serializer_json_ld->cur_resource,
			                               prop);

			if (!prev) {
				json_object_set_member (serializer_json_ld->cur_resource,
				                        prop, value);
			} else if (JSON_NODE_HOLDS_ARRAY (prev)) {
				array = json_node_get_array (prev);
				json_array_add_element (array, value);
			} else if (!json_node_equal (prev, value)) {
				array = json_array_new ();
				json_array_add_element (array, json_node_ref (prev));
				json_array_add_element (array, value);

				json_object_set_array_member (serializer_json_ld->cur_resource,
				                              prop, array);
			}
		}

		g_free (prop);
		serializer_json_ld->cursor_started = TRUE;

		if (check_finish && serializer_json_ld->data->len > pos)
			break;
	}

	return TRUE;
}

static gssize
tracker_serializer_json_ld_read (GInputStream  *istream,
                                 gpointer       buffer,
                                 gsize          count,
                                 GCancellable  *cancellable,
                                 GError       **error)
{
	TrackerSerializerJsonLD *serializer_json_ld = TRACKER_SERIALIZER_JSON_LD (istream);
	gsize bytes_copied;

	if (serializer_json_ld->stream_closed ||
	    (serializer_json_ld->cursor_finished &&
	     serializer_json_ld->data->len == 0))
		return 0;

	if (!serialize_up_to_position (serializer_json_ld,
	                               count,
	                               cancellable,
	                               error))
		return -1;

	bytes_copied = MIN (count, serializer_json_ld->data->len);

	memcpy (buffer,
	        serializer_json_ld->data->str,
	        bytes_copied);
	g_string_erase (serializer_json_ld->data, 0, bytes_copied);

	return bytes_copied;
}

static gboolean
tracker_serializer_json_ld_close (GInputStream  *istream,
                                  GCancellable  *cancellable,
                                  GError       **error)
{
	TrackerSerializerJsonLD *serializer_json_ld = TRACKER_SERIALIZER_JSON_LD (istream);

	if (serializer_json_ld->data) {
		g_string_free (serializer_json_ld->data, TRUE);
		serializer_json_ld->data = NULL;
	}

	g_clear_object (&serializer_json_ld->generator);
	g_clear_object (&serializer_json_ld->builder);
	serializer_json_ld->stream_closed = TRUE;
	g_clear_pointer (&serializer_json_ld->vars, g_ptr_array_unref);

	return TRUE;
}

static void
tracker_serializer_json_ld_class_init (TrackerSerializerJsonLDClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GInputStreamClass *istream_class = G_INPUT_STREAM_CLASS (klass);

	object_class->finalize = tracker_serializer_json_ld_finalize;

	istream_class->read_fn = tracker_serializer_json_ld_read;
	istream_class->close_fn = tracker_serializer_json_ld_close;
}

static void
tracker_serializer_json_ld_init (TrackerSerializerJsonLD *serializer)
{
}
