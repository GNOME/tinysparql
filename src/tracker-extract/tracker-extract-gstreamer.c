/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Laurent Aguerreche (laurent.aguerreche@free.fr)
 * Copyright (C) 2007, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia
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

#include <libtracker-common/tracker-type-utils.h>
#include <libtracker-common/tracker-file-utils.h>
#include <libtracker-common/tracker-ontology.h>
#include <libtracker-common/tracker-utils.h>

#include "tracker-main.h"
#include "tracker-extract-albumart.h"

/* We wait this long (seconds) for NULL state before freeing */
#define TRACKER_EXTRACT_GUARD_TIMEOUT 3

/* An additional tag in gstreamer for the content source. Remove when in upstream */
#ifndef GST_TAG_CLASSIFICATION
#define GST_TAG_CLASSIFICATION "classification"
#endif

#define NMM_PREFIX TRACKER_NMM_PREFIX
#define NFO_PREFIX TRACKER_NFO_PREFIX
#define NIE_PREFIX TRACKER_NIE_PREFIX
#define DC_PREFIX TRACKER_DC_PREFIX
#define NCO_PREFIX TRACKER_NCO_PREFIX
#define RDF_PREFIX TRACKER_RDF_PREFIX
#define RDF_TYPE RDF_PREFIX "type"

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
	EXTRACT_MIME_IMAGE
} ExtractMime;

typedef struct {
	/* Common pipeline elements */
	GstElement     *pipeline;

	GstBus         *bus;

	ExtractMime	mime;


	/* Decodebin elements and properties*/
	GstElement     *bin;	
	GList          *fsinks;

	gint64          duration;
	gint		video_height;
	gint		video_width;
	gint		video_fps_n;
	gint		video_fps_d;
	gint		audio_channels;
	gint		audio_samplerate;

	/* Tags and data */
	GstTagList     *tagcache;

	unsigned char  *album_art_data;
	guint           album_art_size;
	const gchar    *album_art_mime;

} MetadataExtractor;

static void extract_gstreamer_audio (const gchar *uri, GPtrArray *metadata);
static void extract_gstreamer_video (const gchar *uri, GPtrArray *metadata);
static void extract_gstreamer_image (const gchar *uri, GPtrArray *metadata);

static TrackerExtractData data[] = {
	{ "audio/*", extract_gstreamer_audio },
	{ "video/*", extract_gstreamer_video },
	{ "image/*", extract_gstreamer_image },
	{ NULL, NULL }
};

/* Not using define directly since we might want to make this dynamic */
#ifdef TRACKER_EXTRACT_GSTREAMER_USE_TAGREADBIN
const gboolean use_tagreadbin = TRUE;
#else
const gboolean use_tagreadbin = FALSE;
#endif

static void
add_int64_info (GPtrArray *metadata,
		const gchar *uri,
		const gchar *key,
		gint64	    info)
{
	tracker_statement_list_insert_with_int64 (metadata, uri, key, info);
}

static void
add_uint_info (GPtrArray *metadata,
	       const gchar *uri,
	       const gchar *key,
	       guint	   info)
{
	tracker_statement_list_insert_with_int (metadata, uri, key, info);
}

static void
add_string_gst_tag (GPtrArray	*metadata,
		    const gchar *uri,
		    const gchar *key,
		    GstTagList	*tag_list,
		    const gchar *tag)
{
	gchar	 *s;
	gboolean  ret;

	s = NULL;
	ret = gst_tag_list_get_string (tag_list, tag, &s);

	if (s) {
		if (ret && s[0] != '\0') {
			tracker_statement_list_insert (metadata, uri, key, s);
		}

		g_free (s);
	}
}

static void
add_uint_gst_tag (GPtrArray   *metadata,
		  const gchar *uri,
		  const gchar *key,
		  GstTagList  *tag_list,
		  const gchar *tag)
{
	gboolean ret;
	guint	 n;

	ret = gst_tag_list_get_uint (tag_list, tag, &n);

	if (ret) {
		tracker_statement_list_insert_with_int (metadata, uri, key, n);
	}
}

