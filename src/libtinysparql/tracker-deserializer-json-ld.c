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

/* Deserialization to cursors for the JSON format defined at:
 *  https://www.w3.org/TR/json-ld/
 */

#include "config.h"

#include "tracker-deserializer-json-ld.h"

#include <json-glib/json-glib.h>

enum {
	STATE_INITIAL,
	STATE_ROOT_LIST,
	STATE_MAYBE_GRAPH,
	STATE_OBJECT_LIST,
	STATE_PROPERTIES,
	STATE_VALUE_LIST,
	STATE_VALUE,
	STATE_VALUE_AS_OBJECT,
	STATE_FINAL,
};

enum {
	STACK_ARRAY,
	STACK_OBJECT,
};

typedef struct {
	guint type;
	guint state;
	union {
		struct {
			gint idx;
			guint elements;
		} array;
		struct {
			gint idx;
			gchar **members;
			gchar *id;
			gboolean is_graph;
		} object;
	} data;
} StateStack;

struct _TrackerDeserializerJsonLD {
	TrackerDeserializer parent_instance;
	JsonParser *parser;
	JsonReader *reader;
	GArray *state_stack;
	gchar *default_lang;
	gchar *cur_graph;
	gchar *cur_subject;
	gchar *cur_predicate;
	gchar *cur_object;
	gchar *cur_object_lang;
	TrackerSparqlValueType object_type;
	guint state;
	gboolean has_row;
	guint blank_node_idx;
	GError *init_error;
};

G_DEFINE_TYPE (TrackerDeserializerJsonLD,
               tracker_deserializer_json_ld,
               TRACKER_TYPE_DESERIALIZER_RDF)

static void
tracker_deserializer_json_ld_finalize (GObject *object)
{
	TrackerDeserializerJsonLD *deserializer =
		TRACKER_DESERIALIZER_JSON_LD (object);

	tracker_sparql_cursor_close (TRACKER_SPARQL_CURSOR (deserializer));

	g_clear_object (&deserializer->reader);
	g_clear_object (&deserializer->parser);
	g_array_unref (deserializer->state_stack);
	g_clear_pointer (&deserializer->default_lang, g_free);
	g_clear_pointer (&deserializer->cur_graph, g_free);
	g_clear_pointer (&deserializer->cur_subject, g_free);
	g_clear_pointer (&deserializer->cur_predicate, g_free);
	g_clear_pointer (&deserializer->cur_object, g_free);
	g_clear_pointer (&deserializer->cur_object_lang, g_free);

	G_OBJECT_CLASS (tracker_deserializer_json_ld_parent_class)->finalize (object);
}

static void
tracker_deserializer_json_ld_constructed (GObject *object)
{
	TrackerDeserializerJsonLD *deserializer =
		TRACKER_DESERIALIZER_JSON_LD (object);
	GInputStream *stream;

	G_OBJECT_CLASS (tracker_deserializer_json_ld_parent_class)->constructed (object);

	stream = tracker_deserializer_get_stream (TRACKER_DESERIALIZER (object));

	if (json_parser_load_from_stream (deserializer->parser,
	                                  stream,
	                                  NULL,
	                                  &deserializer->init_error)) {
		JsonNode *root;

		root = json_parser_get_root (deserializer->parser);
		deserializer->reader = json_reader_new (root);
	}
}

static void
state_clear (gpointer user_data)
{
	StateStack *elem = user_data;

	if (elem->type == STACK_OBJECT) {
		g_strfreev (elem->data.object.members);
		g_free (elem->data.object.id);
	}
}

static gboolean
advance_stack (TrackerDeserializerJsonLD *deserializer)
{
	StateStack *elem;

	g_assert (deserializer->state_stack->len > 0);

	elem = &g_array_index (deserializer->state_stack,
	                       StateStack,
	                       deserializer->state_stack->len - 1);

	if (elem->type == STACK_ARRAY) {
		if (elem->data.array.idx >= 0)
			json_reader_end_element (deserializer->reader);

		elem->data.array.idx++;

		if (elem->data.array.idx >= (gint) elem->data.array.elements)
			return FALSE;

		return json_reader_read_element (deserializer->reader,
		                                 elem->data.array.idx);
	} else if (elem->type == STACK_OBJECT) {
		if (elem->data.object.idx >= 0)
			json_reader_end_member (deserializer->reader);

		elem->data.object.idx++;

		if (elem->data.object.members[elem->data.array.idx] == NULL)
			return FALSE;

		return json_reader_read_member (deserializer->reader,
		                                elem->data.object.members[elem->data.array.idx]);
	}

	return FALSE;
}

