/* Tracker Meta Folder
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
#include <glib.h>
#include <glib-object.h>

#include "../libtracker/tracker.h" 


static void
get_meta_table_data (gpointer value)
		    
{
	char **meta, **meta_p;

	meta = (char **)value;

	int i = 0;
	for (meta_p = meta; *meta_p; meta_p++) {

		if (i == 0) {
			g_print ("%s : ", *meta_p);

		} else {
			g_print ("%s ", *meta_p);
		}
		i++;
	}
	g_print ("\n");
}



int 
main (int argc, char **argv) 
{
	
	GPtrArray *out_array = NULL;
	GError *error = NULL;
	TrackerClient *client = NULL;


	setlocale (LC_ALL, "");

	if (argc < 2) {
		g_print ("usage - tracker-meta-folder FOLDER [Metadata1...]\n");
		return 1;
	}

	client =  tracker_connect (FALSE);

	if (!client) {
		g_print ("Could not initialise Tracker - exiting...\n");
		return 1;
	}


	char **meta_fields = NULL; 

	if (argc == 2) {
		meta_fields = g_new (char *, 1);

		meta_fields[0] = NULL;

	} else if (argc > 2) {
		int i;

		meta_fields = g_new (char *, (argc-1));

		for (i=0; i < (argc-2); i++) {
			meta_fields[i] = g_locale_to_utf8 (argv[i+2], -1, NULL, NULL, NULL);
		}
		meta_fields[argc-2] = NULL;
	}

	char *folder = g_filename_to_utf8 (argv[1], -1, NULL, NULL, NULL);

	out_array = tracker_files_get_metadata_for_files_in_folder (client, -1, folder, meta_fields, &error);

	g_free (folder);

	g_strfreev (meta_fields);

	if (error) {
		g_warning ("An error has occured : %s", error->message);
		g_error_free (error);
	}

	if (out_array) {
		g_ptr_array_foreach (out_array, (GFunc)get_meta_table_data, NULL);
		g_ptr_array_free (out_array, TRUE);
	}


	tracker_disconnect (client);

	return 0;
}
