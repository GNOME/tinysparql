/* Tracker - audio/video metadata extraction based on GStreamer
 * Copyright (C) 2006, Laurent Aguerreche (laurent.aguerreche@free.fr)
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

/*
 * This file has been wrote starting from file bacon-video-widget-gst-0.10.c
 * from Totem:
 *
 * Copyright (C) 2003-2006 the GStreamer project
 *      Julien Moutte <julien@moutte.net>
 *      Ronald Bultje <rbultje@ronald.bitfreak.net>
 *      Tim-Philipp Müller <tim centricular net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * The Totem project hereby grant permission for non-gpl compatible GStreamer
 * plugins to be used and distributed together with GStreamer and Totem. This
 * permission is above and beyond the permissions granted by the GPL license
 * Totem is covered by.
 *
 * Monday 7th February 2005: Christian Schaller: Add exemption clause.
 * See license_change file for details.
 *
 */

#include <string.h>
#include <glib.h>
#include <gst/gst.h>


typedef struct {
	GstElement	*playbin;

	GstTagList	*tagcache;
	GstTagList	*audiotags;
	GstTagList	*videotags;

	GstMessageType	ignore_messages_mask;

	gboolean	has_audio;
	gboolean	has_video;

	gint		video_height;
	gint		video_width;
	gint		video_fps_n;
	gint		video_fps_d;
	gint		audio_channels;
	gint		audio_samplerate;
} MetadataExtractor;


static void
caps_set (GObject *obj, MetadataExtractor *extractor, const char *type)
{
	GstPad		*pad;
	GstStructure	*s;
	GstCaps		*caps;

	pad = GST_PAD (obj);

	if (!(caps = gst_pad_get_negotiated_caps (pad))) {
		return;
	}

	s = gst_caps_get_structure (caps, 0);

	if (s) {
		if (!strcmp (type, "audio")) {
			if ((extractor->audio_channels != -1 && extractor->audio_samplerate != -1) ||
			    !(gst_structure_get_int (s, "channels", &extractor->audio_channels) &&
			      (gst_structure_get_int (s, "rate", &extractor->audio_samplerate)))) {

				return;
			}
		} else if (!strcmp (type, "video")) {
			if ((extractor->video_fps_n != -1 && extractor->video_fps_d != -1 && extractor->video_width != -1 && extractor->video_height != -1) ||
			    !(gst_structure_get_fraction (s, "framerate", &extractor->video_fps_n, &extractor->video_fps_d) &&
			      gst_structure_get_int (s, "width", &extractor->video_width) &&
			      gst_structure_get_int (s, "height", &extractor->video_height))) {

				return;
			}
		} else {
			g_assert_not_reached ();
		}
	}

	gst_caps_unref (caps);
}


static void
caps_set_audio (GObject *obj, MetadataExtractor *extractor)
{
	g_return_if_fail (obj && extractor);

	caps_set (obj, extractor, "audio");
}


static void
caps_set_video (GObject *obj, MetadataExtractor *extractor)
{
	g_return_if_fail (obj && extractor);

	caps_set (obj, extractor, "video");
}


static void
update_stream_info (MetadataExtractor *extractor)
{
	GList	*streaminfo;
	GstPad	*audiopad, *videopad;

	g_return_if_fail (extractor);


	streaminfo = NULL;
	audiopad = videopad = NULL;

	g_object_get (extractor->playbin, "stream-info", &streaminfo, NULL);
	streaminfo = g_list_copy (streaminfo);
	g_list_foreach (streaminfo, (GFunc) g_object_ref, NULL);

	for ( ; streaminfo; streaminfo = streaminfo->next) {
		GObject		*info;
		gint		type;
		GParamSpec	*pspec;
		GEnumValue	*val;

		info = streaminfo->data;

		if (!info) {
			continue;
		}

		g_object_get (info, "type", &type, NULL);
		pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (info), "type");
		val = g_enum_get_value (G_PARAM_SPEC_ENUM (pspec)->enum_class, type);

		if (!strcmp (val->value_nick, "audio")) {
			extractor->has_audio = TRUE;
			if (!audiopad) {
				g_object_get (info, "object", &audiopad, NULL);
			}
		} else if (!strcmp (val->value_nick, "video")) {
			extractor->has_video = TRUE;
			if (!videopad) {
				g_object_get (info, "object", &videopad, NULL);
			}
		}
	}

	if (audiopad) {
		GstCaps *caps;

		if ((caps = gst_pad_get_negotiated_caps (audiopad))) {
			caps_set_audio (G_OBJECT (audiopad), extractor);
			gst_caps_unref (caps);
		}
	}

	if (videopad) {
		GstCaps *caps;

		if ((caps = gst_pad_get_negotiated_caps (videopad))) {
			caps_set_video (G_OBJECT (videopad), extractor);
			gst_caps_unref (caps);
		}
	}

	g_list_foreach (streaminfo, (GFunc) g_object_unref, NULL);
	g_list_free (streaminfo);
}


