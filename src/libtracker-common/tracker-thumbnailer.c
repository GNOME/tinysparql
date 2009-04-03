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

	GSList *uris;
	GSList *mime_types;

	guint request_id;

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

	g_slist_foreach (private->uris, (GFunc) g_free, NULL);
	g_slist_free (private->uris);

	g_slist_foreach (private->mime_types, (GFunc) g_free, NULL);
	g_slist_free (private->mime_types);
	
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

	g_message ("Thumbnailer service %s", 
		   private->service_is_enabled ? "enabled" : "disabled");
}

void
tracker_thumbnailer_init (TrackerConfig *config)
{
	TrackerThumbnailerPrivate *private;
	DBusGConnection *connection;
	GStrv mime_types = NULL;
	GError *error = NULL;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	private = g_new0 (TrackerThumbnailerPrivate, 1);

	private->config = g_object_ref (config);
	private->service_is_enabled = tracker_config_get_enable_thumbnails (private->config);

	g_signal_connect (private->config, "notify::enable-thumbnails",
			  G_CALLBACK (thumbnailer_enabled_cb), 
			  NULL);

	g_static_private_set (&private_key,
			      private,
			      private_free);

	g_message ("Thumbnailer connections being set up...");

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

	dbus_g_proxy_call (private->manager_proxy,
			   "GetSupported", &error, 
			   G_TYPE_INVALID,
			   G_TYPE_STRV, &mime_types, 
			   G_TYPE_INVALID);
	
	if (error) {
		g_message ("Thumbnailer service did not return supported mime types, %s",
			   error->message);

		g_error_free (error);
	} else if (mime_types) {
		g_message ("Thumbnailer supports %d mime types", 
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

	g_message ("Thumbnailer request to move uri from:'%s' to:'%s', request_id:%d...",
		   from_uri,
		   to_uri,
		   private->request_id); 

	if (!strstr (to_uri, ":/")) {
		to[0] = g_filename_to_uri (to_uri, NULL, NULL);
	} else {
		to[0] = g_strdup (to_uri);
	}

	if (!strstr (from_uri, ":/")) {
		from[0] = g_filename_to_uri (from_uri, NULL, NULL);
	} else {
		from[0] = g_strdup (from_uri);
	}

	dbus_g_proxy_call_no_reply (private->requester_proxy,
				    "Move",
				    G_TYPE_STRV, from,
				    G_TYPE_STRV, to,
				    G_TYPE_INVALID,
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
		g_debug ("Thumbnailer ignoring uri:'%s', mime type:'%s'",
			 uri,
			 mime_type);
		return;
	}

	private->request_id++;

	if (!strstr (uri, ":/")) {
		uris[0] = g_filename_to_uri (uri, NULL, NULL);
	} else {
		uris[0] = g_strdup (uri);
	}

	g_message ("Thumbnailer request to remove uri:'%s', request_id:%d...",
		   uri,
		   private->request_id); 
	
	dbus_g_proxy_call_no_reply (private->requester_proxy,
				    "Delete",
				    G_TYPE_STRV, uris,
				    G_TYPE_INVALID,
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

	g_message ("Thumbnailer cleaning up uri:'%s', request_id:%d...",
		   uri_prefix,
		   private->request_id); 

	dbus_g_proxy_call_no_reply (private->requester_proxy,
				    "Cleanup",
				    G_TYPE_STRING, uri_prefix,
				    G_TYPE_UINT, 0,
				    G_TYPE_INVALID,
				    G_TYPE_INVALID);
}

void
tracker_thumbnailer_queue_file (const gchar *uri,
				const gchar *mime_type)
{
	TrackerThumbnailerPrivate *private;
	gchar *used_uri;
	gchar *used_mime_type;

	g_return_if_fail (uri != NULL);
	g_return_if_fail (mime_type != NULL);

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	if (!private->service_is_available ||
	    !private->service_is_enabled) {
		return;
	}

	if (!should_be_thumbnailed (private->supported_mime_types, mime_type)) {
		g_debug ("Thumbnailer ignoring uri:'%s', mime type:'%s'",
			 uri,
			 mime_type);
		return;
	}

	private->request_id++;

	/* Add new URI (detect if we got passed a path) */
	if (!strstr (uri, ":/")) {
		used_uri = g_filename_to_uri (uri, NULL, NULL);
	} else {
		used_uri = g_strdup (uri);
	}

	if (mime_type) {
		used_mime_type = g_strdup (mime_type);
	} else {
		used_mime_type = g_strdup ("unknown/unknown");
	}

	private->uris = g_slist_append (private->uris, used_uri);
	private->mime_types = g_slist_append (private->mime_types, used_mime_type);

	g_message ("Thumbnailer queue appended with uri:'%s', mime type:'%s', request_id:%d...",
		   used_uri,
		   used_mime_type,
		   private->request_id); 
}

void
tracker_thumbnailer_queue_send (void)
{
	TrackerThumbnailerPrivate *private;
	GStrv uri_strv;
	GStrv mime_type_strv;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	uri_strv = tracker_dbus_slist_to_strv (private->uris);
	mime_type_strv = tracker_dbus_slist_to_strv (private->mime_types);

	dbus_g_proxy_call_no_reply (private->requester_proxy,
				    "Queue",
				    G_TYPE_STRV, uri_strv,
				    G_TYPE_STRV, mime_type_strv,
				    G_TYPE_UINT, 0,
				    G_TYPE_INVALID,
				    G_TYPE_INVALID);

	g_message ("Thumbnailer queue sent with %d items to thumbnailer daemon, request ID:%d...", 
		   g_slist_length (private->uris),
		   private->request_id);

	/* Clean up newly created GStrv */
	g_strfreev (uri_strv);
	g_strfreev (mime_type_strv);

	/* Clean up privately held data */
	g_slist_foreach (private->uris, (GFunc) g_free, NULL);
	g_slist_free (private->uris);
	private->uris = NULL;

	g_slist_foreach (private->mime_types, (GFunc) g_free, NULL);
	g_slist_free (private->mime_types);
	private->mime_types = NULL;
}
