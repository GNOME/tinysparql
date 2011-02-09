/*
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include "config.h"

#include <string.h>

#include <libtracker-common/tracker-dbus.h>

#include "tracker-thumbnailer.h"

/**
 * SECTION:tracker-thumbnailer
 * @short_description: Request the thumbnailer service creates or
 * updates thumbnails.
 * @include: libtracker-miner/tracker-miner.h
 *
 * This is a convenience API using D-Bus for creating and updating
 * thumbnails for files being mined. It is also used to create
 * thumbnails for album art found embedded in some medias.
 *
 * This follows the thumbnailer specification:
 * http://live.gnome.org/ThumbnailerSpec
 **/

#define THUMBCACHE_SERVICE      "org.freedesktop.thumbnails.Cache1"
#define THUMBCACHE_PATH         "/org/freedesktop/thumbnails/Cache1"
#define THUMBCACHE_INTERFACE    "org.freedesktop.thumbnails.Cache1"

#define THUMBMAN_SERVICE        "org.freedesktop.thumbnails.Thumbnailer1"
#define THUMBMAN_PATH           "/org/freedesktop/thumbnails/Thumbnailer1"
#define THUMBMAN_INTERFACE      "org.freedesktop.thumbnails.Thumbnailer1"

typedef struct {
	GDBusProxy *cache_proxy;
	GDBusProxy *manager_proxy;
	GDBusConnection *connection;

	GStrv supported_mime_types;

	GSList *removes;
	GSList *moves_to;
	GSList *moves_from;

	guint request_id;
	gboolean service_is_available;
} TrackerThumbnailerPrivate;

static GStaticPrivate private_key = G_STATIC_PRIVATE_INIT;

static void
private_free (gpointer data)
{
	TrackerThumbnailerPrivate *private;

	private = data;

	if (private->cache_proxy) {
		g_object_unref (private->cache_proxy);
	}

	if (private->manager_proxy) {
		g_object_unref (private->manager_proxy);
	}

	if (private->connection) {
		g_object_unref (private->connection);
	}

	g_strfreev (private->supported_mime_types);

	g_slist_foreach (private->removes, (GFunc) g_free, NULL);
	g_slist_free (private->removes);

	g_slist_foreach (private->moves_to, (GFunc) g_free, NULL);
	g_slist_free (private->moves_to);

	g_slist_foreach (private->moves_from, (GFunc) g_free, NULL);
	g_slist_free (private->moves_from);

	g_free (private);
}

inline static gboolean
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

/**
 * tracker_thumbnailer_init:
 *
 * Initializes the thumbnailer connection.
 *
 * Returns: #TRUE if connection was successfully initialized, #FALSE otherwise.
 *
 * Since: 0.8
 */
gboolean
tracker_thumbnailer_init (void)
{
	TrackerThumbnailerPrivate *private;
	GError *error = NULL;
	GVariant *v;

	private = g_new0 (TrackerThumbnailerPrivate, 1);

	/* Don't start at 0, start at 1. */
	private->request_id = 1;

	g_static_private_set (&private_key,
	                      private,
	                      private_free);

	g_message ("Thumbnailer connections being set up...");

	private->connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);

	if (!private->connection) {
		g_critical ("Could not connect to the D-Bus session bus, %s",
		            error ? error->message : "no error given.");
		g_clear_error (&error);

		private->service_is_available = FALSE;

		return FALSE;
	}

	private->cache_proxy = g_dbus_proxy_new_sync (private->connection,
	                                              G_DBUS_PROXY_FLAGS_NONE,
	                                              NULL,
	                                              THUMBCACHE_SERVICE,
	                                              THUMBCACHE_PATH,
	                                              THUMBCACHE_INTERFACE,
	                                              NULL,
	                                              &error);

	if (error) {
		goto error_handler;
	}

	private->manager_proxy = g_dbus_proxy_new_sync (private->connection,
	                                                G_DBUS_PROXY_FLAGS_NONE,
	                                                NULL,
	                                                THUMBMAN_SERVICE,
	                                                THUMBMAN_PATH,
	                                                THUMBMAN_INTERFACE,
	                                                NULL,
	                                                &error);

	if (error) {
		goto error_handler;
	}

	v = g_dbus_proxy_call_sync (private->manager_proxy,
	                            "GetSupported",
	                            NULL,
	                            G_DBUS_CALL_FLAGS_NONE,
	                            -1,
	                            NULL,
	                            &error);

