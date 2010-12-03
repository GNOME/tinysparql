/*
 * Copyright (C) 2006, Laurent Aguerreche <laurent.aguerreche@free.fr>
 * Copyright (C) 2007, Jamie McCracken <jamiemcc@gnome.org>
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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

#include <glib.h>
#include <glib/gstdio.h>

#include <gst/gst.h>
#include <gst/tag/tag.h>

#include <libtracker-extract/tracker-extract.h>

#include "tracker-albumart.h"
#include "tracker-dbus.h"

/* We wait this long (seconds) for NULL state before freeing */
#define TRACKER_EXTRACT_GUARD_TIMEOUT 3

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


typedef enum {
	EXTRACT_MIME_AUDIO,
	EXTRACT_MIME_VIDEO,
	EXTRACT_MIME_IMAGE,
	EXTRACT_MIME_GUESS,
} ExtractMime;

typedef struct {
	/* Common pipeline elements */
	GstElement     *pipeline;

	GstBus         *bus;

	ExtractMime     mime;


	/* Decodebin elements and properties*/
	GstElement     *bin;
	GList          *fsinks;

	gint64          duration;
	gint            video_height;
	gint            video_width;
	gint            video_fps_n;
	gint            video_fps_d;
	gint            audio_channels;
	gint            audio_samplerate;
	gint            aspect_ratio;

	/* Tags and data */
	GstTagList     *tagcache;

	gboolean        is_content_encrypted;

	unsigned char  *album_art_data;
	guint           album_art_size;
	const gchar    *album_art_mime;

} MetadataExtractor;

static void extract_gstreamer_audio (const gchar          *uri,
                                     TrackerSparqlBuilder *preupdate,
                                     TrackerSparqlBuilder *metadata);
static void extract_gstreamer_video (const gchar          *uri,
                                     TrackerSparqlBuilder *preupdate,
                                     TrackerSparqlBuilder *metadata);
static void extract_gstreamer_image (const gchar          *uri,
                                     TrackerSparqlBuilder *preupdate,
                                     TrackerSparqlBuilder *metadata);
static void extract_gstreamer_guess (const gchar          *uri,
                                     TrackerSparqlBuilder *preupdate,
                                     TrackerSparqlBuilder *metadata);

static TrackerExtractData data[] = {
	{ "audio/*", extract_gstreamer_audio },
	{ "video/*", extract_gstreamer_video },
	{ "image/*", extract_gstreamer_image },
	/* Tell gstreamer to guess if mimetype guessing returns video also for audio files */
	{ "video/3gpp", extract_gstreamer_guess },
	{ "video/mp4", extract_gstreamer_guess },
	{ "video/x-ms-asf", extract_gstreamer_guess },
	{ NULL, NULL }
};

/* Not using define directly since we might want to make this dynamic */
#ifdef TRACKER_EXTRACT_GSTREAMER_USE_TAGREADBIN
const gboolean use_tagreadbin = TRUE;
#else
const gboolean use_tagreadbin = FALSE;
#endif

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
	gchar    *s;
	gboolean  ret;

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
add_uint_gst_tag (TrackerSparqlBuilder  *metadata,
                  const gchar           *uri,
                  const gchar           *key,
                  GstTagList            *tag_list,
                  const gchar           *tag)
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
add_int_gst_tag (TrackerSparqlBuilder  *metadata,
                 const gchar           *uri,
                 const gchar           *key,
                 GstTagList            *tag_list,
                 const gchar           *tag)
{
	gboolean ret;
	gint     n;

	ret = gst_tag_list_get_int (tag_list, tag, &n);

	if (ret) {
		tracker_sparql_builder_predicate (metadata, key);
		tracker_sparql_builder_object_int64 (metadata, n);
	}
}

static void
add_double_gst_tag (TrackerSparqlBuilder  *metadata,
                    const gchar           *uri,
                    const gchar           *key,
                    GstTagList            *tag_list,
                    const gchar           *tag)
{
	gboolean ret;
	gdouble          n;

	ret = gst_tag_list_get_double (tag_list, tag, &n);

	if (ret) {
		tracker_sparql_builder_predicate (metadata, key);
		tracker_sparql_builder_object_int64 (metadata, (gint64) n);
	}
}