static void
add_int_gst_tag (GPtrArray   *metadata,
		 const gchar *uri,
		 const gchar *key,
		 GstTagList  *tag_list,
		 const gchar *tag)
{
	gboolean ret;
	gint	 n;

	ret = gst_tag_list_get_int (tag_list, tag, &n);

	if (ret) {
		tracker_statement_list_insert_with_int (metadata, uri, key, n);	
	}
}

static void
add_double_gst_tag (GPtrArray	*metadata,
		    const gchar *uri,
		    const gchar *key,
		    GstTagList	*tag_list,
		    const gchar *tag)
{
	gboolean ret;
	gdouble	 n;

	ret = gst_tag_list_get_double (tag_list, tag, &n);

	if (ret) {
		tracker_statement_list_insert_with_double (metadata, uri, key, n);
	}
}

static void
add_fraction_gst_tag (GPtrArray         *metadata,
		      const gchar       *uri,
		      const gchar       *key,
		      GstTagList	*tag_list,
		      const gchar       *tag)
{
	gboolean ret;
	GValue	 n = {0,};
	gfloat   f;

	ret = gst_tag_list_copy_value (&n, tag_list, tag);

	f = (gfloat)gst_value_get_fraction_numerator (&n)/
		gst_value_get_fraction_denominator (&n);

	if (ret) {
		tracker_statement_list_insert_with_float (metadata, uri, key, f);
	}
}

static void
add_y_date_gst_tag (GPtrArray  *metadata,
		    const gchar *uri,
		    const gchar *key,
		    GstTagList  *tag_list,
		    const gchar *tag)
{
	GDate	 *date;
	gboolean  ret;

	date = NULL;
	ret = gst_tag_list_get_date (tag_list, tag, &date);

	if (ret) {
		gchar buf[10];

		if (g_date_strftime (buf, 10, "%Y", date)) {
			tracker_statement_list_insert (metadata, uri,
						       key, buf);
		}
	}

	if (date) {
		g_date_free (date);
	}
}

static void
add_time_gst_tag (GPtrArray   *metadata,
		  const gchar *uri,
		  const gchar *key,
		  GstTagList  *tag_list,
		  const gchar *tag)
{
	gboolean ret;
	guint64	 n;

	ret = gst_tag_list_get_uint64 (tag_list, tag, &n);

	if (ret) {
		gchar *str = g_strdup_printf ("%lld", llroundl ((long double)n/(long double)GST_SECOND));
		tracker_statement_list_insert (metadata, uri, key, str);
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
				gst_object_unref (caps);
				return TRUE;
			}

			gst_object_unref (caps);

			lindex++;
		}
	} while (value);

	value = gst_tag_list_get_value_index (extractor->tagcache, GST_TAG_PREVIEW_IMAGE, lindex);

	if (value) {
		GstBuffer    *buffer;
		GstCaps      *caps;
		GstStructure *caps_struct;

		buffer = gst_value_get_buffer (value);
		caps   = gst_buffer_get_caps (buffer);
		caps_struct = gst_caps_get_structure (buffer->caps, 0);

		extractor->album_art_data = buffer->data;
		extractor->album_art_size = buffer->size;
		extractor->album_art_mime = gst_structure_get_name (caps_struct);		

		gst_object_unref (caps);

		return TRUE;
	}

	return FALSE;
}

