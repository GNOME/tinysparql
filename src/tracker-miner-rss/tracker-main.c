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

static GOptionEntry entries[] = {
	{ "verbosity", 'v', 0,
	  G_OPTION_ARG_INT, &verbosity,
	  N_("Logging, 0 = errors only, "
	  "1 = minimal, 2 = detailed and 3 = debug (default=0)"),
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
	g_option_context_free (context);

	tracker_log_init (verbosity, &log_filename);
	g_print ("Starting log:\n  File:'%s'\n", log_filename);
	g_free (log_filename);

	miner = g_object_new (TRACKER_TYPE_MINER_RSS, "name", "RSS", NULL);
	tracker_miner_start (TRACKER_MINER (miner));

	loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (loop);

	tracker_log_shutdown ();
	g_main_loop_unref (loop);
	g_object_unref (miner);

	return 0;
}
