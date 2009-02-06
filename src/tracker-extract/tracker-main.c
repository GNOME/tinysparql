/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#define _XOPEN_SOURCE
#include <time.h>
#include <stdlib.h>
#include <locale.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n.h>

#ifndef G_OS_WIN32
#include <sys/resource.h>
#endif

#include <libtracker-common/tracker-os-dependant.h>
#include <libtracker-common/tracker-thumbnailer.h>

#include "tracker-main.h"
#include "tracker-dbus.h"
#include "tracker-extract.h"

static GMainLoop *main_loop;
static guint      shutdown_timeout_id;

static gboolean
shutdown_timeout_cb (gpointer user_data)
{
	shutdown_timeout_id = 0;
	g_main_loop_quit (main_loop);

	return FALSE;
}

void
tracker_main_shutdown_timeout_reset (void)
{
	if (shutdown_timeout_id != 0) {
		g_source_remove (shutdown_timeout_id);
	}

	shutdown_timeout_id = g_timeout_add_seconds (30, 
						     shutdown_timeout_cb, 
						     NULL);
}

static void
initialize_directories (void)
{
	gchar *user_data_dir;

	/* NOTE: We don't create the database directories here, the
	 * tracker-db-manager does that for us.
	 */
	
	user_data_dir = g_build_filename (g_get_user_data_dir (),
					  "tracker",
					  NULL);
	
	g_message ("Checking directory exists:'%s'", user_data_dir);
	g_mkdir_with_parents (user_data_dir, 00755);

	g_free (user_data_dir);
}

int
main (int argc, char *argv[])
{
	GOptionContext *context;
	gchar          *summary;
	TrackerConfig  *config;
	gchar          *log_filename;

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* Translators: this messagge will apper immediately after the	*/
	/* usage string - Usage: COMMAND [OPTION]... <THIS_MESSAGE>	*/
	context = g_option_context_new (_("- Extract file meta data"));

	/* Translators: this message will appear after the usage string */
	/* and before the list of options.				*/
	summary = g_strconcat (_("This command works two ways:"),
			       "\n",
			       "\n",
			       _(" - Calling the DBus API once running"),
			       "\n",
			       _(" - Passing arguments:"),
			       "\n",
			       "     tracker-extract [filename] [mime-type]\n",
			       NULL);

	g_option_context_set_summary (context, summary);
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	g_free (summary);

	tracker_memory_setrlimits ();

	g_type_init ();

	if (!g_thread_supported ()) {
		g_thread_init (NULL);
	}

	dbus_g_thread_init ();

	g_set_application_name ("tracker-extract");

	setlocale (LC_ALL, "");

	if (argc >= 2) {
		TrackerExtract *object;

		object = tracker_extract_new ();
		if (!object) {
			return EXIT_FAILURE;
		}

		if (argc >= 3) {
			tracker_extract_get_metadata_by_cmdline (object, argv[1], argv[2]);
		} else {
			tracker_extract_get_metadata_by_cmdline (object, argv[1], NULL);
		}

		g_object_unref (object);

		return EXIT_SUCCESS;
	}

	config = tracker_config_new ();

	log_filename =
		g_build_filename (g_get_user_data_dir (),
				  "tracker",
				  "tracker-extract.log",
				  NULL);

	/* Initialize subsystems */
	initialize_directories ();

	tracker_log_init (log_filename, tracker_config_get_verbosity (config));
	g_print ("Starting log:\n  File:'%s'\n", log_filename);

	tracker_thumbnailer_init (config, 0);
	tracker_dbus_init ();

	/* Make Tracker available for introspection */
	if (!tracker_dbus_register_objects ()) {
		g_free (log_filename);
		g_object_unref (config);

		return EXIT_FAILURE;
	}

	/* Main loop */
	main_loop = g_main_loop_new (NULL, FALSE);
	tracker_main_shutdown_timeout_reset ();
	g_main_loop_run (main_loop);
	g_main_loop_unref (main_loop);

	/* Shutdown subsystems */
	tracker_dbus_shutdown ();
	tracker_thumbnailer_shutdown ();
	tracker_log_shutdown ();

	g_free (log_filename);
	g_object_unref (config);

	return EXIT_SUCCESS;
}
