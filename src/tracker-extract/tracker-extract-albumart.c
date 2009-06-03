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

#include <libtracker-common/tracker-common.h>
#include <libtracker-common/tracker-thumbnailer.h>

#include "tracker-main.h"
#include "tracker-extract-albumart.h"

#ifdef HAVE_GDKPIXBUF

static GHashTable *album_art_done = NULL;

static gboolean
set_albumart (const unsigned char *buffer,
	      size_t               len,
	      const gchar         *mime,
	      const gchar         *artist, 
	      const gchar         *album,
	      const gchar         *uri)
{
	GdkPixbufLoader *loader;
	GdkPixbuf       *pixbuf = NULL;
	gchar           *filename;
	GError          *error = NULL;

	g_type_init ();

	if (!artist && !album) {
		g_warning ("No identification data for embedded image");
		return FALSE;
	}

	tracker_albumart_get_path (artist, album, "album", NULL, &filename, NULL);

	if (g_strcmp0 (mime, "image/jpeg") == 0 || g_strcmp0 (mime, "JPG") == 0) {
		g_file_set_contents (filename, buffer, (gssize) len, NULL);
	} else {
		loader = gdk_pixbuf_loader_new ();

		if (!gdk_pixbuf_loader_write (loader, buffer, len, &error)) {
			g_warning ("%s\n", error->message);
			g_error_free (error);

			gdk_pixbuf_loader_close (loader, NULL);
			g_free (filename);
			return FALSE;
		}

		pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);

		if (!gdk_pixbuf_save (pixbuf, filename, "jpeg", &error, NULL)) {
			g_warning ("%s\n", error->message);
			g_error_free (error);

			g_free (filename);
			g_object_unref (pixbuf);

			gdk_pixbuf_loader_close (loader, NULL);
			return FALSE;
		}


		g_object_unref (pixbuf);

		if (!gdk_pixbuf_loader_close (loader, &error)) {
			g_warning ("%s\n", error->message);
			g_error_free (error);
		}
	}

	tracker_thumbnailer_queue_file (filename, "image/jpeg");
	g_free (filename);

	return TRUE;
}

#endif /* HAVE_GDKPIXBUF */

gboolean
tracker_process_albumart (const unsigned char *buffer,
                          size_t               len,
                          const gchar         *buf_mime,
                          const gchar         *artist,
                          const gchar         *album,
                          const gchar         *trackercnt_str,
                          const gchar         *filename)
{
	gchar *art_path;
	gboolean retval = TRUE;
	gchar *local_uri = NULL;
	gchar *filename_uri;
	gboolean lcopied = FALSE;
	gboolean art_exists;
	gchar *as_uri;

	if (strchr (filename, ':')) {
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
			retval = set_albumart (buffer, 
					       len, buf_mime,
					       artist,
					       album,
					       filename);

			lcopied = !retval;

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

			key = g_strdup_printf ("%s-%s-%s", artist ? artist : "",
					       album ? album : "",
					       dirname ? dirname : "");

			g_free (dirname);

			/* We store these in a table because we want to avoid 
			  * that we do many requests for the same directory
			  * subsequently without success. It's a small but
			  * known leak on the variable "key" and the hashtable
			  * itself.
			  *
			  * We could get rid of the leak by having a shutdown 
			  * function for extract modules. I don't think this 
			  * mini leak is enough reason to add such shutdown
			  * infrastructure to the extract modules. */

			if (!album_art_done) {
				album_art_done = g_hash_table_new_full (g_str_hash,
									g_str_equal,
									(GDestroyNotify) g_free,
									NULL);
			}

			if (!g_hash_table_lookup (album_art_done, key)) {
				if (!tracker_albumart_heuristic (artist, album, 
					                         trackercnt_str, 
					                         filename, 
					                         local_uri, 
					                         &lcopied)) {

					/* If the heuristic failed, we request the download 
					 * of the media-art to the media-art downloaders */
					lcopied = TRUE;
					tracker_albumart_request_download (tracker_main_get_hal (), 
									   artist,
									   album,
									   local_uri,
									   art_path);
				}
				g_hash_table_insert (album_art_done, key, GINT_TO_POINTER(TRUE));
			} else {
				g_free (key);
			}
#ifdef HAVE_GDKPIXBUF
		}
#endif /* HAVE_GDKPIXBUF */

		as_uri = g_filename_to_uri (art_path, NULL, NULL);
		tracker_thumbnailer_queue_file (as_uri, "image/jpeg");
		g_free (as_uri);

	}

	if (local_uri && !g_file_test (local_uri, G_FILE_TEST_EXISTS)) {
		/* We can't reuse art_exists here because the situation might
		 * have changed */
		if (g_file_test (art_path, G_FILE_TEST_EXISTS)) {
			tracker_albumart_copy_to_local (tracker_main_get_hal (),
							art_path, 
							local_uri);
		}
	}

	g_free (art_path);
	g_free (filename_uri);
	g_free (local_uri);

	return retval;
}

