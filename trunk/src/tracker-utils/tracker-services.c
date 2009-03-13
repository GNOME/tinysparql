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

#include <libtracker/tracker.h>

static gboolean services = FALSE;
static gboolean properties = FALSE;

static GOptionEntry entries[] = {
	{ "service-types", 's', 0, G_OPTION_ARG_NONE, &services,
	  N_("Return the known service types"),
	  NULL
	},
	{ "properties", 'p', 0, G_OPTION_ARG_NONE, &properties,
	  N_("Return the known properties"),
	  NULL
	},
	{ NULL }
};

int
main (int argc, char **argv)
{
	TrackerClient   *client;
	GOptionContext  *context;
	gchar          **array;
	GError	        *error = NULL;

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* Translators: this messagge will apper immediately after the	*/
	/* usage string - Usage: COMMAND [OPTION]... <THIS_MESSAGE>	*/
	context = g_option_context_new (_(" - Show all available service types and properties in tracker"));
	g_option_context_add_main_entries (context, entries, "tracker-services");
	g_option_context_parse (context, &argc, &argv, NULL);

	if (!services && !properties) {
		gchar *help;
		
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

	if (services) {
		array = tracker_metadata_get_registered_classes (client, &error);

		if (error) {
			g_printerr ("%s, %s\n",
				    _("Could not get Tracker services"),
				    error->message);
			g_error_free (error);
			
			return EXIT_FAILURE;
		}

		if (!array) {
			g_print ("%s\n",
				 _("No services available"));
		} else {
			gchar **p;

			g_print ("%s:\n",
				 _("Service types available in Tracker"));
			
			for (p = array; *p; p++) {
				g_print ("  %s\n", *p);
			}

			g_strfreev (array);
		}
	}

	if (properties) {
		array = tracker_metadata_get_registered_types (client, "*", &error);
		
		if (error) {
			g_printerr ("%s, %s\n",
				    _("Could not get Tracker properties"),
				    error->message);
			g_error_free (error);
			
			return EXIT_FAILURE;
		}
		
		if (!array) {
			g_print ("%s\n",
				 _("No properties available"));
		} else {
			GList *l, *sorted = NULL;
			gchar **p;

			g_print ("%s:\n",
				 _("Properties available in Tracker"));

			for (p = array; *p; p++) {
				sorted = g_list_insert_sorted (sorted, 
							       *p,
							       (GCompareFunc) strcmp);
			}
			
			for (l = sorted; l; l = g_list_next (l)) {
				g_print ("  %s\n", (const gchar*) l->data);
			}

			g_list_free (sorted);
			g_strfreev (array);
		}
		
	}

	tracker_disconnect (client);

	return EXIT_SUCCESS;
}
