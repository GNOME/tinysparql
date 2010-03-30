/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
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

#include <vorbis/vorbisfile.h>

#include <libtracker-common/tracker-file-utils.h>

#include <libtracker-client/tracker.h>

#include <libtracker-extract/tracker-extract.h>

static void extract_vorbis (const char            *uri,
                            TrackerSparqlBuilder  *preupdate,
                            TrackerSparqlBuilder  *metadata);


typedef struct {
	gchar *creator;
} VorbisNeedsMergeData;

typedef struct {
	gchar *title,*artist,*album,*albumartist, *trackcount, *tracknumber,
		*DiscNo, *Performer, *TrackGain, *TrackPeakGain, *AlbumGain,
		*AlbumPeakGain, *date, *comment, *genre, *Codec, *CodecVersion,
		*Samplerate ,*Channels, *MBAlbumID, *MBArtistID, *MBAlbumArtistID,
		*MBTrackID, *Lyrics, *Copyright, *License, *Organization, *Location,
		*Publisher;
} VorbisData;

static TrackerExtractData extract_data[] = {
	{ "audio/x-vorbis+ogg", extract_vorbis },
	{ "application/ogg", extract_vorbis },
	{ NULL, NULL }
};

static gchar *
ogg_get_comment (vorbis_comment *vc,
                 gchar          *label)
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

