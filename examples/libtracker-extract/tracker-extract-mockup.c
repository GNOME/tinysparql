/*
 * Copyright (C) 2010, Your name <Your email address>
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

#include <stdio.h>

/* TODO: Include any 3rd party libraries here. */

#include <gio/gio.h>

#include <libtracker-extract/tracker-extract.h>
#include <libtracker-sparql/tracker-sparql.h>

static void extract_mockup (const gchar          *uri,
                            TrackerSparqlBuilder *preupdate,
                            TrackerSparqlBuilder *metadata);

static TrackerExtractData data[] = {
	/* TODO: Insert mime types and functions here. */
	{ "audio/mpeg",  extract_mockup },
	{ "audio/x-mp3", extract_mockup },
	{ NULL, NULL }
};

static void
extract_mockup (const gchar          *uri,
                TrackerSparqlBuilder *preupdate,
                TrackerSparqlBuilder *metadata)
{
	/* File information */
	FILE *f;
	GFileInfo *info;
	GFile *file;
	GError *error = NULL;
	gchar *filename;
	goffset size;

	/* Data input */
	gchar *title_tagv1;
	gchar *title_tagv2;
	gchar *title_tagv3;
	gchar *title_unknown;
	gchar *lyricist_tagv2;
	gchar *lyricist_unknown;

	/* Coalesced input */
	const gchar *title;
	gchar *performer;
	gchar *performer_uri;
	const gchar *lyricist;
	gchar *lyricist_uri;
	gchar *album;
	gchar *album_uri;
	gchar *genre;
	gchar *text;
	gchar *recording_time;
	gchar *copyright;
	gchar *publisher;
	gchar *comment;
	gchar *composer;
	gchar *composer_uri;
	gint track_number;
	gint track_count;
	guint32 duration;

	filename = g_filename_from_uri (uri, NULL, NULL);

	file = g_file_new_for_path (filename);
	info = g_file_query_info (file,
	                          G_FILE_ATTRIBUTE_STANDARD_SIZE,
	                          G_FILE_QUERY_INFO_NONE,
	                          NULL,
	                          &error);

	if (G_UNLIKELY (error)) {
		g_message ("Could not get size for '%s', %s",
		           filename,
		           error->message);
		g_error_free (error);
		size = 0;
	} else {
		size = g_file_info_get_size (info);
		g_object_unref (info);
	}

	g_object_unref (file);

	/* TODO: Do any pre-checks on the file
	 * (i.e check file size so we don't handle files of 0 size)
	 */
	if (size < 64) {
		g_free (filename);
		return;
	}

	/* TODO: Open file */
	f = fopen (filename, "r");

	if (!f) {
		g_free (filename);
		return;
	}
	
	/* TODO: Get data from file.
	 *
	 * (We will use dummy data for this example)
	 */
	title_tagv1 = NULL;
	title_tagv2 = g_strdup ("Burn The Witch");
	title_tagv3 = g_strdup ("");
	title_unknown = g_strdup ("Unknown");

	lyricist_tagv2 = g_strdup ("Someone");
	lyricist_unknown = g_strdup ("Someone");

	/* TODO: Close file */
	fclose (f);

	/* TODO: Make sure we coalesce duplicate values */
	title = tracker_coalesce_strip (4, title_tagv1, title_tagv2, title_tagv3, title_unknown);
	lyricist = tracker_coalesce_strip (2, lyricist_tagv2, lyricist_unknown);

	performer = g_strdup ("Stone Gods");
	composer = NULL;
	album = g_strdup ("Silver Spoons And Broken Bones");
	genre = g_strdup ("Rock");
	recording_time = NULL;
	publisher = NULL;
	text = g_strdup ("Some description");
	copyright = g_strdup ("Who knows?");
	comment = g_strdup ("I love this track");
	track_number = 1;
	track_count = 12;
	duration = 32;

	/* TODO: Do any pre-updates
	 *
	 * (This involves creating any database nodes for consistent
	 *  data objects, for example, an artist which might be used n times)
	 */
	if (performer) {
		performer_uri = tracker_sparql_escape_uri_printf ("urn:artist:%s", performer);

		tracker_sparql_builder_insert_open (preupdate, NULL);
		tracker_sparql_builder_subject_iri (preupdate, performer_uri);
		tracker_sparql_builder_predicate (preupdate, "a");
		tracker_sparql_builder_object (preupdate, "nmm:Artist");
		tracker_sparql_builder_predicate (preupdate, "nmm:artistName");
		tracker_sparql_builder_object_unvalidated (preupdate, performer);
		tracker_sparql_builder_insert_close (preupdate);
	} else {
		performer_uri = NULL;
	}

	if (composer) {
		composer_uri = tracker_sparql_escape_uri_printf ("urn:artist:%s", composer);

		tracker_sparql_builder_insert_open (preupdate, NULL);
		tracker_sparql_builder_subject_iri (preupdate, composer_uri);
		tracker_sparql_builder_predicate (preupdate, "a");
		tracker_sparql_builder_object (preupdate, "nmm:Artist");
		tracker_sparql_builder_predicate (preupdate, "nmm:artistName");
		tracker_sparql_builder_object_unvalidated (preupdate, composer);
		tracker_sparql_builder_insert_close (preupdate);
	} else {
		composer_uri = NULL;
	}

	if (lyricist) {
		lyricist_uri = tracker_sparql_escape_uri_printf ("urn:artist:%s", lyricist);

		tracker_sparql_builder_insert_open (preupdate, NULL);
		tracker_sparql_builder_subject_iri (preupdate, lyricist_uri);
		tracker_sparql_builder_predicate (preupdate, "a");
		tracker_sparql_builder_object (preupdate, "nmm:Artist");
		tracker_sparql_builder_predicate (preupdate, "nmm:artistName");
		tracker_sparql_builder_object_unvalidated (preupdate, lyricist);
		tracker_sparql_builder_insert_close (preupdate);
	} else {
		lyricist_uri = NULL;
	}

	if (album) {
		album_uri = tracker_sparql_escape_uri_printf ("urn:album:%s", album);

		tracker_sparql_builder_insert_open (preupdate, NULL);
		tracker_sparql_builder_subject_iri (preupdate, album_uri);
		tracker_sparql_builder_predicate (preupdate, "a");
		tracker_sparql_builder_object (preupdate, "nmm:MusicAlbum");
		tracker_sparql_builder_predicate (preupdate, "nmm:albumTitle");
		tracker_sparql_builder_object_unvalidated (preupdate, album);
		tracker_sparql_builder_insert_close (preupdate);

		if (track_count > 0) {
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
			tracker_sparql_builder_object_int64 (preupdate, track_count);
			tracker_sparql_builder_insert_close (preupdate);
		}
	} else {
		album_uri = NULL;
	}

	/* TODO: Do any metadata updates
	 *
	 * (This is where you can use entities created in the
	 *  pre-updates part, like an artist).
	 */
	tracker_sparql_builder_predicate (metadata, "a");
	tracker_sparql_builder_object (metadata, "nmm:MusicPiece");
	tracker_sparql_builder_object (metadata, "nfo:Audio");

	if (title) {
		tracker_sparql_builder_predicate (metadata, "nie:title");
		tracker_sparql_builder_object_unvalidated (metadata, title);
	}

	if (lyricist_uri) {
		tracker_sparql_builder_predicate (metadata, "nmm:lyricist");
		tracker_sparql_builder_object_iri (metadata, lyricist_uri);
		g_free (lyricist_uri);
	}

	if (performer_uri) {
		tracker_sparql_builder_predicate (metadata, "nmm:performer");
		tracker_sparql_builder_object_iri (metadata, performer_uri);
		g_free (performer_uri);
	}

	if (composer_uri) {
		tracker_sparql_builder_predicate (metadata, "nmm:composer");
		tracker_sparql_builder_object_iri (metadata, composer_uri);
		g_free (composer_uri);
	}

	if (album_uri) {
		tracker_sparql_builder_predicate (metadata, "nmm:musicAlbum");
		tracker_sparql_builder_object_iri (metadata, album_uri);
		g_free (album_uri);
	}

	if (recording_time) {
		tracker_sparql_builder_predicate (metadata, "nie:contentCreated");
		tracker_sparql_builder_object_unvalidated (metadata, recording_time);
		g_free (recording_time);
	}

	if (genre) {
		tracker_sparql_builder_predicate (metadata, "nfo:genre");
		tracker_sparql_builder_object_unvalidated (metadata, genre);
		g_free (genre);
	}

	if (copyright) {
		tracker_sparql_builder_predicate (metadata, "nie:copyright");
		tracker_sparql_builder_object_unvalidated (metadata, copyright);
		g_free (copyright);
	}

	if (comment) {
		tracker_sparql_builder_predicate (metadata, "nie:comment");
		tracker_sparql_builder_object_unvalidated (metadata, comment);
		g_free (comment);
	}

	if (publisher) {
		tracker_sparql_builder_predicate (metadata, "nco:publisher");
		tracker_sparql_builder_object_blank_open (metadata);
		tracker_sparql_builder_predicate (metadata, "a");
		tracker_sparql_builder_object (metadata, "nco:Contact");
		tracker_sparql_builder_predicate (metadata, "nco:fullname");
		tracker_sparql_builder_object_unvalidated (metadata, publisher);
		tracker_sparql_builder_object_blank_close (metadata);
		g_free (publisher);
	}

	if (track_number > 0) {
		tracker_sparql_builder_predicate (metadata, "nmm:trackNumber");
		tracker_sparql_builder_object_int64 (metadata, track_number);
	}

	if (duration > 0) {
		tracker_sparql_builder_predicate (metadata, "nfo:duration");
		tracker_sparql_builder_object_int64 (metadata, duration);
	}

	if (text) {
		/* NOTE: This is for Full Text Search (FTS) indexing content */
		tracker_sparql_builder_predicate (metadata, "nie:plainTextContent");
		tracker_sparql_builder_object_unvalidated (metadata, text);
		g_free (text);
	}

	/* TODO: Clean up */
	g_free (title_tagv1);
	g_free (title_tagv2);
	g_free (title_tagv3);
	g_free (title_unknown);

	g_free (lyricist_tagv2);
	g_free (lyricist_unknown);

	g_free (album);
	g_free (composer);
	g_free (performer);
	g_free (filename);
}

TrackerExtractData *
tracker_extract_get_data (void)
{
	/* NOTE: This function has to exist, tracker-extract checks
	 * the symbole table for this function and if it doesn't
	 * exist, the module is not loaded to be used as an extractor.
	 */
	return data;
}
