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
#include <signal.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n.h>

#ifndef G_OS_WIN32
#include <sys/resource.h>
#endif

#include <libtracker-common/tracker-log.h>
#include <libtracker-common/tracker-config.h>
#include <libtracker-common/tracker-dbus.h>
#include <libtracker-common/tracker-os-dependant.h>
#include <libtracker-common/tracker-thumbnailer.h>

#include "tracker-main.h"
#include "tracker-dbus.h"
#include "tracker-extract.h"

/* Temporary hack for out of date kernels, also, this value may not be
 * the same on all architectures, but it is for x86.
 */
#ifndef SCHED_IDLE
#define SCHED_IDLE 5
#endif

#define ABOUT								  \
	"Tracker " PACKAGE_VERSION "\n"

#define LICENSE								  \
	"This program is free software and comes without any warranty.\n" \
	"It is licensed under version 2 or later of the General Public "  \
	"License which can be viewed at:\n"				  \
	"\n"								  \
	"  http://www.gnu.org/licenses/gpl.txt\n"

#define QUIT_TIMEOUT 30 /* 1/2 minutes worth of seconds */

static GMainLoop *main_loop;
static guint      quit_timeout_id;

static gboolean   version;
static gint       verbosity = -1;
static gchar     *filename;
static gchar     *mime_type;

static GOptionEntry  entries[] = {
	{ "version", 'V', 0,
	  G_OPTION_ARG_NONE, &version,
	  N_("Displays version information"),
	  NULL },
	{ "verbosity", 'v', 0,
	  G_OPTION_ARG_INT, &verbosity,
	  N_("Logging, 0 = errors only, "
	     "1 = minimal, 2 = detailed and 3 = debug (default = 0)"),
	  NULL },
	{ "file", 'f', 0,
	  G_OPTION_ARG_STRING, &filename,
	  N_("File to extract metadata for"),
	  N_("FILE") },
	{ "file", 'm', 0,
	  G_OPTION_ARG_STRING, &mime_type,
	  N_("MIME type for file (if not provided, this will be guessed)"),
	  N_("MIME") },

	{ NULL }
};

static gboolean
quit_timeout_cb (gpointer user_data)
{
	quit_timeout_id = 0;
	g_main_loop_quit (main_loop);

	return FALSE;
}

void
tracker_main_quit_timeout_reset (void)
{
	if (quit_timeout_id != 0) {
		g_source_remove (quit_timeout_id);
	}

	quit_timeout_id = g_timeout_add_seconds (QUIT_TIMEOUT, 
						 quit_timeout_cb, 
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

static void
signal_handler (int signo)
{
	static gboolean in_loop = FALSE;

	/* Die if we get re-entrant signals handler calls */
	if (in_loop) {
		exit (EXIT_FAILURE);
	}

	switch (signo) {
	case SIGALRM:
		exit (EXIT_FAILURE);
		break;
	case SIGTERM:
	case SIGINT:
		in_loop = TRUE;
		quit_timeout_cb (NULL);
	default:
		if (g_strsignal (signo)) {
			g_print ("\n");
			g_print ("Received signal:%d->'%s'",
				 signo,
				 g_strsignal (signo));
		}
		break;
	}
}

static void
initialize_signal_handler (void)
{
#ifndef G_OS_WIN32
	struct sigaction act;
	sigset_t	 empty_mask;

	sigemptyset (&empty_mask);
	act.sa_handler = signal_handler;
	act.sa_mask    = empty_mask;
	act.sa_flags   = 0;

	sigaction (SIGTERM, &act, NULL);
	sigaction (SIGINT,  &act, NULL);
	sigaction (SIGHUP,  &act, NULL);
	sigaction (SIGALRM,  &act, NULL);
#endif /* G_OS_WIN32 */
}

int
main (int argc, char *argv[])
{
	GOptionContext *context;
	GError         *error = NULL;
	TrackerConfig  *config;
	gchar          *log_filename;

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* Translators: this message will appear immediately after the	*/
	/* usage string - Usage: COMMAND [OPTION]... <THIS_MESSAGE>	*/
	context = g_option_context_new (_("- Extract file meta data"));

	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, &error);

	if (!filename && mime_type) {
		gchar *help;

		g_printerr ("%s\n\n",
			    _("Filename and mime type must be provided together"));

		help = g_option_context_get_help (context, TRUE, NULL);
		g_option_context_free (context);
		g_printerr ("%s", help);
		g_free (help);

		return EXIT_FAILURE;
	}

	g_option_context_free (context);

	if (version) {
		g_print ("\n" ABOUT "\n" LICENSE "\n");
		return EXIT_SUCCESS;
	}

	g_print ("Initializing tracker-extract...\n");

	initialize_signal_handler ();

	tracker_memory_setrlimits ();

	g_type_init ();

	if (!g_thread_supported ()) {
		g_thread_init (NULL);
	}

	dbus_g_thread_init ();

	g_set_application_name ("tracker-extract");

	setlocale (LC_ALL, "");

	if (filename) {
		TrackerExtract *object;

		object = tracker_extract_new ();
		if (!object) {
			return EXIT_FAILURE;
		}

		tracker_extract_get_metadata_by_cmdline (object, filename, mime_type);

		g_object_unref (object);

		return EXIT_SUCCESS;
	}

	config = tracker_config_new ();

	log_filename =
		g_build_filename (g_get_user_data_dir (),
				  "tracker",
				  "tracker-extract.log",
				  NULL);

	/* Extractor command line arguments */
	if (verbosity > -1) {
		tracker_config_set_verbosity (config, verbosity);
	}

	tracker_dbus_init ();

	/* Initialize subsystems */
	initialize_directories ();

	tracker_log_init (log_filename, tracker_config_get_verbosity (config));
	g_print ("Starting log:\n  File:'%s'\n", log_filename);

	tracker_thumbnailer_init (config, 0);

	/* Make Tracker available for introspection */
	if (!tracker_dbus_register_objects ()) {
		g_free (log_filename);
		g_object_unref (config);

		return EXIT_FAILURE;
	}

	/* Main loop */
	main_loop = g_main_loop_new (NULL, FALSE);
	tracker_main_quit_timeout_reset ();
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
