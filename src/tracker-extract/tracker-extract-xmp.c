/* Tracker Extract - extracts embedded metadata from files
 * Copyright (C) 2007, Jason Kivlighn (jkivlighn@gmail.com)
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

#include <glib.h>
#include <gio/gio.h>

#include <libtracker-common/tracker-statement-list.h>
#include <libtracker-common/tracker-ontology.h>
#include <libtracker-common/tracker-type-utils.h>
#include <libtracker-common/tracker-statement-list.h>
#include <libtracker-common/tracker-file-utils.h>
#include <libtracker-common/tracker-utils.h>

#include "tracker-main.h"
#include "tracker-xmp.h"


#define NMM_PREFIX TRACKER_NMM_PREFIX
#define NFO_PREFIX TRACKER_NFO_PREFIX
#define NIE_PREFIX TRACKER_NIE_PREFIX
#define DC_PREFIX TRACKER_DC_PREFIX
#define NCO_PREFIX TRACKER_NCO_PREFIX
#define RDF_PREFIX TRACKER_RDF_PREFIX

static void extract_xmp (const gchar          *filename, 
                         TrackerSparqlBuilder *metadata);

static TrackerExtractData data[] = {
	{ "application/rdf+xml", extract_xmp },
	{ NULL, NULL }
};

/* This function is used to find the URI for a file.xmp file. The point here is 
 * that the URI for file.xmp is not file:///file.xmp but instead for example
 * file:///file.jpeg or file:///file.png. The reason is that file.xmp is a
 * sidekick, and a sidekick doesn't describe itself, it describes another file. */

static gchar *
find_orig_uri (const gchar *xmp_filename)
{
	GFile *file;
	GFile *dir;
	GFileEnumerator *iter;
	GFileInfo *orig_info;
	const gchar *filename_a;
	gchar *found_file = NULL;

	file = g_file_new_for_path (xmp_filename);
	dir = g_file_get_parent (file);

	orig_info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_NAME,
	                               G_FILE_QUERY_INFO_NONE, 
	                               NULL, NULL);

	filename_a = g_file_info_get_name (orig_info);

	iter = g_file_enumerate_children (dir, G_FILE_ATTRIBUTE_STANDARD_NAME, 
	                                  G_FILE_QUERY_INFO_NONE, 
	                                  NULL, NULL);

	if (iter) {
		GFileInfo *info;

		while ((info = g_file_enumerator_next_file (iter, NULL, NULL)) && !found_file) {
			const gchar *filename_b;
			const gchar *ext_a, *ext_b;
			gchar *casefold_a, *casefold_b;

			/* OK, important: 
			 * 1. Files can't be the same.
			 * 2. File names (without extension) must match
			 * 3. Something else? */

			filename_b = g_file_info_get_name (info);

			ext_a = g_utf8_strrchr (filename_a, -1, '.');
			ext_b = g_utf8_strrchr (filename_b, -1, '.');

			/* Look for extension */
			if (!ext_a || !ext_b) {
				g_object_unref (info);
				continue;
			}

			/* Name part is the same length */
			if ((ext_a - filename_a) != (ext_b - filename_b)) {
				g_object_unref (info);
				continue;
			}
			
			/* Check extensions are not the same (i.e. same len and ext) */
			if (g_strcmp0 (ext_a, ext_b) == 0) {
				g_object_unref (info);
				continue;
			}

			/* Don't compare the ".xmp" with ".jpeg" and don't match the same file */

			/* Now compare name (without ext) and make
			 * sure they are the same in a caseless
			 * compare. */

			casefold_a = g_utf8_casefold (filename_a, (ext_a - filename_a));
			casefold_b = g_utf8_casefold (filename_b, (ext_b - filename_b));

			if (g_strcmp0 (casefold_a, casefold_b) == 0) {
				GFile *found;

				found = g_file_get_child (dir, filename_b);
				found_file = g_file_get_uri (found);
				g_object_unref (found);
			}

			g_free (casefold_a);
			g_free (casefold_b);
			g_object_unref (info);
		}

		g_object_unref (iter);
	}

	g_object_unref (orig_info);
	g_object_unref (file);
	g_object_unref (dir);

	return found_file;
}


