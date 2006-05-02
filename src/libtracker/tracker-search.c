/* Tracker Search
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

#include <glib.h>
#include "../libtracker/tracker.h" 

static GMainLoop 	*loop;

static void
my_callback (char **result, GError *error, gpointer user_data)
{
	
	char **p_strarray;
	
	if (error) {
		g_warning ("An error has occured : %s", error->message);
		g_error_free (error);
		return;
	}

	if (!result) {
		g_print ("no results were found matching your query");
		return;
	}
	
	for (p_strarray = result; *p_strarray; p_strarray++) {
    		 g_print ("%s\n", *p_strarray);
	}

	g_strfreev (result);

	g_main_loop_quit (loop);

}

int 
main (int argc, char **argv) 
{

	TrackerClient *client = NULL;

	if (argc < 2) {
		g_print ("usage - tracker-search SearchTerm [mimetype]");
		return 1;

	}

	client =  tracker_connect (FALSE);

	if (!client) {
		g_print ("Could not initialise Tracker - exiting...");
		return 1;
	}

	loop = g_main_loop_new (NULL, TRUE);

	if (argc ==3) {
		char **array;

		array = g_new (char *, 3);
		array[0] = argv[2];
		array[1] = argv[3];
		array[2] = NULL;
		tracker_search_metadata_by_text_and_mime_async (client, argv[1], (const char**)array, my_callback, NULL);
	} else {
		tracker_search_metadata_by_text_async (client, argv[1], my_callback, NULL);
	}

	g_main_loop_run (loop);

	tracker_disconnect (client);

	return 0;
}
