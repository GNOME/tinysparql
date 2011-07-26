/*
 * Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 */

#include "config.h"

#include <stdlib.h>

#include <taglib/tag_c.h>

#include <glib-object.h>

#include <libtracker-common/tracker-ontologies.h>

#include "tracker-writeback-file.h"

#define TRACKER_TYPE_WRITEBACK_TAGLIB (tracker_writeback_taglib_get_type ())

typedef struct TrackerWritebackTaglib TrackerWritebackTaglib;
typedef struct TrackerWritebackTaglibClass TrackerWritebackTaglibClass;

struct TrackerWritebackTaglib {
	TrackerWritebackFile parent_instance;
};

struct TrackerWritebackTaglibClass {
	TrackerWritebackFileClass parent_class;
};

static GType                tracker_writeback_taglib_get_type         (void) G_GNUC_CONST;
static gboolean             writeback_taglib_update_file_metadata     (TrackerWritebackFile    *wbf,
                                                                       GFile                   *file,
                                                                       GPtrArray               *values,
                                                                       TrackerSparqlConnection *connection,
                                                                       GCancellable            *cancellable);
static const gchar * const *writeback_taglib_content_types            (TrackerWritebackFile    *wbf);
static gchar*               writeback_taglib_get_artist_name          (TrackerSparqlConnection *connection,
                                                                       const gchar             *urn);
static gchar*               writeback_taglib_get_album_name           (TrackerSparqlConnection *connection,
                                                                       const gchar             *urn);

G_DEFINE_DYNAMIC_TYPE (TrackerWritebackTaglib, tracker_writeback_taglib, TRACKER_TYPE_WRITEBACK_FILE);

static void
tracker_writeback_taglib_class_init (TrackerWritebackTaglibClass *klass)
{
	TrackerWritebackFileClass *writeback_file_class = TRACKER_WRITEBACK_FILE_CLASS (klass);

	writeback_file_class->update_file_metadata = writeback_taglib_update_file_metadata;
	writeback_file_class->content_types = writeback_taglib_content_types;
}

static void
tracker_writeback_taglib_class_finalize (TrackerWritebackTaglibClass *klass)
{
}

static void
tracker_writeback_taglib_init (TrackerWritebackTaglib *wbt)
{
}

static const gchar * const *
writeback_taglib_content_types (TrackerWritebackFile *wbf)
{
	static const gchar *content_types [] = {
		"audio/x-mpc",
		"audio/x-musepack",
		"audio/mpc",
		"audio/musepack",
		"audio/mpeg",
		"audio/x-mpeg",
		"audio/mp3",
		"audio/x-mp3",
		"audio/mpeg3",
		"audio/x-mpeg3",
		"audio/mpg",
		"audio/x-mpg",
		"audio/x-mpegaudio",
		"audio/flac",
		"audio/mp4",
		"audio/asf",
		"application/asx",
		"video/x-ms-asf-plugin",
		"application/x-mplayer2",
		"video/x-ms-asf",
		"application/vnd.ms-asf",
		"video/x-ms-asf-plugin",
		"video/x-ms-wm",
		"video/x-ms-wmx",
		"audio/aiff",
		"audio/x-aiff",
		"sound/aiff",
		"audio/rmf",
		"audio/x-rmf",
		"audio/x-pn-aiff",
		"audio/x-gsm",
		"audio/mid",
		"audio/x-midi",
		"audio/vnd.qcelp",
		"audio/wav",
		"audio/x-wav",
		"audio/wave",
		"audio/x-pn-wav",
		"audio/tta",
		"audio/x-tta",
		"audio/ogg",
		"application/ogg",
		"audio/x-ogg",
		"application/x-ogg",
		"audio/x-speex",
		NULL
	};

	return content_types;
}

