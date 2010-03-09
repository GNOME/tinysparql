/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <glib.h>

#include <libtracker-common/tracker-ontology.h>
#include <libtracker-common/tracker-os-dependant.h>

#include <libtracker-extract/tracker-extract.h>

static void extract_imagemagick (const gchar          *uri,
                                 TrackerSparqlBuilder *preupdate,
                                 TrackerSparqlBuilder *metadata);

static TrackerExtractData data[] = {
	{ "image/*", extract_imagemagick },
	{ NULL, NULL }
};

static void
extract_imagemagick (const gchar          *uri,
                     TrackerSparqlBuilder *preupdate,
                     TrackerSparqlBuilder *metadata)
{
	gchar *argv[6];
	gchar *identify;
	gchar **lines;
	gint  exit_status;
	gchar *filename;

	filename = g_filename_from_uri (uri, NULL, NULL);

	g_return_if_fail (filename != NULL);

	/* Imagemagick crashes trying to extract from xcf files */
	if (g_str_has_suffix (filename, ".xcf")) {
		g_free (filename);
		return;
	}

	argv[0] = g_strdup ("identify");
	argv[1] = g_strdup ("-format");
	argv[2] = g_strdup ("%w;\\n%h;\\n%c;\\n");

	if (g_str_has_suffix (filename, ".xcf")) {
		argv[3] = g_strdup (filename);
		argv[4] = NULL;
	} else {
		argv[3] = g_strdup ("-ping");
		argv[4] = g_strdup (filename);
	}

	argv[5] = NULL;

	tracker_sparql_builder_predicate (metadata, "a");
	tracker_sparql_builder_predicate (metadata, "nfo:Image");

	if (tracker_spawn (argv, 10, &identify, &exit_status) &&
	    exit_status == EXIT_SUCCESS) {
		lines = g_strsplit (identify, ";\n", 4);

		tracker_sparql_builder_predicate (metadata, "nfo:width");
		tracker_sparql_builder_object_unvalidated (metadata, lines[0]);

		tracker_sparql_builder_predicate (metadata, "nfo:height");
		tracker_sparql_builder_object_unvalidated (metadata, lines[1]);

		tracker_sparql_builder_predicate (metadata, "nie:comment");
		tracker_sparql_builder_object_unvalidated (metadata, lines[2]);

		g_strfreev (lines);
	}

#ifdef HAVE_EXEMPI
	/* FIXME: Convert is buggy atm so disable temporarily */
	g_free (filename); return;

	gchar *xmp;

	argv[0] = g_strdup ("convert");
	argv[1] = g_strdup (filename);
	argv[2] = g_strdup ("xmp:-");
	argv[3] = NULL;

	if (tracker_spawn (argv, 10, &xmp, &exit_status)) {
		if (exit_status == EXIT_SUCCESS && xmp) {
			TrackerXmpData xmp_data = { 0 };

			tracker_read_xmp (xmp, strlen (xmp), uri, &xmp_data);

			tracker_apply_xmp (metadata, uri, &xmp_data);
		}
	}
#endif /* HAVE_EXEMPI */

	g_free (filename);

}

TrackerExtractData *
tracker_extract_get_data (void)
{
	return data;
}
