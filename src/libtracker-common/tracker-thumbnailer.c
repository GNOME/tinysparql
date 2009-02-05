/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia

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
 */

#include "config.h"

#include <string.h>

#include <libtracker-data/tracker-data-metadata.h>

#include <tracker-indexer/tracker-module-file.h>

#include "tracker-config.h"
#include "tracker-dbus.h"
#include "tracker-thumbnailer.h"


#define THUMBNAILER_SERVICE	 "org.freedesktop.thumbnailer"
#define THUMBNAILER_PATH	 "/org/freedesktop/thumbnailer/Generic"
#define THUMBNAILER_INTERFACE	 "org.freedesktop.thumbnailer.Generic"

#define THUMBMAN_PATH		 "/org/freedesktop/thumbnailer/Manager"
#define THUMBMAN_INTERFACE	 "org.freedesktop.thumbnailer.Manager"

#define THUMBNAIL_REQUEST_LIMIT 50

typedef struct {
	TrackerConfig *config;

	DBusGProxy *requester_proxy;
	DBusGProxy *manager_proxy;

	GStrv supported_mime_types;

	gchar *uris[THUMBNAIL_REQUEST_LIMIT + 1];
	gchar *mime_types[THUMBNAIL_REQUEST_LIMIT + 1];

	guint request_id;
	guint timeout_id;
	guint count;
	guint timeout_seconds;

	gboolean service_is_available;
	gboolean service_is_enabled;
} TrackerThumbnailerPrivate;

static void thumbnailer_enabled_cb (GObject    *pspec,
				    GParamSpec *gobject,
				    gpointer    user_data);

static GStaticPrivate private_key = G_STATIC_PRIVATE_INIT;

static void
private_free (gpointer data)
{
	TrackerThumbnailerPrivate *private;
	guint i;

	private = data;

	if (private->config) {
		g_signal_handlers_disconnect_by_func (private->config, 
						      thumbnailer_enabled_cb, 
						      NULL); 

		g_object_unref (private->config);
	}
	
	if (private->requester_proxy) {
		g_object_unref (private->requester_proxy);
	}

	if (private->manager_proxy) {
		g_object_unref (private->manager_proxy);
	}

	g_strfreev (private->supported_mime_types);

	for (i = 0; i <= private->count; i++) {
		g_free (private->uris[i]);
		g_free (private->mime_types[i]);
	}

	if (private->timeout_id) {
		g_source_remove (private->timeout_id);
	}

	g_free (private);
}

static gboolean
should_be_thumbnailed (GStrv        list, 
		       const gchar *mime)
{
	gboolean should_thumbnail;
	guint i;

	if (!list) {
		return TRUE;
	}

	for (should_thumbnail = FALSE, i = 0; 
	     should_thumbnail == FALSE && list[i] != NULL; 
	     i++) {
		if (g_ascii_strcasecmp (list[i], mime) == 0) {
			should_thumbnail = TRUE;
		}
	}

	return should_thumbnail;
}

static void 
thumbnailer_enabled_cb (GObject    *pspec,
			GParamSpec *gobject,
			gpointer    user_data)
{
	TrackerThumbnailerPrivate *private;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	private->service_is_enabled = tracker_config_get_enable_thumbnails (private->config);

	g_debug ("Thumbnailer service %s", 
		   private->service_is_enabled ? "enabled" : "disabled");
}

static void
thumbnailer_reply_cb (DBusGProxy     *proxy,
		      DBusGProxyCall *call,
		      gpointer	      user_data)
{
	GError *error = NULL;
	guint	handle;

	/* The point of this is dbus-glib correctness. Answering this
	 * because this comment used to be the question: what is the
	 * point of this. It's correct this way because we do
	 * asynchronous DBus calls using glib-dbus. For asynchronous
	 * DBus calls it's recommended (if not required for cleaning
	 * up) to call dbus_g_proxy_end_call.
	 */
	dbus_g_proxy_end_call (proxy, call, &error,
			       G_TYPE_UINT, &handle,
			       G_TYPE_INVALID);

	if (error) {
		g_warning ("%s", error->message);
		g_error_free (error);
		return;
	}

	g_debug ("Received response from thumbnailer, request ID:%d",
	       GPOINTER_TO_UINT (user_data));
}

