/*
 * Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
 * Copyright (C) 2020, Sam Thursfield <sam@afuera.me.uk>
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

#include "tracker-version.h"

G_BEGIN_DECLS

/**
 * TrackerSparqlError:
 * @TRACKER_SPARQL_ERROR_CONSTRAINT: Subject is not in the domain of a property or
 *                             trying to set multiple values for a single valued
 *                             property.
 * @TRACKER_SPARQL_ERROR_INTERNAL: Internal error.
 * @TRACKER_SPARQL_ERROR_NO_SPACE: There was no disk space available to perform the request.
 * @TRACKER_SPARQL_ERROR_ONTOLOGY_NOT_FOUND: The specified ontology wasn't found.
 * @TRACKER_SPARQL_ERROR_OPEN_ERROR: Problem encountered while opening the database.
 * @TRACKER_SPARQL_ERROR_PARSE: Error parsing the SPARQL string.
 * @TRACKER_SPARQL_ERROR_QUERY_FAILED: Problem while executing the query.
 * @TRACKER_SPARQL_ERROR_TYPE: Type constraint failed when trying to insert data.
 * @TRACKER_SPARQL_ERROR_UNKNOWN_CLASS: Unknown class.
 * @TRACKER_SPARQL_ERROR_UNKNOWN_GRAPH: Unknown graph.
 * @TRACKER_SPARQL_ERROR_UNKNOWN_PROPERTY: Unknown property.
 * @TRACKER_SPARQL_ERROR_UNSUPPORTED: Unsupported feature or method.
 * @TRACKER_SPARQL_ERROR_MISSING_LAST_MODIFIED_HEADER: The ontology doesn't contain nrl:lastModified header
 * @TRACKER_SPARQL_ERROR_INCOMPLETE_PROPERTY_DEFINITION: The property is not completely defined.
 * @TRACKER_SPARQL_ERROR_CORRUPT: A soft/hard corruption was found in the database during operation.
 *   If this error is obtained during regular operations with an existing [class@SparqlConnection],
 *   the corruption was newly found. This event will be persistently recorded so that the
 *   [func@SparqlConnection.new_async] constructor (or its synchronous variant) will
 *   perform database repair attempts. If this error is obtained during one of those constructors, the
 *   database could not be repaired automatically and data loss is unavoidable. It is left to the discretion
 *   of the API user to set up the appropriate fallbacks in this situation, to replace the
 *   database and recover from the error. See [ctor@SparqlConnection.new] documentation
 *   for more information on corruption handling.
 * @TRACKER_SPARQL_ERROR_LAST: The total number of error codes.
 *
 * Error domain for Tracker Sparql. Errors in this domain will be from the
 * [error@SparqlError] enumeration. See [struct@GLib.Error] for more information on error
 * domains.
 */
typedef enum {
	TRACKER_SPARQL_ERROR_CONSTRAINT,
	TRACKER_SPARQL_ERROR_INTERNAL,
	TRACKER_SPARQL_ERROR_NO_SPACE,
	TRACKER_SPARQL_ERROR_ONTOLOGY_NOT_FOUND,
	TRACKER_SPARQL_ERROR_OPEN_ERROR,
	TRACKER_SPARQL_ERROR_PARSE,
	TRACKER_SPARQL_ERROR_QUERY_FAILED,
	TRACKER_SPARQL_ERROR_TYPE,
	TRACKER_SPARQL_ERROR_UNKNOWN_CLASS,
	TRACKER_SPARQL_ERROR_UNKNOWN_GRAPH,
	TRACKER_SPARQL_ERROR_UNKNOWN_PROPERTY,
	TRACKER_SPARQL_ERROR_UNSUPPORTED,
	TRACKER_SPARQL_ERROR_MISSING_LAST_MODIFIED_HEADER,
	TRACKER_SPARQL_ERROR_INCOMPLETE_PROPERTY_DEFINITION,
	TRACKER_SPARQL_ERROR_CORRUPT,
	TRACKER_SPARQL_ERROR_LAST
} TrackerSparqlError;

#define TRACKER_SPARQL_N_ERRORS TRACKER_SPARQL_ERROR_LAST
#define TRACKER_SPARQL_ERROR (tracker_sparql_error_quark ())

TRACKER_AVAILABLE_IN_ALL
GQuark tracker_sparql_error_quark (void);

G_END_DECLS