static void
extract_stream_metadata_tagreadbin (MetadataExtractor *extractor,
				    const gchar       *uri,
				    GPtrArray         *metadata)
{
	if (extractor->mime != EXTRACT_MIME_IMAGE) {
		add_uint_gst_tag (metadata, uri, NFO_PREFIX "channels", extractor->tagcache, GST_TAG_CHANNEL);
		add_uint_gst_tag (metadata, uri, NFO_PREFIX "sampleRate", extractor->tagcache, GST_TAG_RATE);
		add_time_gst_tag (metadata, uri, NFO_PREFIX "duration", extractor->tagcache, GST_TAG_DURATION); 
	}

	add_int_gst_tag (metadata, uri, NFO_PREFIX "height", extractor->tagcache, GST_TAG_HEIGHT);
	add_int_gst_tag (metadata, uri, NFO_PREFIX "width", extractor->tagcache, GST_TAG_WIDTH);

	if (extractor->mime == EXTRACT_MIME_VIDEO) {
		add_fraction_gst_tag   (metadata, uri, NFO_PREFIX "frameRate", extractor->tagcache, GST_TAG_FRAMERATE);	
	}
}

static void
extract_stream_metadata_decodebin (MetadataExtractor *extractor,
				   const gchar       *uri,
				   GPtrArray         *metadata)
{
	if (extractor->mime != EXTRACT_MIME_IMAGE) {
		if (extractor->audio_channels >= 0) {
			add_uint_info (metadata,
				       uri, NFO_PREFIX "channels",
				       extractor->audio_channels);
		}

		if (extractor->audio_samplerate >= 0) {
			add_uint_info (metadata,
				       uri, NFO_PREFIX "sampleRate",
				       extractor->audio_samplerate);
		}

		if (extractor->duration >= 0) {
			add_int64_info (metadata, uri, NFO_PREFIX "duration", extractor->duration);
		}
	}

	if (extractor->video_height >= 0) {
		add_uint_info (metadata,
			       uri, NFO_PREFIX "height",
			       extractor->video_height);
	}

	if (extractor->video_width >= 0) {
		add_uint_info (metadata,
			       uri, NFO_PREFIX "width",
			       extractor->video_width);
	}

	if (extractor->mime == EXTRACT_MIME_VIDEO) {
		if (extractor->video_fps_n >= 0 && extractor->video_fps_d >= 0) {
			add_uint_info (metadata,
				       uri, NFO_PREFIX "frameRate",
				       ((extractor->video_fps_n + extractor->video_fps_d / 2) / 
					extractor->video_fps_d));
		}
	}
}

