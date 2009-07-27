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

#include "config.h"

#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>

#include <glib.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>

#include <gio/gio.h>

#ifdef HAVE_GDKPIXBUF
#include <gdk-pixbuf/gdk-pixbuf.h>
#endif

#include <libtracker-common/tracker-thumbnailer.h>
#include <libtracker-common/tracker-albumart.h>

#include "tracker-albumart.h"
#include "tracker-dbus.h"
#include "tracker-marshal.h"

static gboolean initialized;
static GHashTable *albumart_cache;
static TrackerStorage *albumart_storage;

static gboolean albumart_process_cb (DBusGProxy          *proxy,
				     const unsigned char *buffer,
				     size_t               len,
				     const gchar         *mime,
				     const gchar         *artist,
				     const gchar         *album,
				     const gchar         *filename,
				     gpointer             user_data);

#ifdef HAVE_GDKPIXBUF

static gboolean
albumart_set (const unsigned char *buffer,
	      size_t               len,
	      const gchar         *mime,
	      const gchar         *artist, 
	      const gchar         *album,
	      const gchar         *uri)
{
	gchar *local_path;

	if (!artist && !album) {
		g_warning ("Could not save embedded album art, no artist or album supplied");
		return FALSE;
	}

	tracker_albumart_get_path (artist, album, "album", NULL, &local_path, NULL);

	g_message ("Saving album art using GdkPixbuf for uri:'%s'", 
		   local_path);

	if (g_strcmp0 (mime, "image/jpeg") == 0 ||
	    g_strcmp0 (mime, "JPG") == 0) {
		g_file_set_contents (local_path, buffer, (gssize) len, NULL);
	} else {
		GdkPixbuf *pixbuf;
		GdkPixbufLoader *loader;
		GError *error = NULL;

		loader = gdk_pixbuf_loader_new ();

		if (!gdk_pixbuf_loader_write (loader, buffer, len, &error)) {
			g_warning ("Could not write with GdkPixbufLoader when setting album art, %s", 
				   error ? error->message : "no error given");

			g_clear_error (&error);
			gdk_pixbuf_loader_close (loader, NULL);
			g_free (local_path);

			return FALSE;
		}

		pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);

		if (!gdk_pixbuf_save (pixbuf, local_path, "jpeg", &error, NULL)) {
			g_warning ("Could not save GdkPixbuf when setting album art, %s", 
				   error ? error->message : "no error given");

			g_clear_error (&error);
			g_free (local_path);
			g_object_unref (pixbuf);
			gdk_pixbuf_loader_close (loader, NULL);

			return FALSE;
		}

		g_object_unref (pixbuf);

		if (!gdk_pixbuf_loader_close (loader, &error)) {
			g_warning ("Could not close GdkPixbufLoader when setting album art, %s", 
				   error ? error->message : "no error given");
			g_clear_error (&error);
		}
	}

	tracker_thumbnailer_queue_add (local_path, "image/jpeg"); 
	g_free (local_path);

	return TRUE;
}

#endif /* HAVE_GDKPIXBUF */

