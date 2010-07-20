/*
 * Copyright (C) 2006, Edward Duffy <eduffy@gmail.com>
 * Copyright (C) 2006, Laurent Aguerreche <laurent.aguerreche@free.fr>
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

#include <libtracker-common/tracker-ontologies.h>
#include <libtracker-common/tracker-os-dependant.h>
#include <libtracker-common/tracker-utils.h>

#include <libtracker-extract/tracker-extract.h>

static void extract_mplayer (const gchar          *uri,
                             TrackerSparqlBuilder *preupdate,
                             TrackerSparqlBuilder *metadata);

static TrackerExtractData extract_data[] = {
	{ "audio/*", extract_mplayer },
	{ "video/*", extract_mplayer },
	{ NULL, NULL }
};

static const gchar *video_tags[][2] = {
	{ "ID_VIDEO_HEIGHT",    "nfo:height"         },
	{ "ID_VIDEO_WIDTH",     "nfo:width"          },
	{ "ID_VIDEO_FPS",       "nfo:frameRate"      },
	{ "ID_VIDEO_CODEC",     "nfo:codec"          },
	{ "ID_VIDEO_BITRATE",   "nfo:averageBitrate" },
	{ NULL,                         NULL                 }
};

static const gchar *audio_tags[][2] = {
	{ "ID_AUDIO_BITRATE",   "nfo:averageBitrate" },
	{ "ID_AUDIO_RATE",      "nfo:sampleRate"     },
	{ "ID_AUDIO_CODEC",     "nfo:codec"          },
	{ "ID_AUDIO_NCH",       "nfo:channels"       },
	{ NULL,                         NULL                 }
};

/* Some of "info_tags" tags can belong to Audio or/and video or none, so 3 cases :
 * 1/ tag does not belong to audio nor video, it is a general tag ;
 * 2/ tag only belongs to audio ;
 * 3/ tag can belong to audio and video. If current media has video we will associate
 *    tag to Video, otherwise to Audio if it has audio.
 */
static const gchar *info_tags[][2] = {
	{ "Comment",            "nie:comment"        },
	{ "Title",              "nie:title"          },
	{ "Genre",              "nfo:genre"          },
	{ "Track",              "nmm:trackNumber"    },
	{ "Artist",             "nmm:performer"      },
	{ "Album",              "nie:title"          },
	{ "Year",               "nie:contentCreated" },
	{ "copyright",          "nie:copyright"      },
	{ NULL,                 NULL,                }
};

typedef struct {
	TrackerSparqlBuilder *preupdate;
	TrackerSparqlBuilder *metadata;
	const gchar *uri;
} ForeachCopyInfo;

static void
copy_hash_table_entry (gpointer key,
                       gpointer value,
                       gpointer user_data)
{
	ForeachCopyInfo *info = user_data;

	if (g_strcmp0 (key, "nmm:performer") == 0) {
		gchar *canonical_uri = tracker_sparql_escape_uri_printf ("urn:artist:%s", value);

		tracker_sparql_builder_insert_open (info->preupdate, NULL);

		tracker_sparql_builder_subject_iri (info->preupdate, canonical_uri);
		tracker_sparql_builder_predicate (info->preupdate, "a");
		tracker_sparql_builder_object (info->preupdate, "nmm:Artist");

		tracker_sparql_builder_predicate (info->preupdate, "nmm:artistName");
		tracker_sparql_builder_object_unvalidated (info->preupdate, value);

		tracker_sparql_builder_insert_close (info->preupdate);

		g_free (canonical_uri);
	} else {
		tracker_sparql_builder_predicate (info->metadata, key);
		tracker_sparql_builder_object_unvalidated (info->metadata, value);
	}
}

