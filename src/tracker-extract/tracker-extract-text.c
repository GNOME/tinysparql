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

#define TRY_LOCALE_TO_UTF8_CONVERSION 0

static gchar *
get_file_content (GFile *file,
                  gsize  n_bytes)
{
	GError *error = NULL;
	gchar *text, *uri, *path;
	int fd;

	/* If no content requested, return */
	if (n_bytes == 0) {
		return NULL;
	}

	uri = g_file_get_uri (file);

	/* Get filename from URI */
	path = g_file_get_path (file);

	fd = g_open (path, O_RDONLY | O_NOATIME, 0);

	if (fd == -1) {
		g_message ("Could not open file '%s': %s",
		           uri,
		           error->message);
		g_error_free (error);
		g_free (uri);
		g_free (path);
		return NULL;
	}

	g_debug ("  Starting to read '%s' up to %" G_GSIZE_FORMAT " bytes...",
	         uri, n_bytes);

	/* Read up to n_bytes from stream. Output is always, always valid UTF-8 */
	text = tracker_read_text_from_fd (fd,
	                                  n_bytes,
	                                  TRY_LOCALE_TO_UTF8_CONVERSION);

	close (fd);
	g_free (uri);
	g_free (path);

	return text;
}

G_MODULE_EXPORT gboolean
tracker_extract_module_init (TrackerModuleThreadAwareness  *thread_awareness_ret,
                             GError                       **error)
{
	*thread_awareness_ret = TRACKER_MODULE_MULTI_THREAD;
	return TRUE;
}

G_MODULE_EXPORT gboolean
tracker_extract_get_metadata (TrackerExtractInfo *info)
{
	TrackerSparqlBuilder *metadata;
	TrackerConfig *config;
	gchar *content;

	config = tracker_main_get_config ();
	metadata = tracker_extract_info_get_metadata_builder (info);

	content = get_file_content (tracker_extract_info_get_file (info),
	                            tracker_config_get_max_bytes (config));

	tracker_sparql_builder_predicate (metadata, "a");
	tracker_sparql_builder_object (metadata, "nfo:PlainTextDocument");

	if (content) {
		tracker_sparql_builder_predicate (metadata, "nie:plainTextContent");
		tracker_sparql_builder_object_unvalidated (metadata, content);
		g_free (content);
	}

	return TRUE;
}
