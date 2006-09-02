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

#include <locale.h>
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
		g_print ("no results were found matching your query\n");
		return;
	}
	
	for (p_strarray = result; *p_strarray; p_strarray++) {
		char *s = NULL;

		s = g_locale_from_utf8 (*p_strarray, -1, NULL, NULL, NULL);

		if (s) {
			g_print ("%s\n", *p_strarray);

			g_free (s);
		}
	}

	g_strfreev (result);

	g_main_loop_quit (loop);
}


int 
main (int argc, char **argv) 
{

	TrackerClient *client = NULL;


	setlocale (LC_ALL, "");

	if (argc < 2) {
		g_print ("usage - tracker-search SearchTerm1  [Searchterm2...]\nMultiple search terms are ANDed by default\n");
		return 1;
	}

	client =  tracker_connect (FALSE);

	if (!client) {
		g_print ("Could not initialise Tracker - exiting...\n");
		return 1;
	}

	loop = g_main_loop_new (NULL, TRUE);
	
	if (argc > 2) {

		GString *str = g_string_new (argv[1]);
		int i;

		for (i=0; i<(argc-2); i++) {
			char *s = NULL;

			s = g_locale_to_utf8 (argv[i+2], -1, NULL, NULL, NULL);

			if (s) {
				g_string_append_printf (str, " %s", s);

				g_free (s);
			}
		}

		char *search = g_string_free (str, FALSE);

		tracker_search_text_async (client, -1, SERVICE_FILES, search, 0, 512, FALSE, my_callback, NULL);

	} else {
		char *s = NULL;

		s = g_locale_to_utf8 (argv[1], -1, NULL, NULL, NULL);

		if (s) {
			tracker_search_text_async (client, -1, SERVICE_FILES, s, 0, 512, FALSE, my_callback, NULL);

			g_free (s);
		}
	}

	g_main_loop_run (loop);

	tracker_disconnect (client);

	return 0;
}
