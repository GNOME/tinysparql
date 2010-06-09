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

#include <dbus/dbus-glib-bindings.h>

#include <libtracker-miner/tracker-miner.h>

#include "tracker-albumart.h"
#include "tracker-dbus.h"
#include "tracker-extract.h"
#include "tracker-marshal.h"
#include "tracker-albumart-generic.h"

#define ALBUMARTER_SERVICE    "com.nokia.albumart"
#define ALBUMARTER_PATH       "/com/nokia/albumart/Requester"
#define ALBUMARTER_INTERFACE  "com.nokia.albumart.Requester"

typedef struct {
	TrackerStorage *storage;
	gchar *art_path;
	gchar *local_uri;
} GetFileInfo;

static void albumart_queue_cb (DBusGProxy     *proxy,
                               DBusGProxyCall *call,
                               gpointer        user_data);

static gboolean initialized;
static gboolean disable_requests;
static TrackerStorage *albumart_storage;
static GHashTable *albumart_cache;
static DBusGProxy *albumart_proxy;

static gboolean
albumart_strip_find_next_block (const gchar    *original,
                                const gunichar  open_char,
                                const gunichar  close_char,
                                gint           *open_pos,
                                gint           *close_pos)
{
	const gchar *p1, *p2;

	if (open_pos) {
		*open_pos = -1;
	}

	if (close_pos) {
		*close_pos = -1;
	}

	p1 = g_utf8_strchr (original, -1, open_char);
	if (p1) {
		if (open_pos) {
			*open_pos = p1 - original;
		}

		p2 = g_utf8_strchr (g_utf8_next_char (p1), -1, close_char);
		if (p2) {
			if (close_pos) {
				*close_pos = p2 - original;
			}

			return TRUE;
		}
	}

	return FALSE;
}

static gchar *
albumart_strip_invalid_entities (const gchar *original)
{
	GString         *str_no_blocks;
	gchar          **strv;
	gchar           *str;
	gboolean         blocks_done = FALSE;
	const gchar     *p;
	const gchar     *invalid_chars = "()[]<>{}_!@#$^&*+=|\\/\"'?~";
	const gchar     *invalid_chars_delimiter = "*";
	const gchar     *convert_chars = "\t";
	const gchar     *convert_chars_delimiter = " ";
	const gunichar   blocks[5][2] = {
		{ '(', ')' },
		{ '{', '}' },
		{ '[', ']' },
		{ '<', '>' },
		{  0,   0  }
	};

	str_no_blocks = g_string_new ("");

	p = original;

	while (!blocks_done) {
		gint pos1, pos2, i;

		pos1 = -1;
		pos2 = -1;

		for (i = 0; blocks[i][0] != 0; i++) {
			gint start, end;

			/* Go through blocks, find the earliest block we can */
			if (albumart_strip_find_next_block (p, blocks[i][0], blocks[i][1], &start, &end)) {
				if (pos1 == -1 || start < pos1) {
					pos1 = start;
					pos2 = end;
				}
			}
		}

		/* If either are -1 we didn't find any */
		if (pos1 == -1) {
			/* This means no blocks were found */
			g_string_append (str_no_blocks, p);
			blocks_done = TRUE;
		} else {
			/* Append the test BEFORE the block */
			if (pos1 > 0) {
				g_string_append_len (str_no_blocks, p, pos1);
			}

			p = g_utf8_next_char (p + pos2);

			/* Do same again for position AFTER block */
			if (*p == '\0') {
				blocks_done = TRUE;
			}
		}
	}

	/* Now convert chars to lower case */
	str = g_utf8_strdown (str_no_blocks->str, -1);
	g_string_free (str_no_blocks, TRUE);

	/* Now strip invalid chars */
	g_strdelimit (str, invalid_chars, *invalid_chars_delimiter);
	strv = g_strsplit (str, invalid_chars_delimiter, -1);
	g_free (str);
	str = g_strjoinv (NULL, strv);
	g_strfreev (strv);

	/* Now convert chars */
	g_strdelimit (str, convert_chars, *convert_chars_delimiter);
	strv = g_strsplit (str, convert_chars_delimiter, -1);
	g_free (str);
	str = g_strjoinv (convert_chars_delimiter, strv);
	g_strfreev (strv);

	/* Now remove double spaces */
	strv = g_strsplit (str, "  ", -1);
	g_free (str);
	str = g_strjoinv (" ", strv);
	g_strfreev (strv);

	/* Now strip leading/trailing white space */
	g_strstrip (str);

	return str;
}

