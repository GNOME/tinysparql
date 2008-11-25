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

#include "tracker-dbus.h"

/* Undef this to disable thumbnailing (don't remove unless you
 * understand that that either you need to put in place another method
 * to disable/enable this and that what will be used by the packager
 * is probably *your* default, or unless you want to break expected
 * functionality on-purpose) (It's not the first time that these
 * ifdef-else-endifs where rewrapped incorrectly, and that way
 * obviously broke the feature)
 */

#ifndef THUMBNAILING_OVER_DBUS
#define THUMBNAILING_OVER_DBUS
#endif 

#ifdef THUMBNAILING_OVER_DBUS

#define THUMBNAILER_SERVICE	 "org.freedesktop.thumbnailer"
#define THUMBNAILER_PATH	 "/org/freedesktop/thumbnailer/Generic"
#define THUMBNAILER_INTERFACE	 "org.freedesktop.thumbnailer.Generic"

#define THUMBMAN_PATH		 "/org/freedesktop/thumbnailer/Manager"
#define THUMBMAN_INTERFACE	 "org.freedesktop.thumbnailer.Manager"

#define THUMBNAIL_REQUEST_LIMIT 50



static DBusGProxy*
get_thumb_requester (void)
{
	static DBusGProxy *thumb_proxy = NULL;

	if (!thumb_proxy) {
		GError          *error = NULL;
		DBusGConnection *connection;

		connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

		if (!error) {
			thumb_proxy = dbus_g_proxy_new_for_name (connection,
								 THUMBNAILER_SERVICE,
								 THUMBNAILER_PATH,
								 THUMBNAILER_INTERFACE);
		} else {
			g_error_free (error);
		}
	}

	return thumb_proxy;
}


static DBusGProxy*
get_thumb_manager (void)
{
	static DBusGProxy *thumbm_proxy = NULL;

	if (!thumbm_proxy) {
		GError          *error = NULL;
		DBusGConnection *connection;

		connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

		if (!error) {
			thumbm_proxy = dbus_g_proxy_new_for_name (connection,
								 THUMBNAILER_SERVICE,
								 THUMBMAN_PATH,
								 THUMBMAN_INTERFACE);
		} else {
			g_error_free (error);
		}
	}

	return thumbm_proxy;
}

typedef struct {
	GStrv     supported_mime_types;

	gchar    *uris[THUMBNAIL_REQUEST_LIMIT + 1];
	gchar    *mime_types[THUMBNAIL_REQUEST_LIMIT + 1];

	guint     request_id;
	guint     count;
	guint     timeout_id;

	gboolean  service_is_prepared;
} TrackerThumbnailerPrivate;

static GStaticPrivate private_key = G_STATIC_PRIVATE_INIT;

