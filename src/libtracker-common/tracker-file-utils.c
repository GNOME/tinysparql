/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/file.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>

#include <glib.h>
#include <gio/gio.h>

#include "tracker-log.h"
#include "tracker-os-dependant.h"
#include "tracker-file-utils.h"
#include "tracker-type-utils.h"

#define TEXT_SNIFF_SIZE 4096

static GHashTable *file_locks = NULL;

FILE *
tracker_file_open (const gchar *uri,
                   const gchar *how,
                   gboolean     sequential)
{
	FILE     *file;
	gboolean  readonly;
	int       flags;

	g_return_val_if_fail (uri != NULL, NULL);
	g_return_val_if_fail (how != NULL, NULL);

	file = fopen (uri, how);
	if (!file) {
		return NULL;
	}

	/* Are we opening for readonly? */
	readonly = !strstr (uri, "r+") && strchr (uri, 'r');

	if (readonly) {
		int fd;

		fd = fileno (file);

#if defined(__linux__)
		/* Make sure we set the NOATIME flag if we have permissions to */
		if ((flags = fcntl (fd, F_GETFL, 0)) != -1) {
			fcntl (fd, F_SETFL, flags | O_NOATIME);
		}
#endif
	}

	return file;
}

void
tracker_file_close (FILE     *file,
                    gboolean  need_again_soon)
{
	g_return_if_fail (file != NULL);

#ifdef HAVE_POSIX_FADVISE
	if (!need_again_soon) {
		posix_fadvise (fileno (file), 0, 0, POSIX_FADV_DONTNEED);
	}
#endif /* HAVE_POSIX_FADVISE */

	fclose (file);
}

goffset
tracker_file_get_size (const gchar *path)
{
	GFileInfo *info;
	GFile     *file;
	GError    *error = NULL;
	goffset    size;

	g_return_val_if_fail (path != NULL, 0);

	file = g_file_new_for_path (path);
	info = g_file_query_info (file,
	                          G_FILE_ATTRIBUTE_STANDARD_SIZE,
	                          G_FILE_QUERY_INFO_NONE,
	                          NULL,
	                          &error);

	if (G_UNLIKELY (error)) {
		g_message ("Could not get size for '%s', %s",
		           path,
		           error->message);
		g_error_free (error);
		size = 0;
	} else {
		size = g_file_info_get_size (info);
		g_object_unref (info);
	}

	g_object_unref (file);

	return size;
}

guint64
tracker_file_get_mtime (const gchar *path)
{
	GFileInfo *info;
	GFile     *file;
	GError    *error = NULL;
	guint64    mtime;

	g_return_val_if_fail (path != NULL, 0);

	file = g_file_new_for_path (path);
	info = g_file_query_info (file,
	                          G_FILE_ATTRIBUTE_TIME_MODIFIED,
	                          G_FILE_QUERY_INFO_NONE,
	                          NULL,
	                          &error);

	if (G_UNLIKELY (error)) {
		g_message ("Could not get mtime for '%s', %s",
		           path,
		           error->message);
		g_error_free (error);
		mtime = 0;
	} else {
		mtime = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
		g_object_unref (info);
	}

	g_object_unref (file);

	return mtime;
}

gchar *
tracker_file_get_mime_type (GFile *file)
{
	GFileInfo *info;
	GError    *error = NULL;
	gchar     *content_type;

	g_return_val_if_fail (G_IS_FILE (file), NULL);

	info = g_file_query_info (file,
	                          G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
	                          G_FILE_QUERY_INFO_NONE,
	                          NULL,
	                          &error);

	if (G_UNLIKELY (error)) {
		g_message ("Could not guess mimetype, %s",
		           error->message);
		g_error_free (error);
		content_type = NULL;
	} else {
		content_type = g_strdup (g_file_info_get_content_type (info));
		g_object_unref (info);
	}

	return content_type ? content_type : g_strdup ("unknown");
}

