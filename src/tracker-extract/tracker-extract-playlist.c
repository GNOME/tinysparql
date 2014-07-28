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
	TrackerSparqlBuilder *metadata;
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
	PlaylistMetadata *data;

	data = (PlaylistMetadata *) user_data;
	data->track_counter++;

	if (data->track_counter > 1000) {
		/* limit playlists to 1000 entries for query performance reasons */
		g_message ("Playlist has > 1000 entries. Ignoring for performance reasons.");
		return;
	}

	if (data->track_counter == 1) {
		/* first track, predicate needed */
		tracker_sparql_builder_predicate (data->metadata, "nfo:hasMediaFileListEntry");
	}

	tracker_sparql_builder_object_blank_open (data->metadata);
	tracker_sparql_builder_predicate (data->metadata, "a");
	tracker_sparql_builder_object (data->metadata, "nfo:MediaFileListEntry");

	tracker_sparql_builder_predicate (data->metadata, "nfo:entryUrl");
	tracker_sparql_builder_object_unvalidated (data->metadata, to_uri);

	tracker_sparql_builder_predicate (data->metadata, "nfo:listPosition");
	tracker_sparql_builder_object_int64 (data->metadata, (gint64) data->track_counter);

	tracker_sparql_builder_object_blank_close (data->metadata);

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
	TrackerSparqlBuilder *metadata;
	PlaylistMetadata data;
	GFile *file;
	gchar *uri;

	pl = totem_pl_parser_new ();
	file = tracker_extract_info_get_file (info);
	uri = g_file_get_uri (file);
	metadata = tracker_extract_info_get_metadata_builder (info);

	data.track_counter = PLAYLIST_DEFAULT_NO_TRACKS;
	data.total_time =  PLAYLIST_DEFAULT_DURATION;
	data.title = NULL;
	data.metadata = metadata;

	g_object_set (pl, "recurse", FALSE, "disable-unsafe", TRUE, NULL);

	g_signal_connect (G_OBJECT (pl), "playlist-started", G_CALLBACK (playlist_started), &data);
	g_signal_connect (G_OBJECT (pl), "entry-parsed", G_CALLBACK (entry_parsed), &data);

	tracker_sparql_builder_predicate (metadata, "a");
	tracker_sparql_builder_object (metadata, "nmm:Playlist");
	tracker_sparql_builder_object (metadata, "nfo:MediaList");

	if (totem_pl_parser_parse (pl, uri, FALSE) == TOTEM_PL_PARSER_RESULT_SUCCESS) {
		if (data.title != NULL) {
			g_message ("Playlist title:'%s'", data.title);
			tracker_sparql_builder_predicate (metadata, "nie:title");
			tracker_sparql_builder_object_unvalidated (metadata, data.title);
			g_free (data.title);
		} else {
			g_message ("Playlist has no title");
		}

		if (data.total_time > 0) {
			tracker_sparql_builder_predicate (metadata, "nfo:listDuration");
			tracker_sparql_builder_object_int64 (metadata, data.total_time);
		}

		if (data.track_counter > 0) {
			tracker_sparql_builder_predicate (metadata, "nfo:entryCounter");
			tracker_sparql_builder_object_int64 (metadata, data.track_counter);
		}
	} else {
		g_warning ("Playlist could not be parsed, no error given");
	}

	g_object_unref (pl);
	g_free (uri);

	return TRUE;
}
