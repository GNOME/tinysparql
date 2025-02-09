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

#include "tracker-error.h"

#include "core/tracker-data.h"

static const GDBusErrorEntry tracker_sparql_error_entries[] =
{
	{TRACKER_SPARQL_ERROR_CONSTRAINT, "org.freedesktop.Tracker.Error.Constraint"},
	{TRACKER_SPARQL_ERROR_INTERNAL, "org.freedesktop.Tracker.Error.Internal"},
	{TRACKER_SPARQL_ERROR_NO_SPACE, "org.freedesktop.Tracker.Error.NoSpace"},
	{TRACKER_SPARQL_ERROR_ONTOLOGY_NOT_FOUND, "org.freedesktop.Tracker.Error.OntologyNotFound"},
	{TRACKER_SPARQL_ERROR_OPEN_ERROR, "org.freedesktop.Tracker.Error.OpenError"},
	{TRACKER_SPARQL_ERROR_PARSE, "org.freedesktop.Tracker.Error.Parse"},
	{TRACKER_SPARQL_ERROR_QUERY_FAILED, "org.freedesktop.Tracker.Error.QueryFailed"},
	{TRACKER_SPARQL_ERROR_TYPE, "org.freedesktop.Tracker.Error.Type"},
	{TRACKER_SPARQL_ERROR_UNKNOWN_CLASS, "org.freedesktop.Tracker.Error.UnknownClass"},
	{TRACKER_SPARQL_ERROR_UNKNOWN_GRAPH, "org.freedesktop.Tracker.Error.UnknownGraph"},
	{TRACKER_SPARQL_ERROR_UNKNOWN_PROPERTY, "org.freedesktop.Tracker.Error.UnknownProperty"},
	{TRACKER_SPARQL_ERROR_UNSUPPORTED, "org.freedesktop.Tracker.Error.Unsupported"},
	{TRACKER_SPARQL_ERROR_MISSING_LAST_MODIFIED_HEADER, "org.freedesktop.Tracker.Error.MissingLastModifiedHeader"},
	{TRACKER_SPARQL_ERROR_INCOMPLETE_PROPERTY_DEFINITION, "org.freedesktop.Tracker.Error.IncompleteProperty"},
	{TRACKER_SPARQL_ERROR_CORRUPT, "org.freedesktop.Tracker.Error.Corrupt"},
};

typedef struct
{
	int obtained_error_code;
	int mapped_error_code;
} TrackerErrorMap;

static const TrackerErrorMap ontology_error_map[] = {
	{ TRACKER_DATA_UNSUPPORTED_ONTOLOGY_CHANGE, TRACKER_SPARQL_ERROR_UNSUPPORTED },
	{ TRACKER_DATA_ONTOLOGY_NOT_FOUND, TRACKER_SPARQL_ERROR_ONTOLOGY_NOT_FOUND },
	{ TRACKER_DATA_UNSUPPORTED_LOCATION, TRACKER_SPARQL_ERROR_UNSUPPORTED },
};

static const TrackerErrorMap db_interface_error_map[] = {
	{ TRACKER_DB_QUERY_ERROR, TRACKER_SPARQL_ERROR_QUERY_FAILED },
	{ TRACKER_DB_INTERRUPTED, TRACKER_SPARQL_ERROR_INTERNAL },
	{ TRACKER_DB_OPEN_ERROR, TRACKER_SPARQL_ERROR_OPEN_ERROR },
	{ TRACKER_DB_NO_SPACE, TRACKER_SPARQL_ERROR_NO_SPACE },
	{ TRACKER_DB_CONSTRAINT, TRACKER_SPARQL_ERROR_CONSTRAINT },
	{ TRACKER_DB_CORRUPT, TRACKER_SPARQL_ERROR_CORRUPT },
};

G_STATIC_ASSERT (G_N_ELEMENTS (tracker_sparql_error_entries) == TRACKER_SPARQL_N_ERRORS);

GQuark
tracker_sparql_error_quark (void)
{
	static gsize quark = 0;

	g_dbus_error_register_error_domain ("tracker-sparql-error-quark",
	                                    &quark,
	                                    tracker_sparql_error_entries,
	                                    G_N_ELEMENTS (tracker_sparql_error_entries));
	return (GQuark) quark;
}

/* Converts internal error codes into public
 * TrackerSparqlError codes. */
GError *
_translate_internal_error (GError *error)
{
	const TrackerErrorMap *map = NULL;
	GError *new_error = NULL;
	int i, n_elems;

	if (error->domain == TRACKER_DATA_ONTOLOGY_ERROR) {
		map = ontology_error_map;
		n_elems = G_N_ELEMENTS (ontology_error_map);
	} else if (error->domain == TRACKER_DB_INTERFACE_ERROR) {
		map = db_interface_error_map;
		n_elems = G_N_ELEMENTS (db_interface_error_map);
	}

	if (map) {
		for (i = 0; i < n_elems; i++) {
			if (map[i].obtained_error_code == error->code) {
				new_error = g_error_new_literal (TRACKER_SPARQL_ERROR,
				                                 map[i].mapped_error_code,
				                                 error->message);
				break;
			}
		}
	}

	if (new_error) {
		g_error_free (error);
		return new_error;
	} else {
		return error;
	}
}
