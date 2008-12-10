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
#include <string.h>
#include <time.h>
#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include <libtracker/tracker.h>

static gchar	     *service;
static gchar        **uri = NULL;

static GOptionEntry   entries[] = {
	{ "service", 's', 0, G_OPTION_ARG_STRING, &service,
	  N_("Service type of the file"),
	  NULL
	},
	{ G_OPTION_REMAINING, 0,
	  G_OPTION_FLAG_FILENAME, G_OPTION_ARG_STRING_ARRAY, &uri,
	  N_("FILE..."),
	  N_("FILE")},
	{ NULL }
};

static void
print_property_value (gpointer value)
{
	gchar **pair;

	pair = value;

	g_print ("%s - %s\n", pair[0], pair[1]);
}

int
main (int argc, char **argv)
{
	TrackerClient	*client;
	ServiceType	 type;
	GFile           *file;
	gchar           *abs_path;
	GOptionContext	*context;
	GError		*error = NULL;
	GPtrArray	*results;

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* Translators: this messagge will apper immediately after the	*/
	/* usage string - Usage: COMMAND [OPTION]... <THIS_MESSAGE>	*/
	context = g_option_context_new (_("- Get all information from a certain file"));

	/* Translators: this message will appear after the usage string */
	/* and before the list of options.				*/
	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);

	if (!uri) {
		gchar *help;

		g_printerr ("%s\n\n",
			    _("Uri missing"));

		help = g_option_context_get_help (context, TRUE, NULL);
		g_option_context_free (context);
		g_printerr ("%s", help);
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

	if (!service) {
		g_print ("%s\n",
			 _("Defaulting to 'files' service"));

		type = SERVICE_FILES;
	} else {
		type = tracker_service_name_to_type (service);

		if (type == SERVICE_OTHER_FILES && g_ascii_strcasecmp (service, "Other")) {
			g_printerr ("%s\n",
				    _("Service type not recognized, using 'Other' ..."));
		}
	}

	file = g_file_new_for_commandline_arg (uri[0]);
	abs_path = g_file_get_path (file);

	results = tracker_metadata_get_all (client,
					    type,
					    abs_path,
					    &error);
	g_free (abs_path);
	g_object_unref (file);
	
	if (error) {
		g_printerr ("%s, %s\n",
			    _("Unable to retrieve data for uri"),
			    error->message);
		
		g_error_free (error);
		tracker_disconnect (client);
		
		return EXIT_FAILURE;
	}

	if (!results) {
		g_print ("%s\n",
			 _("No metadata available for that uri"));
	} else {
		g_print ("%s\n",
			 _("Results:"));
		
		g_ptr_array_foreach (results, (GFunc) print_property_value, NULL);
		g_ptr_array_foreach (results, (GFunc) g_strfreev, NULL);
		g_ptr_array_free (results, TRUE);
	}

	tracker_disconnect (client);

	return EXIT_SUCCESS;
}