static gboolean
albumart_process_cb (DBusGProxy          *proxy,
		     const unsigned char *buffer,
		     size_t               len,
		     const gchar         *mime,
		     const gchar         *artist,
		     const gchar         *album,
		     const gchar         *filename,
		     gpointer             user_data)
{
	gchar *art_path;
	gboolean processed = TRUE;
	gchar *local_uri = NULL;
	gchar *filename_uri;

	g_message ("***** PROCESSING NEW ALBUM ART!! *****");

	if (strstr (filename, "://")) {
		filename_uri = g_strdup (filename);
	} else {
		filename_uri = g_filename_to_uri (filename, NULL, NULL);
	}

	tracker_albumart_get_path (artist, 
				   album, 
				   "album", 
				   filename_uri, 
				   &art_path, 
				   &local_uri);

	if (!art_path) {
		g_warning ("Albumart path could not be obtained, not processing any further");

		g_free (filename_uri);
		g_free (local_uri);

		return FALSE;
	}

	if (!g_file_test (art_path, G_FILE_TEST_EXISTS)) {
#ifdef HAVE_GDKPIXBUF
		/* If we have embedded album art */
		if (buffer && len) {
			processed = albumart_set (buffer, 
						  len, 
						  mime,
						  artist,
						  album,
						  filename);
		} else {
#endif /* HAVE_GDK_PIXBUF */
			/* If not, we perform a heuristic on the dir */
			gchar *key;
			gchar *dirname;
			GFile *file, *dirf;

			file = g_file_new_for_path (filename);
			dirf = g_file_get_parent (file);
			dirname = g_file_get_path (dirf);
			g_object_unref (file);
			g_object_unref (dirf);

			key = g_strdup_printf ("%s-%s-%s", 
					       artist ? artist : "",
					       album ? album : "",
					       dirname ? dirname : "");

			g_free (dirname);

			if (!g_hash_table_lookup (albumart_cache, key)) {
				if (!tracker_albumart_heuristic (artist, 
								 album, 
					                         filename, 
					                         local_uri, 
					                         NULL)) {
					/* If the heuristic failed, we
					 * request the download the
					 * media-art to the media-art
					 * downloaders
					 */
					tracker_albumart_request_download (albumart_storage, 
									   artist,
									   album,
									   local_uri,
									   art_path);
				}

				g_hash_table_insert (albumart_cache, 
						     key, 
						     GINT_TO_POINTER(TRUE));
			} else {
				g_free (key);
			}
#ifdef HAVE_GDKPIXBUF
		}
#endif /* HAVE_GDKPIXBUF */

		if (processed) {
			tracker_thumbnailer_queue_add (filename_uri, "image/jpeg"); 
		}
	} else {
		g_message ("Albumart already exists for uri:'%s'", 
			   filename_uri);
	}

	if (local_uri && !g_file_test (local_uri, G_FILE_TEST_EXISTS)) {
		/* We can't reuse art_exists here because the
		 * situation might have changed
		 */
		if (g_file_test (art_path, G_FILE_TEST_EXISTS)) {
			tracker_albumart_copy_to_local (albumart_storage,
							art_path, 
							local_uri);
		}
	}

	g_free (art_path);
	g_free (filename_uri);
	g_free (local_uri);

	return processed;
}

gboolean
tracker_albumart_init (TrackerStorage *storage)
{
	DBusGProxy *proxy;
	DBusGConnection *connection;
	GError *error = NULL;

	g_return_val_if_fail (initialized == FALSE, FALSE);

	albumart_cache = g_hash_table_new_full (g_str_hash,
						g_str_equal,
						(GDestroyNotify) g_free,
						NULL);

#ifdef HAVE_HAL
	g_return_val_if_fail (TRACKER_IS_STORAGE (storage), FALSE);

	albumart_storage = g_object_ref (storage);
#endif /* HAVE_HAL */

	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

	if (!connection) {
		g_critical ("Could not connect to the DBus session bus, %s",
			    error ? error->message : "no error given.");
		g_clear_error (&error);
		return FALSE;
	}

	/* Get proxy for Service / Path / Interface of the indexer */
	proxy = dbus_g_proxy_new_for_name (connection,
					   "org.freedesktop.Tracker.Extract",
					   "/org/freedesktop/Tracker/Extract",
					   "org.freedesktop.Tracker.Extract");

	if (!proxy) {
		g_critical ("Could not create a DBusGProxy to the extract service");
                return FALSE;
	}

        dbus_g_object_register_marshaller (tracker_marshal_VOID__UCHAR_INT_STRING_STRING_STRING_STRING,
                                           G_TYPE_NONE,
					   G_TYPE_UCHAR,
					   G_TYPE_INT,
					   G_TYPE_STRING,
					   G_TYPE_STRING,
                                           G_TYPE_STRING,
                                           G_TYPE_STRING,
                                           G_TYPE_INVALID);
	
        dbus_g_proxy_add_signal (proxy,
                                 "ProcessAlbumArt",
                                 G_TYPE_UCHAR,
                                 G_TYPE_INT,
                                 G_TYPE_STRING,
                                 G_TYPE_STRING,
                                 G_TYPE_STRING,
                                 G_TYPE_STRING,
                                 G_TYPE_INVALID);
        			
        dbus_g_proxy_connect_signal (proxy,
                                     "ProcessAlbumArt",
                                     G_CALLBACK (albumart_process_cb),
                                     NULL,
                                     NULL);

	return TRUE;
}

void
tracker_albumart_shutdown (void)
{
	g_return_if_fail (initialized == TRUE);

	if (albumart_cache) {
		g_hash_table_unref (albumart_cache);
	}

#ifdef HAVE_HAL
	if (albumart_storage) {
		g_object_unref (albumart_storage);
	}
#endif /* HAVE_HAL */
}
