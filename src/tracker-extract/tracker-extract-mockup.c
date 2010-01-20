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

#include <libtracker-extract/tracker-extract.h>

static void extract_mockup (const gchar          *uri,
                            TrackerSparqlBuilder *metadata);

typedef struct {
	gchar *key;
	gchar *value;
} MockupTag;

static MockupTag tags[] = {
	{ "Audio:Album", "Album" },
	{ "Audio:Artist", "Artist" },
	{ "Audio:Title", "Title" },
	{ "Video:Title", "Title for Video" },
	{ "Audio:Genre", "Genre" },
	{ "Image:Location", "Here" },
	{ "Image:Software", "Softa" },
	{ "Image:Height", "480" },
	{ "Image:ExposureTime", "0.223" },
	{ NULL, NULL }
};

static TrackerExtractData data[] = {
	{ "audio/*", extract_mockup },
	{ "video/*", extract_mockup },
	{ "image/*", extract_mockup },
	{ NULL, NULL }
};

static void
extract_mockup (const gchar           *uri,
                TrackerSparqlBuilder  *metadata)
{
	MockupTag *p;

	tracker_sparql_builder_subject_iri (metadata, uri);
	tracker_sparql_builder_predicate (metadata, "a");
	tracker_sparql_builder_object (metadata, "nfo:Document");

	for (p = tags; p->key; ++p) {
		if (!p->key) {
			continue;
		}

		if (strcmp (p->key, "Image:Height") == 0) {
			gint64 value;

			value = g_ascii_strtoll (p->value, NULL, 10);
			tracker_sparql_builder_predicate (metadata, p->key);
			tracker_sparql_builder_object_int64 (metadata, value);
		} else if (strcmp (p->key, "Image:ExposureTime") == 0) {
			gdouble value;

			value = g_strtod (p->value, NULL);
			tracker_sparql_builder_predicate (metadata, p->key);
			tracker_sparql_builder_object_double (metadata, value);
		} else {
			/* If property is a raw string undefined in
			 * ontology use object_unvalidated() API, otherwise,
			 * just _object() API:
			 */
			tracker_sparql_builder_predicate (metadata, p->key);
			tracker_sparql_builder_object_unvalidated (metadata, p->value);
		}
	}
}

TrackerExtractData *
tracker_extract_get_data (void)
{
	return data;
}
