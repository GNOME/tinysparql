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
 *  https://www.w3.org/TR/sparql11-results-json/
 */

#include "config.h"

#include "tracker-deserializer-json.h"

#include <json-glib/json-glib.h>

typedef struct {
	TrackerSparqlValueType type;
	const gchar *str;
	const gchar *langtag;
} ColumnData;

struct _TrackerDeserializerJson {
	TrackerDeserializer parent_instance;
	GArray *columns;
	JsonParser *parser;
	JsonArray *vars;
	JsonArray *results;
	JsonObject *current_row;
	guint idx;
	gboolean started;
	GError *init_error;
};

G_DEFINE_TYPE (TrackerDeserializerJson,
               tracker_deserializer_json,
               TRACKER_TYPE_DESERIALIZER)

static void
tracker_deserializer_json_finalize (GObject *object)
{
	TrackerDeserializerJson *deserializer =
		TRACKER_DESERIALIZER_JSON (object);

	g_clear_object (&deserializer->parser);
	g_array_unref (deserializer->columns);

	G_OBJECT_CLASS (tracker_deserializer_json_parent_class)->finalize (object);
}

static void
tracker_deserializer_json_constructed (GObject *object)
{
	TrackerDeserializerJson *deserializer =
		TRACKER_DESERIALIZER_JSON (object);
	GInputStream *stream;
	JsonNode *root_node;
	JsonObject *root, *head, *results;

	G_OBJECT_CLASS (tracker_deserializer_json_parent_class)->constructed (object);

	stream = tracker_deserializer_get_stream (TRACKER_DESERIALIZER (object));

	if (json_parser_load_from_stream (deserializer->parser,
	                                  stream,
	                                  NULL,
	                                  &deserializer->init_error)) {
		root_node = json_parser_get_root (deserializer->parser);
		root = json_node_get_object (root_node);

		head = json_object_get_object_member (root, "head");
		deserializer->vars = json_object_get_array_member (head, "vars");

		results = json_object_get_object_member (root, "results");
		deserializer->results = json_object_get_array_member (results, "bindings");
	}
}

static gint
tracker_deserializer_json_get_n_columns (TrackerSparqlCursor  *cursor)
{
	TrackerDeserializerJson *deserializer =
		TRACKER_DESERIALIZER_JSON (cursor);

	return json_array_get_length (deserializer->vars);
}

static TrackerSparqlValueType
tracker_deserializer_json_get_value_type (TrackerSparqlCursor  *cursor,
                                          gint                  column)
{
	TrackerDeserializerJson *deserializer =
		TRACKER_DESERIALIZER_JSON (cursor);
	ColumnData *col;

	if (column < 0 || column >= (gint) deserializer->columns->len)
		return TRACKER_SPARQL_VALUE_TYPE_UNBOUND;

	col = &g_array_index (deserializer->columns, ColumnData, column);

	return col->type;
}

static const gchar *
tracker_deserializer_json_get_variable_name (TrackerSparqlCursor  *cursor,
                                             gint                  column)
{
	TrackerDeserializerJson *deserializer =
		TRACKER_DESERIALIZER_JSON (cursor);

	if (column < 0 || (guint) column >= json_array_get_length (deserializer->vars))
		return NULL;

	return json_array_get_string_element (deserializer->vars, column);
}

static const gchar *
tracker_deserializer_json_get_string (TrackerSparqlCursor   *cursor,
                                      gint                   column,
                                      const gchar          **langtag,
                                      glong                 *length)
{
	TrackerDeserializerJson *deserializer =
		TRACKER_DESERIALIZER_JSON (cursor);
	ColumnData *col;

	if (length)
		*length = 0;
	if (langtag)
		*langtag = NULL;

	if (column < 0 || column >= (gint) deserializer->columns->len)
		return NULL;

	col = &g_array_index (deserializer->columns, ColumnData, column);

	if (length)
		*length = strlen (col->str);
	if (langtag)
		*langtag = col->langtag;

	return col->str;
}

