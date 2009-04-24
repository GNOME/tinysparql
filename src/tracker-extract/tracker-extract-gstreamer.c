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

#include "tracker-main.h"
#include "tracker-extract-albumart.h"

/* We wait this long (seconds) for NULL state before freeing */
#define TRACKER_EXTRACT_GUARD_TIMEOUT 3

/* An additional tag in gstreamer for the content source. Remove when in upstream */
#ifndef GST_TAG_CLASSIFICATION
#define GST_TAG_CLASSIFICATION "classification"
#endif

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

static void extract_gstreamer_audio (const gchar *uri, GHashTable *metadata);
static void extract_gstreamer_video (const gchar *uri, GHashTable *metadata);
static void extract_gstreamer_image (const gchar *uri, GHashTable *metadata);

static TrackerExtractData data[] = {
	{ "audio/*", extract_gstreamer_audio },
	{ "video/*", extract_gstreamer_video },
	{ "image/*", extract_gstreamer_image },
	{ NULL, NULL }
};

static void
add_int64_info (GHashTable *metadata,
		gchar	   *key,
		gint64	    info)
{
	gchar *str_info;

	str_info = tracker_escape_metadata_printf ("%" G_GINT64_FORMAT, info);
	g_hash_table_insert (metadata, key, str_info);
}

static void
add_uint_info (GHashTable *metadata,
	       gchar	  *key,
	       guint	   info)
{
	gchar *str_info;

	str_info = tracker_escape_metadata_printf ("%d", info);
	g_hash_table_insert (metadata, key, str_info);
}

static void
add_string_gst_tag (GHashTable	*metadata,
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
			g_hash_table_insert (metadata,
					     g_strdup (key),
					     tracker_escape_metadata (s));
		}

		g_free (s);
	}
}

static void
add_uint_gst_tag (GHashTable  *metadata,
		  const gchar *key,
		  GstTagList  *tag_list,
		  const gchar *tag)
{
	gboolean ret;
	guint	 n;

	ret = gst_tag_list_get_uint (tag_list, tag, &n);

	if (ret) {
		g_hash_table_insert (metadata,
				     g_strdup (key),
				     tracker_escape_metadata_printf ("%d", n));
	}
}

static void
add_double_gst_tag (GHashTable	*metadata,
		    const gchar *key,
		    GstTagList	*tag_list,
		    const gchar *tag)
{
	gboolean ret;
	gdouble	 n;

	ret = gst_tag_list_get_double (tag_list, tag, &n);

	if (ret) {
		g_hash_table_insert (metadata,
				     g_strdup (key),
				     tracker_escape_metadata_printf ("%f", n));
	}
}

