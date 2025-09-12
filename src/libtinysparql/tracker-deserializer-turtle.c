/*
 * Copyright (C) 2020, Red Hat Inc.
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

/* Deserialization to cursors for the turtle format defined at:
 *  https://www.w3.org/TR/turtle/
 *
 * And the related TRIG format defined at:
 *  http://www.w3.org/TR/trig/
 */

#include "config.h"

#include "tracker-deserializer-turtle.h"

#include <strings.h>

#include "core/tracker-sparql-grammar.h"
#include "core/tracker-uuid.h"
#include "tracker-private.h"

#define BUF_SIZE 4096
#define RDF_TYPE "http://www.w3.org/1999/02/22-rdf-syntax-ns#type"

typedef enum
{
	STATE_INITIAL,
	STATE_GRAPH,
	STATE_SUBJECT,
	STATE_PREDICATE,
	STATE_OBJECT,
	STATE_STEP,
} ParserState;

typedef struct {
	gchar *subject;
	gchar *predicate;
	ParserState state;
} StateStack;

struct _TrackerDeserializerTurtle {
	TrackerDeserializerRdf parent_instance;
	GBufferedInputStream *buffered_stream;
	GArray *parser_state;
	gchar *base;
	gchar *graph;
	gchar *subject;
	gchar *predicate;
	gchar *object;
	gchar *object_lang;
	gboolean object_is_uri;
	ParserState state;
	goffset line_no;
	goffset column_no;
	gboolean parse_trig;
};

G_DEFINE_TYPE (TrackerDeserializerTurtle,
               tracker_deserializer_turtle,
               TRACKER_TYPE_DESERIALIZER_RDF)

static void advance_whitespace_and_comments (TrackerDeserializerTurtle *deserializer);

static void
tracker_deserializer_turtle_finalize (GObject *object)
{
	TrackerDeserializerTurtle *deserializer = TRACKER_DESERIALIZER_TURTLE (object);

	tracker_sparql_cursor_close (TRACKER_SPARQL_CURSOR (deserializer));

	g_clear_object (&deserializer->buffered_stream);
	g_clear_pointer (&deserializer->parser_state, g_array_unref);
	g_clear_pointer (&deserializer->subject, g_free);
	g_clear_pointer (&deserializer->predicate, g_free);
	g_clear_pointer (&deserializer->object, g_free);
	g_clear_pointer (&deserializer->object_lang, g_free);
	g_clear_pointer (&deserializer->graph, g_free);
	g_clear_pointer (&deserializer->base, g_free);

	G_OBJECT_CLASS (tracker_deserializer_turtle_parent_class)->finalize (object);
}

static void
tracker_deserializer_turtle_constructed (GObject *object)
{
	TrackerDeserializerTurtle *deserializer_ttl = TRACKER_DESERIALIZER_TURTLE (object);
	TrackerDeserializer *deserializer = TRACKER_DESERIALIZER (object);
	GInputStream *stream;

	G_OBJECT_CLASS (tracker_deserializer_turtle_parent_class)->constructed (object);

	stream = tracker_deserializer_get_stream (deserializer);
	deserializer_ttl->buffered_stream =
		G_BUFFERED_INPUT_STREAM (g_buffered_input_stream_new (stream));
	deserializer_ttl->line_no = 1;
	deserializer_ttl->column_no = 1;

	g_object_get (object,
	              "has-graph", &deserializer_ttl->parse_trig,
	              NULL);
}

static void
clear_parser_state (StateStack *state)
{
	g_free (state->subject);
	g_free (state->predicate);
}

static void
push_stack (TrackerDeserializerTurtle *deserializer)
{
	StateStack state;

	state.subject = g_strdup (deserializer->subject);
	state.predicate = g_strdup (deserializer->predicate);
	state.state = deserializer->state;
	g_array_append_val (deserializer->parser_state, state);
}

