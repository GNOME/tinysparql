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

#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>

#include <libtracker/tracker.h>

static gchar	   *service;
static gchar	  **mimes;
static gint	    limit = 512;
static gint	    offset;

static GOptionEntry entries[] = {
	{ "service", 's', 0, G_OPTION_ARG_STRING, &service,
	  N_("Search from a specific service"),
	  NULL
	},
	{ "limit", 'l', 0, G_OPTION_ARG_INT, &limit,
	  N_("Limit the number of results shown"),
	  N_("512")
	},
	{ "offset", 'o', 0, G_OPTION_ARG_INT, &offset,
	  N_("Offset the results"),
	  N_("0")
	},
	{ "add-mime", 'm', 0, G_OPTION_ARG_STRING_ARRAY, &mimes,
	  N_("MIME types (can be used multiple times)"),
	  NULL
	},
	{ NULL }
};

int
main (int argc, char **argv)
{

	TrackerClient  *client;
	ServiceType	type;
	GOptionContext *context;
	GError	       *error = NULL;

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	context = g_option_context_new (_("- Search for files by service or by MIME type"));
	g_option_context_add_main_entries (context, entries, "tracker-files");
	g_option_context_parse (context, &argc, &argv, NULL);

	if (!service && !mimes) {
		gchar *help;

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

	if (service) {
		gchar **array;
		gchar **p_strarray;

		type = tracker_service_name_to_type (service);

		array = tracker_files_get_by_service_type (client,
							   time (NULL),
							   type,
							   offset,
							   limit,
							   &error);

		if (error) {
			g_printerr ("%s:'%s', %s\n",
				    _("Could not get files by service type"),
				    service,
				    error->message);
			g_error_free (error);

			return EXIT_FAILURE;
		}

		if (!array) {
			g_print ("%s\n",
				 _("No files found by that service type"));

			return EXIT_FAILURE;
		}

		g_print ("%s:\n",
			 _("Results"));

		for (p_strarray = array; *p_strarray; p_strarray++) {
			g_print ("  %s\n", *p_strarray);
		}

		g_strfreev (array);
	}

	if (mimes) {
		gchar **array;
		gchar **p;

		array = tracker_files_get_by_mime_type (client,
							time (NULL),
							mimes,
							offset,
							limit,
							&error);

		if (error) {
			g_printerr ("%s, %s\n",
				    _("Could not get files by MIME type"),
				    error->message);
			g_error_free (error);

			return EXIT_FAILURE;
		}

		if (!array) {
			g_print ("%s\n",
				 _("No files found by those MIME types"));

			return EXIT_FAILURE;
		}

		g_print ("%s:\n",
			 _("Results"));

		for (p = array; *p; p++) {
			g_print ("  %s\n", *p);
		}

		g_strfreev (array);
	}

	tracker_disconnect (client);

	return EXIT_SUCCESS;
}
