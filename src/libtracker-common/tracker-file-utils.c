/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>

#include <glib.h>
#include <gio/gio.h>

#include "tracker-log.h"
#include "tracker-os-dependant.h"
#include "tracker-file-utils.h"
#include "tracker-type-utils.h"

#define TEXT_SNIFF_SIZE 4096

FILE *
tracker_file_open (const gchar *uri,
		   const gchar *how,
		   gboolean	sequential)
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
		
		/* Make sure we set the NOATIME flag if we have permissions to */
		if ((flags = fcntl (fd, F_GETFL, 0)) != -1) {
			fcntl (fd, F_SETFL, flags | O_NOATIME);
		}

#ifdef HAVE_POSIX_FADVISE
		if (sequential_access) {
			posix_fadvise (fd, 0, 0, POSIX_FADV_SEQUENTIAL);
		} else {
			posix_fadvise (fd, 0, 0, POSIX_FADV_RANDOM);
		}
#endif
	}
	
	/* FIXME: Do nothing with posix_fadvise() for non-readonly operations */

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
#endif

	fclose (file);
}

gboolean
tracker_file_unlink (const gchar *uri)
{
	gchar	 *str;
	gboolean  result;

	str = g_filename_from_utf8 (uri, -1, NULL, NULL, NULL);
	result = g_unlink (str) == 0;
	g_free (str);

	return result;
}