static void
gst_bus_cb (GstBus *bus, GstMessage *message, MetadataExtractor *extractor)
{
	GstMessageType msg_type;

	g_return_if_fail (bus && message && extractor);


	msg_type = GST_MESSAGE_TYPE (message);

	/* somebody else is handling the message, probably in poll_for_state_change */
	if (extractor->ignore_messages_mask & msg_type) {
		gchar *src_name;

		src_name = gst_object_get_name (message->src);
		GST_LOG ("Ignoring %s message from element %s as requested",
			 gst_message_type_get_name (msg_type), src_name);
		g_free (src_name);

		return;
	}

	switch (msg_type) {
	case GST_MESSAGE_ERROR: {
		GstMessage	*message;
		GError		*gsterror;
		gchar		*debug;

		gst_message_parse_error (message, &gsterror, &debug);
		g_warning ("Error: %s (%s)", gsterror->message, debug);

		gst_message_unref (message);
		g_error_free (gsterror);
		g_free (debug);
	}
		break;

	case GST_MESSAGE_STATE_CHANGED: {
		GstState old_state, new_state;

		gst_message_parse_state_changed (message, &old_state, &new_state, NULL);

		if (old_state == new_state) {
			break;
		}

		/* we only care about playbin (pipeline) state changes */
		if (GST_MESSAGE_SRC (message) != GST_OBJECT (extractor->playbin)) {
			break;
		}

		if (old_state == GST_STATE_READY && new_state == GST_STATE_PAUSED) {
			update_stream_info (extractor);

		} else if (old_state == GST_STATE_PAUSED && new_state == GST_STATE_READY) {
			/* clean metadata cache */

			if (extractor->tagcache) {
				gst_tag_list_free (extractor->tagcache);
				extractor->tagcache = NULL;
			}

			if (extractor->audiotags) {
				gst_tag_list_free (extractor->audiotags);
				extractor->audiotags = NULL;
			}

			if (extractor->videotags) {
				gst_tag_list_free (extractor->videotags);
				extractor->videotags = NULL;
			}

			extractor->has_audio = extractor->has_video = FALSE;

			extractor->video_fps_n = extractor->video_fps_d = -1;
			extractor->video_height = extractor->video_width = -1;
			extractor->audio_channels = -1;
			extractor->audio_samplerate = -1;
		}

		break;
	}

	case GST_MESSAGE_TAG: {
		GstTagList	  *tag_list, *result;
		GstElementFactory *f;

		gst_message_parse_tag (message, &tag_list);

		GST_DEBUG ("Tags: %" GST_PTR_FORMAT, tag_list);

		/* all tags */
		result = gst_tag_list_merge (extractor->tagcache, tag_list, GST_TAG_MERGE_KEEP);

		if (extractor->tagcache) {
			gst_tag_list_free (extractor->tagcache);
		}

		extractor->tagcache = result;

		/* media-type-specific tags */
		if (GST_IS_ELEMENT (message->src) && (f = gst_element_get_factory (GST_ELEMENT (message->src)))) {
			const gchar *klass;
			GstTagList  **cache;

			klass = gst_element_factory_get_klass (f);

			cache = NULL;

			if (g_strrstr (klass, "Audio")) {
				cache = &extractor->audiotags;
			} else if (g_strrstr (klass, "Video")) {
				cache = &extractor->videotags;
			}

			if (cache) {
				result = gst_tag_list_merge (*cache, tag_list, GST_TAG_MERGE_KEEP);
				if (*cache) {
					gst_tag_list_free (*cache);
				}
				*cache = result;
			}
		}

		/* clean up */
		gst_tag_list_free (tag_list);

		break;
	}

	default:
		break;
	}
}


