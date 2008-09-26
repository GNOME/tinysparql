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

static void
get_meta_table_data (gpointer value)
{
	gchar **meta;
	gchar **p;
	gint	i;

	meta = value;

	for (p = meta, i = 0; *p; p++, i++) {
		if (i == 0) {
			g_print ("  %s", *p);
		} else {
			g_print (" = %s", *p);
		}
	}

	g_print ("\n");
}

int
main (int argc, char **argv)
{
	TrackerClient  *client;
	GOptionContext *context;
	GPtrArray      *array;
	GError	       *error = NULL;

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* Translators: this messagge will apper immediately after the	*/
	/* usage string - Usage: COMMAND [OPTION]... <THIS_MESSAGE>	*/
	context = g_option_context_new (_(" - Show number of indexed files for each service"));
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	client = tracker_connect (FALSE);

	if (!client) {
		g_printerr ("%s\n",
			    _("Could not establish a DBus connection to Tracker"));
		return EXIT_FAILURE;
	}

	array = tracker_get_stats (client, &error);

	if (error) {
		g_printerr ("%s, %s\n",
			    _("Could not get Tracker statistics"),
			    error->message);
		g_error_free (error);

		return EXIT_FAILURE;
	}

	if (!array) {
		g_print ("%s\n",
			 _("No statistics available"));
	} else {
		g_print ("%s\n",
			 _("Statistics:"));

		g_ptr_array_foreach (array, (GFunc) get_meta_table_data, NULL);
		g_ptr_array_free (array, TRUE);
	}

	tracker_disconnect (client);

	return EXIT_SUCCESS;
}