static void
push_stack (TrackerDeserializerJsonLD *deserializer,
            guint                      state)
{
	StateStack elem = { 0 };
	const gchar *id = NULL;

	if (json_reader_is_array (deserializer->reader)) {
		elem.type = STACK_ARRAY;
		elem.data.array.idx = -1;
		elem.data.array.elements =
			json_reader_count_elements (deserializer->reader);
	} else if (json_reader_is_object (deserializer->reader)) {
		elem.type = STACK_OBJECT;
		elem.data.object.idx = -1;
		elem.data.object.members =
			json_reader_list_members (deserializer->reader);

		elem.data.object.is_graph =
			json_reader_read_member (deserializer->reader, "@graph");
		json_reader_end_member (deserializer->reader);

		if (json_reader_read_member (deserializer->reader, "@id"))
			id = json_reader_get_string_value (deserializer->reader);
		json_reader_end_member (deserializer->reader);

		if (id) {
			TrackerNamespaceManager *namespaces;

			namespaces = tracker_deserializer_get_namespaces (TRACKER_DESERIALIZER (deserializer));
			elem.data.object.id =
				tracker_namespace_manager_expand_uri (namespaces, id);
		}
	} else {
		g_assert_not_reached ();
	}

	elem.state = state;
	g_array_append_val (deserializer->state_stack, elem);
	deserializer->state = state;
}

static void
pop_stack (TrackerDeserializerJsonLD *deserializer)
{
	StateStack *elem;

	g_assert (deserializer->state_stack->len > 0);

	g_array_set_size (deserializer->state_stack,
	                  deserializer->state_stack->len - 1);

	if (deserializer->state_stack->len > 0) {
		elem = &g_array_index (deserializer->state_stack,
		                       StateStack,
		                       deserializer->state_stack->len - 1);
		deserializer->state = elem->state;
	} else {
		deserializer->state = STATE_FINAL;
	}
}

static guint
stack_state (TrackerDeserializerJsonLD *deserializer)
{
	StateStack *elem;

	g_assert (deserializer->state_stack->len > 0);

	elem = &g_array_index (deserializer->state_stack,
	                       StateStack,
	                       deserializer->state_stack->len - 1);

	return elem->state;
}

static const gchar *
current_member (TrackerDeserializerJsonLD *deserializer)
{
	StateStack *elem;
	gint i;

	g_assert (deserializer->state_stack->len > 0);

	for (i = (gint) deserializer->state_stack->len - 1; i >= 0; i--) {
		elem = &g_array_index (deserializer->state_stack,
		                       StateStack, i);

		if (elem->type == STACK_OBJECT) {
			return elem->data.object.idx >= 0 ?
			       elem->data.object.members[elem->data.object.idx] :
			       NULL;
		}
	}

	return NULL;
}

static const gchar *
current_id (TrackerDeserializerJsonLD *deserializer)
{
	StateStack *elem;
	gint i;

	g_assert (deserializer->state_stack->len > 0);

	for (i = (gint) deserializer->state_stack->len - 1; i >= 0; i--) {
		elem = &g_array_index (deserializer->state_stack,
		                       StateStack, i);
		if (elem->type == STACK_OBJECT &&
		    !elem->data.object.is_graph &&
		    elem->data.object.id)
			return elem->data.object.id;
	}

	return NULL;
}

static const gchar *
current_graph (TrackerDeserializerJsonLD *deserializer)
{
	StateStack *elem;
	gint i;

	g_assert (deserializer->state_stack->len > 0);

	for (i = (gint) deserializer->state_stack->len - 1; i >= 0; i--) {
		elem = &g_array_index (deserializer->state_stack,
		                       StateStack, i);
		if (elem->type == STACK_OBJECT &&
		    elem->data.object.is_graph)
			return elem->data.object.id;
	}

	return NULL;
}

