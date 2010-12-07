/*
 * Copyright (C) 2010, Intel Corporation
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

static void extract_flac (const gchar          *uri,
                          TrackerSparqlBuilder *preupdate,
                          TrackerSparqlBuilder *metadata);

static TrackerExtractData extract_data[] = {
        { "audio/x-flac", extract_flac },
        { NULL, NULL }
};

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
		} else if (g_ascii_strncasecmp (entry.entry, "artist", 6) == 0) {
			/* FIXME: Handle multiple instances of artist */
			if (fd->artist == NULL) {
				fd->artist = g_strdup (entry.entry + 7);
			}
		} else if (g_ascii_strncasecmp (entry.entry, "album", 5) == 0) {
			fd->album = g_strdup (entry.entry + 6);
		} else if (g_ascii_strncasecmp (entry.entry, "albumartist", 11) == 0) {
			fd->albumartist = g_strdup (entry.entry + 12);
		} else if (g_ascii_strncasecmp (entry.entry, "trackcount", 10) == 0) {
			fd->trackcount = g_strdup (entry.entry + 11);
		} else if (g_ascii_strncasecmp (entry.entry, "tracknumber", 11) == 0) {
			fd->tracknumber = g_strdup (entry.entry + 12);
		} else if (g_ascii_strncasecmp (entry.entry, "discno", 6) == 0) {
			fd->discno = g_strdup (entry.entry + 7);
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
add_tuple (TrackerSparqlBuilder *metadata,
           const char           *predicate,
           const char           *object)
{
	if (object) {
		tracker_sparql_builder_predicate (metadata, predicate);
		tracker_sparql_builder_object_unvalidated (metadata, object);
	}
}

static void
extract_flac (const gchar          *uri,
              TrackerSparqlBuilder *preupdate,
              TrackerSparqlBuilder *metadata)
{
	FLAC__Metadata_SimpleIterator *iter;
	FLAC__StreamMetadata *stream = NULL, *vorbis, *picture;
	FLAC__bool success;
	FlacData fd = { 0 };
	gchar *filename, *artist_uri = NULL, *album_uri = NULL;
	const gchar *creator;
	goffset size;

	filename = g_filename_from_uri (uri, NULL, NULL);

	size = tracker_file_get_size (filename);

	if (size < 18) {
		g_free (filename);
		return;
	}

	iter = FLAC__metadata_simple_iterator_new ();
	success = FLAC__metadata_simple_iterator_init (iter, filename, TRUE, FALSE);
	g_free (filename);

	if (!success) {
		FLAC__metadata_simple_iterator_delete (iter);
		return;
	}

	while (!FLAC__metadata_simple_iterator_is_last (iter)) {
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

		FLAC__metadata_simple_iterator_next (iter);
	}

	creator = tracker_coalesce_strip (3, fd.artist, fd.albumartist,
	                                  fd.performer);

	if (creator) {
		artist_uri = tracker_sparql_escape_uri_printf ("urn:artist:%s", creator);

		tracker_sparql_builder_insert_open (preupdate, NULL);
		tracker_sparql_builder_subject_iri (preupdate, artist_uri);
		tracker_sparql_builder_predicate (preupdate, "a");
		tracker_sparql_builder_object (preupdate, "nmm:Artist");
		tracker_sparql_builder_predicate (preupdate, "nmm:artistName");
		tracker_sparql_builder_object_unvalidated (preupdate, creator);
		tracker_sparql_builder_insert_close (preupdate);
	}

	if (fd.album) {
		album_uri = tracker_sparql_escape_uri_printf ("urn:album:%s", fd.album);

		tracker_sparql_builder_insert_open (preupdate, NULL);

		tracker_sparql_builder_subject_iri (preupdate, album_uri);
		tracker_sparql_builder_predicate (preupdate, "a");
		tracker_sparql_builder_object (preupdate, "nmm:MusicAlbum");
		/* FIXME: nmm:albumTitle is now deprecated
		 * tracker_sparql_builder_predicate (preupdate, "nie:title");
		 */
		tracker_sparql_builder_predicate (preupdate, "nmm:albumTitle");
		tracker_sparql_builder_object_unvalidated (preupdate, fd.album);

		tracker_sparql_builder_insert_close (preupdate);

		if (fd.trackcount) {
			tracker_sparql_builder_delete_open (preupdate, NULL);
			tracker_sparql_builder_subject_iri (preupdate, album_uri);
			tracker_sparql_builder_predicate (preupdate, "nmm:albumTrackCount");
			tracker_sparql_builder_object_variable (preupdate, "unknown");
			tracker_sparql_builder_delete_close (preupdate);

			tracker_sparql_builder_where_open (preupdate);
			tracker_sparql_builder_subject_iri (preupdate, album_uri);
			tracker_sparql_builder_predicate (preupdate, "nmm:albumTrackCount");
			tracker_sparql_builder_object_variable (preupdate, "unknown");
			tracker_sparql_builder_where_close (preupdate);

			tracker_sparql_builder_insert_open (preupdate, NULL);

			tracker_sparql_builder_subject_iri (preupdate, album_uri);
			tracker_sparql_builder_predicate (preupdate, "nmm:albumTrackCount");
			tracker_sparql_builder_object_unvalidated (preupdate, fd.trackcount);

			tracker_sparql_builder_insert_close (preupdate);
		}

		if (fd.albumgain) {
			tracker_sparql_builder_delete_open (preupdate, NULL);
			tracker_sparql_builder_subject_iri (preupdate, album_uri);
			tracker_sparql_builder_predicate (preupdate, "nmm:albumGain");
			tracker_sparql_builder_object_variable (preupdate, "unknown");
			tracker_sparql_builder_delete_close (preupdate);

			tracker_sparql_builder_where_open (preupdate);
			tracker_sparql_builder_subject_iri (preupdate, album_uri);
			tracker_sparql_builder_predicate (preupdate, "nmm:albumGain");
			tracker_sparql_builder_object_variable (preupdate, "unknown");
			tracker_sparql_builder_where_close (preupdate);

			tracker_sparql_builder_insert_open (preupdate, NULL);

			tracker_sparql_builder_subject_iri (preupdate, album_uri);
			tracker_sparql_builder_predicate (preupdate, "nmm:albumGain");
			tracker_sparql_builder_object_double (preupdate, atof (fd.albumgain));

			tracker_sparql_builder_insert_close (preupdate);
		}

		if (fd.albumpeakgain) {
			tracker_sparql_builder_delete_open (preupdate, NULL);
			tracker_sparql_builder_subject_iri (preupdate, album_uri);
			tracker_sparql_builder_predicate (preupdate, "nmm:albumPeakGain");
			tracker_sparql_builder_object_variable (preupdate, "unknown");
			tracker_sparql_builder_delete_close (preupdate);

			tracker_sparql_builder_where_open (preupdate);
			tracker_sparql_builder_subject_iri (preupdate, album_uri);
			tracker_sparql_builder_predicate (preupdate, "nmm:albumPeakGain");
			tracker_sparql_builder_object_variable (preupdate, "unknown");
			tracker_sparql_builder_where_close (preupdate);

			tracker_sparql_builder_insert_open (preupdate, NULL);

			tracker_sparql_builder_subject_iri (preupdate, album_uri);
			tracker_sparql_builder_predicate (preupdate, "nmm:albumPeakGain");
			tracker_sparql_builder_object_double (preupdate, atof (fd.albumpeakgain));

			tracker_sparql_builder_insert_close (preupdate);
		}
	}

	tracker_sparql_builder_predicate (metadata, "a");
	tracker_sparql_builder_object (metadata, "nmm:MusicPiece");
	tracker_sparql_builder_object (metadata, "nfo:Audio");

	add_tuple (metadata, "nmm:performer", artist_uri);
	g_free (artist_uri);

	add_tuple (metadata, "nmm:musicAlbum", album_uri);
	g_free (album_uri);

	add_tuple (metadata, "nie:title", fd.title);
	add_tuple (metadata, "nmm:trackNumber", fd.tracknumber);

	if (fd.discno && fd.album && album_uri) {
		gchar *album_disc_uri;

		album_disc_uri = tracker_sparql_escape_uri_printf ("urn:album-disc:%s:Disc%d",
		                                                   fd.album, atoi(fd.discno));

		tracker_sparql_builder_insert_open (preupdate, NULL);
		tracker_sparql_builder_subject_iri (preupdate, album_disc_uri);
		tracker_sparql_builder_predicate (preupdate, "a");
		tracker_sparql_builder_object (preupdate, "nmm:MusicAlbumDisc");
		tracker_sparql_builder_insert_close (preupdate);

		tracker_sparql_builder_delete_open (preupdate, NULL);
		tracker_sparql_builder_subject_iri (preupdate, album_disc_uri);
		tracker_sparql_builder_predicate (preupdate, "nmm:setNumber");
		tracker_sparql_builder_object_variable (preupdate, "unknown");
		tracker_sparql_builder_delete_close (preupdate);
		tracker_sparql_builder_where_open (preupdate);
		tracker_sparql_builder_subject_iri (preupdate, album_disc_uri);
		tracker_sparql_builder_predicate (preupdate, "nmm:setNumber");
		tracker_sparql_builder_object_variable (preupdate, "unknown");
		tracker_sparql_builder_where_close (preupdate);

		tracker_sparql_builder_insert_open (preupdate, NULL);
		tracker_sparql_builder_subject_iri (preupdate, album_disc_uri);
		tracker_sparql_builder_predicate (preupdate, "nmm:setNumber");
		tracker_sparql_builder_object_int64 (preupdate, atoi (fd.discno));
		tracker_sparql_builder_insert_close (preupdate);

		tracker_sparql_builder_delete_open (preupdate, NULL);
		tracker_sparql_builder_subject_iri (preupdate, album_disc_uri);
		tracker_sparql_builder_predicate (preupdate, "nmm:albumDiscAlbum");
		tracker_sparql_builder_object_variable (preupdate, "unknown");
		tracker_sparql_builder_delete_close (preupdate);
		tracker_sparql_builder_where_open (preupdate);
		tracker_sparql_builder_subject_iri (preupdate, album_disc_uri);
		tracker_sparql_builder_predicate (preupdate, "nmm:albumDiscAlbum");
		tracker_sparql_builder_object_variable (preupdate, "unknown");
		tracker_sparql_builder_where_close (preupdate);

		tracker_sparql_builder_insert_open (preupdate, NULL);
		tracker_sparql_builder_subject_iri (preupdate, album_disc_uri);
		tracker_sparql_builder_predicate (preupdate, "nmm:albumDiscAlbum");
		tracker_sparql_builder_object_iri (preupdate, album_uri);
		tracker_sparql_builder_insert_close (preupdate);

		tracker_sparql_builder_predicate (metadata, "nmm:musicAlbumDisc");
		tracker_sparql_builder_object_iri (metadata, album_disc_uri);

		g_free (album_disc_uri);
	}

	/* FIXME: Trackgain/Trackpeakgain: commented out in vorbis */

	add_tuple (metadata, "nie:comment", fd.comment);
	add_tuple (metadata, "nie:contentCreated", fd.date);
	add_tuple (metadata, "nfo:genre", fd.genre);
	add_tuple (metadata, "nie:plainTextContent", fd.lyrics);
	add_tuple (metadata, "nie:copyright", fd.copyright);
	add_tuple (metadata, "nie:license", fd.license);

	if (fd.publisher) {
		tracker_sparql_builder_predicate (metadata, "dc:publisher");

		tracker_sparql_builder_object_blank_open (metadata);
		tracker_sparql_builder_predicate (metadata, "a");
		tracker_sparql_builder_object (metadata, "nco:Contact");

		tracker_sparql_builder_predicate (metadata, "nco:fullname");
		tracker_sparql_builder_object_unvalidated (metadata,
		                                           fd.publisher);
		tracker_sparql_builder_object_blank_close (metadata);
	}

	if (stream) {
		tracker_sparql_builder_predicate (metadata, "nfo:sampleRate");
		tracker_sparql_builder_object_int64 (metadata, 
		                                     stream->data.stream_info.sample_rate);

		tracker_sparql_builder_predicate (metadata, "nfo:channels");
		tracker_sparql_builder_object_int64 (metadata, 
		                                     stream->data.stream_info.channels);

		tracker_sparql_builder_predicate (metadata,
		                                  "nfo:averageBitrate");
		tracker_sparql_builder_object_int64 (metadata, 
		                                     stream->data.stream_info.bits_per_sample);

		tracker_sparql_builder_predicate (metadata, "nfo:duration");
		tracker_sparql_builder_object_int64 (metadata, 
		                                     stream->data.stream_info.total_samples / 
		                                     stream->data.stream_info.sample_rate);
	}

	g_free (fd.artist);
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
}

TrackerExtractData *
tracker_extract_get_data (void)
{
	return extract_data;
}
