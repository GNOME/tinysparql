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

/* Serialization of cursors to the JSON format defined at:
 *  https://www.w3.org/TR/sparql11-results-json/
 */

#include "config.h"

#include "tracker-serializer-json.h"

#include <json-glib/json-glib.h>

struct _TrackerSerializerJson
{
	TrackerSerializer parent_instance;
	JsonGenerator *generator;
	GString *data;
	GPtrArray *vars;
	gsize current_pos;
	guint stream_closed : 1;
	guint cursor_started : 1;
	guint cursor_finished : 1;
	guint head_printed : 1;
};

G_DEFINE_TYPE (TrackerSerializerJson, tracker_serializer_json,
               TRACKER_TYPE_SERIALIZER)

static void
tracker_serializer_json_finalize (GObject *object)
{
	g_input_stream_close (G_INPUT_STREAM (object), NULL, NULL);

	G_OBJECT_CLASS (tracker_serializer_json_parent_class)->finalize (object);
}

static gboolean
serialize_up_to_position (TrackerSerializerJson  *serializer_json,
                          gsize                   pos,
                          GCancellable           *cancellable,
                          GError                **error)
{
	TrackerSparqlCursor *cursor;
	GError *inner_error = NULL;
	JsonBuilder *builder;
	JsonNode *node;
	gint i;

	if (!serializer_json->data)
		serializer_json->data = g_string_new (NULL);
	if (!serializer_json->generator)
		serializer_json->generator = json_generator_new ();
	if (!serializer_json->vars)
		serializer_json->vars = g_ptr_array_new_with_free_func (g_free);

	cursor = tracker_serializer_get_cursor (TRACKER_SERIALIZER (serializer_json));
	builder = json_builder_new ();

	if (!serializer_json->head_printed) {
		json_builder_reset (builder);
		json_builder_begin_object (builder);
		json_builder_set_member_name (builder, "vars");
		json_builder_begin_array (builder);

		for (i = 0; i < tracker_sparql_cursor_get_n_columns (cursor); i++) {
			const gchar *var;

			var = tracker_sparql_cursor_get_variable_name (cursor, i);

			if (var && *var) {
				g_ptr_array_add (serializer_json->vars,
				                 g_strdup (var));
			} else {
				g_ptr_array_add (serializer_json->vars,
				                 g_strdup_printf ("var%d", i + 1));
			}

			json_builder_add_string_value (builder,
			                               g_ptr_array_index (serializer_json->vars, i));
		}

		json_builder_end_array (builder);
		json_builder_end_object (builder);

		node = json_builder_get_root (builder);

		g_string_append_printf (serializer_json->data,
		                        "{\"head\":");
		json_generator_set_root (serializer_json->generator, node);
		json_generator_to_gstring (serializer_json->generator,
		                           serializer_json->data);
		g_string_append_printf (serializer_json->data,
		                        ",\"results\":{\"bindings\":[");

		serializer_json->head_printed = TRUE;
	}

	while (!serializer_json->cursor_finished &&
	       serializer_json->data->len < pos) {
		if (!tracker_sparql_cursor_next (cursor, cancellable, &inner_error)) {
			if (inner_error) {
				g_propagate_error (error, inner_error);
				g_clear_object (&builder);
				return FALSE;
			} else {
				serializer_json->cursor_finished = TRUE;
				g_string_append (serializer_json->data, "]}}");
				break;
			}
		} else {
			if (serializer_json->cursor_started)
				g_string_append_c (serializer_json->data, ',');

			serializer_json->cursor_started = TRUE;
		}

		json_builder_reset (builder);
		json_builder_begin_object (builder);

		for (i = 0; i < tracker_sparql_cursor_get_n_columns (cursor); i++) {
			const gchar *var, *str, *type = NULL, *datatype = NULL, *langtag = NULL;

			switch (tracker_sparql_cursor_get_value_type (cursor, i)) {
			case TRACKER_SPARQL_VALUE_TYPE_URI:
				type = "uri";
				break;
			case TRACKER_SPARQL_VALUE_TYPE_STRING:
				type = "literal";
				datatype = TRACKER_PREFIX_XSD "string";
				break;
			case TRACKER_SPARQL_VALUE_TYPE_INTEGER:
				type = "literal";
				datatype = TRACKER_PREFIX_XSD "integer";
				break;
			case TRACKER_SPARQL_VALUE_TYPE_BOOLEAN:
				type = "literal";
				datatype = TRACKER_PREFIX_XSD "boolean";
				break;
			case TRACKER_SPARQL_VALUE_TYPE_DOUBLE:
				type = "literal";
				datatype = TRACKER_PREFIX_XSD "double";
				break;
			case TRACKER_SPARQL_VALUE_TYPE_DATETIME:
				type = "literal";
				datatype = TRACKER_PREFIX_XSD "dateTime";
				break;
			case TRACKER_SPARQL_VALUE_TYPE_BLANK_NODE:
				type = "bnode";
				break;
			case TRACKER_SPARQL_VALUE_TYPE_UNBOUND:
                                continue;
			}

			var = g_ptr_array_index (serializer_json->vars, i);
			json_builder_set_member_name (builder, var);

			json_builder_begin_object (builder);

			json_builder_set_member_name (builder, "type");
			json_builder_add_string_value (builder, type);

			str = tracker_sparql_cursor_get_langstring (cursor, i, &langtag, NULL);

			if (langtag) {
				datatype = TRACKER_PREFIX_RDF "langString";
				json_builder_set_member_name (builder, "xml:lang");
				json_builder_add_string_value (builder, langtag);
			}

			if (datatype) {
				json_builder_set_member_name (builder, "datatype");
				json_builder_add_string_value (builder, datatype);
			}

			if (str) {
				json_builder_set_member_name (builder, "value");
				json_builder_add_string_value (builder,
				                               tracker_sparql_cursor_get_string (cursor, i, NULL));
				json_builder_end_object (builder);
			}
		}

		json_builder_end_object (builder);
		node = json_builder_get_root (builder);

		json_generator_set_root (serializer_json->generator, node);
		json_generator_to_gstring (serializer_json->generator,
		                           serializer_json->data);
	}

	g_clear_object (&builder);

	return TRUE;
}

