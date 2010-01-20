/*
 * Copyright (C) 2008, Nokia
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <glib.h>
#include <glib/gstdio.h>

#include <libtracker-extract/tracker-extract.h>

/*
 * Prototype of the parsing function.
 */
static void extract_function (const gchar          *uri,
                              TrackerSparqlBuilder *metadata);

/*
 * Link between mimetype and parsing function
 */
static TrackerExtractData data[] = {
	{ "mimetype/x-dummy", extract_dummy },
	{ NULL, NULL }
};

/*
 * Implementation of the parsing function
 */
static void
extract_function (const gchar          *uri,
                  TrackerSparqlBuilder *metadata)
{
	gchar *name;

	/*
	 * Open the file and do whatever you need to do with it.
	 *
	 * The extracted properties must be added to the metadata
	 * hash table.
	 */

	/* Example 1. Insert a picture with the creator */
	name = g_strdup ("Martyn Russell");
	
	tracker_sparql_builder_subject_iri (metadata, uri);
	tracker_sparql_builder_predicate (metadata, "a");
	tracker_sparql_builder_object (metadata, "nfo:Image");
	tracker_sparql_builder_object (metadata, "nmm:Photo");

	tracker_sparql_builder_predicate (metadata, "nco:creator");

	tracker_sparql_builder_object_blank_open (metadata);
	tracker_sparql_builder_predicate (metadata, "a");
	tracker_sparql_builder_object (metadata, "nco:Contact");

	tracker_sparql_builder_predicate (metadata, "nco:fullname");
	tracker_sparql_builder_object_unvalidated (metadata, name);
	tracker_sparql_builder_object_blank_close (metadata);
	g_free (name);
}

/*
 * Dont touch this function! Keep it in your module with this exact name.
 * It is the "public" function used to load the module.
 */
TrackerExtractData *
tracker_extract_get_data (void)
{
	return data;
}
