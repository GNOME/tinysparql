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

/* Deserialization to cursors for the XML format defined at:
 *   https://www.w3.org/TR/2013/REC-rdf-sparql-XMLres-20130321/
 */

#include "config.h"

#include "tracker-deserializer-xml.h"

#include <libxml/xmlreader.h>

typedef struct {
	TrackerSparqlValueType type;
	xmlChar *str;
	xmlChar *langtag;
} ColumnData;

struct _TrackerDeserializerXml {
	TrackerDeserializer parent_instance;
	xmlTextReaderPtr reader;
	GPtrArray *columns;
	GPtrArray *column_names;
	GError *error;
	gboolean started;
};

G_DEFINE_TYPE (TrackerDeserializerXml,
               tracker_deserializer_xml,
               TRACKER_TYPE_DESERIALIZER)

static ColumnData *
column_new (TrackerSparqlValueType  type,
            xmlChar                *str,
            xmlChar                *langtag)
{
	ColumnData *col;

	col = g_slice_new0 (ColumnData);
	col->type = type;
	col->str = str;
	col->langtag = langtag;

	return col;
}

static void
column_free (gpointer data)
{
	ColumnData *col = data;

	xmlFree (col->str);
	xmlFree (col->langtag);
	g_slice_free (ColumnData, col);
}

static void
tracker_deserializer_xml_finalize (GObject *object)
{
	TrackerDeserializerXml *deserializer =
		TRACKER_DESERIALIZER_XML (object);

	g_clear_pointer (&deserializer->reader, xmlFreeTextReader);
	g_ptr_array_unref (deserializer->columns);

	G_OBJECT_CLASS (tracker_deserializer_xml_parent_class)->finalize (object);
}

static int
stream_read (gpointer  context,
             gchar    *buf,
             int       len)
{
	GInputStream *stream = context;

	return g_input_stream_read (stream, buf, len, NULL, NULL);
}

static int
stream_close (gpointer context)
{
	GInputStream *stream = context;

	return g_input_stream_close (stream, NULL, NULL) ? 0 : -1;
}

static void
error_handler (gpointer                 user_data,
               const gchar             *msg,
               xmlParserSeverities      severity,
               xmlTextReaderLocatorPtr  locator)
{
	TrackerDeserializerXml *deserializer = user_data;

	deserializer->error = g_error_new (TRACKER_SPARQL_ERROR,
	                                   TRACKER_SPARQL_ERROR_PARSE,
	                                   "Could not parse XML response: %s",
	                                   msg);
}

static gboolean
reader_in_element (TrackerDeserializerXml *deserializer,
                   const gchar            *name,
                   int                     depth)
{
	return (xmlTextReaderNodeType (deserializer->reader) == XML_READER_TYPE_ELEMENT &&
	        g_strcmp0 ((gchar *) xmlTextReaderConstName (deserializer->reader), name) == 0 &&
	        xmlTextReaderDepth (deserializer->reader) == depth);
}

static gboolean
parse_head (TrackerDeserializerXml  *deserializer,
            GError                 **error)
{
	gboolean seen_link = FALSE;

	if (xmlTextReaderRead(deserializer->reader) <= 0 ||
	    !reader_in_element (deserializer, "head", 1))
		goto error;

	while (xmlTextReaderRead (deserializer->reader) > 0) {
		if (xmlTextReaderNodeType (deserializer->reader) == XML_READER_TYPE_END_ELEMENT)
			break;

		if (reader_in_element (deserializer, "variable", 2)) {
			xmlChar *name;

			if (seen_link) {
				g_set_error (error,
				             TRACKER_SPARQL_ERROR,
				             TRACKER_SPARQL_ERROR_PARSE,
				             "Wrong XML format, variable node found after link");
				break;
			}

			name = xmlTextReaderGetAttribute (deserializer->reader,
			                                  (xmlChar *) "name");
			g_ptr_array_add (deserializer->column_names, name);
		} else if (reader_in_element (deserializer, "link", 2)) {
			/* We do nothing about extra links in headers, but still
			 * mandate that these appear after all variable nodes
			 * as per spec.
			 */
			seen_link = TRUE;
		} else {
			goto error;
		}
	}

	return TRUE;

 error:
	g_set_error (error,
	             TRACKER_SPARQL_ERROR,
	             TRACKER_SPARQL_ERROR_PARSE,
	             "Wrong XML format, unexpected node '%s'",
	             xmlTextReaderConstName (deserializer->reader));

	return FALSE;
}

