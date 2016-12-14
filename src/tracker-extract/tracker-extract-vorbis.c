/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>

#include <glib.h>

#include <vorbis/vorbisfile.h>

#ifdef HAVE_LIBMEDIAART
#include <libmediaart/mediaart.h>
#endif

#include <libtracker-common/tracker-common.h>

#include <libtracker-extract/tracker-extract.h>

typedef struct {
	const gchar *creator_name;
	TrackerResource *creator;
} MergeData;

typedef struct {
	gchar *title;
	gchar *artist;
	gchar *album;
	gchar *album_artist;
	gchar *track_count;
	gchar *track_number;
	gchar *disc_number;
	gchar *performer;
	gchar *track_gain; 
	gchar *track_peak_gain;
	gchar *album_gain;
	gchar *album_peak_gain;
	gchar *date; 
	gchar *comment;
	gchar *genre; 
	gchar *codec;
	gchar *codec_version;
	gchar *sample_rate;
	gchar *channels; 
	gchar *mb_album_id; 
	gchar *mb_artist_id; 
	gchar *mb_album_artist_id;
	gchar *mb_track_id; 
	gchar *lyrics; 
	gchar *copyright; 
	gchar *license; 
	gchar *organization; 
	gchar *location;
	gchar *publisher;
} VorbisData;

static gchar *
ogg_get_comment (vorbis_comment *vc,
                 const gchar    *label)
{
	gchar *tag;
	gchar *utf_tag;

	if (vc && (tag = vorbis_comment_query (vc, label, 0)) != NULL) {
		utf_tag = g_locale_to_utf8 (tag, -1, NULL, NULL, NULL);
		/*g_free (tag);*/

		return utf_tag;
	} else {
		return NULL;
	}
}