static gboolean
parse_column_type (JsonObject              *column,
                   TrackerSparqlValueType  *value,
                   GError                 **error)
{
	const gchar *type;

	if (!json_object_has_member (column, "type")) {
		g_set_error (error,
		             TRACKER_SPARQL_ERROR,
		             TRACKER_SPARQL_ERROR_PARSE,
		             "Column object does not have 'type' member");
		return FALSE;
	}

	type = json_object_get_string_member (column, "type");

	if (g_str_equal (type, "uri")) {
		*value = TRACKER_SPARQL_VALUE_TYPE_URI;
	} else if (g_str_equal (type, "bnode")) {
		*value = TRACKER_SPARQL_VALUE_TYPE_BLANK_NODE;
	} else if (g_str_equal (type, "literal")) {
		const gchar *datatype, *suffix;

		if (json_object_has_member (column, "datatype"))
			datatype = json_object_get_string_member (column, "datatype");
		else
			datatype = TRACKER_PREFIX_XSD "string";

		if (!g_str_has_prefix (datatype, TRACKER_PREFIX_XSD)) {
			*value = TRACKER_SPARQL_VALUE_TYPE_STRING;
			return TRUE;
		}

		suffix = &datatype[strlen (TRACKER_PREFIX_XSD)];

		if (g_str_equal (suffix, "byte") ||
		    g_str_equal (suffix, "int") ||
		    g_str_equal (suffix, "integer") ||
		    g_str_equal (suffix, "long"))
			*value = TRACKER_SPARQL_VALUE_TYPE_INTEGER;
		else if (g_str_equal (suffix, "decimal") ||
		         g_str_equal (suffix, "double"))
			*value = TRACKER_SPARQL_VALUE_TYPE_DOUBLE;
		else if (g_str_equal (suffix, "date") ||
		         g_str_equal (suffix, "dateTime"))
			*value = TRACKER_SPARQL_VALUE_TYPE_DATETIME;
		else if (g_str_equal (suffix, "boolean"))
			*value = TRACKER_SPARQL_VALUE_TYPE_BOOLEAN;
		else
			*value = TRACKER_SPARQL_VALUE_TYPE_STRING;
	} else {
		g_set_error (error,
		             TRACKER_SPARQL_ERROR,
		             TRACKER_SPARQL_ERROR_PARSE,
		             "Unknown type '%s'", type);
		return FALSE;
	}

	return TRUE;
}

static gboolean
parse_column_string (JsonObject   *column,
                     const gchar **str,
                     GError      **error)
{
	if (!json_object_has_member (column, "value")) {
		g_set_error (error,
		             TRACKER_SPARQL_ERROR,
		             TRACKER_SPARQL_ERROR_PARSE,
		             "Column object does not have 'value' member");
		return FALSE;
	}

	*str = json_object_get_string_member (column, "value");
	return TRUE;
}

static const gchar *
parse_column_langtag (JsonObject *column)
{
	if (!json_object_has_member (column, "xml:lang"))
		return NULL;

	return json_object_get_string_member (column, "xml:lang");
}

static gboolean
parse_row (TrackerDeserializerJson  *deserializer,
           GError                  **error)
{
	TrackerSparqlCursor *cursor = TRACKER_SPARQL_CURSOR (deserializer);
	const gchar *var_name;
	JsonObject *column;
	gint n_columns, i;

	g_array_set_size (deserializer->columns, 0);
	n_columns = tracker_sparql_cursor_get_n_columns (cursor);

	for (i = 0; i < n_columns; i++) {
		ColumnData col = { 0 };

		var_name = tracker_sparql_cursor_get_variable_name (cursor, i);

		if (json_object_has_member (deserializer->current_row, var_name)) {
			column = json_object_get_object_member (deserializer->current_row,
			                                        var_name);
			if (column) {
				if (!parse_column_string (column, &col.str, error) ||
				    !parse_column_type (column, &col.type, error))
					return FALSE;

				col.langtag = parse_column_langtag (column);
				g_array_append_val (deserializer->columns, col);
				continue;
			}
		}

		col = (ColumnData) { TRACKER_SPARQL_VALUE_TYPE_UNBOUND, NULL };
		g_array_append_val (deserializer->columns, col);
	}

	return TRUE;
}

