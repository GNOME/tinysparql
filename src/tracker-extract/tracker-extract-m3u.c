/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007, Mr Jamie McCracken (jamiemcc@gnome.org)
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

#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

#include "tracker-extract.h"

#define M3U_PROPERTY_NO_TRACKS "Playlist:Songs"
#define M3U_PROPERTY_DURATION  "Playlist:TotalLength"

static void extract_m3u (const gchar *filename,
			 GHashTable  *metadata);


static TrackerExtractorData data[] = {
	{ "audio/x-mpegurl", extract_m3u },
	{ "audio/mpegurl", extract_m3u },
	{ NULL, NULL }
};


static void
extract_m3u (const gchar *filename,
	     GHashTable  *metadata)
{

	GMappedFile *file;
	GError      *error = NULL;
	gchar       *contents;
	gchar      **lines;
	gint         i = 0, songs = 0, total_time = 0;
	gboolean     has_metadata = FALSE;

	g_type_init ();

	file = g_mapped_file_new (filename, FALSE, &error);

	if (error) {
		return;
	}

	contents = g_mapped_file_get_contents (file);

	if (!contents) {
		return;
	}

	lines = g_strsplit (contents, "\n", -1);

	if (!lines || !lines[0]) {
		return;
	}

	if (strncmp (lines[0], "#EXTM3U", 7)) {
		g_debug ("M3U extractor: '%s' is not extended m3u (basic m3u or empty file)",
			   filename);
		has_metadata = FALSE;
	}

	for (i = 1; lines[i] != NULL; i++) {
		/* Skip blank lines */
		if (g_utf8_strlen (lines[i], -1) < 2) {
			continue;
		}

		/* Process EXTINF meta information lines */
		if (has_metadata && g_str_has_prefix (lines[i], "#EXTINF:")) {
			gchar **head;
			gint    time;

			/* #EXTINF:nnn, ... where nnn is length of the song in seconds */
			head = g_strsplit (lines[i] + strlen ("#EXTINF:"), ",", 2);
			
			if (!head || g_strv_length (head) < 1) {
				g_warning ("Error processing a line in %s\n",
					   filename);
				continue;
			}

			time = atoi (head[0]);

			if (time > 0) {
				total_time += time;
			}

			g_strfreev (head);

			continue;
		}
		songs += 1;
	}

	g_strfreev (lines);

	if (has_metadata) {
		if (total_time < 0) {
			total_time = -1;
		}

		g_hash_table_insert (metadata,
				     g_strdup (M3U_PROPERTY_DURATION),
				     g_strdup_printf ("%d", total_time));
	}

	g_hash_table_insert (metadata,
			     g_strdup (M3U_PROPERTY_NO_TRACKS),
			     g_strdup_printf ("%d", songs));

	return;

}

TrackerExtractorData *
tracker_get_extractor_data (void)
{
	return data;
}