G_MODULE_EXPORT gboolean
tracker_extract_get_metadata (TrackerExtractInfo *info)
{
	TrackerResource *metadata;
	VorbisData vd = { 0 };
	MergeData md = { 0 };
	FILE *f;
	gchar *filename;
	OggVorbis_File vf;
	vorbis_comment *comment;
	vorbis_info *vi;
	unsigned int bitrate;
	gint time;
	GFile *file;

	file = tracker_extract_info_get_file (info);
	filename = g_file_get_path (file);
	f = tracker_file_open (filename);
	g_free (filename);

	if (!f) {
		return FALSE;
	}

	if (ov_open (f, &vf, NULL, 0) < 0) {
		tracker_file_close (f, FALSE);
		return FALSE;
	}

	metadata = tracker_resource_new (NULL);
	tracker_resource_add_uri (metadata, "rdf:type", "nmm:MusicPiece");
	tracker_resource_add_uri (metadata, "rdf:type", "nfo:Audio");

	if ((comment = ov_comment (&vf, -1)) != NULL) {
		gchar *date;

		vd.title = ogg_get_comment (comment, "title");
		vd.artist = ogg_get_comment (comment, "artist");
		vd.album = ogg_get_comment (comment, "album");
		vd.album_artist = ogg_get_comment (comment, "albumartist");
		vd.track_count = ogg_get_comment (comment, "trackcount");
		vd.track_number = ogg_get_comment (comment, "tracknumber");
		vd.disc_number = ogg_get_comment (comment, "DiscNo");
                if (vd.disc_number == NULL)
                        vd.disc_number = ogg_get_comment (comment, "DiscNumber");
		vd.performer = ogg_get_comment (comment, "Performer");
		vd.track_gain = ogg_get_comment (comment, "TrackGain");
		vd.track_peak_gain = ogg_get_comment (comment, "TrackPeakGain");
		vd.album_gain = ogg_get_comment (comment, "AlbumGain");
		vd.album_peak_gain = ogg_get_comment (comment, "AlbumPeakGain");

		date = ogg_get_comment (comment, "date");
		vd.date = tracker_date_guess (date);
		g_free (date);

		vd.comment = ogg_get_comment (comment, "comment");
		vd.genre = ogg_get_comment (comment, "genre");
		vd.codec = ogg_get_comment (comment, "Codec");
		vd.codec_version = ogg_get_comment (comment, "CodecVersion");
		vd.sample_rate = ogg_get_comment (comment, "SampleRate");
		vd.channels = ogg_get_comment (comment, "Channels");
		vd.mb_album_id = ogg_get_comment (comment, "MBAlbumID");
		vd.mb_artist_id = ogg_get_comment (comment, "MBArtistID");
		vd.mb_album_artist_id = ogg_get_comment (comment, "MBAlbumArtistID");
		vd.mb_track_id = ogg_get_comment (comment, "MBTrackID");
		vd.lyrics = ogg_get_comment (comment, "Lyrics");
		vd.copyright = ogg_get_comment (comment, "Copyright");
		vd.license = ogg_get_comment (comment, "License");
		vd.organization = ogg_get_comment (comment, "Organization");
		vd.location = ogg_get_comment (comment, "Location");
		vd.publisher = ogg_get_comment (comment, "Publisher");

		vorbis_comment_clear (comment);
	}

	md.creator_name = tracker_coalesce_strip (3, vd.artist, vd.album_artist, vd.performer);

	if (md.creator_name) {
		md.creator = tracker_extract_new_artist (md.creator_name);

		tracker_resource_set_relation (metadata, "nmm:performer", md.creator);
	}

	if (vd.album) {
		TrackerResource *album, *album_disc, *album_artist;

		if (vd.album_artist) {
			album_artist = tracker_extract_new_artist (vd.album_artist);
		} else {
			album_artist = NULL;
		}

		album_disc = tracker_extract_new_music_album_disc (vd.album,
		                                                   album_artist,
		                                                   vd.disc_number ? atoi(vd.disc_number) : 1,
		                                                   vd.date);

		g_clear_object (&album_artist);

		album = tracker_resource_get_first_relation (album_disc, "nmm:albumDiscAlbum");


		if (vd.track_count) {
			tracker_resource_set_string (album, "nmm:albumTrackCount", vd.track_count);
		}

		if (vd.album_gain) {
			tracker_resource_set_double (album, "nmm:albumGain", atof (vd.album_gain));
		}

		if (vd.album_peak_gain) {
			tracker_resource_set_double (album, "nmm:albumPeakGain", atof (vd.album_peak_gain));
		}

		tracker_resource_set_relation (metadata, "nmm:musicAlbum", album);
		tracker_resource_set_relation (metadata, "nmm:musicAlbumDisc", album_disc);

		g_object_unref (album_disc);
	}

	g_free (vd.track_count);
	g_free (vd.album_peak_gain);
	g_free (vd.album_gain);
	g_free (vd.disc_number);

	if (vd.title) {
		tracker_resource_set_string (metadata, "nie:title", vd.title);
		g_free (vd.title);
	}

	if (vd.track_number) {
		tracker_resource_set_string (metadata, "nmm:trackNumber", vd.track_number);
		g_free (vd.track_number);
	}

	if (vd.track_gain) {
		/* TODO */
		g_free (vd.track_gain);
	}

	if (vd.track_peak_gain) {
		/* TODO */
		g_free (vd.track_peak_gain);
	}

	if (vd.comment) {
		tracker_resource_set_string (metadata, "nie:comment", vd.comment);
		g_free (vd.comment);
	}

	if (vd.date) {
		tracker_resource_set_string (metadata, "nie:contentCreated", vd.date);
		g_free (vd.date);
	}

	if (vd.genre) {
		tracker_resource_set_string (metadata, "nfo:genre", vd.genre);
		g_free (vd.genre);
	}

	if (vd.codec) {
		tracker_resource_set_string (metadata, "nfo:codec", vd.codec);
		g_free (vd.codec);
	}

	if (vd.codec_version) {
		/* TODO */
		g_free (vd.codec_version);
	}

	if (vd.sample_rate) {
		tracker_resource_set_string (metadata, "nfo:sampleRate", vd.sample_rate);
		g_free (vd.sample_rate);
	}

	if (vd.channels) {
		tracker_resource_set_string (metadata, "nfo:channels", vd.channels);
		g_free (vd.channels);
	}

	if (vd.mb_album_id) {
		/* TODO */
		g_free (vd.mb_album_id);
	}

	if (vd.mb_artist_id) {
		/* TODO */
		g_free (vd.mb_artist_id);
	}

	if (vd.mb_album_artist_id) {
		/* TODO */
		g_free (vd.mb_album_artist_id);
	}

	if (vd.mb_track_id) {
		/* TODO */
		g_free (vd.mb_track_id);
	}

	if (vd.lyrics) {
		tracker_resource_set_string (metadata, "nie:plainTextContent", vd.lyrics);
		g_free (vd.lyrics);
	}

	if (vd.copyright) {
		tracker_resource_set_string (metadata, "nie:copyright", vd.copyright);
		g_free (vd.copyright);
	}

	if (vd.license) {
		tracker_resource_set_string (metadata, "nie:license", vd.license);
		g_free (vd.license);
	}

	if (vd.organization) {
		/* TODO */
		g_free (vd.organization);
	}

	if (vd.location) {
		/* TODO */
		g_free (vd.location);
	}

	if (vd.publisher) {
		TrackerResource *publisher = tracker_extract_new_contact (vd.publisher);

		tracker_resource_set_relation (metadata, "dc:publisher", publisher);

		g_object_unref (publisher);
	}

	if ((vi = ov_info (&vf, 0)) != NULL ) {
		bitrate = vi->bitrate_nominal / 1000;

		tracker_resource_set_int64 (metadata, "nfo:averageBitrate", (gint64) bitrate);
	}

	/* Duration */
	if ((time = ov_time_total (&vf, -1)) != OV_EINVAL) {
		tracker_resource_set_int64 (metadata, "nfo:duration", (gint64) time);
	}

#ifdef HAVE_LIBMEDIAART
	if ((vd.album_artist || vd.artist) || vd.album) {
		MediaArtProcess *media_art_process;
		GError *error = NULL;
		gboolean success;

		media_art_process = tracker_extract_info_get_media_art_process (info);

		success = media_art_process_file (media_art_process,
		                                  MEDIA_ART_ALBUM,
		                                  MEDIA_ART_PROCESS_FLAGS_NONE,
		                                  file,
		                                  vd.album_artist ? vd.album_artist : vd.artist,
		                                  vd.album,
		                                  NULL,
		                                  &error);

		if (!success || error) {
			gchar *uri;

			uri = g_file_get_uri (file);
			g_warning ("Could not process media art for '%s', %s",
			           uri,
			           error ? error->message : "No error given");
			g_free (uri);
			g_clear_error (&error);
		}
	}
#endif

	g_free (vd.artist);
	g_free (vd.album);
	g_free (vd.album_artist);
	g_free (vd.performer);

	g_object_unref (md.creator);

#ifdef HAVE_POSIX_FADVISE
	if (posix_fadvise (fileno (f), 0, 0, POSIX_FADV_DONTNEED) != 0)
		g_warning ("posix_fadvise() call failed: %m");
#endif /* HAVE_POSIX_FADVISE */

	/* NOTE: This calls fclose on the file */
	ov_clear (&vf);

	tracker_extract_info_set_resource (info, metadata);
	g_object_unref (metadata);

	return TRUE;
}
