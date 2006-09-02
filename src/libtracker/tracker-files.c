/* Tracker Files - Returns files basedontheir service or mime type
 *
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)	
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
#include <stdlib.h>
#include <string.h>
#include <glib.h>

#include "../libtracker/tracker.h" 

#define USAGE "usage: \ntracker-files -s ServiceType\t: Gets files with ServiceType (Documents, Music, Images, Videos, Text, Development, Other)\ntracker-files -m Mime1 [Mime2...] : Get all files that match one of the specified mime types\n"


static ServiceType
get_service_type (const char* service)
{
	if (g_ascii_strcasecmp (service, "Documents") ==0) {
		return SERVICE_DOCUMENTS;
	} else 	if (g_ascii_strcasecmp (service, "Music") ==0) {
		return SERVICE_MUSIC;
	} else 	if (g_ascii_strcasecmp (service, "Images") ==0) {
		return SERVICE_IMAGES;
	} else 	if (g_ascii_strcasecmp (service, "Videos") ==0) {
		return SERVICE_VIDEOS;
	} else 	if (g_ascii_strcasecmp (service, "Text") ==0) {
		return SERVICE_TEXT_FILES;
	} else 	if (g_ascii_strcasecmp (service, "Development") ==0) {
		return SERVICE_DEVELOPMENT_FILES;
	} else {
		return SERVICE_OTHER_FILES;
	}
}


int
main (int argc, char **argv) 
{

	TrackerClient *client = NULL;
	GError *error = NULL;
	ServiceType type;


	setlocale (LC_ALL, "");

	if (argc < 3) {
		g_print (USAGE);
		return 1;
	}

	client = tracker_connect (FALSE);

	if (!client) {
		g_print ("Could not initialise Tracker - exiting...\n");
		return 1;
	}

	if (strcmp (argv[1], "-s") == 0) {

		if (argc != 3) {
			g_print (USAGE);
			return 1;
		} else {

			type = get_service_type (argv[2]);

			char **array = NULL;

			array = tracker_files_get_by_service_type (client, -1, type, 0, 512, &error);

			if (error) {
				g_warning ("An error has occured : %s", error->message);
				g_error_free (error);
				return 1;
			}

			if (!array) {
				g_print ("no results were found matching your query\n");
				return 1;
			}

			char **p_strarray;

			for (p_strarray = array; *p_strarray; p_strarray++) {
				g_print ("%s\n", *p_strarray);
			}

			g_strfreev (array);
		}

	} else if (strcmp (argv[1], "-m") == 0)  {

		if (argc < 3) {
			g_print (USAGE);
			return 1;
		} else {

			char **mimes = NULL;
			char **array = NULL;
			int i;

			mimes = g_new (char *, (argc-1));

			for (i=0; i < (argc-2); i++) {
				mimes[i] = g_locale_to_utf8 (argv[i+2], -1, NULL, NULL, NULL);
			}
			mimes[argc-2] = NULL;
		
			array = tracker_files_get_by_mime_type (client, -1, mimes, 0, 512, &error);

			g_strfreev (mimes);

			if (error) {
				g_warning ("An error has occured : %s", error->message);
				g_error_free (error);
				return 1;
			}

			if (!array) {
				g_print ("no results were found matching your query\n");
				return 1;
			}

			char **p_strarray;

			for (p_strarray = array; *p_strarray; p_strarray++) {
				g_print ("%s\n", *p_strarray);
			}

			g_strfreev (array);
		}

	}  else {
		g_print (USAGE);
		return 1;
	}

	tracker_disconnect (client);

	return 0;
}