static void
pop_stack (TrackerDeserializerTurtle *deserializer)
{
	StateStack *state;
	gchar *s, *p, *o;

	s = deserializer->subject;
	p = deserializer->predicate;
	o = deserializer->object;
	deserializer->subject = deserializer->predicate = deserializer->object = NULL;

	state = &g_array_index (deserializer->parser_state, StateStack, deserializer->parser_state->len - 1);
	deserializer->subject = g_steal_pointer (&state->subject);
	deserializer->predicate = g_steal_pointer (&state->predicate);
	deserializer->state = state->state;

	if (deserializer->state == STATE_OBJECT) {
		/* Restore the old subject as current object */
		deserializer->object = s;
		deserializer->object_is_uri = TRUE;
		g_clear_pointer (&deserializer->object_lang, g_free);
		s = NULL;
	} else if (deserializer->state == STATE_SUBJECT) {
		g_clear_pointer (&deserializer->subject, g_free);
		deserializer->subject = s;
		s = NULL;
	}

	g_free (s);
	g_free (p);
	g_free (o);
	g_array_remove_index (deserializer->parser_state, deserializer->parser_state->len - 1);
}

static void
calculate_num_lines_and_columns (const gchar     *start,
                                 gsize            count,
                                 goffset         *num_lines,
                                 goffset         *num_columns)
{
	*num_lines = 0;
	*num_columns = 0;

	for (size_t i = 0; i < count; i++)
	{
		if (*(start + i) == '\n') {
			*num_lines += 1;
			*num_columns = 1;
		} else {
			*num_columns += 1;
		}
	}
}

static gsize
seek_input (TrackerDeserializerTurtle *deserializer,
            gsize                      count)
{
	const gchar *buffer;
	gsize size;
	goffset num_lines;
	goffset num_columns;

	buffer = g_buffered_input_stream_peek_buffer (deserializer->buffered_stream,
	                                              &size);
	count = MIN (count, size);
	if (!count)
		return 0;

	calculate_num_lines_and_columns (buffer, count, &num_lines, &num_columns);

	deserializer->line_no += num_lines;
	if (num_lines > 0) {
		deserializer->column_no = num_columns;
	} else {
		deserializer->column_no += num_columns;
	}
	return g_input_stream_skip (G_INPUT_STREAM (deserializer->buffered_stream),
	                            count, NULL, NULL);
}

static gboolean
parse_token (TrackerDeserializerTurtle *deserializer,
             const gchar               *token)
{
	int len = strlen (token);
	const gchar *buffer;
	gsize size;

	buffer = g_buffered_input_stream_peek_buffer (deserializer->buffered_stream,
	                                              &size);
	if (size == 0)
		return FALSE;
	if (size < (gsize) len)
		return FALSE;
	if (strncasecmp (buffer, token, len) != 0)
		return FALSE;
	if (!seek_input (deserializer, len))
		return FALSE;

	return TRUE;
}

static gboolean
parse_terminal (TrackerDeserializerTurtle  *deserializer,
                TrackerTerminalFunc         terminal_func,
                guint                       padding,
                gchar                     **out)
{
	const gchar *end, *buffer;
	gchar *str;
	gsize size;

	buffer = g_buffered_input_stream_peek_buffer (deserializer->buffered_stream,
	                                              &size);
	if (size == 0)
		return FALSE;

	if (!terminal_func (buffer, &buffer[size], &end))
		return FALSE;

	if (end - buffer < 2 * padding)
		return FALSE;

	str = g_strndup (&buffer[padding], end - buffer - (2 * padding));

	if (!seek_input (deserializer, end - buffer)) {
		g_free (str);
		return FALSE;
	}

	if (out)
		*out = str;
	else
		g_free (str);

	return TRUE;
}

static gchar *
expand_prefix (TrackerDeserializerTurtle  *deserializer,
               const gchar                *shortname,
               GError                    **error)
{
	TrackerNamespaceManager *namespaces;
	gchar *expanded;

	namespaces = tracker_deserializer_get_namespaces (TRACKER_DESERIALIZER (deserializer));
	expanded = tracker_namespace_manager_expand_uri (namespaces, shortname);

	if (g_strcmp0 (expanded, shortname) == 0) {
		/* Point to beginning of term */
		deserializer->column_no -= strlen(shortname);
		g_free (expanded);
		g_set_error (error,
		             TRACKER_SPARQL_ERROR,
		             TRACKER_SPARQL_ERROR_PARSE,
		             "Unknown prefix %s", shortname);
		return NULL;
	}

	return expanded;
}