static gchar *
albumart_checksum_for_data (GChecksumType  checksum_type,
                            const guchar  *data,
                            gsize          length)
{
	GChecksum *checksum;
	gchar *retval;

	checksum = g_checksum_new (checksum_type);
	if (!checksum) {
		return NULL;
	}

	g_checksum_update (checksum, data, length);
	retval = g_strdup (g_checksum_get_string (checksum));
	g_checksum_free (checksum);

	return retval;
}

static void
albumart_get_path (const gchar  *artist,
                   const gchar  *album,
                   const gchar  *prefix,
                   const gchar  *uri,
                   gchar       **path,
                   gchar       **local_uri)
{
	gchar *art_filename;
	gchar *dir;
	gchar *artist_down, *album_down;
	gchar *artist_stripped, *album_stripped;
	gchar *artist_checksum, *album_checksum;

	/* http://live.gnome.org/MediaArtStorageSpec */

	if (path) {
		*path = NULL;
	}

	if (local_uri) {
		*local_uri = NULL;
	}

	if (!artist && !album) {
		return;
	}

	if (!artist) {
		artist_stripped = g_strdup (" ");
	} else {
		artist_stripped = albumart_strip_invalid_entities (artist);
	}

	if (!album) {
		album_stripped = g_strdup (" ");
	} else {
		album_stripped = albumart_strip_invalid_entities (album);
	}

	artist_down = g_utf8_strdown (artist_stripped, -1);
	album_down = g_utf8_strdown (album_stripped, -1);

	g_free (artist_stripped);
	g_free (album_stripped);

	dir = g_build_filename (g_get_user_cache_dir (),
	                        "media-art",
	                        NULL);

	if (!g_file_test (dir, G_FILE_TEST_EXISTS)) {
		g_mkdir_with_parents (dir, 0770);
	}

	artist_checksum = albumart_checksum_for_data (G_CHECKSUM_MD5,
	                                              (const guchar *) artist_down,
	                                              strlen (artist_down));
	album_checksum = albumart_checksum_for_data (G_CHECKSUM_MD5,
	                                             (const guchar *) album_down,
	                                             strlen (album_down));

	g_free (artist_down);
	g_free (album_down);

	art_filename = g_strdup_printf ("%s-%s-%s.jpeg",
	                                prefix ? prefix : "album",
	                                artist_checksum,
	                                album_checksum);

	if (path) {
		*path = g_build_filename (dir, art_filename, NULL);
	}

	if (local_uri) {
		gchar *local_dir;
		GFile *file, *parent;

		if (strstr (uri, "://")) {
			file = g_file_new_for_uri (uri);
		} else {
			file = g_file_new_for_path (uri);
		}

		parent = g_file_get_parent (file);
		local_dir = g_file_get_uri (parent);

		/* This is a URI, don't use g_build_filename here */
		*local_uri = g_strdup_printf ("%s/.mediaartlocal/%s", local_dir, art_filename);

		g_free (local_dir);
		g_object_unref (file);
		g_object_unref (parent);
	}

	g_free (dir);
	g_free (art_filename);
	g_free (artist_checksum);
	g_free (album_checksum);
}

