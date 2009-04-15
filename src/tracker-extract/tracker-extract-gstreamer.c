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

#include <glib.h>
#include <glib/gstdio.h>

#include <gst/gst.h>
#include <gst/tag/tag.h>

#include <libtracker-common/tracker-type-utils.h>
#include <libtracker-common/tracker-statement-list.h>
#include <libtracker-common/tracker-ontology.h>
#include <libtracker-common/tracker-utils.h>

#include "tracker-main.h"
#include "tracker-extract-albumart.h"

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

typedef enum {
	EXTRACT_MIME_UNDEFINED,
	EXTRACT_MIME_AUDIO,
	EXTRACT_MIME_VIDEO,
	EXTRACT_MIME_IMAGE
} ExtractMime;

typedef struct {
	/* Pipeline elements */
	GMainLoop      *loop;

	GstElement     *bin;
	GstElement     *filesrc;
	GstElement     *cache;
	GstElement     *pipeline;

	GstBus         *bus;
	guint           id;

	GList          *fsinks;

	ExtractMime	mime;


	/* Tags and data */
	GstTagList     *tagcache;

	GstTagList     *audiotags;
	GstTagList     *videotags;

	gint64          duration;
	gint		video_height;
	gint		video_width;
	gint		video_fps_n;
	gint		video_fps_d;
	gint		audio_channels;
	gint		audio_samplerate;

	unsigned char  *album_art_data;
	guint           album_art_size;

} MetadataExtractor;

/* FIXME Make these definable/dynamic at some point */
const guint use_dbin  = 1;
const guint use_cache = 0;

static void extract_gstreamer_audio (const gchar *uri, GPtrArray *metadata);
static void extract_gstreamer_video (const gchar *uri, GPtrArray *metadata);
static void extract_gstreamer_image (const gchar *uri, GPtrArray *metadata);

static TrackerExtractData gstreamer_data[] = {
	{ "audio/*", extract_gstreamer_audio },
	{ "video/*", extract_gstreamer_video },
	{ "image/*", extract_gstreamer_image },
	{ NULL, NULL }
};

static void
add_int64_info (GPtrArray *metadata,
		const gchar *uri,
		const gchar	   *key,
		gint64	    info)
{
	tracker_statement_list_insert_with_int64 (metadata, uri, key, info);
}

