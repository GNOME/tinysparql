/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Edward Duffy (eduffy@gmail.com)
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

#include <string.h>

#include <glib.h>

#include <libtracker-common/tracker-os-dependant.h>

#include "tracker-extract.h"

static gchar *tags[][2] = {
	{ "TOTEM_INFO_VIDEO_HEIGHT",		"Video:Height"		},
	{ "TOTEM_INFO_VIDEO_WIDTH",		"Video:Width"		},
	{ "TOTEM_INFO_FPS",			"Video:FrameRate"	},
	{ "TOTEM_INFO_VIDEO_CODEC",		"Video:Codec"		},
	{ "TOTEM_INFO_VIDEO_BITRATE",		"Video:Bitrate"		},
	{ "TOTEM_INFO_TITLE",			"Video:Title"		},
	{ "TOTEM_INFO_AUTHOR",			"Video:Author"		},
	{ "TOTEM_INFO_AUDIO_BITRATE",		"Audio:Bitrate"		},
	{ "TOTEM_INFO_AUDIO_SAMPLE_RATE",	"Audio:Samplerate"	},
	{ "TOTEM_INFO_AUDIO_CODEC",		"Audio:Codec"		},
	{ "TOTEM_INFO_AUDIO_CHANNELS",		"Audio:Channels"	},
	{ NULL,					NULL			}
};

static void extract_totem (const gchar *filename,
			   GHashTable  *metadata);

static TrackerExtractorData data[] = {
	{ "audio/*", extract_totem },
	{ "video/*", extract_totem },
	{ NULL, NULL }
};

static void
extract_totem (const gchar *filename,
	       GHashTable  *metadata)
{
	gchar *argv[3];
	gchar *totem;

	argv[0] = g_strdup ("totem-video-indexer");
	argv[1] = g_strdup (filename);
	argv[2] = NULL;

	if (tracker_spawn (argv, 10, &totem, NULL)) {
		gchar **lines, **line;

		lines = g_strsplit (totem, "\n", -1);

		for (line = lines; *line; ++line) {
			gint i;

			for (i = 0; tags[i][0]; i++) {
				if (g_str_has_prefix (*line, tags[i][0])) {
					g_hash_table_insert (metadata,
							     g_strdup (tags[i][1]),
							     g_strdup ((*line) + strlen (tags[i][0]) + 1));
					break;
				}
			}
		}
	}
}

TrackerExtractorData *
tracker_get_extractor_data (void)
{
	return data;
}
