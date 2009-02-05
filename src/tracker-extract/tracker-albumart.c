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

#include <dbus/dbus-glib-bindings.h>

#include <libtracker-common/tracker-common.h>
#include <libtracker-common/tracker-thumbnailer.h>

#include "tracker-albumart.h"

#define ALBUMARTER_SERVICE      "com.nokia.albumart"
#define ALBUMARTER_PATH         "/com/nokia/albumart/Requester"
#define ALBUMARTER_INTERFACE    "com.nokia.albumart.Requester"

#define THUMBNAILER_SERVICE      "org.freedesktop.thumbnailer"
#define THUMBNAILER_PATH         "/org/freedesktop/thumbnailer/Generic"
#define THUMBNAILER_INTERFACE    "org.freedesktop.thumbnailer.Generic"


static void get_albumart_path (const gchar  *a, 
			       const gchar  *b, 
			       const gchar  *prefix, 
			       const gchar  *uri,
			       gchar       **path,
			       gchar       **local);

static gchar *
my_compute_checksum_for_data (GChecksumType  checksum_type,
                              const guchar  *data,
                              gsize          length)
{
  GChecksum *checksum;
  gchar *retval;

  checksum = g_checksum_new (checksum_type);
  if (!checksum)
    return NULL;

  g_checksum_update (checksum, data, length);
  retval = g_strdup (g_checksum_get_string (checksum));
  g_checksum_free (checksum);

  return retval;
}

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

/* NOTE: This function was stolen from GLib 2.18.x. Since upstream and
 * the Maemo branch don't have this in circulation yet, we have copied
 * it here. This can be removed an replaced with
 * g_file_make_directory_with_parents() when we get it. -mr
 */
static gboolean
make_directory_with_parents (GFile         *file,
			     GCancellable  *cancellable,
			     GError       **error)
{
  gboolean result;
  GFile *parent_file, *work_file;
  GList *list = NULL, *l;
  GError *my_error = NULL;

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return FALSE;

  result = g_file_make_directory (file, cancellable, &my_error);
  if (result || my_error->code != G_IO_ERROR_NOT_FOUND)
    {
      if (my_error)
        g_propagate_error (error, my_error);
      return result;
    }

  work_file = file;

  while (!result && my_error->code == G_IO_ERROR_NOT_FOUND)
    {
      g_clear_error (&my_error);

      parent_file = g_file_get_parent (work_file);
      if (parent_file == NULL)
        break;
      result = g_file_make_directory (parent_file, cancellable, &my_error);

      if (!result && my_error->code == G_IO_ERROR_NOT_FOUND)
        list = g_list_prepend (list, parent_file);

      work_file = parent_file;
    }

  for (l = list; result && l; l = l->next)
    {
      result = g_file_make_directory ((GFile *) l->data, cancellable, &my_error);
    }

  /* Clean up */
  while (list != NULL)
    {
      g_object_unref ((GFile *) list->data);
      list = g_list_remove (list, list->data);
    }

  if (!result)
    {
      g_propagate_error (error, my_error);
      return result;
    }

  return g_file_make_directory (file, cancellable, error);
}



static gchar*
strip_characters (const gchar *original)
{
	const gchar *foo = "()[]<>{}_!@#$^&*+=|\\/\"'?~";
	guint osize = strlen (original);
	gchar *retval = (gchar *) g_malloc0 (sizeof (gchar *) * osize + 1);
	guint i = 0, y = 0;

	while (i < osize) {

		/* Remove (anything) */

		if (original[i] == '(') {
			gchar *loc = strchr (original+i, ')');
			if (loc) {
				i = loc - original + 1;
				continue;
			}
		}

		/* Remove [anything] */

		if (original[i] == '[') {
			gchar *loc = strchr (original+i, ']');
			if (loc) {
				i = loc - original + 1;
				continue;
			}
		}

		/* Remove {anything} */

		if (original[i] == '{') {
			gchar *loc = strchr (original+i, '}');
			if (loc) {
				i = loc - original + 1;
				continue;
			}
		}

		/* Remove <anything> */

		if (original[i] == '<') {
			gchar *loc = strchr (original+i, '>');
			if (loc) {
				i = loc - original + 1;
				continue;
			}
		}

		/* Remove double whitespaces */

		if ((y > 0) &&
		    (original[i] == ' ' || original[i] == '\t') &&
		    (retval[y-1] == ' ' || retval[y-1] == '\t')) {
			i++;
			continue;
		}

		/* Remove strange characters */

		if (!strchr (foo, original[i])) {
			retval[y] = original[i]!='\t'?original[i]:' ';
			y++;
		}

		i++;
	}

	retval[y] = 0;

	return retval;
}


