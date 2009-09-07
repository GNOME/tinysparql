/* Tracker Extract - extracts embedded metadata from files
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
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

#include <glib.h>

#include <vorbis/vorbisfile.h>

#include <libtracker-common/tracker-file-utils.h>
#include <libtracker-common/tracker-statement-list.h>
#include <libtracker-common/tracker-ontology.h>
#include <libtracker-common/tracker-utils.h>

#include "tracker-main.h"

#define NIE_PREFIX TRACKER_NIE_PREFIX
#define NFO_PREFIX TRACKER_NFO_PREFIX
#define DC_PREFIX TRACKER_DC_PREFIX
#define NCO_PREFIX TRACKER_NCO_PREFIX
#define NMM_PREFIX TRACKER_NMM_PREFIX
#define RDF_PREFIX TRACKER_RDF_PREFIX

static void extract_vorbis (const char *uri, 
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
                TrackerSparqlBuilder *metadata)
{
	FILE	       *f;
	OggVorbis_File  vf;
	vorbis_comment *comment;
	vorbis_info    *vi;
	unsigned int    bitrate;
	gint            time;
	gchar          *filename;
	VorbisData      vorbis_data = { 0 };
	VorbisNeedsMergeData merge_data = { 0 };

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
		vorbis_data.date = ogg_get_comment (comment, "date");
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

	tracker_statement_list_insert (metadata, uri, 
	                               RDF_PREFIX "type", 
	                               NFO_PREFIX "Audio");

	tracker_statement_list_insert (metadata, uri, 
	                               RDF_PREFIX "type", 
	                               NMM_PREFIX "MusicPiece");

	if (merge_data.creator) {
		gchar *canonical_uri = tracker_uri_printf_escaped ("urn:artist:%s", merge_data.creator);
		tracker_statement_list_insert (metadata, canonical_uri, RDF_PREFIX "type", NMM_PREFIX "Artist");
		tracker_statement_list_insert (metadata, canonical_uri, NMM_PREFIX "artistName", merge_data.creator);
		tracker_statement_list_insert (metadata, uri, NMM_PREFIX "performer", canonical_uri);
		g_free (canonical_uri);
		g_free (merge_data.creator);
	}

	if (vorbis_data.album) {
		gchar *canonical_uri = tracker_uri_printf_escaped ("urn:album:%s", vorbis_data.album);
		tracker_statement_list_insert (metadata, canonical_uri, RDF_PREFIX "type", NMM_PREFIX "MusicAlbum");
		tracker_statement_list_insert (metadata, canonical_uri, NMM_PREFIX "albumTitle", vorbis_data.album);
		tracker_statement_list_insert (metadata, uri, NMM_PREFIX "musicAlbum", canonical_uri);
		g_free (canonical_uri);
		g_free (vorbis_data.album);
	}

	if (vorbis_data.title) {
		tracker_statement_list_insert (metadata, uri, NIE_PREFIX "title", vorbis_data.title);
		g_free (vorbis_data.title);
	}

	if (vorbis_data.trackcount) {
		tracker_statement_list_insert (metadata, uri, NMM_PREFIX "albumTrackCount", vorbis_data.trackcount);
		g_free (vorbis_data.trackcount);
	}

	if (vorbis_data.tracknumber) {
		tracker_statement_list_insert (metadata, uri, NMM_PREFIX "tracknumber", vorbis_data.tracknumber);
		g_free (vorbis_data.tracknumber);
	}

	if (vorbis_data.DiscNo) {
		tracker_statement_list_insert (metadata, uri, NMM_PREFIX "setNumber", vorbis_data.DiscNo);
		g_free (vorbis_data.DiscNo);
	}

	 if (vorbis_data.TrackGain) {
		/* tracker_statement_list_insert (metadata, uri, _PREFIX "", vorbis_data.); */
		g_free (vorbis_data.TrackGain);
	} 
	if (vorbis_data.TrackPeakGain) {
		/* tracker_statement_list_insert (metadata, uri, _PREFIX "", vorbis_data.); */
		g_free (vorbis_data.TrackPeakGain);
	}

	if (vorbis_data.AlbumGain) {
		tracker_statement_list_insert (metadata, uri, NMM_PREFIX "albumGain", vorbis_data.AlbumGain);
		g_free (vorbis_data.AlbumGain);
	}

	if (vorbis_data.AlbumPeakGain) {
		tracker_statement_list_insert (metadata, uri, NMM_PREFIX "albumPeakGain", vorbis_data.AlbumPeakGain);
		g_free (vorbis_data.AlbumPeakGain);
	}

	if (vorbis_data.comment) {
		tracker_statement_list_insert (metadata, uri, NIE_PREFIX "comment", vorbis_data.comment);
		g_free (vorbis_data.comment);
	}

	if (vorbis_data.date) {
		tracker_statement_list_insert (metadata, uri, NIE_PREFIX "contentCreated", vorbis_data.date);
		g_free (vorbis_data.date);
	}

	if (vorbis_data.genre) {
		tracker_statement_list_insert (metadata, uri, NFO_PREFIX "genre", vorbis_data.genre);
		g_free (vorbis_data.genre);
	}

	if (vorbis_data.Codec) {
		tracker_statement_list_insert (metadata, uri, NFO_PREFIX "Codec", vorbis_data.Codec);
		g_free (vorbis_data.Codec);
	}

	if (vorbis_data.CodecVersion) {
		/* tracker_statement_list_insert (metadata, uri, _PREFIX "", vorbis_data.); */
		g_free (vorbis_data.CodecVersion);
	}

	if (vorbis_data.Samplerate) {
		tracker_statement_list_insert (metadata, uri, NFO_PREFIX "sampleRate", vorbis_data.Samplerate);
		g_free (vorbis_data.Samplerate);
	}

	if (vorbis_data.Channels) {
		tracker_statement_list_insert (metadata, uri, NFO_PREFIX "channels", vorbis_data.Channels);
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
		tracker_statement_list_insert (metadata, uri, NIE_PREFIX "plainTextContent", vorbis_data.Lyrics);
		g_free (vorbis_data.Lyrics);
	}

	if (vorbis_data.Copyright) {
		tracker_statement_list_insert (metadata, uri, NIE_PREFIX "copyright", vorbis_data.Copyright);
		g_free (vorbis_data.Copyright);
	}

	if (vorbis_data.License) {
		tracker_statement_list_insert (metadata, uri, NIE_PREFIX "license", vorbis_data.License);
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
		tracker_statement_list_insert (metadata, ":", RDF_PREFIX "type", NCO_PREFIX "Contact");
		tracker_statement_list_insert (metadata, ":", NCO_PREFIX "fullname", vorbis_data.Publisher);
		tracker_statement_list_insert (metadata, uri, DC_PREFIX "publisher", ":");
		g_free (vorbis_data.Publisher);
	}

	if ((vi = ov_info (&vf, 0)) != NULL ) {
		bitrate = vi->bitrate_nominal / 1000;
		tracker_statement_list_insert_with_int (metadata, uri, NFO_PREFIX "averageBitrate", bitrate);
		/* tracker_statement_list_insert_with_int (metadata, uri, "Audio.CodecVersion", vi->version); */
		tracker_statement_list_insert_with_int (metadata, uri, NFO_PREFIX "channels", vi->channels);
		tracker_statement_list_insert_with_int (metadata, uri, NFO_PREFIX "sampleRate", vi->rate);
	}

	/* Duration */
	if ((time = ov_time_total (&vf, -1)) != OV_EINVAL) {
		tracker_statement_list_insert_with_int (metadata, uri, NFO_PREFIX "duration", time);
	}
	tracker_statement_list_insert (metadata, uri, NFO_PREFIX "codec", "vorbis");

	/* NOTE: This calls fclose on the file */
	ov_clear (&vf);
}

TrackerExtractData *
tracker_get_extract_data (void)
{
	return extract_data;
}
