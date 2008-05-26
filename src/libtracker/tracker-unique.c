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
#include <sys/param.h>
#include <stdlib.h>
#include <glib.h>
#include <glib-object.h>
#include <glib/gstdio.h>

#include "../libtracker/tracker.h" 

#include <config.h>
#ifdef OS_WIN32
#include "../trackerd/mingw-compat.h"
#endif

static char **fields = NULL;
static gchar *service = NULL;
static gchar *rdf = NULL;
static gboolean descending = FALSE;

static GOptionEntry entries[] = {
	{"service", 's', 0, G_OPTION_ARG_STRING, &service, "search from a specific service", "service"},
	{"rdf", 'r', 0, G_OPTION_ARG_STRING, &rdf, "use an rdf query as filter", "rdf"},	
	{"desc", 'o', 0, G_OPTION_ARG_NONE, &descending, "sort to descending order", NULL},
	{G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &fields, "Required fields", NULL},
	{NULL}
};


static char *
realpath_in_utf8 (const char *path)
{
	char *str_path_tmp = NULL, *str_path = NULL;

	str_path_tmp = realpath (path, NULL);

	if (str_path_tmp) {
		str_path = g_filename_to_utf8 (str_path_tmp, -1, NULL, NULL, NULL);

		g_free (str_path_tmp);
	}

	if (!str_path) {
		g_warning ("realpath_in_utf8(): locale to UTF8 failed!");
		return NULL;
	}

	return str_path;
}

static void
get_meta_table_data (gpointer value)
		    
{
	char **meta, **meta_p;

	meta = (char **)value;

	int i = 0;
	for (meta_p = meta; *meta_p; meta_p++) {

		char *str;

		str = g_filename_from_utf8 (*meta_p, -1, NULL, NULL, NULL);

		if (i == 0) {
			g_print ("%s : ", str);

		} else {
			g_print ("%s, ", *meta_p);
		}
		i++;
	}
	g_print ("\n");
}



int
main (int argc, char **argv) 
{
	GOptionContext *context = NULL;
	ServiceType type;

	char *buffer = NULL, *tmp;
	gsize buffer_length;
	GPtrArray *out_array = NULL;
	GError *error = NULL;
	TrackerClient *client = NULL;

	setlocale (LC_ALL, "");

	context = g_option_context_new ("MetaDataField [RDFQueryFile] - Get unique values with an optional rdf query filter");
	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, &error);

	if (error) {
		g_printerr ("invalid arguments: %s\n", error->message);
		return 1;
	}

	if (!fields) {
		g_printerr ("missing metadata type specification, try --help for help\n");
		return 1;
	}
	

	if (!service) {
		type = SERVICE_FILES;
	} else {
		type = tracker_service_name_to_type (service);

		if (type == SERVICE_OTHER_FILES && g_ascii_strcasecmp (service, "Other")) {
			g_printerr ("service not recognized, searching in Other Files...\n");
		}
	}

	if (rdf) {

	  char *str_path = realpath_in_utf8 (rdf);
	  
	  if (!str_path) {
	    return 1;
	  }
	  
	  if (!g_file_get_contents (str_path, &tmp, &buffer_length, NULL)) {
	    g_print ("Could not read file %s\n", str_path);
	    return 1;
	  }
	  
	  g_free (str_path);
	  
	  buffer = g_locale_to_utf8 (tmp, buffer_length, NULL, NULL, NULL);
	  
	  if (!buffer) {
	    g_warning ("Cannot convert query file to UTF8!");
	    g_free (tmp);
	    return 1;
	  }
	}

	client =  tracker_connect (FALSE);

	if (!client) {
		g_print ("Could not initialise Tracker over dbus connection - exiting...\n");
		return 1; 
	}


	out_array = tracker_metadata_get_unique_values (client, type, fields, buffer, descending, 0, 512, &error);

	if (error) {
		g_warning ("An error has occurred : %s\n", error->message);
		g_error_free (error);
		g_free (buffer);
		return 1;
	}

	if (out_array) {
		g_ptr_array_foreach (out_array, (GFunc)get_meta_table_data, NULL);
		g_ptr_array_free (out_array, TRUE);
	}

	tracker_disconnect (client);	

	g_free (buffer);

	return 0;
}