static gchar *
expand_base (TrackerDeserializerTurtle *deserializer,
             const gchar               *suffix)
{
	if (deserializer->base && !strstr (suffix, ":/")) {
		return g_strconcat (deserializer->base, suffix, NULL);
	} else {
		return g_strdup (suffix);
	}
}

static void
advance_whitespace (TrackerDeserializerTurtle *deserializer)
{
	while (TRUE) {
		gsize size;
		const gchar *data;
		gchar ch;

		data = g_buffered_input_stream_peek_buffer (deserializer->buffered_stream, &size);
		if (size == 0)
			break;

		ch = data[0];
		if (!(WS))
			break;

		if (!seek_input (deserializer, 1))
			break;
	}
}

static void
add_prefix (TrackerDeserializerTurtle  *deserializer,
            const gchar                *prefix,
            const gchar                *uri)
{
	TrackerNamespaceManager *namespaces;

	namespaces = tracker_deserializer_get_namespaces (TRACKER_DESERIALIZER (deserializer));
	tracker_namespace_manager_add_prefix (namespaces, prefix, uri);
}

static gboolean
handle_prefix (TrackerDeserializerTurtle  *deserializer,
               GError                    **error)
{
	gchar *prefix = NULL, *uri = NULL, *expanded;

	advance_whitespace_and_comments (deserializer);
	if (!parse_terminal (deserializer, terminal_PNAME_NS, 0, &prefix))
		goto error;

	advance_whitespace_and_comments (deserializer);
	if (!parse_terminal (deserializer, terminal_IRIREF, 1, &uri))
		goto error;

	advance_whitespace_and_comments (deserializer);
	if (!parse_token (deserializer, "."))
		goto error;

	/* Remove the trailing ':' in prefix */
	prefix[strlen(prefix) - 1] = '\0';

	expanded = expand_base (deserializer, uri);
	add_prefix (deserializer, prefix, expanded);
	g_free (prefix);
	g_free (uri);
	g_free (expanded);

	return TRUE;
error:
	g_free (prefix);
	g_free (uri);
	g_set_error (error,
	             TRACKER_SPARQL_ERROR,
	             TRACKER_SPARQL_ERROR_PARSE,
	             "Could not parse @prefix");
	return FALSE;
}

static gboolean
handle_base (TrackerDeserializerTurtle  *deserializer,
             GError                    **error)
{
	gchar *base = NULL;

	advance_whitespace_and_comments (deserializer);
	if (!parse_terminal (deserializer, terminal_IRIREF, 1, &base))
		goto error;

	advance_whitespace_and_comments (deserializer);
	if (!parse_token (deserializer, "."))
		goto error;

	g_clear_pointer (&deserializer->base, g_free);
	deserializer->base = g_steal_pointer (&base);

	return TRUE;
error:
	g_free (base);
	g_set_error (error,
	             TRACKER_SPARQL_ERROR,
	             TRACKER_SPARQL_ERROR_PARSE,
	             "Could not parse @base");
	return FALSE;
}

static gboolean
handle_type_cast (TrackerDeserializerTurtle  *deserializer,
                  GError                    **error)
{
	/* These actually go ignored, imposed by the ontology */
	if (parse_token (deserializer, "^^")) {
		if (parse_terminal (deserializer, terminal_IRIREF, 1, NULL) ||
		    parse_terminal (deserializer, terminal_PNAME_LN, 0, NULL) ||
		    parse_terminal (deserializer, terminal_PNAME_NS, 0, NULL))
			return TRUE;

		g_set_error (error,
		             TRACKER_SPARQL_ERROR,
		             TRACKER_SPARQL_ERROR_PARSE,
		             "Error parsing type cast");
		return FALSE;
	}

	return TRUE;
}

