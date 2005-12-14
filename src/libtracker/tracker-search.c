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

void 
main (int argc, char **argv) 
{
	char **strarray;
	char **p_strarray;
	GError *error = NULL;

	if (!argv[1]) {
		g_print ("usage - tracker-search SearchTerm");
	}

	if (!tracker_init ()) {
		g_print ("Could not initialise Tracker - exiting...");
		return;
	}

	strarray = tracker_search_metadata (argv[1], error);

	if (!strarray) {
		g_print ("no results were found matching your query");
		return;
	}
	
	for (p_strarray = strarray; *p_strarray; p_strarray++) {
    		 g_print ("%s\n", *p_strarray);
	}

	g_strfreev (strarray);

	tracker_close ();

}


		