error_handler:

	if (error) {
		g_message ("Thumbnailer service did not return supported mime types, %s",
		           error->message);

		g_error_free (error);

		if (private->cache_proxy) {
			g_object_unref (private->cache_proxy);
			private->cache_proxy = NULL;
		}

		if (private->manager_proxy) {
			g_object_unref (private->manager_proxy);
			private->manager_proxy = NULL;
		}

		return FALSE;
	} else if (v) {
		GStrv mime_types = NULL;
		GStrv uri_schemes = NULL;

		g_variant_get (v, "(^a&s^a&s)", &uri_schemes, &mime_types);

		if (mime_types) {
			GHashTable *hash;
			GHashTableIter iter;
			gpointer key, value;
			guint i;

			/* The table that you receive may contain duplicate mime-types, because
			 * they are grouped against the uri_schemes table */

			hash = g_hash_table_new (g_str_hash, g_str_equal);

			for (i = 0; mime_types[i] != NULL; i++) {
				g_hash_table_insert (hash, mime_types[i], NULL);
			}

			i = g_hash_table_size (hash);
			g_message ("Thumbnailer supports %d mime types", i);

			g_hash_table_iter_init (&iter, hash);
			private->supported_mime_types = (GStrv) g_new0 (gchar *, i + 1);

			i = 0;
			while (g_hash_table_iter_next (&iter, &key, &value)) {
				private->supported_mime_types[i] = g_strdup (key);
				i++;
			}

			g_hash_table_unref (hash);

			private->service_is_available = TRUE;
		}

		g_free (mime_types);
		g_free (uri_schemes);

		g_variant_unref (v);
	}

	return TRUE;
}

/**
 * tracker_thumbnailer_shutdown:
 *
 * Shuts down the thumbnailer connection.
 *
 * Since: 0.8
 */
void
tracker_thumbnailer_shutdown (void)
{
	g_static_private_set (&private_key, NULL, NULL);
}

/**
 * tracker_thumbnailer_move_add:
 * @from_uri: URI of the file before the move
 * @mime_type: mime-type of the file
 * @to_uri: URI of the file after the move
 *
 * Adds a new request to tell the thumbnailer that @from_uri was moved to
 * @to_uri. Stored requests can be sent with tracker_thumbnailer_send().
 *
 * Returns: #TRUE if successfully stored to be reported, #FALSE otherwise.
 *
 * Since: 0.8
 */
gboolean
tracker_thumbnailer_move_add (const gchar *from_uri,
                              const gchar *mime_type,
                              const gchar *to_uri)
{

	TrackerThumbnailerPrivate *private;

	/* mime_type can be NULL */

	g_return_val_if_fail (from_uri != NULL, FALSE);
	g_return_val_if_fail (to_uri != NULL, FALSE);

	private = g_static_private_get (&private_key);
	g_return_val_if_fail (private != NULL, FALSE);

	if (!private->service_is_available) {
		return FALSE;
	}

	if (mime_type && !should_be_thumbnailed (private->supported_mime_types, mime_type)) {
		return FALSE;
	}

	private->moves_from = g_slist_prepend (private->moves_from, g_strdup (from_uri));
	private->moves_to = g_slist_prepend (private->moves_to, g_strdup (to_uri));

	g_debug ("Thumbnailer request to move uri from:'%s' to:'%s' queued",
	         from_uri,
	         to_uri);

	return TRUE;
}

/**
 * tracker_thumbnailer_remove_add:
 * @uri: URI of the file
 * @mime_type: mime-type of the file
 *
 * Adds a new request to tell the thumbnailer that @uri was removed.
 * Stored requests can be sent with tracker_thumbnailer_send().
 *
 * Returns: #TRUE if successfully stored to be reported, #FALSE otherwise.
 *
 * Since: 0.8
 */
