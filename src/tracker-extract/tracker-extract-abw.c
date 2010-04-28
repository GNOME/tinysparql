/*
 * Copyright (C) 2007, Jamie McCracken <jamiemcc@gnome.org>
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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

#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <libtracker-common/tracker-file-utils.h>

#include <libtracker-extract/tracker-extract.h>

static void extract_abw (const gchar          *uri,
                         TrackerSparqlBuilder *preupdate,
                         TrackerSparqlBuilder *metadata);

static TrackerExtractData data[] = {
	{ "application/x-abiword", extract_abw },
	{ NULL, NULL }
};


static void
extract_abw (const gchar          *uri,
             TrackerSparqlBuilder *preupdate,
             TrackerSparqlBuilder *metadata)
{
	FILE *f;
	gchar *filename;

	filename = g_filename_from_uri (uri, NULL, NULL);
	f = tracker_file_open (filename, "r", TRUE);
	g_free (filename);

	if (f) {
		gchar  *line;
		gsize  length;
		gssize read_char;

		line = NULL;
		length = 0;

		tracker_sparql_builder_predicate (metadata, "a");
		tracker_sparql_builder_object (metadata, "nfo:Document");

		while ((read_char = tracker_getline (&line, &length, f)) != -1) {
			if (g_str_has_suffix (line, "</m>\n")) {
				line[read_char - 5] = '\0';
			}

			if (g_str_has_prefix (line, "<m key=\"dc.title\">")) {
				tracker_sparql_builder_predicate (metadata, "nie:title");
				tracker_sparql_builder_object_unvalidated (metadata, line + 18);
			} else if (g_str_has_prefix (line, "<m key=\"dc.subject\">")) {
				tracker_sparql_builder_predicate (metadata, "nie:subject");
				tracker_sparql_builder_object_unvalidated (metadata, line + 20);
			} else if (g_str_has_prefix (line, "<m key=\"dc.creator\">")) {
				tracker_sparql_builder_predicate (metadata, "nco:creator");
				tracker_sparql_builder_object_unvalidated (metadata, line + 20);
			} else if (g_str_has_prefix (line, "<m key=\"abiword.keywords\">")) {
				gchar *keywords = g_strdup (line + 26);
				char *lasts, *keyw;

				for (keyw = strtok_r (keywords, ",; ", &lasts); keyw;
				     keyw = strtok_r (NULL, ",; ", &lasts)) {
					tracker_sparql_builder_predicate (metadata, "nie:keyword");
					tracker_sparql_builder_object_unvalidated (metadata, keyw);
				}

				g_free (keywords);
			} else if (g_str_has_prefix (line, "<m key=\"dc.description\">")) {
				tracker_sparql_builder_predicate (metadata, "nie:comment");
				tracker_sparql_builder_object_unvalidated (metadata, line + 24);
			}

			g_free (line);
			line = NULL;
			length = 0;
		}

		if (line) {
			g_free (line);
		}

		tracker_file_close (f, FALSE);
	}
}

TrackerExtractData *
tracker_extract_get_data (void)
{
	return data;
}
