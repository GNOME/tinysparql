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

#include <string.h>
#include <glib.h>
#include "../libtracker/tracker.h" 

#define USAGE "usage: \ntracker-tag -a File Tags\t: Add Tags to File\ntracker-tag -l File \t\t: List all tags on a File\ntracker-tag -r File Tags \t: Remove Tags from File\ntracker-tag -R File  \t\t: Remove all tags from File\ntracker-tag -s Tags  \t\t: Search files for specified tags\n"



/*

tracker-tag -a File Tags\t\t: Add Tags to File\n
tracker-tag -l File \t\t: List all tags on a File\n
tracker-tag -r File Tags \t\t: Remove Tags from File\n
tracker-tag -R File  \t\t: Remove all tags from File\n
tracker-tag -s Tags  \t\t: Search files for specified tags\n
*/





int 
main (int argc, char **argv) 
{

	TrackerClient *client = NULL;
	GError *error = NULL;

	if (argc < 3) {
		g_print (USAGE);
		return 1;

	}



	client =  tracker_connect (FALSE);

	if (!client) {
		g_print ("Could not initialise Tracker - exiting...\n");
		return 1;
	}

	
	if (strcmp (argv[1], "-a") == 0) {

		if (argc < 4) {
			g_print (USAGE);
			return 1;
		} else {

			int i;
			char **tags;

			tags = g_new (char *, (argc-2));
			for (i=0; i < (argc-3); i++) {
				tags[i] = argv[i+3];
				g_print ("adding tag %s to %s\n", tags[i], argv[2]);
			}

			tags[argc-3] = NULL;

			tracker_keywords_add (client,  SERVICE_FILES, argv[2], tags, &error);
			
			if (error) {
				g_warning ("An error has occured : %s\n", error->message);
				g_error_free (error);
				return 1;
			} else {
				g_print ("All tags added to %s\n", argv[2]);
			}
			
		}

	} else if (strcmp (argv[1], "-l") == 0)  {

		if (argc != 3) {
			g_print (USAGE);
			return 1;
		} else {

			
			char **tags = NULL;

			tags = tracker_keywords_get (client, SERVICE_FILES, argv[2],  &error);
			char **p_strarray;
	
			if (error) {
				g_warning ("An error has occured : %s", error->message);
				g_error_free (error);
				return 1;
			}

			if (!tags) {
				g_print ("no results were found matching your query\n");
				return 1;
			}
	
			for (p_strarray = tags; *p_strarray; p_strarray++) {
				if (*p_strarray) {
	   				g_print ("%s\n", *p_strarray);
				}
			}

			g_strfreev (tags);

		}

	} else if (strcmp (argv[1], "-r") == 0)  {

		if (argc < 4) {
			g_print (USAGE);
			return 1;
	
		} else {

			int i;
			char **tags;

			tags = g_new (char *, (argc-2));
			for (i=0; i < (argc-3); i++) {
				tags[i] = argv[i+3];
			}
			tags[argc-3] = NULL;

			tracker_keywords_remove (client,  SERVICE_FILES, argv[2], tags, &error);
		}

	} else if (strcmp (argv[1], "-R") == 0)  {

		if (argc != 3) {
			g_print (USAGE);
			return 1;
	
		} else {

		
			

			tracker_keywords_remove_all (client,  SERVICE_FILES, argv[2], &error);
		}

	} else if (strcmp (argv[1], "-s") == 0)  {

		if (argc < 3) {
			g_print (USAGE);
			return 1;
	
		} else {

			int i;
			char **tags;

			tags = g_new (char *, (argc-1));
			for (i=0; i < (argc-2); i++) {
				tags[i] = argv[i+2];
			}
			tags[argc-2] = NULL;

			char **results = tracker_keywords_search (client, -1, SERVICE_FILES, tags, 512, &error);
			char **p_strarray;
	
			if (error) {
				g_warning ("An error has occured : %s", error->message);
				g_error_free (error);
				return 1;
			}

			if (!results) {
				g_print ("no results were found matching your query\n");
				return 1;
			}
	
			for (p_strarray = results; *p_strarray; p_strarray++) {
    				g_print ("%s\n", *p_strarray);
			}

			g_strfreev (results);

		}

	}  else {
		g_print (USAGE);
		return 1;
	}
	

	tracker_disconnect (client);

	return 0;
}