gboolean
tracker_thumbnailer_remove_add (const gchar *uri,
                                const gchar *mime_type)
{
	TrackerThumbnailerPrivate *private;

	/* mime_type can be NULL */

	g_return_val_if_fail (uri != NULL, FALSE);

	private = g_static_private_get (&private_key);
	g_return_val_if_fail (private != NULL, FALSE);

	if (!private->service_is_available) {
		return FALSE;
	}

	if (mime_type && !should_be_thumbnailed (private->supported_mime_types, mime_type)) {
		return FALSE;
	}

	private->removes = g_slist_prepend (private->removes, g_strdup (uri));

	g_debug ("Thumbnailer request to remove uri:'%s', appended to queue", uri);

	return TRUE;
}

/**
 * tracker_thumbnailer_cleanup:
 * @uri_prefix: URI prefix
 *
 * Tells thumbnailer to cleanup all thumbnails under @uri_prefix.
 *
 * Returns: #TRUE if successfully reported, #FALSE otherwise.
 *
 * Since: 0.8
 */
gboolean
tracker_thumbnailer_cleanup (const gchar *uri_prefix)
{
	TrackerThumbnailerPrivate *private;

	g_return_val_if_fail (uri_prefix != NULL, FALSE);

	private = g_static_private_get (&private_key);
	g_return_val_if_fail (private != NULL, FALSE);

	if (!private->service_is_available) {
		return FALSE;
	}

	private->request_id++;

	g_debug ("Thumbnailer cleaning up uri:'%s', request_id:%d...",
	         uri_prefix,
	         private->request_id);

	g_dbus_proxy_call (private->cache_proxy,
	                   "Cleanup",
	                   g_variant_new ("(s)", uri_prefix),
	                   G_DBUS_CALL_FLAGS_NONE,
	                   -1,
	                   NULL,
	                   NULL,
	                   NULL);

	return TRUE;
}

/**
 * tracker_thumbnailer_send:
 *
 * Sends to the thumbnailer all stored requests.
 *
 * Since: 0.8
 */
void
tracker_thumbnailer_send (void)
{
	TrackerThumbnailerPrivate *private;
	guint list_len;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	if (!private->service_is_available) {
		return;
	}

	list_len = g_slist_length (private->removes);

	if (list_len > 0) {
		GStrv uri_strv;

		uri_strv = tracker_dbus_slist_to_strv (private->removes);

		g_dbus_proxy_call (private->cache_proxy,
		                   "Delete",
		                   g_variant_new ("(^as)", uri_strv),
		                   G_DBUS_CALL_FLAGS_NONE,
		                   -1,
		                   NULL,
		                   NULL,
		                   NULL);

		g_message ("Thumbnailer removes queue sent with %d items to thumbnailer daemon, request ID:%d...",
		           list_len,
		           private->request_id++);

		/* Clean up newly created GStrv */
		g_strfreev (uri_strv);

		/* Clean up privately held data */
		g_slist_foreach (private->removes, (GFunc) g_free, NULL);
		g_slist_free (private->removes);
		private->removes = NULL;
	}

	list_len = g_slist_length (private->moves_from);

	if (list_len > 0) {
		GStrv from_strv, to_strv;

		g_assert (list_len == g_slist_length (private->moves_to));

		from_strv = tracker_dbus_slist_to_strv (private->moves_from);
		to_strv = tracker_dbus_slist_to_strv (private->moves_to);

		g_dbus_proxy_call (private->cache_proxy,
		                   "Move",
		                   g_variant_new ("(^as^as)", from_strv, to_strv),
		                   G_DBUS_CALL_FLAGS_NONE,
		                   -1,
		                   NULL,
		                   NULL,
		                   NULL);

		g_message ("Thumbnailer moves queue sent with %d items to thumbnailer daemon, request ID:%d...",
		           list_len,
		           private->request_id++);

		/* Clean up newly created GStrv */
		g_strfreev (from_strv);
		g_strfreev (to_strv);

		/* Clean up privately held data */
		g_slist_foreach (private->moves_from, (GFunc) g_free, NULL);
		g_slist_free (private->moves_from);
		private->moves_from = NULL;

		g_slist_foreach (private->moves_to, (GFunc) g_free, NULL);
		g_slist_free (private->moves_to);
		private->moves_to = NULL;
	}
}