static gboolean
find_needle (const gchar *buffer,
             gsize        buffer_len,
             gsize        start,
             const gchar *needle)
{
	const gchar *ptr, *prev;

	if (start >= buffer_len)
		return FALSE;

 retry:
	ptr = memmem (&buffer[start], buffer_len - start,
	              needle, strlen (needle));
	if (!ptr)
		return FALSE;

	/* Empty string */
	if (ptr == &buffer[start])
		return TRUE;

	prev = ptr - 1;
	g_assert (prev >= &buffer[start]);

	if (*prev == '\\') {
		start = ptr - buffer + 1;
		goto retry;
	}

	return TRUE;
}

static void
advance_whitespace_and_comments (TrackerDeserializerTurtle *deserializer)
{
	const gchar *buffer, *str;
	gsize size, skip;

	while (TRUE) {
		advance_whitespace (deserializer);
		buffer = g_buffered_input_stream_peek_buffer (deserializer->buffered_stream,
		                                              &size);
		if (size == 0)
			break;
		if (buffer[0] != '#')
			break;

		while (!find_needle (buffer, size, 0, "\n")) {
			if (!seek_input (deserializer, size))
				break;

			buffer = g_buffered_input_stream_peek_buffer (deserializer->buffered_stream,
			                                              &size);

			if (size == 0)
				break;
		}

		str = memmem (buffer, size, "\n", strlen ("\n"));
		if (str)
			skip = str + 1 - buffer;
		else
			skip = size;

		if (!seek_input (deserializer, skip))
			break;
	}
}


static gboolean
maybe_expand_buffer (TrackerDeserializerTurtle  *deserializer,
                     GError                    **error)
{
	const gchar *buffer, *needle;
	gsize start, buffer_len;

	/* Expand the buffer to be able to read string terminals fully,
	 * this only applies if there is a string terminal to read right
	 * now.
	 */
	buffer = g_buffered_input_stream_peek_buffer (deserializer->buffered_stream,
	                                              &buffer_len);
	if (buffer_len >= 3 && strncmp (buffer, "\"\"\"", 3) == 0) {
		needle = "\"\"\"";
		start = 3;
	} else if (buffer_len >= 3 && strncmp (buffer, "'''", 3) == 0) {
		needle = "'''";
		start = 3;
	} else if (buffer_len >= 1 && strncmp (buffer, "\"", 1) == 0) {
		needle = "\"";
		start = 1;
	} else if (buffer_len >= 1 && strncmp (buffer, "'", 1) == 0) {
		needle = "'";
		start = 1;
	} else {
		return TRUE;
	}

	while (!find_needle (buffer, buffer_len, start, needle)) {
		gsize size, available;

		available = g_buffered_input_stream_get_available (deserializer->buffered_stream);
		size = g_buffered_input_stream_get_buffer_size (deserializer->buffered_stream);

		if (available == size) {
			size *= 2;

			/* We only allow strings up to 1GB */
			if (size > 1024 * 1024 * 1024) {
				g_set_error (error,
				             TRACKER_SPARQL_ERROR,
				             TRACKER_SPARQL_ERROR_PARSE,
				             "String too big to parse");
				return FALSE;
			}

			g_buffered_input_stream_set_buffer_size (deserializer->buffered_stream,
			                                         size);
		}

		if (g_buffered_input_stream_fill (deserializer->buffered_stream, -1, NULL, error) < 0)
			return FALSE;

		/* If we reached EOF already, the string is not terminated properly */
		if (g_buffered_input_stream_get_available (deserializer->buffered_stream) == available) {
			g_set_error (error,
			             TRACKER_SPARQL_ERROR,
			             TRACKER_SPARQL_ERROR_PARSE,
			             "Unterminated string");
			return FALSE;
		}

		buffer = g_buffered_input_stream_peek_buffer (deserializer->buffered_stream,
		                                              &buffer_len);
	}

	return TRUE;
}