static void
tracker_deserializer_xml_constructed (GObject *object)
{
	TrackerDeserializerXml *deserializer =
		TRACKER_DESERIALIZER_XML (object);
	GInputStream *stream;

	G_OBJECT_CLASS (tracker_deserializer_xml_parent_class)->constructed (object);

	stream = tracker_deserializer_get_stream (TRACKER_DESERIALIZER (object));

	deserializer->reader = xmlReaderForIO (stream_read,
	                                       stream_close,
	                                       stream,
	                                       NULL, NULL, 0);
	if (deserializer->reader) {
		xmlTextReaderSetErrorHandler (deserializer->reader,
		                              error_handler, deserializer);
	}

	if (deserializer->reader &&
	    xmlTextReaderRead(deserializer->reader) > 0 &&
	    reader_in_element (deserializer, "sparql", 0)) {
		parse_head (deserializer, &deserializer->error);
	} else {
		g_set_error (&deserializer->error,
		             TRACKER_SPARQL_ERROR,
		             TRACKER_SPARQL_ERROR_PARSE,
		             "Wrong XML format, variable node found after link");
	}
}

static gint
tracker_deserializer_xml_get_n_columns (TrackerSparqlCursor  *cursor)
{
	TrackerDeserializerXml *deserializer =
		TRACKER_DESERIALIZER_XML (cursor);

	return deserializer->column_names->len;
}

static TrackerSparqlValueType
tracker_deserializer_xml_get_value_type (TrackerSparqlCursor  *cursor,
                                         gint                  column)
{
	TrackerDeserializerXml *deserializer =
		TRACKER_DESERIALIZER_XML (cursor);
	ColumnData *col;

	if (column < 0 || column >= (gint) deserializer->columns->len)
		return TRACKER_SPARQL_VALUE_TYPE_UNBOUND;

	col = g_ptr_array_index (deserializer->columns, column);

	return col->type;
}

static const gchar *
tracker_deserializer_xml_get_variable_name (TrackerSparqlCursor  *cursor,
                                            gint                  column)
{
	TrackerDeserializerXml *deserializer =
		TRACKER_DESERIALIZER_XML (cursor);

	if (column < 0 || column >= (gint) deserializer->column_names->len)
		return NULL;

	return g_ptr_array_index (deserializer->column_names, column);
}

static const gchar *
tracker_deserializer_xml_get_string (TrackerSparqlCursor  *cursor,
                                     gint                  column,
                                     const gchar         **langtag,
                                     glong                *length)
{
	TrackerDeserializerXml *deserializer =
		TRACKER_DESERIALIZER_XML (cursor);
	ColumnData *col;

	if (length)
		*length = 0;
	if (langtag)
		*langtag = NULL;

	if (column < 0 || column >= (gint) deserializer->columns->len)
		return NULL;

	col = g_ptr_array_index (deserializer->columns, column);

	if (length)
		*length = strlen ((const gchar *) col->str);
	if (langtag)
		*langtag = (const gchar *) col->langtag;

	return (const gchar *) col->str;
}

static gboolean
maybe_propagate_error (TrackerDeserializerXml  *deserializer,
                       GError                 **error)
{
	if (deserializer->error) {
		g_propagate_error (error, deserializer->error);
		deserializer->error = NULL;
		return TRUE;
	}

	return FALSE;
}

static gboolean
parse_binding_type (TrackerDeserializerXml  *deserializer,
                    TrackerSparqlValueType  *type,
                    GError                 **error)
{
	if (reader_in_element (deserializer, "uri", 4)) {
		*type = TRACKER_SPARQL_VALUE_TYPE_URI;
	} else if (reader_in_element (deserializer, "bnode", 4)) {
		*type = TRACKER_SPARQL_VALUE_TYPE_BLANK_NODE;
	} else if (reader_in_element (deserializer, "literal", 4)) {
		xmlChar *datatype;
		const gchar *suffix;

		datatype = xmlTextReaderGetAttribute (deserializer->reader,
		                                      (xmlChar *) "datatype");

		if (!datatype ||
		    !g_str_has_prefix ((const gchar *) datatype, TRACKER_PREFIX_XSD)) {
			*type = TRACKER_SPARQL_VALUE_TYPE_STRING;
			return TRUE;
		}

		suffix = (const gchar *) &datatype[strlen (TRACKER_PREFIX_XSD)];

		if (g_str_equal (suffix, "byte") ||
		    g_str_equal (suffix, "int") ||
		    g_str_equal (suffix, "integer") ||
		    g_str_equal (suffix, "long"))
			*type = TRACKER_SPARQL_VALUE_TYPE_INTEGER;
		else if (g_str_equal (suffix, "decimal") ||
		         g_str_equal (suffix, "double"))
			*type = TRACKER_SPARQL_VALUE_TYPE_DOUBLE;
		else if (g_str_equal (suffix, "date") ||
		         g_str_equal (suffix, "dateTime"))
			*type = TRACKER_SPARQL_VALUE_TYPE_DATETIME;
		else if (g_str_equal (suffix, "boolean"))
			*type = TRACKER_SPARQL_VALUE_TYPE_BOOLEAN;
		else
			*type = TRACKER_SPARQL_VALUE_TYPE_STRING;

		xmlFree (datatype);
	} else {
		g_set_error (error,
		             TRACKER_SPARQL_ERROR,
		             TRACKER_SPARQL_ERROR_PARSE,
		             "Unknown binding type '%s'",
		             xmlTextReaderConstName (deserializer->reader));
		return FALSE;
	}

	return TRUE;
}