static void
perhaps_copy_to_local (const gchar *filename, const gchar *local_uri)
{
#ifdef HAVE_HAL
	TrackerHal *hal;
#endif
	GList *removable_roots, *l;
	gboolean on_removable_device = FALSE;
	guint flen;

	if (!filename)
		return;

	if (!local_uri)
		return;

	flen = strlen (filename);

	/* Determining if we are on a removable device */

#ifdef HAVE_HAL
	hal = tracker_hal_new ();
	removable_roots = tracker_hal_get_removable_device_roots (hal);
	g_object_unref (hal);
#else
	removable_roots = g_list_append (removable_roots, "/media");
	removable_roots = g_list_append (removable_roots, "/mnt");
#endif

	for (l = removable_roots; l; l = l->next) {
		guint len;
		
		len = strlen (l->data);

		if (flen >= len && strncmp (filename, l->data, len)) {
			on_removable_device = TRUE;
			break;
		}
	}

#ifdef HAVE_HAL
	g_list_foreach (removable_roots, (GFunc) g_free, NULL);
#endif

	g_list_free (removable_roots);

	if (on_removable_device) {
		GFile *local_file, *from;

		from = g_file_new_for_path (filename);
		local_file = g_file_new_for_uri (local_uri);

		/* We don't try to overwrite, but we also ignore all errors.
		 * Such an error could be that the removable device is 
		 * read-only. Well that's fine then ... ignore */

		if (!g_file_query_exists (local_file, NULL)) {
			GFile *dirf;

			dirf = g_file_get_parent (local_file);
			make_directory_with_parents (dirf, NULL, NULL);
			g_object_unref (dirf);

			g_file_copy_async (from, local_file, 0, 0, 
					   NULL, NULL, NULL, NULL, NULL);
		}

		g_object_unref (local_file);
		g_object_unref (from);
	}
}

