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

/* Serialization of cursors to the turtle format defined at:
 *  https://www.w3.org/TR/turtle/
 */

#include "config.h"

#include "tracker-serializer-turtle.h"

typedef struct _TrackerTriple TrackerTriple;

struct _TrackerTriple
{
	gchar *subject;
	gchar *predicate;
	gchar *object;
	gchar *object_langtag;
	TrackerSparqlValueType subject_type;
	TrackerSparqlValueType object_type;
};

struct _TrackerSerializerTurtle
{
	TrackerSerializer parent_instance;
	TrackerTriple last_triple;
	GString *data;
	guint stream_closed : 1;
	guint cursor_started : 1;
	guint cursor_finished : 1;
	guint head_printed : 1;
	guint has_triples : 1;
};

G_DEFINE_TYPE (TrackerSerializerTurtle, tracker_serializer_turtle,
               TRACKER_TYPE_SERIALIZER)

typedef enum
{
	TRACKER_TRIPLE_BREAK_NONE,
	TRACKER_TRIPLE_BREAK_SUBJECT,
	TRACKER_TRIPLE_BREAK_PREDICATE,
	TRACKER_TRIPLE_BREAK_OBJECT,
} TrackerTripleBreak;

static void
tracker_triple_init_from_cursor (TrackerTriple       *triple,
                                 TrackerSparqlCursor *cursor)
{
	const gchar *langtag;

	triple->subject_type = tracker_sparql_cursor_get_value_type (cursor, 0);
	triple->object_type = tracker_sparql_cursor_get_value_type (cursor, 2);
	triple->subject = g_strdup (tracker_sparql_cursor_get_string (cursor, 0, NULL));
	triple->predicate = g_strdup (tracker_sparql_cursor_get_string (cursor, 1, NULL));
	triple->object = g_strdup (tracker_sparql_cursor_get_langstring (cursor, 2, &langtag, NULL));
	if (triple->object)
		triple->object_langtag = g_strdup (langtag);

	if (triple->subject_type == TRACKER_SPARQL_VALUE_TYPE_STRING) {
		if (g_str_has_prefix (triple->subject, "urn:bnode:")) {
			triple->subject_type = TRACKER_SPARQL_VALUE_TYPE_BLANK_NODE;
		} else {
			triple->subject_type = TRACKER_SPARQL_VALUE_TYPE_URI;
		}
	}

	if (triple->object_type == TRACKER_SPARQL_VALUE_TYPE_STRING) {
		if (g_str_has_prefix (triple->object, "urn:bnode:")) {
			triple->object_type = TRACKER_SPARQL_VALUE_TYPE_BLANK_NODE;
		}
	}
}

static void
tracker_triple_clear (TrackerTriple *triple)
{
	g_clear_pointer (&triple->subject, g_free);
	g_clear_pointer (&triple->predicate, g_free);
	g_clear_pointer (&triple->object, g_free);
	g_clear_pointer (&triple->object_langtag, g_free);
}

static TrackerTripleBreak
tracker_triple_get_break (TrackerTriple *last,
                          TrackerTriple *cur)
{
	if (!last->subject)
		return TRACKER_TRIPLE_BREAK_NONE;

	if (g_strcmp0 (last->subject, cur->subject) != 0)
		return TRACKER_TRIPLE_BREAK_SUBJECT;

	if (g_strcmp0 (last->predicate, cur->predicate) != 0)
		return TRACKER_TRIPLE_BREAK_PREDICATE;

	return TRACKER_TRIPLE_BREAK_OBJECT;
}

static void
tracker_serializer_turtle_finalize (GObject *object)
{
	g_input_stream_close (G_INPUT_STREAM (object), NULL, NULL);

	G_OBJECT_CLASS (tracker_serializer_turtle_parent_class)->finalize (object);
}

