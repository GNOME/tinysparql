/*
 * Copyright (C) 2006, Edward Duffy <eduffy@gmail.com>
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

#include <libtracker-common/tracker-common.h>

#include <libtracker-extract/tracker-extract.h>

static void extract_totem (const gchar          *uri,
                           TrackerSparqlBuilder *preupdate,
                           TrackerSparqlBuilder *metadata);

static const gchar *tags[][2] = {
	{ "TOTEM_INFO_VIDEO_HEIGHT",      "nfo:height"         },
	{ "TOTEM_INFO_VIDEO_WIDTH",       "nfo:width"          },
	{ "TOTEM_INFO_FPS",               "nfo:frameRate"      },
	{ "TOTEM_INFO_VIDEO_CODEC",       "nfo:codec"          },
	{ "TOTEM_INFO_VIDEO_BITRATE",     "nfo:averageBitrate" },
	{ "TOTEM_INFO_TITLE",             "nie:title"          },
	{ "TOTEM_INFO_ARTIST",            "nco:creator"        },
	{ "TOTEM_INFO_ALBUM",             "nmm:musicAlbum"     },
	{ "TOTEM_INFO_AUDIO_BITRATE",     "nfo:averageBitrate" },
	{ "TOTEM_INFO_AUDIO_SAMPLE_RATE", "nfo:sampleRate"     },
	{ "TOTEM_INFO_AUDIO_CODEC",       "nfo:codec"          },
	{ "TOTEM_INFO_AUDIO_CHANNELS",    "nfo:channels"       },
	{ NULL, NULL }
};

static TrackerExtractData data[] = {
	{ "audio/*", extract_totem },
	{ "video/*", extract_totem },
	{ NULL, NULL }
};

static void
metadata_write_foreach (gpointer key,
                        gpointer value,
                        gpointer user_data)
{
	TrackerSparqlBuilder *metadata = user_data;

	tracker_sparql_builder_predicate (metadata, (const gchar *) key);
	tracker_sparql_builder_object_unvalidated (metadata, (const gchar *) value);
}

static void
extract_totem (const gchar          *uri,
               TrackerSparqlBuilder *preupdate,
               TrackerSparqlBuilder *metadata)
{
	gchar *argv[3];
	gchar *totem;
	gboolean has_video = FALSE;
	GHashTable *tmp_metadata;

	argv[0] = g_strdup ("totem-video-indexer");
	argv[1] = g_filename_from_uri (uri, NULL, NULL);
	argv[2] = NULL;

	tmp_metadata = g_hash_table_new_full (g_str_hash,
	                                      g_str_equal,
	                                      (GDestroyNotify) g_free,
	                                      (GDestroyNotify) g_free);

	if (tracker_spawn (argv, 10, &totem, NULL)) {
		gchar **lines, **line;
		gchar *artist = NULL, *album = NULL;
		gchar *artist_uri = NULL, *album_uri = NULL;

		lines = g_strsplit (totem, "\n", -1);

		for (line = lines; *line; ++line) {
			gint i;

			for (i = 0; tags[i][0]; i++) {
				if (g_strcmp0 (*line, "TOTEM_INFO_HAS_VIDEO=True") == 0) {
					has_video = TRUE;
				}

				if (g_str_has_prefix (*line, tags[i][0])) {
					gchar *value = (*line) + strlen (tags[i][0]) + 1;

					if (g_strcmp0 (tags[i][0], "TOTEM_INFO_ARTIST") == 0) {
						artist = g_strdup (value);
						artist_uri = tracker_sparql_escape_uri_printf ("urn:artist:%s", artist);
					} else if (g_strcmp0 (tags[i][0], "TOTEM_INFO_ALBUM") == 0) {
						album = g_strdup (value);
						album_uri = tracker_sparql_escape_uri_printf ("urn:album:%s", album);
					} else {
						g_hash_table_insert (tmp_metadata, g_strdup (tags[i][1]), g_strdup (value));
					}
					break;
				}
			}
		}

		if (artist) {
			tracker_sparql_builder_insert_open (preupdate, NULL);

			tracker_sparql_builder_subject_iri (preupdate, artist_uri);
			tracker_sparql_builder_predicate (preupdate, "a");
			tracker_sparql_builder_object (preupdate, "nmm:Artist");

			if (has_video) {
				tracker_sparql_builder_object (preupdate, "nmm:director");
			} else {
				tracker_sparql_builder_object (preupdate, "nmm:composer");
			}

			tracker_sparql_builder_predicate (preupdate, "nmm:artistName");
			tracker_sparql_builder_object_unvalidated (preupdate, artist);

			tracker_sparql_builder_insert_close (preupdate);
		}

		if (album) {
			tracker_sparql_builder_insert_open (preupdate, NULL);

			tracker_sparql_builder_subject_iri (preupdate, album_uri);
			tracker_sparql_builder_predicate (preupdate, "a");
			tracker_sparql_builder_object (preupdate, "nmm:MusicAlbum");
			tracker_sparql_builder_predicate (preupdate, "nmm:albumTitle");
			tracker_sparql_builder_object_unvalidated (preupdate, album);

			tracker_sparql_builder_insert_close (preupdate);
		}

		tracker_sparql_builder_predicate (metadata, "a");

		if (has_video) {
			tracker_sparql_builder_object (metadata, "nmm:Video");
		} else {
			tracker_sparql_builder_object (metadata, "nmm:MusicPiece");
			tracker_sparql_builder_object (metadata, "nfo:Audio");
		}

		g_hash_table_foreach (tmp_metadata, metadata_write_foreach, metadata);

		if (artist) {
			if (has_video) {
				tracker_sparql_builder_object (metadata, "nmm:leadActor");
			} else {
				tracker_sparql_builder_predicate (metadata, "nmm:performer");
			}

			tracker_sparql_builder_object_iri (metadata, artist_uri);
		}

		if (album && !has_video) {
			tracker_sparql_builder_predicate (metadata, "nmm:musicAlbum");
			tracker_sparql_builder_object_iri (metadata, album_uri);
		}

		g_free (album_uri);
		g_free (artist_uri);
		g_free (album);
		g_free (artist);
	}

	g_hash_table_destroy (tmp_metadata);
}

TrackerExtractData *
tracker_extract_get_data (void)
{
	return data;
}
