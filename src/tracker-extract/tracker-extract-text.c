/*
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <gio/gio.h>

#include <libtracker-extract/tracker-extract.h>

#include "tracker-main.h"
#include "tracker-read.h"

#define  TRY_LOCALE_TO_UTF8_CONVERSION 0

static void extract_text (const gchar          *uri,
                          TrackerSparqlBuilder *preupdate,
                          TrackerSparqlBuilder *metadata);

static TrackerExtractData data[] = {
	{ "text/*", extract_text },
	{ NULL, NULL }
};


static gchar *
get_file_content (const gchar  *uri,
                  gsize         n_bytes)
{
	gchar *text, *path;
	int fd;

	/* If no content requested, return */
	if (n_bytes == 0) {
		return NULL;
	}

	/* Get filename from URI */
	path = g_filename_from_uri (uri, NULL, NULL);

	fd = g_open (path, O_RDONLY | O_NOATIME, 0);
	if (fd == -1 && errno == EPERM) {
		fd = g_open (path, O_RDONLY, 0);
	}

	if (fd == -1) {
		g_message ("Could not open file '%s': %s",
		           uri,
		           g_strerror (errno));
		g_free (path);
		return NULL;
	}

	g_debug ("  Starting to read '%s' up to %" G_GSIZE_FORMAT " bytes...",
	         uri, n_bytes);

	/* Read up to n_bytes from stream. Output is always, always valid UTF-8,
	 * this function closes the FD.
	 */
	text = tracker_read_text_from_fd (fd,
	                                  n_bytes,
	                                  TRY_LOCALE_TO_UTF8_CONVERSION);
	g_free (uri);
	g_free (path);

	return text;
}

static void
extract_text (const gchar          *uri,
              TrackerSparqlBuilder *preupdate,
              TrackerSparqlBuilder *metadata)
{
	TrackerConfig *config;
	gchar *content;

	config = tracker_main_get_config ();

	content = get_file_content (uri,
	                            tracker_config_get_max_bytes (config));

	tracker_sparql_builder_predicate (metadata, "a");
	tracker_sparql_builder_object (metadata, "nfo:PlainTextDocument");

	if (content) {
		tracker_sparql_builder_predicate (metadata, "nie:plainTextContent");
		tracker_sparql_builder_object_unvalidated (metadata, content);
		g_free (content);
	}
}

TrackerExtractData *
tracker_extract_get_data (void)
{
	return data;
}
