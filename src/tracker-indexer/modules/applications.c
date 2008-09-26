/* Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia

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

#include <stdlib.h>
#include <glib.h>
#include <tracker-indexer/tracker-module.h>
#include <tracker-indexer/tracker-metadata.h>

#define GROUP_DESKTOP_ENTRY "Desktop Entry"
#define KEY_TYPE	    "Type"
#define KEY_HIDDEN	    "Hidden"
#define KEY_NAME	    "Name"
#define KEY_GENERIC_NAME    "GenericName"
#define KEY_COMMENT	    "Comment"
#define KEY_EXECUTABLE	    "Exec"
#define KEY_ICON	    "Icon"
#define KEY_MIMETYPE	    "MimeType"
#define KEY_CATEGORIES	    "Categories"

#define METADATA_FILE_NAME	  "File:Name"
#define METADATA_APP_NAME	  "App:Name"
#define METADATA_APP_DISPLAY_NAME "App:DisplayName"
#define METADATA_APP_GENERIC_NAME "App:GenericName"
#define METADATA_APP_COMMENT	  "App:Comment"
#define METADATA_APP_EXECUTABLE   "App:Exec"
#define METADATA_APP_ICON	  "App:Icon"
#define METADATA_APP_MIMETYPE	  "App:MimeType"
#define METADATA_APP_CATEGORIES   "App:Categories"

G_CONST_RETURN gchar *
tracker_module_get_name (void)
{
	/* Return module name here */
	return "Applications";
}

static void
insert_data_from_desktop_file (TrackerMetadata *metadata,
			       const gchar     *metadata_key,
			       GKeyFile        *desktop_file,
			       const gchar     *key,
			       gboolean		use_locale)
{
	gchar *str;

	if (use_locale) {
		str = g_key_file_get_locale_string (desktop_file, GROUP_DESKTOP_ENTRY, key, NULL, NULL);
	} else {
		str = g_key_file_get_string (desktop_file, GROUP_DESKTOP_ENTRY, key, NULL);
	}

	if (str) {
		tracker_metadata_insert (metadata, metadata_key, str);
	}
}

static void
insert_list_from_desktop_file (TrackerMetadata *metadata,
			       const gchar     *metadata_key,
			       GKeyFile        *desktop_file,
			       const gchar     *key,
			       gboolean		use_locale)
{
	gchar **arr;

	if (use_locale) {
		arr = g_key_file_get_locale_string_list (desktop_file, GROUP_DESKTOP_ENTRY, key, NULL, NULL, NULL);
	} else {
		arr = g_key_file_get_string_list (desktop_file, GROUP_DESKTOP_ENTRY, key, NULL, NULL);
	}

	if (arr) {
		GList *list = NULL;
		gint i;

		for (i = 0; arr[i]; i++) {
			list = g_list_prepend (list, arr[i]);
		}

		list = g_list_reverse (list);
		g_free (arr);

		tracker_metadata_insert_multiple_values (metadata, metadata_key, list);
	}
}

TrackerMetadata *
tracker_module_file_get_metadata (TrackerFile *file)
{
	TrackerMetadata *metadata;
	GKeyFile *key_file;
	gchar *type, *filename;

	/* Check we're dealing with a desktop file */
	if (!g_str_has_suffix (file->path, ".desktop")) {
		return NULL;
	}

	key_file = g_key_file_new ();

	if (!g_key_file_load_from_file (key_file, file->path, G_KEY_FILE_NONE, NULL)) {
		g_key_file_free (key_file);
		return NULL;
	}

	if (g_key_file_get_boolean (key_file, GROUP_DESKTOP_ENTRY, KEY_HIDDEN, NULL)) {
		g_key_file_free (key_file);
		return NULL;
	}

	type = g_key_file_get_string (key_file, GROUP_DESKTOP_ENTRY, KEY_TYPE, NULL);

	if (!type || g_ascii_strcasecmp (type, "Application") != 0) {
		g_key_file_free (key_file);
		g_free (type);
		return NULL;
	}

	/* Begin collecting data */
	metadata = tracker_metadata_new ();

	insert_data_from_desktop_file (metadata, METADATA_APP_NAME, key_file, KEY_NAME, FALSE);
	insert_data_from_desktop_file (metadata, METADATA_APP_DISPLAY_NAME, key_file, KEY_NAME, TRUE);
	insert_data_from_desktop_file (metadata, METADATA_APP_GENERIC_NAME, key_file, KEY_GENERIC_NAME, TRUE);
	insert_data_from_desktop_file (metadata, METADATA_APP_COMMENT, key_file, KEY_COMMENT, TRUE);
	insert_data_from_desktop_file (metadata, METADATA_APP_EXECUTABLE, key_file, KEY_EXECUTABLE, FALSE);
	insert_data_from_desktop_file (metadata, METADATA_APP_ICON, key_file, KEY_ICON, FALSE);

	insert_list_from_desktop_file (metadata, METADATA_APP_MIMETYPE, key_file, KEY_MIMETYPE, FALSE);
	insert_list_from_desktop_file (metadata, METADATA_APP_CATEGORIES, key_file, KEY_CATEGORIES, FALSE);

	filename = g_filename_display_basename (file->path);
	tracker_metadata_insert (metadata, METADATA_FILE_NAME, filename);

	g_key_file_free (key_file);
	g_free (type);

	return metadata;
}
