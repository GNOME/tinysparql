/* Tracker - audio/video metadata extraction based on Xine
 * Copyright (C) 2006, Laurent Aguerreche <laurent.aguerreche@free.fr>
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

#include <libtracker-common/tracker-ontologies.h>
#include <libtracker-common/tracker-utils.h>

#include <libtracker-extract/tracker-extract.h>

static void
tracker_extract_xine (const gchar          *uri,
                      TrackerSparqlBuilder *preupdate,
		      TrackerSparqlBuilder *metadata)
{
	xine_t            *xine_base;
	xine_audio_port_t *audio_port;
	xine_video_port_t *video_port;
	xine_stream_t     *stream;
	char              *mrl;

	gboolean          has_audio;
	gboolean          has_video;

	int               pos_stream;
	int               pos_time;
	int               length_time;

	const char        *comment;
	const char        *title;
	const char        *author;
	const char        *album;
	gchar             *year;
	const char        *genre;
	const char        *track;

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

	author = xine_get_meta_info (stream, XINE_META_INFO_ARTIST);
	if (author) {
		gchar *canonical_uri = tracker_sparql_escape_uri_printf ("urn:artist:%s", author);

		tracker_sparql_builder_insert_open (preupdate, NULL);

		tracker_sparql_builder_subject_iri (preupdate, canonical_uri);
		tracker_sparql_builder_predicate (preupdate, "a");
		tracker_sparql_builder_object (preupdate, "nmm:Artist");

		tracker_sparql_builder_insert_close (preupdate);

		g_free (canonical_uri);
	}

	album = xine_get_meta_info (stream, XINE_META_INFO_ALBUM);
	if (album) {
		gchar *canonical_uri = tracker_sparql_escape_uri_printf ("urn:album:%s", album);

		tracker_sparql_builder_insert_open (preupdate, NULL);

		tracker_sparql_builder_subject_iri (preupdate, canonical_uri);
		tracker_sparql_builder_predicate (preupdate, "a");
		tracker_sparql_builder_object (preupdate, "nmm:MusicAlbum");
		tracker_sparql_builder_predicate (preupdate, "nmm:albumTitle");
		tracker_sparql_builder_object_unvalidated (preupdate, album);

		tracker_sparql_builder_insert_close (preupdate);

		g_free (canonical_uri);
	}

	tracker_sparql_builder_predicate (metadata, "a");

	if (has_video) {
		tracker_sparql_builder_object (metadata, "nmm:Video");
	} else if (has_audio) {
		tracker_sparql_builder_object (metadata, "nmm:MusicPiece");
		tracker_sparql_builder_object (metadata, "nfo:Audio");
	} else {
		tracker_sparql_builder_object (metadata, "nfo:Media");
	}

	if (xine_get_pos_length (stream, &pos_stream, &pos_time, &length_time)) {
		if (length_time >= 0) {
			guint32 duration;

			duration = (guint32) length_time / 1000; /* from miliseconds to seconds */

			if (has_video || has_audio) {
				tracker_sparql_builder_predicate (metadata, "nfo:duration");
				tracker_sparql_builder_object_int64 (metadata, (gint64) duration);
			}
		}
	}

	if (has_video) {
		guint32 n, n0;
		const char *video_codec;

		n  = xine_get_stream_info (stream, XINE_STREAM_INFO_VIDEO_HEIGHT);
		n0 = xine_get_stream_info (stream, XINE_STREAM_INFO_VIDEO_WIDTH);
		if (n > 0 && n0 > 0) {
			tracker_sparql_builder_predicate (metadata, "nfo:height");
			tracker_sparql_builder_object_int64 (metadata, (gint64) n);

			tracker_sparql_builder_predicate (metadata, "nfo:width");
			tracker_sparql_builder_object_int64 (metadata, (gint64) n0);
		}

		n = xine_get_stream_info (stream, XINE_STREAM_INFO_FRAME_DURATION);
		if (n > 0) {
			/* 90000 because it is how is done in Xine! */
			tracker_sparql_builder_predicate (metadata, "nfo:frameRate");
			tracker_sparql_builder_object_double (metadata, (gdouble) 90000 / n);
		}

		n = xine_get_stream_info (stream, XINE_STREAM_INFO_VIDEO_BITRATE);
		if (n > 0) {
			tracker_sparql_builder_predicate (metadata, "nfo:averageBitrate");
			tracker_sparql_builder_object_int64 (metadata, (gint64) n);
		}

		video_codec = xine_get_meta_info (stream, XINE_META_INFO_VIDEOCODEC);
		if (video_codec) {
			tracker_sparql_builder_predicate (metadata, "nfo:codec");
			tracker_sparql_builder_object_unvalidated (metadata, video_codec);
		}
	}

	if (has_audio) {
		guint32   n;
		const char *audio_codec;

		n = xine_get_stream_info (stream, XINE_STREAM_INFO_AUDIO_BITRATE);
		if (n > 0) {
			tracker_sparql_builder_predicate (metadata, "nfo:averageBitrate");
			tracker_sparql_builder_object_int64 (metadata, (gint64) n);
		}

		n = xine_get_stream_info (stream, XINE_STREAM_INFO_AUDIO_SAMPLERATE);
		if (n > 0) {
			tracker_sparql_builder_predicate (metadata, "nfo:samplerate");
			tracker_sparql_builder_object_int64 (metadata, (gint64) n);
		}

		n = xine_get_stream_info (stream, XINE_STREAM_INFO_AUDIO_CHANNELS);
		if (n > 0) {
			tracker_sparql_builder_predicate (metadata, "nfo:channels");
			tracker_sparql_builder_object_int64 (metadata, (gint64) n);
		}

		audio_codec = xine_get_meta_info (stream, XINE_META_INFO_AUDIOCODEC);
		if (audio_codec) {
			tracker_sparql_builder_predicate (metadata, "nfo:codec");
			tracker_sparql_builder_object_unvalidated (metadata, audio_codec);
		}

		track = xine_get_meta_info (stream, XINE_META_INFO_TRACK_NUMBER);
		if (track) {
			tracker_sparql_builder_predicate (metadata, "nmm:trackNumber");
			tracker_sparql_builder_object_unvalidated (metadata, track);
		}

	}


	/* Tags */

	comment = xine_get_meta_info (stream, XINE_META_INFO_COMMENT);
	if (comment && (has_video || has_audio)) {
		tracker_sparql_builder_predicate (metadata, "nie:comment");
		tracker_sparql_builder_object_unvalidated (metadata, comment);
	}

	title = xine_get_meta_info (stream, XINE_META_INFO_TITLE);
	if (title && (has_video || has_audio)) {
		tracker_sparql_builder_predicate (metadata, "nie:title");
		tracker_sparql_builder_object_unvalidated (metadata, title);
	}

	year = tracker_extract_guess_date (xine_get_meta_info (stream, XINE_META_INFO_YEAR));
	if (year) {
		tracker_sparql_builder_predicate (metadata, "nie:contentCreated");
		tracker_sparql_builder_object_unvalidated (metadata, year);
		g_free (year);
	}

	genre = xine_get_meta_info (stream, XINE_META_INFO_GENRE);
	if (genre) {
		tracker_sparql_builder_predicate (metadata, "nfo:genre");
		tracker_sparql_builder_object_unvalidated (metadata, genre);
	}


#if 0
	/* FIXME: "Video.Copyright" seems missing */
	tracker_sparql_builder_predicate (metadata, "nie:copyright");
	tracker_sparql_builder_object_unvalidated (metadata, NULL);
#endif

endofit:

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
tracker_extract_get_data (void)
{
	return data;
}
