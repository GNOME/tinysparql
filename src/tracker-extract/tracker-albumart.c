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
 * License along with this library; if not, write to the
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
#include <sys/types.h>
#include <utime.h>
#include <time.h>
#include <errno.h>

#include <glib.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

#include <libtracker-miner/tracker-miner.h>
#include <libtracker-common/tracker-file-utils.h>
#include <libtracker-common/tracker-date-time.h>
#include <libtracker-common/tracker-albumart.h>

#include "tracker-albumart.h"
#include "tracker-extract.h"
#include "tracker-marshal.h"
#include "tracker-albumart-generic.h"

#define ALBUMARTER_SERVICE    "com.nokia.albumart"
#define ALBUMARTER_PATH       "/com/nokia/albumart/Requester"
#define ALBUMARTER_INTERFACE  "com.nokia.albumart.Requester"

static const gchar *media_art_type_name[TRACKER_MEDIA_ART_TYPE_COUNT] = {
	"invalid",
	"album",
	"video"
};

typedef struct {
	TrackerStorage *storage;
	gchar *art_path;
	gchar *local_uri;
} GetFileInfo;


static gboolean initialized;
static gboolean disable_requests;
static TrackerStorage *albumart_storage;
static GHashTable *albumart_cache;
static GDBusConnection *connection;

static void
albumart_queue_cb (GObject      *source_object,
                   GAsyncResult *res,
                   gpointer      user_data);


