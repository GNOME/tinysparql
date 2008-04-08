/* Tracker - indexer and metadata database engine
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
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

#include "config.h"

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#ifdef OS_WIN32
#include <conio.h>
#else
#include <sys/resource.h>
#endif

#include <glib/gstdio.h>

#include <libtracker-common/tracker-log.h>

#include "tracker-metadata.h"
#include "tracker-utils.h"
#include "tracker-service-manager.h"

extern Tracker *tracker;


char *
tracker_metadata_get_text_file (const char *uri, const char *mime)
{
	char		 *text_filter_file;
	char *service_type;
	text_filter_file = NULL;

	/* no need to filter text based files - index em directly */
	service_type = tracker_service_manager_get_service_type_for_mime (mime);
	if ( !strcmp ("Text", service_type) || !strcmp ("Development", service_type)) {

		g_free (service_type);
		return g_filename_from_utf8 (uri, -1, NULL, NULL, NULL);

	} else {
		char *tmp;

		tmp = g_strdup (LIBDIR "/tracker/filters/");

#ifdef OS_WIN32
		text_filter_file = g_strconcat (tmp, mime, "_filter.bat", NULL);
#else
		text_filter_file = g_strconcat (tmp, mime, "_filter", NULL);
#endif

		g_free (tmp);
	}

	if (text_filter_file && g_file_test (text_filter_file, G_FILE_TEST_EXISTS)) {
		char *argv[4];
		char *temp_file_name;
		int  fd;

		temp_file_name = g_build_filename (tracker->sys_tmp_root_dir, "tmp_text_file_XXXXXX", NULL);

		fd = g_mkstemp (temp_file_name);

		if (fd == -1) {
			g_warning ("make tmp file %s failed", temp_file_name);
			return NULL;
		} else {
			close (fd);
		}

		argv[0] = g_strdup (text_filter_file);
		argv[1] = g_filename_from_utf8 (uri, -1, NULL, NULL, NULL);
		argv[2] = g_strdup (temp_file_name);
		argv[3] = NULL;

		g_free (text_filter_file);

		if (!argv[1]) {
			tracker_error ("ERROR: uri could not be converted to locale format");
			g_free (argv[0]);
			g_free (argv[2]);
			return NULL;
		}

		tracker_info ("extracting text for %s using filter %s", argv[1], argv[0]);

		if (tracker_spawn (argv, 30, NULL, NULL)) {


			g_free (argv[0]);
			g_free (argv[1]);
			g_free (argv[2]);

			if (tracker_file_is_valid (temp_file_name)) {
				return temp_file_name;
			} else {
				g_free (temp_file_name);
				return NULL;
			}

		} else {
			g_free (temp_file_name);

			g_free (argv[0]);
			g_free (argv[1]);
			g_free (argv[2]);

			return NULL;
		}

	} else {
		g_free (text_filter_file);
	}

	return NULL;
}


char *
tracker_metadata_get_thumbnail (const char *path, const char *mime, const char *size)
{
	gchar   *thumbnail;
	gchar   *argv[5];
	gint     exit_status;

	argv[0] = g_strdup ("tracker-thumbnailer");
	argv[1] = g_filename_from_utf8 (path, -1, NULL, NULL, NULL);
	argv[2] = g_strdup (mime);
	argv[3] = g_strdup (size);
	argv[4] = NULL;

	if (!tracker_spawn (argv, 10, &thumbnail, &exit_status)) {
		thumbnail = NULL;
	} else if (exit_status != EXIT_SUCCESS) {
		thumbnail = NULL;
	} else {
		tracker_log ("got thumbnail %s", thumbnail);
	}

	g_free (argv[0]);
	g_free (argv[1]);
	g_free (argv[2]);
	g_free (argv[3]);

	return thumbnail;
}

void
tracker_metadata_get_embedded (const char *uri, const char *mime, GHashTable *table)
{
	gboolean success;
	char *argv[4];
	char *output;
	char **values;
	char *service_type;
	gint i;

	if (!uri || !mime || !table) {
		return;
	}

	service_type = tracker_service_manager_get_service_type_for_mime (mime);
	if (!service_type ) {
		return;
	}

	if (!tracker_service_manager_has_metadata (service_type)) {
		g_free (service_type);
		return;
	}

	/* we extract metadata out of process using pipes */
	argv[0] = g_strdup ("tracker-extract");
	argv[1] = g_filename_from_utf8 (uri, -1, NULL, NULL, NULL);
	argv[2] = g_locale_from_utf8 (mime, -1, NULL, NULL, NULL);
	argv[3] = NULL;

	if (!argv[1] || !argv[2]) {
		tracker_error ("ERROR: uri or mime could not be converted to locale format");

		g_free (argv[0]);
		g_free (argv[1]);
		g_free (argv[2]);

		return;
	}

	success = tracker_spawn (argv, 10, &output, NULL);

	g_free (argv[0]);
	g_free (argv[1]);
	g_free (argv[2]);

	if (!success || !output)
		return;

	/* parse returned stdout and extract keys and associated metadata values */

	values = g_strsplit_set (output, ";", -1);

	for (i = 0; values[i]; i++) {
		char *meta_data, *sep;
		const char *name, *value;
		char *utf_value;

		meta_data = g_strstrip (values[i]);
		sep = strchr (meta_data, '=');

		if (!sep)
			continue;

		/* zero out the separator, so we get
		 * NULL-terminated name and value
		 */
		sep[0] = '\0';
		name = meta_data;
		value = sep + 1;

		if (!name || !value)
			continue;

		if (g_hash_table_lookup (table, name))
			continue;

		if (!g_utf8_validate (value, -1, NULL)) {
			utf_value = g_locale_to_utf8 (value, -1, NULL, NULL, NULL);
		} else {
			utf_value = g_strdup (value);
		}

		if (!utf_value)
			continue;

		tracker_add_metadata_to_table (table, g_strdup (name), utf_value);
	}

	g_strfreev (values);
	g_free (output);
}
