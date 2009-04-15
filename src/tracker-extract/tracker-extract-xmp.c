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

#include <libtracker-common/tracker-statement-list.h>

#include "tracker-main.h"
#include "tracker-xmp.h"

static void extract_xmp (const gchar *filename, 
                         GPtrArray   *metadata);

static TrackerExtractData data[] = {
	{ "application/rdf+xml", extract_xmp },
	{ NULL, NULL }
};

static void
extract_xmp (const gchar *uri, 
             GPtrArray   *metadata)
{
	gchar *contents;
	gsize length;
	GError *error;
	gchar *filename = g_filename_from_uri (uri, NULL, NULL);

	if (g_file_get_contents (filename, &contents, &length, &error)) {
		/* URI is very very wrong here. The URI is location://filename.xmp whereas
		 * the metadata is about location://filename.jpeg (in case it's a sidecar
		 * for filename.jpeg) */
		tracker_read_xmp (contents, length, uri, metadata);
	}

	g_free (filename);

}

TrackerExtractData *
tracker_get_extract_data (void)
{
	return data;
}
