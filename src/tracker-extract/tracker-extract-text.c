/*
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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

#include <string.h>

#include <glib.h>
#include <gio/gio.h>

#include <libtracker-extract/tracker-extract.h>

#include "tracker-main.h"
#include "tracker-iochannel.h"

#define  TRY_LOCALE_TO_UTF8_CONVERSION 0

static void extract_text (const gchar          *uri,
                          TrackerSparqlBuilder *preupdate,
                          TrackerSparqlBuilder *metadata);

static TrackerExtractData data[] = {
	{ "text/*", extract_text },
	{ NULL, NULL }
};


static gchar *
get_file_content (const gchar *uri,
                  gsize        n_bytes)
{
	GIOChannel *channel;
	GError     *error = NULL;
	gchar      *text;
	gchar      *filename;

	/* Get filename from URI */
	filename = g_filename_from_uri (uri, NULL, &error);
	if (error) {
		g_message ("Could not get filename from URI '%s': %s",
		           uri,
		           error->message);
		g_error_free (error);

		return NULL;
	}

	/* New channel from the given file */
	channel = g_io_channel_new_file (filename, "r", &error);
	if (error) {
		g_message ("Could not read file '%s': %s",
		           uri,
		           error->message);
		g_error_free (error);
		g_free (filename);

		return NULL;
	}

	g_free (filename);

	g_debug ("  Starting to read '%s' up to %" G_GSIZE_FORMAT " bytes...",
	         uri, n_bytes);

	/* Read up to n_bytes from stream */
	text = tracker_iochannel_read_text (channel,
	                                    n_bytes,
	                                    TRY_LOCALE_TO_UTF8_CONVERSION,
	                                    TRUE);

	/* Note: Channel already closed and unrefed */

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
