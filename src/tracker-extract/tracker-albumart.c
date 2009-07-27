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

#include "tracker-main.h"
#include "tracker-albumart.h"

static GHashTable *albumart;
static TrackerStorage *hal;

#ifdef HAVE_GDKPIXBUF

static gboolean
set_albumart (const unsigned char *buffer,
	      size_t               len,
	      const gchar         *mime,
	      const gchar         *artist, 
	      const gchar         *album,
	      const gchar         *uri,
	      gboolean            *was_thumbnail_queued)
{
	GdkPixbufLoader *loader;
	GdkPixbuf       *pixbuf = NULL;
	gchar           *filename;
	GError          *error = NULL;

	if (was_thumbnail_queued) {
		*was_thumbnail_queued = FALSE;
	}

	if (!artist && !album) {
		g_warning ("No identification data for embedded image");
		return FALSE;
	}

	tracker_albumart_get_path (artist, album, "album", NULL, &filename, NULL);

	if (g_strcmp0 (mime, "image/jpeg") == 0 ||
	    g_strcmp0 (mime, "JPG") == 0) {
		g_file_set_contents (filename, buffer, (gssize) len, NULL);
	} else {
		loader = gdk_pixbuf_loader_new ();

		if (!gdk_pixbuf_loader_write (loader, buffer, len, &error)) {
			g_warning ("Could not write with GdkPixbufLoader when setting album art, %s", 
				   error ? error->message : "no error given");

			g_clear_error (&error);
			gdk_pixbuf_loader_close (loader, NULL);
			g_free (filename);

			return FALSE;
		}

		pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);

		if (!gdk_pixbuf_save (pixbuf, filename, "jpeg", &error, NULL)) {
			g_warning ("Could not save GdkPixbuf when setting album art, %s", 
				   error ? error->message : "no error given");

			g_clear_error (&error);
			g_free (filename);
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

	if (was_thumbnail_queued) {
		*was_thumbnail_queued = TRUE;
	}

	tracker_thumbnailer_queue_file (filename, "image/jpeg");
	g_free (filename);

	return TRUE;
}

#endif /* HAVE_GDKPIXBUF */

void
tracker_albumart_init (void)
{
	g_type_init ();

	albumart = g_hash_table_new_full (g_str_hash,
					  g_str_equal,
					  (GDestroyNotify) g_free,
					  NULL);

#ifdef HAVE_HAL
	hal = tracker_storage_new ();
#else 
	hal = NULL;
#endif
}

void
tracker_albumart_shutdown (void)
{
	g_hash_table_unref (albumart);
}

gboolean
tracker_albumart_process (const unsigned char *buffer,
                          size_t               len,
                          const gchar         *mime,
                          const gchar         *artist,
                          const gchar         *album,
                          const gchar         *trackercnt_str,
                          const gchar         *filename)
{
	gchar *art_path;
	gboolean processed = TRUE;
	gchar *local_uri = NULL;
	gchar *filename_uri;
	gboolean art_exists;
	gboolean was_thumbnail_queued;

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
		g_free (filename_uri);
		g_free (local_uri);

		return FALSE;
	}

	art_exists = g_file_test (art_path, G_FILE_TEST_EXISTS);

	if (!art_exists) {
#ifdef HAVE_GDKPIXBUF
		/* If we have embedded album art */
		if (buffer && len) {
			processed = set_albumart (buffer, 
						  len, 
						  mime,
						  artist,
						  album,
						  filename,
						  &was_thumbnail_queued);
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

			if (!g_hash_table_lookup (albumart, key)) {
				if (!tracker_albumart_heuristic (artist, 
								 album, 
					                         trackercnt_str, 
					                         filename, 
					                         local_uri, 
					                         NULL)) {
					/* If the heuristic failed, we
					 * request the download the
					 * media-art to the media-art
					 * downloaders
					 */
					tracker_albumart_request_download (hal, 
									   artist,
									   album,
									   local_uri,
									   art_path);
				}

				g_hash_table_insert (albumart, 
						     key, 
						     GINT_TO_POINTER(TRUE));
			} else {
				g_free (key);
			}
#ifdef HAVE_GDKPIXBUF
		}
#endif /* HAVE_GDKPIXBUF */

		if (!was_thumbnail_queued) {
			was_thumbnail_queued = TRUE;
			tracker_thumbnailer_queue_file (filename_uri, "image/jpeg");
		}
	}

	if (local_uri && !g_file_test (local_uri, G_FILE_TEST_EXISTS)) {
		/* We can't reuse art_exists here because the
		 * situation might have changed
		 */
		if (g_file_test (art_path, G_FILE_TEST_EXISTS)) {
			tracker_albumart_copy_to_local (hal,
							art_path, 
							local_uri);
		}
	}

	g_free (art_path);
	g_free (filename_uri);
	g_free (local_uri);

	return processed;
}