static gboolean
parse_binding (TrackerDeserializerXml  *deserializer,
               TrackerSparqlValueType  *type,
               xmlChar                **name,
               xmlChar                **value,
               xmlChar                **langtag,
               GError                 **error)
{
	xmlChar *binding_name = NULL, *binding_value = NULL, *binding_langtag = NULL;

	if (!reader_in_element (deserializer, "binding", 3))
		goto error;

	binding_name = xmlTextReaderGetAttribute (deserializer->reader,
	                                          (xmlChar *) "name");

	if (xmlTextReaderRead(deserializer->reader) <= 0)
		goto error;

	binding_langtag = xmlTextReaderGetAttribute (deserializer->reader,
	                                             (xmlChar *) "xml:lang");

	if (!parse_binding_type (deserializer, type, error))
		goto error_already_set;

	if (xmlTextReaderRead(deserializer->reader) <= 0)
		goto error;

	binding_value = xmlTextReaderValue (deserializer->reader);

	/* End of binding content */
	if (xmlTextReaderRead(deserializer->reader) <= 0 ||
	    xmlTextReaderNodeType (deserializer->reader) != XML_READER_TYPE_END_ELEMENT)
		goto error;

	/* End of binding */
	if (xmlTextReaderRead(deserializer->reader) <= 0 ||
	    xmlTextReaderNodeType (deserializer->reader) != XML_READER_TYPE_END_ELEMENT)
		goto error;

	*name = binding_name;
	*value = binding_value;
	*langtag = binding_langtag;

	return TRUE;
 error:
	g_set_error (error,
	             TRACKER_SPARQL_ERROR,
	             TRACKER_SPARQL_ERROR_PARSE,
	             "Wrong XML format, unexpected node '%s'",
	             xmlTextReaderConstName (deserializer->reader));
 error_already_set:
	g_clear_pointer (&binding_name, xmlFree);
	g_clear_pointer (&binding_value, xmlFree);
	g_clear_pointer (&binding_langtag, xmlFree);

	return FALSE;
}

static gboolean
parse_result (TrackerDeserializerXml  *deserializer,
              GError                 **error)
{
	TrackerSparqlCursor *cursor = TRACKER_SPARQL_CURSOR (deserializer);
	const gchar *var_name;
	GHashTable *ht = NULL;
	gint n_columns, i;

	if (!reader_in_element (deserializer, "result", 2))
		goto error;

	g_ptr_array_set_size (deserializer->columns, 0);
	ht = g_hash_table_new_full (g_str_hash, g_str_equal, xmlFree, column_free);

	while (xmlTextReaderRead (deserializer->reader) > 0) {
		ColumnData *col;
		xmlChar *name, *value, *langtag;
		TrackerSparqlValueType type;

		if (xmlTextReaderNodeType (deserializer->reader) == XML_READER_TYPE_END_ELEMENT)
			break;

		if (!parse_binding (deserializer, &type, &name, &value, &langtag, error))
			goto error_already_set;

		col = column_new (type, value, langtag);
		g_hash_table_insert (ht, name, col);
	}

	if (maybe_propagate_error (deserializer, error))
		goto error_already_set;

	n_columns = tracker_sparql_cursor_get_n_columns (cursor);

	for (i = 0; i < n_columns; i++) {
		ColumnData *col;

		var_name = tracker_sparql_cursor_get_variable_name (cursor, i);
		col = g_hash_table_lookup (ht, var_name);
		g_hash_table_steal (ht, var_name);
		if (!col)
			col = column_new (TRACKER_SPARQL_VALUE_TYPE_UNBOUND, NULL, NULL);

		g_ptr_array_add (deserializer->columns, col);
	}

	/* There should be no bindings left */
	if (g_hash_table_size (ht) > 0) {
		g_set_error (error,
		             TRACKER_SPARQL_ERROR,
		             TRACKER_SPARQL_ERROR_PARSE,
		             "Wrong XML format, unexpected additional bindings");
		return FALSE;
	}

	return TRUE;

 error:
	g_set_error (error,
	             TRACKER_SPARQL_ERROR,
	             TRACKER_SPARQL_ERROR_PARSE,
	             "Wrong XML format, unexpected node '%s'",
	             xmlTextReaderConstName (deserializer->reader));
 error_already_set:
	g_clear_pointer (&ht, g_hash_table_unref);
	return FALSE;
}

