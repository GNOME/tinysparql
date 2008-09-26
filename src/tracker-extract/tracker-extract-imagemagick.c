/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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

#include <libtracker-common/tracker-os-dependant.h>

#include "tracker-extract.h"
#include "tracker-xmp.h"

static void
tracker_extract_imagemagick (const gchar *filename, GHashTable *metadata)
{
	gchar *argv[6];
	gchar *identify;
	gchar **lines;
	gint  exit_status;

	g_return_if_fail (filename != NULL);

	/* imagemagick crashes trying to extract from xcf files */
	if (g_str_has_suffix (filename, ".xcf")) {
		return;
	}

	argv[0] = g_strdup ("identify");
	argv[1] = g_strdup ("-format");
	argv[2] = g_strdup ("%w;\\n%h;\\n%c;\\n");
	if (g_str_has_suffix (filename, ".xcf")) {
		argv[3] = g_strdup (filename);
		argv[4] = NULL;
	}
	else {
		argv[3] = g_strdup ("-ping");
		argv[4] = g_strdup (filename);
	}
	argv[5] = NULL;

	if (tracker_spawn (argv, 10, &identify, &exit_status)) {
		if (exit_status == EXIT_SUCCESS) {
			lines = g_strsplit (identify, ";\n", 4);
			g_hash_table_insert (metadata, g_strdup ("Image:Width"), g_strdup (lines[0]));
			g_hash_table_insert (metadata, g_strdup ("Image:Height"), g_strdup (lines[1]));
			g_hash_table_insert (metadata, g_strdup ("Image:Comments"), g_strdup (g_strescape (lines[2], "")));
		}
	}

#ifdef HAVE_EXEMPI

	/* convert is buggy atm so disable temporarily */

	return;

	gchar *xmp;

	argv[0] = g_strdup ("convert");
	argv[1] = g_strdup (filename);
	argv[2] = g_strdup ("xmp:-");
	argv[3] = NULL;

	if (tracker_spawn (argv, 10, &xmp, &exit_status)) {
		if (exit_status == EXIT_SUCCESS && xmp) {
			tracker_read_xmp (xmp, strlen (xmp), metadata);
		}
	}
#endif
}


TrackerExtractorData data[] = {
	{ "image/*", tracker_extract_imagemagick },
	{ NULL, NULL }
};


TrackerExtractorData *
tracker_get_extractor_data (void)
{
	return data;
}
