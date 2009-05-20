/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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

#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <png.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <libtracker-common/tracker-type-utils.h>
#include <libtracker-common/tracker-file-utils.h>

#include "tracker-main.h"
#include "tracker-escape.h"

static void extract_mockup (const gchar *filename,
			    GHashTable  *metadata);

typedef struct {
	gchar	     *key;
	gchar        *value;
} MockupTag;

static MockupTag tags[] = {
	{ "Audio:Album", "Album" },
	{ "Audio:Artist", "Artist" },
	{ "Audio:Title", "Title" },
	{ "Video:Title", "Title for Video" },
	{ "Audio:Genre", "Genre" },
	{ "Audio:Duration", "5" },
	{ "Image:Location", "Here" },
#ifdef ENABLE_DETAILED_METADATA
	{ "Image:Software", "Softa" },
#endif /* ENABLE_DETAILED_METADATA */
	{ NULL, NULL }
};

static TrackerExtractData data[] = {
	{ "audio/*", extract_mockup },
	{ "video/*", extract_mockup },
	{ "image/*", extract_mockup },
	{ NULL, NULL }
};

static void
extract_mockup (const gchar *filename,
	     GHashTable  *metadata)
{
	MockupTag *p;

	for (p=tags;p->key;++p) {		
		g_hash_table_insert (metadata,
				     g_strdup (p->key),
				     tracker_escape_metadata (p->value)); 
	}
}

TrackerExtractData *
tracker_get_extract_data (void)
{
	return data;
}
