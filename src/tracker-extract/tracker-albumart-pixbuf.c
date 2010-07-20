/*
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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
 *
 * Authors:
 * Philip Van Hoof <philip@codeminded.be>
 */

#include <gdk-pixbuf/gdk-pixbuf.h>

#include "tracker-albumart-generic.h"

gboolean
tracker_albumart_file_to_jpeg (const gchar *filename,
                               const gchar *target)
{
	GdkPixbuf *pixbuf;
	GError *error = NULL;

	pixbuf = gdk_pixbuf_new_from_file (filename, &error);

	if (error) {
		g_clear_error (&error);

		return FALSE;
	} else {
		gdk_pixbuf_save (pixbuf, target, "jpeg", &error, NULL);
		g_object_unref (pixbuf);

		if (error) {
			g_clear_error (&error);
			return FALSE;
		}
	}

	return TRUE;
}


gboolean
tracker_albumart_buffer_to_jpeg (const unsigned char *buffer,
                                 size_t               len,
                                 const gchar         *buffer_mime,
                                 const gchar         *target)
{
	if (g_strcmp0 (buffer_mime, "image/jpeg") == 0 ||
	    g_strcmp0 (buffer_mime, "JPG") == 0) {

		g_debug ("Saving album art using raw data as uri:'%s'",
		         target);

		g_file_set_contents (target, buffer, (gssize) len, NULL);
	} else {
		GdkPixbuf *pixbuf;
		GdkPixbufLoader *loader;
		GError *error = NULL;

		g_debug ("Saving album art using GdkPixbufLoader for uri:'%s'",
		         target);

		loader = gdk_pixbuf_loader_new ();

		if (!gdk_pixbuf_loader_write (loader, buffer, len, &error)) {
			g_warning ("Could not write with GdkPixbufLoader when setting album art, %s",
			           error ? error->message : "no error given");

			g_clear_error (&error);
			gdk_pixbuf_loader_close (loader, NULL);

			return FALSE;
		}

		pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);

		if (pixbuf == NULL) {
			g_warning ("Could not get pixbuf from GdkPixbufLoader when setting album art");

			gdk_pixbuf_loader_close (loader, NULL);

			return FALSE;
		}

		if (!gdk_pixbuf_save (pixbuf, target, "jpeg", &error, NULL)) {
			g_warning ("Could not save GdkPixbuf when setting album art, %s",
			           error ? error->message : "no error given");

			g_clear_error (&error);
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

	return TRUE;
}
