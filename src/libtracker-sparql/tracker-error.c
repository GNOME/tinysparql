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

#include <libtracker-sparql/core/tracker-data.h>

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
};

G_STATIC_ASSERT (G_N_ELEMENTS (tracker_sparql_error_entries) == TRACKER_SPARQL_N_ERRORS);

/**
 * tracker_sparql_error_quark:
 *
 * Returns: The error domain quark used for Tracker errors.
 **/
GQuark
tracker_sparql_error_quark (void)
{
       static volatile gsize quark_volatile = 0;
       g_dbus_error_register_error_domain ("tracker-sparql-error-quark",
                                           &quark_volatile,
                                           tracker_sparql_error_entries,
                                           G_N_ELEMENTS (tracker_sparql_error_entries));
       return (GQuark) quark_volatile;
}

/* Converts internal error codes into public
 * TrackerSparqlError codes. */
GError *
_translate_internal_error (GError *error)
{
	GError *new_error = NULL;

	if (error->domain == TRACKER_DATA_ONTOLOGY_ERROR) {
		switch (error->code) {
			case TRACKER_DATA_ONTOLOGY_NOT_FOUND:
				new_error = g_error_new_literal (TRACKER_SPARQL_ERROR,
				                                 TRACKER_SPARQL_ERROR_ONTOLOGY_NOT_FOUND,
				                                 error->message);
				break;
			case TRACKER_DATA_UNSUPPORTED_LOCATION:
			case TRACKER_DATA_UNSUPPORTED_ONTOLOGY_CHANGE:
				new_error = g_error_new_literal (TRACKER_SPARQL_ERROR,
				                                 TRACKER_SPARQL_ERROR_UNSUPPORTED,
				                                 error->message);
				break;
			default:
				new_error = g_error_new_literal (TRACKER_SPARQL_ERROR,
				                                 TRACKER_SPARQL_ERROR_INTERNAL,
				                                 error->message);
		}
	} else if (error->domain == TRACKER_DB_INTERFACE_ERROR) {
		TrackerSparqlError new_code = TRACKER_SPARQL_ERROR_INTERNAL;

		switch (error->code) {
			case TRACKER_DB_QUERY_ERROR: new_code = TRACKER_SPARQL_ERROR_QUERY_FAILED; break;
			case TRACKER_DB_OPEN_ERROR: new_code = TRACKER_SPARQL_ERROR_OPEN_ERROR; break;
			case TRACKER_DB_NO_SPACE: new_code = TRACKER_SPARQL_ERROR_NO_SPACE; break;
			/* This should never happen as we don't call sqlite3_interrupt()
			 * anywhere, so it doesn't get its own public error code. */
			case TRACKER_DB_INTERRUPTED: new_code = TRACKER_SPARQL_ERROR_INTERNAL; break;
			case TRACKER_DB_CONSTRAINT: new_code = TRACKER_SPARQL_ERROR_CONSTRAINT; break;
			default: g_warn_if_reached ();
		}

		new_error = g_error_new_literal (TRACKER_SPARQL_ERROR, new_code, error->message);
	}

	if (new_error) {
		g_error_free (error);
		return new_error;
	} else {
		return error;
	}
}