gboolean
tracker_file_system_has_enough_space (const gchar *path,
                                      gulong       required_bytes,
                                      gboolean     creating_db)
{
	struct statvfs st;
	gchar *str1;
	gchar *str2;
	gboolean enough;

	g_return_val_if_fail (path != NULL, FALSE);

	if (statvfs (path, &st) == -1) {
		g_critical ("Could not statvfs() '%s'", path);
		return FALSE;
	}

	enough = ((long long) st.f_bsize * st.f_bavail) >= required_bytes;

	if (creating_db) {
		str1 = g_format_size_for_display (required_bytes);
		str2 = g_format_size_for_display (st.f_bsize * st.f_bavail);

		if (!enough) {
			g_critical ("Not enough disk space to create databases, "
				    "%s remaining, %s required as a minimum",
				    str2,
				    str1);
		} else {
			g_message ("Checking for adequate disk space to create databases, "
				   "%s remaining, %s required as a minimum",
				   str2,
				   str1);
		}

		g_free (str2);
		g_free (str1);
	}

	return enough;
}

gboolean
tracker_path_is_in_path (const gchar *path,
                         const gchar *in_path)
{
	gchar    *new_path;
	gchar    *new_in_path;
	gboolean  is_in_path = FALSE;

	g_return_val_if_fail (path != NULL, FALSE);
	g_return_val_if_fail (in_path != NULL, FALSE);

	if (!g_str_has_suffix (path, G_DIR_SEPARATOR_S)) {
		new_path = g_strconcat (path, G_DIR_SEPARATOR_S, NULL);
	} else {
		new_path = g_strdup (path);
	}

	if (!g_str_has_suffix (in_path, G_DIR_SEPARATOR_S)) {
		new_in_path = g_strconcat (in_path, G_DIR_SEPARATOR_S, NULL);
	} else {
		new_in_path = g_strdup (in_path);
	}

	if (g_str_has_prefix (new_path, new_in_path)) {
		is_in_path = TRUE;
	}

	g_free (new_in_path);
	g_free (new_path);

	return is_in_path;
}

GSList *
tracker_path_list_filter_duplicates (GSList      *roots,
                                     const gchar *basename_exception_prefix,
                                     gboolean     is_recursive)
{
	GSList *l1, *l2;
	GSList *new_list;

	new_list = tracker_gslist_copy_with_string_data (roots);
	l1 = new_list;

	while (l1) {
		const gchar *path;
		gchar       *p;
		gboolean     reset = FALSE;

		path = l1->data;

		l2 = new_list;

		while (l2 && !reset) {
			const gchar *in_path;

			in_path = l2->data;

			if (path == in_path) {
				/* Do nothing */
				l2 = l2->next;
				continue;
			}

			if (basename_exception_prefix) {
				gchar *lbasename;
				gboolean has_prefix = FALSE;

				lbasename = g_path_get_basename (path);
				if (!g_str_has_prefix (lbasename, basename_exception_prefix)) {
					g_free (lbasename);

					lbasename = g_path_get_basename (in_path);
					if (g_str_has_prefix (lbasename, basename_exception_prefix)) {
						has_prefix = TRUE;
					}
				} else {
					has_prefix = TRUE;
				}

				g_free (lbasename);

				/* This is so we can ignore this check
				 * on files which prefix with ".".
				 */
				if (has_prefix) {
					l2 = l2->next;
					continue;
				}
			}

			if (is_recursive && tracker_path_is_in_path (path, in_path)) {
				g_debug ("Removing path:'%s', it is in path:'%s'",
				         path, in_path);

				g_free (l1->data);
				new_list = g_slist_delete_link (new_list, l1);
				l1 = new_list;

				reset = TRUE;

				continue;
			}
			else if (is_recursive && tracker_path_is_in_path (in_path, path)) {
				g_debug ("Removing path:'%s', it is in path:'%s'",
				         in_path, path);

				g_free (l2->data);
				new_list = g_slist_delete_link (new_list, l2);
				l1 = new_list;

				reset = TRUE;

				continue;
			}

			l2 = l2->next;
		}

		if (G_LIKELY (!reset)) {
			p = strrchr (path, G_DIR_SEPARATOR);

			/* Make sure the path doesn't have the '/' suffix. */
			if (p && !p[1]) {
				*p = '\0';
			}

			l1 = l1->next;
		}
	}

#ifdef TESTING
	g_debug ("GSList paths were filtered down to:");

	if (TRUE) {
		GSList *l;

		for (l = new_list; l; l = l->next) {
			g_debug ("  %s", (gchar*) l->data);
		}
	}
#endif /* TESTING */

	return new_list;
}

