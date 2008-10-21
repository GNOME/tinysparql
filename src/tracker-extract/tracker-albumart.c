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

#include <dbus/dbus-glib-bindings.h>

#include "tracker-albumart.h"

#define ALBUMARTER_SERVICE      "com.nokia.albumart"
#define ALBUMARTER_PATH         "/com/nokia/albumart/Requester"
#define ALBUMARTER_INTERFACE    "com.nokia.albumart.Requester"

static void get_albumart_path (const gchar  *a, 
			       const gchar  *b, 
			       const gchar  *prefix, 
			       gchar       **path);

#ifndef HAVE_STRCASESTR

static gchar *
strcasestr (const gchar *haystack, 
	    const gchar *needle)
{
	gchar *p;
	gchar *startn = NULL;
	gchar *np = NULL;

	for (p = (gchar *) haystack; *p; p++) {
		if (np) {
			if (toupper (*p) == toupper (*np)) {
				if (!*++np) {
					return startn;
				}
			} else {
				np = 0;
			}
		} else if (toupper (*p) == toupper (*needle)) {
			np = (gchar *) needle + 1;
			startn = p;
		}
	}

	return NULL;
}

#endif /* HAVE_STRCASESTR */

static gboolean 
heuristic_albumart (const gchar *artist,  
		    const gchar *album, 
		    const gchar *tracks_str, 
		    const gchar *filename)
{
	GFile *file;
	GDir *dir;
	struct stat st;
	gchar *target = NULL;
	gchar *basename;
	const gchar *name;
	gboolean retval;
	gint tracks;
	gint count;
	
	file = g_file_new_for_path (filename);
	basename = g_file_get_basename (file);
	g_object_unref (file);

	if (!basename) {
		return FALSE;
	}

	dir = g_dir_open (basename, 0, NULL);

	if (!dir) {
		g_free (basename);
		return FALSE;
	}
	
	retval = FALSE;
	file = NULL;

	g_stat (basename, &st);
	count = st.st_nlink;
	
	if (tracks_str) {
		tracks = atoi (tracks_str);
	} else {
		tracks = -1;
	}
	
	if ((tracks != -1 && tracks < count + 3 && tracks > count - 3) || 
	    (tracks == -1 && count > 8 && count < 50)) {
		gchar *found = NULL;
		
		for (name = g_dir_read_name (dir); name; name = g_dir_read_name (dir)) {
			if ((artist && strcasestr (name, artist)) || 
			    (album && strcasestr (name, album)) || 
			    (strcasestr (name, "cover"))) {
				GError *error = NULL;
				
				if (g_str_has_suffix (name, "jpeg") || 
				    g_str_has_suffix (name, "jpg")) {
					GFile *file_found;
					
					if (!target) {
						get_albumart_path (artist, album, "album", &target);
					}
					
					if (!file) {
						file = g_file_new_for_path (target);
					}
					
					found = g_build_filename (basename, name, NULL);
					file_found = g_file_new_for_path (found);
					
					g_file_copy (file_found, file, 0, NULL, NULL, NULL, &error);
					
					if (!error) {
						retval = TRUE;
					} else {
						g_error_free (error);
						retval = FALSE;
					}
					
					g_free (found);
					g_object_unref (file_found);
				} else {
#ifdef HAVE_GDKPIXBUF
					GdkPixbuf *pixbuf;
					
					found = g_build_filename (basename, name, NULL);
					pixbuf = gdk_pixbuf_new_from_file (found, &error);
					
					if (error) {
						g_error_free (error);
						retval = FALSE;
					} else {
						if (!target) {
							get_albumart_path (artist, album, "album", &target);
						}
						
						gdk_pixbuf_save (pixbuf, target, "jpeg", &error, NULL);
						
						if (!error) {
							retval = TRUE;
						} else {
							g_error_free (error);
							retval = FALSE;
						}
					}
					
					g_free (found);
#else  /* HAVE_GDKPIXBUF */
					retval = FALSE;
#endif /* HAVE_GDKPIXBUF */
				}
				
				break;
			}
		}
		
	}
	
	g_dir_close (dir);
	
	if (file) {
		g_object_unref (file);
	}
	
	g_free (target);
	g_free (basename);

	return retval;
}