static gssize
tracker_serializer_json_read (GInputStream  *istream,
                              gpointer       buffer,
                              gsize          count,
                              GCancellable  *cancellable,
                              GError       **error)
{
	TrackerSerializerJson *serializer_json = TRACKER_SERIALIZER_JSON (istream);
	gsize bytes_unflushed, bytes_copied;

	if (serializer_json->stream_closed ||
	    (serializer_json->cursor_finished &&
	     serializer_json->current_pos == serializer_json->data->len))
		return 0;

	if (!serialize_up_to_position (serializer_json,
	                               serializer_json->current_pos + count,
	                               cancellable,
	                               error))
		return -1;

	bytes_unflushed =
		serializer_json->data->len - serializer_json->current_pos;
	bytes_copied = MIN (count, bytes_unflushed);

	memcpy (buffer,
	        &serializer_json->data->str[serializer_json->current_pos],
	        bytes_copied);
	serializer_json->current_pos += bytes_copied;

	return bytes_copied;
}

static gboolean
tracker_serializer_json_close (GInputStream  *istream,
                               GCancellable  *cancellable,
                               GError       **error)
{
	TrackerSerializerJson *serializer_json = TRACKER_SERIALIZER_JSON (istream);

	if (serializer_json->data) {
		g_string_free (serializer_json->data, TRUE);
		serializer_json->data = NULL;
	}

	g_clear_object (&serializer_json->generator);
	serializer_json->stream_closed = TRUE;
	g_clear_pointer (&serializer_json->vars, g_ptr_array_unref);

	return TRUE;
}

static void
tracker_serializer_json_class_init (TrackerSerializerJsonClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GInputStreamClass *istream_class = G_INPUT_STREAM_CLASS (klass);

	object_class->finalize = tracker_serializer_json_finalize;

	istream_class->read_fn = tracker_serializer_json_read;
	istream_class->close_fn = tracker_serializer_json_close;
}

static void
tracker_serializer_json_init (TrackerSerializerJson *serializer)
{
}