static gchar *
object_to_value (TrackerDeserializerJsonLD  *deserializer,
                 TrackerNamespaceManager    *namespaces,
                 gchar                     **langtag,
                 TrackerSparqlValueType     *value_type)
{
	const gchar *value = NULL, *type = NULL;

	if (json_reader_read_member (deserializer->reader, "@value"))
		value = json_reader_get_string_value (deserializer->reader);
	json_reader_end_member (deserializer->reader);

	if (json_reader_read_member (deserializer->reader, "@language"))
		*langtag = g_strdup (json_reader_get_string_value (deserializer->reader));
	json_reader_end_member (deserializer->reader);

	if (json_reader_read_member (deserializer->reader, "@type"))
		type = json_reader_get_string_value (deserializer->reader);
	json_reader_end_member (deserializer->reader);

	if (g_strcmp0 (type, TRACKER_PREFIX_XSD "string") == 0 ||
	    g_strcmp0 (type, TRACKER_PREFIX_RDF "langString") == 0)
		*value_type = TRACKER_SPARQL_VALUE_TYPE_STRING;
	else if (g_strcmp0 (type, TRACKER_PREFIX_XSD "integer") == 0)
		*value_type = TRACKER_SPARQL_VALUE_TYPE_INTEGER;
	else if (g_strcmp0 (type, TRACKER_PREFIX_XSD "boolean") == 0)
		*value_type = TRACKER_SPARQL_VALUE_TYPE_BOOLEAN;
	else if (g_strcmp0 (type, TRACKER_PREFIX_XSD "double") == 0)
		*value_type = TRACKER_SPARQL_VALUE_TYPE_DOUBLE;
	else if (g_strcmp0 (type, TRACKER_PREFIX_XSD "date") == 0 ||
	         g_strcmp0 (type, TRACKER_PREFIX_XSD "dateTime") == 0)
		*value_type = TRACKER_SPARQL_VALUE_TYPE_DATETIME;
	else
		*value_type = TRACKER_SPARQL_VALUE_TYPE_STRING;

	return g_strdup (value);
}

static gchar *
node_to_value (JsonNode                *node,
               TrackerNamespaceManager *namespaces,
               TrackerSparqlValueType  *value_type)
{
	GValue value = G_VALUE_INIT;
	GType type;
	gchar *str = NULL;

	json_node_get_value (node, &value);
	type = json_node_get_value_type (node);

	if (type == G_TYPE_INT64) {
		*value_type = TRACKER_SPARQL_VALUE_TYPE_INTEGER;
		str = g_strdup_printf ("%" G_GINT64_FORMAT, g_value_get_int64 (&value));
	} else if (type == G_TYPE_STRING) {
		*value_type = TRACKER_SPARQL_VALUE_TYPE_STRING;
		str = tracker_namespace_manager_expand_uri (namespaces, g_value_get_string (&value));
	} else if (type == G_TYPE_DOUBLE) {
		gchar buf[G_ASCII_DTOSTR_BUF_SIZE];

		g_ascii_dtostr (buf, G_ASCII_DTOSTR_BUF_SIZE, g_value_get_double (&value));
		*value_type = TRACKER_SPARQL_VALUE_TYPE_DOUBLE;
		str = g_strdup (buf);
	} else if (type == G_TYPE_BOOLEAN) {
		*value_type = TRACKER_SPARQL_VALUE_TYPE_BOOLEAN;
		str = g_strdup (g_value_get_boolean (&value) ? "true" : "false");
	} else {
		*value_type = TRACKER_SPARQL_VALUE_TYPE_UNBOUND;
	}

	g_value_unset (&value);

	return str;
}

static void
load_special_key (TrackerDeserializerJsonLD *deserializer,
                  const gchar               *key)
{
	const gchar *value;

	if (json_reader_read_member (deserializer->reader, key)) {
		value = json_reader_get_string_value (deserializer->reader);

		if (g_strcmp0 (key, "@language") == 0) {
			g_clear_pointer (&deserializer->default_lang, g_free);
			deserializer->default_lang = g_strdup (value);
		}
	}

	json_reader_end_member (deserializer->reader);
}

