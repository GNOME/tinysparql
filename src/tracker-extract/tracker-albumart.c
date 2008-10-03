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

#include <glib.h>
#include <glib/gprintf.h>

#include <gio/gio.h>

#include <gdk-pixbuf/gdk-pixbuf.h>

#include "tracker-albumart.h"

gboolean
tracker_save_albumart (const unsigned char *buffer,
		       size_t               len,
		       const gchar         *artist, 
		       const gchar         *album,
		       const gchar         *uri)
{
	GdkPixbufLoader *loader;
	GdkPixbuf       *pixbuf = NULL;
	gchar            name[128];
	gchar           *filename;
	gchar           *dir;
	gchar           *checksum;
	GError          *error = NULL;

	g_type_init ();

	if (artist && album) {
		g_sprintf (name, "%s %s", album, artist);
	} else if (uri) {
		g_sprintf (name, "%s", uri);
	} else {
		g_warning ("No identification data for embedded image");		
		return FALSE;
	}

	checksum = g_compute_checksum_for_string (G_CHECKSUM_MD5, name, -1);
	g_sprintf (name, "%s.png", checksum);
	g_free (checksum);

	dir = g_build_filename (g_get_home_dir (),
				".album_art",
				NULL);

	filename = g_build_filename (dir, name, NULL);

	if (g_file_test (filename, G_FILE_TEST_EXISTS)) {
		g_free (filename);
		g_free (dir);

		return TRUE;
	}

	if (!g_file_test (dir, G_FILE_TEST_EXISTS)) {
		g_mkdir_with_parents (dir, 0770);
	}

	g_free (dir);

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

	g_free (filename);
	g_object_unref (pixbuf);

	if (!gdk_pixbuf_loader_close (loader, &error)) {
		g_warning ("%s\n", error->message);
		g_error_free (error);
		return FALSE;
	}

	return TRUE;
}
