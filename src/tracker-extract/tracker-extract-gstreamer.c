/*
 * Copyright (C) 2006, Laurent Aguerreche <laurent.aguerreche@free.fr>
 * Copyright (C) 2007, Jamie McCracken <jamiemcc@gnome.org>
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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

/* Ensure we have a valid backend enabled */
#if !defined(GSTREAMER_BACKEND_DISCOVERER) && \
    !defined(GSTREAMER_BACKEND_GUPNP_DLNA)
#error Not a valid GStreamer backend defined
#endif

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <glib.h>
#include <glib/gstdio.h>

#if defined(GSTREAMER_BACKEND_DISCOVERER) || \
    defined(GSTREAMER_BACKEND_GUPNP_DLNA)
#define GST_USE_UNSTABLE_API
#include <gst/pbutils/pbutils.h>
#endif

#if defined(GSTREAMER_BACKEND_GUPNP_DLNA)
#include <libgupnp-dlna/gupnp-dlna.h>
#include <libgupnp-dlna/gupnp-dlna-gst-utils.h>
#endif

#include <gst/gst.h>
#include <gst/tag/tag.h>

#ifdef HAVE_LIBMEDIAART
#include <libmediaart/mediaart.h>
#endif

#include <libtracker-common/tracker-common.h>
#include <libtracker-extract/tracker-extract.h>

#include "tracker-cue-sheet.h"

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

typedef enum {
	EXTRACT_MIME_AUDIO,
	EXTRACT_MIME_VIDEO,
	EXTRACT_MIME_IMAGE,
	EXTRACT_MIME_GUESS,
	EXTRACT_MIME_SVG,
} ExtractMime;

typedef struct {
	ExtractMime     mime;
	GstTagList     *tagcache;
	TrackerToc     *toc;
	gboolean        is_content_encrypted;

	GSList         *artist_list;

#ifdef HAVE_LIBMEDIAART
	MediaArtType    media_art_type;
	gchar          *media_art_artist;
	gchar          *media_art_title;

	unsigned char  *media_art_buffer;
	guint           media_art_buffer_size;
	const gchar    *media_art_buffer_mime;
#endif

	GstSample      *sample;
	GstMapInfo      info;

#if defined(GSTREAMER_BACKEND_DISCOVERER) || \
    defined(GSTREAMER_BACKEND_GUPNP_DLNA)
	gboolean        has_image;
	gboolean        has_audio;
	gboolean        has_video;
	GList          *streams;
#endif

#if defined(GSTREAMER_BACKEND_DISCOVERER) || \
    defined(GSTREAMER_BACKEND_GUPNP_DLNA)
	GstDiscoverer  *discoverer;
#endif

#if defined(GSTREAMER_BACKEND_GUPNP_DLNA)
	const gchar          *dlna_profile;
	const gchar          *dlna_mime;
#endif

#if defined(GSTREAMER_BACKEND_DISCOVERER) || \
    defined(GSTREAMER_BACKEND_GUPNP_DLNA)
	gint64          duration;
	gint            audio_channels;
	gint            audio_samplerate;
	gint            height;
	gint            width;
	gfloat          aspect_ratio;
	gfloat          video_fps;
#endif
} MetadataExtractor;

static void common_extract_stream_metadata (MetadataExtractor    *extractor,
                                            const gchar          *uri,
                                            TrackerSparqlBuilder *metadata);

static void
add_artist (MetadataExtractor     *extractor,
            TrackerSparqlBuilder  *preupdate,
            const gchar           *graph,
            const gchar           *artist_name,
            gchar                **p_artist_uri)
{
	g_return_if_fail (artist_name != NULL);

	*p_artist_uri = tracker_sparql_escape_uri_printf ("urn:artist:%s", artist_name);

	/* Check if already added to the preupdate, to avoid sending 9 identical INSERTs */
	if (g_slist_find_custom (extractor->artist_list, artist_name, (GCompareFunc) strcmp))
		return;

	tracker_sparql_builder_insert_open (preupdate, NULL);
	if (graph) {
		tracker_sparql_builder_graph_open (preupdate, graph);
	}

	tracker_sparql_builder_subject_iri (preupdate, *p_artist_uri);
	tracker_sparql_builder_predicate (preupdate, "a");
	tracker_sparql_builder_object (preupdate, "nmm:Artist");
	tracker_sparql_builder_predicate (preupdate, "nmm:artistName");
	tracker_sparql_builder_object_unvalidated (preupdate, artist_name);

	if (graph) {
		tracker_sparql_builder_graph_close (preupdate);
	}
	tracker_sparql_builder_insert_close (preupdate);

	extractor->artist_list = g_slist_prepend (extractor->artist_list, g_strdup (artist_name));
}

