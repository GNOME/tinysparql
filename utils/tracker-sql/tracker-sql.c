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
#include <glib/gi18n.h>

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
	GOptionContext *context;
	GError *error = NULL;
	const gchar *error_message;
	TrackerDBInterface *iface;
	TrackerDBStatement *stmt;
	TrackerDBCursor *cursor;
	
	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	context = g_option_context_new (_("- Query or update using SQL"));
	
	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);
	
	if (!file && !query) {
		error_message = _("An argument must be supplied");
	} else if (file && query) {
		error_message = _("File and query can not be used together");
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

	g_type_init ();
	if (!g_thread_supported ()) {
		g_thread_init (NULL);
	}

	if (file) {
		gchar *path_in_utf8;
		gsize size;
		
		path_in_utf8 = g_filename_to_utf8 (file, -1, NULL, NULL, &error);
		if (error) {
			g_printerr ("%s:'%s', %s\n",
			            _("Could not get UTF-8 path from path"),
			            file,
			            error->message);
			g_error_free (error);
			
			return EXIT_FAILURE;
		}
		
		g_file_get_contents (path_in_utf8, &query, &size, &error);
		if (error) {
			g_printerr ("%s:'%s', %s\n",
			            _("Could not read file"),
			            path_in_utf8,
			            error->message);
			g_error_free (error);
			g_free (path_in_utf8);
			
			return EXIT_FAILURE;
		}
		
		g_free (path_in_utf8);
	}
	
	if (query) {
		gboolean first_time = FALSE;
		gint n_rows = 0;

		if (!tracker_data_manager_init (0,
						NULL,
						&first_time,
						FALSE,
						100,
						100,
						NULL,
						NULL,
						NULL)) {
			g_printerr ("%s\n", 
			            _("Failed to initialize data manager"));
			return EXIT_FAILURE;
		}

		g_print ("--------------------------------------------------\n");
		g_print ("\n\n");
	
		iface = tracker_db_manager_get_db_interface ();

		stmt = tracker_db_interface_create_statement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE, &error, "%s", query);

		if (stmt) {
			cursor = tracker_db_statement_start_cursor (stmt, &error);
		}

		if (error) {
			g_printerr ("%s: %s\n",
			            _("Could not run query"),
			            error->message);
			g_error_free (error);

			return EXIT_FAILURE;
		}
		
		g_print ("%s:\n", _("Results"));

		while (tracker_db_cursor_iter_next (cursor, NULL, &error)) {
			guint i;
			
			for (i =  0; i < tracker_db_cursor_get_n_columns (cursor); i++) {
				const gchar *str;
				
				if (i)
					g_print (" | ");

				str = tracker_db_cursor_get_string (cursor, i, NULL);
				if (str) {
					g_print ("%s", str);
				} else {
					g_print ("(null)");
				}
			}
			
			g_print ("\n");
			
			n_rows++;
		}

		if (error) {
			g_printerr ("%s: %s\n",
			            _("Could not run query"),
			            error->message);
			g_error_free (error);

			return EXIT_FAILURE;
		}

		if (n_rows == 0) {
			g_print ("%s\n", _("Empty result set"));
		}
	}

	g_print ("\n");

	return EXIT_SUCCESS;
}

