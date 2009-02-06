/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
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

/* This extractor is still unfinished as required support from
 * tagreadbin for streaminfo elements is missing
 */

#include "config.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <glib.h>
#include <gst/gst.h>
#include <gst/tag/tag.h>
#include <gst/interfaces/tagreader.h>

#include "tracker-main.h"
#include "tracker-albumart.h"

typedef enum {
	EXTRACT_MIME_UNDEFINED,
	EXTRACT_MIME_AUDIO,
	EXTRACT_MIME_VIDEO,
	EXTRACT_MIME_IMAGE
} ExtractMime;

typedef struct {
	GMainLoop      *loop;

	GstElement     *bin;
	GstElement     *filesrc;
	GstElement     *cache;
	GstElement     *pipeline;

	GstBus         *bus;
	guint           id;

	GList          *fsinks;

	ExtractMime	mime;

	GstTagList     *tagcache;

} MetadataExtractor;

const guint use_dbin  = 0;
const guint use_cache = 0;

static void extract_gstreamer_audio (const gchar *uri, GHashTable *metadata);
static void extract_gstreamer_video (const gchar *uri, GHashTable *metadata);
static void extract_gstreamer_image (const gchar *uri, GHashTable *metadata);

static TrackerExtractorData data[] = {
	{ "audiotag/*", extract_gstreamer_audio },
	{ "videotag/*", extract_gstreamer_video },
	{ "imagetag/*", extract_gstreamer_image },
	{ NULL, NULL }
};

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
	g_return_val_if_fail (extractor->bin, -1);

	fmt = GST_FORMAT_TIME;

	duration = -1;
	
	if (gst_element_query_duration (extractor->bin,
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
	gint64 duration;

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
			add_string_gst_tag (metadata, "Image:Author", extractor->tagcache, GST_TAG_ARTIST);
		} else if (extractor->mime == EXTRACT_MIME_VIDEO) {
			add_string_gst_tag (metadata, "Video:Title", extractor->tagcache, GST_TAG_TITLE);
			add_string_gst_tag (metadata, "Video:Comments", extractor->tagcache, GST_TAG_COMMENT);
			add_string_gst_tag (metadata, "Video:Author", extractor->tagcache, GST_TAG_ARTIST);
			add_string_gst_tag (metadata, "File:Copyright", extractor->tagcache, GST_TAG_COPYRIGHT);
		} else if (extractor->mime == EXTRACT_MIME_AUDIO) {
			add_string_gst_tag (metadata, "Audio:Title", extractor->tagcache, GST_TAG_TITLE);
			add_string_gst_tag (metadata, "Audio:Artist", extractor->tagcache, GST_TAG_ARTIST);
			add_string_gst_tag (metadata, "Audio:Comment", extractor->tagcache, GST_TAG_COMMENT);
		}
 	}

/* 	duration = get_media_duration (extractor); */

/* 	if (extractor->mime == EXTRACT_MIME_VIDEO) { */
/* 		if (duration >= 0) { */
/* 			add_int64_info (metadata, g_strdup ("Video:Duration"), duration); */
/* 		}	 */
/* 	} else if (extractor->mime == EXTRACT_MIME_AUDIO) { */
/* 		if (duration >= 0) { */
/* 			add_int64_info (metadata, g_strdup ("Audio:Duration"), duration); */
/* 		} */
/* 	} */
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

void dbin_dpad_cb (GstElement* e, GstPad* pad, gboolean cont, gpointer data)
{
	MetadataExtractor *extractor = (MetadataExtractor *)data;
	GstElement        *fsink;
	GstPad            *fsinkpad;
	
	fsink = gst_element_factory_make ("fakesink", NULL);
	
	extractor->fsinks = g_list_append (extractor->fsinks, fsink);
	
	gst_element_set_state (fsink, GST_STATE_PAUSED);
	
	gst_bin_add (GST_BIN_CAST (extractor->pipeline), fsink);
	fsinkpad = gst_element_get_static_pad (fsink, "sink");
	gst_pad_link (pad, fsinkpad);
	gst_object_unref (fsinkpad);
}

static void
add_tags (GstMessage *msg, MetadataExtractor *extractor)
{
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
			if (use_dbin && sender == extractor->pipeline) {
				GstState newstate;
				gst_message_parse_state_changed (msg, NULL, &newstate, NULL);
				if (newstate == GST_STATE_PAUSED) {
					stop = TRUE;
				}
			}
		}
        default:
		break;
	}
	
	if (stop) {
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
	gchar		  *mrl;
	
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
		extractor->bin = gst_element_factory_make ("decodebin", "decodebin");
		if (!extractor->bin) {
			g_error ("Failed to create decodebin");
			return;
		}
		extractor->id = g_signal_connect (G_OBJECT (extractor->bin), 
				       "new-decoded-pad",
				       G_CALLBACK (dbin_dpad_cb), 
				       extractor->pipeline);
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
		if (! gst_element_link_many (extractor->filesrc, extractor->bin, NULL)) {
			g_error ("Can't link elements\n");
			/* FIXME Clean up */
			return;
		}
	}

	extractor->loop = g_main_loop_new (NULL, FALSE);
	extractor->bus = gst_pipeline_get_bus (GST_PIPELINE (extractor->pipeline));
	gst_bus_add_watch (extractor->bus, metadata_bus_async_cb, extractor);

	mrl = g_strconcat ("file://", uri, NULL);
	g_object_set (G_OBJECT (extractor->filesrc), "location", uri, NULL);

	gst_element_set_state (extractor->pipeline, GST_STATE_PAUSED);
	g_main_loop_run (extractor->loop);

	extract_metadata (extractor, metadata);

	gst_element_set_state (extractor->pipeline, GST_STATE_NULL);
	gst_object_unref (extractor->bus);
	gst_object_unref (GST_OBJECT (extractor->pipeline));
	g_slice_free (MetadataExtractor, extractor);
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

TrackerExtractorData *
tracker_get_extractor_data (void)
{
	return data;
}