static void
add_string_gst_tag (TrackerSparqlBuilder *metadata,
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
add_uint_gst_tag (TrackerSparqlBuilder  *metadata,
                  const gchar           *key,
                  GstTagList            *tag_list,
                  const gchar           *tag)
{
	gboolean ret;
	guint n;

	ret = gst_tag_list_get_uint (tag_list, tag, &n);

	if (ret) {
		tracker_sparql_builder_predicate (metadata, key);
		tracker_sparql_builder_object_int64 (metadata, n);
	}
}

static void
add_double_gst_tag (TrackerSparqlBuilder  *metadata,
                    const gchar           *key,
                    GstTagList            *tag_list,
                    const gchar           *tag)
{
	gboolean ret;
	gdouble n;

	ret = gst_tag_list_get_double (tag_list, tag, &n);

	if (ret) {
		tracker_sparql_builder_predicate (metadata, key);
		tracker_sparql_builder_object_int64 (metadata, (gint64) n);
	}
}

static inline gboolean
get_gst_date_time_to_buf (GstDateTime *date_time,
                          gchar       *buf,
                          size_t       size)
{
	const gchar *offset_str;
	gint year, month, day, hour, minute, second;
	gfloat offset;
	gboolean complete;

	offset_str = "+";
	year = month = day = hour = minute = second = 0;
	offset = 0.0;
	complete = TRUE;

	if (gst_date_time_has_year (date_time)) {
		year = gst_date_time_get_year (date_time);
	} else {
		complete = FALSE;
	}

	if (gst_date_time_has_month (date_time)) {
		month = gst_date_time_get_month (date_time);
	} else {
		complete = FALSE;
	}

	if (gst_date_time_has_day (date_time)) {
		day = gst_date_time_get_day (date_time);
	} else {
		complete = FALSE;
	}

	/* Hour and Minute data is retrieved by first checking the
	 * _has_time() API.
	 */

	if (gst_date_time_has_second (date_time)) {
		second = gst_date_time_get_second (date_time);
	} else {
		complete = FALSE;
	}

	if (gst_date_time_has_time (date_time)) {
		hour = gst_date_time_get_hour (date_time);
		minute = gst_date_time_get_minute (date_time);
		offset_str = gst_date_time_get_time_zone_offset (date_time) >= 0 ? "+" : "";
		offset = gst_date_time_get_time_zone_offset (date_time);
	} else {
		offset_str = "+";
		complete = FALSE;
	}

	snprintf (buf, size, "%04d-%02d-%02dT%02d:%02d:%02d%s%02d00",
	          year,
	          month,
	          day,
	          hour,
	          minute,
	          second,
	          offset_str,
	          (gint) offset);

	return complete;
}

static void
add_date_time_gst_tag_with_mtime_fallback (TrackerSparqlBuilder  *metadata,
                                           const gchar           *uri,
                                           const gchar           *key,
                                           GstTagList            *tag_list,
                                           const gchar           *tag_date_time,
                                           const gchar           *tag_date)
{
	GstDateTime *date_time;
	GDate *date;
	gchar buf[25];

	date_time = NULL;
	date = NULL;
	buf[0] = '\0';

	if (gst_tag_list_get_date_time (tag_list, tag_date_time, &date_time)) {
		gboolean complete;

		complete = get_gst_date_time_to_buf (date_time, buf, sizeof (buf));
		gst_date_time_unref (date_time);

		if (!complete) {
			g_message ("GstDateTime was not complete, parts of the date/time were missing (e.g. hours, minutes, seconds)");
		}
	} else if (gst_tag_list_get_date (tag_list, tag_date, &date)) {
		gboolean ret = FALSE;

		if (date && g_date_valid (date)) {
			if (date->julian)
				ret = g_date_valid_julian (date->julian_days);
			if (date->dmy)
				ret = g_date_valid_dmy (date->day, date->month, date->year);
		}

		if (ret) {
			/* GDate does not carry time zone information, assume UTC */
			g_date_strftime (buf, sizeof (buf), "%Y-%m-%dT%H:%M:%SZ", date);
		}
	}

	if (date) {
		g_date_free (date);
	}

	tracker_guarantee_date_from_file_mtime (metadata, key, buf, uri);
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
replace_double_gst_tag (TrackerSparqlBuilder  *preupdate,
                        const gchar           *uri,
                        const gchar           *key,
                        GstTagList            *tag_list,
                        const gchar           *tag,
                        const gchar           *graph)
{
	gdouble  value;
	gboolean has_it;

	has_it = gst_tag_list_get_double (tag_list, tag, &value);

	if (! has_it)
		return;

	tracker_sparql_builder_delete_open (preupdate, NULL);
	tracker_sparql_builder_subject_iri (preupdate, uri);
	tracker_sparql_builder_predicate (preupdate, key);
	tracker_sparql_builder_object_variable (preupdate, "unknown");
	tracker_sparql_builder_delete_close (preupdate);

	tracker_sparql_builder_where_open (preupdate);
	tracker_sparql_builder_subject_iri (preupdate, uri);
	tracker_sparql_builder_predicate (preupdate, key);
	tracker_sparql_builder_object_variable (preupdate, "unknown");
	tracker_sparql_builder_where_close (preupdate);

	tracker_sparql_builder_insert_open (preupdate, NULL);
	if (graph) {
		tracker_sparql_builder_graph_open (preupdate, graph);
	}

	tracker_sparql_builder_subject_iri (preupdate, uri);
	tracker_sparql_builder_predicate (preupdate, key);
	tracker_sparql_builder_object_double (preupdate, value);

	if (graph) {
		tracker_sparql_builder_graph_close (preupdate);
	}
	tracker_sparql_builder_insert_close (preupdate);
}

static gchar *
get_embedded_cue_sheet_data (GstTagList *tag_list)
{
	gint i, count;
	gchar *buffer = NULL;

	count = gst_tag_list_get_tag_size (tag_list, GST_TAG_EXTENDED_COMMENT);
	for (i = 0; i < count; i++) {
		gst_tag_list_get_string_index (tag_list, GST_TAG_EXTENDED_COMMENT, i, &buffer);

		if (g_ascii_strncasecmp (buffer, "cuesheet=", 9) == 0) {
			/* Use same functionality as g_strchug() here
			 * for cuesheet, to avoid allocating new
			 * memory but also to return the string and
			 * not have to jump past cuesheet= on the
			 * returned value.
			 */
			g_memmove (buffer, buffer + 9, strlen ((gchar *) buffer + 9) + 1);

			return buffer;
		}

		g_free (buffer);
	}

	return NULL;
}

#ifdef HAVE_LIBMEDIAART

static gboolean
get_embedded_media_art (MetadataExtractor *extractor)
{
	gboolean have_sample;
	guint lindex;

	lindex = 0;

	do {
		have_sample = gst_tag_list_get_sample_index (extractor->tagcache, GST_TAG_IMAGE, lindex, &extractor->sample);

		if (have_sample) {
			GstBuffer *buffer;
			const GstStructure *info_struct;
			gint type;

			buffer = gst_sample_get_buffer (extractor->sample);
			info_struct = gst_sample_get_info (extractor->sample);
			if (gst_structure_get_enum (info_struct,
			                            "image-type",
			                            GST_TYPE_TAG_IMAGE_TYPE,
			                            &type)) {
				if (type == GST_TAG_IMAGE_TYPE_FRONT_COVER ||
				    (type == GST_TAG_IMAGE_TYPE_UNDEFINED &&
				     extractor->media_art_buffer_size == 0)) {
					GstCaps *caps;
					GstStructure *caps_struct;

					if (!gst_buffer_map (buffer, &extractor->info, GST_MAP_READ))
						return FALSE;

					caps = gst_sample_get_caps (extractor->sample);
					caps_struct = gst_caps_get_structure (caps, 0);

					extractor->media_art_buffer = extractor->info.data;
					extractor->media_art_buffer_size = extractor->info.size;
					extractor->media_art_buffer_mime = gst_structure_get_name (caps_struct);

					return TRUE;
				}
			}

			lindex++;
		}

	} while (have_sample);

	have_sample = gst_tag_list_get_sample_index (extractor->tagcache, GST_TAG_IMAGE, lindex, &extractor->sample);

	if (have_sample) {
		GstBuffer *buffer;
		GstCaps *caps;
		GstStructure *caps_struct;

		buffer = gst_sample_get_buffer (extractor->sample);
		caps = gst_sample_get_caps (extractor->sample);
		caps_struct = gst_caps_get_structure (caps, 0);

		if (!gst_buffer_map (buffer, &extractor->info, GST_MAP_READ))
			return FALSE;

		extractor->media_art_buffer = extractor->info.data;
		extractor->media_art_buffer_size = extractor->info.size;
		extractor->media_art_buffer_mime = gst_structure_get_name (caps_struct);

		return TRUE;
	}

	return FALSE;
}

#endif

static void
extractor_apply_geolocation_metadata (MetadataExtractor     *extractor,
                                      GstTagList            *tag_list,
                                      TrackerSparqlBuilder  *preupdate,
                                      TrackerSparqlBuilder  *metadata,
                                      const gchar           *graph)
{
	gchar *country = NULL, *city = NULL, *sublocation = NULL;
	gdouble lat, lon, alt;
	gboolean has_coords;

	g_debug ("Retrieving geolocation metadata...");

	country = city = sublocation = NULL;
	has_coords = (gst_tag_list_get_double (tag_list, GST_TAG_GEO_LOCATION_LATITUDE, &lat) &&
	              gst_tag_list_get_double (tag_list, GST_TAG_GEO_LOCATION_LONGITUDE, &lon) &&
	              gst_tag_list_get_double (tag_list, GST_TAG_GEO_LOCATION_ELEVATION, &alt));

	gst_tag_list_get_string (tag_list, GST_TAG_GEO_LOCATION_CITY, &city);
	gst_tag_list_get_string (tag_list, GST_TAG_GEO_LOCATION_COUNTRY, &country);
	gst_tag_list_get_string (tag_list, GST_TAG_GEO_LOCATION_SUBLOCATION, &sublocation);

	if (city || country || sublocation || has_coords) {
		gchar *address_uri = NULL;

		/* Create postal address */
		if (city || country || sublocation) {
			address_uri = tracker_sparql_get_uuid_urn ();

			tracker_sparql_builder_insert_open (preupdate, NULL);
			if (graph) {
				tracker_sparql_builder_graph_open (preupdate, graph);
			}

			tracker_sparql_builder_subject_iri (preupdate, address_uri);
			tracker_sparql_builder_predicate (preupdate, "a");
			tracker_sparql_builder_object (preupdate, "nco:PostalAddress");

			if (sublocation) {
				tracker_sparql_builder_predicate (preupdate, "nco:region");
				tracker_sparql_builder_object_unvalidated (preupdate, sublocation);
			}

			if (city) {
				tracker_sparql_builder_predicate (preupdate, "nco:locality");
				tracker_sparql_builder_object_unvalidated (preupdate, city);
			}

			if (country) {
				tracker_sparql_builder_predicate (preupdate, "nco:country");
				tracker_sparql_builder_object_unvalidated (preupdate, country);
			}

			if (graph) {
				tracker_sparql_builder_graph_close (preupdate);
			}
			tracker_sparql_builder_insert_close (preupdate);
		}

		/* Create geolocation */
		tracker_sparql_builder_predicate (metadata, "slo:location");

		tracker_sparql_builder_object_blank_open (metadata);
		tracker_sparql_builder_predicate (metadata, "a");
		tracker_sparql_builder_object (metadata, "slo:GeoLocation");

		if (address_uri) {
			tracker_sparql_builder_predicate (metadata, "slo:postalAddress");
			tracker_sparql_builder_object_iri (metadata, address_uri);
		}

		if (has_coords) {
			tracker_sparql_builder_predicate (metadata, "slo:latitude");
			tracker_sparql_builder_object_double (metadata, lat);

			tracker_sparql_builder_predicate (metadata, "slo:longitude");
			tracker_sparql_builder_object_double (metadata, lon);

			tracker_sparql_builder_predicate (metadata, "slo:altitude");
			tracker_sparql_builder_object_double (metadata, alt);
		}

		tracker_sparql_builder_object_blank_close (metadata);
		g_free (address_uri);
	}

	g_free (city);
	g_free (country);
	g_free (sublocation);
}

static void
extractor_guess_content_type (MetadataExtractor *extractor)
{
	if (extractor->has_video) {
		extractor->mime = EXTRACT_MIME_VIDEO;
	} else if (extractor->has_audio) {
		extractor->mime = EXTRACT_MIME_AUDIO;
	} else if (extractor->has_image) {
		extractor->mime = EXTRACT_MIME_IMAGE;
	} else {
		/* default to video */
		extractor->mime = EXTRACT_MIME_VIDEO;
	}
}

static void
extractor_apply_general_metadata (MetadataExtractor     *extractor,
                                  GstTagList            *tag_list,
                                  const gchar           *file_url,
                                  TrackerSparqlBuilder  *preupdate,
                                  TrackerSparqlBuilder  *metadata,
                                  const gchar           *graph,
                                  gchar                **p_performer_uri,
                                  gchar                **p_composer_uri)
{
	const gchar *performer = NULL;
	gchar *performer_temp = NULL;
	gchar *artist_temp = NULL;
	gchar *composer = NULL;
	gchar *genre = NULL;
	gchar *title = NULL;
	gchar *title_guaranteed = NULL;

	gst_tag_list_get_string (tag_list, GST_TAG_PERFORMER, &performer_temp);
	gst_tag_list_get_string (tag_list, GST_TAG_ARTIST, &artist_temp);
	gst_tag_list_get_string (tag_list, GST_TAG_COMPOSER, &composer);

	performer = tracker_coalesce_strip (2, performer_temp, artist_temp);

	if (performer != NULL) {
		add_artist (extractor, preupdate, graph, performer, p_performer_uri);
	}

	if (composer != NULL) {
		add_artist (extractor, preupdate, graph, composer, p_composer_uri);
	}

	gst_tag_list_get_string (tag_list, GST_TAG_GENRE, &genre);
	gst_tag_list_get_string (tag_list, GST_TAG_TITLE, &title);

	if (genre && g_strcmp0 (genre, "Unknown") != 0) {
		tracker_sparql_builder_predicate (metadata, "nfo:genre");
		tracker_sparql_builder_object_unvalidated (metadata, genre);
	}

	tracker_guarantee_title_from_file (metadata,
	                                   "nie:title",
	                                   title,
	                                   file_url,
	                                   &title_guaranteed);

	add_date_time_gst_tag_with_mtime_fallback (metadata,
	                                           file_url,
	                                           "nie:contentCreated",
	                                           tag_list,
	                                           GST_TAG_DATE_TIME,
	                                           GST_TAG_DATE);

	add_string_gst_tag (metadata, "nie:copyright", tag_list, GST_TAG_COPYRIGHT);
	add_string_gst_tag (metadata, "nie:license", tag_list, GST_TAG_LICENSE);
	add_string_gst_tag (metadata, "dc:coverage", tag_list, GST_TAG_LOCATION);
	add_string_gst_tag (metadata, "nie:comment", tag_list, GST_TAG_COMMENT);

#ifdef HAVE_LIBMEDIAART
	if (extractor->media_art_type == MEDIA_ART_VIDEO) {
		extractor->media_art_title = title_guaranteed;
	} else {
		g_free (title_guaranteed);
	}
#else
	g_free (title_guaranteed);
#endif

	g_free (performer_temp);
	g_free (artist_temp);
	g_free (composer);
	g_free (genre);
	g_free (title);
}

static void
extractor_apply_album_metadata (MetadataExtractor     *extractor,
                                GstTagList            *tag_list,
                                TrackerSparqlBuilder  *preupdate,
                                const gchar           *graph,
                                gchar                **p_album_artist_uri,
                                gchar                **p_album_uri,
                                gchar                **p_album_disc_uri)
{
	gchar *album_artist;
	gchar *album_title = NULL;
	gchar *album_artist_temp = NULL;
	gchar *track_artist_temp = NULL;
	gboolean has_it;
	guint count;

	gst_tag_list_get_string (tag_list, GST_TAG_ALBUM, &album_title);

	if (!album_title)
		return;

	gst_tag_list_get_string (tag_list, GST_TAG_ALBUM_ARTIST, &album_artist_temp);
	gst_tag_list_get_string (tag_list, GST_TAG_ARTIST, &track_artist_temp);

	album_artist = g_strdup (tracker_coalesce_strip (2, album_artist_temp, track_artist_temp));

        if (album_artist != NULL) {
                add_artist (extractor, preupdate, graph, album_artist, p_album_artist_uri);
                *p_album_uri = tracker_sparql_escape_uri_printf ("urn:album:%s:%s", album_title, album_artist);
        } else {
                *p_album_uri = tracker_sparql_escape_uri_printf ("urn:album:%s", album_title);
        }

	tracker_sparql_builder_insert_open (preupdate, NULL);
	if (graph) {
		tracker_sparql_builder_graph_open (preupdate, graph);
	}

	tracker_sparql_builder_subject_iri (preupdate, *p_album_uri);
	tracker_sparql_builder_predicate (preupdate, "a");
	tracker_sparql_builder_object (preupdate, "nmm:MusicAlbum");
	/* FIXME: nmm:albumTitle is now deprecated
	 * tracker_sparql_builder_predicate (preupdate, "nie:title");
	 */
	tracker_sparql_builder_predicate (preupdate, "nmm:albumTitle");
	tracker_sparql_builder_object_unvalidated (preupdate, album_title);

	if (*p_album_artist_uri) {
		tracker_sparql_builder_predicate (preupdate, "nmm:albumArtist");
		tracker_sparql_builder_object_iri (preupdate, *p_album_artist_uri);
	}

	if (graph) {
		tracker_sparql_builder_graph_close (preupdate);
	}
	tracker_sparql_builder_insert_close (preupdate);

	has_it = gst_tag_list_get_uint (tag_list, GST_TAG_TRACK_COUNT, &count);

	if (has_it) {
		tracker_sparql_builder_delete_open (preupdate, NULL);
		tracker_sparql_builder_subject_iri (preupdate, *p_album_uri);
		tracker_sparql_builder_predicate (preupdate, "nmm:albumTrackCount");
		tracker_sparql_builder_object_variable (preupdate, "unknown");
		tracker_sparql_builder_delete_close (preupdate);
		tracker_sparql_builder_where_open (preupdate);
		tracker_sparql_builder_subject_iri (preupdate, *p_album_uri);
		tracker_sparql_builder_predicate (preupdate, "nmm:albumTrackCount");
		tracker_sparql_builder_object_variable (preupdate, "unknown");
		tracker_sparql_builder_where_close (preupdate);

		tracker_sparql_builder_insert_open (preupdate, NULL);
		if (graph) {
			tracker_sparql_builder_graph_open (preupdate, graph);
		}

		tracker_sparql_builder_subject_iri (preupdate, *p_album_uri);
		tracker_sparql_builder_predicate (preupdate, "nmm:albumTrackCount");
		tracker_sparql_builder_object_int64 (preupdate, count);

		if (graph) {
			tracker_sparql_builder_graph_close (preupdate);
		}
		tracker_sparql_builder_insert_close (preupdate);
	}

	has_it = gst_tag_list_get_uint (tag_list, GST_TAG_ALBUM_VOLUME_NUMBER, &count);

        if (album_artist) {
                *p_album_disc_uri = tracker_sparql_escape_uri_printf ("urn:album-disc:%s:%s:Disc%d",
                                                                      album_title, album_artist,
                                                                      has_it ? count : 1);
        } else {
                *p_album_disc_uri = tracker_sparql_escape_uri_printf ("urn:album-disc:%s:Disc%d",
                                                                      album_title,
                                                                      has_it ? count : 1);
        }


	tracker_sparql_builder_delete_open (preupdate, NULL);
	tracker_sparql_builder_subject_iri (preupdate, *p_album_disc_uri);
	tracker_sparql_builder_predicate (preupdate, "nmm:setNumber");
	tracker_sparql_builder_object_variable (preupdate, "unknown");
	tracker_sparql_builder_delete_close (preupdate);
	tracker_sparql_builder_where_open (preupdate);
	tracker_sparql_builder_subject_iri (preupdate, *p_album_disc_uri);
	tracker_sparql_builder_predicate (preupdate, "nmm:setNumber");
	tracker_sparql_builder_object_variable (preupdate, "unknown");
	tracker_sparql_builder_where_close (preupdate);

	tracker_sparql_builder_delete_open (preupdate, NULL);
	tracker_sparql_builder_subject_iri (preupdate, *p_album_disc_uri);
	tracker_sparql_builder_predicate (preupdate, "nmm:albumDiscAlbum");
	tracker_sparql_builder_object_variable (preupdate, "unknown");
	tracker_sparql_builder_delete_close (preupdate);
	tracker_sparql_builder_where_open (preupdate);
	tracker_sparql_builder_subject_iri (preupdate, *p_album_disc_uri);
	tracker_sparql_builder_predicate (preupdate, "nmm:albumDiscAlbum");
	tracker_sparql_builder_object_variable (preupdate, "unknown");
	tracker_sparql_builder_where_close (preupdate);

	tracker_sparql_builder_insert_open (preupdate, NULL);
	if (graph) {
		tracker_sparql_builder_graph_open (preupdate, graph);
	}

	tracker_sparql_builder_subject_iri (preupdate, *p_album_disc_uri);
	tracker_sparql_builder_predicate (preupdate, "a");
	tracker_sparql_builder_object (preupdate, "nmm:MusicAlbumDisc");
	tracker_sparql_builder_predicate (preupdate, "nmm:setNumber");
	tracker_sparql_builder_object_int64 (preupdate, has_it ? count : 1);
	tracker_sparql_builder_predicate (preupdate, "nmm:albumDiscAlbum");
	tracker_sparql_builder_object_iri (preupdate, *p_album_uri);

	if (graph) {
		tracker_sparql_builder_graph_close (preupdate);
	}
	tracker_sparql_builder_insert_close (preupdate);

	replace_double_gst_tag (preupdate, *p_album_uri, "nmm:albumGain", extractor->tagcache, GST_TAG_ALBUM_GAIN, graph);
	replace_double_gst_tag (preupdate, *p_album_uri, "nmm:albumPeakGain", extractor->tagcache, GST_TAG_ALBUM_PEAK, graph);

#ifdef HAVE_LIBMEDIAART
	extractor->media_art_artist = album_artist;
	extractor->media_art_title = album_title;
#endif

	g_free (album_artist_temp);
	g_free (track_artist_temp);
}

static void
extractor_apply_device_metadata (MetadataExtractor    *extractor,
                                 GstTagList           *tag_list,
                                 TrackerSparqlBuilder *preupdate,
                                 TrackerSparqlBuilder *metadata,
                                 const gchar          *graph)
{
	gchar *equip_uri;
	gchar *model = NULL, *manuf = NULL;

	gst_tag_list_get_string (tag_list, GST_TAG_DEVICE_MODEL, &model);
	gst_tag_list_get_string (tag_list, GST_TAG_DEVICE_MANUFACTURER, &manuf);

	if (model == NULL && manuf == NULL)
		return;

	equip_uri = tracker_sparql_escape_uri_printf ("urn:equipment:%s:%s:",
	                                              manuf ? manuf : "",
	                                              model ? model : "");

	tracker_sparql_builder_insert_open (preupdate, NULL);
	if (graph) {
		tracker_sparql_builder_graph_open (preupdate, graph);
	}

	tracker_sparql_builder_subject_iri (preupdate, equip_uri);
	tracker_sparql_builder_predicate (preupdate, "a");
	tracker_sparql_builder_object (preupdate, "nfo:Equipment");

	if (manuf) {
		tracker_sparql_builder_predicate (preupdate, "nfo:manufacturer");
		tracker_sparql_builder_object_unvalidated (preupdate, manuf);
	}
	if (model) {
		tracker_sparql_builder_predicate (preupdate, "nfo:model");
		tracker_sparql_builder_object_unvalidated (preupdate, model);
	}

	if (graph) {
		tracker_sparql_builder_graph_close (preupdate);
	}
	tracker_sparql_builder_insert_close (preupdate);

	tracker_sparql_builder_predicate (metadata, "nfo:equipment");
	tracker_sparql_builder_object_iri (metadata, equip_uri);

	g_free (equip_uri);
	g_free (model);
	g_free (manuf);
}

static void
extractor_apply_audio_metadata (MetadataExtractor     *extractor,
                                GstTagList            *tag_list,
                                TrackerSparqlBuilder  *metadata,
                                const gchar           *performer_uri,
                                const gchar           *composer_uri,
                                const gchar           *album_uri,
                                const gchar           *album_disc_uri)
{
	add_uint_gst_tag (metadata, "nmm:trackNumber", tag_list, GST_TAG_TRACK_NUMBER);
	add_string_gst_tag (metadata, "nfo:codec", tag_list, GST_TAG_AUDIO_CODEC);
	add_double_gst_tag (metadata, "nfo:gain", tag_list, GST_TAG_TRACK_GAIN);
	add_double_gst_tag (metadata, "nfo:peakGain", tag_list, GST_TAG_TRACK_PEAK);

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
		tracker_sparql_builder_predicate (metadata, "nmm:musicAlbumDisc");
		tracker_sparql_builder_object_iri (metadata, album_disc_uri);
	}
}

static void
extractor_apply_video_metadata (MetadataExtractor    *extractor,
                                GstTagList           *tag_list,
                                TrackerSparqlBuilder *metadata,
                                const gchar          *performer_uri,
                                const gchar          *composer_uri)
{
	add_string_gst_tag (metadata, "dc:source", tag_list, GST_TAG_CLASSIFICATION);

	if (performer_uri) {
		tracker_sparql_builder_predicate (metadata, "nmm:leadActor");
		tracker_sparql_builder_object_iri (metadata, performer_uri);
	}

	if (composer_uri) {
		tracker_sparql_builder_predicate (metadata, "nmm:director");
		tracker_sparql_builder_object_iri (metadata, composer_uri);
	}

	add_keywords_gst_tag (metadata, tag_list);
}

static void
extract_track_metadata (MetadataExtractor    *extractor,
                        TrackerTocEntry      *toc_entry,
                        const gchar          *file_url,
                        TrackerSparqlBuilder *preupdate,
                        TrackerSparqlBuilder *postupdate,
                        const gchar          *graph,
                        const gchar          *album_uri,
                        const gchar          *album_disc_uri)
{
	gchar *track_performer_uri = NULL;
	gchar *track_composer_uri = NULL;
	gchar *track_uri;

	track_uri = tracker_sparql_get_uuid_urn ();

	tracker_sparql_builder_subject_iri (postupdate, track_uri);

	tracker_sparql_builder_predicate (postupdate, "a");
	tracker_sparql_builder_object (postupdate, "nmm:MusicPiece");
	tracker_sparql_builder_object (postupdate, "nfo:Audio");

	extractor_apply_general_metadata (extractor,
	                                  toc_entry->tag_list,
	                                  file_url,
	                                  preupdate,
	                                  postupdate,
	                                  graph,
	                                  &track_performer_uri,
	                                  &track_composer_uri);

	extractor_apply_audio_metadata (extractor,
	                                toc_entry->tag_list,
	                                postupdate,
	                                track_performer_uri,
	                                track_composer_uri,
	                                album_uri,
	                                album_disc_uri);

	if (toc_entry->duration > 0) {
		tracker_sparql_builder_predicate (postupdate, "nfo:duration");
		tracker_sparql_builder_object_int64 (postupdate, (gint64)toc_entry->duration);
	}

	tracker_sparql_builder_predicate (postupdate, "nfo:audioOffset");
	tracker_sparql_builder_object_double (postupdate, toc_entry->start);

	/* Link the track to its container file. Since the file might not have been
	 * inserted yet, we use a WHERE clause based on its nie:url to refer to it.
	 */
	tracker_sparql_builder_predicate (postupdate, "nie:isStoredAs");
	tracker_sparql_builder_object_variable (postupdate, "file");

	g_free (track_performer_uri);
	g_free (track_composer_uri);

	g_free (track_uri);
}

static void
delete_existing_tracks (TrackerSparqlBuilder *postupdate,
                        const gchar          *graph,
                        const gchar          *file_url)
{
	gchar *sparql;

	/* Delete existing tracks */

	tracker_sparql_builder_delete_open (postupdate, NULL);
	if (graph) {
		tracker_sparql_builder_graph_open (postupdate, graph);
	}

	tracker_sparql_builder_subject_variable (postupdate, "track");
	tracker_sparql_builder_predicate (postupdate, "a");
	tracker_sparql_builder_object (postupdate, "rdfs:Resource");

	if (graph) {
		tracker_sparql_builder_graph_close (postupdate);
	}
	tracker_sparql_builder_delete_close (postupdate);

	sparql = g_strdup_printf ("WHERE { "
	                          "  ?track a nmm:MusicPiece . "
	                          "  ?file a nfo:FileDataObject ; "
	                          "        nie:url \"%s\" . "
	                          "  ?track nie:isStoredAs ?file "
	                          "} \n",
	                          file_url);
	tracker_sparql_builder_append (postupdate, sparql);
	g_free (sparql);
}

#define CHUNK_N_BYTES (2 << 15)

static guint64
extract_gibest_hash (GFile *file)
{
	guint64 buffer[2][CHUNK_N_BYTES/8];
	GInputStream *stream = NULL;
	gssize n_bytes, file_size;
	GError *error = NULL;
	guint64 hash = 0;
	gint i;

	stream = G_INPUT_STREAM (g_file_read (file, NULL, &error));
	if (stream == NULL)
		goto fail;

	/* Extract start/end chunks of the file */
	n_bytes = g_input_stream_read (stream, buffer[0], CHUNK_N_BYTES, NULL, &error);
	if (n_bytes == -1)
		goto fail;

	if (!g_seekable_seek (G_SEEKABLE (stream), -CHUNK_N_BYTES, G_SEEK_END, NULL, &error))
		goto fail;

	n_bytes = g_input_stream_read (stream, buffer[1], CHUNK_N_BYTES, NULL, &error);
	if (n_bytes == -1)
		goto fail;

	for (i = 0; i < G_N_ELEMENTS (buffer[0]); i++)
		hash += buffer[0][i] + buffer[1][i];

	file_size = g_seekable_tell (G_SEEKABLE (stream));

	if (file_size < CHUNK_N_BYTES)
		goto end;

	/* Include file size */
	hash += file_size;
	g_object_unref (stream);

	return hash;

fail:
	g_warning ("Could not get file hash: %s\n", error ? error->message : "Unknown error");
	g_clear_error (&error);

end:
	g_clear_object (&stream);
	return 0;
}

static void
extract_metadata (MetadataExtractor      *extractor,
                  const gchar            *file_url,
                  TrackerSparqlBuilder   *preupdate,
                  TrackerSparqlBuilder   *postupdate,
                  TrackerSparqlBuilder   *metadata,
                  const gchar            *graph)
{
	g_return_if_fail (extractor != NULL);
	g_return_if_fail (preupdate != NULL);
	g_return_if_fail (postupdate != NULL);
	g_return_if_fail (metadata != NULL);

#ifdef HAVE_LIBMEDIAART
	extractor->media_art_type = MEDIA_ART_NONE;
#endif

	if (extractor->toc) {
		gst_tag_list_insert (extractor->tagcache,
		                     extractor->toc->tag_list,
		                     GST_TAG_MERGE_REPLACE);

		if (g_list_length (extractor->toc->entry_list) == 1) {
			/* If we only got one track, stick all the info together and
			 * forget about the table of contents
			 */
			TrackerTocEntry *toc_entry;

			toc_entry = extractor->toc->entry_list->data;
			gst_tag_list_insert (extractor->tagcache,
			                     toc_entry->tag_list,
			                     GST_TAG_MERGE_REPLACE);

			tracker_toc_free (extractor->toc);
			extractor->toc = NULL;
		}
	}

	if (extractor->mime == EXTRACT_MIME_GUESS && !gst_tag_list_is_empty (extractor->tagcache)) {
		extractor_guess_content_type (extractor);
	}

	if (extractor->mime == EXTRACT_MIME_GUESS) {
		g_warning ("Cannot guess real stream type if no tags were read! "
		           "Defaulting to Video.");
		tracker_sparql_builder_predicate (metadata, "a");
		tracker_sparql_builder_object (metadata, "nmm:Video");
	} else {
		tracker_sparql_builder_predicate (metadata, "a");

		if (extractor->mime == EXTRACT_MIME_AUDIO) {
			/* Audio: don't make an nmm:MusicPiece for the file resource if it's
			 * actually a container for an entire album - we will make a
			 * nmm:MusicPiece for each of the tracks inside instead.
			 */
			tracker_sparql_builder_object (metadata, "nfo:Audio");

			if (extractor->toc == NULL || extractor->toc->entry_list == NULL)
				tracker_sparql_builder_object (metadata, "nmm:MusicPiece");

#ifdef HAVE_LIBMEDIAART
			extractor->media_art_type = MEDIA_ART_ALBUM;
#endif
		} else if (extractor->mime == EXTRACT_MIME_VIDEO) {
			tracker_sparql_builder_object (metadata, "nmm:Video");

#ifdef HAVE_LIBMEDIAART
			extractor->media_art_type = MEDIA_ART_VIDEO;
#endif
		} else {
			tracker_sparql_builder_object (metadata, "nfo:Image");

			if (extractor->mime != EXTRACT_MIME_SVG) {
				tracker_sparql_builder_object (metadata, "nmm:Photo");
			} else {
				tracker_sparql_builder_object (metadata, "nfo:VectorImage");
			}
		}
	}

	if (!gst_tag_list_is_empty (extractor->tagcache)) {
		GList *node;
		gchar *performer_uri = NULL;
		gchar *composer_uri = NULL;
		gchar *album_artist_uri = NULL;
		gchar *album_uri = NULL;
		gchar *album_disc_uri = NULL;

		extractor_apply_general_metadata (extractor,
		                                  extractor->tagcache,
		                                  file_url,
		                                  preupdate,
		                                  metadata,
		                                  graph,
		                                  &performer_uri,
		                                  &composer_uri);

		extractor_apply_device_metadata (extractor,
		                                 extractor->tagcache,
		                                 preupdate,
		                                 metadata,
		                                 graph);

		extractor_apply_geolocation_metadata (extractor,
		                                      extractor->tagcache,
		                                      preupdate,
		                                      metadata,
		                                      graph);

		if (extractor->mime == EXTRACT_MIME_VIDEO) {
			extractor_apply_video_metadata (extractor,
			                                extractor->tagcache,
			                                metadata,
			                                performer_uri,
			                                composer_uri);
		}

		if (extractor->mime == EXTRACT_MIME_AUDIO) {
			extractor_apply_album_metadata (extractor,
			                                extractor->tagcache,
			                                preupdate,
			                                graph,
			                                &album_artist_uri,
			                                &album_uri,
			                                &album_disc_uri);

			extractor_apply_audio_metadata (extractor,
			                                extractor->tagcache,
			                                metadata,
			                                performer_uri,
			                                composer_uri,
			                                album_uri,
			                                album_disc_uri);

			/* If the audio file contains multiple tracks, we create the tracks
			 * as abstract information element types and relate them to the
			 * concrete nfo:FileDataObject using nie:isStoredAs.
			 */
			if (extractor->toc && g_list_length (extractor->toc->entry_list) > 1) {
				delete_existing_tracks (postupdate, graph, file_url);

				tracker_sparql_builder_insert_open (postupdate, NULL);
				if (graph) {
					tracker_sparql_builder_graph_open (postupdate, graph);
				}

				for (node = extractor->toc->entry_list; node; node = node->next)
					extract_track_metadata (extractor,
					                        node->data,
					                        file_url,
					                        preupdate,
					                        postupdate,
					                        graph,
					                        album_uri,
					                        album_disc_uri);

				if (graph) {
					tracker_sparql_builder_graph_close (postupdate);
				}
				tracker_sparql_builder_insert_close (postupdate);

				tracker_sparql_builder_where_open (postupdate);
				tracker_sparql_builder_subject_variable (postupdate, "file");
				tracker_sparql_builder_predicate (postupdate, "nie:url");
				tracker_sparql_builder_object_string (postupdate, file_url);
				tracker_sparql_builder_where_close (postupdate);
			}
		}

		g_free (performer_uri);
		g_free (composer_uri);
		g_free (album_uri);
		g_free (album_disc_uri);
		g_free (album_artist_uri);
	}

	/* OpenSubtitles compatible hash */
	if (extractor->mime == EXTRACT_MIME_VIDEO) {
		guint64 hash;
		GFile *file;

		file = g_file_new_for_uri (file_url);
		hash = extract_gibest_hash (file);
		g_object_unref (file);

		if (hash) {
			char *hash_str;

			/* { <foo> a nfo:FileHash; nfo:hashValue "..."; nfo:hashAlgorithm "gibest" } */
			tracker_sparql_builder_predicate (metadata, "nfo:hasHash");

			tracker_sparql_builder_object_blank_open (metadata);
			tracker_sparql_builder_predicate (metadata, "a");
			tracker_sparql_builder_object (metadata, "nfo:FileHash");

			tracker_sparql_builder_predicate (metadata, "nfo:hashValue");
			hash_str = g_strdup_printf ("%" G_GSIZE_MODIFIER "x", hash);
			tracker_sparql_builder_object_string (metadata, hash_str);
			g_free (hash_str);

			tracker_sparql_builder_predicate (metadata, "nfo:hashAlgorithm");
			tracker_sparql_builder_object_string (metadata, "gibest");

			tracker_sparql_builder_object_blank_close (metadata);
		}
	}

	/* If content was encrypted, set it. */
/* #warning TODO: handle encrypted content with the Discoverer/GUPnP-DLNA backends */

	common_extract_stream_metadata (extractor, file_url, metadata);

#ifdef HAVE_LIBMEDIAART
	if (extractor->mime == EXTRACT_MIME_AUDIO) {
		get_embedded_media_art (extractor);
	}
#endif
}

#if defined(GSTREAMER_BACKEND_DISCOVERER) || \
    defined(GSTREAMER_BACKEND_GUPNP_DLNA)
static void
common_extract_stream_metadata (MetadataExtractor    *extractor,
                                const gchar          *uri,
                                TrackerSparqlBuilder *metadata)
{
	if (extractor->mime == EXTRACT_MIME_AUDIO ||
	    extractor->mime == EXTRACT_MIME_VIDEO) {
		if (extractor->audio_channels >= 0) {
			tracker_sparql_builder_predicate (metadata, "nfo:channels");
			tracker_sparql_builder_object_int64 (metadata, extractor->audio_channels);
		}

		if (extractor->audio_samplerate >= 0) {
			tracker_sparql_builder_predicate (metadata, "nfo:sampleRate");
			tracker_sparql_builder_object_int64 (metadata, extractor->audio_samplerate);
		}

		if (extractor->duration >= 0) {
			tracker_sparql_builder_predicate (metadata, "nfo:duration");
			tracker_sparql_builder_object_int64 (metadata, extractor->duration);
		}
	}

	if (extractor->mime == EXTRACT_MIME_VIDEO) {
		if (extractor->video_fps >= 0) {
			tracker_sparql_builder_predicate (metadata, "nfo:frameRate");
			tracker_sparql_builder_object_double (metadata, (gdouble)extractor->video_fps);
		}
	}

	if (extractor->mime == EXTRACT_MIME_IMAGE ||
	    extractor->mime == EXTRACT_MIME_SVG ||
	    extractor->mime == EXTRACT_MIME_VIDEO) {

		if (extractor->width >= 0) {
			tracker_sparql_builder_predicate (metadata, "nfo:width");
			tracker_sparql_builder_object_int64 (metadata, extractor->width);
		}

		if (extractor->height >= 0) {
			tracker_sparql_builder_predicate (metadata, "nfo:height");
			tracker_sparql_builder_object_int64 (metadata, extractor->height);
		}

		if (extractor->aspect_ratio >= 0) {
			tracker_sparql_builder_predicate (metadata, "nfo:aspectRatio");
			tracker_sparql_builder_object_double (metadata, (gdouble)extractor->aspect_ratio);
		}
	}

#if defined(GSTREAMER_BACKEND_GUPNP_DLNA)
	if (extractor->dlna_profile) {
		tracker_sparql_builder_predicate (metadata, "nmm:dlnaProfile");
		tracker_sparql_builder_object_string (metadata, extractor->dlna_profile);
	} else {
		g_debug ("No DLNA profile found");
	}

	if (extractor->dlna_mime) {
		tracker_sparql_builder_predicate (metadata, "nmm:dlnaMime");
		tracker_sparql_builder_object_string (metadata, extractor->dlna_mime);
	} else {
		g_debug ("No DLNA mime found");
	}
#endif /* GSTREAMER_BACKEND_GUPNP_DLNA */
}

#endif

/* ----------------------- Discoverer/GUPnP-DLNA specific implementation --------------- */

#if defined(GSTREAMER_BACKEND_DISCOVERER) || \
    defined(GSTREAMER_BACKEND_GUPNP_DLNA)

static void
discoverer_shutdown (MetadataExtractor *extractor)
{
	if (extractor->streams)
		gst_discoverer_stream_info_list_free (extractor->streams);
	if (extractor->discoverer)
		g_object_unref (extractor->discoverer);
}

static gboolean
discoverer_init_and_run (MetadataExtractor *extractor,
                         const gchar       *uri)
{
	GstDiscovererInfo *info;
	const GstTagList *discoverer_tags;
	GError *error = NULL;
	GList *l;

	extractor->duration = -1;
	extractor->audio_channels = -1;
	extractor->audio_samplerate = -1;
	extractor->height = -1;
	extractor->width = -1;
	extractor->video_fps = -1.0;
	extractor->aspect_ratio = -1.0;

	extractor->has_image = FALSE;
	extractor->has_video = FALSE;
	extractor->has_audio = FALSE;

	extractor->discoverer = gst_discoverer_new (5 * GST_SECOND, &error);
	if (!extractor->discoverer) {
		g_warning ("Couldn't create discoverer: %s",
		           error ? error->message : "unknown error");
		g_clear_error (&error);
		return FALSE;
	}

#if defined(GST_TYPE_DISCOVERER_FLAGS)
	/* Tell the discoverer to use *only* Tagreadbin backend.
	 *  See https://bugzilla.gnome.org/show_bug.cgi?id=656345
	 */
	g_debug ("Using Tagreadbin backend in the GStreamer discoverer...");
	g_object_set (extractor->discoverer,
	              "flags", GST_DISCOVERER_FLAGS_EXTRACT_LIGHTWEIGHT,
	              NULL);
#endif

	info = gst_discoverer_discover_uri (extractor->discoverer,
	                                    uri,
	                                    &error);

	if (!info) {
		g_warning ("Nothing discovered, bailing out");
		return TRUE;
	}

	if (error) {
		g_warning ("Call to gst_discoverer_discover_uri() failed: %s",
		           error->message);
		gst_discoverer_info_unref (info);
		g_error_free (error);
		return FALSE;
	}

#if defined(GSTREAMER_BACKEND_GUPNP_DLNA)
	{
		GUPnPDLNAProfile *profile;
		GUPnPDLNAInformation *dlna_info;
		GUPnPDLNAProfileGuesser *guesser;

		dlna_info = gupnp_dlna_gst_utils_information_from_discoverer_info (info);
		guesser = gupnp_dlna_profile_guesser_new (TRUE, FALSE);
		profile = gupnp_dlna_profile_guesser_guess_profile_from_info (guesser, dlna_info);

		if (profile) {
			extractor->dlna_profile = gupnp_dlna_profile_get_name (profile);
			extractor->dlna_mime = gupnp_dlna_profile_get_mime (profile);
		}

		g_object_unref (guesser);
		g_object_unref (dlna_info);
	}
#endif

	extractor->duration = gst_discoverer_info_get_duration (info) / GST_SECOND;

	/* Retrieve global tags */
	discoverer_tags = gst_discoverer_info_get_tags (info);

	if (discoverer_tags) {
		gst_tag_list_insert (extractor->tagcache,
				     discoverer_tags,
				     GST_TAG_MERGE_APPEND);
	}

	/* Get list of Streams to iterate */
	extractor->streams = gst_discoverer_info_get_stream_list (info);
	for (l = extractor->streams; l; l = g_list_next (l)) {
		GstDiscovererStreamInfo *stream = l->data;
		const GstTagList *stream_tags;

		if (G_TYPE_CHECK_INSTANCE_TYPE (stream, GST_TYPE_DISCOVERER_AUDIO_INFO)) {
			GstDiscovererAudioInfo *audio = (GstDiscovererAudioInfo*)stream;

			extractor->has_audio = TRUE;
			extractor->audio_samplerate = gst_discoverer_audio_info_get_sample_rate (audio);
			extractor->audio_channels = gst_discoverer_audio_info_get_channels (audio);
		} else if (G_TYPE_CHECK_INSTANCE_TYPE (stream, GST_TYPE_DISCOVERER_VIDEO_INFO)) {
			GstDiscovererVideoInfo *video = (GstDiscovererVideoInfo*)stream;

			if (gst_discoverer_video_info_is_image (video)) {
				extractor->has_image = TRUE;
			} else {
				extractor->has_video = TRUE;
				if (gst_discoverer_video_info_get_framerate_denom (video) > 0) {
					extractor->video_fps = (gfloat)(gst_discoverer_video_info_get_framerate_num (video) /
						                        gst_discoverer_video_info_get_framerate_denom (video));
				}
				extractor->width = gst_discoverer_video_info_get_width (video);
				extractor->height = gst_discoverer_video_info_get_height (video);
				if (gst_discoverer_video_info_get_par_denom (video) > 0) {
					extractor->aspect_ratio = (gfloat)(gst_discoverer_video_info_get_par_num (video) /
						                           gst_discoverer_video_info_get_par_denom (video));
				}
			}
		} else {
			/* Unknown type - do nothing */
		}

		stream_tags = gst_discoverer_stream_info_get_tags (stream);

		if (stream_tags) {
			gst_tag_list_insert (extractor->tagcache,
					     stream_tags,
					     GST_TAG_MERGE_APPEND);
		}
	}

	gst_discoverer_info_unref (info);

	return TRUE;
}

#endif /* defined(GSTREAMER_BACKEND_DISCOVERER) || \
          defined(GSTREAMER_BACKEND_GUPNP_DLNA) */

static void
tracker_extract_gstreamer (const gchar          *uri,
                           TrackerExtractInfo   *info,
                           ExtractMime           type,
                           const gchar          *graph)
{
	TrackerSparqlBuilder *metadata, *preupdate, *postupdate;
	MetadataExtractor *extractor;
	GstBuffer *buffer;
	gchar *cue_sheet;
	gboolean success;

#ifdef HAVE_LIBMEDIAART
	MediaArtProcess *media_art_process;
#endif

	g_return_if_fail (uri);

	graph = tracker_extract_info_get_graph (info);
	metadata = tracker_extract_info_get_metadata_builder (info);
	preupdate = tracker_extract_info_get_preupdate_builder (info);
	postupdate = tracker_extract_info_get_postupdate_builder (info);

	g_return_if_fail (metadata);

	gst_init (NULL, NULL);

	extractor = g_slice_new0 (MetadataExtractor);
	extractor->mime = type;
	extractor->tagcache = gst_tag_list_new_empty ();

#ifdef HAVE_LIBMEDIAART
	media_art_process = tracker_extract_info_get_media_art_process (info);
	extractor->media_art_type = MEDIA_ART_NONE;
#endif

	g_debug ("GStreamer backend in use:");
	g_debug ("  Discoverer/GUPnP-DLNA");
	success = discoverer_init_and_run (extractor, uri);

	if (success) {
		cue_sheet = get_embedded_cue_sheet_data (extractor->tagcache);

		if (cue_sheet) {
			g_debug ("Using embedded CUE sheet.");
			extractor->toc = tracker_cue_sheet_parse (cue_sheet);
			g_free (cue_sheet);
		}

		if (extractor->toc == NULL) {
			extractor->toc = tracker_cue_sheet_parse_uri (uri);
		}

		extract_metadata (extractor,
		                  uri,
		                  preupdate,
		                  postupdate,
		                  metadata,
		                  graph);

#ifdef HAVE_LIBMEDIAART
		if (extractor->media_art_type != MEDIA_ART_NONE) {
			GError *error = NULL;
			gboolean success = TRUE;

			if (extractor->media_art_buffer) {
				success = media_art_process_buffer (media_art_process,
				                                    extractor->media_art_type,
				                                    MEDIA_ART_PROCESS_FLAGS_NONE,
				                                    tracker_extract_info_get_file (info),
				                                    extractor->media_art_buffer,
				                                    extractor->media_art_buffer_size,
				                                    extractor->media_art_buffer_mime,
				                                    extractor->media_art_artist,
				                                    extractor->media_art_title,
				                                    &error);
			} else {
				success = media_art_process_file (media_art_process,
				                                  extractor->media_art_type,
				                                  MEDIA_ART_PROCESS_FLAGS_NONE,
				                                  tracker_extract_info_get_file (info),
				                                  extractor->media_art_artist,
				                                  extractor->media_art_title,
				                                  &error);
			}

			if (!success || error) {
				g_warning ("Could not process media art for '%s', %s",
				           uri,
				           error ? error->message : "No error given");
				g_clear_error (&error);
			}
		}
#endif
	}

	/* Clean up */
#ifdef HAVE_LIBMEDIAART
	g_free (extractor->media_art_artist);
	g_free (extractor->media_art_title);
#endif

	if (extractor->sample) {
		buffer = gst_sample_get_buffer (extractor->sample);
		gst_buffer_unmap (buffer, &extractor->info);
		gst_sample_unref (extractor->sample);
	}

	gst_tag_list_free (extractor->tagcache);

	tracker_toc_free (extractor->toc);

	g_slist_foreach (extractor->artist_list, (GFunc)g_free, NULL);
	g_slist_free (extractor->artist_list);

	discoverer_shutdown (extractor);

	g_slice_free (MetadataExtractor, extractor);
}

G_MODULE_EXPORT gboolean
tracker_extract_get_metadata (TrackerExtractInfo *info)
{
	GFile *file;
	gchar *uri;
	const gchar *graph;
	const gchar *mimetype;

	file = tracker_extract_info_get_file (info);
	uri = g_file_get_uri (file);
	graph = tracker_extract_info_get_graph (info);
	mimetype = tracker_extract_info_get_mimetype (info);

#if defined(GSTREAMER_BACKEND_GUPNP_DLNA)
	if (g_str_has_prefix (mimetype, "dlna/")) {
		tracker_extract_gstreamer (uri, info, EXTRACT_MIME_GUESS, graph);
	} else
#endif /* GSTREAMER_BACKEND_GUPNP_DLNA */

	if (strcmp (mimetype, "image/svg+xml") == 0) {
		tracker_extract_gstreamer (uri, info, EXTRACT_MIME_SVG, graph);
	} else if (strcmp (mimetype, "video/3gpp") == 0 ||
	           strcmp (mimetype, "video/mp4") == 0 ||
	           strcmp (mimetype, "video/x-ms-asf") == 0 ||
	           strcmp (mimetype, "application/vnd.rn-realmedia") == 0) {
		tracker_extract_gstreamer (uri, info, EXTRACT_MIME_GUESS, graph);
	} else if (g_str_has_prefix (mimetype, "audio/")) {
		tracker_extract_gstreamer (uri, info, EXTRACT_MIME_AUDIO, graph);
	} else if (g_str_has_prefix (mimetype, "video/")) {
		tracker_extract_gstreamer (uri, info, EXTRACT_MIME_VIDEO, graph);
	} else if (g_str_has_prefix (mimetype, "image/")) {
		tracker_extract_gstreamer (uri, info, EXTRACT_MIME_IMAGE, graph);
	} else {
		g_free (uri);
		return FALSE;
	}

	g_free (uri);
	return TRUE;
}