static void
insert_keywords (TrackerSparqlBuilder *metadata, const gchar *uri, gchar *keywords)
{
	char *lasts, *keyw;
	size_t len;

	keyw = keywords;
	keywords = strchr (keywords, '"');
	if (keywords)
		keywords++;
	else 
		keywords = keyw;

	len = strlen (keywords);
	if (keywords[len - 1] == '"')
		keywords[len - 1] = '\0';

	for (keyw = strtok_r (keywords, ",; ", &lasts); keyw; 
	     keyw = strtok_r (NULL, ",; ", &lasts)) {
		tracker_statement_list_insert (metadata, uri, 
		                               NIE_PREFIX "keyword", 
		                               (const gchar*) keyw);
	}
}

static void
extract_xmp (const gchar          *uri, 
             TrackerSparqlBuilder *metadata)
{
	gchar *contents;
	gsize length;
	GError *error;
	gchar *filename = g_filename_from_uri (uri, NULL, NULL);
	TrackerXmpData xmp_data = { 0 };

	if (g_file_get_contents (filename, &contents, &length, &error)) {
		gchar *orig_uri = find_orig_uri (filename);

		/* If no orig file is found for the sidekick, we use the sidekick to
		 * describe itself instead, falling back to uri */

		tracker_read_xmp (contents,
		                  length, 
		                  orig_uri ? orig_uri : uri, 
		                  &xmp_data);

		if (xmp_data.keywords) {
			insert_keywords (metadata, uri, xmp_data.keywords);
			g_free (xmp_data.keywords);
		}

		if (xmp_data.subject) {
			insert_keywords (metadata, uri, xmp_data.subject);
			g_free (xmp_data.subject);
		}

		if (xmp_data.publisher) {
			tracker_statement_list_insert (metadata, ":", RDF_PREFIX "type", NCO_PREFIX "Contact");
			tracker_statement_list_insert (metadata, ":", NCO_PREFIX "fullname", xmp_data.publisher);
			tracker_statement_list_insert (metadata, uri, NCO_PREFIX "publisher", ":");
			g_free (xmp_data.publisher);
		}

		if (xmp_data.type) {
			tracker_statement_list_insert (metadata, uri, DC_PREFIX "type", xmp_data.type);
			g_free (xmp_data.type);
		}

		if (xmp_data.format) {
			tracker_statement_list_insert (metadata, uri, DC_PREFIX "format", xmp_data.format);
			g_free (xmp_data.format);
		}

		if (xmp_data.identifier) {
			tracker_statement_list_insert (metadata, uri, DC_PREFIX "identifier", xmp_data.identifier);
			g_free (xmp_data.identifier);
		}

		if (xmp_data.source) {
			tracker_statement_list_insert (metadata, uri, DC_PREFIX "source", xmp_data.source);
			g_free (xmp_data.source);
		}

		if (xmp_data.language) {
			tracker_statement_list_insert (metadata, uri, DC_PREFIX "language", xmp_data.language);
			g_free (xmp_data.language);
		}

		if (xmp_data.relation) {
			tracker_statement_list_insert (metadata, uri, DC_PREFIX "relation", xmp_data.relation);
			g_free (xmp_data.relation);
		}

		if (xmp_data.coverage) {
			tracker_statement_list_insert (metadata, uri, DC_PREFIX "coverage", xmp_data.coverage);
			g_free (xmp_data.coverage);
		}

		if (xmp_data.license) {
			tracker_statement_list_insert (metadata, uri, NIE_PREFIX "license", xmp_data.license);
			g_free (xmp_data.license);
		}

		if (xmp_data.Make || xmp_data.Model) {
			gchar *final_camera = tracker_coalesce (2, xmp_data.Make, xmp_data.Model); 
			tracker_statement_list_insert (metadata, uri, NMM_PREFIX "camera", final_camera);
			g_free (final_camera);
		}

		if (xmp_data.title) {
			tracker_statement_list_insert (metadata, uri, NIE_PREFIX "title", xmp_data.title);
			g_free (xmp_data.title);
		}

		if (xmp_data.Orientation) {
			tracker_statement_list_insert (metadata, uri, NFO_PREFIX "orientation", xmp_data.Orientation);
			g_free (xmp_data.Orientation);
		}

		if (xmp_data.rights) {
			tracker_statement_list_insert (metadata, uri, NIE_PREFIX "copyright", xmp_data.rights);
			g_free (xmp_data.rights);
		}

		if (xmp_data.WhiteBalance) {
			tracker_statement_list_insert (metadata, uri, NMM_PREFIX "whiteBalance", xmp_data.WhiteBalance);
			g_free (xmp_data.WhiteBalance);
		}

		if (xmp_data.FNumber) {
			tracker_statement_list_insert (metadata, uri, NMM_PREFIX "fnumber", xmp_data.FNumber);
			g_free (xmp_data.FNumber);
		}

		if (xmp_data.Flash) {
			tracker_statement_list_insert (metadata, uri, NMM_PREFIX "flash", xmp_data.Flash);
			g_free (xmp_data.Flash);
		}

		if (xmp_data.FocalLength) {
			tracker_statement_list_insert (metadata, uri, NMM_PREFIX "focalLength", xmp_data.FocalLength);
			g_free (xmp_data.FocalLength);
		}

		if (xmp_data.Artist || xmp_data.contributor) {
			gchar *final_artist =  tracker_coalesce (2, xmp_data.Artist, xmp_data.contributor);
			tracker_statement_list_insert (metadata, ":", RDF_PREFIX "type", NCO_PREFIX "Contact");
			tracker_statement_list_insert (metadata, ":", NCO_PREFIX "fullname", final_artist);
			tracker_statement_list_insert (metadata, uri, NCO_PREFIX "contributor", ":");
			g_free (final_artist);
		}

		if (xmp_data.ExposureTime) {
			tracker_statement_list_insert (metadata, uri, NMM_PREFIX "exposureTime", xmp_data.ExposureTime);
			g_free (xmp_data.ExposureTime);
		}

		if (xmp_data.ISOSpeedRatings) {
			tracker_statement_list_insert (metadata, uri, NMM_PREFIX "isoSpeed", xmp_data.ISOSpeedRatings);
			g_free (xmp_data.ISOSpeedRatings);
		}

		if (xmp_data.date || xmp_data.DateTimeOriginal) {
			gchar *final_date =  tracker_coalesce (2, xmp_data.date, xmp_data.DateTimeOriginal);
			tracker_statement_list_insert (metadata, uri, NIE_PREFIX "contentCreated", final_date);
			g_free (final_date);
		}

		if (xmp_data.description) {
			tracker_statement_list_insert (metadata, uri, NIE_PREFIX "description", xmp_data.description);
			g_free (xmp_data.description);
		}

		if (xmp_data.MeteringMode) {
			tracker_statement_list_insert (metadata, uri, NMM_PREFIX "meteringMode", xmp_data.MeteringMode);
			g_free (xmp_data.MeteringMode);
		}

		if (xmp_data.creator) {
			tracker_statement_list_insert (metadata, ":", RDF_PREFIX "type", NCO_PREFIX "Contact");
			tracker_statement_list_insert (metadata, ":", NCO_PREFIX "fullname", xmp_data.creator);
			tracker_statement_list_insert (metadata, uri, NCO_PREFIX "creator", ":");
			g_free (xmp_data.creator);
		}

		g_free (orig_uri);
	}

	g_free (filename);

}

TrackerExtractData *
tracker_get_extract_data (void)
{
	return data;
}