static gboolean
writeback_taglib_update_file_metadata (TrackerWritebackFile     *writeback_file,
                                       GFile                    *file,
                                       GPtrArray                *values,
                                       TrackerSparqlConnection  *connection,
                                       GCancellable            *cancellable)
{
	gboolean ret;
	gchar *path;
	TagLib_File *taglib_file = NULL;
	TagLib_Tag *tag;
	guint n;

	ret = FALSE;
	path = g_file_get_path (file);
	taglib_file = taglib_file_new (path);

	if (!taglib_file || !taglib_file_is_valid (taglib_file)) {
		goto out;
	}

	tag = taglib_file_tag (taglib_file);

	for (n = 0; n < values->len; n++) {
		const GStrv row = g_ptr_array_index (values, n);

		if (g_strcmp0 (row[2], TRACKER_NIE_PREFIX "title") == 0) {
			taglib_tag_set_title (tag, row[3]);
		}

		if (g_strcmp0 (row[2], TRACKER_NMM_PREFIX "performer") == 0) {
			gchar *artist_name = writeback_taglib_get_artist_name (connection, row[3]);

			if (artist_name) {
				taglib_tag_set_artist (tag, artist_name);
				g_free (artist_name);
			}
		}

		if (g_strcmp0 (row[2], TRACKER_NMM_PREFIX "musicAlbum") == 0) {
			gchar *album_name = writeback_taglib_get_album_name (connection, row[3]);

			if (album_name) {
				taglib_tag_set_album (tag, album_name);
				g_free (album_name);
			}
		}

		if (g_strcmp0 (row[2], TRACKER_RDFS_PREFIX "comment") == 0) {
			taglib_tag_set_comment (tag, row[3]);
		}

		if (g_strcmp0 (row[2], TRACKER_NMM_PREFIX "genre") == 0) {
			taglib_tag_set_genre (tag, row[3]);
		}

		if (g_strcmp0 (row[2], TRACKER_NMM_PREFIX "trackNumber") == 0) {
			taglib_tag_set_track (tag, atoi (row[3]));
		}
	}

	taglib_file_save (taglib_file);

	ret = TRUE;

 out:
	g_free (path);
	if (taglib_file) {
		taglib_file_free (taglib_file);
	}

	return ret;
}

static gchar*
writeback_taglib_get_from_query (TrackerSparqlConnection *connection,
                                 const gchar             *urn,
                                 const gchar             *query,
                                 const gchar             *errmsg)
{
	TrackerSparqlCursor *cursor;
	GError *error = NULL;
	gchar *value = NULL;

	cursor = tracker_sparql_connection_query (connection,
	                                          query,
	                                          NULL,
	                                          &error);

	if (error || !cursor || !tracker_sparql_cursor_next (cursor, NULL, NULL)) {
		g_warning ("Couldn't find %s for artist with urn '%s', %s",
		           errmsg,
		           urn,
		           error ? error->message : "no such was found");

		if (error) {
			g_error_free (error);
		}
	} else {
		value = g_strdup (tracker_sparql_cursor_get_string (cursor, 0, NULL));
	}

	g_object_unref (cursor);

	return value;
}


static gchar*
writeback_taglib_get_artist_name (TrackerSparqlConnection *connection,
                                  const gchar             *urn)
{
	gchar *val, *query;

	query = g_strdup_printf ("SELECT ?artistName WHERE {<%s> nmm:artistName ?artistName}",
	                         urn);
	val = writeback_taglib_get_from_query (connection, urn, query, "artist name");
	g_free (query);

	return val;
}

static gchar*
writeback_taglib_get_album_name (TrackerSparqlConnection *connection,
                                 const gchar             *urn)
{
	gchar *val, *query;

	query = g_strdup_printf ("SELECT ?albumName WHERE {<%s> dc:title ?albumName}",
	                         urn);
	val = writeback_taglib_get_from_query (connection, urn, query, "album name");
	g_free (query);

	return val;
}

TrackerWriteback *
writeback_module_create (GTypeModule *module)
{
	tracker_writeback_taglib_register_type (module);

	return g_object_new (TRACKER_TYPE_WRITEBACK_TAGLIB, NULL);
}

const gchar * const *
writeback_module_get_rdf_types (void)
{
	static const gchar *rdftypes[] = {
		TRACKER_NFO_PREFIX "Audio",
		NULL
	};

	return rdftypes;
}