static gboolean
tracker_deserializer_json_next (TrackerSparqlCursor  *cursor,
                                GCancellable         *cancellable,
                                GError              **error)
{
	TrackerDeserializerJson *deserializer =
		TRACKER_DESERIALIZER_JSON (cursor);

	g_array_set_size (deserializer->columns, 0);

	if (deserializer->init_error) {
		GError *init_error;

		init_error = g_steal_pointer (&deserializer->init_error);
		g_propagate_error (error, init_error);
		return FALSE;
	}

	if (deserializer->started)
		deserializer->idx++;

	if (deserializer->idx >= json_array_get_length (deserializer->results))
		return FALSE;

	if (g_cancellable_set_error_if_cancelled (cancellable, error))
		return FALSE;

	deserializer->current_row =
		json_array_get_object_element (deserializer->results,
		                               deserializer->idx);
	deserializer->started = TRUE;

	return parse_row (deserializer, error);
}

static void
tracker_deserializer_json_next_async (TrackerSparqlCursor  *cursor,
                                      GCancellable         *cancellable,
                                      GAsyncReadyCallback   cb,
                                      gpointer              user_data)
{
	GError *error = NULL;
	GTask *task;

	task = g_task_new (cursor, cancellable, cb, user_data);

	if (tracker_sparql_cursor_next (cursor, cancellable, &error))
		g_task_return_boolean (task, TRUE);
	else if (!error)
		g_task_return_boolean (task, FALSE);
	else
		g_task_return_error (task, error);

	g_object_unref (task);
}

static gboolean
tracker_deserializer_json_next_finish (TrackerSparqlCursor  *cursor,
                                       GAsyncResult         *res,
                                       GError              **error)
{
	return g_task_propagate_boolean (G_TASK (res), error);
}

gboolean
tracker_deserializer_json_get_parser_location (TrackerDeserializer *deserializer,
                                               goffset             *line_no,
                                               goffset             *column_no)
{
	return FALSE;
}

static void
tracker_deserializer_json_class_init (TrackerDeserializerJsonClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	TrackerSparqlCursorClass *cursor_class =
		TRACKER_SPARQL_CURSOR_CLASS (klass);
	TrackerDeserializerClass *deserializer_class =
		TRACKER_DESERIALIZER_CLASS (klass);

	object_class->finalize = tracker_deserializer_json_finalize;
	object_class->constructed = tracker_deserializer_json_constructed;

	cursor_class->get_n_columns = tracker_deserializer_json_get_n_columns;
	cursor_class->get_value_type = tracker_deserializer_json_get_value_type;
	cursor_class->get_variable_name = tracker_deserializer_json_get_variable_name;
	cursor_class->get_string = tracker_deserializer_json_get_string;
	cursor_class->next = tracker_deserializer_json_next;
	cursor_class->next_async = tracker_deserializer_json_next_async;
	cursor_class->next_finish = tracker_deserializer_json_next_finish;

	deserializer_class->get_parser_location =
		tracker_deserializer_json_get_parser_location;
}

static void
tracker_deserializer_json_init (TrackerDeserializerJson *deserializer)
{
	deserializer->parser = json_parser_new ();
	deserializer->columns = g_array_new (FALSE, FALSE, sizeof (ColumnData));
}

TrackerSparqlCursor *
tracker_deserializer_json_new (GInputStream            *stream,
                               TrackerNamespaceManager *namespaces)
{
	return g_object_new (TRACKER_TYPE_DESERIALIZER_JSON,
	                     "stream", stream,
	                     "namespace-manager", namespaces,
	                     NULL);
}