static gboolean
tracker_deserializer_turtle_iterate_next (TrackerDeserializerTurtle  *deserializer,
                                          GError                    **error)
{
	while (TRUE) {
		gchar *str, *lang;
		gsize available;

		available = g_buffered_input_stream_get_available (deserializer->buffered_stream);

		if (available < BUF_SIZE) {
			if (g_buffered_input_stream_fill (deserializer->buffered_stream,
			                                  BUF_SIZE - available,
			                                  NULL, error) < 0)
				return FALSE;
		}

		advance_whitespace_and_comments (deserializer);

		switch (deserializer->state) {
		case STATE_INITIAL:
			if (parse_token (deserializer, "@prefix")) {
				if (!handle_prefix (deserializer, error))
					return FALSE;
				break;
			} else if (parse_token (deserializer, "@base")) {
				if (!handle_base (deserializer, error))
					return FALSE;
				break;
			} else {
				if (deserializer->parse_trig)
					deserializer->state = STATE_GRAPH;
				else
					deserializer->state = STATE_SUBJECT;
			}
			break;
		case STATE_GRAPH:
			if (g_buffered_input_stream_get_available (deserializer->buffered_stream) == 0)
				return FALSE;

			g_clear_pointer (&deserializer->graph, g_free);

			if (parse_token (deserializer, "graph")) {
				advance_whitespace_and_comments (deserializer);

				if (parse_terminal (deserializer, terminal_IRIREF, 1, &str)) {
					deserializer->graph = expand_base (deserializer, str);
					g_free (str);
				} else if (parse_terminal (deserializer, terminal_PNAME_LN, 0, &str) ||
				           parse_terminal (deserializer, terminal_PNAME_NS, 0, &str)) {
					deserializer->graph = expand_prefix (deserializer, str, error);
					g_free (str);
					if (!deserializer->graph)
						return FALSE;
				} else {
					g_set_error (error,
					             TRACKER_SPARQL_ERROR,
					             TRACKER_SPARQL_ERROR_PARSE,
					             "Wrong graph token");
					return FALSE;
				}
			}

			advance_whitespace_and_comments (deserializer);

			if (!parse_token (deserializer, "{")) {
				if (deserializer->graph ||
				    g_buffered_input_stream_get_available (deserializer->buffered_stream) > 0) {
					g_set_error (error,
					             TRACKER_SPARQL_ERROR,
					             TRACKER_SPARQL_ERROR_PARSE,
					             "Expected graph block");
					return FALSE;
				} else {
					/* Empty RDF data */
					return TRUE;
				}
			}

			deserializer->state = STATE_SUBJECT;
			break;
		case STATE_SUBJECT:
			if (g_buffered_input_stream_get_available (deserializer->buffered_stream) == 0)
				return FALSE;

			g_clear_pointer (&deserializer->subject, g_free);

			if (parse_token (deserializer, "[")) {
				/* Anonymous blank node */
				push_stack (deserializer);
				g_clear_pointer (&deserializer->subject, g_free);
				deserializer->subject = tracker_generate_uuid ("_:bnode");
				deserializer->state = STATE_PREDICATE;
				continue;
			}

			if (parse_terminal (deserializer, terminal_IRIREF, 1, &str)) {
				deserializer->subject = expand_base (deserializer, str);
				g_free (str);
			} else if (parse_terminal (deserializer, terminal_PNAME_LN, 0, &str) ||
			           parse_terminal (deserializer, terminal_PNAME_NS, 0, &str)) {
				deserializer->subject = expand_prefix (deserializer, str, error);
				g_free (str);

				if (*error) {
					return FALSE;
				}
			} else if (parse_terminal (deserializer, terminal_BLANK_NODE_LABEL, 0, &str)) {
				deserializer->subject = str;
			} else {
				g_set_error (error,
				             TRACKER_SPARQL_ERROR,
				             TRACKER_SPARQL_ERROR_PARSE,
				             "Wrong subject token");
				return FALSE;
			}

			deserializer->state = STATE_PREDICATE;
			break;
		case STATE_PREDICATE:
			g_clear_pointer (&deserializer->predicate, g_free);

			if (parse_token (deserializer, "a")) {
				deserializer->predicate = g_strdup (RDF_TYPE);
			} else if (parse_terminal (deserializer, terminal_IRIREF, 1, &str)) {
				deserializer->predicate = expand_base (deserializer, str);
				g_free (str);
			} else if (parse_terminal (deserializer, terminal_PNAME_LN, 0, &str) ||
			           parse_terminal (deserializer, terminal_PNAME_NS, 0, &str)) {
				deserializer->predicate = expand_prefix (deserializer, str, error);
				g_free (str);

				if (*error) {
					return FALSE;
				}
			} else {
				g_set_error (error,
				             TRACKER_SPARQL_ERROR,
				             TRACKER_SPARQL_ERROR_PARSE,
				             "Wrong predicate token");
				return FALSE;
			}

			deserializer->state = STATE_OBJECT;
			break;
		case STATE_OBJECT:
			g_clear_pointer (&deserializer->object, g_free);
			g_clear_pointer (&deserializer->object_lang, g_free);
			deserializer->object_is_uri = FALSE;

			if (parse_token (deserializer, "[")) {
				/* Anonymous blank node */
				push_stack (deserializer);
				g_clear_pointer (&deserializer->subject, g_free);
				deserializer->subject = tracker_generate_uuid ("_:bnode");
				deserializer->state = STATE_PREDICATE;
				continue;
			}

			if (!maybe_expand_buffer (deserializer, error))
				return FALSE;

			if (parse_terminal (deserializer, terminal_IRIREF, 1, &str)) {
				deserializer->object = expand_base (deserializer, str);
				deserializer->object_is_uri = TRUE;
				g_free (str);
			} else if (parse_terminal (deserializer, terminal_PNAME_LN, 0, &str) ||
			           parse_terminal (deserializer, terminal_PNAME_NS, 0, &str)) {
				deserializer->object = expand_prefix (deserializer, str, error);
				deserializer->object_is_uri = TRUE;
				g_free (str);

				if (*error) {
					return FALSE;
				}
			} else if (parse_terminal (deserializer, terminal_BLANK_NODE_LABEL, 0, &str)) {
				deserializer->object = str;
				deserializer->object_is_uri = TRUE;
			} else if (parse_terminal (deserializer, terminal_STRING_LITERAL_LONG1, 3, &str) ||
			           parse_terminal (deserializer, terminal_STRING_LITERAL_LONG2, 3, &str)) {
				deserializer->object = g_strcompress (str);
				g_free (str);
				if (parse_terminal (deserializer, terminal_LANGTAG, 0, &lang)) {
					deserializer->object_lang = lang;
				} else if (!handle_type_cast (deserializer, error)) {
					return FALSE;
				}
			} else if (parse_terminal (deserializer, terminal_STRING_LITERAL1, 1, &str) ||
			           parse_terminal (deserializer, terminal_STRING_LITERAL2, 1, &str)) {
				deserializer->object = g_strcompress (str);
				g_free (str);
				if (parse_terminal (deserializer, terminal_LANGTAG, 0, &lang)) {
					deserializer->object_lang = lang;
				} else if (!handle_type_cast (deserializer, error)) {
					return FALSE;
				}
			} else if (parse_terminal (deserializer, terminal_DOUBLE_POSITIVE, 0, &str) ||
			           parse_terminal (deserializer, terminal_DECIMAL_POSITIVE, 0, &str) ||
			           parse_terminal (deserializer, terminal_INTEGER_POSITIVE, 0, &str) ||
			           parse_terminal (deserializer, terminal_DOUBLE_NEGATIVE, 0, &str) ||
			           parse_terminal (deserializer, terminal_DECIMAL_NEGATIVE, 0, &str) ||
			           parse_terminal (deserializer, terminal_INTEGER_NEGATIVE, 0, &str) ||
			           parse_terminal (deserializer, terminal_DOUBLE, 0, &str) ||
			           parse_terminal (deserializer, terminal_DECIMAL, 0, &str) ||
			           parse_terminal (deserializer, terminal_INTEGER, 0, &str)) {
				deserializer->object = str;
			} else if (parse_token (deserializer, "true")) {
				deserializer->object = g_strdup ("true");
			} else if (parse_token (deserializer, "false")) {
				deserializer->object = g_strdup ("false");
			} else {
				g_set_error (error,
				             TRACKER_SPARQL_ERROR,
				             TRACKER_SPARQL_ERROR_PARSE,
				             "Wrong object token");
				return FALSE;
			}

			deserializer->state = STATE_STEP;

			/* This is where next() stops, on lack of errors */
			return TRUE;
			break;
		case STATE_STEP:
			if (deserializer->parser_state->len > 0 && parse_token (deserializer, "]")) {
				pop_stack (deserializer);
				if (deserializer->state == STATE_SUBJECT) {
					deserializer->state = STATE_PREDICATE;
					continue;
				} else if (deserializer->state == STATE_OBJECT) {
					deserializer->state = STATE_STEP;
					return TRUE;
				}
			}

			if (parse_token (deserializer, ",")) {
				deserializer->state = STATE_OBJECT;
				break;
			}

			if (parse_token (deserializer, ";")) {
				advance_whitespace_and_comments (deserializer);
				deserializer->state = STATE_PREDICATE;
				/* Dot is allowed after semicolon, continue here */
			}

			if (parse_token (deserializer, ".")) {
				advance_whitespace_and_comments (deserializer);
				deserializer->state = deserializer->parse_trig ?
					STATE_SUBJECT : STATE_INITIAL;
			}

			if (deserializer->parse_trig &&
			    parse_token (deserializer, "}")) {
				advance_whitespace_and_comments (deserializer);
				deserializer->state = STATE_INITIAL;
			}

			/* If we did not advance state, this is a parsing error */
			if (deserializer->state == STATE_STEP) {
				g_set_error (error,
				             TRACKER_SPARQL_ERROR,
				             TRACKER_SPARQL_ERROR_PARSE,
				             "Expected comma, semicolon, or dot");
				return FALSE;
			}

			break;
		}
	}
}

