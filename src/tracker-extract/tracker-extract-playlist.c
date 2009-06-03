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

#include <totem-pl-parser.h>
#include <libtracker-common/tracker-statement-list.h>

#include <libtracker-common/tracker-ontology.h>

#include "tracker-main.h"

#define PLAYLIST_PROPERTY_NO_TRACKS "Playlist:Songs"
#define PLAYLIST_PROPERTY_DURATION  "Playlist:Duration"
#define PLAYLIST_PROPERTY_CALCULATED "Playlist:ValidDuration"

#define PLAYLIST_DEFAULT_NO_TRACKS 0
#define PLAYLIST_DEFAULT_DURATION 0 


#define NFO_PREFIX TRACKER_NFO_PREFIX
#define RDF_PREFIX TRACKER_RDF_PREFIX
#define RDF_TYPE RDF_PREFIX "type"

typedef struct {
	guint        track_counter;
	gint64      total_time;
	GPtrArray   *metadata;
	const gchar *uri;
} PlaylistMetadata;

static void extract_playlist (const gchar *uri,
			      GPtrArray   *metadata);


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

	tracker_statement_list_insert (data->metadata, data->uri,
				  NFO_PREFIX "hasMediaFileListEntry", 
				  to_uri);

	data->track_counter += 1;

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
extract_playlist (const gchar *uri,
		  GPtrArray   *metadata)
{
	TotemPlParser       *pl;
	TotemPlParserResult  result;
	PlaylistMetadata     data = { 0, 0, metadata, uri };

	pl = totem_pl_parser_new ();

	g_object_set (pl, "recurse", FALSE, "disable-unsafe", TRUE, NULL);

	g_signal_connect (G_OBJECT (pl), "entry-parsed", 
			  G_CALLBACK (entry_parsed), &data);

	tracker_statement_list_insert (metadata, uri, 
	                          RDF_TYPE, 
	                          NFO_PREFIX "MediaList");

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

	/* TODO
	tracker_statement_list_insert_with_int (metadata, uri,
					   PLAYLIST_PROPERTY_DURATION, 
					   data.total_time);

	tracker_statement_list_insert_with_int (metadata, uri,
					   PLAYLIST_PROPERTY_NO_TRACKS, 
					   data.track_counter);

	tracker_statement_list_insert_with_int (metadata, uri,
					   PLAYLIST_PROPERTY_CALCULATED,
					   data.total_time);
	*/

	g_object_unref (pl);
}

TrackerExtractData *
tracker_get_extract_data (void)
{
	return playlist_data;
}
