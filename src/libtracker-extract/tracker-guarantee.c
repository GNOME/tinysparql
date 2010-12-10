/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
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

#include <glib.h>

#include <libtracker-common/tracker-common.h>

#include "tracker-guarantee.h"

/**
 * tracker_guarantee_title_from_filename:
 * @uri: a string representing a URI to use
 *
 * Parses the filename pointed to by @uri and uses the basename
 * (before the "." and extension of the file) as the title. If the
 * title has any "_" characters, they are also converted into spaces.
 *
 * Returns: A newly allocated string which must be freed with g_free()
 * or %NULL on error.
 *
 * Since: 0.10
 **/
gchar *
tracker_guarantee_title_from_filename (const gchar *uri)
{
	gchar *filename;
	gchar *basename;
	gchar *p;

	g_return_val_if_fail (uri != NULL, NULL);

	filename = g_filename_from_uri (uri, NULL, NULL);
	basename = g_filename_display_basename (filename);
	g_free (filename);

	p = strchr (basename, '.');
	if (p) {
		*p = '\0';
	}

	return g_strdelimit (basename, "_", ' ');

}

/**
 * tracker_guarantee_date_from_filename_mtime:
 * @uri: a string representing a URI to use
 *
 * Calls stat() on the filename pointed to by @uri to create a date
 * based on the file's mtime.
 *
 * Returns: A newly allocated string which must be freed with g_free()
 * or %NULL on error. The string represents the date in ISO8160.
 *
 * Since: 0.10
 **/
gchar *
tracker_guarantee_date_from_filename_mtime (const gchar *uri)
{
	gchar *filename;
	gchar *date;
	guint64 mtime;

	g_return_val_if_fail (uri != NULL, NULL);

	filename = g_filename_from_uri (uri, NULL, NULL);
	mtime = tracker_file_get_mtime (filename);
	g_free (filename);

	date = tracker_date_to_string ((time_t) mtime);

	return date;
}
