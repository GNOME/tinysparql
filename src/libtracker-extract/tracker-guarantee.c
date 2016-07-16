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

#include <libtracker-common/tracker-file-utils.h>
#include <libtracker-common/tracker-date-time.h>

#include "tracker-guarantee.h"

#ifdef GUARANTEE_METADATA

static gchar *
get_title_from_file (const gchar *uri)
{
	gchar *filename;
	gchar *basename;
	gchar *p;

	filename = g_filename_from_uri (uri, NULL, NULL);
	basename = g_filename_display_basename (filename);
	g_free (filename);

	p = strrchr (basename, '.');
	if (p) {
                if (p == basename) {
                        p = g_strdup (&basename[1]);
                        g_free (basename);
                        basename = p;
                } else {
                        *p = '\0';
                }
	}

	return g_strdelimit (basename, "_", ' ');
}

static gchar *
get_date_from_file_mtime (const gchar *uri)
{
	gchar *date;
	guint64 mtime;

	mtime = tracker_file_get_mtime_uri (uri);

	date = tracker_date_to_string ((time_t) mtime);

	return date;
}

#endif /* GUARANTEE_METADATA */

/**
 * tracker_guarantee_resource_title_from_file:
 * @resource: the relevant #TrackerResource
 * @key: the property URI to set
 * @current_value: the current data to check before looking at @uri.
 * @uri: a string representing a URI to use
 * @p_new_value: pointer to a string which receives the new title, or
 *             %NULL
 *
 * Checks @current_value to make sure it is usable (i.e. not %NULL or an
 * empty string). If it is not usable, then @uri is parsed to guarantee a
 * metadata value for @key.
 *
 * Parses the file pointed to by @uri and uses the basename
 * (before the "." and extension of the file) as the title. If the
 * title has any "_" characters, they are also converted into spaces.
 *
 * This function only operates if Tracker was compiled with
 * --enable-guarantee-metadata enabled at configure-time.
 *
 * Returns: %TRUE on success and content was added to @metadata, otherwise %FALSE.
 *
 * Since: 1.10
 **/
gboolean
tracker_guarantee_resource_title_from_file (TrackerResource  *resource,
                                            const gchar      *key,
                                            const gchar      *current_value,
                                            const gchar      *uri,
                                            gchar           **p_new_value)
{
	gboolean success = TRUE;

#ifdef GUARANTEE_METADATA
	g_return_val_if_fail (resource != NULL, FALSE);
	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (uri != NULL, FALSE);

	if (current_value && *current_value != '\0') {
		tracker_resource_set_string (resource, key, current_value);

		if (p_new_value != NULL) {
			*p_new_value = g_strdup (current_value);
		}
	} else {
		gchar *value;

		value = get_title_from_file (uri);

		if (value && value[0] != '\0') {
			tracker_resource_set_string (resource, key, value);
		} else {
			success = FALSE;
		}

		if (p_new_value != NULL) {
			*p_new_value = value;
		} else {
			g_free (value);
		}
	}
#else  /* GUARANTEE_METADATA */
	if (current_value && *current_value != '\0') {
		tracker_resource_set_string (resource, key, current_value);

		if (p_new_value != NULL) {
			*p_new_value = g_strdup (current_value);
		}
	} else {
		success = FALSE;
	}
#endif /* GUARANTEE_METADATA */

	return success;
}

/**
 * tracker_guarantee_resource_date_from_file_mtime:
 * @resource: the relevant #TrackerResource
 * @key: the property URI to set
 * @current_value: the current data to check before looking at @uri
 * @uri: a string representing a URI to use
 *
 * Checks @current_value to make sure it is sane (i.e. not %NULL or an
 * empty string). If it is, then @uri is parsed to guarantee a
 * metadata value for @key.
 *
 * When parsing @uri, stat() is called on the file to create a
 * date based on the file's mtime.
 *
 * This function only operates if Tracker was compiled with
 * --enable-guarantee-metadata enabled at configure-time.
 *
 * Returns: %TRUE on success and content was added to @metadata, otherwise %FALSE.
 *
 * Since: 1.10
 **/
gboolean
tracker_guarantee_resource_date_from_file_mtime (TrackerResource *resource,
                                                 const gchar     *key,
                                                 const gchar     *current_value,
                                                 const gchar     *uri)
{
	gboolean success = TRUE;

#ifdef GUARANTEE_METADATA
	g_return_val_if_fail (resource != NULL, FALSE);
	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (uri != NULL, FALSE);

	if (current_value && *current_value != '\0') {
		tracker_resource_set_string (resource, key, current_value);
	} else {
		gchar *value;

		value = get_date_from_file_mtime (uri);

		if (value && *value != '\0') {
			tracker_resource_set_string (resource, key, value);
		} else {
			success = FALSE;
		}

		g_free (value);
	}
#else  /* GUARANTEE_METADATA */
	if (current_value && *current_value != '\0') {
		tracker_resource_set_string (resource, key, current_value);
	} else {
		success = FALSE;
	}
#endif /* GUARANTEE_METADATA */

	return success;
}

gboolean
tracker_guarantee_resource_utf8_string (TrackerResource *resource,
                                        const gchar     *key,
                                        const gchar     *value)
{
	const gchar *end;
	gchar *str;

	if (!g_utf8_validate (value, -1, &end)) {
		if (end == value)
			return FALSE;

		str = g_strndup (value, end - value);
		tracker_resource_set_string (resource, key, str);
		g_free (str);
	} else {
		tracker_resource_set_string (resource, key, value);
	}

	return TRUE;
}