static void
load_context (TrackerDeserializerJsonLD *deserializer)
{
	TrackerNamespaceManager *namespaces;

	namespaces = tracker_deserializer_get_namespaces (TRACKER_DESERIALIZER (deserializer));

	if (json_reader_read_member (deserializer->reader, "@context")) {
		gchar **members = json_reader_list_members (deserializer->reader);
		guint i;

		for (i = 0; members && members[i] != NULL; i++) {
			if (members[i][0] == '@') {
				load_special_key (deserializer, members[i]);
				continue;
			}

			if (tracker_namespace_manager_lookup_prefix (namespaces, members[i]))
				continue;

			if (json_reader_read_member (deserializer->reader, members[i])) {
				const gchar *expanded = json_reader_get_string_value (deserializer->reader);
				tracker_namespace_manager_add_prefix (namespaces, members[i], expanded);
			}

			json_reader_end_member (deserializer->reader);
		}

		g_strfreev (members);
	}

	json_reader_end_member (deserializer->reader);
}

static void
forward_state_for_value (TrackerDeserializerJsonLD *deserializer)
{
	if (json_reader_is_object (deserializer->reader)) {
		if (json_reader_read_member (deserializer->reader, "@value")) {
			json_reader_end_member (deserializer->reader);
			deserializer->state = STATE_VALUE_AS_OBJECT;
		} else {
			json_reader_end_member (deserializer->reader);
			push_stack (deserializer, STATE_PROPERTIES);
			deserializer->state = STATE_MAYBE_GRAPH;
		}
	} else {
		deserializer->state = STATE_VALUE;
	}
}

static gboolean
forward_state (TrackerDeserializerJsonLD  *deserializer,
               GError                    **error)
{
	TrackerNamespaceManager *namespaces;
	const gchar *member;

	namespaces = tracker_deserializer_get_namespaces (TRACKER_DESERIALIZER (deserializer));

	switch (deserializer->state) {
	case STATE_INITIAL:
		if (json_reader_is_array (deserializer->reader)) {
			push_stack (deserializer, STATE_ROOT_LIST);
		} else if (json_reader_is_object (deserializer->reader)) {
			push_stack (deserializer, STATE_PROPERTIES);
			deserializer->state = STATE_MAYBE_GRAPH;
		}
		break;
	case STATE_ROOT_LIST:
		if (!advance_stack (deserializer)) {
			pop_stack (deserializer);
			break;
		}

		if (json_reader_is_object (deserializer->reader)) {
			push_stack (deserializer, STATE_MAYBE_GRAPH);
		} else {
			g_set_error (error,
			             TRACKER_SPARQL_ERROR,
			             TRACKER_SPARQL_ERROR_PARSE,
			             "Expected graph or resource object");
			return FALSE;
		}
		break;
	case STATE_MAYBE_GRAPH:
		load_context (deserializer);

		if (json_reader_read_member (deserializer->reader, "@graph")) {
			g_clear_pointer (&deserializer->cur_graph, g_free);
			deserializer->cur_graph = g_strdup (current_graph (deserializer));

			if (json_reader_is_array (deserializer->reader)) {
				push_stack (deserializer, STATE_OBJECT_LIST);
			} else {
				g_set_error (error,
				             TRACKER_SPARQL_ERROR,
				             TRACKER_SPARQL_ERROR_PARSE,
				             "Expected resource list");
				return FALSE;
			}
		} else {
			json_reader_end_member (deserializer->reader);
			g_clear_pointer (&deserializer->cur_subject, g_free);
			deserializer->cur_subject = g_strdup (current_id (deserializer));
			deserializer->state = STATE_PROPERTIES;
		}
		break;
	case STATE_OBJECT_LIST:
		if (!advance_stack (deserializer)) {
			/* Pop the graph array, close manually the @graph
			 * member, and pop the graph object too
			 */
			pop_stack (deserializer);
			json_reader_end_member (deserializer->reader);
			pop_stack (deserializer);
			break;
		}

		if (json_reader_is_object (deserializer->reader)) {
			push_stack (deserializer, STATE_PROPERTIES);
			deserializer->state = STATE_MAYBE_GRAPH;
		} else {
			g_set_error (error,
			             TRACKER_SPARQL_ERROR,
			             TRACKER_SPARQL_ERROR_PARSE,
			             "Expected resource object");
			return FALSE;
		}
		break;
	case STATE_PROPERTIES:
		if (!advance_stack (deserializer)) {
			pop_stack (deserializer);

			if (deserializer->state == STATE_PROPERTIES ||
			    deserializer->state == STATE_VALUE_LIST) {
				gchar *nested_object_id;

				/* The state popped belonged to a nested object,
				 * switch subject/predicate back to the parent
				 * object, and finalize the property that defined it.
				 */
				nested_object_id = g_steal_pointer (&deserializer->cur_subject);

				deserializer->cur_subject = g_strdup (current_id (deserializer));
				g_clear_pointer (&deserializer->cur_predicate, g_free);
				deserializer->cur_predicate =
					tracker_namespace_manager_expand_uri (namespaces, current_member (deserializer));
				g_clear_pointer (&deserializer->cur_object, g_free);
				g_clear_pointer (&deserializer->cur_object_lang, g_free);
				deserializer->cur_object = nested_object_id;
				deserializer->object_type = TRACKER_SPARQL_VALUE_TYPE_STRING;
				deserializer->has_row = TRUE;
			}

			break;
		}

		member = current_member (deserializer);
		g_clear_pointer (&deserializer->cur_predicate, g_free);

		if (g_strcmp0 (member, "@type") == 0)
			deserializer->cur_predicate = g_strdup (TRACKER_PREFIX_RDF "type");
		else if (member[0] != '@')
			deserializer->cur_predicate = tracker_namespace_manager_expand_uri (namespaces, member);
		else
			break;

		if (json_reader_is_array (deserializer->reader))
			push_stack (deserializer, STATE_VALUE_LIST);
		else
			forward_state_for_value (deserializer);
		break;
	case STATE_VALUE_LIST:
		if (!advance_stack (deserializer)) {
			pop_stack (deserializer);
			break;
		}

		forward_state_for_value (deserializer);
		break;
	case STATE_VALUE_AS_OBJECT:
		g_clear_pointer (&deserializer->cur_object, g_free);
		g_clear_pointer (&deserializer->cur_object_lang, g_free);
		deserializer->cur_object = object_to_value (deserializer,
		                                            namespaces,
		                                            &deserializer->cur_object_lang,
		                                            &deserializer->object_type);
		deserializer->has_row = TRUE;

		deserializer->state = stack_state (deserializer);
		break;
	case STATE_VALUE:
		g_clear_pointer (&deserializer->cur_object, g_free);
		g_clear_pointer (&deserializer->cur_object_lang, g_free);
		deserializer->cur_object = node_to_value (json_reader_get_value (deserializer->reader),
		                                          namespaces,
		                                          &deserializer->object_type);
		deserializer->has_row = TRUE;

		deserializer->state = stack_state (deserializer);
		break;
	case STATE_FINAL:
		break;
	}

	return deserializer->state_stack->len > 0;
}