static void
add_fraction_gst_tag (TrackerSparqlBuilder  *metadata,
                      const gchar           *uri,
                      const gchar           *key,
                      GstTagList            *tag_list,
                      const gchar           *tag)
{
	gboolean ret;
	GValue   n = {0,};
	gfloat   f;

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
	GDate    *date;
	gboolean  ret;

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
add_time_gst_tag (TrackerSparqlBuilder   *metadata,
                  const gchar *uri,
                  const gchar *key,
                  GstTagList  *tag_list,
                  const gchar *tag)
{
	gboolean ret;
	guint64          n;

	ret = gst_tag_list_get_uint64 (tag_list, tag, &n);

	if (ret) {
		gint64 duration;

		duration = (n + (GST_SECOND / 2)) / GST_SECOND;

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

static gboolean
get_embedded_album_art(MetadataExtractor *extractor)
{
	const GValue *value;
	guint         lindex;

	lindex = 0;

	do {
		value = gst_tag_list_get_value_index (extractor->tagcache, GST_TAG_IMAGE, lindex);

		if (value) {
			GstBuffer    *buffer;
			GstCaps      *caps;
			GstStructure *caps_struct;
			gint          type;

			buffer = gst_value_get_buffer (value);
			caps   = gst_buffer_get_caps (buffer);
			caps_struct = gst_caps_get_structure (buffer->caps, 0);

			gst_structure_get_enum (caps_struct,
			                        "image-type",
			                        GST_TYPE_TAG_IMAGE_TYPE,
			                        &type);

			if ((type == GST_TAG_IMAGE_TYPE_FRONT_COVER)||
			    ((type == GST_TAG_IMAGE_TYPE_UNDEFINED)&&(extractor->album_art_size == 0))) {
				extractor->album_art_data = buffer->data;
				extractor->album_art_size = buffer->size;
				extractor->album_art_mime = gst_structure_get_name (caps_struct);
				gst_caps_unref (caps);
				return TRUE;
			}

			gst_caps_unref (caps);

			lindex++;
		}
	} while (value);

	value = gst_tag_list_get_value_index (extractor->tagcache, GST_TAG_PREVIEW_IMAGE, lindex);

	if (value) {
		GstBuffer    *buffer;
		GstStructure *caps_struct;

		buffer = gst_value_get_buffer (value);
		caps_struct = gst_caps_get_structure (buffer->caps, 0);

		extractor->album_art_data = buffer->data;
		extractor->album_art_size = buffer->size;
		extractor->album_art_mime = gst_structure_get_name (caps_struct);


		return TRUE;
	}

	return FALSE;
}

static void
extract_stream_metadata_tagreadbin (MetadataExtractor     *extractor,
                                    const gchar           *uri,
                                    TrackerSparqlBuilder  *metadata)
{
	if (extractor->mime != EXTRACT_MIME_IMAGE) {
		add_int_gst_tag (metadata, uri, "nfo:channels", extractor->tagcache, GST_TAG_CHANNEL);
		add_int_gst_tag (metadata, uri, "nfo:sampleRate", extractor->tagcache, GST_TAG_RATE);
		add_time_gst_tag (metadata, uri, "nfo:duration", extractor->tagcache, GST_TAG_DURATION);
	}

	if (extractor->mime == EXTRACT_MIME_IMAGE || extractor->mime == EXTRACT_MIME_VIDEO) {
		add_fraction_gst_tag (metadata, uri, "nfo:aspectRatio", extractor->tagcache, GST_TAG_PIXEL_RATIO);
	}

	add_int_gst_tag (metadata, uri, "nfo:height", extractor->tagcache, GST_TAG_HEIGHT);
	add_int_gst_tag (metadata, uri, "nfo:width", extractor->tagcache, GST_TAG_WIDTH);

	if (extractor->mime == EXTRACT_MIME_VIDEO) {
		add_fraction_gst_tag (metadata, uri, "nfo:frameRate", extractor->tagcache, GST_TAG_FRAMERATE);
	}
}

static void
extract_stream_metadata_decodebin (MetadataExtractor *extractor,
                                   const gchar       *uri,
                                   TrackerSparqlBuilder         *metadata)
{
	if (extractor->mime != EXTRACT_MIME_IMAGE) {

		if (extractor->audio_channels >= 0) {
			add_uint_info (metadata, uri, "nfo:channels", extractor->audio_channels);
		}

		if (extractor->audio_samplerate >= 0) {
			add_uint_info (metadata, uri, "nfo:sampleRate", extractor->audio_samplerate);
		}

		if (extractor->duration >= 0) {
			add_int64_info (metadata, uri, "nfo:duration", extractor->duration);
		}
	} else {
		if (extractor->aspect_ratio >= 0) {
			add_uint_info (metadata, uri, "nfo:aspectRatio", extractor->aspect_ratio);
		}
	}

	if (extractor->video_height >= 0) {
		add_uint_info (metadata, uri, "nfo:height", extractor->video_height);
	}

	if (extractor->video_width >= 0) {
		add_uint_info (metadata, uri, "nfo:width", extractor->video_width);
	}

	if (extractor->mime == EXTRACT_MIME_VIDEO) {
		if (extractor->video_fps_n >= 0 && extractor->video_fps_d >= 0) {
			add_uint_info (metadata,
			               uri, "nfo:frameRate",
			               ((extractor->video_fps_n + extractor->video_fps_d / 2) /
			                extractor->video_fps_d));
		}
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
	gchar *composer = NULL, *albumname = NULL, *genre = NULL;
	gboolean ret;
	gint count;
	gboolean needs_audio = FALSE;

	g_return_if_fail (extractor != NULL);
	g_return_if_fail (metadata != NULL);

	if (extractor->tagcache) {
		gchar *artist_uri = NULL;
		gchar *performer_uri = NULL;
		gchar *composer_uri = NULL;
		gchar *album_uri = NULL;
		gchar *album_disc_uri = NULL;

		if (extractor->mime == EXTRACT_MIME_GUESS) {
			gchar *video_codec = NULL, *audio_codec = NULL;

			gst_tag_list_get_string (extractor->tagcache, GST_TAG_VIDEO_CODEC, &video_codec);
			gst_tag_list_get_string (extractor->tagcache, GST_TAG_AUDIO_CODEC, &audio_codec);

			if (audio_codec && !video_codec) {
				extractor->mime = EXTRACT_MIME_AUDIO;
			} else {
				/* default to video */
				extractor->mime = EXTRACT_MIME_VIDEO;
			}

			g_free (video_codec);
			g_free (audio_codec);
		}

		/* General */
		if (extractor->mime == EXTRACT_MIME_AUDIO || extractor->mime == EXTRACT_MIME_VIDEO) {
			gchar *performer = NULL, *artist_local = NULL;

			gst_tag_list_get_string (extractor->tagcache, GST_TAG_PERFORMER, &performer);
			gst_tag_list_get_string (extractor->tagcache, GST_TAG_ARTIST, &artist_local);

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

			gst_tag_list_get_string (extractor->tagcache, GST_TAG_COMPOSER, &composer);

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

		if (extractor->mime == EXTRACT_MIME_AUDIO) {
			gst_tag_list_get_string (extractor->tagcache, GST_TAG_ALBUM, &albumname);
		}

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

			has_it = gst_tag_list_get_uint (extractor->tagcache,
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

			has_it = gst_tag_list_get_uint (extractor->tagcache,
			                                GST_TAG_ALBUM_VOLUME_NUMBER,
			                                &count);

			if (has_it) {
				album_disc_uri = tracker_sparql_escape_uri_printf ("urn:album-disc:%s:Disc%d",
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

			has_it = gst_tag_list_get_double (extractor->tagcache,
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

			has_it = gst_tag_list_get_double (extractor->tagcache,
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

		if (extractor->mime == EXTRACT_MIME_AUDIO)
			needs_audio = TRUE;

		tracker_sparql_builder_predicate (metadata, "a");

		if (needs_audio) {
			tracker_sparql_builder_object (metadata, "nmm:MusicPiece");
			tracker_sparql_builder_object (metadata, "nfo:Audio");
		}

		if (extractor->mime == EXTRACT_MIME_VIDEO) {
			tracker_sparql_builder_object (metadata, "nmm:Video");
		} else if (!needs_audio) {
			tracker_sparql_builder_object (metadata, "nfo:Image");
			tracker_sparql_builder_object (metadata, "nmm:Photo");
		}

		gst_tag_list_get_string (extractor->tagcache, GST_TAG_GENRE, &genre);
		if (g_strcmp0 (genre, "Unknown") != 0) {
			tracker_sparql_builder_predicate (metadata, "nfo:genre");
			tracker_sparql_builder_object_unvalidated (metadata, genre);
		}
		g_free (genre);

		add_string_gst_tag (metadata, uri, "nie:title", extractor->tagcache, GST_TAG_TITLE);
		add_string_gst_tag (metadata, uri, "nie:copyright", extractor->tagcache, GST_TAG_COPYRIGHT);
		add_string_gst_tag (metadata, uri, "nie:license", extractor->tagcache, GST_TAG_LICENSE);
		add_string_gst_tag (metadata, uri, "dc:coverage", extractor->tagcache, GST_TAG_LOCATION);
		add_y_date_gst_tag (metadata, uri, "nie:contentCreated", extractor->tagcache, GST_TAG_DATE);
		add_string_gst_tag (metadata, uri, "nie:comment", extractor->tagcache, GST_TAG_COMMENT);

		gst_tag_list_get_string (extractor->tagcache, GST_TAG_DEVICE_MODEL, &model);
		gst_tag_list_get_string (extractor->tagcache, GST_TAG_DEVICE_MAKE, &make);
		gst_tag_list_get_string (extractor->tagcache, GST_TAG_DEVICE_MANUFACTURER, &manuf);

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

		if (extractor->is_content_encrypted) {
			tracker_sparql_builder_predicate (metadata, "nfo:isContentEncrypted");
			tracker_sparql_builder_object_boolean (metadata, TRUE);
		}

		if (extractor->mime == EXTRACT_MIME_VIDEO) {
			add_string_gst_tag (metadata, uri, "dc:source", extractor->tagcache, GST_TAG_CLASSIFICATION);

			if (performer_uri) {
				tracker_sparql_builder_predicate (metadata, "nmm:leadActor");
				tracker_sparql_builder_object_iri (metadata, performer_uri);
			}

			if (composer_uri) {
				tracker_sparql_builder_predicate (metadata, "nmm:director");
				tracker_sparql_builder_object_iri (metadata, composer_uri);
			}

			add_keywords_gst_tag (metadata, extractor->tagcache);
		}

		if (needs_audio) {
			/* Audio */
			ret = gst_tag_list_get_uint (extractor->tagcache, GST_TAG_TRACK_COUNT, &count);
			if (ret) {
				*scount = g_strdup_printf ("%d", count);
			}

			add_uint_gst_tag   (metadata, uri, "nmm:trackNumber", extractor->tagcache, GST_TAG_TRACK_NUMBER);

			add_double_gst_tag (metadata, uri, "nfo:gain", extractor->tagcache, GST_TAG_TRACK_GAIN);
			add_double_gst_tag (metadata, uri, "nfo:peakGain", extractor->tagcache, GST_TAG_TRACK_PEAK);

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

		g_free (performer_uri);
		g_free (composer_uri);
		g_free (album_uri);
		g_free (album_disc_uri);

		add_string_gst_tag (metadata, uri, "nfo:codec", extractor->tagcache, GST_TAG_AUDIO_CODEC);
	} else {
		if (extractor->mime == EXTRACT_MIME_AUDIO)
			needs_audio = TRUE;

		tracker_sparql_builder_predicate (metadata, "a");

		if (needs_audio) {
			tracker_sparql_builder_object (metadata, "nmm:MusicPiece");
			tracker_sparql_builder_object (metadata, "nfo:Audio");
		}

		if (extractor->mime == EXTRACT_MIME_VIDEO) {
			tracker_sparql_builder_object (metadata, "nmm:Video");
		} else if (!needs_audio) {
			tracker_sparql_builder_object (metadata, "nfo:Image");
			tracker_sparql_builder_object (metadata, "nmm:Photo");
		}
	}


	if (use_tagreadbin) {
		extract_stream_metadata_tagreadbin (extractor, uri, metadata);
	} else {
		extract_stream_metadata_decodebin (extractor, uri, metadata);
	}

	if (extractor->mime == EXTRACT_MIME_AUDIO) {
		get_embedded_album_art (extractor);
	}
}

static void
unlink_fsink (void *obj, void *data_)
{
	MetadataExtractor *extractor = (MetadataExtractor *)data_;
	GstElement        *fsink     = (GstElement *) obj;

	gst_element_unlink (extractor->bin, fsink);
	gst_bin_remove (GST_BIN (extractor->pipeline), fsink);
	gst_element_set_state (fsink, GST_STATE_NULL);
}

static void
dbin_dpad_cb (GstElement* e, GstPad* pad, gboolean cont, gpointer data_)
{
	MetadataExtractor *extractor = (MetadataExtractor *)data_;
	GstElement        *fsink;
	GstPad            *fsinkpad;
	GValue             val = {0, };

	fsink = gst_element_factory_make ("fakesink", NULL);

	/* We increase the preroll buffer so we get duration (one frame not enough)*/
	g_value_init (&val, G_TYPE_INT);
	g_value_set_int (&val, 51);
	g_object_set_property (G_OBJECT (fsink), "preroll-queue-len", &val);
	g_value_unset (&val);

	extractor->fsinks = g_list_append (extractor->fsinks, fsink);
	gst_element_set_state (fsink, GST_STATE_PAUSED);

	gst_bin_add (GST_BIN (extractor->pipeline), fsink);
	fsinkpad = gst_element_get_static_pad (fsink, "sink");
	gst_pad_link (pad, fsinkpad);
	gst_object_unref (fsinkpad);
}

static guint64
get_media_duration (MetadataExtractor *extractor)
{
	gint64    duration;
	GstFormat fmt;

	g_return_val_if_fail (extractor, -1);
	g_return_val_if_fail (extractor->pipeline, -1);

	fmt = GST_FORMAT_TIME;

	duration = -1;

	if (gst_element_query_duration (extractor->pipeline,
	                                &fmt,
	                                &duration) &&
	    duration >= 0) {
		return duration / GST_SECOND;
	} else {
		return -1;
	}
}

static void
add_stream_tag (void *obj, void *data_)
{
	MetadataExtractor *extractor = (MetadataExtractor *)data_;
	GstElement        *fsink     = (GstElement *) obj;

	GstStructure      *s         = NULL;
	GstCaps                   *caps      = NULL;

	if ((caps = GST_PAD_CAPS (fsink))) {
		s = gst_caps_get_structure (caps, 0);

		if (s) {
			if (g_strrstr (gst_structure_get_name (s), "audio")) {
				if ( ( (extractor->audio_channels != -1) &&
				       (extractor->audio_samplerate != -1) ) ||
				     !( (gst_structure_get_int (s,
				                                "channels",
				                                &extractor->audio_channels) ) &&
				        (gst_structure_get_int (s,
				                                "rate",
				                                &extractor->audio_samplerate)) ) ) {
					return;
				}
			} else if (g_strrstr (gst_structure_get_name (s), "video")) {
				if ( ( (extractor->video_fps_n != -1) &&
				       (extractor->video_fps_d != -1) &&
				       (extractor->video_width != -1) &&
				       (extractor->video_height != -1) ) ||
				     !( (gst_structure_get_fraction (s,
				                                     "framerate",
				                                     &extractor->video_fps_n,
				                                     &extractor->video_fps_d) ) &&
				        (gst_structure_get_int (s, "width", &extractor->video_width)) &&
				        (gst_structure_get_int (s, "height", &extractor->video_height)) &&
				        (gst_structure_get_int (s, "pixel-aspect-ratio", &extractor->aspect_ratio)))) {
					return;
				}
			}
		}
	}
}

static void
add_stream_tags (MetadataExtractor *extractor)
{
	extractor->duration = get_media_duration (extractor);
	g_list_foreach (extractor->fsinks, add_stream_tag, extractor);
}

static void
add_tags (GstTagList *new_tags, MetadataExtractor *extractor)
{
	GstTagList   *result;

	result = gst_tag_list_merge (extractor->tagcache,
	                             new_tags,
	                             GST_TAG_MERGE_KEEP);

	if (extractor->tagcache) {
		gst_tag_list_free (extractor->tagcache);
	}

	extractor->tagcache = result;
}


static gboolean
poll_for_ready (MetadataExtractor *extractor,
                GstState           state,
                gboolean           ready_with_state,
                gboolean           ready_with_eos)
{
	gint64              timeout   = 5 * GST_SECOND;
	GstBus             *bus       = extractor->bus;
	GstTagList         *new_tags;

	gst_element_set_state (extractor->pipeline, state);

	while (TRUE) {
		GstMessage *message;
		GstElement *src;

		message = gst_bus_timed_pop (bus, timeout);

		if (!message) {
			g_warning ("Pipeline timed out");
			return FALSE;
		}

		src = (GstElement*)GST_MESSAGE_SRC (message);

		switch (GST_MESSAGE_TYPE (message)) {
		case GST_MESSAGE_STATE_CHANGED: {
			if (ready_with_state) {
				GstState old, new, pending;

				if (src == extractor->pipeline) {
					gst_message_parse_state_changed (message, &old, &new, &pending);
					if (new == state) {
						gst_message_unref (message);
						return TRUE;
					}
				}
			}
			break;
		}
		case GST_MESSAGE_ERROR: {
			GError *lerror = NULL;
			gchar  *error_message;

			gst_message_parse_error (message, &lerror, &error_message);

#if (GST_VERSION_MICRO >= 20)
			if (lerror->domain == GST_STREAM_ERROR) {
				if (lerror->code == GST_STREAM_ERROR_DECRYPT ||
				    lerror->code == GST_STREAM_ERROR_DECRYPT_NOKEY) {
					/* also extract metadata from encrypted streams */

					extractor->is_content_encrypted = TRUE;

					g_free (error_message);
					g_error_free (lerror);
					break;
				}
			}
#endif

			gst_message_unref (message);
			g_warning ("Got error :%s", error_message);
			g_free (error_message);
			g_error_free (lerror);

			return FALSE;
			break;
		}
		case GST_MESSAGE_EOS: {
			gst_message_unref (message);

			if (ready_with_eos) {
				return TRUE;
			} else {
				g_warning ("Reached end-of-file without proper content");
				return FALSE;
			}
			break;
		}
		case GST_MESSAGE_TAG: {
			gst_message_parse_tag (message, &new_tags);
			add_tags (new_tags, extractor);
			gst_tag_list_free (new_tags);
			break;
		}
		default:
			/* Nothing to do here */
			break;
		}

		gst_message_unref (message);
	}

	g_assert_not_reached ();

	return FALSE;
}

static GstElement *
create_decodebin_pipeline (MetadataExtractor *extractor,
                           const gchar *uri)
{
	GstElement *pipeline = NULL;

	GstElement *filesrc  = NULL;
	GstElement *bin      = NULL;

	pipeline = gst_element_factory_make ("pipeline", NULL);
	if (!pipeline) {
		g_warning ("Failed to create GStreamer pipeline");
		return NULL;
	}

	filesrc = gst_element_factory_make ("giosrc", NULL);
	if (!filesrc) {
		g_warning ("Failed to create GStreamer giosrc");
		gst_object_unref (GST_OBJECT (pipeline));
		return NULL;
	}

	bin = gst_element_factory_make ("decodebin2", "decodebin2");
	if (!bin) {
		g_warning ("Failed to create GStreamer decodebin");
		gst_object_unref (GST_OBJECT (pipeline));
		gst_object_unref (GST_OBJECT (filesrc));
		return NULL;
	}

	g_signal_connect (G_OBJECT (bin),
			  "new-decoded-pad",
			  G_CALLBACK (dbin_dpad_cb),
			  extractor);

	gst_bin_add (GST_BIN (pipeline), filesrc);
	gst_bin_add (GST_BIN (pipeline), bin);

	if (!gst_element_link_many (filesrc, bin, NULL)) {
		g_warning ("Could not link GStreamer elements");
		gst_object_unref (GST_OBJECT (pipeline));
		return NULL;
	}

	g_object_set (G_OBJECT (filesrc), "location", uri, NULL);

	extractor->bin = bin;

	return pipeline;
}

static GstElement *
create_tagreadbin_pipeline (MetadataExtractor *extractor,
                            const gchar *uri)
{
	GstElement *pipeline = NULL;

	pipeline = gst_element_factory_make ("tagreadbin", "tagreadbin");
	if (!pipeline) {
		g_warning ("Failed to create GStreamer tagreadbin");
		return NULL;
	}

	g_object_set (G_OBJECT (pipeline), "uri", uri, NULL);
	return pipeline;
}


static void
tracker_extract_gstreamer (const gchar *uri,
                           TrackerSparqlBuilder  *preupdate,
                           TrackerSparqlBuilder  *metadata,
                           ExtractMime  type)
{
	MetadataExtractor *extractor;
	gchar *artist, *album, *scount;

	g_return_if_fail (uri);
	g_return_if_fail (metadata);

	/* These must be moved to main() */
	g_type_init ();

	gst_init (NULL, NULL);

	extractor               = g_slice_new0 (MetadataExtractor);

	extractor->pipeline     = NULL;
	extractor->bus          = NULL;
	extractor->mime         = type;

	extractor->tagcache     = NULL;

	extractor->album_art_data = NULL;
	extractor->album_art_size = 0;
	extractor->album_art_mime = NULL;

	extractor->bin          = NULL;
	extractor->fsinks       = NULL;

	extractor->duration = -1;
	extractor->video_fps_n = extractor->video_fps_d = -1;
	extractor->video_height = extractor->video_width = -1;
	extractor->audio_channels = -1;
	extractor->aspect_ratio = -1;
	extractor->audio_samplerate = -1;

	if (use_tagreadbin) {
		extractor->pipeline = create_tagreadbin_pipeline (extractor, uri);
	} else {
		extractor->pipeline = create_decodebin_pipeline (extractor, uri);
	}

	if (!extractor->pipeline) {
		g_warning ("No valid pipeline for uri %s", uri);
		g_slice_free (MetadataExtractor, extractor);
		return;
	}

	extractor->bus = gst_pipeline_get_bus (GST_PIPELINE (extractor->pipeline));

	if (use_tagreadbin) {
		if (!poll_for_ready (extractor, GST_STATE_PLAYING, FALSE, TRUE)) {
			g_warning ("Error running tagreadbin");
			gst_element_set_state (extractor->pipeline, GST_STATE_NULL);
			gst_object_unref (GST_OBJECT (extractor->pipeline));
			gst_object_unref (extractor->bus);
			g_slice_free (MetadataExtractor, extractor);
			return;
		}
	} else {
		if (!poll_for_ready (extractor, GST_STATE_PAUSED, TRUE, FALSE)) {
			g_warning ("Error running decodebin");
			gst_element_set_state (extractor->pipeline, GST_STATE_NULL);
			gst_object_unref (GST_OBJECT (extractor->pipeline));
			gst_object_unref (extractor->bus);
			g_slice_free (MetadataExtractor, extractor);
			return;
		}

		add_stream_tags(extractor);
		gst_element_set_state (extractor->pipeline, GST_STATE_READY);
		gst_element_get_state (extractor->pipeline, NULL, NULL, 5 * GST_SECOND);
		g_list_foreach (extractor->fsinks, unlink_fsink, extractor);
		g_list_free (extractor->fsinks);
		extractor->fsinks = NULL;
	}

	album = NULL;
	scount = NULL;
	artist = NULL;

	extract_metadata (extractor, uri, preupdate, metadata, &artist, &album, &scount);

	tracker_albumart_process (extractor->album_art_data,
	                          extractor->album_art_size,
	                          extractor->album_art_mime,
	                          artist,
	                          album,
	                          uri);

	g_free (scount);
	g_free (album);
	g_free (artist);

	gst_element_set_state (extractor->pipeline, GST_STATE_NULL);
	gst_element_get_state (extractor->pipeline, NULL, NULL, TRACKER_EXTRACT_GUARD_TIMEOUT* GST_SECOND);
	gst_object_unref (extractor->bus);

	if (extractor->tagcache) {
		gst_tag_list_free (extractor->tagcache);
	}

	gst_object_unref (GST_OBJECT (extractor->pipeline));
	g_slice_free (MetadataExtractor, extractor);
}


static void
extract_gstreamer_audio (const gchar          *uri,
                         TrackerSparqlBuilder *preupdate,
                         TrackerSparqlBuilder *metadata)
{
	tracker_extract_gstreamer (uri, preupdate, metadata, EXTRACT_MIME_AUDIO);
}

static void
extract_gstreamer_video (const gchar          *uri,
                         TrackerSparqlBuilder *preupdate,
                         TrackerSparqlBuilder *metadata)
{
	tracker_extract_gstreamer (uri, preupdate, metadata, EXTRACT_MIME_VIDEO);
}

static void
extract_gstreamer_image (const gchar          *uri,
                         TrackerSparqlBuilder *preupdate,
                         TrackerSparqlBuilder *metadata)
{
	tracker_extract_gstreamer (uri, preupdate, metadata, EXTRACT_MIME_IMAGE);
}

static void
extract_gstreamer_guess (const gchar          *uri,
                        TrackerSparqlBuilder *preupdate,
                         TrackerSparqlBuilder *metadata)
{
	tracker_extract_gstreamer (uri, preupdate, metadata, EXTRACT_MIME_GUESS);
}

TrackerExtractData *
tracker_extract_get_data (void)
{
	return data;
}

