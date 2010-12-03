/*
 * Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* I don't know why, but this prototype ain't in my math.h */
long long int llroundl(long double x);

#include <glib.h>
#include <glib/gstdio.h>

#define GST_USE_UNSTABLE_API
#include <libgupnp-dlna/gupnp-dlna-discoverer.h>

#include <gst/tag/tag.h>

#include <libtracker-client/tracker.h>

#include <libtracker-extract/tracker-extract.h>

#include "tracker-albumart.h"
#include "tracker-dbus.h"

/* An additional tag in gstreamer for the content source. Remove when in upstream */
#ifndef GST_TAG_CLASSIFICATION
#define GST_TAG_CLASSIFICATION "classification"
#endif

/* Some additional tagreadbin tags (FIXME until they are defined upstream)*/
#ifndef GST_TAG_CHANNEL
#define GST_TAG_CHANNEL "channels"
#endif

#ifndef GST_TAG_RATE
#define GST_TAG_RATE "rate"
#endif

#ifndef GST_TAG_WIDTH
#define GST_TAG_WIDTH "width"
#endif

#ifndef GST_TAG_HEIGHT
#define GST_TAG_HEIGHT "height"
#endif

#ifndef GST_TAG_PIXEL_RATIO
#define GST_TAG_PIXEL_RATIO "pixel-aspect-ratio"
#endif

#ifndef GST_TAG_FRAMERATE
#define GST_TAG_FRAMERATE "framerate"
#endif

#ifndef GST_TAG_DEVICE_MODEL
#define GST_TAG_DEVICE_MODEL "device-model"
#endif

#ifndef GST_TAG_DEVICE_MANUFACTURER
#define GST_TAG_DEVICE_MANUFACTURER "device-manufacturer"
#endif

#ifndef GST_TAG_DEVICE_MAKE
#define GST_TAG_DEVICE_MAKE "device-make"
#endif

static void extract_gupnp_dlna_audio (const gchar          *uri,
                                     TrackerSparqlBuilder *preupdate,
                                     TrackerSparqlBuilder *metadata);
static void extract_gupnp_dlna_video (const gchar          *uri,
                                     TrackerSparqlBuilder *preupdate,
                                     TrackerSparqlBuilder *metadata);
static void extract_gupnp_dlna_image (const gchar          *uri,
                                     TrackerSparqlBuilder *preupdate,
                                     TrackerSparqlBuilder *metadata);
static void extract_gupnp_dlna_guess (const gchar          *uri,
                                     TrackerSparqlBuilder *preupdate,
                                     TrackerSparqlBuilder *metadata);

static TrackerExtractData data[] = {
	{ "audio/*", extract_gupnp_dlna_audio },
	{ "video/*", extract_gupnp_dlna_video },
	{ "image/*", extract_gupnp_dlna_image },
	{ "dlna/*",  extract_gupnp_dlna_guess },
	{ "video/3gpp", extract_gupnp_dlna_guess },
	{ "video/mp4", extract_gupnp_dlna_guess },
	{ "video/x-ms-asf", extract_gupnp_dlna_guess },
	{ NULL, NULL }
};

typedef enum {
	CONTENT_NONE,
	CONTENT_GUESS,
	CONTENT_VIDEO,
	CONTENT_AUDIO,
	CONTENT_IMAGE,
} ContentType;

typedef struct {
	ContentType     content;
	gboolean        has_image;
	gboolean        has_audio;
	gboolean        has_video;
	const gchar    *dlna_profile;
	GstTagList     *tags;
	guint           width;
	guint           height;
	gfloat          frame_rate;
	gfloat          aspect_ratio;
	guint           sample_rate;
	guint           bitrate;
	guint           channels;
	guint           duration;
	gboolean        is_content_encrypted;
	unsigned char  *album_art_data;
	guint           album_art_size;
	const gchar    *album_art_mime;
} MetadataExtractor;

static void
add_int64_info (TrackerSparqlBuilder *metadata,
                const gchar          *uri,
                const gchar          *key,
                gint64                info)
{
	tracker_sparql_builder_predicate (metadata, key);
	tracker_sparql_builder_object_int64 (metadata, info);
}