static TrackerSparqlValueType
tracker_deserializer_json_ld_get_value_type (TrackerSparqlCursor  *cursor,
                                             gint                  column)
{
	TrackerDeserializerJsonLD *deserializer =
		TRACKER_DESERIALIZER_JSON_LD (cursor);

	switch (column) {
	case TRACKER_RDF_COL_SUBJECT:
		if (!deserializer->cur_subject)
			return TRACKER_SPARQL_VALUE_TYPE_UNBOUND;
		else if (strncmp (deserializer->cur_subject, "_:", 2) == 0)
			return TRACKER_SPARQL_VALUE_TYPE_BLANK_NODE;
		else
			return TRACKER_SPARQL_VALUE_TYPE_URI;
		break;
	case TRACKER_RDF_COL_PREDICATE:
		if (!deserializer->cur_predicate)
			return TRACKER_SPARQL_VALUE_TYPE_UNBOUND;
		else
			return TRACKER_SPARQL_VALUE_TYPE_URI;
		break;
	case TRACKER_RDF_COL_OBJECT:
		if (!deserializer->cur_object)
			return TRACKER_SPARQL_VALUE_TYPE_UNBOUND;
		else
			return deserializer->object_type;
		break;
	case TRACKER_RDF_COL_GRAPH:
		if (!deserializer->cur_graph)
			return TRACKER_SPARQL_VALUE_TYPE_UNBOUND;
		else
			return TRACKER_SPARQL_VALUE_TYPE_URI;
		break;
	default:
		return TRACKER_SPARQL_VALUE_TYPE_UNBOUND;
	}
}

