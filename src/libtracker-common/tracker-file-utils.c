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

#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/file.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>

#ifdef __linux__
#include <sys/statfs.h>
#endif

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

#include "tracker-file-utils.h"

#define TEXT_SNIFF_SIZE 4096

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
		gchar *uri;

		uri = g_file_get_uri (file);
		g_message ("Could not get size for '%s', %s",
		           uri,
		           error->message);
		g_free (uri);
		g_error_free (error);
		size = 0;
	} else {
		size = g_file_info_get_size (info);
		g_object_unref (info);
	}

	g_object_unref (file);

	return size;
}

#ifdef __linux__

#define __bsize f_bsize

#ifdef __USE_LARGEFILE64
#define __statvfs statfs64
#else
#define __statvfs statfs
#endif

#else /* __linux__ */

#define __bsize f_frsize

#ifdef HAVE_STATVFS64
#define __statvfs statvfs64
#else
#define __statvfs statvfs
#endif

#endif /* __linux__ */

static gboolean
statvfs_helper (const gchar *path, struct __statvfs *st)
{
	gchar *_path;
	int retval;

//LCOV_EXCL_START
	/* Iterate up the path to the root until statvfs() doesn’t error with
	 * ENOENT. This prevents the call failing on first-startup when (for
	 * example) ~/.cache/tracker might not exist. */
	_path = g_strdup (path);

	while ((retval = __statvfs (_path, st)) == -1 && errno == ENOENT) {
		gchar *tmp = g_path_get_dirname (_path);
		g_free (_path);
		_path = tmp;
	}

	g_free (_path);
//LCOV_EXCL_STOP

	if (retval == -1) {
		g_critical ("Could not statvfs() '%s': %s",
		            path,
		            g_strerror (errno));
	}

	return (retval == 0);
}

static guint64
tracker_file_system_get_remaining_space (const gchar *path)
{
	struct __statvfs st;
	guint64 available;

	if (statvfs_helper (path, &st)) {
		available = (geteuid () == 0) ? st.f_bfree : st.f_bavail;
		/* __bsize is a platform dependent #define above */
		return st.__bsize * available;
	} else {
		return 0;
	}
}

gboolean
tracker_file_system_has_enough_space (const gchar *path,
                                      gulong       required_bytes,
                                      gboolean     creating_db)
{
	gchar *str1;
	gchar *str2;
	gboolean enough;
	guint64 remaining;

	g_return_val_if_fail (path != NULL, FALSE);

	remaining = tracker_file_system_get_remaining_space (path);
	enough = (remaining >= required_bytes);

	if (creating_db) {
		str1 = g_format_size (required_bytes);
		str2 = g_format_size (remaining);

		if (!enough) {
			g_critical ("Not enough disk space to create databases, "
			            "%s remaining, %s required as a minimum",
			            str2,
			            str1);
		} else {
			g_debug ("Checking for adequate disk space to create databases, "
			         "%s remaining, %s required as a minimum",
			         str2,
			         str1);
		}

		g_free (str2);
		g_free (str1);
	}

	return enough;
}