goffset
tracker_file_get_size (const gchar *uri)
{
	GFileInfo *info;
	GFile	  *file;
	GError	  *error = NULL;
	goffset    size;

	g_return_val_if_fail (uri != NULL, 0);

	/* NOTE: We will need to fix this in Jurg's branch and call
	 * the _for_uri() variant.
	 */
	file = g_file_new_for_path (uri);
	info = g_file_query_info (file,
				  G_FILE_ATTRIBUTE_STANDARD_SIZE,
				  G_FILE_QUERY_INFO_NONE,
				  NULL,
				  &error);

	if (G_UNLIKELY (error)) {
		g_message ("Could not get size for '%s', %s",
			   uri,
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
tracker_file_get_mtime (const gchar *uri)
{
	GFileInfo *info;
	GFile	  *file;
	GError	  *error = NULL;
	guint64    mtime;

	g_return_val_if_fail (uri != NULL, 0);

	/* NOTE: We will need to fix this in Jurg's branch and call
	 * the _for_uri() variant.
	 */
	file = g_file_new_for_path (uri);
	info = g_file_query_info (file,
				  G_FILE_ATTRIBUTE_TIME_MODIFIED,
				  G_FILE_QUERY_INFO_NONE,
				  NULL,
				  &error);

	if (G_UNLIKELY (error)) {
		g_message ("Could not get mtime for '%s', %s",
			   uri,
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
tracker_file_get_mime_type (const gchar *uri)
{
	GFileInfo *info;
	GFile	  *file;
	GError	  *error = NULL;
	gchar	  *content_type;

	/* NOTE: We will need to fix this in Jurg's branch and call
	 * the _for_uri() variant.
	 */
	file = g_file_new_for_path (uri);
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

	g_object_unref (file);

	return content_type ? content_type : g_strdup ("unknown");
}

static gchar *
tracker_file_get_vfs_path (const gchar *uri)
{
	gchar *p;

	if (!uri || !strchr (uri, G_DIR_SEPARATOR)) {
		return NULL;
	}

	p = (gchar*) uri + strlen (uri) - 1;

	/* Skip trailing slash */
	if (p != uri && *p == G_DIR_SEPARATOR) {
		p--;
	}

	/* Search backwards to the next slash. */
	while (p != uri && *p != G_DIR_SEPARATOR) {
		p--;
	}

	if (p[0] != '\0') {
		gchar *new_uri_text;
		gint   length;

		length = p - uri;

		if (length == 0) {
			new_uri_text = g_strdup (G_DIR_SEPARATOR_S);
		} else {
			new_uri_text = g_malloc (length + 1);
			memcpy (new_uri_text, uri, length);
			new_uri_text[length] = '\0';
		}

		return new_uri_text;
	} else {
		return g_strdup (G_DIR_SEPARATOR_S);
	}
}

static gchar *
tracker_file_get_vfs_name (const gchar *uri)
{
	gchar *p, *res, *tmp, *result;

	if (!uri || !strchr (uri, G_DIR_SEPARATOR)) {
		return g_strdup (" ");
	}

	tmp = g_strdup (uri);
	p = tmp + strlen (uri) - 1;

	/* Skip trailing slash */
	if (p != tmp && *p == G_DIR_SEPARATOR) {
		*p = '\0';
	}

	/* Search backwards to the next slash.	*/
	while (p != tmp && *p != G_DIR_SEPARATOR) {
		p--;
	}

	res = p + 1;

	if (res && res[0] != '\0') {
		result = g_strdup (res);
		g_free (tmp);

		return result;
	}

	g_free (tmp);

	return g_strdup (" ");
}


static gchar *
normalize_uri (const gchar *uri) {

	GFile  *f;
	gchar *normalized;

	f = g_file_new_for_path (uri);
	normalized =  g_file_get_path (f);
	g_object_unref (f);

	return normalized;
}

void
tracker_file_get_path_and_name (const gchar *uri,
				gchar **path,
				gchar **name)
{

	g_return_if_fail (uri);
	g_return_if_fail (path);
	g_return_if_fail (name);

	if (uri[0] == G_DIR_SEPARATOR) {
		gchar *checked_uri;

		checked_uri = normalize_uri (uri);
		*name = g_path_get_basename (checked_uri);
		*path = g_path_get_dirname (checked_uri);

		g_free (checked_uri);
	} else {
		*name = tracker_file_get_vfs_name (uri);
		*path = tracker_file_get_vfs_path (uri);
	}

}

void
tracker_path_remove (const gchar *uri)
{
	GQueue *dirs;
	GSList *dirs_to_remove = NULL;

	g_return_if_fail (uri != NULL);

	dirs = g_queue_new ();

	g_queue_push_tail (dirs, g_strdup (uri));

	while (!g_queue_is_empty (dirs)) {
		GDir  *p;
		gchar *dir;

		dir = g_queue_pop_head (dirs);
		dirs_to_remove = g_slist_prepend (dirs_to_remove, dir);

		if ((p = g_dir_open (dir, 0, NULL))) {
			const gchar *file;

			while ((file = g_dir_read_name (p))) {
				gchar *full_filename;

				full_filename = g_build_filename (dir, file, NULL);

				if (g_file_test (full_filename, G_FILE_TEST_IS_DIR)) {
					g_queue_push_tail (dirs, full_filename);
				} else {
					g_unlink (full_filename);
					g_free (full_filename);
				}
			}

			g_dir_close (p);
		}
	}

	g_queue_free (dirs);

	/* Remove directories (now they are empty) */
	g_slist_foreach (dirs_to_remove, (GFunc) g_remove, NULL);
	g_slist_foreach (dirs_to_remove, (GFunc) g_free, NULL);
	g_slist_free (dirs_to_remove);
}

gboolean
tracker_path_is_in_path (const gchar *path,
			 const gchar *in_path)
{
	gchar	 *new_path;
	gchar	 *new_in_path;
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

void
tracker_path_hash_table_filter_duplicates (GHashTable *roots)
{
	GHashTableIter iter1, iter2;
	gpointer       key;

	g_hash_table_iter_init (&iter1, roots);
	while (g_hash_table_iter_next (&iter1, &key, NULL)) {
		const gchar *path;
		gchar       *p;

		path = key;

		g_hash_table_iter_init (&iter2, roots);
		while (g_hash_table_iter_next (&iter2, &key, NULL)) {
			const gchar *in_path;

			in_path = key;

			if (path == in_path) {
				continue;
			}

			if (tracker_path_is_in_path (path, in_path)) {
				g_debug ("Removing path:'%s', it is in path:'%s'",
					 path, in_path);

				g_hash_table_iter_remove (&iter1);
				g_hash_table_iter_init (&iter1, roots);
				break;
			} else if (tracker_path_is_in_path (in_path, path)) {
				g_debug ("Removing path:'%s', it is in path:'%s'",
					 in_path, path);

				g_hash_table_iter_remove (&iter2);
				g_hash_table_iter_init (&iter1, roots);
				break;
			}
		}

		/* Make sure the path doesn't have the '/' suffix. */	
		p = strrchr (path, G_DIR_SEPARATOR);

		if (p) {
			if (*(p + 1) == '\0') {
				*p = '\0';
			}
		}
	}

#ifdef TESTING
	g_debug ("Hash table paths were filtered down to:");

	if (TRUE) {
		GList *keys, *l;

		keys = g_hash_table_get_keys (roots);

		for (l = keys; l; l = l->next) {
			g_debug ("  %s", (gchar*) l->data);
		}

		g_list_free (keys);
	}
#endif /* TESTING */
}

GSList *
tracker_path_list_filter_duplicates (GSList      *roots,
				     const gchar *basename_exception_prefix)
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

			if (tracker_path_is_in_path (path, in_path)) {
				g_debug ("Removing path:'%s', it is in path:'%s'",
					 path, in_path);

				g_free (l1->data);
				new_list = g_slist_delete_link (new_list, l1);
				l1 = new_list;

				reset = TRUE;

				continue;
			} 
			else if (tracker_path_is_in_path (in_path, path)) {
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
		
		/* Make sure the path doesn't have the '/' suffix. */	
		p = strrchr (path, G_DIR_SEPARATOR);

		if (p) {
			if (*(p + 1) == '\0') {
				*p = '\0';
			}
		}

		/* Continue in list unless reset. */	
		if (G_LIKELY (!reset)) {
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
tracker_path_evaluate_name (const gchar *uri)
{
	gchar	     *final_path;
	gchar	    **tokens;
	gchar	    **token;
	gchar	     *start;
	gchar	     *end;
	const gchar  *env;
	gchar	     *expanded;

	if (!uri || uri[0] == '\0') {
		return NULL;
	}

	/* First check the simple case of using tilder */
	if (uri[0] == '~') {
		const char *home = g_get_home_dir ();

		if (!home || home[0] == '\0') {
			return NULL;
		}

		return g_build_path (G_DIR_SEPARATOR_S,
				     home,
				     uri + 1,
				     NULL);
	}

	/* Second try to find any environment variables and expand
	 * them, like $HOME or ${FOO}
	 */
	tokens = g_strsplit (uri, G_DIR_SEPARATOR_S, -1);

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
		expanded = g_strdup (uri);
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
	GFile	  *file;
	GFileInfo *info;
	GError	  *error = NULL;
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
	}

	g_object_unref (info);

	return writable;
}

static gboolean
path_has_write_access_or_was_created (const gchar *path)
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
	gchar	    *new_dir;
	gboolean     success;

	g_message ("Checking XDG_DATA_HOME is writable and exists");

	/* NOTE: We don't use g_get_user_data_dir() here because as
	 * soon as we do, it sets the result and doesn't re-fetch the
	 * XDG_DATA_HOME environment variable which we set below.
	 */
	user_data_dir = g_getenv ("XDG_DATA_HOME");

	/* Check the default XDG_DATA_HOME location */
	g_message ("  XDG_DATA_HOME is '%s'", user_data_dir);

	if (user_data_dir && path_has_write_access_or_was_created (user_data_dir)) {
		return TRUE;
	}

	/* Change environment, this is actually what we have on Ubuntu. */
	new_dir = g_build_path (G_DIR_SEPARATOR_S, g_get_home_dir (), ".local", "share", NULL);

	/* Check the new XDG_DATA_HOME location */
	success = g_setenv ("XDG_DATA_HOME", new_dir, TRUE);

	if (success) {
		g_message ("  XDG_DATA_HOME set to '%s'", new_dir);
		success = path_has_write_access_or_was_created (new_dir);
	} else {
		g_message ("  XDG_DATA_HOME could not be set");
	}

	g_free (new_dir);

	return success;
}
