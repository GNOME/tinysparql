/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2007, Jason Kivlighn (jkivlighn@gmail.com)
 * Copyright (C) 2007, Creative Commons (http://creativecommons.org)
 * Copyright (C) 2008, Nokia
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

#include <libtracker-common/tracker-file-utils.h>
#include <libtracker-common/tracker-parser.h>
#include <libtracker-common/tracker-type-utils.h>
#include <libtracker-common/tracker-utils.h>

#include <libtracker-db/tracker-db-dbus.h>
#include <libtracker-db/tracker-db-interface-sqlite.h>
#include <libtracker-db/tracker-db-manager.h>

#include "tracker-data-manager.h"
#include "tracker-data-query.h"
#include "tracker-sparql-query.h"

GPtrArray*
tracker_data_query_rdf_type (guint32 id)
{
	TrackerDBResultSet *result_set;
	TrackerDBInterface *iface;
	TrackerDBStatement *stmt;
	GPtrArray *ret = NULL;

	iface = tracker_db_manager_get_db_interface ();

	stmt = tracker_db_interface_create_statement (iface,
			"SELECT \"rdfs:Resource\".\"Uri\" "
			"FROM \"rdfs:Resource_rdf:type\" "
			"INNER JOIN \"rdfs:Resource\" "
			"ON \"rdfs:Resource_rdf:type\".\"rdf:type\" = \"rdfs:Resource\".\"ID\" "
			"WHERE \"rdfs:Resource_rdf:type\".\"ID\" = ?");

	tracker_db_statement_bind_int (stmt, 0, id);
	result_set = tracker_db_statement_execute (stmt, NULL);
	g_object_unref (stmt);

	if (result_set) {
		guint rows;

		rows = tracker_db_result_set_get_n_rows (result_set);
		ret = g_ptr_array_sized_new (rows);
		do {
			gchar *uri;

			tracker_db_result_set_get (result_set, 0, &uri, -1);

			g_ptr_array_add (ret, uri);

		} while (tracker_db_result_set_iter_next (result_set));

		g_object_unref (result_set);
	}

	return ret;
}

guint32
tracker_data_query_resource_id (const gchar	   *uri)
{
	TrackerDBResultSet *result_set;
	TrackerDBInterface *iface;
	TrackerDBStatement *stmt;
	guint32		    id = 0;

	g_return_val_if_fail (uri != NULL, 0);

	iface = tracker_db_manager_get_db_interface ();

	stmt = tracker_db_interface_create_statement (iface,
		"SELECT ID FROM \"rdfs:Resource\" WHERE Uri = ?");
	tracker_db_statement_bind_text (stmt, 0, uri);
	result_set = tracker_db_statement_execute (stmt, NULL);
	g_object_unref (stmt);

	if (result_set) {
		tracker_db_result_set_get (result_set, 0, &id, -1);
		g_object_unref (result_set);
	}

	return id;
}

gboolean
tracker_data_query_resource_exists (const gchar	  *uri,
				   guint32	  *resource_id)
{
	TrackerDBInterface *iface;
	TrackerDBStatement *stmt;
	TrackerDBResultSet *result_set;
	guint db_id;
	gboolean found = FALSE;

	db_id = 0;

	iface = tracker_db_manager_get_db_interface ();

	stmt = tracker_db_interface_create_statement (iface,
		"SELECT ID FROM \"rdfs:Resource\" WHERE Uri = ?");
	tracker_db_statement_bind_text (stmt, 0, uri);
	result_set = tracker_db_statement_execute (stmt, NULL);
	g_object_unref (stmt);

	if (result_set) {
		tracker_db_result_set_get (result_set,
					   0, &db_id,
					   -1);
		g_object_unref (result_set);
		found = TRUE;
	}

	if (resource_id) {
		*resource_id = (guint32) db_id;
	}

	return found;
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

