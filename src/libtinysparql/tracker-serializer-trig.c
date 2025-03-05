/*
 * Copyright (C) 2021, Red Hat, Inc
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

/* Serialization of cursors to the TRIG format defined at:
 *  http://www.w3.org/TR/trig/
 */

#include "config.h"

#include "tracker-serializer-trig.h"

typedef struct _TrackerQuad TrackerQuad;

struct _TrackerQuad
{
	gchar *subject;
	gchar *predicate;
	gchar *object;
	gchar *graph;
	gchar *object_langtag;
	TrackerSparqlValueType subject_type;
	TrackerSparqlValueType object_type;
};

struct _TrackerSerializerTrig
{
	TrackerSerializer parent_instance;
	TrackerQuad last_quad;
	GString *data;
	guint stream_closed : 1;
	guint cursor_started : 1;
	guint cursor_finished : 1;
	guint head_printed : 1;
	guint has_quads : 1;
};

G_DEFINE_TYPE (TrackerSerializerTrig, tracker_serializer_trig,
               TRACKER_TYPE_SERIALIZER)

typedef enum
{
	TRACKER_QUAD_BREAK_NONE,
	TRACKER_QUAD_BREAK_GRAPH,
	TRACKER_QUAD_BREAK_SUBJECT,
	TRACKER_QUAD_BREAK_PREDICATE,
	TRACKER_QUAD_BREAK_OBJECT,
} TrackerQuadBreak;

static void
tracker_quad_init_from_cursor (TrackerQuad         *quad,
                               TrackerSparqlCursor *cursor)
{
	const gchar *langtag;

	quad->subject_type = tracker_sparql_cursor_get_value_type (cursor, 0);
	quad->object_type = tracker_sparql_cursor_get_value_type (cursor, 2);
	quad->subject = g_strdup (tracker_sparql_cursor_get_string (cursor, 0, NULL));
	quad->predicate = g_strdup (tracker_sparql_cursor_get_string (cursor, 1, NULL));
	quad->object = g_strdup (tracker_sparql_cursor_get_langstring (cursor, 2, &langtag, NULL));
	if (quad->object)
		quad->object_langtag = g_strdup (langtag);

	if (tracker_sparql_cursor_get_n_columns (cursor) >= 4)
		quad->graph = g_strdup (tracker_sparql_cursor_get_string (cursor, 3, NULL));
	else
		quad->graph = NULL;

	if (quad->subject_type == TRACKER_SPARQL_VALUE_TYPE_STRING) {
		if (g_str_has_prefix (quad->subject, "urn:bnode:")) {
			quad->subject_type = TRACKER_SPARQL_VALUE_TYPE_BLANK_NODE;
		} else {
			quad->subject_type = TRACKER_SPARQL_VALUE_TYPE_URI;
		}
	}

	if (quad->object_type == TRACKER_SPARQL_VALUE_TYPE_STRING) {
		if (g_str_has_prefix (quad->object, "urn:bnode:")) {
			quad->object_type = TRACKER_SPARQL_VALUE_TYPE_BLANK_NODE;
		}
	}
}

static void
tracker_quad_clear (TrackerQuad *quad)
{
	g_clear_pointer (&quad->subject, g_free);
	g_clear_pointer (&quad->predicate, g_free);
	g_clear_pointer (&quad->object, g_free);
	g_clear_pointer (&quad->graph, g_free);
	g_clear_pointer (&quad->object_langtag, g_free);
}

static TrackerQuadBreak
tracker_quad_get_break (TrackerQuad *last,
                        TrackerQuad *cur)
{
	if (!last->subject)
		return TRACKER_QUAD_BREAK_NONE;

	if (g_strcmp0 (last->graph, cur->graph) != 0)
		return TRACKER_QUAD_BREAK_GRAPH;

	if (g_strcmp0 (last->subject, cur->subject) != 0)
		return TRACKER_QUAD_BREAK_SUBJECT;

	if (g_strcmp0 (last->predicate, cur->predicate) != 0)
		return TRACKER_QUAD_BREAK_PREDICATE;

	return TRACKER_QUAD_BREAK_OBJECT;
}

