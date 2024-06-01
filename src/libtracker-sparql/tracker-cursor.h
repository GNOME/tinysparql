/*
 * Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
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
 */

#pragma once

#if !defined (__LIBTRACKER_SPARQL_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "only <tinysparql.h> must be included directly."
#endif

#include <gio/gio.h>
#include <tracker-version.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_SPARQL_CURSOR tracker_sparql_cursor_get_type ()
#define TRACKER_SPARQL_TYPE_CURSOR TRACKER_TYPE_SPARQL_CURSOR
TRACKER_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (TrackerSparqlCursor, tracker_sparql_cursor,
                          TRACKER, SPARQL_CURSOR, GObject)

#include "tracker-connection.h"

/**
 * TrackerSparqlValueType:
 * @TRACKER_SPARQL_VALUE_TYPE_UNBOUND: Unbound value type
 * @TRACKER_SPARQL_VALUE_TYPE_URI: Uri value type, rdfs:Resource
 * @TRACKER_SPARQL_VALUE_TYPE_STRING: String value type, xsd:string or rdf:langString
 * @TRACKER_SPARQL_VALUE_TYPE_INTEGER: Integer value type, xsd:integer
 * @TRACKER_SPARQL_VALUE_TYPE_DOUBLE: Double value type, xsd:double
 * @TRACKER_SPARQL_VALUE_TYPE_DATETIME: Datetime value type, xsd:dateTime
 * @TRACKER_SPARQL_VALUE_TYPE_BLANK_NODE: Blank node value type
 * @TRACKER_SPARQL_VALUE_TYPE_BOOLEAN: Boolean value type, xsd:boolean
 *
 * Enumeration with the possible types of the cursor's cells
 */
typedef enum {
	TRACKER_SPARQL_VALUE_TYPE_UNBOUND,
	TRACKER_SPARQL_VALUE_TYPE_URI,
	TRACKER_SPARQL_VALUE_TYPE_STRING,
	TRACKER_SPARQL_VALUE_TYPE_INTEGER,
	TRACKER_SPARQL_VALUE_TYPE_DOUBLE,
	TRACKER_SPARQL_VALUE_TYPE_DATETIME,
	TRACKER_SPARQL_VALUE_TYPE_BLANK_NODE,
	TRACKER_SPARQL_VALUE_TYPE_BOOLEAN,
} TrackerSparqlValueType;

TRACKER_AVAILABLE_IN_ALL
TrackerSparqlConnection * tracker_sparql_cursor_get_connection (TrackerSparqlCursor *cursor);
TRACKER_AVAILABLE_IN_ALL
gint tracker_sparql_cursor_get_n_columns (TrackerSparqlCursor *cursor);

TRACKER_AVAILABLE_IN_ALL
const gchar * tracker_sparql_cursor_get_string (TrackerSparqlCursor *cursor,
                                                gint                 column,
                                                glong               *length);
TRACKER_AVAILABLE_IN_3_7
const gchar * tracker_sparql_cursor_get_langstring (TrackerSparqlCursor  *cursor,
                                                    gint                  column,
                                                    const gchar         **langtag,
                                                    glong                *length);
TRACKER_AVAILABLE_IN_ALL
gboolean tracker_sparql_cursor_get_boolean (TrackerSparqlCursor *cursor,
                                            gint                 column);
TRACKER_AVAILABLE_IN_ALL
gdouble tracker_sparql_cursor_get_double (TrackerSparqlCursor *cursor,
                                          gint                 column);
TRACKER_AVAILABLE_IN_ALL
gint64 tracker_sparql_cursor_get_integer (TrackerSparqlCursor *cursor,
                                          gint                 column);
TRACKER_AVAILABLE_IN_ALL
TrackerSparqlValueType tracker_sparql_cursor_get_value_type (TrackerSparqlCursor *cursor,
                                                             gint                 column);
TRACKER_AVAILABLE_IN_ALL
const gchar * tracker_sparql_cursor_get_variable_name (TrackerSparqlCursor *cursor,
                                                       gint                 column);
TRACKER_AVAILABLE_IN_3_2
GDateTime * tracker_sparql_cursor_get_datetime (TrackerSparqlCursor *cursor,
                                                gint                 column);
TRACKER_AVAILABLE_IN_ALL
void tracker_sparql_cursor_close (TrackerSparqlCursor *cursor);

TRACKER_AVAILABLE_IN_ALL
gboolean tracker_sparql_cursor_is_bound (TrackerSparqlCursor *cursor,
                                         gint                 column);

TRACKER_AVAILABLE_IN_ALL
gboolean tracker_sparql_cursor_next (TrackerSparqlCursor  *cursor,
                                     GCancellable         *cancellable,
                                     GError              **error);

TRACKER_AVAILABLE_IN_ALL
void tracker_sparql_cursor_next_async (TrackerSparqlCursor  *cursor,
                                       GCancellable         *cancellable,
                                       GAsyncReadyCallback   callback,
                                       gpointer              user_data);

TRACKER_AVAILABLE_IN_ALL
gboolean tracker_sparql_cursor_next_finish (TrackerSparqlCursor  *cursor,
                                            GAsyncResult         *res,
                                            GError              **error);

TRACKER_DEPRECATED_IN_3_5
void tracker_sparql_cursor_rewind (TrackerSparqlCursor *cursor);

G_END_DECLS