static void
private_free (gpointer data)
{
	TrackerThumbnailerPrivate *private;
	gint i;

	private = data;
	
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

	g_message ("Received response from thumbnailer, request ID:%d",
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
	
	g_message ("Sending request to thumbnailer to queue %d files, request ID:%d...", 
		   private->count,
		   private->request_id);
	
	dbus_g_proxy_begin_call (get_thumb_requester (),
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

static void
thumbnailer_prepare (void)
{
	TrackerThumbnailerPrivate *private;
	GStrv mime_types = NULL;
	GError *error = NULL;

	private = g_static_private_get (&private_key);
	
	if (private->service_is_prepared) {
		return;
	}

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

	g_message ("Thumbnailer supported mime types being requested...");

	dbus_g_proxy_call (get_thumb_manager (),
			   "GetSupported", &error, 
			   G_TYPE_INVALID,
			   G_TYPE_STRV, &mime_types, 
			   G_TYPE_INVALID);
	
	if (error) {
		g_warning ("Thumbnailer service did not return supported mime types, %s",
			   error->message);
		g_error_free (error);
	} else if (mime_types) {
		g_message ("Thumbnailer supports %d mime types", 
			   g_strv_length (mime_types));
		private->supported_mime_types = mime_types;
	}
	
	private->service_is_prepared = TRUE;
}

#endif /* THUMBNAILING_OVER_DBUS */

void
tracker_thumbnailer_init (void)
{
	TrackerThumbnailerPrivate *private;

	private = g_new0 (TrackerThumbnailerPrivate, 1);
	g_static_private_set (&private_key,
			      private,
			      private_free);

	thumbnailer_prepare ();
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
#ifdef THUMBNAILING_OVER_DBUS
	TrackerThumbnailerPrivate *private;
	const gchar *to[2] = { NULL, NULL };
	const gchar *from[2] = { NULL, NULL };

	g_return_if_fail (from_uri != NULL);
	g_return_if_fail (mime_type != NULL);
	g_return_if_fail (to_uri != NULL);

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	if (!should_be_thumbnailed (private->supported_mime_types, mime_type)) {
		g_debug ("Thumbnailer ignoring mime type:'%s'",
			 mime_type);
		return;
	}

	private->request_id++;

	g_message ("Requesting thumbnailer moves URI from:'%s' to:'%s', request_id:%d...",
		   from_uri,
		   to_uri,
		   private->request_id); 

	to[0] = to_uri;
	from[0] = from_uri;
	
	dbus_g_proxy_begin_call (get_thumb_requester (),
				 "Move",
				 thumbnailer_reply_cb,
				 GUINT_TO_POINTER (private->request_id), 
				 NULL,
				 G_TYPE_STRV, from,
				 G_TYPE_STRV, to,
				 G_TYPE_INVALID);
#endif /* THUMBNAILING_OVER_DBUS */
}

void
tracker_thumbnailer_remove (const gchar *uri, 
			    const gchar *mime_type)
{
#ifdef THUMBNAILING_OVER_DBUS
	TrackerThumbnailerPrivate *private;
	const gchar *uris[2] = { NULL, NULL };

	g_return_if_fail (uri != NULL);
	g_return_if_fail (mime_type != NULL);

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	if (!should_be_thumbnailed (private->supported_mime_types, mime_type)) {
		g_debug ("Thumbnailer ignoring mime type:'%s' and uri:'%s'",
			 mime_type,
			 uri);
		return;
	}

	private->request_id++;

	uris[0] = uri;

	g_message ("Requesting thumbnailer removes URI:'%s', request_id:%d...",
		   uri,
		   private->request_id); 
	
	dbus_g_proxy_begin_call (get_thumb_requester (),
				 "Delete",
				 thumbnailer_reply_cb,
				 GUINT_TO_POINTER (private->request_id),
				 NULL,
				 G_TYPE_STRV, uri,
				 G_TYPE_INVALID);
#endif /* THUMBNAILING_OVER_DBUS */
}

void 
tracker_thumbnailer_cleanup (const gchar *uri_prefix)
{
#ifdef THUMBNAILING_OVER_DBUS
	TrackerThumbnailerPrivate *private;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	private->request_id++;

	g_message ("Requesting thumbnailer cleanup URI:'%s', request_id:%d...",
		   uri_prefix,
		   private->request_id); 

	dbus_g_proxy_begin_call (get_thumb_requester (),
				 "Cleanup",
				 thumbnailer_reply_cb,
				 GUINT_TO_POINTER (private->request_id),
				 NULL,
				 G_TYPE_STRING, uri_prefix,
				 G_TYPE_INT64, 0,
				 G_TYPE_INVALID);

#endif /* THUMBNAILING_OVER_DBUS */
}


void
tracker_thumbnailer_get_file_thumbnail (const gchar *uri,
					const gchar *mime_type)
{
#ifdef THUMBNAILING_OVER_DBUS
	TrackerThumbnailerPrivate *private;

	g_return_if_fail (uri != NULL);
	g_return_if_fail (mime_type != NULL);

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	if (!should_be_thumbnailed (private->supported_mime_types, mime_type)) {
		g_debug ("Thumbnailer ignoring mime type:'%s' and uri:'%s'",
			 mime_type,
			 uri);
		return;
	}

	private->request_id++;

	g_message ("Requesting thumbnailer to get thumbnail for URI:'%s', request_id:%d...",
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

	/* Add new URI */
	private->uris[private->count] = g_strdup (uri);

	if (mime_type) {
		private->mime_types[private->count] = g_strdup (mime_type);
	} else if (g_strv_length (private->mime_types) > 0) {
		private->mime_types[private->count] = g_strdup ("unknown/unknown");
	}
	
	private->count++;
	
	if (private->timeout_id == 0) {
		private->timeout_id = 
			g_timeout_add_seconds (30, 
					       thumbnailer_request_timeout_cb, 
					       NULL);
	}
#endif /* THUMBNAILING_OVER_DBUS */
}
