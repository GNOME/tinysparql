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

#include "config.h"

#include <xine.h>
#include <glib.h>

#include <libtracker-common/tracker-statement-list.h>
#include <libtracker-common/tracker-ontology.h>
#include <libtracker-common/tracker-utils.h>

#include "tracker-main.h"

#define NMM_PREFIX TRACKER_NMM_PREFIX
#define NFO_PREFIX TRACKER_NFO_PREFIX
#define NIE_PREFIX TRACKER_NIE_PREFIX
#define DC_PREFIX TRACKER_DC_PREFIX
#define NCO_PREFIX TRACKER_NCO_PREFIX

#define RDF_PREFIX TRACKER_RDF_PREFIX
#define RDF_TYPE RDF_PREFIX "type"

/* Take an absolute path to a file and fill a hashtable with metadata.
 */
void
tracker_extract_xine (gchar *uri, GPtrArray *metadata)
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

	mrl = g_filename_from_uri (uri, NULL, NULL);

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
			guint32 duration;

			duration = (guint32) length_time / 1000; /* from miliseconds to seconds */

			if (has_video) {
				tracker_statement_list_insert_with_int (metadata, uri, NFO_PREFIX "duration", duration);
			} else if (has_audio) {
				tracker_statement_list_insert_with_int (metadata, uri, NFO_PREFIX "duration", duration);
			}
		}
	}


	/* Video */

	if (has_video) {
		guint32 n, n0;
		const char *video_codec;

		tracker_statement_list_insert (metadata, uri, 
		                          RDF_TYPE, 
		                          NMM_PREFIX "Video");

		n  = xine_get_stream_info (stream, XINE_STREAM_INFO_VIDEO_HEIGHT);
		n0 = xine_get_stream_info (stream, XINE_STREAM_INFO_VIDEO_WIDTH);
		if (n > 0 && n0 > 0) {
			tracker_statement_list_insert_with_int (metadata, uri, NFO_PREFIX "height", n);
			tracker_statement_list_insert_with_int (metadata, uri, NFO_PREFIX "width", n0);
		}

		n = xine_get_stream_info (stream, XINE_STREAM_INFO_FRAME_DURATION);
		if (n > 0) {
			/* 90000 because it is how is done in Xine! */
			tracker_statement_list_insert_with_int (metadata, uri, NFO_PREFIX "frameRate", 90000 / n);
		}

		n = xine_get_stream_info (stream, XINE_STREAM_INFO_VIDEO_BITRATE);
		if (n > 0) {
			tracker_statement_list_insert_with_int (metadata, uri, NFO_PREFIX "averageBitrate", n);
		}

		video_codec = xine_get_meta_info (stream, XINE_META_INFO_VIDEOCODEC);
		if (video_codec) {
			tracker_statement_list_insert (metadata, uri, NFO_PREFIX "codec", video_codec);
		}
	} else if (has_audio) {
		tracker_statement_list_insert (metadata, uri, 
		                          RDF_TYPE, 
		                          NMM_PREFIX "MusicPiece");
	} else {
		tracker_statement_list_insert (metadata, uri, 
		                          RDF_TYPE, 
		                          NFO_PREFIX "FileDataObject");
	}


	/* Audio */

	if (has_audio) {
		guint32   n;
		const char *audio_codec;

		n = xine_get_stream_info (stream, XINE_STREAM_INFO_AUDIO_BITRATE);
		if (n > 0) {
			tracker_statement_list_insert_with_int (metadata, uri, NFO_PREFIX "averageBitrate", n);
		}

		n = xine_get_stream_info (stream, XINE_STREAM_INFO_AUDIO_SAMPLERATE);
		if (n > 0) {
			tracker_statement_list_insert_with_int (metadata, uri, NFO_PREFIX "samplerate", n);
		}

		n = xine_get_stream_info (stream, XINE_STREAM_INFO_AUDIO_CHANNELS);
		if (n > 0) {
			tracker_statement_list_insert_with_int (metadata, uri, NFO_PREFIX "channels", n);
		}

		audio_codec = xine_get_meta_info (stream, XINE_META_INFO_AUDIOCODEC);
		if (audio_codec) {
			tracker_statement_list_insert (metadata, uri, NFO_PREFIX "codec", audio_codec);
		}
	}


	/* Tags */

	comment = xine_get_meta_info (stream, XINE_META_INFO_COMMENT);
	if (comment) {
		if (has_video) {
			tracker_statement_list_insert (metadata, uri, NIE_PREFIX "comment", comment);
		} else if (has_audio) {
			tracker_statement_list_insert (metadata, uri, NIE_PREFIX "comment", comment);
		}
	}

	title = xine_get_meta_info (stream, XINE_META_INFO_TITLE);
	if (title) {
		if (has_video) {
			tracker_statement_list_insert (metadata, uri, NIE_PREFIX "title", title);
		} else if (has_audio) {
			tracker_statement_list_insert (metadata, uri, NIE_PREFIX "title", title);
		}
	}

	author = xine_get_meta_info (stream, XINE_META_INFO_ARTIST);
	if (author) {
		gchar *canonical_uri = tracker_uri_printf_escaped ("urn:artist:%s", author);
		tracker_statement_list_insert (metadata, canonical_uri, RDF_TYPE, NCO_PREFIX "Contact");
		tracker_statement_list_insert (metadata, canonical_uri, NCO_PREFIX "fullname", author);
		tracker_statement_list_insert (metadata, uri, NCO_PREFIX "creator", canonical_uri);
		g_free (canonical_uri);
	}

	album = xine_get_meta_info (stream, XINE_META_INFO_ALBUM);
	if (album) {
		tracker_statement_list_insert (metadata, uri, NIE_PREFIX "title", album);
	}

	year = xine_get_meta_info (stream, XINE_META_INFO_YEAR);
	if (year) {
	/*	tracker_statement_list_insert (metadata, uri, "Audio:Year", year); */
	}

	genre = xine_get_meta_info (stream, XINE_META_INFO_GENRE);
	if (genre) {
		tracker_statement_list_insert (metadata, uri, NFO_PREFIX "genre" , genre);
	}

	track = xine_get_meta_info (stream, XINE_META_INFO_TRACK_NUMBER);
	if (track) {
		tracker_statement_list_insert (metadata, uri, NMM_PREFIX "trackNumber", track);
	}

	/* FIXME: "Video.Copyright" seems missing */
	/* tracker_statement_list_insert (metadata, uri, "Video.Copyright", NULL); */


	xine_dispose (stream);

	xine_close_audio_driver (xine_base, audio_port);
	xine_close_video_driver (xine_base, video_port);

	xine_exit (xine_base);
}


TrackerExtractData data[] = {
	{ "audio/*", tracker_extract_xine },
	{ "video/*", tracker_extract_xine },
	{ NULL, NULL }
};


TrackerExtractData *
tracker_get_extract_data (void)
{
	return data;
}
