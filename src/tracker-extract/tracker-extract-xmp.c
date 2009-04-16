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

#include "tracker-main.h"
#include "tracker-xmp.h"

static void extract_xmp (const gchar *filename, 
                         GPtrArray   *metadata);

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
	GFile *file = g_file_new_for_path (xmp_filename);
	GFile *dir = g_file_get_parent (file);
	GFileEnumerator *iter;
	GFileInfo *orig_info;
	gchar *compare_part, *found_file = NULL;
	guint len;

	orig_info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_NAME,
								   G_FILE_QUERY_INFO_NONE, NULL, NULL);

	compare_part = g_utf8_strup (g_file_info_get_name (orig_info), -1);

	len = g_utf8_strlen (compare_part, -1);

	iter =  g_file_enumerate_children (dir, G_FILE_ATTRIBUTE_STANDARD_NAME, 
									   G_FILE_QUERY_INFO_NONE, NULL, NULL);

	if (iter) {
		GFileInfo *info;

		while ((info = g_file_enumerator_next_file (iter, NULL, NULL)) && !found_file) {
			gchar *compare_with;
			const gchar *filename;

			filename = g_file_info_get_name (orig_info);
			compare_with = g_utf8_strup (filename, -1);

			/* Don't compare the ".xmp" with ".jpeg" and don't match the same file */

			if (g_strncasecmp (compare_part, compare_with, len - 4) == 0 &&
				g_strcmp0 (compare_part, compare_with) != 0) {
				GFile *found = g_file_get_child (dir, filename);
				found_file = g_file_get_uri (found);
				g_object_unref (found);
			}

			g_free (compare_with);
			g_object_unref (info);
		}

		g_object_unref (iter);
	}

	g_object_unref (orig_info);
	g_object_unref (file);
	g_object_unref (dir);
	g_free (compare_part);

	return found_file;
}

static void
extract_xmp (const gchar *uri, 
             GPtrArray   *metadata)
{
	gchar *contents;
	gsize length;
	GError *error;
	gchar *filename = g_filename_from_uri (uri, NULL, NULL);

	if (g_file_get_contents (filename, &contents, &length, &error)) {
		gchar *orig_uri = find_orig_uri (filename);

		/* If no orig file is found for the sidekick, we use the sidekick to
		 * describe itself instead, falling back to uri */
		tracker_read_xmp (contents, length, orig_uri ? orig_uri : uri, metadata);

		g_free (orig_uri);
	}

	g_free (filename);

}

TrackerExtractData *
tracker_get_extract_data (void)
{
	return data;
}