static void
print_value (GString                 *str,
             const gchar             *value,
             TrackerSparqlValueType   value_type,
             TrackerNamespaceManager *namespaces)
{
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
serialize_up_to_size (TrackerSerializerTurtle *serializer_ttl,
                      gsize                    size,
                      GCancellable            *cancellable,
                      GError                 **error)
{
	TrackerSparqlCursor *cursor;
	TrackerNamespaceManager *namespaces;
	GError *inner_error = NULL;
	TrackerTriple cur;

	if (!serializer_ttl->data)
		serializer_ttl->data = g_string_new (NULL);

	cursor = tracker_serializer_get_cursor (TRACKER_SERIALIZER (serializer_ttl));
	namespaces = tracker_serializer_get_namespaces (TRACKER_SERIALIZER (serializer_ttl));

	if (!serializer_ttl->head_printed) {
		gchar *str;

		str = tracker_namespace_manager_print_turtle (namespaces);

		g_string_append (serializer_ttl->data, str);
		g_string_append_c (serializer_ttl->data, '\n');
		g_free (str);
		serializer_ttl->head_printed = TRUE;
	}

	while (!serializer_ttl->cursor_finished &&
	       serializer_ttl->data->len < size) {
		TrackerTripleBreak br;

		if (!tracker_sparql_cursor_next (cursor, cancellable, &inner_error)) {
			if (inner_error) {
				g_propagate_error (error, inner_error);
				return FALSE;
			} else {
				serializer_ttl->cursor_finished = TRUE;
				break;
			}
		} else {
			serializer_ttl->cursor_started = TRUE;
		}

		tracker_triple_init_from_cursor (&cur, cursor);

		if (!cur.subject || !cur.predicate || !cur.object) {
			g_set_error (error,
			             TRACKER_SPARQL_ERROR,
			             TRACKER_SPARQL_ERROR_INTERNAL,
			             "Cursor has no subject/predicate/object columns");
			tracker_triple_clear (&cur);
			return FALSE;
		}

		br = tracker_triple_get_break (&serializer_ttl->last_triple, &cur);

		if (br <= TRACKER_TRIPLE_BREAK_SUBJECT) {
			if (br == TRACKER_TRIPLE_BREAK_SUBJECT)
				g_string_append (serializer_ttl->data, " .\n\n");
			print_value (serializer_ttl->data, cur.subject, cur.subject_type, namespaces);
		}

		if (br <= TRACKER_TRIPLE_BREAK_PREDICATE) {
			if (br == TRACKER_TRIPLE_BREAK_PREDICATE)
				g_string_append (serializer_ttl->data, " ;\n  ");
			else
				g_string_append_c (serializer_ttl->data, ' ');

			print_value (serializer_ttl->data, cur.predicate,
			             TRACKER_SPARQL_VALUE_TYPE_URI, namespaces);
		}

		if (br <= TRACKER_TRIPLE_BREAK_OBJECT) {
			if (br == TRACKER_TRIPLE_BREAK_OBJECT)
				g_string_append (serializer_ttl->data, ",");

			g_string_append_c (serializer_ttl->data, ' ');
			print_value (serializer_ttl->data, cur.object, cur.object_type, namespaces);

			if (cur.object_langtag) {
				g_string_append_c (serializer_ttl->data, '@');
				g_string_append (serializer_ttl->data, cur.object_langtag);
			}
		}

		serializer_ttl->has_triples = TRUE;
		tracker_triple_clear (&serializer_ttl->last_triple);
		memcpy (&serializer_ttl->last_triple, &cur, sizeof (TrackerTriple));
	}

	/* Print dot for the last triple */
	if (serializer_ttl->cursor_finished &&
	    serializer_ttl->has_triples)
		g_string_append (serializer_ttl->data, " .\n");

	return TRUE;
}

static gssize
tracker_serializer_turtle_read (GInputStream  *istream,
                                gpointer       buffer,
                                gsize          count,
                                GCancellable  *cancellable,
                                GError       **error)
{
	TrackerSerializerTurtle *serializer_ttl = TRACKER_SERIALIZER_TURTLE (istream);
	gsize bytes_copied;

	if (serializer_ttl->stream_closed ||
	    (serializer_ttl->cursor_finished &&
	     serializer_ttl->data->len == 0))
		return 0;

	if (!serialize_up_to_size (serializer_ttl,
				   count,
				   cancellable,
				   error))
		return -1;

	bytes_copied = MIN (count, serializer_ttl->data->len);

	memcpy (buffer,
	        serializer_ttl->data->str,
	        bytes_copied);
	g_string_erase (serializer_ttl->data, 0, bytes_copied);

	return bytes_copied;
}

static gboolean
tracker_serializer_turtle_close (GInputStream  *istream,
                                 GCancellable  *cancellable,
                                 GError       **error)
{
	TrackerSerializerTurtle *serializer_ttl = TRACKER_SERIALIZER_TURTLE (istream);

	tracker_triple_clear (&serializer_ttl->last_triple);

	if (serializer_ttl->data) {
		g_string_free (serializer_ttl->data, TRUE);
		serializer_ttl->data = NULL;
	}

	return TRUE;
}

static void
tracker_serializer_turtle_class_init (TrackerSerializerTurtleClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GInputStreamClass *istream_class = G_INPUT_STREAM_CLASS (klass);

	object_class->finalize = tracker_serializer_turtle_finalize;

	istream_class->read_fn = tracker_serializer_turtle_read;
	istream_class->close_fn = tracker_serializer_turtle_close;
}

static void
tracker_serializer_turtle_init (TrackerSerializerTurtle *serializer)
{
}
