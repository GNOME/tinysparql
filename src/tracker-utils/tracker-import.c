/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
 * Copyright (C) 2008-2009, Nokia <ivan.frade@nokia.com>
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
#include <stdint.h>

#include <glib.h>
#include <glib/gi18n.h>

#include <libtracker-client/tracker-client.h>
#include <libtracker-common/tracker-common.h>

#define ABOUT	  \
	"Tracker " PACKAGE_VERSION "\n"

#define LICENSE	  \
	"This program is free software and comes without any warranty.\n" \
	"It is licensed under version 2 or later of the General Public " \
	"License which can be viewed at:\n" \
	"\n" \
	"  http://www.gnu.org/licenses/gpl.txt\n"


static gchar        **filenames = NULL;
static gboolean       print_version;

static GOptionEntry   entries[] = {
	{ "version", 'V', 0, G_OPTION_ARG_NONE, &print_version,
	  N_("Print version"),
	  NULL,
	},
	{ G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &filenames,
	  N_("FILE"),
	  N_("FILE")},
	{ NULL }
};

int
main (int argc, char **argv)
{
	TrackerClient   *client;
	GOptionContext  *context;
	gchar          **p;

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* Translators: this messagge will apper immediately after the  */
	/* usage string - Usage: COMMAND [OPTION]... <THIS_MESSAGE>     */
	context = g_option_context_new (_("- Import data using Turtle files"));

	/* Translators: this message will appear after the usage string */
	/* and before the list of options.                              */
	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);

	if (print_version) {
		g_print ("\n" ABOUT "\n" LICENSE "\n");
		g_option_context_free (context);

		return EXIT_SUCCESS;
	}

	if (!filenames) {
		gchar *help;

		g_printerr ("%s\n\n",
		            _("One or more files have not been specified"));

		help = g_option_context_get_help (context, TRUE, NULL);
		g_option_context_free (context);
		g_printerr ("%s", help);
		g_free (help);

		return EXIT_FAILURE;
	}

	g_option_context_free (context);

	client = tracker_client_new (0, G_MAXINT);

	if (!client) {
		g_printerr ("%s\n",
		            _("Could not establish a D-Bus connection to Tracker"));
		return EXIT_FAILURE;
	}

	for (p = filenames; *p; p++) {
		GError *error = NULL;
		GFile  *file;
		gchar  *uri;

		g_print ("%s:'%s'\n",
		         _("Importing Turtle file"),
		         *p);

		file = g_file_new_for_commandline_arg (*p);
		uri = g_file_get_uri (file);

		tracker_resources_load (client, uri, &error);

		g_object_unref (file);
		g_free (uri);

		if (error) {
			g_printerr ("%s, %s\n",
			            _("Unable to import Turtle file"),
			            error->message);

			g_error_free (error);
			continue;
		}

		g_print ("  %s\n", _("Done"));
		g_print ("\n");
	}

	g_object_unref (client);

	return EXIT_SUCCESS;
}
