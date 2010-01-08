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

#include "tracker-main.h"

#define PLAYLIST_PROPERTY_NO_TRACKS "Playlist:Songs"
#define PLAYLIST_PROPERTY_DURATION  "Playlist:Duration"
#define PLAYLIST_PROPERTY_CALCULATED "Playlist:ValidDuration"

#define PLAYLIST_DEFAULT_NO_TRACKS 0
#define PLAYLIST_DEFAULT_DURATION 0 

typedef struct {
	guint32     track_counter;
	gint64      total_time;
} PlaylistMetadata;

static void extract_playlist (const gchar *filename,
			      GHashTable  *metadata);


static TrackerExtractData data[] = {
	{ "audio/x-mpegurl", extract_playlist },
	{ "audio/mpegurl", extract_playlist },
	{ "audio/x-scpls", extract_playlist },
	{ "audio/x-pn-realaudio", extract_playlist },
	{ "application/ram", extract_playlist },
	{ "application/vnd.ms-wpl", extract_playlist }, 
	{ "application/smil", extract_playlist }, 
	{ "audio/x-ms-asx", extract_playlist },
	{ "application/x-ms-asx", extract_playlist },
	{ NULL, NULL }
};

static void
entry_parsed (TotemPlParser *parser, const gchar *uri, GHashTable *metadata, gpointer user_data)
{
	gchar *duration;
	PlaylistMetadata *data;

	data = (PlaylistMetadata *)user_data;

	data->track_counter += 1;

	duration = g_hash_table_lookup (metadata, TOTEM_PL_PARSER_FIELD_DURATION);

	if (duration == NULL) {
		duration = g_hash_table_lookup (metadata, TOTEM_PL_PARSER_FIELD_DURATION_MS);
	}

	if (duration != NULL) {
		gint64 secs = totem_pl_parser_parse_duration (duration, FALSE);
		if (secs > 0) {
			data->total_time += secs;
		}
	} 
}

static void
extract_playlist (const gchar *filename,
		  GHashTable  *metadata)
{
	TotemPlParser       *pl;
	TotemPlParserResult  result;
	PlaylistMetadata     data = {0, 0};
	gchar               *proper_filename;

	pl = totem_pl_parser_new ();

        g_object_set (pl, "recurse", FALSE, "disable-unsafe", TRUE, NULL);

        g_signal_connect (G_OBJECT (pl), "entry-parsed", 
                          G_CALLBACK (entry_parsed), &data);

	if (g_str_has_prefix (filename, "file://")) {
		proper_filename = g_strdup (filename);
	} else {
		proper_filename = g_strconcat ("file://", filename, NULL);
	}

        result = totem_pl_parser_parse (pl, proper_filename, FALSE);

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

	g_hash_table_insert (metadata, 
			     g_strdup (PLAYLIST_PROPERTY_DURATION), 
			     tracker_escape_metadata_printf ("%" G_GINT64_FORMAT, data.total_time));
	g_hash_table_insert (metadata, 
			     g_strdup (PLAYLIST_PROPERTY_NO_TRACKS), 
			     tracker_escape_metadata_printf ("%" G_GUINT32_FORMAT, data.track_counter));
	g_hash_table_insert (metadata,
			     g_strdup (PLAYLIST_PROPERTY_CALCULATED),
			     g_strdup (data.total_time == 0 ? "0" : "1"));
	g_free (proper_filename);
        g_object_unref (pl);
}

TrackerExtractData *
tracker_get_extract_data (void)
{
	return data;
}
