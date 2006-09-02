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
#include <string.h>
#include <glib.h>
#include <glib-object.h>

#include "../libtracker/tracker.h" 

#define TOTAL_COUNT "Total files indexed"


static void
get_meta_table_data (char *key,
   		     gpointer value)
		    
{
	char **meta, **meta_p;

	if (!G_VALUE_HOLDS (value, G_TYPE_STRV)) {
		g_warning ("fatal communication error\n");
		return;
	}

	g_print ("%s : ", key);

	meta = g_value_get_boxed (value);

	int i = 0;
	for (meta_p = meta; *meta_p; meta_p++) {

		if (i == 0) {
			g_print ("%s ", *meta_p);

		} else if (i == 1 && (strcmp (key, TOTAL_COUNT) != 0)) {
			g_print ("(%s\%)", *meta_p);
		}
		i++;
	}
	g_print ("\n");
}

static void
get_meta_table_data_for_each (gpointer key,
			      gpointer value,
			      gpointer user_data)
{
	if (strcmp ((char* )key, TOTAL_COUNT) != 0) {
		get_meta_table_data ((char *)key, value);
	}
}


int 
main (int argc, char **argv) 
{
	
	GHashTable *table = NULL;
	GError *error = NULL;
	TrackerClient *client = NULL;


	setlocale (LC_ALL, "");

	if (argc > 1) {
		g_print ("usage - tracker-stats\n");
		return 1;
	}

	client =  tracker_connect (FALSE);

	if (!client) {
		g_print ("Could not initialise Tracker - exiting...\n");
		return 1;
	}

	table =  tracker_get_stats (client, &error);


	if (error) {
		g_warning ("An error has occured : %s", error->message);
		g_error_free (error);
	}


	if (table) {

		g_print ("\n-------fetching index stats---------\n\n");
		
		gpointer ptr = g_hash_table_lookup (table, TOTAL_COUNT);

		if (ptr) {
			get_meta_table_data (TOTAL_COUNT, ptr);
			g_print ("\n");
		}

		g_hash_table_foreach (table, get_meta_table_data_for_each, NULL);
		g_print ("------------------------------------\n\n");
		g_hash_table_destroy (table);	
	}


	tracker_disconnect (client);

	return 0;
}
