/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Edward Duffy (eduffy@gmail.com)
 * Copyright (C) 2006, Laurent Aguerreche (laurent.aguerreche@free.fr)
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

#include <string.h>

#include <glib.h>

#include <libtracker-common/tracker-ontology.h>
#include <libtracker-common/tracker-os-dependant.h>
#include <libtracker-common/tracker-statement-list.h>

#include "tracker-main.h"

#define NMM_PREFIX TRACKER_NMM_PREFIX
#define NFO_PREFIX TRACKER_NFO_PREFIX
#define NIE_PREFIX TRACKER_NIE_PREFIX
#define DC_PREFIX TRACKER_DC_PREFIX
#define NCO_PREFIX TRACKER_NCO_PREFIX

#define RDF_PREFIX TRACKER_RDF_PREFIX
#define RDF_TYPE RDF_PREFIX "type"

static void extract_mplayer (const gchar *uri,
			     GPtrArray   *metadata);

static TrackerExtractData extract_data[] = {
	{ "audio/*", extract_mplayer },
	{ "video/*", extract_mplayer },
	{ NULL, NULL }
};

static const gchar *video_tags[][2] = {
	{ "ID_VIDEO_HEIGHT",	NFO_PREFIX "height"	},
	{ "ID_VIDEO_WIDTH",	NFO_PREFIX "width"	},
	{ "ID_VIDEO_FPS",	NFO_PREFIX "frameRate"	},
	{ "ID_VIDEO_CODEC",	NFO_PREFIX "codec"	},
	{ "ID_VIDEO_BITRATE",	NFO_PREFIX "averageBitrate" },
	{ NULL,			NULL			}
};

static const gchar *audio_tags[][2] = {
	{ "ID_AUDIO_BITRATE",	NFO_PREFIX "averageBitrate" },
	{ "ID_AUDIO_RATE",	NFO_PREFIX "sampleRate"	},
	{ "ID_AUDIO_CODEC",	NFO_PREFIX "codec"	},
	{ "ID_AUDIO_NCH",	NFO_PREFIX "channels"	},
	{ NULL,			NULL			}
};

/* Some of "info_tags" tags can belong to Audio or/and video or none, so 3 cases :
 * 1/ tag does not belong to audio nor video, it is a general tag ;
 * 2/ tag only belongs to audio ;
 * 3/ tag can belong to audio and video. If current media has video we will associate
 *    tag to Video, otherwise to Audio if it has audio.
 */
static const gchar *info_tags[][3] = {
	{ "Comment",		NIE_PREFIX "comment",	NIE_PREFIX "comment"	},
	{ "Title",		NIE_PREFIX "title",	NIE_PREFIX "title"	},
	{ "Genre",		NFO_PREFIX "genre",	NFO_PREFIX "genre"	},
	{ "Track",		NMM_PREFIX "trackNumber", NMM_PREFIX "trackNumber" },
	{ "Artist",		NMM_PREFIX "performer",	NMM_PREFIX "performer"	},
	{ "Album",		NIE_PREFIX "title",	NIE_PREFIX "title"	},
	{ "Year",		NIE_PREFIX "contentCreated", NIE_PREFIX "contentCreated" },
	{ "copyright",		NIE_PREFIX "copyright", NIE_PREFIX "copyright"	},
	{ NULL,			NULL,			NULL		}
};

typedef struct {
	GPtrArray *metadata;
	const gchar *uri;
} ForeachCopyInfo;

static void
copy_hash_table_entry (gpointer key,
		       gpointer value,
		       gpointer user_data)
{
	ForeachCopyInfo *info = user_data;

	if (g_strcmp0 (key, NMM_PREFIX "performer") == 0) {
		gchar *canonical_uri = tracker_uri_printf_escaped ("urn:artist:%s", value);
		tracker_statement_list_insert (info->metadata, canonical_uri, RDF_TYPE, NCO_PREFIX "Contact");
		tracker_statement_list_insert (info->metadata, canonical_uri, NCO_PREFIX "fullname", value);
		tracker_statement_list_insert (info->metadata, info->uri, key, canonical_uri);
		g_free (canonical_uri);
	} else {
		tracker_statement_list_insert (info->metadata, info->uri, key, value);
	}
}

static void
extract_mplayer (const gchar *uri,
		 GPtrArray  *metadata)
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
		gchar	      *duration;
		gchar	     **lines, **line;

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
						tracker_statement_list_insert (metadata, uri,
								     audio_tags[i][1],
								     (*line) + strlen (audio_tags[i][0]) + 1);
						break;
					}
				}
			}

			else if (g_pattern_match_string (pattern_ID_VIDEO, *line)) {
				gint i;

				for (i = 0; video_tags[i][0]; i++) {
					if (g_str_has_prefix (*line, video_tags[i][0])) {
						tracker_statement_list_insert (metadata, uri,
								     video_tags[i][1],
								     (*line) + strlen (video_tags[i][0]) + 1);
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

								data = g_strdup (equal_char_pos + 1);

								if (data) {
									if (data[0] != '\0') {
										g_hash_table_insert (tmp_metadata_audio,
												     g_strdup (info_tags[i][1]),
												     data);

										if (info_tags[i][2]) {
											g_hash_table_insert (tmp_metadata_video,
													     g_strdup (info_tags[i][2]),
													     g_strdup (data));
										}
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

			tracker_statement_list_insert (metadata, uri, 
			                          RDF_TYPE, 
			                          NMM_PREFIX "Video");

			if (tmp_metadata_video) {
				ForeachCopyInfo info = { metadata, uri };
				g_hash_table_foreach (tmp_metadata_video, 
						      copy_hash_table_entry, 
						      &info);
				g_hash_table_destroy (tmp_metadata_video);
			}

			if (duration) {
				tracker_statement_list_insert (metadata, uri, NFO_PREFIX "duration", duration);
				g_free (duration);
			}
		} else if (has_audio) {

			tracker_statement_list_insert (metadata, uri, 
			                          RDF_TYPE, 
			                          NMM_PREFIX "MusicPiece");

			if (tmp_metadata_video) {
				ForeachCopyInfo info = { metadata, uri };
				g_hash_table_foreach (tmp_metadata_audio, 
						      copy_hash_table_entry, 
						      &info);
				g_hash_table_destroy (tmp_metadata_audio);
			}

			if (duration) {
				tracker_statement_list_insert (metadata, uri, NFO_PREFIX "duration", duration);
				g_free (duration);
			}
		} else {
			tracker_statement_list_insert (metadata, uri, 
			                          RDF_TYPE, 
			                          NFO_PREFIX "FileDataObject");
		}

	}
}

TrackerExtractData *
tracker_get_extract_data (void)
{
	return extract_data;
}
