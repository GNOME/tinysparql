/*
 * Copyright (C) 2010, Intel Corporation
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

#include <FLAC/metadata.h>

#include <libtracker-common/tracker-common.h>

#include <libtracker-extract/tracker-extract.h>

typedef struct {
	gchar *title;
	gchar *artist;
	gchar *album;
	gchar *albumartist;
	gchar *trackcount;
	gchar *tracknumber;
	gchar *discno;
	gchar *performer;
	gchar *trackgain;
	gchar *trackpeakgain;
	gchar *albumgain;
	gchar *albumpeakgain;
	gchar *date;
	gchar *comment;
	gchar *genre;
	gchar *mbalbumid;
	gchar *mbartistid;
	gchar *mbalbumartistid;
	gchar *mbtrackid;
	gchar *lyrics;
	gchar *copyright;
	gchar *license;
	gchar *organisation;
	gchar *location;
	gchar *publisher;
	guint samplerate;
	guint channels;
	guint bps;
	guint64 total;
} FlacData;

static void
parse_vorbis_comments (FLAC__StreamMetadata_VorbisComment *comment,
                       FlacData                           *fd)
{
	gint i;

	/* FIXME: I hate the amount of duplicating this does, complete
	   memory fragmentation. We should be able to use some
	   GStringChunks */
	for (i = 0; i < comment->num_comments; i++) {
		FLAC__StreamMetadata_VorbisComment_Entry entry;

		entry = comment->comments[i];

		/* entry.entry is the format NAME=metadata */
		if (g_ascii_strncasecmp (entry.entry, "title", 5) == 0) {
			fd->title = g_strdup (entry.entry + 6);
		} else if (g_ascii_strncasecmp (entry.entry, "artist=", 7) == 0) {
			/* FIXME: Handle multiple instances of artist */
			if (fd->artist == NULL) {
				fd->artist = g_strdup (entry.entry + 7);
			}
		} else if (g_ascii_strncasecmp (entry.entry, "album=", 6) == 0) {
			fd->album = g_strdup (entry.entry + 6);
		} else if (g_ascii_strncasecmp (entry.entry, "albumartist", 11) == 0) {
			fd->albumartist = g_strdup (entry.entry + 12);
		} else if (g_ascii_strncasecmp (entry.entry, "trackcount", 10) == 0) {
			fd->trackcount = g_strdup (entry.entry + 11);
		} else if (g_ascii_strncasecmp (entry.entry, "tracknumber", 11) == 0) {
			fd->tracknumber = g_strdup (entry.entry + 12);
		} else if (g_ascii_strncasecmp (entry.entry, "discno", 6) == 0) {
			fd->discno = g_strdup (entry.entry + 7);
                } else if (fd->discno == NULL && g_ascii_strncasecmp(entry.entry, "discnumber", 10) == 0) {
                        fd->discno = g_strdup (entry.entry + 11);
		} else if (g_ascii_strncasecmp (entry.entry, "performer", 9) == 0) {
			/* FIXME: Handle multiple instances of performer */
			if (fd->performer == NULL) {
				fd->performer = g_strdup (entry.entry + 10);
			}
		} else if (g_ascii_strncasecmp (entry.entry, "trackgain", 9) == 0) {
			fd->trackgain = g_strdup (entry.entry + 10);
		} else if (g_ascii_strncasecmp (entry.entry, "trackpeakgain", 13) == 0) {
			fd->trackpeakgain = g_strdup (entry.entry + 14);
		} else if (g_ascii_strncasecmp (entry.entry, "albumgain", 9) == 0) {
			fd->albumgain = g_strdup (entry.entry + 10);
		} else if (g_ascii_strncasecmp (entry.entry, "albumpeakgain", 13) == 0) {
			fd->albumpeakgain = g_strdup (entry.entry + 14);
		} else if (g_ascii_strncasecmp (entry.entry, "date", 4) == 0) {
			fd->date = tracker_date_guess (entry.entry + 5);
		} else if (g_ascii_strncasecmp (entry.entry, "comment", 7) == 0) {
			fd->comment = g_strdup (entry.entry + 8);
		} else if (g_ascii_strncasecmp (entry.entry, "genre", 5) == 0) {
			fd->genre = g_strdup (entry.entry + 6);
		} else if (g_ascii_strncasecmp (entry.entry, "mbalbumid", 9) == 0) {
			fd->mbalbumid = g_strdup (entry.entry + 10);
		} else if (g_ascii_strncasecmp (entry.entry, "mbartistid", 10) == 0) {
			fd->mbartistid = g_strdup (entry.entry + 11);
		} else if (g_ascii_strncasecmp (entry.entry, "mbalbumartistid", 15) == 0) {
			fd->mbalbumartistid = g_strdup (entry.entry + 16);
		} else if (g_ascii_strncasecmp (entry.entry, "mbtrackid", 9) == 0) {
			fd->mbtrackid = g_strdup (entry.entry + 10);
		} else if (g_ascii_strncasecmp (entry.entry, "lyrics", 6) == 0) {
			fd->lyrics = g_strdup (entry.entry + 7);
		} else if (g_ascii_strncasecmp (entry.entry, "copyright", 9) == 0) {
			fd->copyright = g_strdup (entry.entry + 10);
		} else if (g_ascii_strncasecmp (entry.entry, "license", 8) == 0) {
			fd->license = g_strdup (entry.entry + 9);
		} else if (g_ascii_strncasecmp (entry.entry, "organization", 12) == 0) {
			fd->organisation = g_strdup (entry.entry + 13);
		} else if (g_ascii_strncasecmp (entry.entry, "location", 8) == 0) {
			fd->location = g_strdup (entry.entry + 9);
		} else if (g_ascii_strncasecmp (entry.entry, "publisher", 9) == 0) {
			fd->publisher = g_strdup (entry.entry + 10);
		}
	}
}