static void
add_uint_info (GPtrArray *metadata,
	       const gchar *uri,
	       const gchar	  *key,
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
add_uint_gst_tag (GPtrArray  *metadata,
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

static gint64
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
extract_metadata (MetadataExtractor *extractor,
		  const gchar       *uri,
		  GPtrArray         *metadata,
		  gchar            **artist,
		  gchar            **album,
		  gchar            **scount)
{
	gchar *s;
	gint   n;

	g_return_if_fail (extractor);
	g_return_if_fail (metadata);

	if (extractor->tagcache) {
		/* General */
		add_string_gst_tag (metadata, uri, NIE_PREFIX "copyright", extractor->tagcache, GST_TAG_COPYRIGHT);
		add_string_gst_tag (metadata, uri, NIE_PREFIX "license", extractor->tagcache, GST_TAG_LICENSE);
		add_string_gst_tag (metadata, uri, DC_PREFIX "coverage", extractor->tagcache, GST_TAG_LOCATION);

		/* Audio */
		gst_tag_list_get_string (extractor->tagcache, GST_TAG_ALBUM, &s);
		if (s) {
			gchar *canonical_uri = tracker_uri_printf_escaped ("urn:album:%s", s);
			tracker_statement_list_insert (metadata, canonical_uri, RDF_TYPE, NMM_PREFIX "MusicAlbum");
			tracker_statement_list_insert (metadata, canonical_uri, NMM_PREFIX "albumTitle", s);
			tracker_statement_list_insert (metadata, uri, NMM_PREFIX "musicAlbum", canonical_uri);
			g_free (canonical_uri);
			*album = s;
		}

		add_uint_gst_tag   (metadata, uri, NMM_PREFIX "albumTrackCount", extractor->tagcache, GST_TAG_TRACK_COUNT);


		if (gst_tag_list_get_uint (extractor->tagcache, GST_TAG_TRACK_COUNT, &n)) {
			*scount = g_strdup_printf ("%d", n);
		}

		add_uint_gst_tag   (metadata, uri, NMM_PREFIX "trackNumber", extractor->tagcache, GST_TAG_TRACK_NUMBER);

		add_uint_gst_tag   (metadata, uri, NMM_PREFIX "setNumber", extractor->tagcache, GST_TAG_ALBUM_VOLUME_NUMBER);

		gst_tag_list_get_string (extractor->tagcache, GST_TAG_PERFORMER, &s);
		if (s) {
			gchar *canonical_uri = tracker_uri_printf_escaped ("urn:artist:%s", s);
			tracker_statement_list_insert (metadata, canonical_uri, RDF_TYPE, NMM_PREFIX "Artist");
			tracker_statement_list_insert (metadata, canonical_uri, NMM_PREFIX "artistName", s);
			tracker_statement_list_insert (metadata, uri, NMM_PREFIX "performer", canonical_uri);
			g_free (canonical_uri);
			g_free (s);
		}
	
		add_double_gst_tag (metadata, uri, NFO_PREFIX "gain", extractor->tagcache, GST_TAG_TRACK_GAIN);

		add_double_gst_tag (metadata, uri, NFO_PREFIX "peakGain", extractor->tagcache, GST_TAG_TRACK_PEAK);

		add_double_gst_tag (metadata, uri, NMM_PREFIX "albumGain", extractor->tagcache, GST_TAG_ALBUM_GAIN);
		add_double_gst_tag (metadata, uri, NMM_PREFIX "albumPeakGain", extractor->tagcache, GST_TAG_ALBUM_PEAK);
		
		
		add_y_date_gst_tag (metadata, uri, NIE_PREFIX "contentCreated", extractor->tagcache, GST_TAG_DATE);
		add_string_gst_tag (metadata, uri, NFO_PREFIX "genre", extractor->tagcache, GST_TAG_GENRE);
		add_string_gst_tag (metadata, uri, NFO_PREFIX "codec", extractor->tagcache, GST_TAG_AUDIO_CODEC);

		/* Video */
		add_string_gst_tag (metadata, uri, NFO_PREFIX "codec", extractor->tagcache, GST_TAG_VIDEO_CODEC);

		if (extractor->mime == EXTRACT_MIME_IMAGE) {
			add_string_gst_tag (metadata, uri, NIE_PREFIX "title", extractor->tagcache, GST_TAG_TITLE);
			add_string_gst_tag (metadata, uri, NIE_PREFIX "comment", extractor->tagcache, GST_TAG_COMMENT);

			gst_tag_list_get_string (extractor->tagcache, GST_TAG_ARTIST, &s);
			if (s) {
				gchar *canonical_uri = tracker_uri_printf_escaped ("urn:artist:%s", s);
				tracker_statement_list_insert (metadata, canonical_uri, RDF_TYPE, NMM_PREFIX "Artist");
				tracker_statement_list_insert (metadata, canonical_uri, NMM_PREFIX "artistName", s);
				tracker_statement_list_insert (metadata, uri, NMM_PREFIX "performer", canonical_uri);
				g_free (canonical_uri);
				g_free (s);
			}

		} else if (extractor->mime == EXTRACT_MIME_VIDEO) {
			add_string_gst_tag (metadata, uri, NIE_PREFIX "title", extractor->tagcache, GST_TAG_TITLE);
			add_string_gst_tag (metadata, uri, NIE_PREFIX "comment", extractor->tagcache, GST_TAG_COMMENT);

			gst_tag_list_get_string (extractor->tagcache, GST_TAG_ARTIST, &s);
			if (s) {
				gchar *canonical_uri = tracker_uri_printf_escaped ("urn:artist:%s", s);
				tracker_statement_list_insert (metadata, canonical_uri, RDF_TYPE, NMM_PREFIX "Artist");
				tracker_statement_list_insert (metadata, canonical_uri, NMM_PREFIX "artistName", s);
				tracker_statement_list_insert (metadata, uri, NMM_PREFIX "performer", canonical_uri);
				g_free (canonical_uri);
				g_free (s);
			}

			add_string_gst_tag (metadata, uri, DC_PREFIX "source", extractor->tagcache, GST_TAG_CLASSIFICATION);
		} else if (extractor->mime == EXTRACT_MIME_AUDIO) {
			add_string_gst_tag (metadata, uri, NIE_PREFIX "title", extractor->tagcache, GST_TAG_TITLE);

			gst_tag_list_get_string (extractor->tagcache, GST_TAG_ARTIST, &s);
			if (s) {
				gchar *canonical_uri = tracker_uri_printf_escaped ("urn:artist:%s", s);
				tracker_statement_list_insert (metadata, canonical_uri, RDF_TYPE, NMM_PREFIX "Artist");
				tracker_statement_list_insert (metadata, canonical_uri, NMM_PREFIX "artistName", s);
				tracker_statement_list_insert (metadata, uri, NMM_PREFIX "performer", canonical_uri);
				g_free (canonical_uri);
				g_free (s);
			}

			if (gst_tag_list_get_string (extractor->tagcache, GST_TAG_ARTIST, &s)) {
				*artist = s;
			}

			add_string_gst_tag (metadata, uri, NIE_PREFIX "comment", extractor->tagcache, GST_TAG_COMMENT);
		}
 	}

	if (extractor->audio_channels >= 0) {
		add_uint_info (metadata, uri,
			       NFO_PREFIX "channels",
			       (guint) extractor->audio_channels);
	}

	if (extractor->audio_samplerate >= 0) {
		add_uint_info (metadata, uri,
			       NFO_PREFIX "sampleRate",
			       (guint) extractor->audio_samplerate);
	}

	if (extractor->video_height >= 0) {
		if (extractor->mime == EXTRACT_MIME_IMAGE) {
			add_uint_info (metadata, uri,
				       NFO_PREFIX "height",
				       (guint) extractor->video_height);
		} else {
			add_uint_info (metadata, uri,
				       NFO_PREFIX "height",
				      (guint)  extractor->video_height);
		}
	}

	if (extractor->video_width >= 0) {
		if (extractor->mime == EXTRACT_MIME_IMAGE) {
			add_uint_info (metadata, uri,
				       NFO_PREFIX "width",
				       (guint) extractor->video_width);
		} else {
			add_uint_info (metadata, uri,
				       NFO_PREFIX "width",
				       (guint) extractor->video_width);
		}
	}

 	if (extractor->mime == EXTRACT_MIME_VIDEO) {
 
		tracker_statement_list_insert (metadata, uri, 
		                          RDF_TYPE, 
		                          NMM_PREFIX "Video");

		if (extractor->video_fps_n >= 0 && extractor->video_fps_d >= 0) {
			add_uint_info (metadata, uri,
				       NFO_PREFIX "frameRate",
				       ((extractor->video_fps_n + extractor->video_fps_d / 2) / 
					extractor->video_fps_d));
		}
 		if (extractor->duration >= 0) {
 			add_int64_info (metadata, uri, NFO_PREFIX "duration", extractor->duration);
 		}
 	} else if (extractor->mime == EXTRACT_MIME_AUDIO) {

		tracker_statement_list_insert (metadata, uri, 
		                          RDF_TYPE, 
		                          NMM_PREFIX "MusicPiece");

 		if (extractor->duration >= 0) {
 			add_int64_info (metadata, uri, NMM_PREFIX "length", extractor->duration);
 		}
 	} else if (extractor->mime == EXTRACT_MIME_IMAGE) {
		tracker_statement_list_insert (metadata, uri, 
		                          RDF_TYPE, 
		                          NFO_PREFIX "Image");
	} else {
		tracker_statement_list_insert (metadata, uri, 
		                          RDF_TYPE, 
		                          NFO_PREFIX "FileDataObject");
	}

	if (extractor->audiotags) {
		add_uint_gst_tag (metadata, uri, NFO_PREFIX "averageBitrate", extractor->audiotags, GST_TAG_BITRATE);
	}

	if (extractor->videotags) {
		add_uint_gst_tag (metadata, uri, NFO_PREFIX "averageBitrate", extractor->videotags, GST_TAG_BITRATE);
	}

}

static void
unlink_fsink (void *obj, void *data)
{
	MetadataExtractor *extractor = (MetadataExtractor *)data;
	GstElement        *fsink     = (GstElement *) obj;

	gst_element_unlink (extractor->bin, fsink);
	gst_bin_remove (GST_BIN (extractor->pipeline), fsink);
	gst_element_set_state (fsink, GST_STATE_NULL);
}

static void
dbin_dpad_cb (GstElement* e, GstPad* pad, gboolean cont, gpointer data)
{
	MetadataExtractor *extractor = (MetadataExtractor *)data;
	GstElement        *fsink;
	GstPad            *fsinkpad;
	GValue             val = {0, };

	fsink = gst_element_factory_make ("fakesink", NULL);
	
	g_value_init (&val, G_TYPE_INT);
	g_value_set_int (&val, 50);

	g_object_set_property (G_OBJECT (fsink), "preroll-queue-len", &val);

	g_value_unset (&val);

	extractor->fsinks = g_list_append (extractor->fsinks, fsink);
	
	gst_element_set_state (fsink, GST_STATE_PAUSED);
	
	gst_bin_add (GST_BIN (extractor->pipeline), fsink);
	fsinkpad = gst_element_get_static_pad (fsink, "sink");
	gst_pad_link (pad, fsinkpad);
	gst_object_unref (fsinkpad);
}



static void
add_stream_tags_tagreadbin_for_element (MetadataExtractor *extractor, GstElement *elem)
{
	GstStructure      *s         = NULL;
	GstCaps	          *caps      = NULL;
	GstIterator       *iter      = NULL;
	gboolean           done      = FALSE;
	gpointer           item;

	iter = gst_element_iterate_sink_pads (elem);

	while (!done) {
		switch (gst_iterator_next (iter, &item)) {
		case GST_ITERATOR_OK:
			
			if ((caps = GST_PAD_CAPS (item))) {
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
			gst_object_unref (item);
			break;
		case GST_ITERATOR_RESYNC:
			gst_iterator_resync (iter);
			break;
		case GST_ITERATOR_ERROR:
		case GST_ITERATOR_DONE:
			done = TRUE;
			break;
		default:
			break;
		}
	}
	gst_iterator_free (iter);
}

/* FIXME This is a temporary solution while tagreadbin does not support pad signals */

static void
add_stream_tags_tagreadbin (MetadataExtractor *extractor)
{
	GstIterator       *iter      = NULL;
	gboolean           done      = FALSE;
	gpointer           item;

	iter = gst_bin_iterate_elements (GST_BIN(extractor->bin));
	
	while (!done) {
		switch (gst_iterator_next (iter, &item)) {
		case GST_ITERATOR_OK:
			add_stream_tags_tagreadbin_for_element (extractor, item);
			g_object_unref (item);
			break;
		case GST_ITERATOR_RESYNC:
			gst_iterator_resync (iter);
			break;
		case GST_ITERATOR_ERROR:
		case GST_ITERATOR_DONE:
			done = TRUE;
			break;
		}
	}

	gst_iterator_free (iter);
}

static void
add_stream_tag (void *obj, void *data)
{
	MetadataExtractor *extractor = (MetadataExtractor *)data;
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

	if (use_dbin) {
		g_list_foreach (extractor->fsinks, add_stream_tag, extractor);
	} else {
		add_stream_tags_tagreadbin (extractor);
	}
}

static void
add_tags (GstMessage *msg, MetadataExtractor *extractor)
{
	GstPad       *pad;
	GstTagList   *new_tags;
	GstTagList   *result;

	gst_message_parse_tag (msg, &new_tags);

	result = gst_tag_list_merge (extractor->tagcache,
				     new_tags,
				     GST_TAG_MERGE_KEEP);

	if (extractor->tagcache) {
		gst_tag_list_free (extractor->tagcache);
	}

	extractor->tagcache = result;

	/* media-type-specific tags */
	if (GST_IS_ELEMENT (msg->src) &&
	    (pad = gst_element_get_static_pad (GST_ELEMENT (msg->src), "sink"))) {
		GstTagList  **cache;
		const GstStructure *s;
		GstCaps *caps;

		cache = NULL;
		
		caps = gst_pad_get_caps (pad);
		s = gst_caps_get_structure (caps, 0);

		if (g_strrstr (gst_structure_get_name(s), "audio")) {
			cache = &extractor->audiotags;
		} else if (g_strrstr (gst_structure_get_name(s), "video")) {
			cache = &extractor->videotags;
		}
		
		if (cache) {
			result = gst_tag_list_merge (*cache,
						     new_tags,
						     GST_TAG_MERGE_KEEP);
			if (*cache) {
				gst_tag_list_free (*cache);
			}
			
			*cache = result;
		}

		gst_caps_unref (caps);
	}
	
	gst_tag_list_free (new_tags);
}

static gboolean
metadata_bus_async_cb (GstBus *bus, GstMessage *msg, gpointer data)
{
	MetadataExtractor *extractor = (MetadataExtractor *)data;
	GError            *error     = NULL;
	gboolean           stop      = FALSE;
	
	switch (GST_MESSAGE_TYPE (msg)) {
        case GST_MESSAGE_ERROR:
		gst_message_parse_error (msg, &error, NULL);
		printf ("ERROR: %s\n", error->message);
		g_error_free (error);
		stop = TRUE;
		break;
        case GST_MESSAGE_TAG:
		add_tags (msg, extractor);
		break;
        case GST_MESSAGE_EOS:
		stop = TRUE;
		break;
        case GST_MESSAGE_STATE_CHANGED:
		{
			GstElement *sender = (GstElement *) GST_MESSAGE_SRC (msg);
			if (sender == extractor->pipeline) {
				GstState newstate;
				GstState oldstate;
				gst_message_parse_state_changed (msg, &oldstate, &newstate, NULL);
				if ((oldstate == GST_STATE_READY) && (newstate == GST_STATE_PAUSED)) {
					stop = TRUE;
				}
			}
		}
		break;
	case GST_MESSAGE_DURATION:
		/* The reasoning here is that if we already got duration we should have also
		   all the other data we need since getting the duration should take the longest */
		stop = TRUE;
		break;
        default:
		break;
	}
	
	if (stop) {
		add_stream_tags(extractor);
		gst_element_set_state (extractor->pipeline, GST_STATE_READY);
		gst_element_get_state (extractor->pipeline, NULL, NULL, 5 * GST_SECOND);
		g_list_foreach (extractor->fsinks, unlink_fsink, extractor);
		g_list_free (extractor->fsinks);
		extractor->fsinks = NULL;
		g_main_loop_quit (extractor->loop);
	}
	
	return TRUE;
}

static void
tracker_extract_gstreamer (const gchar *uri,
			   GPtrArray   *metadata,
			   ExtractMime	type)
{
	MetadataExtractor *extractor;
	gchar		  *album = NULL, *artist = NULL, *scount = NULL;

	g_return_if_fail (uri);
	g_return_if_fail (metadata);
	
	g_type_init ();

	gst_init (NULL, NULL);

	extractor               = g_slice_new0 (MetadataExtractor);
	extractor->loop         = NULL;
	extractor->bin          = NULL;
	extractor->filesrc      = NULL;
	extractor->cache        = NULL;
	extractor->pipeline     = NULL;

	extractor->bus          = NULL;
	extractor->id           = 0;

	extractor->fsinks       = NULL;

	extractor->mime         = type;

	extractor->tagcache     = NULL;
	
	extractor->audiotags    = NULL;
	extractor->videotags    = NULL;	

	extractor->duration = -1;
	extractor->video_fps_n = extractor->video_fps_d = -1;
	extractor->video_height = extractor->video_width = -1;
	extractor->audio_channels = -1;
	extractor->audio_samplerate = -1;
	
	extractor->pipeline = gst_element_factory_make ("pipeline", NULL);
	if (!extractor->pipeline) {
		g_error ("Failed to create pipeline");
		return;
	}
	extractor->filesrc  = gst_element_factory_make ("filesrc",  NULL);
	if (!extractor->filesrc) {
		g_error ("Failed to create filesrc");
		return;
	}
	if (use_cache) {
		extractor->cache    = gst_element_factory_make ("cache",    NULL);
		if (!extractor->cache) {
			g_error ("Failed to create cache");
			return;
		}
	}

	if (use_dbin) {
		extractor->bin = gst_element_factory_make ("decodebin2", "decodebin2");
		if (!extractor->bin) {
			g_error ("Failed to create decodebin");
			return;
		}
		extractor->id = g_signal_connect (G_OBJECT (extractor->bin), 
				       "new-decoded-pad",
				       G_CALLBACK (dbin_dpad_cb), 
				       extractor);
	} else {
	        extractor->bin = gst_element_factory_make ("tagreadbin", "tagreadbin");
		if (!extractor->bin) {
			g_error ("Failed to create tagreadbin");
			return;
		}
		extractor->id = 0;
	}
	
	gst_bin_add (GST_BIN (extractor->pipeline), extractor->filesrc);
	gst_bin_add (GST_BIN (extractor->pipeline), extractor->bin);

	if (use_cache) {
		gst_bin_add (GST_BIN (extractor->pipeline), extractor->cache);
		if (! gst_element_link_many (extractor->filesrc, extractor->cache, extractor->bin, NULL)) {
		g_error ("Can't link elements\n");
		/* FIXME Clean up */
		return;
		}
	} else {
		if (!gst_element_link_many (extractor->filesrc, extractor->bin, NULL)) {
			g_error ("Can't link elements\n");
			/* FIXME Clean up */
			return;
		}
	}

	extractor->loop = g_main_loop_new (NULL, FALSE);
	extractor->bus = gst_pipeline_get_bus (GST_PIPELINE (extractor->pipeline));
	gst_bus_add_watch (extractor->bus, metadata_bus_async_cb, extractor);

	g_object_set (G_OBJECT (extractor->filesrc), "location", uri, NULL);

	gst_element_set_state (extractor->pipeline, GST_STATE_PAUSED);

	g_main_loop_run (extractor->loop);

	extract_metadata (extractor, uri, metadata, 
			  &artist, &album, &scount);

	/* Save embedded art */
	if (extractor->album_art_data && extractor->album_art_size) {
#ifdef HAVE_GDKPIXBUF
		tracker_process_albumart (extractor->album_art_data, extractor->album_art_size,
					  /* artist */ NULL,
					  album,
					  scount,
					  uri);
#else
		tracker_process_albumart (NULL, 0,
					  /* artist */ NULL,
					  album,
					  scount,
					  uri);
		
#endif /* HAVE_GDKPIXBUF */
	}

	g_free (album);
	g_free (artist);
	g_free (scount);

	gst_element_set_state (extractor->pipeline, GST_STATE_NULL);
	gst_object_unref (extractor->bus);

	if (extractor->tagcache) {
		gst_tag_list_free (extractor->tagcache);
	}

	if (extractor->audiotags) {
		gst_tag_list_free (extractor->audiotags);
	}

	if (extractor->videotags) {
		gst_tag_list_free (extractor->videotags);
	}

	gst_object_unref (GST_OBJECT (extractor->pipeline));
	g_main_loop_unref (extractor->loop);
	g_slice_free (MetadataExtractor, extractor);

	if (type == EXTRACT_MIME_IMAGE) {
		if (!tracker_statement_list_find (metadata, uri, "Image:Date")) {
			struct stat st;
			
			if (g_lstat (uri, &st) >= 0) {
				gchar *date;

				date = tracker_date_to_string (st.st_mtime);

				tracker_statement_list_insert (metadata, uri,
							  "Image:Date",
							  date);

				g_free (date);
			}
		}
	} else if (type == EXTRACT_MIME_VIDEO) {
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
	} else if (type == EXTRACT_MIME_AUDIO) {
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

}


static void
extract_gstreamer_audio (const gchar *uri, GPtrArray *metadata)
{
	tracker_extract_gstreamer (uri, metadata, EXTRACT_MIME_AUDIO);
}

static void
extract_gstreamer_video (const gchar *uri, GPtrArray *metadata)
{
	tracker_extract_gstreamer (uri, metadata, EXTRACT_MIME_VIDEO);
}

static void
extract_gstreamer_image (const gchar *uri, GPtrArray *metadata)
{
	tracker_extract_gstreamer (uri, metadata, EXTRACT_MIME_IMAGE);
}

TrackerExtractData *
tracker_get_extract_data (void)
{
	return gstreamer_data;
}

