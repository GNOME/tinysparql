/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia
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

#include "config.h"

#include <stdlib.h>
#include <time.h>
#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>

#include <libtracker/tracker.h>

#define MAX_FILENAME_WIDTH 35

static gchar	     *path;
static gchar	    **fields;

static GOptionEntry   entries[] = {
	{ "path", 'p', 0, G_OPTION_ARG_STRING, &path,
	  N_("Path to use for directory to get metadata information about"),
	  NULL,
	},
	{ G_OPTION_REMAINING, 0, 0,
	  G_OPTION_ARG_STRING_ARRAY, &fields,
	  NULL,
	  NULL
	},
	{ NULL }
};

static void
print_header (gchar **fields_resolved)
{
	gint cols;
	gint width;
	gint i;

	/* Headers */
	g_print ("  %-*.*s ",
		 MAX_FILENAME_WIDTH,
		 MAX_FILENAME_WIDTH,
		 _("Filename"));

	width  = MAX_FILENAME_WIDTH;
	width += 1;

	cols = g_strv_length (fields_resolved);

	for (i = 0; i < cols; i++) {
		g_print ("%s%s",
			 fields_resolved[i],
			 i < cols - 1 ? ", " : "");

		width += g_utf8_strlen (fields_resolved[i], -1);
		width += i < cols - 1 ? 2 : 0;
	}
	g_print ("\n");

	/* Line under header */
	g_print ("  ");
	for (i = 0; i < width; i++) {
		g_print ("-");
	}
	g_print ("\n");
}

static void
get_meta_table_data (gpointer data,
		     gpointer user_data)
{
	gchar **meta;
	gchar **p;
	gchar **fields;
	gchar  *basename;
	gint	i;
	gint	len;
	gint	cols;

	meta = data;
	fields = user_data;

	basename = g_path_get_basename (*meta);
	len = g_utf8_strlen (basename, -1);
	cols = g_strv_length (meta);

	for (p = meta, i = 0; *p; p++, i++) {
		if (i == 0) {
			g_print ("  %-*.*s",
				 MAX (len, MAX_FILENAME_WIDTH),
				 MAX (len, MAX_FILENAME_WIDTH),
				 basename);

			if (len > MAX_FILENAME_WIDTH) {
				gint i = 0;

				g_print ("\n");
				while (i++ < MAX_FILENAME_WIDTH + 2) {
					g_print (" ");
				}
			}

			g_print (" (");
		} else {
			g_print ("%s%s",
				 *p,
				 i < cols - 1 ? ", " : "");
		}
	}

	g_free (basename);

	g_print (")\n");
}

int
main (int argc, char **argv)
{
	TrackerClient	*client;
	GOptionContext	*context;
	GError		*error = NULL;
	gchar		*summary;
	const gchar	*failed = NULL;
	gchar	       **fields_resolved = NULL;
	gchar		*path_in_utf8;
	GPtrArray	*array;
	gint		 i, j;

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* Translators: this messagge will apper immediately after the
	 * usage string - Usage: COMMAND [OPTION]... <THIS_MESSAGE>
	 */
	context = g_option_context_new (_("Retrieve meta-data information about files in a directory"));

	/* Translators: this message will appear after the usage string
	 * and before the list of options, showing an usage example.
	 */
	summary = g_strconcat (_("To use multiple meta-data types simply list them, for example:"),
			       "\n"
			       "\n"
			       "  -p ", _("PATH"), " File:Size File:Type",
			       NULL);

	g_option_context_set_summary (context, summary);
	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);
	g_free (summary);

	if (!path) {
		failed = _("No path was given");
	} else if (!fields) {
		failed = _("No fields were given");
	}

	if (failed) {
		gchar *help;

		g_printerr ("%s\n\n", failed);

		help = g_option_context_get_help (context, TRUE, NULL);
		g_option_context_free (context);
		g_printerr (help);
		g_free (help);

		return EXIT_FAILURE;
	}

	g_option_context_free (context);

	client = tracker_connect (FALSE);

	if (!client) {
		g_printerr ("%s\n",
			    _("Could not establish a DBus connection to Tracker"));
		return EXIT_FAILURE;
	}

	fields_resolved = g_new0 (gchar*, g_strv_length (fields) + 1);

	for (i = 0, j = 0; fields && fields[i] != NULL; i++) {
		gchar *field;

		field = g_locale_to_utf8 (fields[i], -1, NULL, NULL, NULL);

		if (field) {
			fields_resolved[j++] = field;
		}
	}

	fields_resolved[j] = NULL;

	path_in_utf8 = g_filename_to_utf8 (path, -1, NULL, NULL, &error);
	if (error) {
		g_printerr ("%s:'%s', %s\n",
			    _("Could not get UTF-8 path from path"),
			    path,
			    error->message);
		g_error_free (error);
		tracker_disconnect (client);

		return EXIT_FAILURE;
	}

	array = tracker_files_get_metadata_for_files_in_folder (client,
								time (NULL),
								path_in_utf8,
								fields_resolved,
								&error);

	g_free (path_in_utf8);

	if (error) {
		g_printerr ("%s:'%s', %s\n",
			    _("Could not get meta-data for files in directory"),
			    path,
			    error->message);
		g_error_free (error);
		g_strfreev (fields_resolved);
		tracker_disconnect (client);

		return EXIT_FAILURE;
	}

	if (!array) {
		g_print ("%s\n",
			 _("No meta-data found for files in that directory"));
	} else {
		g_print ("%s:\n",
			 _("Results"));

		print_header (fields_resolved);

		g_ptr_array_foreach (array,
				     get_meta_table_data,
				     fields_resolved);
		g_ptr_array_free (array, TRUE);
	}

	g_strfreev (fields_resolved);
	tracker_disconnect (client);

	return EXIT_SUCCESS;
}