static TrackerSparqlValueType
tracker_deserializer_turtle_get_value_type (TrackerSparqlCursor *cursor,
                                            gint                 column)
{
	TrackerDeserializerTurtle *deserializer = TRACKER_DESERIALIZER_TURTLE (cursor);

	switch (column) {
	case TRACKER_RDF_COL_SUBJECT:
		if (g_str_has_prefix (deserializer->subject, "_:"))
			return TRACKER_SPARQL_VALUE_TYPE_BLANK_NODE;
		else
			return TRACKER_SPARQL_VALUE_TYPE_URI;
	case TRACKER_RDF_COL_PREDICATE:
		return TRACKER_SPARQL_VALUE_TYPE_URI;
	case TRACKER_RDF_COL_OBJECT:
		if (deserializer->object_is_uri &&
		    g_str_has_prefix (deserializer->object, "_:"))
			return TRACKER_SPARQL_VALUE_TYPE_BLANK_NODE;
		else if (deserializer->object_is_uri)
			return TRACKER_SPARQL_VALUE_TYPE_URI;
		else
			return TRACKER_SPARQL_VALUE_TYPE_STRING;
	case TRACKER_RDF_COL_GRAPH:
		if (deserializer->parse_trig && deserializer->graph)
			return TRACKER_SPARQL_VALUE_TYPE_URI;
		else
			return TRACKER_SPARQL_VALUE_TYPE_UNBOUND;
	default:
		return TRACKER_SPARQL_VALUE_TYPE_UNBOUND;
	}
}