static DBusGProxy*
get_albumart_requester (void)
{
	static DBusGProxy *albart_proxy = NULL;

	if (!albart_proxy) {
		GError          *error = NULL;
		DBusGConnection *connection;

		connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

		if (!error) {
			albart_proxy = dbus_g_proxy_new_for_name (connection,
								  ALBUMARTER_SERVICE,
								  ALBUMARTER_PATH,
								  ALBUMARTER_INTERFACE);
		} else {
			g_error_free (error);
		}
	}

	return albart_proxy;
}

static void
get_file_albumart_queue_cb (DBusGProxy     *proxy,
			    DBusGProxyCall *call,
			    gpointer	     user_data)
{
	GError *error = NULL;
	guint	handle;

	dbus_g_proxy_end_call (proxy, call, &error,
			       G_TYPE_UINT, &handle,
			       G_TYPE_INVALID);

	if (error) {
		g_warning (error->message);
		g_error_free (error);
	}
}

static void
get_albumart_path (const gchar  *a, 
		   const gchar  *b, 
		   const gchar  *prefix, 
		   gchar       **path)
{
	gchar *art_filename;
	gchar *dir;
	gchar *str;
	gchar *down;

	if (!prefix) {
		prefix = "album";
	}

	*path = NULL;

	if (!a && !b) {
		return;
	}

	str = g_strconcat (a ? a : "", 
			   " ", 
			   b ? b : "", 
			   NULL);
	down = g_utf8_strdown (str, -1);
	g_free (str);

	dir = g_build_filename (g_get_user_cache_dir (), "media-art", NULL);

	if (!g_file_test (dir, G_FILE_TEST_EXISTS)) {
		g_mkdir_with_parents (dir, 0770);
	}

	str = g_compute_checksum_for_string (G_CHECKSUM_MD5, down, -1);
	g_free (down);

	art_filename = g_strdup_printf ("%s-%s.jpeg", prefix, str);
	g_free (str);

	*path = g_build_filename (dir, art_filename, NULL);
	g_free (dir);
	g_free (art_filename);
}

#ifdef HAVE_GDKPIXBUF

static gboolean
set_albumart (const unsigned char *buffer,
	      size_t               len,
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

	get_albumart_path (artist, album, "album", &filename);

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

#endif /* HAVE_GDKPIXBUF */

gboolean
tracker_process_albumart (const unsigned char *buffer,
                          size_t               len,
                          const gchar         *artist,
                          const gchar         *album,
                          const gchar         *trackercnt_str,
                          const gchar         *filename)
{
	gchar *art_path;
	gboolean retval = TRUE;

	get_albumart_path (artist, album, "album", &art_path);

	if (!g_file_test (art_path, G_FILE_TEST_EXISTS)) {
#ifdef HAVE_GDKPIXBUF
		if (buffer && len) {
			retval = set_albumart (buffer, len,
					       artist,
					       album,
					       filename);
			
		} else {
#endif /* HAVE_GDK_PIXBUF */
			if (!heuristic_albumart (artist, album, trackercnt_str, filename)) {
				dbus_g_proxy_begin_call (get_albumart_requester (),
					 "Queue",
					 get_file_albumart_queue_cb,
					 NULL, NULL,
					 G_TYPE_STRING, artist,
					 G_TYPE_STRING, album,
					 G_TYPE_STRING, "album",
					 G_TYPE_UINT, 0,
					 G_TYPE_INVALID);
			}
#ifdef HAVE_GDKPIXBUF

		}

#endif /* HAVE_GDKPIXBUF */
	}

	g_free (art_path);

	return retval;
}