static gboolean
albumart_heuristic (const gchar *artist,
                    const gchar *album,
                    const gchar *filename_uri,
                    const gchar *local_uri,
                    gboolean    *copied)
{
	GFile *file, *dirf;
	GDir *dir;
	GError *error = NULL;
	gchar *target = NULL;
	gchar *dirname;
	const gchar *name;
	gboolean retval;
	gint count;
	gchar *artist_stripped = NULL;
	gchar *album_stripped = NULL;
	gchar *artist_strdown, *album_strdown;

	if (copied) {
		*copied = FALSE;
	}

	if (artist) {
		artist_stripped = albumart_strip_invalid_entities (artist);
	}

	if (album) {
		album_stripped = albumart_strip_invalid_entities (album);
	}

	/* Copy from local album art (.mediaartlocal) to spec */
	if (local_uri) {
		GFile *local_file;

		local_file = g_file_new_for_uri (local_uri);

		if (g_file_query_exists (local_file, NULL)) {
			g_debug ("Album art being copied from local (.mediaartlocal) file:'%s'",
			         local_uri);

			albumart_get_path (artist_stripped,
			                   album_stripped,
			                   "album", NULL,
			                   &target, NULL);
			if (target) {
				file = g_file_new_for_path (target);

				g_file_copy_async (local_file, file, 0, 0,
				                   NULL, NULL, NULL, NULL, NULL);

				g_object_unref (file);
			}
			g_object_unref (local_file);

			if (copied) {
				*copied = TRUE;
			}

			g_free (target);
			g_free (artist_stripped);
			g_free (album_stripped);

			return TRUE;
		}

		g_object_unref (local_file);
	}

	file = g_file_new_for_uri (filename_uri);
	dirf = g_file_get_parent (file);
	dirname = g_file_get_path (dirf);
	g_object_unref (file);
	g_object_unref (dirf);

	if (!dirname) {
		g_debug ("Album art directory could not be used:'%s'", dirname);

		g_free (artist_stripped);
		g_free (album_stripped);

		return FALSE;
	}

	dir = g_dir_open (dirname, 0, &error);

	if (!dir) {
		g_debug ("Album art directory could not be opened:'%s', %s",
		         dirname,
		         error ? error->message : "no error given");

		g_clear_error (&error);
		g_free (artist_stripped);
		g_free (album_stripped);
		g_free (dirname);

		return FALSE;
	}

	file = NULL;
	artist_strdown = artist_stripped ? g_utf8_strdown (artist_stripped, -1) : g_strdup ("");
	album_strdown = album_stripped ? g_utf8_strdown (album_stripped, -1) : g_strdup ("");

	/* Try to find cover art in the directory */
	for (name = g_dir_read_name (dir), count = 0, retval = FALSE;
	     name != NULL && !retval && count < 50;
	     name = g_dir_read_name (dir), count++) {
		gchar *name_utf8, *name_strdown;

		name_utf8 = g_filename_to_utf8 (name, -1, NULL, NULL, NULL);

		if (!name_utf8) {
			g_debug ("Could not convert filename '%s' to UTF-8", name);
			continue;
		}

		name_strdown = g_utf8_strdown (name_utf8, -1);

		/* Accept cover, front, folder, AlbumArt_{GUID}_Large
		 * reject AlbumArt_{GUID}_Small and AlbumArtSmall
		 */
		if ((artist_strdown && artist_strdown[0] != '\0' && strstr (name_strdown, artist_strdown)) ||
		    (album_strdown && album_strdown[0] != '\0' && strstr (name_strdown, album_strdown)) ||
		    (strstr (name_strdown, "cover")) ||
		    (strstr (name_strdown, "front")) ||
		    (strstr (name_strdown, "folder")) ||
		    ((strstr (name_strdown, "albumart") && strstr (name_strdown, "large")))) {
			if (g_str_has_suffix (name_strdown, "jpeg") ||
			    g_str_has_suffix (name_strdown, "jpg")) {
				if (!target) {
					albumart_get_path (artist_stripped,
					                   album_stripped,
					                   "album",
					                   NULL,
					                   &target,
					                   NULL);
				}

				if (!file && target) {
					file = g_file_new_for_path (target);
				}

				if (file) {
					GFile *found_file;
					gchar *found;

					found = g_build_filename (dirname, name, NULL);
					g_debug ("Album art (JPEG) found in same directory being used:'%s'", found);

					found_file = g_file_new_for_path (found);
					g_free (found);

					g_file_copy (found_file, file, 0, NULL, NULL, NULL, &error);
					g_object_unref (found_file);

					retval = error != NULL;
					g_clear_error (&error);
				}
			} else if (g_str_has_suffix (name_strdown, "png")) {
				gchar *found;

				if (!target) {
					albumart_get_path (artist_stripped,
					                   album_stripped,
					                   "album",
					                   NULL,
					                   &target,
					                   NULL);
				}

				found = g_build_filename (dirname, name, NULL);
				g_debug ("Album art (PNG) found in same directory being used:'%s'", found);
				retval = tracker_albumart_file_to_jpeg (found, target);
				g_free (found);
			}
		}

		g_free (name_utf8);
		g_free (name_strdown);
	}

	if (count >= 50) {
		g_debug ("Album art NOT found in same directory (over 50 files found)");
	} else if (!retval) {
		g_debug ("Album art NOT found in same directory");
	}

	g_free (artist_strdown);
	g_free (album_strdown);

	g_dir_close (dir);

	if (file) {
		g_object_unref (file);
	}

	g_free (target);
	g_free (dirname);
	g_free (artist_stripped);
	g_free (album_stripped);

	return retval;
}