static void
tracker_serializer_trig_finalize (GObject *object)
{
	g_input_stream_close (G_INPUT_STREAM (object), NULL, NULL);

	G_OBJECT_CLASS (tracker_serializer_trig_parent_class)->finalize (object);
}

static void
print_value (GString                 *str,
             const gchar             *value,
             TrackerSparqlValueType   value_type,
             TrackerNamespaceManager *namespaces)
{
	g_assert (value != NULL);

	switch (value_type) {
	case TRACKER_SPARQL_VALUE_TYPE_URI: {
		gchar *shortname;

		shortname = tracker_namespace_manager_compress_uri (namespaces, value);

		if (shortname) {
			g_string_append (str, shortname);
		} else {
			g_string_append_c (str, '<');
			g_string_append (str, value);
			g_string_append_c (str, '>');
		}

		g_free (shortname);
		break;
	}
	case TRACKER_SPARQL_VALUE_TYPE_BLANK_NODE: {
		gchar *bnode_label;

		if (g_str_has_prefix (value, "_:")) {
			g_string_append (str, value);
		} else {
			bnode_label = g_strdelimit (g_strdup (value), ":", '_');
			g_string_append (str, "_:");
			g_string_append (str, bnode_label);
			g_free (bnode_label);
		}
		break;
	}
	case TRACKER_SPARQL_VALUE_TYPE_STRING:
	case TRACKER_SPARQL_VALUE_TYPE_DATETIME: {
		gchar *escaped;

		escaped = tracker_sparql_escape_string (value);
		g_string_append_c (str, '"');
		g_string_append (str, escaped);
		g_string_append_c (str, '"');
		g_free (escaped);
		break;
	}
	case TRACKER_SPARQL_VALUE_TYPE_INTEGER:
	case TRACKER_SPARQL_VALUE_TYPE_DOUBLE:
		g_string_append (str, value);
		break;
	case TRACKER_SPARQL_VALUE_TYPE_BOOLEAN:
		g_string_append (str,
		                 (value[0] == 't' || value[0] == 'T') ?
		                 "true" : "false");
		break;
	default:
		g_assert_not_reached ();
	}
}