static const gchar *
tracker_deserializer_turtle_get_string (TrackerSparqlCursor  *cursor,
                                        gint                  column,
                                        const gchar         **langtag,
                                        glong                *length)
{
	TrackerDeserializerTurtle *deserializer = TRACKER_DESERIALIZER_TURTLE (cursor);
	const gchar *str = NULL;

	if (length)
		*length = 0;
	if (langtag)
		*langtag = NULL;

	switch (column) {
	case TRACKER_RDF_COL_SUBJECT:
		str = deserializer->subject;
		break;
	case TRACKER_RDF_COL_PREDICATE:
		str = deserializer->predicate;
		break;
	case TRACKER_RDF_COL_OBJECT:
		if (langtag && deserializer->object_lang) {
			/* Skip '@' starting langtag */
			*langtag = &deserializer->object_lang[1];
		}
		str = deserializer->object;
		break;
	case TRACKER_RDF_COL_GRAPH:
		str = deserializer->graph;
		break;
	}

	if (length && str)
		*length = strlen (str);

	return str;
}

static gboolean
tracker_deserializer_turtle_next (TrackerSparqlCursor  *cursor,
                                  GCancellable         *cancellable,
                                  GError              **error)
{
	TrackerDeserializerTurtle *deserializer = TRACKER_DESERIALIZER_TURTLE (cursor);

	return tracker_deserializer_turtle_iterate_next (deserializer, error);
}