static void
add_y_date_gst_tag (GHashTable  *metadata,
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
			g_hash_table_insert (metadata,
					     g_strdup (key),
					     tracker_escape_metadata (buf));
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
		  GHashTable        *metadata)
{
	gchar *value;
	g_return_if_fail (extractor);
	g_return_if_fail (metadata);

	if (extractor->tagcache) {
		/* General */
		add_string_gst_tag (metadata, "File:Copyright", extractor->tagcache, GST_TAG_COPYRIGHT);
		add_string_gst_tag (metadata, "File:License", extractor->tagcache, GST_TAG_LICENSE);
		add_string_gst_tag (metadata, "DC:Coverage", extractor->tagcache, GST_TAG_LOCATION);

		/* Audio */
 		add_string_gst_tag (metadata, "Audio:Album", extractor->tagcache, GST_TAG_ALBUM);
		add_uint_gst_tag   (metadata, "Audio:AlbumTrackCount", extractor->tagcache, GST_TAG_TRACK_COUNT);
		add_uint_gst_tag   (metadata, "Audio:TrackNo", extractor->tagcache, GST_TAG_TRACK_NUMBER);
		add_uint_gst_tag   (metadata, "Audio:DiscNo", extractor->tagcache, GST_TAG_ALBUM_VOLUME_NUMBER);
		add_string_gst_tag (metadata, "Audio:Performer", extractor->tagcache, GST_TAG_PERFORMER);
		add_double_gst_tag (metadata, "Audio:TrackGain", extractor->tagcache, GST_TAG_TRACK_GAIN);
		add_double_gst_tag (metadata, "Audio:PeakTrackGain", extractor->tagcache, GST_TAG_TRACK_PEAK);
		add_double_gst_tag (metadata, "Audio:AlbumGain", extractor->tagcache, GST_TAG_ALBUM_GAIN);
		add_double_gst_tag (metadata, "Audio:AlbumPeakGain", extractor->tagcache, GST_TAG_ALBUM_PEAK);
		add_y_date_gst_tag (metadata, "Audio:ReleaseDate", extractor->tagcache, GST_TAG_DATE);
		add_string_gst_tag (metadata, "Audio:Genre", extractor->tagcache, GST_TAG_GENRE);
		add_string_gst_tag (metadata, "Audio:Codec", extractor->tagcache, GST_TAG_AUDIO_CODEC);

		/* Video */
		add_string_gst_tag (metadata, "Video:Codec", extractor->tagcache, GST_TAG_VIDEO_CODEC);

		if (extractor->mime == EXTRACT_MIME_IMAGE) {
			add_string_gst_tag (metadata, "Image:Title", extractor->tagcache, GST_TAG_TITLE);
			add_string_gst_tag (metadata, "Image:Comments", extractor->tagcache, GST_TAG_COMMENT);
			add_string_gst_tag (metadata, "Image:Creator", extractor->tagcache, GST_TAG_ARTIST);
		} else if (extractor->mime == EXTRACT_MIME_VIDEO) {
			add_string_gst_tag (metadata, "Video:Title", extractor->tagcache, GST_TAG_TITLE);
			add_string_gst_tag (metadata, "Video:Comments", extractor->tagcache, GST_TAG_COMMENT);
			add_string_gst_tag (metadata, "Video:Author", extractor->tagcache, GST_TAG_ARTIST);
			add_string_gst_tag (metadata, "Video:Source", extractor->tagcache, GST_TAG_CLASSIFICATION);
		} else if (extractor->mime == EXTRACT_MIME_AUDIO) {
			add_string_gst_tag (metadata, "Audio:Title", extractor->tagcache, GST_TAG_TITLE);
			add_string_gst_tag (metadata, "Audio:Artist", extractor->tagcache, GST_TAG_ARTIST);
			add_string_gst_tag (metadata, "Audio:Comment", extractor->tagcache, GST_TAG_COMMENT);
		}
 	}

	if (extractor->audio_channels >= 0) {
		add_uint_info (metadata,
			       g_strdup ("Audio:Channels"),
			       extractor->audio_channels);
	}

	if (extractor->audio_samplerate >= 0) {
		add_uint_info (metadata,
			       g_strdup ("Audio:Samplerate"),
			       extractor->audio_samplerate);
	}

	if (extractor->video_height >= 0) {
		if (extractor->mime == EXTRACT_MIME_IMAGE) {
			add_uint_info (metadata,
				       g_strdup ("Image:Height"),
				       extractor->video_height);
		} else {
			add_uint_info (metadata,
				       g_strdup ("Video:Height"),
				       extractor->video_height);
		}
	}

	if (extractor->video_width >= 0) {
		if (extractor->mime == EXTRACT_MIME_IMAGE) {
			add_uint_info (metadata,
				       g_strdup ("Image:Width"),
				       extractor->video_width);
		} else {
			add_uint_info (metadata,
				       g_strdup ("Video:Width"),
				       extractor->video_width);
		}
	}

 	if (extractor->mime == EXTRACT_MIME_VIDEO) {
		if (extractor->video_fps_n >= 0 && extractor->video_fps_d >= 0) {
			add_uint_info (metadata,
				       g_strdup ("Video:FrameRate"),
				       ((extractor->video_fps_n + extractor->video_fps_d / 2) / 
					extractor->video_fps_d));
		}
 		if (extractor->duration >= 0) {
 			add_int64_info (metadata, g_strdup ("Video:Duration"), extractor->duration);
 		}
 	} else if (extractor->mime == EXTRACT_MIME_AUDIO) {
 		if (extractor->duration >= 0) {
 			add_int64_info (metadata, g_strdup ("Audio:Duration"), extractor->duration);
 		}
 	}

	if (extractor->audiotags) {
		add_uint_gst_tag (metadata, "Audio:Bitrate", extractor->audiotags, GST_TAG_BITRATE);
	}

	if (extractor->videotags) {
		add_uint_gst_tag (metadata, "Video:Bitrate", extractor->videotags, GST_TAG_BITRATE);
	}

	/* Do some postprocessing (FIXME, or fix gstreamer) */
	if ( (value = g_hash_table_lookup (metadata, "Audio:Genre")) ) {
		if (strcmp(value, "Unknown") == 0) {
			g_hash_table_remove (metadata,
					     "Audio:Genre");
		}
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
			   GHashTable  *metadata,
			   ExtractMime	type)
{
	MetadataExtractor *extractor;

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

	extract_metadata (extractor, metadata);

	/* Save embedded art */
	if (extractor->album_art_data && extractor->album_art_size) {
#ifdef HAVE_GDKPIXBUF
		tracker_process_albumart (extractor->album_art_data, extractor->album_art_size,
					  /* g_hash_table_lookup (metadata, "Audio:Artist") */ NULL,
					  g_hash_table_lookup (metadata, "Audio:Album"),
					  g_hash_table_lookup (metadata, "Audio:AlbumTrackCount"),
					  uri);
#else
		tracker_process_albumart (NULL, 0,
					  /* g_hash_table_lookup (metadata, "Audio:Artist") */ NULL,
					  g_hash_table_lookup (metadata, "Audio:Album"),
					  g_hash_table_lookup (metadata, "Audio:AlbumTrackCount"),
					  uri);
		
#endif /* HAVE_GDKPIXBUF */
	}

	gst_element_set_state (extractor->pipeline, GST_STATE_NULL);
	gst_element_get_state (extractor->pipeline, NULL, NULL, TRACKER_EXTRACT_GUARD_TIMEOUT* GST_SECOND);
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
		if (!g_hash_table_lookup (metadata, "Image:Date")) {
			struct stat st;
			
			if (g_lstat (uri, &st) >= 0) {
				gchar *date;
				
				date = tracker_date_to_string (st.st_mtime);
				
				g_hash_table_insert (metadata,
						     g_strdup ("Image:Date"),
						     tracker_escape_metadata (date));
				g_free (date);
			}
		}
	} else if (type == EXTRACT_MIME_VIDEO) {
		if (!g_hash_table_lookup (metadata, "Video:Title")) {
			gchar  *basename = g_filename_display_basename (uri);
			gchar **parts    = g_strsplit (basename, ".", -1);
			gchar  *title    = g_strdup (parts[0]);
			
			g_strfreev (parts);
			g_free (basename);

			title = g_strdelimit (title, "_", ' ');
			
			g_hash_table_insert (metadata,
					     g_strdup ("Video:Title"),
					     tracker_escape_metadata (title));
			g_free (title);
		}
	} else if (type == EXTRACT_MIME_AUDIO) {
		if (!g_hash_table_lookup (metadata, "Audio:Title")) {
			gchar  *basename = g_filename_display_basename (uri);
			gchar **parts    = g_strsplit (basename, ".", -1);
			gchar  *title    = g_strdup (parts[0]);
			
			g_strfreev (parts);
			g_free (basename);
			
			title = g_strdelimit (title, "_", ' ');
			
			g_hash_table_insert (metadata,
					     g_strdup ("Audio:Title"),
					     tracker_escape_metadata (title));
			g_free (title);
		}
	}

}


static void
extract_gstreamer_audio (const gchar *uri, GHashTable *metadata)
{
	tracker_extract_gstreamer (uri, metadata, EXTRACT_MIME_AUDIO);
}

static void
extract_gstreamer_video (const gchar *uri, GHashTable *metadata)
{
	tracker_extract_gstreamer (uri, metadata, EXTRACT_MIME_VIDEO);
}

static void
extract_gstreamer_image (const gchar *uri, GHashTable *metadata)
{
	tracker_extract_gstreamer (uri, metadata, EXTRACT_MIME_IMAGE);
}

TrackerExtractData *
tracker_get_extract_data (void)
{
	return data;
}