static void
extract_vorbis (const char *uri,
                TrackerSparqlBuilder *preupdate,
                TrackerSparqlBuilder *metadata)
{
	FILE           *f;
	OggVorbis_File  vf;
	vorbis_comment *comment;
	vorbis_info    *vi;
	unsigned int    bitrate;
	gint            time;
	gchar          *filename;
	VorbisData      vorbis_data = { 0 };
	VorbisNeedsMergeData merge_data = { 0 };
	gchar *artist_uri = NULL, *album_uri = NULL;

	filename = g_filename_from_uri (uri, NULL, NULL);
	f = tracker_file_open (filename, "r", FALSE);
	g_free (filename);

	if (!f) {
		return;
	}

	if (ov_open (f, &vf, NULL, 0) < 0) {
		tracker_file_close (f, FALSE);
		return;
	}

	if ((comment = ov_comment (&vf, -1)) != NULL) {
		gchar *date;
		vorbis_data.title = ogg_get_comment (comment, "title");
		vorbis_data.artist = ogg_get_comment (comment, "artist");
		vorbis_data.album = ogg_get_comment (comment, "album");
		vorbis_data.albumartist = ogg_get_comment (comment, "albumartist");
		vorbis_data.trackcount = ogg_get_comment (comment, "trackcount");
		vorbis_data.tracknumber = ogg_get_comment (comment, "tracknumber");
		vorbis_data.DiscNo = ogg_get_comment (comment, "DiscNo");
		vorbis_data.Performer = ogg_get_comment (comment, "Performer");
		vorbis_data.TrackGain = ogg_get_comment (comment, "TrackGain");
		vorbis_data.TrackPeakGain = ogg_get_comment (comment, "TrackPeakGain");
		vorbis_data.AlbumGain = ogg_get_comment (comment, "AlbumGain");
		vorbis_data.AlbumPeakGain = ogg_get_comment (comment, "AlbumPeakGain");
		date = ogg_get_comment (comment, "date");
		vorbis_data.date = tracker_date_guess (date);
		g_free (date);
		vorbis_data.comment = ogg_get_comment (comment, "comment");
		vorbis_data.genre = ogg_get_comment (comment, "genre");
		vorbis_data.Codec = ogg_get_comment (comment, "Codec");
		vorbis_data.CodecVersion = ogg_get_comment (comment, "CodecVersion");
		vorbis_data.Samplerate = ogg_get_comment (comment, "SampleRate");
		vorbis_data.Channels = ogg_get_comment (comment, "Channels");
		vorbis_data.MBAlbumID = ogg_get_comment (comment, "MBAlbumID");
		vorbis_data.MBArtistID = ogg_get_comment (comment, "MBArtistID");
		vorbis_data.MBAlbumArtistID = ogg_get_comment (comment, "MBAlbumArtistID");
		vorbis_data.MBTrackID = ogg_get_comment (comment, "MBTrackID");
		vorbis_data.Lyrics = ogg_get_comment (comment, "Lyrics");
		vorbis_data.Copyright = ogg_get_comment (comment, "Copyright");
		vorbis_data.License = ogg_get_comment (comment, "License");
		vorbis_data.Organization = ogg_get_comment (comment, "Organization");
		vorbis_data.Location = ogg_get_comment (comment, "Location");
		vorbis_data.Publisher = ogg_get_comment (comment, "Publisher");

		vorbis_comment_clear (comment);
	}

	merge_data.creator = tracker_coalesce (3, vorbis_data.artist,
	                                       vorbis_data.albumartist,
	                                       vorbis_data.Performer);

	if (merge_data.creator) {
		artist_uri = tracker_uri_printf_escaped ("urn:artist:%s", merge_data.creator);

		tracker_sparql_builder_insert_open (preupdate, NULL);

		tracker_sparql_builder_subject_iri (preupdate, artist_uri);
		tracker_sparql_builder_predicate (preupdate, "a");
		tracker_sparql_builder_object (preupdate, "nmm:Artist");
		tracker_sparql_builder_predicate (preupdate, "nmm:artistName");
		tracker_sparql_builder_object_unvalidated (preupdate, merge_data.creator);

		tracker_sparql_builder_insert_close (preupdate);

		g_free (merge_data.creator);
	}

	if (vorbis_data.album) {
		album_uri = tracker_uri_printf_escaped ("urn:album:%s", vorbis_data.album);

		tracker_sparql_builder_insert_open (preupdate, NULL);

		tracker_sparql_builder_subject_iri (preupdate, album_uri);
		tracker_sparql_builder_predicate (preupdate, "a");
		tracker_sparql_builder_object (preupdate, "nmm:MusicAlbum");
		tracker_sparql_builder_predicate (preupdate, "nmm:albumTitle");
		tracker_sparql_builder_object_unvalidated (preupdate, vorbis_data.album);

		tracker_sparql_builder_insert_close (preupdate);

		if (vorbis_data.trackcount) {
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
			tracker_sparql_builder_object_unvalidated (preupdate, vorbis_data.trackcount);

			tracker_sparql_builder_insert_close (preupdate);
		}

		if (vorbis_data.AlbumGain) {
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
			tracker_sparql_builder_object_double (preupdate, atof (vorbis_data.AlbumGain));

			tracker_sparql_builder_insert_close (preupdate);
		}

		if (vorbis_data.AlbumPeakGain) {
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
			tracker_sparql_builder_object_double (preupdate, atof (vorbis_data.AlbumPeakGain));

			tracker_sparql_builder_insert_close (preupdate);
		}

		if (vorbis_data.DiscNo) {
			tracker_sparql_builder_delete_open (preupdate, NULL);
			tracker_sparql_builder_subject_iri (preupdate, album_uri);
			tracker_sparql_builder_predicate (preupdate, "nmm:setNumber");
			tracker_sparql_builder_object_variable (preupdate, "unknown");
			tracker_sparql_builder_delete_close (preupdate);

			tracker_sparql_builder_where_open (preupdate);
			tracker_sparql_builder_subject_iri (preupdate, album_uri);
			tracker_sparql_builder_predicate (preupdate, "nmm:setNumber");
			tracker_sparql_builder_object_variable (preupdate, "unknown");
			tracker_sparql_builder_where_close (preupdate);

			tracker_sparql_builder_insert_open (preupdate, NULL);

			tracker_sparql_builder_subject_iri (preupdate, album_uri);
			tracker_sparql_builder_predicate (preupdate, "nmm:setNumber");
			tracker_sparql_builder_object_int64 (preupdate, atoi (vorbis_data.DiscNo));

			tracker_sparql_builder_insert_close (preupdate);
		}

		g_free (vorbis_data.album);
	}

	g_free (vorbis_data.trackcount);
	g_free (vorbis_data.AlbumPeakGain);
	g_free (vorbis_data.AlbumGain);
	g_free (vorbis_data.DiscNo);

	tracker_sparql_builder_predicate (metadata, "a");
	tracker_sparql_builder_object (metadata, "nmm:MusicPiece");
	tracker_sparql_builder_object (metadata, "nfo:Audio");

	tracker_sparql_builder_predicate (metadata, "nmm:performer");
	tracker_sparql_builder_object_unvalidated (metadata, artist_uri);

	tracker_sparql_builder_predicate (metadata, "nmm:musicAlbum");
	tracker_sparql_builder_object_unvalidated (metadata, album_uri);

	if (vorbis_data.title) {
		tracker_sparql_builder_predicate (metadata, "nie:title");
		tracker_sparql_builder_object_unvalidated (metadata, vorbis_data.title);
		g_free (vorbis_data.title);
	}

	if (vorbis_data.tracknumber) {
		tracker_sparql_builder_predicate (metadata, "nmm:trackNumber");
		tracker_sparql_builder_object_unvalidated (metadata, vorbis_data.tracknumber);
		g_free (vorbis_data.tracknumber);
	}

	if (vorbis_data.TrackGain) {
		/* tracker_statement_list_insert (metadata, uri, _PREFIX "", vorbis_data.); */
		g_free (vorbis_data.TrackGain);
	}
	if (vorbis_data.TrackPeakGain) {
		/* tracker_statement_list_insert (metadata, uri, _PREFIX "", vorbis_data.); */
		g_free (vorbis_data.TrackPeakGain);
	}

	if (vorbis_data.comment) {
		tracker_sparql_builder_predicate (metadata, "nie:comment");
		tracker_sparql_builder_object_unvalidated (metadata, vorbis_data.comment);
		g_free (vorbis_data.comment);
	}

	if (vorbis_data.date) {
		tracker_sparql_builder_predicate (metadata, "nie:contentCreated");
		tracker_sparql_builder_object_unvalidated (metadata, vorbis_data.date);
		g_free (vorbis_data.date);
	}

	if (vorbis_data.genre) {
		tracker_sparql_builder_predicate (metadata, "nfo:genre");
		tracker_sparql_builder_object_unvalidated (metadata, vorbis_data.genre);
		g_free (vorbis_data.genre);
	}

	if (vorbis_data.Codec) {
		tracker_sparql_builder_predicate (metadata, "nfo:codec");
		tracker_sparql_builder_object_unvalidated (metadata, vorbis_data.Codec);
		g_free (vorbis_data.Codec);
	}

	if (vorbis_data.CodecVersion) {
		/* tracker_statement_list_insert (metadata, uri, _PREFIX "", vorbis_data.); */
		g_free (vorbis_data.CodecVersion);
	}

	if (vorbis_data.Samplerate) {
		tracker_sparql_builder_predicate (metadata, "nfo:sampleRate");
		tracker_sparql_builder_object_unvalidated (metadata, vorbis_data.Samplerate);
		g_free (vorbis_data.Samplerate);
	}

	if (vorbis_data.Channels) {
		tracker_sparql_builder_predicate (metadata, "nfo:channels");
		tracker_sparql_builder_object_unvalidated (metadata, vorbis_data.Channels);
		g_free (vorbis_data.Channels);
	}

	if (vorbis_data.MBAlbumID) {
		/* tracker_statement_list_insert (metadata, uri, _PREFIX "", vorbis_data.MBAlbumID); */
		g_free (vorbis_data.MBAlbumID);
	}

	if (vorbis_data.MBArtistID) {
		/* tracker_statement_list_insert (metadata, uri, _PREFIX "", vorbis_data.MBArtistID); */
		g_free (vorbis_data.MBArtistID);
	}

	if (vorbis_data.MBAlbumArtistID) {
		/* tracker_statement_list_insert (metadata, uri, _PREFIX "", vorbis_data.MBAlbumArtistID); */
		g_free (vorbis_data.MBAlbumArtistID);
	}

	if (vorbis_data.MBTrackID) {
		/* tracker_statement_list_insert (metadata, uri, _PREFIX "", vorbis_data.MBTrackID); */
		g_free (vorbis_data.MBTrackID);
	}

	if (vorbis_data.Lyrics) {
		tracker_sparql_builder_predicate (metadata, "nie:plainTextContent");
		tracker_sparql_builder_object_unvalidated (metadata, vorbis_data.Lyrics);
		g_free (vorbis_data.Lyrics);
	}

	if (vorbis_data.Copyright) {
		tracker_sparql_builder_predicate (metadata, "nie:copyright");
		tracker_sparql_builder_object_unvalidated (metadata, vorbis_data.Copyright);
		g_free (vorbis_data.Copyright);
	}

	if (vorbis_data.License) {
		tracker_sparql_builder_predicate (metadata, "nie:license");
		tracker_sparql_builder_object_unvalidated (metadata, vorbis_data.License);
		g_free (vorbis_data.License);
	}

	if (vorbis_data.Organization) {
		/* tracker_statement_list_insert (metadata, uri, _PREFIX "", vorbis_data.Organization); */
		g_free (vorbis_data.Organization);
	}

	if (vorbis_data.Location) {
		/* tracker_statement_list_insert (metadata, uri, _PREFIX "", vorbis_data.Location); */
		g_free (vorbis_data.Location);
	}

	if (vorbis_data.Publisher) {
		tracker_sparql_builder_predicate (metadata, "dc:publisher");

		tracker_sparql_builder_object_blank_open (metadata);
		tracker_sparql_builder_predicate (metadata, "a");
		tracker_sparql_builder_object (metadata, "nco:Contact");

		tracker_sparql_builder_predicate (metadata, "nco:fullname");
		tracker_sparql_builder_object_unvalidated (metadata, vorbis_data.Publisher);
		tracker_sparql_builder_object_blank_close (metadata);
		g_free (vorbis_data.Publisher);
	}

	if ((vi = ov_info (&vf, 0)) != NULL ) {
		bitrate = vi->bitrate_nominal / 1000;

		tracker_sparql_builder_predicate (metadata, "nfo:averageBitrate");
		tracker_sparql_builder_object_int64 (metadata, (gint64) bitrate);

		/*
		  tracker_statement_list_insert_with_int (metadata, uri, "Audio.CodecVersion", vi->version);
		  tracker_statement_list_insert_with_int (metadata, uri, NFO_PREFIX "channels", vi->channels);
		  tracker_statement_list_insert_with_int (metadata, uri, NFO_PREFIX "sampleRate", vi->rate);
		*/
	}

	/* Duration */
	if ((time = ov_time_total (&vf, -1)) != OV_EINVAL) {
		tracker_sparql_builder_predicate (metadata, "nfo:duration");
		tracker_sparql_builder_object_int64 (metadata, (gint64) time);
	}

	/* NOTE: This calls fclose on the file */
	ov_clear (&vf);

	g_free (artist_uri);
	g_free (album_uri);
}

TrackerExtractData *
tracker_extract_get_data (void)
{
	return extract_data;
}
