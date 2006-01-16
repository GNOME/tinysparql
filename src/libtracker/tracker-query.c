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

#include <stdio.h>
#include <glib.h>
#include "../libtracker/tracker.h" 

int
main (int argc, char **argv) 
{

	int bytes_read;
	char buffer[2048];
	char **strarray;
	char **p_strarray;
	GError *error = NULL;
	TrackerClient *client = NULL;

	
	client =  tracker_connect (FALSE);

	if (!client) {
		g_print ("Could not initialise Tracker - exiting...");
		return;
	}

	bytes_read = fread(buffer, 1, 2047, stdin);

	if(!bytes_read) {
		return 0;
	}
	
	buffer[bytes_read] = '\0';

	strarray = tracker_search_metadata_by_query (client, buffer, error);

	if (!strarray) {
		g_print ("no results were found matching your query");
		return;
	}
	
	for (p_strarray = strarray; *p_strarray; p_strarray++) {
    		 g_print ("%s\n", *p_strarray);
	}

	g_strfreev (strarray);

	tracker_disconnect (client);	

	return 0;

}


