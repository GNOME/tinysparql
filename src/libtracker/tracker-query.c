/* Tracker Extract - extracts embedded metadata from files
 *
 * Copyright (C) 2005, Mr Jamie McCracken (jamiemcc@gnome.org)	
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA. 
 */

#include <locale.h>
#include <sys/param.h>
#include <stdlib.h>
#include <glib.h>
#include <glib-object.h>
#include <glib/gstdio.h>

#include "../libtracker/tracker.h" 

static char *
realpath_in_utf8 (const char *path)
{
	char *str_path_tmp = NULL, *str_path = NULL;

	str_path_tmp = realpath (path, NULL);

	if (str_path_tmp) {
		str_path = g_filename_to_utf8 (str_path_tmp, -1, NULL, NULL, NULL);

		g_free (str_path_tmp);
	}

	if (!str_path) {
		g_warning ("realpath_in_utf8(): locale to UTF8 failed!");
		return NULL;
	}

	return str_path;
}

static void
get_meta_table_data (gpointer key,
   		     gpointer value,
		     gpointer user_data)
{
	char **meta, **meta_p;

	if (!G_VALUE_HOLDS (value, G_TYPE_STRV)) {
		g_warning ("fatal communication error");
		return;
	}

	g_print ("%s \n", (char *) key);

	meta = g_value_get_boxed (value);

	for (meta_p = meta; *meta_p; meta_p++) {
		g_print ("%s, ", *meta_p);
	}
	g_print ("\n\n");
}


int
main (int argc, char **argv) 
{

	char *buffer, *tmp;
	gsize buffer_length;
	GHashTable *table = NULL;
	GError *error = NULL;
	TrackerClient *client = NULL;


	setlocale (LC_ALL, "");
	
	if (argc < 2) {
		g_print ("usage - tracker-query File [Metadata Fields1...]\nMetadata fields are defined at http://freedesktop.org/wiki/Standards/shared-filemetadata-spec\nExample usage: tracker-query file.rdf File.Format File.Size\n");
		return 1;
	}
	
	char *str_path = realpath_in_utf8 (argv[1]);

	if (!str_path) {
		return 1;
	}

	if (!g_file_get_contents (str_path, &tmp, &buffer_length, NULL)) {
		g_print ("Could not read file %s\n", str_path);
		return 1;
	}

	g_free (str_path);

	buffer = g_locale_to_utf8 (tmp, buffer_length, NULL, NULL, NULL);

	if (!buffer) {
		g_warning ("Cannot convert query file to UTF8!");
		g_free (tmp);
		return 1;
	}


	client =  tracker_connect (FALSE);

	if (!client) {
		g_print ("Could not initialise Tracker over dbus connection - exiting...\n");
		return 1; 
	}

	char **meta_fields = NULL;

	if (argc == 2) {
		meta_fields = g_new (char *, 2);

		meta_fields[0] = g_strdup ("File.Format");
		meta_fields[1] = NULL;

	} else if (argc > 2) {
		int i;

		meta_fields = g_new (char *, (argc-1));

		for (i=0; i < (argc-2); i++) {
			meta_fields[i] = g_locale_to_utf8 (argv[i+2], -1, NULL, NULL, NULL);
		}
		meta_fields[argc-2] = NULL;
	}

	table = tracker_search_query (client, -1, SERVICE_FILES, meta_fields, NULL, buffer, 0, 512, FALSE, &error);

	g_strfreev (meta_fields);

	if (error) {
		g_warning ("An error has occured : %s\n", error->message);
		g_error_free (error);
		g_free (buffer);
		return 1;
	}

	if (table) {
		g_print ("got %d values\n\n", g_hash_table_size (table));
		g_hash_table_foreach (table, get_meta_table_data, NULL);
		g_hash_table_destroy (table);	
	}


	tracker_disconnect (client);	

	g_free (buffer);

	return 0;
}