gchar *
tracker_path_evaluate_name (const gchar *path)
{
	gchar        *final_path;
	gchar       **tokens;
	gchar       **token;
	gchar        *start;
	gchar        *end;
	const gchar  *env;
	gchar        *expanded;

	if (!path || path[0] == '\0') {
		return NULL;
	}

	/* First check the simple case of using tilder */
	if (path[0] == '~') {
		const gchar *home;

		home = g_getenv ("HOME");
		if (! home) {
			home = g_get_home_dir ();
		}

		if (!home || home[0] == '\0') {
			return NULL;
		}

		return g_build_path (G_DIR_SEPARATOR_S,
		                     home,
		                     path + 1,
		                     NULL);
	}

	/* Second try to find any environment variables and expand
	 * them, like $HOME or ${FOO}
	 */
	tokens = g_strsplit (path, G_DIR_SEPARATOR_S, -1);

	for (token = tokens; *token; token++) {
		if (**token != '$') {
			continue;
		}

		start = *token + 1;

		if (*start == '{') {
			start++;
			end = start + (strlen (start)) - 1;
			*end='\0';
		}

		env = g_getenv (start);
		g_free (*token);

		/* Don't do g_strdup (s?s1:s2) as that doesn't work
		 * with certain gcc 2.96 versions.
		 */
		*token = env ? g_strdup (env) : g_strdup ("");
	}

	/* Third get the real path removing any "../" and other
	 * symbolic links to other places, returning only the REAL
	 * location.
	 */
	if (tokens) {
		expanded = g_strjoinv (G_DIR_SEPARATOR_S, tokens);
		g_strfreev (tokens);
	} else {
		expanded = g_strdup (path);
	}

	/* Only resolve relative paths if there is a directory
	 * separator in the path, otherwise it is just a name.
	 */
	if (strchr (expanded, G_DIR_SEPARATOR)) {
		GFile *file;

		file = g_file_new_for_commandline_arg (expanded);
		final_path = g_file_get_path (file);
		g_object_unref (file);
		g_free (expanded);
	} else {
		final_path = expanded;
	}

	return final_path;
}

static gboolean
path_has_write_access (const gchar *path,
                       gboolean    *exists)
{
	GFile     *file;
	GFileInfo *info;
	GError    *error = NULL;
	gboolean   writable;

	g_return_val_if_fail (path != NULL, FALSE);
	g_return_val_if_fail (path[0] != '\0', FALSE);

	file = g_file_new_for_path (path);
	info = g_file_query_info (file,
	                          G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE,
	                          0,
	                          NULL,
	                          &error);
	g_object_unref (file);

	if (G_UNLIKELY (error)) {
		if (error->code == G_IO_ERROR_NOT_FOUND) {
			if (exists) {
				*exists = FALSE;
			}
		} else {
			g_warning ("Could not check if we have write access for "
			           "path '%s', %s",
			           path,
			           error->message);
		}

		g_error_free (error);

		writable = FALSE;
	} else {
		if (exists) {
			*exists = TRUE;
		}

		writable = g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE);

		g_object_unref (info);
	}

	return writable;
}

gboolean
tracker_path_has_write_access_or_was_created (const gchar *path)
{
	gboolean writable;
	gboolean exists = FALSE;

	writable = path_has_write_access (path, &exists);
	if (exists) {
		if (writable) {
			g_message ("  Path is OK");
			return TRUE;
		}

		g_message ("  Path can not be written to");
	} else {
		g_message ("  Path does not exist, attempting to create...");

		if (g_mkdir_with_parents (path, 0700) == 0) {
			g_message ("  Path was created");
			return TRUE;
		}

		g_message ("  Path could not be created");
	}

	return FALSE;
}

