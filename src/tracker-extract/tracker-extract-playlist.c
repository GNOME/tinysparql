/*
 * Copyright (C) 2007, Jamie McCracken <jamiemcc@gnome.org>
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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

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
	TrackerSparqlBuilder *metadata;
	const gchar *uri;
} PlaylistMetadata;

static void extract_playlist (const gchar          *uri,
                              TrackerSparqlBuilder *preupdate,
                              TrackerSparqlBuilder *metadata);

static TrackerExtractData playlist_data[] = {
	{ "audio/x-mpegurl", extract_playlist },
	{ "audio/mpegurl", extract_playlist },
	{ "audio/x-scpls", extract_playlist },
	{ "audio/x-pn-realaudio", extract_playlist },
	{ "application/ram", extract_playlist },
	{ "application/vnd.ms-wpl", extract_playlist },
	{ "application/smil", extract_playlist },
	{ "audio/x-ms-asx", extract_playlist },
	{ NULL, NULL }
};

static void
entry_parsed (TotemPlParser *parser, const gchar *to_uri, GHashTable *to_metadata, gpointer user_data)
{
	gchar *duration;
	PlaylistMetadata *data;

	data = (PlaylistMetadata *)user_data;
	data->track_counter++;

	if (data->track_counter == 1) {
		/* first track, predicate needed */
		tracker_sparql_builder_predicate (data->metadata, "nfo:hasMediaFileListEntry");
	}

	tracker_sparql_builder_object_blank_open (data->metadata);
	tracker_sparql_builder_predicate (data->metadata, "a");
	tracker_sparql_builder_object (data->metadata, "nfo:MediaFileListEntry");

	tracker_sparql_builder_predicate (data->metadata, "nfo:entryContent");
	tracker_sparql_builder_object_unvalidated (data->metadata, to_uri);

	tracker_sparql_builder_predicate (data->metadata, "nfo:listPosition");
	tracker_sparql_builder_object_int64 (data->metadata, (gint64) data->track_counter);

	tracker_sparql_builder_object_blank_close (data->metadata);

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

static void
extract_playlist (const gchar          *uri,
                  TrackerSparqlBuilder *preupdate,
                  TrackerSparqlBuilder *metadata)
{
	TotemPlParser       *pl;
	TotemPlParserResult  result;
	PlaylistMetadata     data = { 0, 0, metadata, uri };

	pl = totem_pl_parser_new ();

	g_object_set (pl, "recurse", FALSE, "disable-unsafe", TRUE, NULL);

	g_signal_connect (G_OBJECT (pl), "entry-parsed",
	                  G_CALLBACK (entry_parsed), &data);

	tracker_sparql_builder_predicate (metadata, "a");
	tracker_sparql_builder_object (metadata, "nmm:Playlist");

	result = totem_pl_parser_parse (pl, uri, FALSE);

	switch (result) {
	case TOTEM_PL_PARSER_RESULT_SUCCESS:
		break;
	case TOTEM_PL_PARSER_RESULT_IGNORED:
	case TOTEM_PL_PARSER_RESULT_ERROR:
	case TOTEM_PL_PARSER_RESULT_UNHANDLED:
		data.total_time = PLAYLIST_DEFAULT_NO_TRACKS;
		data.track_counter = PLAYLIST_DEFAULT_DURATION;
		break;
	default:
		g_warning ("Undefined result in totem-plparser");
	}

	if (data.total_time > 0) {
		tracker_sparql_builder_predicate (metadata, "nfo:listDuration");
		tracker_sparql_builder_object_int64 (metadata, data.total_time);
	}

	if (data.track_counter > 0) {
		tracker_sparql_builder_predicate (metadata, "nfo:entryCounter");
		tracker_sparql_builder_object_int64 (metadata, data.track_counter);
	}

	g_object_unref (pl);
}

TrackerExtractData *
tracker_extract_get_data (void)
{
	return playlist_data;
}