void
tracker_deserializer_turtle_close (TrackerSparqlCursor* cursor)
{
	TrackerDeserializerTurtle *deserializer = TRACKER_DESERIALIZER_TURTLE (cursor);

	g_input_stream_close (G_INPUT_STREAM (deserializer->buffered_stream), NULL, NULL);

	TRACKER_SPARQL_CURSOR_CLASS (tracker_deserializer_turtle_parent_class)->close (cursor);
}

static gboolean
tracker_deserializer_turtle_get_parser_location (TrackerDeserializer *deserializer,
                                                 goffset             *line_no,
                                                 goffset             *column_no)
{
	TrackerDeserializerTurtle *deserializer_ttl = TRACKER_DESERIALIZER_TURTLE (deserializer);

	*line_no = deserializer_ttl->line_no;
	*column_no = deserializer_ttl->column_no;
	return TRUE;
}

static void
tracker_deserializer_turtle_class_init (TrackerDeserializerTurtleClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	TrackerSparqlCursorClass *cursor_class = TRACKER_SPARQL_CURSOR_CLASS (klass);
	TrackerDeserializerClass *deserializer_class = TRACKER_DESERIALIZER_CLASS (klass);

	object_class->finalize = tracker_deserializer_turtle_finalize;
	object_class->constructed = tracker_deserializer_turtle_constructed;

	cursor_class->get_value_type = tracker_deserializer_turtle_get_value_type;
	cursor_class->get_string = tracker_deserializer_turtle_get_string;
	cursor_class->next = tracker_deserializer_turtle_next;
	cursor_class->close = tracker_deserializer_turtle_close;

	deserializer_class->get_parser_location = tracker_deserializer_turtle_get_parser_location;
}

static void
tracker_deserializer_turtle_init (TrackerDeserializerTurtle *deserializer)
{
	deserializer->parser_state = g_array_new (FALSE, FALSE, sizeof (StateStack));
	g_array_set_clear_func (deserializer->parser_state,
	                        (GDestroyNotify) clear_parser_state);
}

TrackerSparqlCursor *
tracker_deserializer_turtle_new (GInputStream            *istream,
                                 TrackerNamespaceManager *namespaces)
{
	g_return_val_if_fail (G_IS_INPUT_STREAM (istream), NULL);

	return g_object_new (TRACKER_TYPE_DESERIALIZER_TURTLE,
	                     "stream", istream,
	                     "namespace-manager", namespaces,
	                     "has-graph", FALSE,
	                     NULL);
}

TrackerSparqlCursor *
tracker_deserializer_trig_new (GInputStream            *istream,
                               TrackerNamespaceManager *namespaces)
{
	g_return_val_if_fail (G_IS_INPUT_STREAM (istream), NULL);

	return g_object_new (TRACKER_TYPE_DESERIALIZER_TURTLE,
	                     "stream", istream,
	                     "namespace-manager", namespaces,
	                     "has-graph", TRUE,
	                     NULL);
}
