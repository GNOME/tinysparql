/*
 * Copyright (C) 2007, Jamie McCracken <jamiemcc@gnome.org>
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

#include <string.h>
#include <stdlib.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

#include <totem-pl-parser.h>

#include <libtracker-extract/tracker-extract.h>
#include <libtracker-extract/tracker-guarantee.h>

#define PLAYLIST_PROPERTY_NO_TRACKS "entryCounter"
#define PLAYLIST_PROPERTY_DURATION  "listDuration"
/*
  FIXME Decide what to do with this in nepomuk
  #define PLAYLIST_PROPERTY_CALCULATED "Playlist:ValidDuration"
*/

#define PLAYLIST_DEFAULT_NO_TRACKS 0
#define PLAYLIST_DEFAULT_DURATION 0

typedef struct {
	guint32 track_counter;
	gint64 total_time;
	gchar *title;
	TrackerResource *metadata;
} PlaylistMetadata;

static void
playlist_started (TotemPlParser         *parser,
                  gchar                 *to_uri,
                  TotemPlParserMetadata *to_metadata,
                  gpointer               user_data)
{
	PlaylistMetadata *data;

	data = (PlaylistMetadata *) user_data;

	/* Avoid looking up every time */
	data->title = g_strdup (g_hash_table_lookup (to_metadata, TOTEM_PL_PARSER_FIELD_TITLE));
}

static void
entry_parsed (TotemPlParser *parser,
              gchar         *to_uri,
              GHashTable    *to_metadata,
              gpointer       user_data)
{
	TrackerResource *entry;
	PlaylistMetadata *data;

	data = (PlaylistMetadata *) user_data;
	data->track_counter++;

	if (data->track_counter > 1000) {
		/* limit playlists to 1000 entries for query performance reasons */
		g_message ("Playlist has > 1000 entries. Ignoring for performance reasons.");
		return;
	}

	entry = tracker_resource_new (NULL);
	tracker_resource_set_uri (entry, "rdf:type", "nfo:MediaFileListEntry");
	tracker_resource_set_string (entry, "nfo:entryUrl", to_uri);
	tracker_resource_set_int (entry, "nfo:listPosition", data->track_counter);

	if (data->track_counter == 1) {
		/* This causes all existing relations to be deleted, when we serialize
		 * to SPARQL. */
		tracker_resource_set_relation (data->metadata, "nfo:hasMediaFileListEntry", entry);
	} else {
		tracker_resource_add_relation (data->metadata, "nfo:hasMediaFileListEntry", entry);
	}
	g_object_unref (entry);

	if (to_metadata) {
		gchar *duration;

		duration = g_hash_table_lookup (to_metadata, TOTEM_PL_PARSER_FIELD_DURATION);

		if (duration == NULL) {
			duration = g_hash_table_lookup (to_metadata, TOTEM_PL_PARSER_FIELD_DURATION_MS);
		}

		if (duration != NULL) {
			gint64 secs = totem_pl_parser_parse_duration (duration, FALSE);

			if (secs > 0) {
				data->total_time += secs;
			}
		}
	}
}

G_MODULE_EXPORT gboolean
tracker_extract_get_metadata (TrackerExtractInfo *info)
{
	TotemPlParser *pl;
	TrackerResource *metadata;
	PlaylistMetadata data;
	GFile *file;
	gchar *uri;

	pl = totem_pl_parser_new ();
	file = tracker_extract_info_get_file (info);
	uri = g_file_get_uri (file);

	metadata = data.metadata = tracker_resource_new (NULL);

	data.track_counter = PLAYLIST_DEFAULT_NO_TRACKS;
	data.total_time =  PLAYLIST_DEFAULT_DURATION;
	data.title = NULL;

	g_object_set (pl, "recurse", FALSE, "disable-unsafe", TRUE, NULL);

	g_signal_connect (G_OBJECT (pl), "playlist-started", G_CALLBACK (playlist_started), &data);
	g_signal_connect (G_OBJECT (pl), "entry-parsed", G_CALLBACK (entry_parsed), &data);

	tracker_resource_add_uri (metadata, "rdf:type", "nmm:Playlist");
	tracker_resource_add_uri (metadata, "rdf:type", "nfo:MediaList");

	if (totem_pl_parser_parse (pl, uri, FALSE) == TOTEM_PL_PARSER_RESULT_SUCCESS) {
		if (data.title != NULL) {
			g_message ("Playlist title:'%s'", data.title);
			tracker_resource_set_string (metadata, "nie:title", data.title);
			g_free (data.title);
		} else {
			g_message ("Playlist has no title, attempting to get one from filename");
			tracker_guarantee_resource_title_from_file (metadata, "nie:title", NULL, uri, NULL);
		}

		if (data.total_time > 0) {
			tracker_resource_set_int64 (metadata, "nfo:listDuration", data.total_time);
		}

		if (data.track_counter > 0) {
			tracker_resource_set_int64 (metadata, "nfo:entryCounter", data.track_counter);
		}
	} else {
		g_warning ("Playlist could not be parsed, no error given");
	}

	g_object_unref (pl);
	g_free (uri);

	tracker_extract_info_set_resource (info, metadata);
	g_object_unref (metadata);

	return TRUE;
}