static gboolean
tracker_deserializer_xml_next (TrackerSparqlCursor  *cursor,
                               GCancellable         *cancellable,
                               GError              **error)
{
	TrackerDeserializerXml *deserializer =
		TRACKER_DESERIALIZER_XML (cursor);

	if (g_cancellable_set_error_if_cancelled (cancellable, error))
		return FALSE;

	g_ptr_array_set_size (deserializer->columns, 0);

 again:
	if (xmlTextReaderRead(deserializer->reader) <= 0) {
		if (!maybe_propagate_error (deserializer, error)) {
			g_set_error (error,
			             TRACKER_SPARQL_ERROR,
			             TRACKER_SPARQL_ERROR_PARSE,
			             "Unexpected termination of XML document");
		}
		return FALSE;
	}

	if (!deserializer->started) {
		if (reader_in_element (deserializer, "results", 1)) {
			deserializer->started = TRUE;
			/* We want to read the next element, the first <result> */
			goto again;
		} else if (reader_in_element (deserializer, "boolean", 1)) {
			ColumnData *col;
			xmlChar *content;

			content = xmlTextReaderValue (deserializer->reader);
			col = column_new (TRACKER_SPARQL_VALUE_TYPE_BOOLEAN, content, NULL);
			g_ptr_array_add (deserializer->columns, col);
		} else {
			g_set_error (error,
			             TRACKER_SPARQL_ERROR,
			             TRACKER_SPARQL_ERROR_PARSE,
			             "Wrong XML format, unexpected node '%s'",
			             xmlTextReaderConstName (deserializer->reader));
			return FALSE;
		}
	}

	/* We've reached the end of results */
	if (xmlTextReaderNodeType (deserializer->reader) == XML_READER_TYPE_END_ELEMENT)
		return FALSE;

	return parse_result (deserializer, error);
}

static void
tracker_deserializer_xml_next_async (TrackerSparqlCursor  *cursor,
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
tracker_deserializer_xml_next_finish (TrackerSparqlCursor  *cursor,
                                      GAsyncResult         *res,
                                      GError              **error)
{
	return g_task_propagate_boolean (G_TASK (res), error);
}

static void
tracker_deserializer_xml_close (TrackerSparqlCursor *cursor)
{
	TrackerDeserializerXml *deserializer =
		TRACKER_DESERIALIZER_XML (cursor);

	xmlTextReaderClose (deserializer->reader);

	TRACKER_SPARQL_CURSOR_CLASS (tracker_deserializer_xml_parent_class)->close (cursor);
}

gboolean
tracker_deserializer_xml_get_parser_location (TrackerDeserializer *deserializer,
                                              goffset             *line_no,
                                              goffset             *column_no)
{
	TrackerDeserializerXml *deserializer_xml =
		TRACKER_DESERIALIZER_XML (deserializer);

	*line_no = xmlTextReaderGetParserLineNumber (deserializer_xml->reader);
	*column_no = xmlTextReaderGetParserColumnNumber (deserializer_xml->reader);

	return TRUE;
}

static void
tracker_deserializer_xml_class_init (TrackerDeserializerXmlClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	TrackerSparqlCursorClass *cursor_class =
		TRACKER_SPARQL_CURSOR_CLASS (klass);
	TrackerDeserializerClass *deserializer_class =
		TRACKER_DESERIALIZER_CLASS (klass);

	object_class->finalize = tracker_deserializer_xml_finalize;
	object_class->constructed = tracker_deserializer_xml_constructed;

	cursor_class->get_n_columns = tracker_deserializer_xml_get_n_columns;
	cursor_class->get_value_type = tracker_deserializer_xml_get_value_type;
	cursor_class->get_variable_name = tracker_deserializer_xml_get_variable_name;
	cursor_class->get_string = tracker_deserializer_xml_get_string;
	cursor_class->next = tracker_deserializer_xml_next;
	cursor_class->next_async = tracker_deserializer_xml_next_async;
	cursor_class->next_finish = tracker_deserializer_xml_next_finish;
	cursor_class->close = tracker_deserializer_xml_close;

	deserializer_class->get_parser_location =
		tracker_deserializer_xml_get_parser_location;
}

static void
tracker_deserializer_xml_init (TrackerDeserializerXml *deserializer)
{
	deserializer->columns = g_ptr_array_new_with_free_func (column_free);
	deserializer->column_names = g_ptr_array_new_with_free_func (xmlFree);
}

TrackerSparqlCursor *
tracker_deserializer_xml_new (GInputStream            *stream,
                              TrackerNamespaceManager *namespaces)
{
	return g_object_new (TRACKER_TYPE_DESERIALIZER_XML,
	                     "stream", stream,
	                     "namespace-manager", namespaces,
	                     NULL);
}