static gboolean
serialize_up_to_size (TrackerSerializerTrig  *serializer_trig,
                      gsize                   size,
                      GCancellable           *cancellable,
                      GError                **error)
{
	TrackerSparqlCursor *cursor;
	TrackerNamespaceManager *namespaces;
	GError *inner_error = NULL;
	TrackerQuad cur;

	if (!serializer_trig->data)
		serializer_trig->data = g_string_new (NULL);

	cursor = tracker_serializer_get_cursor (TRACKER_SERIALIZER (serializer_trig));
	namespaces = tracker_serializer_get_namespaces (TRACKER_SERIALIZER (serializer_trig));

	if (!serializer_trig->head_printed) {
		gchar *str;

		str = tracker_namespace_manager_print_turtle (namespaces);

		g_string_append (serializer_trig->data, str);
		g_string_append_c (serializer_trig->data, '\n');
		g_free (str);
		serializer_trig->head_printed = TRUE;
	}

	while (!serializer_trig->cursor_finished &&
	       serializer_trig->data->len < size) {
		TrackerQuadBreak br;

		if (!tracker_sparql_cursor_next (cursor, cancellable, &inner_error)) {
			if (inner_error) {
				g_propagate_error (error, inner_error);
				return FALSE;
			} else {
				serializer_trig->cursor_finished = TRUE;
				break;
			}
		} else {
			serializer_trig->cursor_started = TRUE;
		}

		tracker_quad_init_from_cursor (&cur, cursor);

		if (!cur.subject && !cur.predicate && !cur.object) {
			g_set_error (error,
			             TRACKER_SPARQL_ERROR,
			             TRACKER_SPARQL_ERROR_INTERNAL,
			             "Cursor has no subject/predicate/object/graph columns");
			tracker_quad_clear (&cur);
			return FALSE;
		}

		br = tracker_quad_get_break (&serializer_trig->last_quad, &cur);

		if (br <= TRACKER_QUAD_BREAK_GRAPH) {
			if (br == TRACKER_QUAD_BREAK_GRAPH)
				g_string_append (serializer_trig->data, " .\n}\n\n");

			if (cur.graph) {
				g_string_append (serializer_trig->data, "GRAPH ");
				print_value (serializer_trig->data, cur.graph,
				             TRACKER_SPARQL_VALUE_TYPE_URI, namespaces);
				g_string_append_c (serializer_trig->data, ' ');
			}

			g_string_append (serializer_trig->data, "{\n  ");
		}

		if (br <= TRACKER_QUAD_BREAK_SUBJECT) {
			if (br == TRACKER_QUAD_BREAK_SUBJECT)
				g_string_append (serializer_trig->data, " .\n\n  ");
			print_value (serializer_trig->data, cur.subject, cur.subject_type, namespaces);
		}

		if (br <= TRACKER_QUAD_BREAK_PREDICATE) {
			if (br == TRACKER_QUAD_BREAK_PREDICATE)
				g_string_append (serializer_trig->data, " ;\n    ");
			else
				g_string_append_c (serializer_trig->data, ' ');

			print_value (serializer_trig->data, cur.predicate,
			             TRACKER_SPARQL_VALUE_TYPE_URI, namespaces);
		}

		if (br <= TRACKER_QUAD_BREAK_OBJECT) {
			if (br == TRACKER_QUAD_BREAK_OBJECT)
				g_string_append (serializer_trig->data, ",");

			g_string_append_c (serializer_trig->data, ' ');
			print_value (serializer_trig->data, cur.object, cur.object_type, namespaces);

			if (cur.object_langtag) {
				g_string_append_c (serializer_trig->data, '@');
				g_string_append (serializer_trig->data, cur.object_langtag);
			}
		}

		serializer_trig->has_quads = TRUE;
		tracker_quad_clear (&serializer_trig->last_quad);
		memcpy (&serializer_trig->last_quad, &cur, sizeof (TrackerQuad));
	}

	/* Close the last quad */
	if (serializer_trig->cursor_finished &&
	    serializer_trig->has_quads) {
		g_string_append (serializer_trig->data, " .\n}\n");
		serializer_trig->has_quads = FALSE;
	}

	return TRUE;
}

static gssize
tracker_serializer_trig_read (GInputStream  *istream,
                              gpointer       buffer,
                              gsize          count,
                              GCancellable  *cancellable,
                              GError       **error)
{
	TrackerSerializerTrig *serializer_trig = TRACKER_SERIALIZER_TRIG (istream);
	gsize bytes_copied;

	if (serializer_trig->stream_closed ||
	    (serializer_trig->cursor_finished &&
	     serializer_trig->data->len == 0))
		return 0;

	if (!serialize_up_to_size (serializer_trig,
				   count,
				   cancellable,
				   error))
		return -1;

	bytes_copied = MIN (count, serializer_trig->data->len);

	memcpy (buffer,
	        serializer_trig->data->str,
	        bytes_copied);
	g_string_erase (serializer_trig->data, 0, bytes_copied);

	return bytes_copied;
}

static gboolean
tracker_serializer_trig_close (GInputStream  *istream,
                               GCancellable  *cancellable,
                               GError       **error)
{
	TrackerSerializerTrig *serializer_trig = TRACKER_SERIALIZER_TRIG (istream);

	tracker_quad_clear (&serializer_trig->last_quad);

	if (serializer_trig->data) {
		g_string_free (serializer_trig->data, TRUE);
		serializer_trig->data = NULL;
	}

	return TRUE;
}

static void
tracker_serializer_trig_class_init (TrackerSerializerTrigClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GInputStreamClass *istream_class = G_INPUT_STREAM_CLASS (klass);

	object_class->finalize = tracker_serializer_trig_finalize;

	istream_class->read_fn = tracker_serializer_trig_read;
	istream_class->close_fn = tracker_serializer_trig_close;
}

static void
tracker_serializer_trig_init (TrackerSerializerTrig *serializer)
{
}