static void
add_int64_info (GHashTable *metadata, char *key, gint64 info)
{
	char *str_info;

	str_info = g_strdup_printf ("%" G_GINT64_FORMAT, info);
	g_hash_table_insert (metadata, key, str_info);
}


static void
add_uint_info (GHashTable *metadata, char *key, guint info)
{
	char *str_info;

	str_info = g_strdup_printf ("%d", info);
	g_hash_table_insert (metadata, key, str_info);
}


static void
add_string_gst_tag (GHashTable *metadata, const char *key, GstTagList *tag_list, const gchar *tag)
{
	gboolean ret;
	char	 *s;

	s = NULL;

	ret = gst_tag_list_get_string (tag_list, tag, &s);

	if (s) {
		if (ret && s[0] != '\0') {
			g_hash_table_insert (metadata, g_strdup (key), s);
		} else {
			g_free (s);
		}
	}
}


static void
add_uint_gst_tag (GHashTable *metadata, const char *key, GstTagList *tag_list, const gchar *tag)
{
	gboolean ret;
	guint	 n;

	ret = gst_tag_list_get_uint (tag_list, tag, &n);

	if (ret) {
		g_hash_table_insert (metadata, g_strdup (key), g_strdup_printf ("%d", n));
	}
}


static void
add_double_gst_tag (GHashTable *metadata, const char *key, GstTagList *tag_list, const gchar *tag)
{
	gboolean ret;
	double	 n;

	ret = gst_tag_list_get_double (tag_list, tag, &n);

	if (ret) {
		g_hash_table_insert (metadata, g_strdup (key), g_strdup_printf ("%f", n));
	}
}


