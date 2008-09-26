/* Tracker - audio/video metadata extraction based on Xine
 * Copyright (C) 2006, Laurent Aguerreche (laurent.aguerreche@free.fr)
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

#include <xine.h>
#include <glib.h>

#include "tracker-extract.h"


static void
add_uint32_info (GHashTable *metadata, char *key, uint32_t info)
{
	char *str_info;

	str_info = g_strdup_printf ("%d", info);
	g_hash_table_insert (metadata, key, str_info);
}


/* Take an absolute path to a file and fill a hashtable with metadata.
 */
void
tracker_extract_xine (gchar *uri, GHashTable *metadata)
{
	xine_t		  *xine_base;
	xine_audio_port_t *audio_port;
	xine_video_port_t *video_port;
	xine_stream_t	  *stream;
	char		  *mrl;

	gboolean	  has_audio;
	gboolean	  has_video;

	int		  pos_stream;
	int		  pos_time;
	int		  length_time;

	const char	  *comment;
	const char	  *title;
	const char	  *author;
	const char	  *album;
	const char	  *year;
	const char	  *genre;
	const char	  *track;

	g_return_if_fail (uri && metadata);


	xine_base = xine_new ();

	if (!xine_base) {
		return;
	}

	xine_init (xine_base);

	audio_port = xine_open_audio_driver (xine_base, NULL, NULL);
	video_port = xine_open_video_driver (xine_base, NULL, XINE_VISUAL_TYPE_NONE, NULL);

	if (!audio_port || !video_port) {
		xine_exit (xine_base);
		return;
	}

	stream = xine_stream_new (xine_base, audio_port, video_port);

	if (!stream) {
		xine_close_audio_driver (xine_base, audio_port);
		xine_close_video_driver (xine_base, video_port);
		xine_exit (xine_base);
		return;
	}

	mrl = g_strconcat ("file://", uri, NULL);

	if (!xine_open (stream, mrl)) {
		g_free (mrl);
		xine_dispose (stream);
		xine_close_audio_driver (xine_base, audio_port);
		xine_close_video_driver (xine_base, video_port);
		xine_exit (xine_base);
		return;
	}

	g_free (mrl);


	has_audio = xine_get_stream_info (stream, XINE_STREAM_INFO_HAS_AUDIO);
	has_video = xine_get_stream_info (stream, XINE_STREAM_INFO_HAS_VIDEO);


	if (xine_get_pos_length (stream, &pos_stream, &pos_time, &length_time)) {
		if (length_time >= 0) {
			uint32_t duration;

			duration = (uint32_t) length_time / 1000; /* from miliseconds to seconds */

			if (has_video) {
				add_uint32_info (metadata, g_strdup ("Video:Duration"), duration);
			} else if (has_audio) {
				add_uint32_info (metadata, g_strdup ("Audio:Duration"), duration);
			}
		}
	}


	/* Video */

	if (has_video) {
		uint32_t   n, n0;
		const char *video_codec;

		n  = xine_get_stream_info (stream, XINE_STREAM_INFO_VIDEO_HEIGHT);
		n0 = xine_get_stream_info (stream, XINE_STREAM_INFO_VIDEO_WIDTH);
		if (n > 0 && n0 > 0) {
			add_uint32_info (metadata, g_strdup ("Video:Height"), n);
			add_uint32_info (metadata, g_strdup ("Video:Width"), n0);
		}

		n = xine_get_stream_info (stream, XINE_STREAM_INFO_FRAME_DURATION);
		if (n > 0) {
			/* 90000 because it is how is done in Xine! */
			add_uint32_info (metadata, g_strdup ("Video:FrameRate"), 90000 / n);
		}

		n = xine_get_stream_info (stream, XINE_STREAM_INFO_VIDEO_BITRATE);
		if (n > 0) {
			add_uint32_info (metadata, g_strdup ("Video:Bitrate"), n);
		}

		video_codec = xine_get_meta_info (stream, XINE_META_INFO_VIDEOCODEC);
		if (video_codec) {
			g_hash_table_insert (metadata, g_strdup ("Video:Codec"), g_strdup (video_codec));
		}
	}


	/* Audio */

	if (has_audio) {
		uint32_t   n;
		const char *audio_codec;

		n = xine_get_stream_info (stream, XINE_STREAM_INFO_AUDIO_BITRATE);
		if (n > 0) {
			add_uint32_info (metadata, g_strdup ("Audio:Bitrate"), n);
		}

		n = xine_get_stream_info (stream, XINE_STREAM_INFO_AUDIO_SAMPLERATE);
		if (n > 0) {
			add_uint32_info (metadata, g_strdup ("Audio:Samplerate"), n);
		}

		n = xine_get_stream_info (stream, XINE_STREAM_INFO_AUDIO_CHANNELS);
		if (n > 0) {
			add_uint32_info (metadata, g_strdup ("Audio:Channels"), n);
		}

		audio_codec = xine_get_meta_info (stream, XINE_META_INFO_AUDIOCODEC);
		if (audio_codec) {
			g_hash_table_insert (metadata, g_strdup ("Audio:Codec"), g_strdup (audio_codec));
		}
	}


	/* Tags */

	comment = xine_get_meta_info (stream, XINE_META_INFO_COMMENT);
	if (comment) {
		if (has_video) {
			g_hash_table_insert (metadata, g_strdup ("Video:Comment"), g_strdup (comment));
		} else if (has_audio) {
			g_hash_table_insert (metadata, g_strdup ("Audio:Comment"), g_strdup (comment));
		}
	}

	title = xine_get_meta_info (stream, XINE_META_INFO_TITLE);
	if (title) {
		if (has_video) {
			g_hash_table_insert (metadata, g_strdup ("Video:Title"), g_strdup (title));
		} else if (has_audio) {
			g_hash_table_insert (metadata, g_strdup ("Audio:Title"), g_strdup (title));
		}
	}

	author = xine_get_meta_info (stream, XINE_META_INFO_ARTIST);
	if (author) {
		if (has_video) {
			g_hash_table_insert (metadata, g_strdup ("Video:Author"), g_strdup (author));
		} else if (has_audio) {
			g_hash_table_insert (metadata, g_strdup ("Audio:Author"), g_strdup (author));
		}
	}

	album = xine_get_meta_info (stream, XINE_META_INFO_ALBUM);
	if (album) {
		g_hash_table_insert (metadata, g_strdup ("Audio:Album"), g_strdup (album));
	}

	year = xine_get_meta_info (stream, XINE_META_INFO_YEAR);
	if (year) {
		g_hash_table_insert (metadata, g_strdup ("Audio:Year"), g_strdup (year));
	}

	genre = xine_get_meta_info (stream, XINE_META_INFO_GENRE);
	if (genre) {
		g_hash_table_insert (metadata, g_strdup ("Audio:Genre"), g_strdup (genre));
	}

	track = xine_get_meta_info (stream, XINE_META_INFO_TRACK_NUMBER);
	if (track) {
		g_hash_table_insert (metadata, g_strdup ("Audio:Track"), g_strdup (track));
	}

	/* FIXME: "Video.Copyright" seems missing */
	/* g_hash_table_insert (metadata, g_strdup ("Video.Copyright"), NULL); */


	xine_dispose (stream);

	xine_close_audio_driver (xine_base, audio_port);
	xine_close_video_driver (xine_base, video_port);

	xine_exit (xine_base);
}


TrackerExtractorData data[] = {
	{ "audio/*", tracker_extract_xine },
	{ "video/*", tracker_extract_xine },
	{ NULL, NULL }
};


TrackerExtractorData *
tracker_get_extractor_data (void)
{
	return data;
}
