/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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

#include <libtracker-sparql/tracker-sparql.h>

#define ABOUT \
	"Tracker " PACKAGE_VERSION "\n"

#define LICENSE \
	"This program is free software and comes without any warranty.\n" \
	"It is licensed under version 2 or later of the General Public " \
	"License which can be viewed at:\n" \
	"\n" \
	"  http://www.gnu.org/licenses/gpl.txt\n"

static gboolean print_version;

static GOptionEntry entries[] = {
	{ "version", 'V', 0, G_OPTION_ARG_NONE, &print_version,
	  N_("Print version"),
	  NULL
	},
	{ NULL }
};

int
main (int argc, char **argv)
{
	TrackerSparqlConnection *connection;
	TrackerSparqlCursor *cursor;
	GOptionContext *context;
	GError *error = NULL;

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* Translators: this messagge will apper immediately after the  */
	/* usage string - Usage: COMMAND [OPTION]... <THIS_MESSAGE>     */
	context = g_option_context_new (_(" - Show statistics for all Nepomuk defined ontology classes"));
	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);

	if (print_version) {
		g_print ("\n" ABOUT "\n" LICENSE "\n");
		g_option_context_free (context);

		return EXIT_SUCCESS;
	}

	g_option_context_free (context);

	g_type_init ();

	if (!g_thread_supported ()) {
		g_thread_init (NULL);
	}

	connection = tracker_sparql_connection_get (NULL, &error);

	if (!connection) {
		g_printerr ("%s: %s\n",
		            _("Could not establish a connection to Tracker"),
		            error ? error->message : _("No error given"));
		g_clear_error (&error);
		return EXIT_FAILURE;
	}

	cursor = tracker_sparql_connection_statistics (connection, NULL, &error);

	if (error) {
		g_printerr ("%s, %s\n",
		            _("Could not get Tracker statistics"),
		            error->message);
		g_error_free (error);
		return EXIT_FAILURE;
	}

	if (!cursor) {
		g_print ("%s\n", _("No statistics available"));
	} else {
		gint count = 0;

		g_print ("%s\n", _("Statistics:"));

		while (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
			g_print ("  %s = %s\n",
			         tracker_sparql_cursor_get_string (cursor, 0, NULL),
			         tracker_sparql_cursor_get_string (cursor, 1, NULL));
			count++;
		}

		if (count == 0) {
			g_print ("  %s\n", _("None"));
		}

		g_print ("\n");

		g_object_unref (cursor);
	}

	g_object_unref (connection);

	return EXIT_SUCCESS;
}
