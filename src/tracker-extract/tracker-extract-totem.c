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
#include <libtracker-common/tracker-statement-list.h>
#include <libtracker-common/tracker-ontology.h>

#include "tracker-main.h"

#define NMM_PREFIX TRACKER_NMM_PREFIX
#define NFO_PREFIX TRACKER_NFO_PREFIX
#define NIE_PREFIX TRACKER_NIE_PREFIX
#define NCO_PREFIX TRACKER_NCO_PREFIX
#define DC_PREFIX TRACKER_DC_PREFIX

#define RDF_PREFIX TRACKER_RDF_PREFIX
#define RDF_TYPE RDF_PREFIX "type"

static const gchar *tags[][2] = {
	{ "TOTEM_INFO_VIDEO_HEIGHT",		NFO_PREFIX "height"	},
	{ "TOTEM_INFO_VIDEO_WIDTH",		NFO_PREFIX "width"	},
	{ "TOTEM_INFO_FPS",			NFO_PREFIX "frameRate"	},
	{ "TOTEM_INFO_VIDEO_CODEC",		NFO_PREFIX "codec"	},
	{ "TOTEM_INFO_VIDEO_BITRATE",		NFO_PREFIX "averageBitrate"	},
	{ "TOTEM_INFO_TITLE",			NIE_PREFIX "title"	},
	{ "TOTEM_INFO_AUTHOR",			NCO_PREFIX "creator"	},
	{ "TOTEM_INFO_AUDIO_BITRATE",		NMM_PREFIX "averageBitrate"	},
	{ "TOTEM_INFO_AUDIO_SAMPLE_RATE",	NFO_PREFIX "sampleRate"		},
	{ "TOTEM_INFO_AUDIO_CODEC",		NFO_PREFIX "codec"		},
	{ "TOTEM_INFO_AUDIO_CHANNELS",		NFO_PREFIX "channels"		},
	{ NULL,					NULL			}
};

static void extract_totem (const gchar *uri,
			   GPtrArray   *metadata);

static TrackerExtractData data[] = {
	{ "audio/*", extract_totem },
	{ "video/*", extract_totem },
	{ NULL, NULL }
};

static void
extract_totem (const gchar *uri,
	       GPtrArray   *metadata)
{
	gchar *argv[3];
	gchar *totem;
	gboolean has_video = FALSE;

	argv[0] = g_strdup ("totem-video-indexer");
	argv[1] = g_filename_from_uri (uri, NULL, NULL);
	argv[2] = NULL;

	if (tracker_spawn (argv, 10, &totem, NULL)) {
		gchar **lines, **line;

		lines = g_strsplit (totem, "\n", -1);

		for (line = lines; *line; ++line) {
			gint i;

			for (i = 0; tags[i][0]; i++) {
				if (g_strcmp0 (*line, "TOTEM_INFO_HAS_VIDEO=True")) {
					has_video = TRUE;
				}

				if (g_str_has_prefix (*line, tags[i][0])) {
					gchar *value = (*line) + strlen (tags[i][0]) + 1;

					if (g_strcmp0 (tags[i][0], "TOTEM_INFO_AUTHOR")) {
						gchar *canonical_uri = tracker_uri_printf_escaped ("urn:artist:%s", value);
						tracker_statement_list_insert (metadata, canonical_uri, RDF_TYPE, NCO_PREFIX "Contact");
						tracker_statement_list_insert (metadata, canonical_uri, NCO_PREFIX "fullname", value);
						tracker_statement_list_insert (metadata, uri, tags[i][1], canonical_uri);
						g_free (canonical_uri);
					} else {
						tracker_statement_list_insert (metadata, uri,
									  tags[i][1],
									  value);
					}
					break;
				}
			}
		}

		if (has_video) {
			tracker_statement_list_insert (metadata, uri, 
			                          RDF_TYPE, 
			                          NMM_PREFIX "Video");
		} else {
			tracker_statement_list_insert (metadata, uri, 
			                          RDF_TYPE, 
			                          NMM_PREFIX "MusicPiece");
		}
	}
}

TrackerExtractData *
tracker_get_extract_data (void)
{
	return data;
}