static void
add_tuple (TrackerResource *metadata,
           const char      *predicate,
           const char      *object)
{
	if (object) {
		tracker_resource_set_string (metadata, predicate, object);
	}
}

G_MODULE_EXPORT gboolean
tracker_extract_get_metadata (TrackerExtractInfo *info)
{
	FLAC__Metadata_SimpleIterator *iter;
	FLAC__StreamMetadata *stream = NULL, *vorbis, *picture;
	FLAC__bool success;
	FlacData fd = { 0 };
	TrackerResource *metadata, *artist, *album_disc, *album, *album_artist;
	gchar *filename, *uri;
	const gchar *creator;
	GFile *file;
	goffset size;

	file = tracker_extract_info_get_file (info);
	filename = g_file_get_path (file);

	size = tracker_file_get_size (filename);

	if (size < 18) {
		g_free (filename);
		return FALSE;
	}

	iter = FLAC__metadata_simple_iterator_new ();
	success = FLAC__metadata_simple_iterator_init (iter, filename, TRUE, TRUE);
	g_free (filename);

	if (!success) {
		FLAC__metadata_simple_iterator_delete (iter);
		return FALSE;
	}

	uri = g_file_get_uri (file);

	do {
		switch (FLAC__metadata_simple_iterator_get_block_type (iter)) {
		case FLAC__METADATA_TYPE_STREAMINFO:
			stream = FLAC__metadata_simple_iterator_get_block (iter);
			break;

		case FLAC__METADATA_TYPE_VORBIS_COMMENT:
			vorbis = FLAC__metadata_simple_iterator_get_block (iter);
			parse_vorbis_comments (&(vorbis->data.vorbis_comment), &fd);
			FLAC__metadata_object_delete (vorbis);
			break;

		case FLAC__METADATA_TYPE_PICTURE:
			picture = FLAC__metadata_simple_iterator_get_block (iter);
			/* Deal with picture */
			FLAC__metadata_object_delete (picture);
			break;

		default:
			break;
		}
	} while (FLAC__metadata_simple_iterator_next (iter));

	metadata = tracker_resource_new (NULL);
	tracker_resource_add_uri (metadata, "rdf:type", "nmm:MusicPiece");
	tracker_resource_add_uri (metadata, "rdf:type", "nfo:Audio");

	creator = tracker_coalesce_strip (3, fd.artist, fd.albumartist,
	                                  fd.performer);

	if (creator) {
		artist = tracker_extract_new_artist (creator);

		tracker_resource_set_relation (metadata, "nmm:performer", artist);

		g_object_unref (artist);
	}

	if (fd.album) {
		if (fd.albumartist) {
			album_artist = tracker_extract_new_artist (fd.albumartist);
		} else {
			album_artist = NULL;
		}

		album_disc = tracker_extract_new_music_album_disc (fd.album,
		                                                   album_artist,
		                                                   fd.discno ? atoi(fd.discno) : 1);

		album = tracker_resource_get_first_relation (album_disc, "nmm:albumDiscAlbum");

		if (fd.trackcount) {
			tracker_resource_set_string (album, "nmm:albumTrackCount", fd.trackcount);
		}

		if (fd.albumgain) {
			tracker_resource_set_double (album, "nmm:albumGain", atof (fd.albumgain));
		}

		if (fd.albumpeakgain) {
			tracker_resource_set_double (album, "nmm:albumPeakGain", atof (fd.albumpeakgain));
		}

		tracker_resource_set_relation (metadata, "nmm:musicAlbum", album);
		tracker_resource_set_relation (metadata, "nmm:musicAlbumDisc", album_disc);

		g_object_unref (album_disc);
		g_object_unref (album_artist);
	}

	tracker_guarantee_resource_title_from_file (metadata, "nie:title", fd.title, uri, NULL);
	add_tuple (metadata, "nmm:trackNumber", fd.tracknumber);

	/* FIXME: Trackgain/Trackpeakgain: commented out in vorbis */

	add_tuple (metadata, "nie:comment", fd.comment);
	add_tuple (metadata, "nie:contentCreated", fd.date);
	add_tuple (metadata, "nfo:genre", fd.genre);
	add_tuple (metadata, "nie:plainTextContent", fd.lyrics);
	add_tuple (metadata, "nie:copyright", fd.copyright);
	add_tuple (metadata, "nie:license", fd.license);

	if (fd.publisher) {
		TrackerResource *publisher = tracker_extract_new_contact (fd.publisher);

		tracker_resource_set_relation (metadata, "dc:publisher", publisher);

		g_object_unref (publisher);
	}

	if (stream) {
		tracker_resource_set_int64 (metadata, "nfo:sampleRate", stream->data.stream_info.sample_rate);
		tracker_resource_set_int64 (metadata, "nfo:channels", stream->data.stream_info.channels);
		tracker_resource_set_int64 (metadata, "nfo:averageBitrate", stream->data.stream_info.bits_per_sample);
		tracker_resource_set_int64 (metadata, "nfo:duration", stream->data.stream_info.total_samples / stream->data.stream_info.sample_rate);
	}

	g_free (fd.artist);
	g_free (fd.album);
	g_free (fd.albumartist);
	g_free (fd.performer);
	g_free (fd.title);
	g_free (fd.trackcount);
	g_free (fd.tracknumber);
	g_free (fd.discno);
	g_free (fd.trackgain);
	g_free (fd.trackpeakgain);
	g_free (fd.albumgain);
	g_free (fd.albumpeakgain);
	g_free (fd.date);
	g_free (fd.comment);
	g_free (fd.genre);
	g_free (fd.mbalbumid);
	g_free (fd.mbartistid);
	g_free (fd.mbalbumartistid);
	g_free (fd.mbtrackid);
	g_free (fd.lyrics);
	g_free (fd.copyright);
	g_free (fd.license);
	g_free (fd.organisation);
	g_free (fd.location);
	g_free (fd.publisher);
	g_free (uri);

	return TRUE;
}