static const gchar *
tracker_deserializer_json_ld_get_string (TrackerSparqlCursor  *cursor,
                                         gint                  column,
                                         const gchar         **langtag,
                                         glong                *length)
{
	TrackerDeserializerJsonLD *deserializer =
		TRACKER_DESERIALIZER_JSON_LD (cursor);
	const gchar *str = NULL;

	if (length)
		*length = 0;
	if (langtag)
		*langtag = NULL;

	switch (column) {
	case TRACKER_RDF_COL_SUBJECT:
		str = deserializer->cur_subject;
		break;
	case TRACKER_RDF_COL_PREDICATE:
		str = deserializer->cur_predicate;
		break;
	case TRACKER_RDF_COL_OBJECT:
		if (langtag) {
			if (deserializer->cur_object_lang)
				*langtag = deserializer->cur_object_lang;
			else
				*langtag = deserializer->default_lang;
		}

		str = deserializer->cur_object;
		break;
	case TRACKER_RDF_COL_GRAPH:
		str = deserializer->cur_graph;
		break;
	default:
		break;
	}

	if (length && str)
		*length = strlen (str);

	return str;
}

static gboolean
tracker_deserializer_json_ld_next (TrackerSparqlCursor  *cursor,
                                   GCancellable         *cancellable,
                                   GError              **error)
{
	TrackerDeserializerJsonLD *deserializer =
		TRACKER_DESERIALIZER_JSON_LD (cursor);

	if (deserializer->init_error) {
		GError *init_error;

		init_error = g_steal_pointer (&deserializer->init_error);
		g_propagate_error (error, init_error);
		return FALSE;
	}

	deserializer->has_row = FALSE;

	while (!deserializer->has_row) {
		GError *inner_error = NULL;

		if (g_cancellable_set_error_if_cancelled (cancellable, error))
			return FALSE;

		if (!forward_state (deserializer, &inner_error)) {
			if (inner_error) {
				g_propagate_error (error, inner_error);
			} else {
				const GError *reader_error;

				reader_error = json_reader_get_error (deserializer->reader);
				if (error && reader_error)
					*error = g_error_copy (reader_error);
			}

			return FALSE;
		}
	}

	if (!deserializer->cur_subject)
		deserializer->cur_subject = g_strdup_printf ("_:%d", deserializer->blank_node_idx++);

	return TRUE;
}

static void
tracker_deserializer_json_ld_close (TrackerSparqlCursor *cursor)
{
}

gboolean
tracker_deserializer_json_ld_get_parser_location (TrackerDeserializer *deserializer,
                                                  goffset             *line_no,
                                                  goffset             *column_no)
{
	return FALSE;
}

static void
tracker_deserializer_json_ld_class_init (TrackerDeserializerJsonLDClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	TrackerSparqlCursorClass *cursor_class =
		TRACKER_SPARQL_CURSOR_CLASS (klass);
	TrackerDeserializerClass *deserializer_class =
		TRACKER_DESERIALIZER_CLASS (klass);

	object_class->finalize = tracker_deserializer_json_ld_finalize;
	object_class->constructed = tracker_deserializer_json_ld_constructed;

	cursor_class->get_value_type = tracker_deserializer_json_ld_get_value_type;
	cursor_class->get_string = tracker_deserializer_json_ld_get_string;
	cursor_class->next = tracker_deserializer_json_ld_next;
	cursor_class->close = tracker_deserializer_json_ld_close;

	deserializer_class->get_parser_location =
		tracker_deserializer_json_ld_get_parser_location;
}

static void
tracker_deserializer_json_ld_init (TrackerDeserializerJsonLD *deserializer)
{
	deserializer->parser = json_parser_new ();
	deserializer->state_stack = g_array_new (FALSE, FALSE, sizeof (StateStack));
	g_array_set_clear_func (deserializer->state_stack, state_clear);
}

TrackerSparqlCursor *
tracker_deserializer_json_ld_new (GInputStream            *stream,
                                  TrackerNamespaceManager *namespaces)
{
	return g_object_new (TRACKER_TYPE_DESERIALIZER_JSON_LD,
	                     "stream", stream,
	                     "namespace-manager", namespaces,
	                     NULL);
}