static gboolean
albumart_set (const unsigned char *buffer,
              size_t               len,
              const gchar         *mime,
              const gchar         *artist,
              const gchar         *album,
              const gchar         *uri)
{
	gchar *local_path;
	gboolean retval;

	if (!artist && !album) {
		g_warning ("Could not save embedded album art, no artist or album supplied");
		return FALSE;
	}

	albumart_get_path (artist, album, "album", NULL, &local_path, NULL);

	retval = tracker_albumart_buffer_to_jpeg (buffer, len, mime, local_path);

	g_free (local_path);

	return retval;
}

static void
albumart_request_download (TrackerStorage *storage,
                           const gchar    *album,
                           const gchar    *artist,
                           const gchar    *local_uri,
                           const gchar    *art_path)
{
	GetFileInfo *info;

	if (disable_requests) {
		return;
	}

	info = g_slice_new (GetFileInfo);

	info->storage = storage ? g_object_ref (storage) : NULL;

	info->local_uri = g_strdup (local_uri);
	info->art_path = g_strdup (art_path);

	if (!albumart_proxy) {
		GError          *error = NULL;
		DBusGConnection *connection;

		connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

		if (!error) {
			albumart_proxy = dbus_g_proxy_new_for_name (connection,
			                                            ALBUMARTER_SERVICE,
			                                            ALBUMARTER_PATH,
			                                            ALBUMARTER_INTERFACE);
		} else {
			g_error_free (error);
		}
	}

	dbus_g_proxy_begin_call (albumart_proxy,
	                         "Queue",
	                         albumart_queue_cb,
	                         info,
	                         NULL,
	                         G_TYPE_STRING, artist,
	                         G_TYPE_STRING, album,
	                         G_TYPE_STRING, "album",
	                         G_TYPE_UINT, 0,
	                         G_TYPE_INVALID);
}

static void
albumart_copy_to_local (TrackerStorage *storage,
                        const gchar    *filename,
                        const gchar    *local_uri)
{
	GSList *roots, *l;
	gboolean on_removable_device = FALSE;
	guint flen;

	/* Determining if we are on a removable device */
	if (!storage) {
		/* This is usually because we are running on the
		 * command line, so we don't error here with
		 * g_return_if_fail().
		 */
		return;
	}

	roots = tracker_storage_get_device_roots (storage, TRACKER_STORAGE_REMOVABLE, FALSE);
	flen = strlen (filename);

	for (l = roots; l; l = l->next) {
		guint len;

		len = strlen (l->data);

		if (flen >= len && strncmp (filename, l->data, len)) {
			on_removable_device = TRUE;
			break;
		}
	}

	g_slist_foreach (roots, (GFunc) g_free, NULL);
	g_slist_free (roots);

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
			g_file_make_directory_with_parents (dirf, NULL, NULL);
			g_object_unref (dirf);

			g_debug ("Copying album art from:'%s' to:'%s'",
			         filename, local_uri);

			g_file_copy_async (from, local_file, 0, 0,
			                   NULL, NULL, NULL, NULL, NULL);
		}

		g_object_unref (local_file);
		g_object_unref (from);
	}
}

static void
albumart_queue_cb (DBusGProxy     *proxy,
                   DBusGProxyCall *call,
                   gpointer        user_data)
{
	GError      *error = NULL;
	guint        handle;
	GetFileInfo *info;

	info = user_data;

	dbus_g_proxy_end_call (proxy, call, &error,
	                       G_TYPE_UINT, &handle,
	                       G_TYPE_INVALID);

	if (error) {
		if (error->code == DBUS_GERROR_SERVICE_UNKNOWN) {
			disable_requests = TRUE;
		} else {
			g_warning ("%s", error->message);
		}

		g_clear_error (&error);
	}

	if (info->storage && info->art_path &&
	    g_file_test (info->art_path, G_FILE_TEST_EXISTS)) {

		albumart_copy_to_local (info->storage,
		                        info->art_path,
		                        info->local_uri);
	}

	g_free (info->art_path);
	g_free (info->local_uri);

	if (info->storage) {
		g_object_unref (info->storage);
	}

	g_slice_free (GetFileInfo, info);
}