gboolean
tracker_env_check_xdg_dirs (void)
{
	const gchar *user_data_dir;
	gchar       *new_dir;
	gboolean     success;

	g_message ("Checking XDG_DATA_HOME is writable and exists");

	/* NOTE: We don't use g_get_user_data_dir() here because as
	 * soon as we do, it sets the result and doesn't re-fetch the
	 * XDG_DATA_HOME environment variable which we set below.
	 */
	user_data_dir = g_getenv ("XDG_DATA_HOME");

	/* Check the default XDG_DATA_HOME location */
	g_message ("  XDG_DATA_HOME is '%s'", user_data_dir);

	if (user_data_dir && tracker_path_has_write_access_or_was_created (user_data_dir)) {
		return TRUE;
	}

	user_data_dir = g_getenv ("HOME");

	if (!user_data_dir || !tracker_path_has_write_access_or_was_created (user_data_dir)) {
		user_data_dir = g_get_home_dir ();
	}

	/* Change environment, this is actually what we have on Ubuntu. */
	new_dir = g_build_path (G_DIR_SEPARATOR_S, user_data_dir, ".local", "share", NULL);

	/* Check the new XDG_DATA_HOME location */
	success = g_setenv ("XDG_DATA_HOME", new_dir, TRUE);

	if (success) {
		g_message ("  XDG_DATA_HOME set to '%s'", new_dir);
		success = tracker_path_has_write_access_or_was_created (new_dir);
	} else {
		g_message ("  XDG_DATA_HOME could not be set");
	}

	g_free (new_dir);

	return success;
}

gboolean
tracker_file_lock (GFile *file)
{
	gint fd, retval;
	gchar *path;

	g_return_val_if_fail (G_IS_FILE (file), FALSE);

	if (G_UNLIKELY (!file_locks)) {
		file_locks = g_hash_table_new_full ((GHashFunc) g_file_hash,
		                                    (GEqualFunc) g_file_equal,
		                                    (GDestroyNotify) g_object_unref,
		                                    NULL);
	}

	/* Don't try to lock twice */
	if (g_hash_table_lookup (file_locks, file) != NULL) {
		return TRUE;
	}

	if (!g_file_is_native (file)) {
		return FALSE;
	}

	path = g_file_get_path (file);

	if (!path) {
		return FALSE;
	}

	fd = open (path, O_RDONLY);

	if (fd < 0) {
		g_warning ("Could not open '%s'", path);
		g_free (path);

		return FALSE;
	}

	retval = flock (fd, LOCK_EX);

	if (retval == 0) {
		g_hash_table_insert (file_locks,
		                     g_object_ref (file),
		                     GINT_TO_POINTER (fd));
	} else {
		g_warning ("Could not lock file '%s'", path);
		close (fd);
	}

	g_free (path);

	return (retval == 0);
}

gboolean
tracker_file_unlock (GFile *file)
{
	gint retval, fd;

	g_return_val_if_fail (G_IS_FILE (file), TRUE);

	if (!file_locks) {
		return TRUE;
	}

	fd = GPOINTER_TO_INT (g_hash_table_lookup (file_locks, file));

	if (fd == 0) {
		/* File wasn't actually locked */
		return TRUE;
	}

	retval = flock (fd, LOCK_UN);

	if (retval < 0) {
		gchar *path;

		path = g_file_get_path (file);
		g_warning ("Could not unlock file '%s'", path);
		g_free (path);

		return FALSE;
	}

	g_hash_table_remove (file_locks, file);
	close (fd);

	return TRUE;
}

gboolean
tracker_file_is_locked (GFile *file)
{
	GFileInfo *file_info;
	gboolean retval = FALSE;
	gchar *path;
	gint fd;

	g_return_val_if_fail (G_IS_FILE (file), FALSE);

	if (!g_file_is_native (file)) {
		return FALSE;
	}

	/* Handle regular files; skip pipes and alike */
	file_info = g_file_query_info (file,
	                               G_FILE_ATTRIBUTE_STANDARD_TYPE,
	                               G_FILE_QUERY_INFO_NONE,
	                               NULL,
	                               NULL);

	if (!file_info) {
		return FALSE;
	}

	if (g_file_info_get_file_type (file_info) != G_FILE_TYPE_REGULAR) {
		g_object_unref (file_info);
		return FALSE;
	}

	g_object_unref (file_info);

	path = g_file_get_path (file);

	if (!path) {
		return FALSE;
	}

	fd = open (path, O_RDONLY);

	if (fd < 0) {
		g_warning ("Could not open '%s'", path);
		g_free (path);

		return FALSE;
	}

	/* Check for locks */
	retval = flock (fd, LOCK_SH | LOCK_NB);

	if (retval < 0) {
		if (errno == EWOULDBLOCK) {
			retval = TRUE;
		}
	} else {
		/* Oops, call was successful, unlock again the file */
		flock (fd, LOCK_UN);
	}

	close (fd);
	g_free (path);

	return retval;
}
