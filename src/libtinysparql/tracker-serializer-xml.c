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

/* Serialization of cursors to the XML format defined at:
 * https://www.w3.org/TR/2013/REC-rdf-sparql-XMLres-20130321/
 */

#include "config.h"

#include "tracker-serializer-xml.h"

#include <libxml/xmlwriter.h>

/* Make required type casts a bit more descriptive. */
#define XML(x) ((const xmlChar *) x)

struct _TrackerSerializerXml
{
	TrackerSerializer parent_instance;
	xmlBufferPtr buffer;
	xmlTextWriterPtr writer;
	GPtrArray *vars;
	gssize current_pos;

	guint stream_closed : 1;
	guint cursor_started : 1;
	guint cursor_finished : 1;
	guint head_printed : 1;
};

G_DEFINE_TYPE (TrackerSerializerXml, tracker_serializer_xml,
               TRACKER_TYPE_SERIALIZER)

static void
tracker_serializer_xml_finalize (GObject *object)
{
	g_input_stream_close (G_INPUT_STREAM (object), NULL, NULL);

	G_OBJECT_CLASS (tracker_serializer_xml_parent_class)->finalize (object);
}

static gboolean
serialize_up_to_position (TrackerSerializerXml  *serializer_xml,
                          gsize                  pos,
                          GCancellable          *cancellable,
                          GError               **error)
{
	TrackerSparqlCursor *cursor;
	GError *inner_error = NULL;
	gint i;

	if (!serializer_xml->buffer)
		serializer_xml->buffer = xmlBufferCreate ();
	if (!serializer_xml->writer)
		serializer_xml->writer = xmlNewTextWriterMemory (serializer_xml->buffer, 0);
	if (!serializer_xml->vars)
		serializer_xml->vars = g_ptr_array_new_with_free_func (g_free);

	cursor = tracker_serializer_get_cursor (TRACKER_SERIALIZER (serializer_xml));

	if (!serializer_xml->head_printed) {
		xmlTextWriterStartDocument (serializer_xml->writer, "1.0", "UTF-8", NULL);

		if (xmlTextWriterStartElement (serializer_xml->writer, XML ("sparql")) < 0)
			goto error;

		if (xmlTextWriterStartElement (serializer_xml->writer, XML ("head")) < 0)
			goto error;

		for (i = 0; i < tracker_sparql_cursor_get_n_columns (cursor); i++) {
			const gchar *var;

			var = tracker_sparql_cursor_get_variable_name (cursor, i);

			if (xmlTextWriterStartElement (serializer_xml->writer, XML ("variable")) < 0)
				goto error;

			if (var && *var) {
				g_ptr_array_add (serializer_xml->vars,
				                 g_strdup (var));
			} else {
				g_ptr_array_add (serializer_xml->vars,
				                 g_strdup_printf ("var%d", i + 1));
			}

			if (xmlTextWriterWriteFormatAttribute (serializer_xml->writer,
			                                       XML ("name"),
			                                       "%s",
			                                       (char *) g_ptr_array_index (serializer_xml->vars, i)) < 0)
				goto error;

			xmlTextWriterEndElement (serializer_xml->writer);
		}

		xmlTextWriterEndElement (serializer_xml->writer);

		if (xmlTextWriterStartElement (serializer_xml->writer, XML ("results")) < 0)
			goto error;

		serializer_xml->head_printed = TRUE;
	}

	while (!serializer_xml->cursor_finished &&
	       (gsize) xmlBufferLength (serializer_xml->buffer) < pos) {
		if (!tracker_sparql_cursor_next (cursor, cancellable, &inner_error)) {
			if (inner_error) {
				g_propagate_error (error, inner_error);
				return FALSE;
			} else {
				xmlTextWriterEndElement (serializer_xml->writer);
				xmlTextWriterEndElement (serializer_xml->writer);
				xmlTextWriterEndDocument (serializer_xml->writer);
				serializer_xml->cursor_finished = TRUE;
				break;
			}
		} else {
			serializer_xml->cursor_started = TRUE;
		}

		if (xmlTextWriterStartElement (serializer_xml->writer, XML ("result")) < 0)
			goto error;

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

			var = g_ptr_array_index (serializer_xml->vars, i);

			if (xmlTextWriterStartElement (serializer_xml->writer, XML ("binding")) < 0)
				goto error;

			if (xmlTextWriterWriteFormatAttribute (serializer_xml->writer,
			                                       XML ("name"),
			                                       "%s",
			                                       var) < 0)
				goto error;

			if (xmlTextWriterStartElement (serializer_xml->writer, XML (type)) < 0)
				goto error;

			str = tracker_sparql_cursor_get_langstring (cursor, i, &langtag, NULL);

			if (langtag) {
				datatype = TRACKER_PREFIX_RDF "langString";

				if (xmlTextWriterWriteFormatAttribute (serializer_xml->writer,
				                                       XML ("xml:lang"),
				                                       "%s",
				                                       langtag) < 0)
					goto error;
			}

			if (datatype) {
				if (xmlTextWriterWriteFormatAttribute (serializer_xml->writer,
				                                       XML ("datatype"),
				                                       "%s",
				                                       datatype) < 0)
					goto error;
			}

			if (str) {
				if (xmlTextWriterWriteRaw (serializer_xml->writer, XML (str)) < 0)
					goto error;
			}

			xmlTextWriterEndElement (serializer_xml->writer);
			xmlTextWriterEndElement (serializer_xml->writer);
		}

		xmlTextWriterEndElement (serializer_xml->writer);
	}

	return TRUE;

 error:
	g_set_error_literal (error,
	                     TRACKER_SPARQL_ERROR,
	                     TRACKER_SPARQL_ERROR_INTERNAL,
	                     "Error writing XML cursor content");
	return FALSE;
}

