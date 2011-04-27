/*
 * Copyright (C) 2009/2010, Roberto Guido <madbob@users.barberaware.org>
 *                          Michele Tameni <michele@amdplanet.it>
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

#include <stdlib.h>

#include <locale.h>
#include <glib/gi18n.h>

#include <libtracker-common/tracker-common.h>

#include "tracker-miner-rss.h"

static gint verbosity = -1;
static gchar *add_feed;
static gchar *title;

static GOptionEntry entries[] = {
	{ "verbosity", 'v', 0,
	  G_OPTION_ARG_INT, &verbosity,
	  N_("Logging, 0 = errors only, "
	  "1 = minimal, 2 = detailed and 3 = debug (default=0)"),
	  NULL },
	{ "add-feed", 'a', 0,
	  G_OPTION_ARG_STRING, &add_feed,
	  N_("Add feed (must be used with --title)"),
	  N_("URL") },
	{ "title", 't', 0,
	  G_OPTION_ARG_STRING, &title,
	  N_("Title to use (must be used with --add-feed)"),
	  NULL },
	{ NULL }
};

int
main (int argc, char **argv)
{
	gchar *log_filename;
	GMainLoop *loop;
	GOptionContext *context;
	TrackerMinerRSS *miner;
	GError *error = NULL;
	const gchar *error_message;

	g_type_init ();
	g_thread_init (NULL);

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
	tzset ();

	/* Translators: this messagge will apper immediately after the
	 * usage string - Usage: COMMAND <THIS_MESSAGE>
	 */
	context = g_option_context_new (_("- start the feeds indexer"));
	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);

	if ((add_feed && !title) || (!add_feed && title)) {
		error_message = _("Adding a feed requires --add-feed and --title");
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

	if (add_feed && title) {
		TrackerSparqlConnection *connection;
		const gchar *query;

		g_print ("Adding feed:\n"
		         "  title:'%s'\n"
		         "  url:'%s'\n",
		         title,
		         add_feed);

		connection = tracker_sparql_connection_get (NULL, &error);

		if (!connection) {
			g_printerr ("%s: %s\n",
			            _("Could not establish a connection to Tracker"),
			            error ? error->message : _("No error given"));
			g_clear_error (&error);
			return EXIT_FAILURE;
		}

		/* FIXME: Make interval configurable */
		query = g_strdup_printf ("INSERT {"
		                         "  _:FeedSettings a mfo:FeedSettings ;"
		                         "                   mfo:updateInterval 20 ."
		                         "  _:Feed a nie:DataObject, mfo:FeedChannel ;"
		                         "           mfo:feedSettings _:FeedSettings ;"
		                         "           nie:url \"%s\" ;"
		                         "           nie:title \"%s\" . "
		                         "}",
		                         add_feed,
		                         title);

		tracker_sparql_connection_update (connection,
		                                  query,
		                                  G_PRIORITY_DEFAULT,
		                                  NULL,
		                                  &error);

		if (error) {
			g_printerr ("%s, %s\n",
			            _("Could not add feed"),
			            error->message);
			g_error_free (error);
			g_object_unref (connection);

			return EXIT_FAILURE;
		}

		g_print ("Done\n");

		return EXIT_SUCCESS;
	}

	tracker_log_init (verbosity, &log_filename);
	g_print ("Starting log:\n  File:'%s'\n", log_filename);
	g_free (log_filename);

	miner = tracker_miner_rss_new (&error);
	if (!miner) {
		g_printerr ("Cannot create new RSS miner: '%s', exiting...\n",
		            error ? error->message : "unknown error");
		return EXIT_FAILURE;
	}

	tracker_miner_start (TRACKER_MINER (miner));

	loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (loop);

	tracker_log_shutdown ();
	g_main_loop_unref (loop);
	g_object_unref (miner);

	return EXIT_SUCCESS;
}