static void
add_uint_info (TrackerSparqlBuilder *metadata,
               const gchar          *uri,
               const gchar          *key,
               guint                 info)
{
	tracker_sparql_builder_predicate (metadata, key);
	tracker_sparql_builder_object_int64 (metadata, info);
}

static void
add_string_gst_tag (TrackerSparqlBuilder *metadata,
                    const gchar          *uri,
                    const gchar          *key,
                    GstTagList           *tag_list,
                    const gchar          *tag)
{
	gchar *s;
	gboolean ret;

	s = NULL;
	ret = gst_tag_list_get_string (tag_list, tag, &s);

	if (s) {
		if (ret && s[0] != '\0') {
			tracker_sparql_builder_predicate (metadata, key);
			tracker_sparql_builder_object_unvalidated (metadata, s);
		}
		g_free (s);
	}
}

static void
add_uint_gst_tag (TrackerSparqlBuilder *metadata,
                  const gchar          *uri,
                  const gchar          *key,
                  GstTagList           *tag_list,
                  const gchar          *tag)
{
	gboolean ret;
	guint    n;

	ret = gst_tag_list_get_uint (tag_list, tag, &n);

	if (ret) {
		tracker_sparql_builder_predicate (metadata, key);
		tracker_sparql_builder_object_int64 (metadata, n);
	}
}

static void
add_int_gst_tag (TrackerSparqlBuilder *metadata,
                 const gchar          *uri,
                 const gchar          *key,
                 GstTagList           *tag_list,
                 const gchar          *tag)
{
	gboolean ret;
	gint n;

	ret = gst_tag_list_get_int (tag_list, tag, &n);

	if (ret) {
		tracker_sparql_builder_predicate (metadata, key);
		tracker_sparql_builder_object_int64 (metadata, n);
	}
}

static void
add_double_gst_tag (TrackerSparqlBuilder *metadata,
                    const gchar          *uri,
                    const gchar          *key,
                    GstTagList           *tag_list,
                    const gchar          *tag)
{
	gboolean ret;
	gdouble n;

	ret = gst_tag_list_get_double (tag_list, tag, &n);

	if (ret) {
		tracker_sparql_builder_predicate (metadata, key);
		tracker_sparql_builder_object_int64 (metadata, (gint64) n);
	}
}

static void
add_fraction_gst_tag (TrackerSparqlBuilder *metadata,
                      const gchar          *uri,
                      const gchar          *key,
                      GstTagList           *tag_list,
                      const gchar          *tag)
{
	gboolean ret;
	GValue n = {0,};
	gfloat f;

	ret = gst_tag_list_copy_value (&n, tag_list, tag);

	if (ret) {
		f = (gfloat)gst_value_get_fraction_numerator (&n)/
		gst_value_get_fraction_denominator (&n);

		tracker_sparql_builder_predicate (metadata, key);
		tracker_sparql_builder_object_double (metadata, (gdouble) f);

		g_value_unset (&n);
	}
}

static void
add_y_date_gst_tag (TrackerSparqlBuilder  *metadata,
                    const gchar           *uri,
                    const gchar           *key,
                    GstTagList            *tag_list,
                    const gchar           *tag)
{
	GDate *date;
	gboolean ret;

	date = NULL;
	ret = gst_tag_list_get_date (tag_list, tag, &date);

	if (ret) {
		if (date && g_date_valid (date)) {
			if (date->julian)
				ret = g_date_valid_julian (date->julian_days);
			if (date->dmy)
				ret = g_date_valid_dmy (date->day, date->month, date->year);
		} else
			ret = FALSE;
	}

	if (ret) {
		gchar buf[25];

		if (g_date_strftime (buf, 25, "%Y-%m-%dT%H:%M:%S%z", date)) {
			tracker_sparql_builder_predicate (metadata, key);
			tracker_sparql_builder_object_unvalidated (metadata, buf);
		}
	}

	if (date) {
		g_date_free (date);
	}
}