static void
add_year_of_gdate_gst_tag (GHashTable *metadata, const char *key, GstTagList *tag_list, const gchar *tag)
{
	gboolean ret;
	GDate	 *date;

	date = NULL;

	ret = gst_tag_list_get_date (tag_list, tag, &date);

	if (ret) {
		char buf[10];

		if (g_date_strftime (buf, 10, "%Y", date)) {
			g_hash_table_insert (metadata, g_strdup (key), g_strdup (buf));
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

	g_return_val_if_fail (extractor && extractor->playbin, -1);

	fmt = GST_FORMAT_TIME;

	duration = -1;

	if (gst_element_query_duration (extractor->playbin, &fmt, &duration) && duration >= 0) {
		return duration / GST_SECOND;
	} else {
		return -1;
	}
}


static void
extract_metadata (MetadataExtractor *extractor, GHashTable *metadata)
{
	g_return_if_fail (extractor && metadata);


	if (extractor->audio_channels >= 0) {
		add_uint_info (metadata, g_strdup ("Audio:Channels"), (guint) extractor->audio_channels);
	}

	if (extractor->audio_samplerate >= 0) {
		add_uint_info (metadata, g_strdup ("Audio:Samplerate"), (guint) extractor->audio_samplerate);
	}

	if (extractor->video_height >= 0) {
		add_uint_info (metadata, g_strdup ("Video:Height"), (guint) extractor->video_height);
	}

	if (extractor->video_width >= 0) {
		add_uint_info (metadata, g_strdup ("Video:Width"), (guint) extractor->video_width);
	}

	if (extractor->video_fps_n >= 0 && extractor->video_fps_d >= 0) {
		add_uint_info (metadata, g_strdup ("Video:FrameRate"),
			       (guint) ((extractor->video_fps_n + extractor->video_fps_d / 2) / extractor->video_fps_d));
	}

	if (extractor->tagcache) {
		gint64 duration;

		/* audio */
		add_string_gst_tag (metadata, "Audio:Album", extractor->tagcache, GST_TAG_ALBUM);
		add_uint_gst_tag (metadata, "Audio:AlbumTrackCount", extractor->tagcache, GST_TAG_TRACK_COUNT);
		add_uint_gst_tag (metadata, "Audio:TrackNo", extractor->tagcache, GST_TAG_TRACK_NUMBER);
		add_uint_gst_tag (metadata, "Audio:DiscNo", extractor->tagcache, GST_TAG_ALBUM_VOLUME_NUMBER);
		add_string_gst_tag (metadata, "Audio:Performer", extractor->tagcache, GST_TAG_PERFORMER);
		add_double_gst_tag (metadata, "Audio:TrackGain", extractor->tagcache, GST_TAG_TRACK_GAIN);
		add_double_gst_tag (metadata, "Audio:TrackPeakGain", extractor->tagcache, GST_TAG_TRACK_PEAK);
		add_double_gst_tag (metadata, "Audio:AlbumGain", extractor->tagcache, GST_TAG_ALBUM_GAIN);
		add_double_gst_tag (metadata, "Audio:AlbumPeakGain", extractor->tagcache, GST_TAG_ALBUM_PEAK);
		add_year_of_gdate_gst_tag (metadata, "Audio:ReleaseDate", extractor->tagcache, GST_TAG_DATE);
		add_string_gst_tag (metadata, "Audio:Genre", extractor->tagcache, GST_TAG_GENRE);
		add_string_gst_tag (metadata, "Audio:Codec", extractor->tagcache, GST_TAG_AUDIO_CODEC);

		/* video */
		add_string_gst_tag (metadata, "Video:Codec", extractor->tagcache, GST_TAG_VIDEO_CODEC);

		/* general */
		add_string_gst_tag (metadata, "File:Copyright", extractor->tagcache, GST_TAG_COPYRIGHT);
		add_string_gst_tag (metadata, "File:License", extractor->tagcache, GST_TAG_LICENSE);
		add_string_gst_tag (metadata, "DC:Coverage", extractor->tagcache, GST_TAG_LOCATION);

		duration = get_media_duration (extractor);


		/* Sometimes it is unclear to what a tag belongs to! */
		if (extractor->has_video) {
			add_string_gst_tag (metadata, "Video:Title", extractor->tagcache, GST_TAG_TITLE);
			add_string_gst_tag (metadata, "Video:Comments", extractor->tagcache, GST_TAG_COMMENT);
			/* FIXME: is it a good idea to use GST_TAG_ARTIST as author?! */
			add_string_gst_tag (metadata, "Video:Author", extractor->tagcache, GST_TAG_ARTIST);
			add_string_gst_tag (metadata, "File:Copyright", extractor->tagcache, GST_TAG_COPYRIGHT);

			if (duration >= 0) {
				add_int64_info (metadata, g_strdup ("Video:Duration"), duration);
			}

		} else if (extractor->has_audio) {
			/* No video? So we assume we are treating a song */
			add_string_gst_tag (metadata, "Audio:Title", extractor->tagcache, GST_TAG_TITLE);
			add_string_gst_tag (metadata, "Audio:Artist", extractor->tagcache, GST_TAG_ARTIST);
			add_string_gst_tag (metadata, "Audio:Comment", extractor->tagcache, GST_TAG_COMMENT);

			if (duration >= 0) {
				add_int64_info (metadata, g_strdup ("Audio:Duration"), duration);
			}
		}
	}

	if (extractor->audiotags) {
		add_uint_gst_tag (metadata, "Audio:Bitrate", extractor->tagcache, GST_TAG_BITRATE);
	}

	if (extractor->videotags) {
		add_uint_gst_tag (metadata, "Video:Bitrate", extractor->tagcache, GST_TAG_BITRATE);
	}
}


static gboolean
poll_for_state_change (MetadataExtractor *extractor, GstState state)
{
	GstBus *bus;
	GstMessageType events, saved_events;

	g_return_val_if_fail (extractor && extractor->playbin, FALSE);


	bus = gst_element_get_bus (extractor->playbin);

	events = (GST_MESSAGE_STATE_CHANGED | GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

	saved_events = extractor->ignore_messages_mask;

	if (extractor->playbin) {
		/* we do want the main handler to process state changed messages for
		 * playbin as well, otherwise it won't hook up the timeout etc. */
		extractor->ignore_messages_mask |= (events ^ GST_MESSAGE_STATE_CHANGED);
	} else {
		extractor->ignore_messages_mask |= events;
	}


	for (;;) {
		GstMessage *message;
		GstElement *src;

		message = gst_bus_poll (bus, events, GST_SECOND * 5);

		if (!message) {
			goto timed_out;
		}

		src = (GstElement *) GST_MESSAGE_SRC (message);

		switch (GST_MESSAGE_TYPE (message)) {
		case GST_MESSAGE_STATE_CHANGED: {
			GstState old, new, pending;

			if (src == extractor->playbin) {
				gst_message_parse_state_changed (message, &old, &new, &pending);

				if (new == state) {
					gst_message_unref (message);
					goto success;
				}
			}
		}
			break;

		case GST_MESSAGE_ERROR: {
			gchar  *debug;
			GError *gsterror;

			gst_message_parse_error (message, &gsterror, &debug);

			g_warning ("Error: %s (%s)", gsterror->message, debug);

			g_error_free (gsterror);
			gst_message_unref (message);
			g_free (debug);
			goto error;
		}
			break;

		case GST_MESSAGE_EOS: {
			g_warning ("Media file could not be played.");
			gst_message_unref (message);
			goto error;
		}
			break;

		default:
			g_assert_not_reached ();
			break;
		}

		gst_message_unref (message);
	}

	g_assert_not_reached ();

 success:
	/* state change succeeded */
	GST_DEBUG ("state change to %s succeeded", gst_element_state_get_name (state));
	extractor->ignore_messages_mask = saved_events;
	return TRUE;

 timed_out:
	/* it's taking a long time to open  */
	GST_DEBUG ("state change to %s timed out, returning success", gst_element_state_get_name (state));
	extractor->ignore_messages_mask = saved_events;
	return TRUE;

 error:
	GST_DEBUG ("error while waiting for state change to %s", gst_element_state_get_name (state));
	/* already set *error */
	extractor->ignore_messages_mask = saved_events;
	return FALSE;
}


void
tracker_extract_gstreamer (gchar *uri, GHashTable *metadata)
{
	MetadataExtractor *extractor;
	gchar		  *mrl;
	GstElement	  *fakesink_audio, *fakesink_video;
	GstBus		  *bus;

	g_return_if_fail (uri && metadata);


	g_type_init ();

	gst_init (NULL, NULL);

	/* set up */
	extractor = g_new0 (MetadataExtractor, 1);

	extractor->tagcache = NULL;
	extractor->audiotags = extractor->videotags = NULL;

	extractor->has_audio = extractor->has_video = FALSE;

	extractor->video_fps_n = extractor->video_fps_d = -1;
	extractor->video_height = extractor->video_width = -1;
	extractor->audio_channels = -1;
	extractor->audio_samplerate = -1;

	extractor->ignore_messages_mask = 0;

	extractor->playbin = gst_element_factory_make ("playbin", "playbin");


	/* add bus callback */
	bus = gst_element_get_bus (GST_ELEMENT (extractor->playbin));
	gst_bus_add_signal_watch (bus);
	g_signal_connect (bus, "message", G_CALLBACK (gst_bus_cb), extractor);
	gst_object_unref (bus);


	mrl = g_strconcat ("file://", uri, NULL);

	/* set playbin object */
	g_object_set (G_OBJECT (extractor->playbin), "uri", mrl, NULL);
	g_free (mrl);

	fakesink_audio = gst_element_factory_make ("fakesink", "fakesink-audio");
	g_object_set (G_OBJECT (extractor->playbin), "audio-sink", fakesink_audio, NULL);

	fakesink_video = gst_element_factory_make ("fakesink", "fakesink-video");
	g_object_set (G_OBJECT (extractor->playbin), "video-sink", fakesink_video, NULL);


	/* start to parse infos and extract them */
	gst_element_set_state (extractor->playbin, GST_STATE_PAUSED);

	poll_for_state_change (extractor, GST_STATE_PAUSED);

	extract_metadata (extractor, metadata);


	/* also clean up */
	gst_element_set_state (extractor->playbin, GST_STATE_NULL);

	gst_object_unref (GST_OBJECT (extractor->playbin));

	g_free (extractor);
}
