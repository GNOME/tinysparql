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

#define MAX_SIZE 800 * 1024

int
LLVMFuzzerTestOneInput (const unsigned char *data, size_t size)
{
	static TrackerSparqlConnection *conn = NULL;
	TrackerBatch *batch;
	GInputStream *istream;

	sqlite3_config (SQLITE_CONFIG_LOOKASIDE, 0, 0);

	fuzz_set_logging_func ();

	if (size > MAX_SIZE)
		return 0;

	istream = g_memory_input_stream_new_from_data (data, size, NULL);

	if (!conn) {
		GFile *ontology;

		/* Point to empty ontology */
		ontology = g_file_new_for_uri ("resource:///");
		conn = tracker_sparql_connection_new (TRACKER_SPARQL_CONNECTION_FLAGS_NONE,
		                                      NULL,
		                                      ontology,
		                                      NULL, NULL);
		g_clear_object (&ontology);
	}

	batch = tracker_sparql_connection_create_batch (conn);

	tracker_batch_add_rdf (batch,
	                       TRACKER_DESERIALIZE_FLAGS_NONE,
	                       TRACKER_RDF_FORMAT_JSON_LD,
	                       NULL,
	                       istream);

	if (tracker_batch_execute (batch, NULL, NULL))
		g_clear_object (&conn);

	g_clear_object (&batch);
	g_clear_object (&istream);

	return 0;
}
