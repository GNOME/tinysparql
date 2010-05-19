/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
 * Copyright (C) 2007, Jason Kivlighn <jkivlighn@gmail.com>
 * Copyright (C) 2007, Creative Commons <http://creativecommons.org>
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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

#include "config.h"

#include <string.h>
#include <stdlib.h>

#include <libtracker-common/tracker-common.h>

#include <libtracker-db/tracker-db-dbus.h>
#include <libtracker-db/tracker-db-interface-sqlite.h>
#include <libtracker-db/tracker-db-manager.h>

#include "tracker-class.h"
#include "tracker-data-manager.h"
#include "tracker-data-query.h"
#include "tracker-ontologies.h"
#include "tracker-sparql-query.h"

GPtrArray*
tracker_data_query_rdf_type (gint id)
{
	TrackerDBCursor *cursor = NULL;
	TrackerDBInterface *iface;
	TrackerDBStatement *stmt;
	GPtrArray *ret = NULL;
	GError *error = NULL;

	iface = tracker_db_manager_get_db_interface ();

	stmt = tracker_db_interface_create_statement (iface, &error,
	                                              "SELECT (SELECT Uri FROM Resource WHERE ID = \"rdf:type\") "
	                                              "FROM \"rdfs:Resource_rdf:type\" "
	                                              "WHERE ID = ?");

	if (stmt) {
		tracker_db_statement_bind_int (stmt, 0, id);
		cursor = tracker_db_statement_start_cursor (stmt, &error);
		g_object_unref (stmt);
	}

	if (cursor) {

		/* Query is usually a rather small result, but let's try to
		 * avoid reallocs in gptrarray.c as much as possible (this
		 * function is called fairly often) */

		ret = g_ptr_array_sized_new (20);
		while (tracker_db_cursor_iter_next (cursor, &error)) {
			const gchar *class_uri;
			TrackerClass *cl;

			class_uri = tracker_db_cursor_get_string (cursor, 0);
			cl = tracker_ontologies_get_class_by_uri (class_uri);
			if (!cl) {
				g_critical ("Unknown class %s", class_uri);
				continue;
			}
			g_ptr_array_add (ret, cl);
		}
		g_object_unref (cursor);
	}

	if (G_UNLIKELY (error)) {
		g_critical ("Could not query RDF type: %s\n", error->message);
		g_error_free (error);

		if (ret) {
			g_ptr_array_free (ret, FALSE);
			ret = NULL;
		}
	}

	return ret;
}

gint
tracker_data_query_resource_id (const gchar *uri)
{
	TrackerDBCursor *cursor = NULL;
	TrackerDBInterface *iface;
	TrackerDBStatement *stmt;
	GError *error = NULL;
	gint id = 0;

	g_return_val_if_fail (uri != NULL, 0);

	iface = tracker_db_manager_get_db_interface ();

	stmt = tracker_db_interface_create_statement (iface, &error,
	                                              "SELECT ID FROM Resource WHERE Uri = ?");

	if (stmt) {
		tracker_db_statement_bind_text (stmt, 0, uri);
		cursor = tracker_db_statement_start_cursor (stmt, &error);
		g_object_unref (stmt);
	}

	if (cursor) {
		if (tracker_db_cursor_iter_next (cursor, &error)) {
			id = tracker_db_cursor_get_int (cursor, 0);
		}

		g_object_unref (cursor);
	}

	if (G_UNLIKELY (error)) {
		g_critical ("Could not query resource ID: %s\n", error->message);
		g_error_free (error);
	}

	return id;
}

TrackerDBResultSet *
tracker_data_query_sparql (const gchar  *query,
                           GError      **error)
{
	TrackerSparqlQuery *sparql_query;
	TrackerDBResultSet *result_set;

	g_return_val_if_fail (query != NULL, NULL);

	sparql_query = tracker_sparql_query_new (query);

	result_set = tracker_sparql_query_execute (sparql_query, error);

	g_object_unref (sparql_query);

	return result_set;
}


TrackerDBCursor *
tracker_data_query_sparql_cursor (const gchar  *query,
                                  GError      **error)
{
	TrackerSparqlQuery *sparql_query;
	TrackerDBCursor *cursor;

	g_return_val_if_fail (query != NULL, NULL);

	sparql_query = tracker_sparql_query_new (query);

	cursor = tracker_sparql_query_execute_cursor (sparql_query, error);

	g_object_unref (sparql_query);

	return cursor;
}

