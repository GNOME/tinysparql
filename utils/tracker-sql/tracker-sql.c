/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2010, Nokia
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */


#include "config.h"

#include <string.h>
#include <locale.h>

#include <glib.h>

#include <libtracker-common/tracker-ontologies.h>
#include <libtracker-data/tracker-data-manager.h>
#include <libtracker-data/tracker-data-query.h>
#include <libtracker-data/tracker-data-update.h>
#include <libtracker-data/tracker-sparql-query.h>

static gchar *file;
static gchar *query;

static GOptionEntry   entries[] = {
	{ "file", 'f', 0, G_OPTION_ARG_FILENAME, &file,
	  "Path to use to run a query from file",
	  "FILE",
	},
	{ "query", 'q', 0, G_OPTION_ARG_STRING, &query,
	  "SQL query",
	  "SQL",
	},
	{ NULL }
};

int
main (int argc, char **argv)
{
	GOptionContext     *context = NULL;
	GError             *error = NULL;
	const gchar        *error_message;
	
	gchar              *query = NULL;
	TrackerDBResultSet *result_set;
	TrackerDBInterface *iface;
	gboolean            first_time = FALSE;
	
	setlocale (LC_ALL, "");
	g_type_init ();
	if (!g_thread_supported ()) {
		g_thread_init (NULL);
	}
	
	context = g_option_context_new ("- Query or update using SQL");
	
	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);
	
	if (!file && !query) {
		error_message = "An argument must be supplied";
	} else if (file && query) {
		error_message = "File and query can not be used together";
	} else {
		error_message = NULL;
	}
	
	if (error_message) {
		gchar *help;
		
		g_printerr ("%s\n\n", error_message);
		
		help = g_option_context_get_help (context, TRUE, NULL);
		g_option_context_free (context);
		g_printerr ("%s", help);
		g_free (help);
		
		return EXIT_FAILURE;
	}
	g_option_context_free (context);
	

	if (file) {
		gchar *path_in_utf8;
		gsize size;
		
		path_in_utf8 = g_filename_to_utf8 (file, -1, NULL, NULL, &error);
		if (error) {
			g_printerr ("%s:'%s', %s\n",
			            "Could not get UTF-8 path from path",
			            file,
			            error->message);
			g_error_free (error);
			
			return EXIT_FAILURE;
		}
		
		g_file_get_contents (path_in_utf8, &query, &size, &error);
		if (error) {
			g_printerr ("%s:'%s', %s\n",
			            "Could not read file",
			            path_in_utf8,
			            error->message);
			g_error_free (error);
			g_free (path_in_utf8);
			
			return EXIT_FAILURE;
		}
		
		g_free (path_in_utf8);
	}
	
	if (query) {
		
		if (!tracker_data_manager_init (0,
						NULL,
						&first_time,
						FALSE,
						100,
						100,
						NULL,
						NULL,
						NULL)) {
			g_printerr ("Failed to initialize data manager\n");
			return EXIT_FAILURE;
			
		}
		
		iface = tracker_db_manager_get_db_interface  ();
		
		result_set = tracker_db_interface_execute_query (iface, &error, "%s", query);
		
		if (error) {
			g_printerr ("%s, %s\n",
			            "Could not run query",
			            error->message);
			g_error_free (error);

			return EXIT_FAILURE;
		}
		
		if (result_set) {
			gboolean valid = TRUE;
			guint columns;
			
			columns = tracker_db_result_set_get_n_columns (result_set);
			
			while (valid) {
				guint i;
				
				for (i =  0; i < columns; i++) {
					GValue value = {0, };
					GValue transform = {0, };
					
					if (i)
						g_print (" | ");
					
					g_value_init (&transform, G_TYPE_STRING);
					_tracker_db_result_set_get_value (result_set, i, &value);
					if (G_IS_VALUE (&value) && g_value_transform (&value, &transform)) {
						gchar *str;
						
						str = g_value_dup_string (&transform);
						g_print ("%s", str);
						g_value_unset (&value);
					} else {
						g_print ("(null)");
					}
					g_value_unset (&transform);
				}
				
				g_print ("\n");
				
				valid = tracker_db_result_set_iter_next (result_set);
			}
		} else {
			g_print ("Empty result set");
		}
		
	}

	return EXIT_SUCCESS;
}

