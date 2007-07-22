/* Tracker
 * routines for applications
 * Copyright (C) 2007, Marcus Rosell (mudflap75@gmail.com)
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

#include<string.h>

#include "tracker-apps.h"
#include "tracker-db.h"


void
tracker_applications_add_service_directories (void)
{
	if (1) { /* TODO: configurable.. tracker->index_applications? */

		char *value;
		gchar *dir = NULL;

		value = getenv ("XDG_DATA_HOME");
		if (value) {
			dir = g_strdup_printf ("%s/applications", value);
		} else {
			dir = g_strdup_printf ("%s/.local/share/applications", g_get_home_dir());
		
		}
		
		/* Add user defined applications path to service directory list */
		if (dir) {
			tracker_log ("Registering path %s as belonging to service Applications", dir);
			tracker_add_service_path ("Applications", dir);
			g_free (dir);
		}
		
		/* Add system defined applications path to service directory list */
		value = getenv ("XDG_DATA_DIRS");
		if (value != NULL) {
			gchar **dir_array;
			dir_array = g_strsplit (value, ":", 0);

			gint i;
			for (i = 0; dir_array[i] != NULL; ++i) {
				dir = g_strdup_printf ("%s/applications", dir_array[i]);
				tracker_info ("Registering path %s as belonging to service Applications", dir);
				tracker_add_service_path ("Applications", dir);
				g_free (dir);	
			}
			g_strfreev (dir_array);

		} else {
			tracker_log ("Registering path %s as belonging to service Applications", "/usr/local/share/applications");
			tracker_log ("Registering path %s as belonging to service Applications", "/usr/share/applications");
			tracker_add_service_path ("Applications", "/usr/local/share/applications");
			tracker_add_service_path ("Applications", "/usr/share/applications");
		}
	}
}

static void					
free_metadata_list (gpointer key, gpointer value, gpointer data) 
{
	GSList *list = value;

	g_slist_foreach (list, (GFunc) g_free, NULL);
	g_slist_free (list);

}

void
tracker_db_index_application (DBConnection *db_con, FileInfo *info)
{
	/* Index application metadata from .desktop files */
	
	GHashTable	*meta_table;

	GError *error = NULL;
	GKeyFile *key_file = NULL;

	gchar *type = NULL;
	gboolean is_hidden = TRUE;
	char *hidden = NULL;
	gchar *tmp_str = NULL;
	gchar desktop_entry[] = { "Desktop Entry" };
	char **mimes = NULL;
	char **categories = NULL;

	const char *file_name = "File:Name";
	const char *app_name = "App:Name";
	const char *app_display_name = "App:DisplayName";
	const char *app_generic_name = "App:GenericName";
	const char *app_comment = "App:Comment";
	const char *app_exec = "App:Exec";
	const char *app_icon = "App:Icon";
	const char *app_mime_type = "App:MimeType";
	const char *app_categories = "App:Categories";

	/* Check (to be sure) if this is a .desktop file */
	if (g_str_has_suffix (info->uri, ".desktop") == FALSE)	return;

	const gchar * const *locale_array;
	locale_array = g_get_language_names();

	key_file = g_key_file_new();

	if (g_key_file_load_from_file (key_file, info->uri, G_KEY_FILE_NONE, &error) == TRUE) {
	
		type = g_key_file_get_string (key_file, desktop_entry, "Type", NULL);
		hidden = g_key_file_get_string (key_file, desktop_entry, "Hidden", NULL);

		if (hidden) {
			is_hidden = (strcasecmp (hidden, "true") == 0);
			g_free (hidden);
		}

		if (!hidden && type && (strcasecmp (type, "Application") == 0)) {

			meta_table = g_hash_table_new (g_str_hash, g_str_equal);

			if ((tmp_str = g_key_file_get_string (key_file, desktop_entry, "Name", NULL)) != NULL) {
				tracker_add_metadata_to_table  (meta_table, app_name, tmp_str);
			} 

			if ((tmp_str = g_key_file_get_locale_string (key_file, desktop_entry, "Name", locale_array[0], NULL)) != NULL) {
				tracker_add_metadata_to_table  (meta_table, app_display_name, tmp_str);
			} 

			if ((tmp_str = g_key_file_get_locale_string (key_file, desktop_entry, "GenericName", locale_array[0], NULL)) != NULL) {
				tracker_add_metadata_to_table  (meta_table, app_generic_name, tmp_str);
			} 

			if ((tmp_str = g_key_file_get_locale_string (key_file, desktop_entry, "Comment", locale_array[0], NULL)) != NULL) {
				tracker_add_metadata_to_table  (meta_table, app_comment, tmp_str);
			} 

			if ((tmp_str = g_key_file_get_string (key_file, desktop_entry, "Exec", NULL)) != NULL) {
				tracker_add_metadata_to_table  (meta_table, app_exec, tmp_str);
			} 

			if ((tmp_str = g_key_file_get_string (key_file, desktop_entry, "Icon", NULL)) != NULL) {
				tracker_add_metadata_to_table  (meta_table, app_icon, tmp_str);
			} 


			if ((mimes = g_key_file_get_string_list (key_file, desktop_entry, "MimeType", NULL, NULL)) != NULL) {
				char **array;

				for (array = mimes; *array; array++) {
					tracker_add_metadata_to_table  (meta_table, app_mime_type, g_strdup (*array));
				}

				if (mimes) {
					g_strfreev (mimes);
				}
			} 

			if ((categories = g_key_file_get_string_list (key_file, desktop_entry, "Categories", NULL, NULL)) != NULL) {
				char **array;

				for (array = categories; *array; array++) {
					tracker_add_metadata_to_table  (meta_table, app_categories, g_strdup (*array));
				}


				if (categories) {
					g_strfreev (categories);
				}
			} 

			char *fname = g_path_get_basename (info->uri);

			/* remove desktop extension */
			int l = strlen (fname) - 8;
			fname[l] = '\0';

			tracker_add_metadata_to_table  (meta_table, file_name, fname);


//			info->mime = g_strdup ("unknown");
			tracker_db_index_service (db_con, info, "Applications", meta_table, NULL, FALSE, TRUE, FALSE, FALSE);

			
			g_hash_table_foreach (meta_table, (GHFunc) free_metadata_list, NULL);
			g_hash_table_destroy (meta_table);
		}

		g_key_file_free (key_file);
		g_free (type);
	}
}

