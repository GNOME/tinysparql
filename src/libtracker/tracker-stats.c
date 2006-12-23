/* Tracker - indexer and metadata database engine
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <locale.h>
#include <string.h>
#include <glib.h>
#include <glib-object.h>

#include "../libtracker/tracker.h" 

#define TOTAL_COUNT "Total files indexed"


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

	if (argc > 1) {
		g_print ("usage - tracker-stats\n");
		return 1;
	}

	client =  tracker_connect (FALSE);

	if (!client) {
		g_print ("Could not initialise Tracker - exiting...\n");
		return 1;
	}

	out_array = tracker_get_stats (client, &error);


	if (error) {
		g_warning ("An error has occured : %s", error->message);
		g_error_free (error);
	}


	if (out_array) {

		g_print ("\n-------fetching index stats---------\n\n");
		g_ptr_array_foreach (out_array, (GFunc)get_meta_table_data, NULL);
		g_ptr_array_free (out_array, TRUE);
		g_print ("------------------------------------\n\n");

	}


	tracker_disconnect (client);

	return 0;
}