gboolean
tracker_albumart_init (void)
{
	DBusGConnection *connection;
	GError *error = NULL;

	g_return_val_if_fail (initialized == FALSE, FALSE);

	albumart_storage = tracker_storage_new ();

	/* Cache to know if we have already handled uris */
	albumart_cache = g_hash_table_new_full (g_str_hash,
	                                        g_str_equal,
	                                        (GDestroyNotify) g_free,
	                                        NULL);

	/* Signal handler for new album art from the extractor */
	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

	if (!connection) {
		g_critical ("Could not connect to the D-Bus session bus, %s",
		            error ? error->message : "no error given.");
		g_clear_error (&error);
		return FALSE;
	}

	/* Get album art downloader proxy */
	albumart_proxy = dbus_g_proxy_new_for_name (connection,
	                                            ALBUMARTER_SERVICE,
	                                            ALBUMARTER_PATH,
	                                            ALBUMARTER_INTERFACE);

	initialized = TRUE;

	return TRUE;
}

void
tracker_albumart_shutdown (void)
{
	g_return_if_fail (initialized == TRUE);

	if (albumart_proxy) {
		g_object_unref (albumart_proxy);
	}

	if (albumart_cache) {
		g_hash_table_unref (albumart_cache);
	}

	if (albumart_storage) {
		g_object_unref (albumart_storage);
	}

	initialized = FALSE;
}

gboolean
tracker_albumart_process (const unsigned char *buffer,
                          size_t               len,
                          const gchar         *mime,
                          const gchar         *artist,
                          const gchar         *album,
                          const gchar         *filename)
{
	gchar *art_path;
	gboolean processed = TRUE;
	gchar *local_uri = NULL;
	gchar *filename_uri;

	g_debug ("Processing album art, buffer is %ld bytes, artist:'%s', album:'%s', filename:'%s', mime:'%s'",
	         (long int) len,
	         artist ? artist : "",
	         album ? album : "",
	         filename,
	         mime);

	if (strstr (filename, "://")) {
		filename_uri = g_strdup (filename);
	} else {
		filename_uri = g_filename_to_uri (filename, NULL, NULL);
	}

	albumart_get_path (artist,
	                   album,
	                   "album",
	                   filename_uri,
	                   &art_path,
	                   &local_uri);

	if (!art_path) {
		g_debug ("Album art path could not be obtained, not processing any further");

		g_free (filename_uri);
		g_free (local_uri);

		return FALSE;
	}

	if (!g_file_test (art_path, G_FILE_TEST_EXISTS)) {
		/* If we have embedded album art */
		if (buffer && len > 0) {
			processed = albumart_set (buffer,
			                          len,
			                          mime,
			                          artist,
			                          album,
			                          filename_uri);
		} else {
			/* If not, we perform a heuristic on the dir */
			gchar *key;
			gchar *dirname;
			GFile *file, *dirf;

			file = g_file_new_for_uri (filename_uri);
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
				if (!albumart_heuristic (artist,
				                         album,
				                         filename_uri,
				                         local_uri,
				                         NULL)) {
					/* If the heuristic failed, we
					 * request the download the
					 * media-art to the media-art
					 * downloaders
					 */
					albumart_request_download (albumart_storage,
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
		}

	} else {
		g_debug ("Album art already exists for uri:'%s' as '%s'",
		         filename_uri,
		         art_path);
	}

	if (local_uri && !g_file_test (local_uri, G_FILE_TEST_EXISTS)) {
		/* We can't reuse art_exists here because the
		 * situation might have changed
		 */
		if (g_file_test (art_path, G_FILE_TEST_EXISTS)) {
			albumart_copy_to_local (albumart_storage,
			                        art_path,
			                        local_uri);
		}
	}

	g_free (art_path);
	g_free (filename_uri);
	g_free (local_uri);

	return processed;
}