static gssize
tracker_serializer_xml_read (GInputStream  *istream,
                             gpointer       buffer,
                             gsize          count,
                             GCancellable  *cancellable,
                             GError       **error)
{
	TrackerSerializerXml *serializer_xml = TRACKER_SERIALIZER_XML (istream);
	gsize bytes_unflushed, bytes_copied;
	const xmlChar *xml_buf;

	if (serializer_xml->stream_closed ||
	    (serializer_xml->cursor_finished &&
	     serializer_xml->current_pos == xmlBufferLength (serializer_xml->buffer)))
		return 0;

	if (!serialize_up_to_position (serializer_xml,
	                               serializer_xml->current_pos + count,
	                               cancellable,
	                               error))
		return -1;

	bytes_unflushed =
		xmlBufferLength (serializer_xml->buffer) - serializer_xml->current_pos;
	bytes_copied = MIN (count, bytes_unflushed);

	xml_buf = xmlBufferContent (serializer_xml->buffer);

	memcpy (buffer,
	        &xml_buf[serializer_xml->current_pos],
	        bytes_copied);
	serializer_xml->current_pos += bytes_copied;

	return bytes_copied;
}

static gboolean
tracker_serializer_xml_close (GInputStream  *istream,
                              GCancellable  *cancellable,
                              GError       **error)
{
	TrackerSerializerXml *serializer_xml = TRACKER_SERIALIZER_XML (istream);

	serializer_xml->stream_closed = TRUE;
	g_clear_pointer (&serializer_xml->buffer, xmlBufferFree);
	g_clear_pointer (&serializer_xml->writer, xmlFreeTextWriter);

	return TRUE;
}

static void
tracker_serializer_xml_class_init (TrackerSerializerXmlClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GInputStreamClass *istream_class = G_INPUT_STREAM_CLASS (klass);

	object_class->finalize = tracker_serializer_xml_finalize;

	istream_class->read_fn = tracker_serializer_xml_read;
	istream_class->close_fn = tracker_serializer_xml_close;
}

static void
tracker_serializer_xml_init (TrackerSerializerXml *serializer)
{
}
