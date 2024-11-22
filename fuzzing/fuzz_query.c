/*
 * Copyright 2024 Red Hat Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#include "fuzz.h"

int
LLVMFuzzerTestOneInput (const unsigned char *data, size_t size)
{
	TrackerSparqlConnection *conn;
	TrackerSparqlStatement *stmt;
	unsigned char *nul_terminated_data = NULL;
	GFile *nepomuk;

	fuzz_set_logging_func ();

	nepomuk = tracker_sparql_get_ontology_nepomuk ();
	conn = tracker_sparql_connection_new (TRACKER_SPARQL_CONNECTION_FLAGS_NONE,
	                                      NULL,
	                                      nepomuk,
	                                      NULL, NULL);

	nul_terminated_data = (unsigned char *) g_strndup ((const gchar *) data, size);
	stmt = tracker_sparql_connection_query_statement (conn,
	                                                  (const gchar *) nul_terminated_data,
	                                                  NULL, NULL);

	g_clear_object (&stmt);

	stmt = tracker_sparql_connection_update_statement (conn,
	                                                   (const gchar *) nul_terminated_data,
	                                                   NULL, NULL);
	g_clear_object (&stmt);

	g_clear_object (&nepomuk);
	g_clear_object (&conn);
	g_free (nul_terminated_data);

	return 0;
}