static void
extract_metadata (MetadataExtractor *extractor,
		  const gchar       *uri,
		  GPtrArray         *metadata,
		  gchar **album, gchar **scount)
{
	gchar *s;
	gboolean ret;
	gint count;

	g_return_if_fail (extractor != NULL);
	g_return_if_fail (metadata != NULL);

	if (extractor->tagcache) {
		/* General */

		add_string_gst_tag (metadata, uri, NIE_PREFIX "title", extractor->tagcache, GST_TAG_TITLE);
		add_string_gst_tag (metadata, uri, NIE_PREFIX "copyright", extractor->tagcache, GST_TAG_COPYRIGHT);
		add_string_gst_tag (metadata, uri, NIE_PREFIX "license", extractor->tagcache, GST_TAG_LICENSE);
		add_string_gst_tag (metadata, uri, DC_PREFIX "coverage", extractor->tagcache, GST_TAG_LOCATION);
		add_y_date_gst_tag (metadata, uri, NIE_PREFIX "contentCreated", extractor->tagcache, GST_TAG_DATE);
		add_string_gst_tag (metadata, uri, NIE_PREFIX "comment", extractor->tagcache, GST_TAG_COMMENT);

		if (extractor->mime == EXTRACT_MIME_VIDEO) {
			add_string_gst_tag (metadata, uri, DC_PREFIX "source", extractor->tagcache, GST_TAG_CLASSIFICATION);
		}

		if (extractor->mime == EXTRACT_MIME_AUDIO) {
			/* Audio */
			s = NULL;
			gst_tag_list_get_string (extractor->tagcache, GST_TAG_ALBUM, &s);
			if (s) {
				gchar *canonical_uri = tracker_uri_printf_escaped ("urn:album:%s", s);
				tracker_statement_list_insert (metadata, canonical_uri, RDF_TYPE, NMM_PREFIX "MusicAlbum");
				tracker_statement_list_insert (metadata, canonical_uri, NMM_PREFIX "albumTitle", s);
				tracker_statement_list_insert (metadata, uri, NMM_PREFIX "musicAlbum", canonical_uri);
				g_free (canonical_uri);
				*album = s;
			}

			ret = gst_tag_list_get_uint (extractor->tagcache, GST_TAG_TRACK_COUNT, &count);
			if (ret) {
				*scount = g_strdup_printf ("%d", count);
			}

			add_uint_gst_tag   (metadata, uri, NMM_PREFIX "albumTrackCount", extractor->tagcache, GST_TAG_TRACK_COUNT);
			add_uint_gst_tag   (metadata, uri, NMM_PREFIX "trackNumber", extractor->tagcache, GST_TAG_TRACK_NUMBER);
			add_uint_gst_tag   (metadata, uri, NMM_PREFIX "setNumber", extractor->tagcache, GST_TAG_ALBUM_VOLUME_NUMBER);

			add_double_gst_tag (metadata, uri, NFO_PREFIX "gain", extractor->tagcache, GST_TAG_TRACK_GAIN);
			add_double_gst_tag (metadata, uri, NFO_PREFIX "peakGain", extractor->tagcache, GST_TAG_TRACK_PEAK);
			add_double_gst_tag (metadata, uri, NFO_PREFIX "albumGain", extractor->tagcache, GST_TAG_ALBUM_GAIN);
			add_double_gst_tag (metadata, uri, NFO_PREFIX "albumPeakGain", extractor->tagcache, GST_TAG_ALBUM_PEAK);
		}

		if (extractor->mime == EXTRACT_MIME_AUDIO || extractor->mime == EXTRACT_MIME_VIDEO) {
			s = NULL;
			gst_tag_list_get_string (extractor->tagcache, GST_TAG_PERFORMER, &s);
			if (s) {
				gchar *canonical_uri = tracker_uri_printf_escaped ("urn:artist:%s", s);
				tracker_statement_list_insert (metadata, canonical_uri, RDF_TYPE, NMM_PREFIX "Artist");
				tracker_statement_list_insert (metadata, canonical_uri, NMM_PREFIX "artistName", s);
				tracker_statement_list_insert (metadata, uri, 
							       (extractor->mime == EXTRACT_MIME_AUDIO ? 
							        NMM_PREFIX "performer" :
							        NMM_PREFIX "leadActor"),
							       canonical_uri);
				g_free (canonical_uri);
				g_free (s);
			}

			/* Warn, same predicate as above. Is this an err in our onto? */
			s = NULL;
			gst_tag_list_get_string (extractor->tagcache, GST_TAG_ARTIST, &s);
			if (s) {
				gchar *canonical_uri = tracker_uri_printf_escaped ("urn:artist:%s", s);
				tracker_statement_list_insert (metadata, canonical_uri, RDF_TYPE, NMM_PREFIX "Artist");
				tracker_statement_list_insert (metadata, canonical_uri, NMM_PREFIX "artistName", s);
				tracker_statement_list_insert (metadata, uri, 
							       (extractor->mime == EXTRACT_MIME_AUDIO ? 
							        NMM_PREFIX "performer" :
							        NMM_PREFIX "leadActor"),
							       canonical_uri);
				g_free (canonical_uri);
				g_free (s);
			}

			s = NULL;
			gst_tag_list_get_string (extractor->tagcache, GST_TAG_COMPOSER, &s);
			if (s) {
				gchar *canonical_uri = tracker_uri_printf_escaped ("urn:artist:%s", s);
				tracker_statement_list_insert (metadata, canonical_uri, RDF_TYPE, NMM_PREFIX "Artist");
				tracker_statement_list_insert (metadata, canonical_uri, NMM_PREFIX "artistName", s);
				tracker_statement_list_insert (metadata, uri, 
							       (extractor->mime == EXTRACT_MIME_AUDIO ? 
							        NMM_PREFIX "composer" :
							        NMM_PREFIX "director"),
							       canonical_uri);
				g_free (canonical_uri);
				g_free (s);
			}

			s = NULL;
			gst_tag_list_get_string (extractor->tagcache, GST_TAG_GENRE, &s);
			if (g_strcmp0 (s, "Unknown") != 0) {
				tracker_statement_list_insert (metadata, uri, NFO_PREFIX "genre", s);
			}
			g_free (s);

			add_string_gst_tag (metadata, uri, NFO_PREFIX "codec", extractor->tagcache, GST_TAG_AUDIO_CODEC);

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
	gint64	  duration;
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
	GstCaps	          *caps      = NULL;

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
					(gst_structure_get_int (s, "height", &extractor->video_height)))) {
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
		GstState state,
		gboolean ready_with_state,
		gboolean ready_with_eos)
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
create_decodebin_pipeline (MetadataExtractor *extractor, const gchar *uri)
{
	GstElement *pipeline = NULL;
	
	GstElement *filesrc  = NULL;
	GstElement *bin      = NULL;

	guint       id;

	pipeline = gst_element_factory_make ("pipeline", NULL);
	if (!pipeline) {
		g_warning ("Failed to create GStreamer pipeline");
		return FALSE;
	}

	filesrc = gst_element_factory_make ("filesrc", NULL);
	if (!filesrc) {
		g_warning ("Failed to create GStreamer filesrc");
		return FALSE;
	}

	bin = gst_element_factory_make ("decodebin2", "decodebin2");
	if (!bin) {
		g_warning ("Failed to create GStreamer decodebin");
		return FALSE;
	}

	id = g_signal_connect (G_OBJECT (bin), 
			       "new-decoded-pad",
			       G_CALLBACK (dbin_dpad_cb), 
			       extractor);
	
	gst_bin_add (GST_BIN (pipeline), filesrc);
	gst_bin_add (GST_BIN (pipeline), bin);

	if (!gst_element_link_many (filesrc, bin, NULL)) {
		g_warning ("Could not link GStreamer elements");
		return FALSE;
	}

	g_object_set (G_OBJECT (filesrc), "location", uri, NULL);

	extractor->bin = bin;

	return pipeline;
}

static GstElement *
create_tagreadbin_pipeline (MetadataExtractor *extractor, const gchar *uri)
{
	GstElement *pipeline = NULL;
	gchar      *complete_uri = NULL;

	pipeline = gst_element_factory_make ("tagreadbin", "tagreadbin");
	if (!pipeline) {
		g_warning ("Failed to create GStreamer tagreadbin");
		return NULL;
	}

	complete_uri = g_filename_to_uri (uri, NULL, NULL);
	if (!complete_uri) {
		g_warning ("Failed to convert filename to uri");
		return NULL;
	}

	g_object_set (G_OBJECT (pipeline), "uri", complete_uri, NULL);
	g_free (complete_uri);
	return pipeline;
}


static void
tracker_extract_gstreamer (const gchar *uri,
			   GPtrArray  *metadata,
			   ExtractMime	type)
{
	MetadataExtractor *extractor;
	gchar *album, *scount;

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
	extractor->audio_samplerate = -1;

	if (use_tagreadbin) {
		extractor->pipeline = create_tagreadbin_pipeline (extractor, uri);
	} else {
		extractor->pipeline = create_decodebin_pipeline (extractor, uri);
	}

	if (!extractor->pipeline) {
		g_warning ("No valid pipeline for uri %s", uri);
		goto fail;
	}

	extractor->bus = gst_pipeline_get_bus (GST_PIPELINE (extractor->pipeline));

	if (use_tagreadbin) {
		if (!poll_for_ready (extractor, GST_STATE_PLAYING, FALSE, TRUE)) {
			g_warning ("Error running tagreadbin");
			goto fail;
		}			
	} else {
		if (!poll_for_ready (extractor, GST_STATE_PAUSED, TRUE, FALSE)) {
			g_warning ("Error running decodebin");
			goto fail;
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

	extract_metadata (extractor, uri, metadata, &album, &scount);

	/* Save embedded art */
	if (extractor->album_art_data && extractor->album_art_size) {
#ifdef HAVE_GDKPIXBUF
		tracker_process_albumart (extractor->album_art_data, extractor->album_art_size, extractor->album_art_mime,
					  /* g_hash_table_lookup (metadata, "Audio:Artist") */ NULL,
					  album,
					  scount,
					  uri);
#else
		tracker_process_albumart (NULL, 0, NULL,
					  /* g_hash_table_lookup (metadata, "Audio:Artist") */ NULL,
					  album,
					  scount,
					  uri);
		
#endif /* HAVE_GDKPIXBUF */
	}

	g_free (scount);
	g_free (album);

	gst_element_set_state (extractor->pipeline, GST_STATE_NULL);
	gst_element_get_state (extractor->pipeline, NULL, NULL, TRACKER_EXTRACT_GUARD_TIMEOUT* GST_SECOND);
	gst_object_unref (extractor->bus);

	if (extractor->tagcache) {
		gst_tag_list_free (extractor->tagcache);
	}

	gst_object_unref (GST_OBJECT (extractor->pipeline));
	g_slice_free (MetadataExtractor, extractor);

fail:
	if (type == EXTRACT_MIME_IMAGE) {
		/* We fallback to the file's modified time for the
		 * "Image:Date" metadata if it doesn't exist.
		 *
		 * FIXME: This shouldn't be necessary.
		 */
		if (!tracker_statement_list_find (metadata, uri, NIE_PREFIX "contentCreated")) {
			gchar *date;
			guint64 mtime;
			
			mtime = tracker_file_get_mtime (uri);
			date = tracker_date_to_string ((time_t) mtime);

			tracker_statement_list_insert (metadata, uri, 
						       NIE_PREFIX "contentCreated",
						       date);
			g_free (date);
		}
	}
 
	if (!tracker_statement_list_find (metadata, uri, NIE_PREFIX "title")) {
		gchar  *basename = g_filename_display_basename (uri);
		gchar **parts    = g_strsplit (basename, ".", -1);
		gchar  *title    = g_strdup (parts[0]);
		
		g_strfreev (parts);
		g_free (basename);

		title = g_strdelimit (title, "_", ' ');

		tracker_statement_list_insert (metadata, uri, 
					       NIE_PREFIX "title",
					       title);
		g_free (title);
	}
}


static void
extract_gstreamer_audio (const gchar *uri, GPtrArray *metadata)
{
	tracker_statement_list_insert (metadata, uri, 
				       RDF_TYPE, 
				       NMM_PREFIX "MusicPiece");

	tracker_extract_gstreamer (uri, metadata, EXTRACT_MIME_AUDIO);
}

static void
extract_gstreamer_video (const gchar *uri, GPtrArray *metadata)
{
	tracker_statement_list_insert (metadata, uri, 
				       RDF_TYPE, 
				       NMM_PREFIX "Video");

	tracker_extract_gstreamer (uri, metadata, EXTRACT_MIME_VIDEO);
}

static void
extract_gstreamer_image (const gchar *uri, GPtrArray *metadata)
{
	tracker_statement_list_insert (metadata, uri, 
				       RDF_TYPE, 
				       NFO_PREFIX "Image");

	tracker_extract_gstreamer (uri, metadata, EXTRACT_MIME_IMAGE);
}

TrackerExtractData *
tracker_get_extract_data (void)
{
	return data;
}

