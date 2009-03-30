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

#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <libtracker-common/tracker-file-utils.h>

#include "tracker-main.h"
#include "tracker-escape.h"

static void extract_abw (const gchar *filename,
			 GHashTable  *metadata);

static TrackerExtractData data[] = {
	{ "application/x-abiword", extract_abw },
	{ NULL, NULL }
};

static void
extract_abw (const gchar *filename,
	     GHashTable  *metadata)
{
	FILE *f;

	f = tracker_file_open (filename, "r", TRUE);

	if (f) {
		gchar  *line;
		gsize  length;
		gssize read_char;

		line = NULL;
		length = 0;

		while ((read_char = getline (&line, &length, f)) != -1) {
			if (g_str_has_suffix (line, "</m>\n")) {
				line[read_char - 5] = '\0';
			}
			if (g_str_has_prefix (line, "<m key=\"dc.title\">")) {
				g_hash_table_insert (metadata,
						     g_strdup ("Doc:Title"),
						     tracker_escape_metadata (line + 18));
			}
			else if (g_str_has_prefix (line, "<m key=\"dc.subject\">")) {
				g_hash_table_insert (metadata,
						     g_strdup ("Doc:Subject"),
						     tracker_escape_metadata (line + 20));
			}
			else if (g_str_has_prefix (line, "<m key=\"dc.creator\">")) {
				g_hash_table_insert (metadata,
						     g_strdup ("Doc:Author"),
						     tracker_escape_metadata (line + 20));
			}
			else if (g_str_has_prefix (line, "<m key=\"abiword.keywords\">")) {
				g_hash_table_insert (metadata,
						     g_strdup ("Doc:Keywords"),
						     tracker_escape_metadata (line + 26));
			}
			else if (g_str_has_prefix (line, "<m key=\"dc.description\">")) {
				g_hash_table_insert (metadata,
						     g_strdup ("Doc:Comments"),
						     tracker_escape_metadata (line + 24));
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
tracker_get_extract_data (void)
{
	return data;
}
