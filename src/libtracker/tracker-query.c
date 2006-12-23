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

static gchar *search = NULL;
static gchar **fields = NULL;
static gchar *service = NULL;
static gchar *keyword = NULL;

static GOptionEntry entries[] = {
	{"service", 's', 0, G_OPTION_ARG_STRING, &service, "search from a specific service", "service"},
	{"search-term", 't', 0, G_OPTION_ARG_STRING, &search, "adds a fulltext search filter", "search-term"},
	{"keyword", 'k', 0, G_OPTION_ARG_STRING, &keyword, "adds a keyword filter", "keyword"},
	{G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &fields, "Metadata Fields", NULL},
	{NULL}
};


static int field_count;

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
	char **p_strarray;

	char *buffer, *tmp;
	gsize buffer_length;
	GPtrArray *out_array = NULL;
	GError *error = NULL;
	TrackerClient *client = NULL;


	setlocale (LC_ALL, "");

	context = g_option_context_new ("RDFQueryFile [MetaDataField...] ... - perform an rdf query and return results witrh specified metadata fields");
	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, &error);

	if (error) {
		g_printerr ("invalid arguments: %s\n", error->message);
		return 1;
	}

	if (!fields) {
		g_printerr ("missing input rdf query file, try --help for help\n");
		return 1;
	}
	

	if (!service) {
		type = SERVICE_FILES;
	} else if (g_ascii_strcasecmp (service, "Documents") == 0) {
		type = SERVICE_DOCUMENTS;
	} else if (g_ascii_strcasecmp (service, "Music") == 0) {
		type = SERVICE_MUSIC;
	} else if (g_ascii_strcasecmp (service, "Images") == 0) {
		type = SERVICE_IMAGES;
	} else if (g_ascii_strcasecmp (service, "Videos") == 0) {
		type = SERVICE_VIDEOS;
	} else if (g_ascii_strcasecmp (service, "Text") == 0) {
		type = SERVICE_TEXT_FILES;
	} else if (g_ascii_strcasecmp (service, "Development") == 0) {
		type = SERVICE_DEVELOPMENT_FILES;
	} else {
		g_printerr ("service not recognized, searching in Other Files...\n");
		type = SERVICE_OTHER_FILES;
	}

	
	char *str_path = realpath_in_utf8 (fields[0]);

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


	client =  tracker_connect (FALSE);

	if (!client) {
		g_print ("Could not initialise Tracker over dbus connection - exiting...\n");
		return 1; 
	}

	char **meta_fields = NULL;

	g_free (fields[0]);
	int i = 0;
	for (p_strarray = fields+1; *p_strarray; p_strarray++) {
		fields[i] = *p_strarray;
		i++;
	}
	fields[i] = NULL;


	if (i == 0) {
		meta_fields = g_new (char *, 2);

		field_count = 1;

		meta_fields[0] = g_strdup ("File:Mime");
		meta_fields[1] = NULL;
		g_strfreev  (fields);
		fields = meta_fields;
	} 

	out_array = tracker_search_query (client, -1, type, fields, search, keyword, buffer, 0, 512, FALSE, &error);
	
	g_strfreev (meta_fields);

	if (error) {
		g_warning ("An error has occured : %s\n", error->message);
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