static void
add_time_gst_tag (TrackerSparqlBuilder *metadata,
                  const gchar          *uri,
                  const gchar          *key,
                  GstTagList           *tag_list,
                  const gchar          *tag)
{
	gboolean ret;
	guint64 n;

	ret = gst_tag_list_get_uint64 (tag_list, tag, &n);

	if (ret) {
		gint64 duration;

		duration = llroundl ((long double) n / (long double) GST_SECOND);

		tracker_sparql_builder_predicate (metadata, key);
		tracker_sparql_builder_object_int64 (metadata, duration);
	}
}

static void
add_keywords_gst_tag (TrackerSparqlBuilder *metadata,
                      GstTagList           *tag_list)
{
	gboolean ret;
	gchar *str;

	ret = gst_tag_list_get_string (tag_list, GST_TAG_KEYWORDS, &str);

	if (ret) {
		GStrv keywords;
		gint i = 0;

		keywords = g_strsplit_set (str, " ,", -1);

		while (keywords[i]) {
			tracker_sparql_builder_predicate (metadata, "nie:keyword");
			tracker_sparql_builder_object_unvalidated (metadata, g_strstrip (keywords[i]));
			i++;
		}

		g_strfreev (keywords);
		g_free (str);
	}
}

static void
extract_metadata (MetadataExtractor      *extractor,
                  const gchar            *uri,
                  TrackerSparqlBuilder   *preupdate,
                  TrackerSparqlBuilder   *metadata,
                  gchar                 **artist,
                  gchar                 **album,
                  gchar                 **scount)
{
	const gchar *temp;
	gchar *make = NULL, *model = NULL, *manuf = NULL;
	gchar *albumname = NULL, *composer = NULL, *genre = NULL;
	gboolean ret;
	gint count;
	gboolean needs_audio = FALSE;

	g_return_if_fail (metadata != NULL);

	if (extractor->tags) {
		gchar *artist_uri = NULL;
		gchar *performer_uri = NULL;
		gchar *composer_uri = NULL;
		gchar *album_uri = NULL;
		gchar *album_disc_uri = NULL;

		/* General */
		if (extractor->content == CONTENT_AUDIO || extractor->content == CONTENT_VIDEO) {
			gchar *performer = NULL, *artist_local = NULL;

			gst_tag_list_get_string (extractor->tags, GST_TAG_PERFORMER, &performer);
			gst_tag_list_get_string (extractor->tags, GST_TAG_ARTIST, &artist_local);

			if (artist_local) {
				artist_uri = tracker_sparql_escape_uri_printf ("urn:artist:%s", artist_local);

				tracker_sparql_builder_insert_open (preupdate, NULL);
				tracker_sparql_builder_subject_iri (preupdate, artist_uri);
				tracker_sparql_builder_predicate (preupdate, "a");
				tracker_sparql_builder_object (preupdate, "nmm:Artist");
				tracker_sparql_builder_predicate (preupdate, "nmm:artistName");
				tracker_sparql_builder_object_unvalidated (preupdate, artist_local);
				tracker_sparql_builder_insert_close (preupdate);
			}

			temp = tracker_coalesce_strip (2, performer, artist_local);

			if (temp) {
				performer_uri = tracker_sparql_escape_uri_printf ("urn:artist:%s", temp);

				tracker_sparql_builder_insert_open (preupdate, NULL);
				tracker_sparql_builder_subject_iri (preupdate, performer_uri);
				tracker_sparql_builder_predicate (preupdate, "a");
				tracker_sparql_builder_object (preupdate, "nmm:Artist");
				tracker_sparql_builder_predicate (preupdate, "nmm:artistName");
				tracker_sparql_builder_object_unvalidated (preupdate, temp);
				tracker_sparql_builder_insert_close (preupdate);

				*artist = g_strdup (temp);
			}

			g_free (performer);
			g_free (artist_local);

			gst_tag_list_get_string (extractor->tags, GST_TAG_COMPOSER, &composer);

			if (composer) {
				composer_uri = tracker_sparql_escape_uri_printf ("urn:artist:%s", composer);

				tracker_sparql_builder_insert_open (preupdate, NULL);

				tracker_sparql_builder_subject_iri (preupdate, composer_uri);
				tracker_sparql_builder_predicate (preupdate, "a");
				tracker_sparql_builder_object (preupdate, "nmm:Artist");

				tracker_sparql_builder_predicate (preupdate, "nmm:artistName");
				tracker_sparql_builder_object_unvalidated (preupdate, composer);

				tracker_sparql_builder_insert_close (preupdate);

				g_free (composer);
			}

		}

		/* Audio */
		gst_tag_list_get_string (extractor->tags, GST_TAG_ALBUM, &albumname);
		if (albumname) {
			gboolean has_it;
			guint count;
			gdouble gain;

			needs_audio = TRUE;

			album_uri = tracker_sparql_escape_uri_printf ("urn:album:%s", albumname);

			tracker_sparql_builder_insert_open (preupdate, NULL);

			tracker_sparql_builder_subject_iri (preupdate, album_uri);
			tracker_sparql_builder_predicate (preupdate, "a");
			tracker_sparql_builder_object (preupdate, "nmm:MusicAlbum");
			/* FIXME: nmm:albumTitle is now deprecated
			 * tracker_sparql_builder_predicate (preupdate, "nie:title");
			 */
			tracker_sparql_builder_predicate (preupdate, "nmm:albumTitle");
			tracker_sparql_builder_object_unvalidated (preupdate, albumname);

			if (artist_uri) {
				tracker_sparql_builder_predicate (preupdate, "nmm:albumArtist");
				tracker_sparql_builder_object_iri (preupdate, artist_uri);
			}

			tracker_sparql_builder_insert_close (preupdate);

			has_it = gst_tag_list_get_uint (extractor->tags,
			                                GST_TAG_TRACK_COUNT,
			                                &count);

			if (has_it) {
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
				tracker_sparql_builder_object_int64 (preupdate, count);

				tracker_sparql_builder_insert_close (preupdate);
			}

			has_it = gst_tag_list_get_uint (extractor->tags,
			                                GST_TAG_ALBUM_VOLUME_NUMBER,
			                                &count);

			if (has_it) {
				album_disc_uri = tracker_sparql_escape_uri_printf ("urn:album-disc:%s/%d",
				                                                   albumname, count);

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
				tracker_sparql_builder_object_int64 (preupdate, count);
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
			}

			has_it = gst_tag_list_get_double (extractor->tags,
			                                  GST_TAG_ALBUM_GAIN,
			                                  &gain);

			if (has_it) {
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
				tracker_sparql_builder_object_double (preupdate, gain);

				tracker_sparql_builder_insert_close (preupdate);
			}

			has_it = gst_tag_list_get_double (extractor->tags,
			                                  GST_TAG_ALBUM_PEAK,
			                                  &gain);

			if (has_it) {
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
				tracker_sparql_builder_object_double (preupdate, gain);

				tracker_sparql_builder_insert_close (preupdate);
			}

			*album = albumname;

		}

		if (extractor->content == CONTENT_AUDIO)
			needs_audio = TRUE;

		tracker_sparql_builder_predicate (metadata, "a");

		if (needs_audio) {
			tracker_sparql_builder_object (metadata, "nmm:MusicPiece");
			tracker_sparql_builder_object (metadata, "nfo:Audio");
		}

		if (extractor->content == CONTENT_VIDEO) {
			tracker_sparql_builder_object (metadata, "nmm:Video");
		} else if (!needs_audio) {
			tracker_sparql_builder_object (metadata, "nfo:Image");
		}

		gst_tag_list_get_string (extractor->tags, GST_TAG_GENRE, &genre);
		if (g_strcmp0 (genre, "Unknown") != 0) {
			tracker_sparql_builder_predicate (metadata, "nfo:genre");
			tracker_sparql_builder_object_unvalidated (metadata, genre);
		}
		g_free (genre);

		add_string_gst_tag (metadata, uri, "nie:title", extractor->tags, GST_TAG_TITLE);
		add_string_gst_tag (metadata, uri, "nie:copyright", extractor->tags, GST_TAG_COPYRIGHT);
		add_string_gst_tag (metadata, uri, "nie:license", extractor->tags, GST_TAG_LICENSE);
		add_string_gst_tag (metadata, uri, "dc:coverage", extractor->tags, GST_TAG_LOCATION);
		add_y_date_gst_tag (metadata, uri, "nie:contentCreated", extractor->tags, GST_TAG_DATE);
		add_string_gst_tag (metadata, uri, "nie:comment", extractor->tags, GST_TAG_COMMENT);

		gst_tag_list_get_string (extractor->tags, GST_TAG_DEVICE_MODEL, &model);
		gst_tag_list_get_string (extractor->tags, GST_TAG_DEVICE_MAKE, &make);
		gst_tag_list_get_string (extractor->tags, GST_TAG_DEVICE_MANUFACTURER, &manuf);

		if (make || model || manuf) {
			gchar *equip_uri;

			equip_uri = tracker_sparql_escape_uri_printf ("urn:equipment:%s:%s:",
			                                              manuf ? manuf : make ? make : "",
			                                              model ? model : "");

			tracker_sparql_builder_insert_open (preupdate, NULL);
			tracker_sparql_builder_subject_iri (preupdate, equip_uri);
			tracker_sparql_builder_predicate (preupdate, "a");
			tracker_sparql_builder_object (preupdate, "nfo:Equipment");

			if (make || manuf) {
				tracker_sparql_builder_predicate (preupdate, "nfo:manufacturer");
				tracker_sparql_builder_object_unvalidated (preupdate, manuf ? manuf : make);
			}
			if (model) {
				tracker_sparql_builder_predicate (preupdate, "nfo:model");
				tracker_sparql_builder_object_unvalidated (preupdate, model);
			}
			tracker_sparql_builder_insert_close (preupdate);

			tracker_sparql_builder_predicate (metadata, "nfo:equipment");
			tracker_sparql_builder_object_iri (metadata, equip_uri);
			g_free (equip_uri);
		}

		g_free (make);
		g_free (model);
		g_free (manuf);

		/* FIXME add this stuff */
		/* if (extractor->is_content_encrypted) { */
		/* 	tracker_sparql_builder_predicate (metadata, "nfo:isContentEncrypted"); */
		/* 	tracker_sparql_builder_object_boolean (metadata, TRUE); */
		/* } */

		if (extractor->content == CONTENT_VIDEO) {
			add_string_gst_tag (metadata, uri, "dc:source", extractor->tags, GST_TAG_CLASSIFICATION);

			if (performer_uri) {
				tracker_sparql_builder_predicate (metadata, "nmm:leadActor");
				tracker_sparql_builder_object_iri (metadata, performer_uri);
			}

			if (composer_uri) {
				tracker_sparql_builder_predicate (metadata, "nmm:director");
				tracker_sparql_builder_object_iri (metadata, composer_uri);
			}

			add_keywords_gst_tag (metadata, extractor->tags);
		}

		if (needs_audio) {
			/* Audio */
			ret = gst_tag_list_get_uint (extractor->tags, GST_TAG_TRACK_COUNT, &count);
			if (ret) {
				*scount = g_strdup_printf ("%d", count);
			}

			add_uint_gst_tag   (metadata, uri, "nmm:trackNumber", extractor->tags, GST_TAG_TRACK_NUMBER);

			add_double_gst_tag (metadata, uri, "nfo:gain", extractor->tags, GST_TAG_TRACK_GAIN);
			add_double_gst_tag (metadata, uri, "nfo:peakGain", extractor->tags, GST_TAG_TRACK_PEAK);

			if (performer_uri) {
				tracker_sparql_builder_predicate (metadata, "nmm:performer");
				tracker_sparql_builder_object_iri (metadata, performer_uri);
			}

			if (composer_uri) {
				tracker_sparql_builder_predicate (metadata, "nmm:composer");
				tracker_sparql_builder_object_iri (metadata, composer_uri);
			}

			if (album_uri) {
				tracker_sparql_builder_predicate (metadata, "nmm:musicAlbum");
				tracker_sparql_builder_object_iri (metadata, album_uri);
			}

			if (album_disc_uri) {
				tracker_sparql_builder_predicate (metadata, "nmm:musicAlbumDisk");
				tracker_sparql_builder_object_iri (metadata, album_disc_uri);
			}
		}

		g_free (artist_uri);
		g_free (performer_uri);
		g_free (composer_uri);
		g_free (album_uri);

		add_string_gst_tag (metadata, uri, "nfo:codec", extractor->tags, GST_TAG_AUDIO_CODEC);
	} else {
		if (extractor->content == CONTENT_AUDIO)
			needs_audio = TRUE;

		tracker_sparql_builder_predicate (metadata, "a");

		if (needs_audio) {
			tracker_sparql_builder_object (metadata, "nmm:MusicPiece");
			tracker_sparql_builder_object (metadata, "nfo:Audio");
		}

		if (extractor->content == CONTENT_VIDEO) {
			tracker_sparql_builder_object (metadata, "nmm:Video");
		} else if (extractor->content == CONTENT_IMAGE) {
			tracker_sparql_builder_object (metadata, "nfo:Image");
		}
	}

	if (extractor->content != CONTENT_IMAGE) {
		if (extractor->channels) {
			tracker_sparql_builder_predicate (metadata, "nfo.channels");
			tracker_sparql_builder_object_int64 (metadata, extractor->channels);
		}

		if (extractor->sample_rate) {
			tracker_sparql_builder_predicate (metadata, "nfo:sampleRate");
			tracker_sparql_builder_object_int64 (metadata, extractor->sample_rate);
		}

		if (extractor->duration) {
			tracker_sparql_builder_predicate (metadata, "nfo:duration");
			tracker_sparql_builder_object_int64 (metadata, extractor->duration);
		}
	}

	if (extractor->content == CONTENT_VIDEO) {
		if (extractor->width) {
			tracker_sparql_builder_predicate (metadata, "nfo:width");
			tracker_sparql_builder_object_int64 (metadata, extractor->width);
		}

		if (extractor->height) {
			tracker_sparql_builder_predicate (metadata, "nfo:height");
			tracker_sparql_builder_object_int64 (metadata, extractor->height);
		}

		if (extractor->frame_rate) {
			tracker_sparql_builder_predicate (metadata, "nfo:frameRate");
			tracker_sparql_builder_object_double (metadata, extractor->frame_rate);
		}

		if (extractor->aspect_ratio) {
			tracker_sparql_builder_predicate (metadata, "nfo:aspectRatio");
			tracker_sparql_builder_object_double (metadata, extractor->aspect_ratio);
		}
	}

	if (extractor->dlna_profile) {
		tracker_sparql_builder_predicate (metadata, "nmm:dlnaProfile");
		tracker_sparql_builder_object_string (metadata, extractor->dlna_profile);
	}
}