static void
extract_mplayer (const gchar          *uri,
                 TrackerSparqlBuilder *preupdate,
                 TrackerSparqlBuilder *metadata)
{
	gchar *argv[10];
	gchar *mplayer;

	argv[0] = g_strdup ("mplayer");
	argv[1] = g_strdup ("-identify");
	argv[2] = g_strdup ("-frames");
	argv[3] = g_strdup ("0");
	argv[4] = g_strdup ("-vo");
	argv[5] = g_strdup ("null");
	argv[6] = g_strdup ("-ao");
	argv[7] = g_strdup ("null");
	argv[8] = g_filename_from_uri (uri, NULL, NULL);
	argv[9] = NULL;

	if (tracker_spawn (argv, 10, &mplayer, NULL)) {
		GPatternSpec  *pattern_ID_AUDIO_ID;
		GPatternSpec  *pattern_ID_VIDEO_ID;
		GPatternSpec  *pattern_ID_AUDIO;
		GPatternSpec  *pattern_ID_VIDEO;
		GPatternSpec  *pattern_ID_CLIP_INFO_NAME;
		GPatternSpec  *pattern_ID_CLIP_INFO_VALUE;
		GPatternSpec  *pattern_ID_LENGTH;
		GHashTable    *tmp_metadata_audio;
		GHashTable    *tmp_metadata_video;
		gboolean       has_audio, has_video;
		gchar         *duration;
		gchar        **lines, **line;

		pattern_ID_AUDIO_ID = g_pattern_spec_new ("ID_AUDIO_ID=*");
		pattern_ID_VIDEO_ID = g_pattern_spec_new ("ID_VIDEO_ID=*");
		pattern_ID_AUDIO = g_pattern_spec_new ("ID_AUDIO*=*");
		pattern_ID_VIDEO = g_pattern_spec_new ("ID_VIDEO*=*");
		pattern_ID_CLIP_INFO_NAME = g_pattern_spec_new ("ID_CLIP_INFO_NAME*=*");
		pattern_ID_CLIP_INFO_VALUE = g_pattern_spec_new ("ID_CLIP_INFO_VALUE*=*");
		pattern_ID_LENGTH = g_pattern_spec_new ("ID_LENGTH=*");

		tmp_metadata_audio = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
		tmp_metadata_video = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

		has_audio = has_video = FALSE;

		duration = NULL;

		lines = g_strsplit (mplayer, "\n", -1);

		for (line = lines; *line; ++line) {
			if (g_pattern_match_string (pattern_ID_AUDIO_ID, *line)) {
				has_audio = TRUE;
			}

			else if (g_pattern_match_string (pattern_ID_VIDEO_ID, *line)) {
				has_video = TRUE;
			}

			else if (g_pattern_match_string (pattern_ID_AUDIO, *line)) {
				gint i;

				for (i = 0; audio_tags[i][0]; i++) {
					if (g_str_has_prefix (*line, audio_tags[i][0])) {
						g_hash_table_insert (tmp_metadata_audio,
						                     g_strdup (audio_tags[i][1]),
						                     g_strdup ((*line) + strlen (audio_tags[i][0]) + 1));
						break;
					}
				}
			}

			else if (g_pattern_match_string (pattern_ID_VIDEO, *line)) {
				gint i;

				for (i = 0; video_tags[i][0]; i++) {
					if (g_str_has_prefix (*line, video_tags[i][0])) {
						g_hash_table_insert (tmp_metadata_video,
						                     g_strdup (video_tags[i][1]),
						                     g_strdup ((*line) + strlen (video_tags[i][0]) + 1));
						break;
					}
				}
			}

			else if (g_pattern_match_string (pattern_ID_CLIP_INFO_NAME, *line)) {
				const char *next_line;

				next_line = *(line + 1);

				if (next_line) {
					if (g_pattern_match_string (pattern_ID_CLIP_INFO_VALUE, next_line)) {
						gint i;

						for (i = 0; info_tags[i][0]; i++) {
							if (g_str_has_suffix (*line, info_tags[i][0])) {
								gchar *equal_char_pos, *data;

								equal_char_pos = strchr (next_line, '=');

								if (g_strcmp0 (info_tags[i][0], "Year") == 0)
									data = tracker_extract_guess_date (equal_char_pos + 1);
								else
									data = g_strdup (equal_char_pos + 1);

								if (data) {
									if (data[0] != '\0') {
										g_hash_table_insert (tmp_metadata_audio,
										                     g_strdup (info_tags[i][1]),
										                     data);
									} else {
										g_free (data);
									}
								}

								break;
							}
						}

						line++;
					}
				}
			}

			else if (g_pattern_match_string (pattern_ID_LENGTH, *line)) {
				gchar *equal_char_pos;

				equal_char_pos = strchr (*line, '=');

				duration = g_strdup (equal_char_pos + 1);
			}
		}

		g_pattern_spec_free (pattern_ID_AUDIO_ID);
		g_pattern_spec_free (pattern_ID_VIDEO_ID);
		g_pattern_spec_free (pattern_ID_AUDIO);
		g_pattern_spec_free (pattern_ID_VIDEO);
		g_pattern_spec_free (pattern_ID_CLIP_INFO_NAME);
		g_pattern_spec_free (pattern_ID_CLIP_INFO_VALUE);
		g_pattern_spec_free (pattern_ID_LENGTH);

		if (has_video) {
			tracker_sparql_builder_predicate (metadata, "a");
			tracker_sparql_builder_object (metadata, "nmm:Video");

			if (tmp_metadata_video) {
				ForeachCopyInfo info = { preupdate, metadata, uri };
				g_hash_table_foreach (tmp_metadata_video,
				                      copy_hash_table_entry,
				                      &info);
				g_hash_table_unref (tmp_metadata_video);
				tmp_metadata_video = NULL;
			}

			if (duration) {
				tracker_sparql_builder_predicate (metadata, "nfo:duration");
				tracker_sparql_builder_object_unvalidated (metadata, duration);
				g_free (duration);
			}
		} else if (has_audio) {
			tracker_sparql_builder_predicate (metadata, "a");
			tracker_sparql_builder_object (metadata, "nmm:MusicPiece");
			tracker_sparql_builder_object (metadata, "nfo:Audio");

			if (tmp_metadata_audio) {
				ForeachCopyInfo info = { preupdate, metadata, uri };
				g_hash_table_foreach (tmp_metadata_audio,
				                      copy_hash_table_entry,
				                      &info);
				g_hash_table_unref (tmp_metadata_audio);
				tmp_metadata_audio = NULL;
			}

			if (duration) {
				tracker_sparql_builder_predicate (metadata, "nfo:duration");
				tracker_sparql_builder_object_unvalidated (metadata, duration);
				g_free (duration);
			}
		} else {
			tracker_sparql_builder_predicate (metadata, "a");
			tracker_sparql_builder_object (metadata, "nfo:FileDataObject");
		}

	}
}

TrackerExtractData *
tracker_extract_get_data (void)
{
	return extract_data;
}
