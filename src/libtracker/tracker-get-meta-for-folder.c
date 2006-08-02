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

#include <stdio.h>
#include <glib.h>
#include "../libtracker/tracker.h" 


static void
get_meta_table_data (gpointer key,
   		     gpointer value,
		     gpointer user_data)
{

	char **meta, **meta_p;

	if (!G_VALUE_HOLDS (value, G_TYPE_STRV)) {
		g_warning ("fatal communication error\n");
		return;
	}


	g_print ("%s \n", (char *)key);

	meta = g_value_get_boxed (value);

	for (meta_p = meta; *meta_p; meta_p++) {
		if (*meta_p) {
	     		 g_print ("%s, ",  (char *)*meta_p);
		}
	}	
	g_print ("\n\n");
}


int 
main (int argc, char **argv) 
{
	
	GHashTable *table = NULL;
	GError *error = NULL;
	TrackerClient *client = NULL;


	if (argc < 2) {
		g_print ("usage - tracker-meta-folder FOLDER [Metadata1...]\n");
		return 1;
	}

	client =  tracker_connect (FALSE);

	if (!client) {
		g_print ("Could not initialise Tracker - exiting...\n");
		return 1;
	}





	if (argc ==2) {
		char *fields[]  = {NULL};
	
		table =  tracker_files_get_metadata_for_files_in_folder (client, -1, argv[1], fields, &error);
	} else {

		int i;
		char **meta_fields;


		if (argc > 2) {
			meta_fields = g_new (char *, (argc-1));
			for (i=0; i < (argc-2); i++) {
				meta_fields[i] = argv[i+2];
			}
			meta_fields[argc-2] = NULL;
		}
		table =  tracker_files_get_metadata_for_files_in_folder (client, -1, argv[1], meta_fields, &error);
	}


	
	
	if (error) {
		g_warning ("An error has occured : %s", error->message);
		g_error_free (error);
	}

	

	if (table) {
		g_print ("got %d values\n", g_hash_table_size (table));
		g_hash_table_foreach (table, get_meta_table_data, NULL);
		g_hash_table_destroy (table);	
	}

	

	tracker_disconnect (client);
	return 0;
}