static gchar *
checksum_for_data (GChecksumType  checksum_type,
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

static gboolean
file_get_checksum_if_exists (GChecksumType   checksum_type,
                             const gchar    *path,
                             gchar         **md5,
                             gboolean        check_jpeg,
                             gboolean       *is_jpeg)
{
	GFile *file = g_file_new_for_path (path);
	GFileInputStream *stream;
	GChecksum *checksum;
	gboolean retval;

	checksum = g_checksum_new (checksum_type);

	if (!checksum) {
		g_debug ("Can't create checksum engine");
		g_object_unref (file);
		return FALSE;
	}

	stream = g_file_read (file, NULL, NULL);

	if (stream) {
		gssize rsize;
		guchar buffer[1024];

		/* File exists & readable always means true retval */
		retval = TRUE;

		if (check_jpeg) {
			if (g_input_stream_read_all (G_INPUT_STREAM (stream), buffer, 3, &rsize, NULL, NULL)) {
				if (rsize >= 3 && buffer[0] == 0xff && buffer[1] == 0xd8 && buffer[2] == 0xff) {
					if (is_jpeg) {
						*is_jpeg = TRUE;
					}
					/* Add the read bytes to the checksum */
					g_checksum_update (checksum, buffer, rsize);
				} else {
					/* Larger than 3 bytes but incorrect jpeg header */
					if (is_jpeg) {
						*is_jpeg = FALSE;
					}
					goto end;
				}
			} else {
				/* Smaller than 3 bytes, not a jpeg */
				if (is_jpeg) {
					*is_jpeg = FALSE;
				}
				goto end;
			}
		}

		while ((rsize = g_input_stream_read (G_INPUT_STREAM (stream), buffer, 1024, NULL, NULL)) > 0) {
			g_checksum_update (checksum, buffer, rsize);
		}

		if (md5) {
			*md5 = g_strdup (g_checksum_get_string (checksum));
		}

	} else {
		g_debug ("%s isn't readable while calculating MD5 checksum", path);
		/* File doesn't exist or isn't readable */
		retval = FALSE;
	}

end:

	if (stream) {
		g_object_unref (stream);
	}
	g_checksum_free (checksum);
	g_object_unref (file);

	return retval;
}

static gboolean
convert_from_other_format (const gchar *found,
                           const gchar *target,
                           const gchar *album_path,
                           const gchar *artist)
{
	gboolean retval;
	gchar *sum1 = NULL;
	gchar *target_temp;

	target_temp = g_strdup_printf ("%s-tmp", target);

	retval = tracker_albumart_file_to_jpeg (found, target_temp);

	if (retval && (artist == NULL || g_strcmp0 (artist, " ") == 0)) {
		if (g_rename (target_temp, album_path) == -1) {
			g_debug ("rename(%s, %s) error: %s", target_temp, album_path, g_strerror (errno));
		}
	} else if (retval && file_get_checksum_if_exists (G_CHECKSUM_MD5, target_temp, &sum1, FALSE, NULL)) {
		gchar *sum2 = NULL;
		if (file_get_checksum_if_exists (G_CHECKSUM_MD5, album_path, &sum2, FALSE, NULL)) {
			if (g_strcmp0 (sum1, sum2) == 0) {

				/* If album-space-md5.jpg is the same as found,
				 * make a symlink */

				if (symlink (album_path, target) != 0) {
					g_debug ("symlink(%s, %s) error: %s", album_path, target, g_strerror (errno));
					retval = FALSE;
				} else {
					retval = TRUE;
				}

				g_unlink (target_temp);

			} else {

				/* If album-space-md5.jpg isn't the same as found,
				 * make a new album-md5-md5.jpg (found -> target) */

				if (g_rename (target_temp, album_path) == -1) {
					g_debug ("rename(%s, %s) error: %s", target_temp, album_path, g_strerror (errno));
				}
			}
			g_free (sum2);
		} else {

			/* If there's not yet a album-space-md5.jpg, make one,
			 * and symlink album-md5-md5.jpg to it */

			g_rename (target_temp, album_path);

			if (symlink (album_path, target) != 0) {
				g_debug ("symlink(%s,%s) error: %s", album_path, target, g_strerror (errno));
				retval = FALSE;
			} else {
				retval = TRUE;
			}

		}

		g_free (sum1);
	} else if (retval) {
		g_debug ("Can't read %s while calculating checksum", target_temp);
		/* Can't read the file that it was converted to, strange ... */
		g_unlink (target_temp);
	}

	g_free (target_temp);

	return retval;
}

static gchar *
tracker_albumart_find_external (const gchar         *uri,
                                TrackerMediaArtType  type,
                                const gchar         *artist,
                                const gchar         *title)
{
	GFile *file, *dirf;
	GDir *dir;
	GError *error = NULL;
	gchar *dirname = NULL;
	const gchar *name;
	gint count;
	gchar *artist_strdown, *title_strdown;
	guint i;
	gchar *temp;
	gchar *art_file_path = NULL;

	g_return_val_if_fail (type > TRACKER_MEDIA_ART_NONE && type < TRACKER_MEDIA_ART_TYPE_COUNT, FALSE);
	g_return_val_if_fail (title != NULL, FALSE);

	file = g_file_new_for_uri (uri);
	dirf = g_file_get_parent (file);
	if (dirf) {
		dirname = g_file_get_path (dirf);
		g_object_unref (dirf);
	}
	g_object_unref (file);


	if (!dirname) {
		g_debug ("No media art directory found for '%s'", uri);
		return FALSE;
	}

	dir = g_dir_open (dirname, 0, &error);

	if (!dir) {
		g_debug ("Media art directory could not be opened:'%s', %s",
		         dirname,
		         error ? error->message : "no error given");

		g_clear_error (&error);
		g_free (dirname);

		return FALSE;
	}

	if (artist) {
		temp = tracker_albumart_strip_invalid_entities (artist);
		artist_strdown = g_utf8_strdown (temp, -1);
	}

	temp = tracker_albumart_strip_invalid_entities (title);
	title_strdown = g_utf8_strdown (temp, -1);

	for (i = 0; i < 2 && !art_file_path; i++) {
		/* We loop twice to find secondary choices, or just once as soon as a
		 * primary choice is found. When i != 0 it means that we're in the
		 * secondary choice's loop (current only switch is 'large' vs. 'small') */

		g_dir_rewind (dir);

		/* Try to find cover art in the directory */
		for (name = g_dir_read_name (dir), count = 0;
		     name != NULL && art_file_path == NULL && count < 50;
		     name = g_dir_read_name (dir), count++) {
			gchar *name_utf8, *name_strdown;

			name_utf8 = g_filename_to_utf8 (name, -1, NULL, NULL, NULL);

			if (!name_utf8) {
				g_debug ("Could not convert filename '%s' to UTF-8", name);
				continue;
			}

			name_strdown = g_utf8_strdown (name_utf8, -1);

			/* Accept cover, front, folder, AlbumArt_{GUID}_Large (first choice)
			 * second choice is AlbumArt_{GUID}_Small and AlbumArtSmall. We 
			 * don't support just AlbumArt. (it must have a Small or Large) */

			if ((artist_strdown && artist_strdown[0] != '\0' && strstr (name_strdown, artist_strdown)) ||
			    (title_strdown && title_strdown[0] != '\0' && strstr (name_strdown, title_strdown)) ||
			    (strstr (name_strdown, "cover")) ||
			    (strstr (name_strdown, "front")) ||
			    (strstr (name_strdown, "folder")) ||
			    ((strstr (name_strdown, "albumart") && strstr (name_strdown, i == 0 ? "large" : "small")))) {

				if (g_str_has_suffix (name_strdown, "jpeg") ||
				    g_str_has_suffix (name_strdown, "jpg") ||
				    g_str_has_suffix (name_strdown, "png")) {

					art_file_path = g_build_filename (dirname, name, NULL);
				}
			}

			g_free (name_utf8);
			g_free (name_strdown);
		}
	}

	if (count >= 50) {
		g_debug ("Album art NOT found in same directory (over 50 files found)");
	} else if (!art_file_path) {
		g_debug ("Album art NOT found in same directory");
	}

	g_free (artist_strdown);
	g_free (title_strdown);
	g_free (dirname);

	g_dir_close (dir);

	return art_file_path;
}

static gboolean
albumart_heuristic (const gchar         *artist,
                    const gchar         *title,
                    TrackerMediaArtType  type,
                    const gchar         *filename_uri,
                    const gchar         *local_uri)
{
	gchar *art_file_path = NULL;
	gchar *album_art_file_path = NULL;
	gchar *target = NULL;
	gchar *artist_stripped = NULL;
	gchar *title_stripped = NULL;
	gboolean retval = FALSE;

	if (title == NULL || title[0] == '\0') {
		g_debug ("Unable to fetch media art, no title specified");
		return FALSE;
	}

	if (artist) {
		artist_stripped = tracker_albumart_strip_invalid_entities (artist);
	}
	title_stripped = tracker_albumart_strip_invalid_entities (title);

	tracker_albumart_get_path (artist_stripped,
	                           title_stripped,
	                           media_art_type_name[type],
	                           NULL,
	                           &target,
	                           NULL);

	/* Copy from local album art (.mediaartlocal) to spec */
	if (local_uri) {
		GFile *local_file, *file;

		local_file = g_file_new_for_uri (local_uri);

		if (g_file_query_exists (local_file, NULL)) {
			g_debug ("Album art being copied from local (.mediaartlocal) file:'%s'",
			         local_uri);

			file = g_file_new_for_path (target);

			g_file_copy_async (local_file, file, 0, 0,
			                   NULL, NULL, NULL, NULL, NULL);

			g_object_unref (file);
			g_object_unref (local_file);

			g_free (target);
			g_free (artist_stripped);
			g_free (title_stripped);

			return TRUE;
		}

		g_object_unref (local_file);
	}

	art_file_path = tracker_albumart_find_external (filename_uri, type, artist, title);

	if (art_file_path != NULL) {
		if (g_str_has_suffix (art_file_path, "jpeg") ||
		    g_str_has_suffix (art_file_path, "jpg")) {

			gboolean is_jpeg = FALSE;
			gchar *sum1 = NULL;

			if (type != TRACKER_MEDIA_ART_ALBUM || (artist == NULL || g_strcmp0 (artist, " ") == 0)) {
				GFile *art_file;
				GFile *target_file;
				GError *err = NULL;

				g_debug ("Album art (JPEG) found in same directory being used:'%s'", art_file_path);

				target_file = g_file_new_for_path (target);
				art_file = g_file_new_for_path (art_file_path);

				g_file_copy (art_file, target_file, 0, NULL, NULL, NULL, &err);
				if (err) {
					g_debug ("%s", err->message);
					g_clear_error (&err);
				}
				g_object_unref (art_file);
				g_object_unref (target_file);
			} else if (file_get_checksum_if_exists (G_CHECKSUM_MD5, art_file_path, &sum1, TRUE, &is_jpeg)) {
				/* Avoid duplicate artwork for each track in an album */
				tracker_albumart_get_path (NULL,
				                           title_stripped,
				                           media_art_type_name [type],
				                           NULL,
				                           &album_art_file_path,
				                           NULL);

				if (is_jpeg) {
					gchar *sum2 = NULL;

					g_debug ("Album art (JPEG) found in same directory being used:'%s'", art_file_path);

					if (file_get_checksum_if_exists (G_CHECKSUM_MD5, album_art_file_path, &sum2, FALSE, NULL)) {
						if (g_strcmp0 (sum1, sum2) == 0) {
							/* If album-space-md5.jpg is the same as found,
							 * make a symlink */

							if (symlink (album_art_file_path, target) != 0) {
								g_debug ("symlink(%s, %s) error: %s", album_art_file_path, target, g_strerror (errno));
								retval = FALSE;
							} else {
								retval = TRUE;
							}
						} else {
							GFile *art_file;
							GFile *target_file;
							GError *err = NULL;

							/* If album-space-md5.jpg isn't the same as found,
							 * make a new album-md5-md5.jpg (found -> target) */

							target_file = g_file_new_for_path (target);
							art_file = g_file_new_for_path (art_file_path);
							retval = g_file_copy (art_file, target_file, 0, NULL, NULL, NULL, &err);
							if (err) {
								g_debug ("%s", err->message);
								g_clear_error (&err);
							}
							g_object_unref (art_file);
							g_object_unref (target_file);
						}
						g_free (sum2);
					} else {
						GFile *art_file;
						GFile *album_art_file;
						GError *err = NULL;

						/* If there's not yet a album-space-md5.jpg, make one,
						 * and symlink album-md5-md5.jpg to it */

						album_art_file = g_file_new_for_path (album_art_file_path);
						art_file = g_file_new_for_path (art_file_path);
						retval = g_file_copy (art_file, album_art_file, 0, NULL, NULL, NULL, &err);

						if (err == NULL) {
							if (symlink (album_art_file_path, target) != 0) {
								g_debug ("symlink(%s, %s) error: %s", album_art_file_path, target, g_strerror (errno));
								retval = FALSE;
							} else {
								retval = TRUE;
							}
						} else {
							g_debug ("%s", err->message);
							g_clear_error (&err);
							retval = FALSE;
						}

						g_object_unref (album_art_file);
						g_object_unref (art_file);
					}
				} else {
					g_debug ("Album art found in same directory but not a real JPEG file (trying to convert): '%s'", art_file_path);
					retval = convert_from_other_format (art_file_path, target, album_art_file_path, artist);
				}

				g_free (sum1);
			} else {
				/* Can't read contents of the cover.jpg file ... */
				retval = FALSE;
			}
		} else if (g_str_has_suffix (art_file_path, "png")) {
			if (!album_art_file_path) {
				tracker_albumart_get_path (NULL,
				                           title_stripped,
				                           media_art_type_name[type],
				                           NULL,
				                           &album_art_file_path,
				                           NULL);
			}

			g_debug ("Album art (PNG) found in same directory being used:'%s'", art_file_path);
			retval = convert_from_other_format (art_file_path, target, album_art_file_path, artist);
		}

		g_free (art_file_path);
		g_free (album_art_file_path);
	}

	g_free (target);
	g_free (artist_stripped);
	g_free (title_stripped);

	return retval;
}

static gboolean
albumart_set (const unsigned char *buffer,
              size_t               len,
              const gchar         *mime,
              TrackerMediaArtType  type,
              const gchar         *artist,
              const gchar         *title,
              const gchar         *uri)
{
	gchar *local_path;
	gboolean retval = FALSE;

	g_return_val_if_fail (type > TRACKER_MEDIA_ART_NONE && type < TRACKER_MEDIA_ART_TYPE_COUNT, FALSE);

	if (!artist && !title) {
		g_warning ("Could not save embedded album art, not enough metadata supplied");
		return FALSE;
	}

	tracker_albumart_get_path (artist, title, media_art_type_name[type], NULL, &local_path, NULL);

	if (type != TRACKER_MEDIA_ART_ALBUM || (artist == NULL || g_strcmp0 (artist, " ") == 0)) {
		retval = tracker_albumart_buffer_to_jpeg (buffer, len, mime, local_path);
	} else {
		gchar *album_path;

		tracker_albumart_get_path (NULL, title, media_art_type_name[type], NULL, &album_path, NULL);

		if (!g_file_test (album_path, G_FILE_TEST_EXISTS)) {
			retval = tracker_albumart_buffer_to_jpeg (buffer, len, mime, album_path);

			/* If album-space-md5.jpg doesn't exist, make one and make a symlink
			 * to album-md5-md5.jpg */

			if (retval && symlink (album_path, local_path) != 0) {
				g_debug ("symlink(%s, %s) error: %s", album_path, local_path, g_strerror (errno));
				retval = FALSE;
			} else {
				retval = TRUE;
			}
		} else {
			gchar *sum2 = NULL;
			
			if (file_get_checksum_if_exists (G_CHECKSUM_MD5, album_path, &sum2, FALSE, NULL)) {
				if ( !(g_strcmp0 (mime, "image/jpeg") == 0 || g_strcmp0 (mime, "JPG") == 0) ||
			       ( !(len > 2 && buffer[0] == 0xff && buffer[1] == 0xd8 && buffer[2] == 0xff) )) {
					gchar *sum1 = NULL;
					gchar *temp = g_strdup_printf ("%s-tmp", album_path);

					/* If buffer isn't a JPEG */

					retval = tracker_albumart_buffer_to_jpeg (buffer, len, mime, temp);

					if (retval && file_get_checksum_if_exists (G_CHECKSUM_MD5, temp, &sum1, FALSE, NULL)) {
						if (g_strcmp0 (sum1, sum2) == 0) {

							/* If album-space-md5.jpg is the same as buffer, make a symlink
							 * to album-md5-md5.jpg */

							g_unlink (temp);
							if (symlink (album_path, local_path) != 0) {
								g_debug ("symlink(%s, %s) error: %s", album_path, local_path, g_strerror (errno));
								retval = FALSE;
							} else {
								retval = TRUE;
							}
						} else {
							/* If album-space-md5.jpg isn't the same as buffer, make a
							 * new album-md5-md5.jpg */
							if (g_rename (temp, local_path) == -1) {
								g_debug ("rename(%s, %s) error: %s", temp, local_path, g_strerror (errno));
							}
						}
						g_free (sum1);
					} else {
						/* Can't read temp file ... */
						g_unlink (temp);
					}

					g_free (temp);
				} else {
					gchar *sum1 = NULL;

					sum1 = checksum_for_data (G_CHECKSUM_MD5, buffer, len);
					/* If album-space-md5.jpg is the same as buffer, make a symlink
				 	 * to album-md5-md5.jpg */

					if (g_strcmp0 (sum1, sum2) == 0) {
						if (symlink (album_path, local_path) != 0) {
							g_debug ("symlink(%s, %s) error: %s", album_path, local_path, g_strerror (errno));
							retval = FALSE;
						} else {
							retval = TRUE;
						}
					} else {
						/* If album-space-md5.jpg isn't the same as buffer, make a
						 * new album-md5-md5.jpg */
						retval = tracker_albumart_buffer_to_jpeg (buffer, len, mime, local_path);
					}
					g_free (sum1);
				}
				g_free (sum2);
			}
			g_free (album_path);
		}
	}

	g_free (local_path);

	return retval;
}

static void
albumart_request_download (TrackerStorage      *storage,
                           TrackerMediaArtType  type,
                           const gchar         *album,
                           const gchar         *artist,
                           const gchar         *local_uri,
                           const gchar         *art_path)
{
	if (connection) {
		GetFileInfo *info;

		if (disable_requests) {
			return;
		}

		if (type != TRACKER_MEDIA_ART_ALBUM) {
			return;
		}

		info = g_slice_new (GetFileInfo);

		info->storage = storage ? g_object_ref (storage) : NULL;

		info->local_uri = g_strdup (local_uri);
		info->art_path = g_strdup (art_path);

		g_dbus_connection_call (connection,
		                        ALBUMARTER_SERVICE,
		                        ALBUMARTER_PATH,
		                        ALBUMARTER_INTERFACE,
		                        "Queue",
		                        g_variant_new ("(sssu)",
		                                       artist ? artist : "",
		                                       album ? album : "",
		                                       "album",
		                                       0),
		                        NULL,
		                        G_DBUS_CALL_FLAGS_NONE,
		                        -1,
		                        NULL,
		                        albumart_queue_cb,
		                        info);
	}
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
			if (dirf) {
				/* Parent file may not exist, as if the file is in the
				 * root of a gvfs mount. In this case we won't try to
				 * create the parent directory, just try to copy the
				 * file there. */
				g_file_make_directory_with_parents (dirf, NULL, NULL);
				g_object_unref (dirf);
			}

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
albumart_queue_cb (GObject      *source_object,
                   GAsyncResult *res,
                   gpointer      user_data)
{
	GError *error = NULL;
	GetFileInfo *info;
	GVariant *v;

	info = user_data;

	v = g_dbus_connection_call_finish ((GDBusConnection *) source_object, res, &error);

	if (error) {
		if (error->code == G_DBUS_ERROR_SERVICE_UNKNOWN) {
			disable_requests = TRUE;
		} else {
			g_warning ("%s", error->message);
		}
		g_clear_error (&error);
	}

	if (v) {
		g_variant_unref (v);
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
	GError *error = NULL;

	g_return_val_if_fail (initialized == FALSE, FALSE);

	tracker_albumart_plugin_init ();

	albumart_storage = tracker_storage_new ();

	/* Cache to know if we have already handled uris */
	albumart_cache = g_hash_table_new_full (g_str_hash,
	                                        g_str_equal,
	                                        (GDestroyNotify) g_free,
	                                        NULL);

	/* Signal handler for new album art from the extractor */
	connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);

	if (!connection) {
		g_critical ("Could not connect to the D-Bus session bus, %s",
		            error ? error->message : "no error given.");
		g_clear_error (&error);
		return FALSE;
	}

	initialized = TRUE;

	return TRUE;
}

void
tracker_albumart_shutdown (void)
{
	g_return_if_fail (initialized == TRUE);

	if (connection) {
		g_object_unref (connection);
	}

	if (albumart_cache) {
		g_hash_table_unref (albumart_cache);
	}

	if (albumart_storage) {
		g_object_unref (albumart_storage);
	}

	tracker_albumart_plugin_shutdown ();

	initialized = FALSE;
}

static void
set_mtime (const gchar *filename, guint64 mtime)
{

	struct utimbuf buf;

	buf.actime = buf.modtime = mtime;
	utime (filename, &buf);
}

gboolean
tracker_albumart_process (const unsigned char *buffer,
                          size_t               len,
                          const gchar         *mime,
                          TrackerMediaArtType  type,
                          const gchar         *artist,
                          const gchar         *title,
                          const gchar         *uri)
{
	gchar *art_path;
	gchar *local_art_uri = NULL;
	gboolean processed = TRUE, a_exists, created = FALSE;
	guint64 mtime, a_mtime = 0;

	g_return_val_if_fail (type > TRACKER_MEDIA_ART_NONE && type < TRACKER_MEDIA_ART_TYPE_COUNT, FALSE);

	g_debug ("Processing media art: artist:'%s', title:'%s', type:'%s', uri:'%s'. Buffer is %ld bytes, mime:'%s'",
	         artist ? artist : "",
	         title ? title : "",
	         media_art_type_name[type],
	         uri,
	         (long int) len,
	         mime);

	/* TODO: We can definitely work with GFiles better here */

	mtime = tracker_file_get_mtime_uri (uri);

	tracker_albumart_get_path (artist,
	                           title,
	                           media_art_type_name[type],
	                           uri,
	                           &art_path,
	                           &local_art_uri);

	if (!art_path) {
		g_debug ("Album art path could not be obtained, not processing any further");

		g_free (local_art_uri);

		return FALSE;
	}

	a_exists = g_file_test (art_path, G_FILE_TEST_EXISTS);

	if (a_exists) {
		a_mtime = tracker_file_get_mtime (art_path);
	}

	if ((buffer && len > 0) && ((!a_exists) || (a_exists && mtime > a_mtime))) {
		processed = albumart_set (buffer, len, mime, type, artist, title, uri);
		set_mtime (art_path, mtime);
		created = TRUE;
	}

	if ((!created) && ((!a_exists) || (a_exists && mtime > a_mtime))) {
		/* If not, we perform a heuristic on the dir */
		gchar *key;
		gchar *dirname = NULL;
		GFile *file, *dirf;

		file = g_file_new_for_uri (uri);
		dirf = g_file_get_parent (file);
		if (dirf) {
			dirname = g_file_get_path (dirf);
			g_object_unref (dirf);
		}
		g_object_unref (file);

		key = g_strdup_printf ("%i-%s-%s-%s",
		                       type,
		                       artist ? artist : "",
		                       title ? title : "",
		                       dirname ? dirname : "");

		g_free (dirname);

		if (!g_hash_table_lookup (albumart_cache, key)) {
			if (!albumart_heuristic (artist,
			                         title,
			                         type,
			                         uri,
			                         local_art_uri)) {
				/* If the heuristic failed, we
				 * request the download the
				 * media-art to the media-art
				 * downloaders
				 */
				albumart_request_download (albumart_storage,
				                           type,
				                           artist,
				                           title,
				                           local_art_uri,
				                           art_path);
			}

			set_mtime (art_path, mtime);

			g_hash_table_insert (albumart_cache,
			                     key,
			                     GINT_TO_POINTER(TRUE));
		} else {
			g_free (key);
		}
	} else {
		if (!created) {
			g_debug ("Album art already exists for uri:'%s' as '%s'",
			         uri,
			         art_path);
		}
	}

	if (local_art_uri && !g_file_test (local_art_uri, G_FILE_TEST_EXISTS)) {
		/* We can't reuse art_exists here because the
		 * situation might have changed
		 */
		if (g_file_test (art_path, G_FILE_TEST_EXISTS)) {
			albumart_copy_to_local (albumart_storage,
			                        art_path,
			                        local_art_uri);
		}
	}

	g_free (art_path);
	g_free (local_art_uri);

	return processed;
}