static gboolean
thumbnailer_request_timeout_cb (gpointer data)
{
	TrackerThumbnailerPrivate *private;
	guint i;

	private = g_static_private_get (&private_key);
	g_return_val_if_fail (private != NULL, FALSE);

	private->request_id++;

	private->uris[private->count] = NULL;
	private->mime_types[private->count] = NULL;
	
	g_debug ("Sending request to thumbnailer to queue %d files, request ID:%d...", 
		   private->count,
		   private->request_id);
	
	dbus_g_proxy_begin_call (private->requester_proxy,
				 "Queue",
				 thumbnailer_reply_cb,
				 GUINT_TO_POINTER (private->request_id), 
				 NULL,
				 G_TYPE_STRV, private->uris,
				 G_TYPE_STRV, private->mime_types,
				 G_TYPE_UINT, 0,
				 G_TYPE_INVALID);
	
	for (i = 0; i <= private->count; i++) {
		g_free (private->uris[i]);
		g_free (private->mime_types[i]);
		private->uris[i] = NULL;
		private->mime_types[i] = NULL;
	}
	
	private->count = 0;
	private->timeout_id = 0;

	return FALSE;
}

void
tracker_thumbnailer_init (TrackerConfig *config, guint timeout_seconds)
{
	TrackerThumbnailerPrivate *private;
	DBusGConnection *connection;
	GStrv mime_types = NULL;
	GError *error = NULL;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	private = g_new0 (TrackerThumbnailerPrivate, 1);

	private->config = g_object_ref (config);
	private->service_is_enabled = tracker_config_get_enable_thumbnails (private->config);
	private->timeout_seconds = timeout_seconds;

	g_signal_connect (private->config, "notify::enable-thumbnails",
			  G_CALLBACK (thumbnailer_enabled_cb), 
			  NULL);

	g_static_private_set (&private_key,
			      private,
			      private_free);

	g_debug ("Thumbnailer connections being set up...");

	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

	if (!connection) {
		g_critical ("Could not connect to the DBus session bus, %s",
			    error ? error->message : "no error given.");
		g_clear_error (&error);

		private->service_is_available = FALSE;

		return;
	}

	private->requester_proxy = 
		dbus_g_proxy_new_for_name (connection,
					   THUMBNAILER_SERVICE,
					   THUMBNAILER_PATH,
					   THUMBNAILER_INTERFACE);

	private->manager_proxy = 
		dbus_g_proxy_new_for_name (connection,
					   THUMBNAILER_SERVICE,
					   THUMBMAN_PATH,
					   THUMBMAN_INTERFACE);
	

	/* It's known that this relatively small GStrv is leaked: it contains
	 * the MIME types that the DBus thumbnailer supports. If a MIME type
	 * is not within this list, yet we retrieved it once, then we decide
	 * not to perform thumbnail actions over DBus. This is a performance
	 * improvement and the GStrv can be resident in memory until the end
	 * of the application - it's a cache - 
	 *
	 * It doesn't support detecting when the DBus thumbnailer starts 
	 * supporting more formats (which can indeed start happening). This is
	 * a known tradeoff and limitation of this cache. We could enhance this
	 * cache to listen for changes on the bus, and invalidate it once we
	 * know that more MIME types have become supported. It has no high
	 * priority now, though (therefore, is this a TODO). 
	 */

	g_debug ("Thumbnailer supported mime types requested...");

	dbus_g_proxy_call (private->manager_proxy,
			   "GetSupported", &error, 
			   G_TYPE_INVALID,
			   G_TYPE_STRV, &mime_types, 
			   G_TYPE_INVALID);
	
	if (error) {
		g_debug ("Thumbnailer service did not return supported mime types, %s",
			   error->message);

		g_error_free (error);
	} else if (mime_types) {
		g_debug ("Thumbnailer supports %d mime types", 
			   g_strv_length (mime_types));

		private->supported_mime_types = mime_types;
		private->service_is_available = TRUE;
	}	
}

void
tracker_thumbnailer_shutdown (void)
{
	g_static_private_set (&private_key, NULL, NULL);
}

void
tracker_thumbnailer_move (const gchar *from_uri,
			  const gchar *mime_type,
			  const gchar *to_uri)
{
	TrackerThumbnailerPrivate *private;
	gchar *to[2] = { NULL, NULL };
	gchar *from[2] = { NULL, NULL };

	g_return_if_fail (from_uri != NULL);
	g_return_if_fail (mime_type != NULL);
	g_return_if_fail (to_uri != NULL);

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	/* NOTE: We don't check the service_is_enabled flag here
	 * because we might want to manage thumbnails even if we are
	 * not creating any new ones. 
	 */
	if (!private->service_is_available) {
		return;
	}

	if (!should_be_thumbnailed (private->supported_mime_types, mime_type)) {
		g_debug ("Thumbnailer ignoring mime type:'%s'",
			 mime_type);
		return;
	}

	private->request_id++;

	g_debug ("Requesting thumbnailer moves URI from:'%s' to:'%s', request_id:%d...",
		   from_uri,
		   to_uri,
		   private->request_id); 

	if (!strstr (to_uri, ":/"))
		to[0] = g_filename_to_uri (to_uri, NULL, NULL);
	else
		to[0] = g_strdup (to_uri);

	if (!strstr (from_uri, ":/"))
		from[0] = g_filename_to_uri (from_uri, NULL, NULL);
	else
		from[0] = g_strdup (from_uri);

	
	dbus_g_proxy_begin_call (private->requester_proxy,
				 "Move",
				 thumbnailer_reply_cb,
				 GUINT_TO_POINTER (private->request_id), 
				 NULL,
				 G_TYPE_STRV, from,
				 G_TYPE_STRV, to,
				 G_TYPE_INVALID);

	g_free (from[0]);
	g_free (to[0]);

}