static void
extract_gupnp_dlna (const gchar           *uri,
                    TrackerSparqlBuilder  *preupdate,
                    TrackerSparqlBuilder  *metadata,
                    ContentType            content)
{
	GUPnPDLNADiscoverer  *discoverer = NULL;
	GUPnPDLNAInformation *dlna_info  = NULL;
	GError               *error      = NULL;
	MetadataExtractor    extractor;

	gst_init (NULL, NULL);

	extractor.has_image = FALSE;
	extractor.has_video = FALSE;
	extractor.has_audio = FALSE;

	extractor.tags      = NULL;

	extractor.width = 0;
	extractor.height = 0;
	extractor.frame_rate = 0;
	extractor.aspect_ratio = 0;

	extractor.sample_rate = 0;
	extractor.bitrate = 0;
	extractor.channels = 0;

	extractor.duration = 0;

	extractor.is_content_encrypted = FALSE;

	extractor.album_art_data = NULL;
	extractor.album_art_size = 0;
	extractor.album_art_mime = NULL;

	discoverer = gupnp_dlna_discoverer_new (5*GST_SECOND, TRUE, FALSE);
	dlna_info = gupnp_dlna_discoverer_discover_uri_sync (discoverer,
							     uri,
							     &error);

	if (error) {
		g_warning ("Error in gst-discovery: %s", error->message);

		g_error_free (error);
		return;
	}

	if (dlna_info) {
		GstDiscovererInformation *info     = NULL;
		GList                    *iter;

		gchar *artist, *album, *scount;

		album  = NULL;
		scount = NULL;
		artist = NULL;

		info = gupnp_dlna_information_get_info (dlna_info);

		iter = g_list_first (info->stream_list);

		while (iter != NULL) {
			GstStreamInformation *stream   = NULL;
			GstTagList           *tmp      = NULL;

			stream = iter->data;

			if (stream->misc == NULL) {
				g_debug ("misc NULL");
			}

			switch (stream->streamtype) {
			case GST_STREAM_AUDIO:
				extractor.has_audio = TRUE;
				extractor.sample_rate = ((GstStreamAudioInformation *)stream)->sample_rate;
				extractor.bitrate     = ((GstStreamAudioInformation *)stream)->bitrate;
				extractor.channels    = ((GstStreamAudioInformation *)stream)->channels;
				break;
			case GST_STREAM_VIDEO:
				extractor.has_video    = TRUE;
				extractor.frame_rate   = (gfloat)gst_value_get_fraction_numerator (&((GstStreamVideoInformation *)stream)->frame_rate)/
				                            gst_value_get_fraction_denominator (&((GstStreamVideoInformation *)stream)->frame_rate);
				extractor.width        = ((GstStreamVideoInformation *)stream)->width;
				extractor.height       = ((GstStreamVideoInformation *)stream)->height;
				extractor.aspect_ratio = (gfloat)gst_value_get_fraction_numerator (&((GstStreamVideoInformation *)stream)->pixel_aspect_ratio)/
				                            gst_value_get_fraction_denominator (&((GstStreamVideoInformation *)stream)->pixel_aspect_ratio);
				break;
			case GST_STREAM_IMAGE:
				extractor.has_image = TRUE;
				break;
			default:
				break;
			}

			tmp = gst_tag_list_merge (extractor.tags, stream->tags, GST_TAG_MERGE_APPEND);
			if (extractor.tags) {
				gst_tag_list_free (extractor.tags);
			}
			extractor.tags = tmp;

			iter = g_list_next (iter);
		}

		extractor.duration = info->duration / GST_SECOND;

		extractor.dlna_profile = gupnp_dlna_information_get_name (dlna_info);

		if (content != CONTENT_GUESS) {
			extractor.content = content;
		} else {
			if (extractor.has_video) {
				extractor.content = CONTENT_VIDEO;
			} else if (extractor.has_audio) {
				extractor.content = CONTENT_AUDIO;
			} else if (extractor.has_image) {
				extractor.content = CONTENT_IMAGE;
			} else {
				extractor.content = CONTENT_NONE;
			}
		}

		extract_metadata (&extractor, uri, preupdate, metadata, &artist, &album, &scount);

		gst_tag_list_free (extractor.tags);
		g_free (artist);
		g_free (album);
		g_free (scount);

	}

	g_object_unref (discoverer);
	g_object_unref (dlna_info);
}

static void
extract_gupnp_dlna_audio (const gchar          *uri,
                          TrackerSparqlBuilder *preupdate,
                          TrackerSparqlBuilder *metadata)
{
	extract_gupnp_dlna (uri, preupdate, metadata, CONTENT_AUDIO);
}

static void
extract_gupnp_dlna_video (const gchar          *uri,
                          TrackerSparqlBuilder *preupdate,
                          TrackerSparqlBuilder *metadata)
{
	extract_gupnp_dlna (uri, preupdate, metadata, CONTENT_VIDEO);
}

static void
extract_gupnp_dlna_image (const gchar          *uri,
                          TrackerSparqlBuilder *preupdate,
                          TrackerSparqlBuilder *metadata)
{
	extract_gupnp_dlna (uri, preupdate, metadata, CONTENT_IMAGE);
}

static void
extract_gupnp_dlna_guess (const gchar          *uri,
                          TrackerSparqlBuilder *preupdate,
                          TrackerSparqlBuilder *metadata)
{
	extract_gupnp_dlna (uri, preupdate, metadata, CONTENT_GUESS);
}


TrackerExtractData *
tracker_extract_get_data (void)
{
	return data;
}