static gboolean 
heuristic_albumart (const gchar *artist_,  
		    const gchar *album_, 
		    const gchar *tracks_str, 
		    const gchar *filename,
		    const gchar *local_uri,
		    gboolean    *copied)
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
	gchar *artist = NULL;
	gchar *album = NULL;

	/* Copy from local album art (.mediaartlocal) to spec */

	if (local_uri) {
	  GFile *local_file;

	  local_file = g_file_new_for_uri (local_uri);

	  if (g_file_query_exists (local_file, NULL)) {

		get_albumart_path (artist, album, 
				   "album", NULL, 
				   &target, NULL);

		file = g_file_new_for_path (target);

		g_file_copy_async (local_file, file, 0, 0, 
				   NULL, NULL, NULL, NULL, NULL);

		g_object_unref (file);
		g_object_unref (local_file);

		*copied = TRUE;
		g_free (target);

		return TRUE;
	  }
	  g_object_unref (local_file);
	}

	*copied = FALSE;

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

	if (artist_)
		artist = strip_characters (artist_);
	if (album_)
		album = strip_characters (album_);

	/* If amount of files and amount of tracks in the album somewhat match */

	if ((tracks != -1 && tracks < count + 3 && tracks > count - 3) || 
	    (tracks == -1 && count > 8 && count < 50)) {
		gchar *found = NULL;

		/* Try to find cover art in the directory */

		for (name = g_dir_read_name (dir); name; name = g_dir_read_name (dir)) {
			if ((artist && strcasestr (name, artist)) || 
			    (album && strcasestr (name, album))   || 
			    (strcasestr (name, "cover"))) {
				GError *error = NULL;
				
				if (g_str_has_suffix (name, "jpeg") || 
				    g_str_has_suffix (name, "jpg")) {
					GFile *file_found;
					
					if (!target) {
						get_albumart_path (artist, album, 
								   "album", NULL, 
								   &target, NULL);
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
							get_albumart_path (artist, 
									   album, 
									   "album", 
									   NULL, 
									   &target, 
									   NULL);
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

				if (retval)
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
	g_free (artist);
	g_free (album);

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

typedef struct {
	gchar *art_path, *local_uri;
} GetFileInfo;


static void
get_file_albumart_queue_cb (DBusGProxy     *proxy,
			    DBusGProxyCall *call,
			    gpointer	    user_data)
{
	GError      *error = NULL;
	guint        handle;
	GetFileInfo *info = user_data;

	dbus_g_proxy_end_call (proxy, call, &error,
			       G_TYPE_UINT, &handle,
			       G_TYPE_INVALID);

	if (g_file_test (info->art_path, G_FILE_TEST_EXISTS)) {

		gchar * asuri = g_filename_to_uri (info->art_path, NULL, NULL);
		tracker_thumbnailer_get_file_thumbnail (asuri, "image/jpeg");
		g_free (asuri);

		perhaps_copy_to_local (info->art_path, info->local_uri);
	}

	g_free (info->art_path);
	g_free (info->local_uri);

	g_slice_free (GetFileInfo, info);

	if (error) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}
}

static void
get_albumart_path (const gchar  *a, 
		   const gchar  *b, 
		   const gchar  *prefix, 
		   const gchar  *uri,
		   gchar       **path,
		   gchar       **local)
{
	gchar *art_filename;
	gchar *dir;
	gchar *down1, *down2;
	gchar *str1 = NULL, *str2 = NULL;
	gchar *f_a = NULL, *f_b = NULL;

	/* http://live.gnome.org/MediaArtStorageSpec */

	*path = NULL;

	if (!a && !b) {
		return;
	}

	if (!a) 
		f_a = g_strdup (" ");
	else
		f_a = strip_characters (a);

	if (!b)
		f_b = g_strdup (" ");
	else
		f_b = strip_characters (b);

	down1 = g_utf8_strdown (f_a, -1);
	down2 = g_utf8_strdown (f_b, -1);

	g_free (f_a);
	g_free (f_b);

	dir = g_build_filename (g_get_user_cache_dir (), "media-art", NULL);

	if (!g_file_test (dir, G_FILE_TEST_EXISTS)) {
		g_mkdir_with_parents (dir, 0770);
	}

	str1 = my_compute_checksum_for_data (G_CHECKSUM_MD5, (const guchar *) down1, strlen (down1));
	str2 = my_compute_checksum_for_data (G_CHECKSUM_MD5, (const guchar *) down2, strlen (down2));

	g_free (down1);
	g_free (down2);

	art_filename = g_strdup_printf ("%s-%s-%s.jpeg", prefix?prefix:"album", str1, str2);

	*path = g_build_filename (dir, art_filename, NULL);
	g_free (dir);
	g_free (art_filename);
	g_free (str1);
	g_free (str2);
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

	get_albumart_path (artist, album, "album", NULL, &filename, NULL);

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

	tracker_thumbnailer_get_file_thumbnail (filename, "image/jpeg");
	g_free (filename);

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
	gchar *local_uri = NULL;
	gchar *filename_uri;
	gboolean lcopied = FALSE;

	if (strchr (filename, ':'))
		filename_uri = g_strdup (filename);
	else
		filename_uri = g_filename_to_uri (filename, NULL, NULL);

	get_albumart_path (artist, album, "album", filename_uri, 
			   &art_path, &local_uri);

	if (!g_file_test (art_path, G_FILE_TEST_EXISTS)) {

#ifdef HAVE_GDKPIXBUF

		/* If we have embedded album art */

		if (buffer && len) {
			retval = set_albumart (buffer, len,
					       artist,
					       album,
					       filename);

			lcopied = !retval;

		} else {
#endif /* HAVE_GDK_PIXBUF */

			/* If not, we perform a heuristic on the dir */

			if (!heuristic_albumart (artist, album, trackercnt_str, filename, local_uri, &lcopied)) {
				GetFileInfo *info;

				/* If the heuristic failed, we request the download 
				 * of the media-art to the media-art downloaders */

				lcopied = TRUE;

				info = g_slice_new (GetFileInfo);

				info->local_uri = g_strdup (local_uri);
				info->art_path = g_strdup (art_path);

				dbus_g_proxy_begin_call (get_albumart_requester (),
					 "Queue",
					 get_file_albumart_queue_cb,
					 info, NULL,
					 G_TYPE_STRING, artist,
					 G_TYPE_STRING, album,
					 G_TYPE_STRING, "album",
					 G_TYPE_UINT, 0,
					 G_TYPE_INVALID);
			}
#ifdef HAVE_GDKPIXBUF

		}

#endif /* HAVE_GDKPIXBUF */

		/* If the heuristic didn't copy from the .mediaartlocal, then 
		 * we'll perhaps copy it to .mediaartlocal (perhaps because this
		 * only copies in case the media is located on a removable 
		 * device */

		if (g_file_test (art_path, G_FILE_TEST_EXISTS)) {
			gchar *asuri = g_filename_to_uri (art_path, NULL, NULL);
			tracker_thumbnailer_get_file_thumbnail (asuri, "image/jpeg");
			g_free (asuri);
		}

		if (!lcopied && g_file_test (art_path, G_FILE_TEST_EXISTS))
			perhaps_copy_to_local (art_path, local_uri);
	}

	g_free (art_path);
	g_free (filename_uri);
	g_free (local_uri);

	return retval;
}