void
tracker_thumbnailer_remove (const gchar *uri, 
			    const gchar *mime_type)
{
	TrackerThumbnailerPrivate *private;
	gchar *uris[2] = { NULL, NULL };

	/* mime_type can be NULL */

	g_return_if_fail (uri != NULL);

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	/* NOTE: We don't check the service_is_enabled flag here
	 * because we might want to manage thumbnails even if we are
	 * not creating any new ones. 
	 */
	if (!private->service_is_available) {
		return;
	}

	if (mime_type && !should_be_thumbnailed (private->supported_mime_types, mime_type)) {
		g_debug ("Thumbnailer ignoring mime type:'%s' and uri:'%s'",
			 mime_type,
			 uri);
		return;
	}

	private->request_id++;

	if (!strstr (uri, ":/"))
		uris[0] = g_filename_to_uri (uri, NULL, NULL);
	else
		uris[0] = g_strdup (uri);

	g_debug ("Requesting thumbnailer removes URI:'%s', request_id:%d...",
		   uri,
		   private->request_id); 
	
	dbus_g_proxy_begin_call (private->requester_proxy,
				 "Delete",
				 thumbnailer_reply_cb,
				 GUINT_TO_POINTER (private->request_id),
				 NULL,
				 G_TYPE_STRV, uris,
				 G_TYPE_INVALID);

	g_free (uris[0]);
}

void 
tracker_thumbnailer_cleanup (const gchar *uri_prefix)
{
	TrackerThumbnailerPrivate *private;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	/* NOTE: We don't check the service_is_enabled flag here
	 * because we might want to manage thumbnails even if we are
	 * not creating any new ones. 
	 */
	if (!private->service_is_available) {
		return;
	}

	private->request_id++;

	g_debug ("Requesting thumbnailer cleanup URI:'%s', request_id:%d...",
		   uri_prefix,
		   private->request_id); 

	dbus_g_proxy_begin_call (private->requester_proxy,
				 "Cleanup",
				 thumbnailer_reply_cb,
				 GUINT_TO_POINTER (private->request_id),
				 NULL,
				 G_TYPE_STRING, uri_prefix,
				 G_TYPE_INT64, 0,
				 G_TYPE_INVALID);
}

void
tracker_thumbnailer_get_file_thumbnail (const gchar *uri,
					const gchar *mime_type)
{
	TrackerThumbnailerPrivate *private;

	g_return_if_fail (uri != NULL);
	g_return_if_fail (mime_type != NULL);

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	if (!private->service_is_available ||
	    !private->service_is_enabled) {
		return;
	}

	if (!should_be_thumbnailed (private->supported_mime_types, mime_type)) {
		g_debug ("Thumbnailer ignoring mime type:'%s' and uri:'%s'",
			 mime_type,
			 uri);
		return;
	}

	private->request_id++;

	g_debug ("Requesting thumbnailer to get thumbnail for URI:'%s', request_id:%d...",
		   uri,
		   private->request_id); 

	/* We want to deal with the current list first if it is
	 * already at the limit.
	 */
	if (private->count == THUMBNAIL_REQUEST_LIMIT) {
		g_debug ("Already have %d thumbnails queued, forcing thumbnailer request", 
			 THUMBNAIL_REQUEST_LIMIT);

		g_source_remove (private->timeout_id);
		private->timeout_id = 0;

		thumbnailer_request_timeout_cb (NULL);
	}

	/* Add new URI (detect if we got passed a path) */
	if (!strstr (uri, ":/"))
		private->uris[private->count] = g_filename_to_uri (uri, NULL, NULL);
	else
		private->uris[private->count] = g_strdup (uri);

	if (mime_type) {
		private->mime_types[private->count] = g_strdup (mime_type);
	} else if (g_strv_length (private->mime_types) > 0) {
		private->mime_types[private->count] = g_strdup ("unknown/unknown");
	}
	
	private->count++;
	
	if (private->timeout_seconds != 0) {
		if (private->timeout_id == 0) {
			private->timeout_id = 
				g_timeout_add_seconds (private->timeout_seconds, 
						       thumbnailer_request_timeout_cb, 
						       NULL);
		}
	} else {
		thumbnailer_request_timeout_cb (NULL);
	}
}
