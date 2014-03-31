/*
 * Copyright (C) 2014, Lanedo GmbH. <martyn@lanedo.com>

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
#include <signal.h>
#include <errno.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n.h>

#include <libtracker-common/tracker-common.h>

#include <libtracker-miner/tracker-miner.h>

#include "tracker-miner-user-guides.h"

#define ABOUT	  \
	"Tracker " PACKAGE_VERSION "\n"

#define LICENSE	  \
	"This program is free software and comes without any warranty.\n" \
	"It is licensed under version 2 or later of the General Public " \
	"License which can be viewed at:\n" \
	"\n" \
	"  http://www.gnu.org/licenses/gpl.txt\n"

static GMainLoop *main_loop;

static gint verbosity = -1;
static gboolean no_daemon;
static gboolean version;

static GOptionEntry entries[] = {
	{ "verbosity", 'v', 0,
	  G_OPTION_ARG_INT, &verbosity,
	  N_("Logging, 0 = errors only, "
	     "1 = minimal, 2 = detailed and 3 = debug (default=0)"),
	  NULL },
	{ "no-daemon", 'n', 0,
	  G_OPTION_ARG_NONE, &no_daemon,
	  N_("Runs until all applications are indexed and then exits"),
	  NULL },
	{ "version", 'V', 0,
	  G_OPTION_ARG_NONE, &version,
	  N_("Displays version information"),
	  NULL },
	{ NULL }
};

static void
signal_handler (int signo)
{
	static gboolean in_loop = FALSE;

	/* Die if we get re-entrant signals handler calls */
	if (in_loop) {
		_exit (EXIT_FAILURE);
	}

	switch (signo) {
	case SIGTERM:
	case SIGINT:
		in_loop = TRUE;
		if (main_loop != NULL) {
			g_main_loop_quit (main_loop);
		} else {
			exit (0);
		}
		/* Fall through */
	default:
		if (g_strsignal (signo)) {
			g_print ("\n");
			g_print ("Received signal:%d->'%s'\n",
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
	sigset_t         empty_mask;

	sigemptyset (&empty_mask);
	act.sa_handler = signal_handler;
	act.sa_mask    = empty_mask;
	act.sa_flags   = 0;

	sigaction (SIGTERM, &act, NULL);
	sigaction (SIGINT,  &act, NULL);
	sigaction (SIGHUP,  &act, NULL);
#endif /* G_OS_WIN32 */
}

static void
initialize_priority_and_scheduling (TrackerSchedIdle sched_idle,
                                    gboolean         first_time_index)
{
	/* Set CPU priority */
	if (sched_idle == TRACKER_SCHED_IDLE_ALWAYS ||
	    (sched_idle == TRACKER_SCHED_IDLE_FIRST_INDEX && first_time_index)) {
		tracker_sched_idle ();
	}

	/* Set disk IO priority and scheduling */
	tracker_ioprio_init ();

	/* Set process priority:
	 * The nice() function uses attribute "warn_unused_result" and
	 * so complains if we do not check its returned value. But it
	 * seems that since glibc 2.2.4, nice() can return -1 on a
	 * successful call so we have to check value of errno too.
	 * Stupid...
	 */

	g_message ("Setting priority nice level to 19");

	errno = 0;
	if (nice (19) == -1 && errno != 0) {
		const gchar *str = g_strerror (errno);

		g_message ("Couldn't set nice value to 19, %s",
		           str ? str : "no error given");
	}
}

static void
miner_finished_cb (TrackerMinerFS *fs,
                   gdouble         seconds_elapsed,
                   guint           total_directories_found,
                   guint           total_directories_ignored,
                   guint           total_files_found,
                   guint           total_files_ignored,
                   gpointer        user_data)
{
	tracker_info ("Finished mining in seconds:%f, total directories:%d, total files:%d",
	              seconds_elapsed,
	              total_directories_found,
	              total_files_found);

	if (no_daemon && main_loop) {
		g_main_loop_quit (main_loop);
	}
}

static gboolean
miner_start_cb (gpointer user_data)
{
	TrackerMiner *miner = user_data;

	tracker_miner_start (miner);

	return FALSE;
}

int
main (gint argc, gchar *argv[])
{
	TrackerMiner *miner_user_guides;
	GOptionContext *context;
	GError *error = NULL;
	gchar *log_filename = NULL;

	main_loop = NULL;

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* Set timezone info */
	tzset ();

	/* Translators: this messagge will apper immediately after the
	 * usage string - Usage: COMMAND <THIS_MESSAGE>
	 */
	context = g_option_context_new (_("- start the user guides data miner"));

	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, &error);
	g_option_context_free (context);

	if (error) {
		g_printerr ("%s\n", error->message);
		g_error_free (error);
		return EXIT_FAILURE;
	}

	if (version) {
		g_print ("\n" ABOUT "\n" LICENSE "\n");
		return EXIT_SUCCESS;
	}

	initialize_signal_handler ();

	tracker_log_init (verbosity, &log_filename);
	if (log_filename) {
		g_message ("Using log file:'%s'", log_filename);
		g_free (log_filename);
	}

	/* This makes sure we don't steal all the system's resources */
	initialize_priority_and_scheduling (TRACKER_SCHED_IDLE_ALWAYS, FALSE);

	main_loop = g_main_loop_new (NULL, FALSE);

	g_message ("Checking if we're running as a daemon:");
	g_message ("  %s %s",
	           no_daemon ? "No" : "Yes",
	           no_daemon ? "(forced by command line)" : "");

	/* Create miner for userguides */
	miner_user_guides = tracker_miner_userguides_new (&error);
	if (!miner_user_guides) {
		g_critical ("Couldn't create new user guides miner, '%s'",
		            error ? error->message : "unknown error");
		tracker_log_shutdown ();
		return EXIT_FAILURE;
	}

	tracker_miner_fs_set_mtime_checking (TRACKER_MINER_FS (miner_user_guides), TRUE);

	g_signal_connect (miner_user_guides, "finished",
	                  G_CALLBACK (miner_finished_cb),
	                  NULL);

	/* Go, go, go! */
	g_idle_add (miner_start_cb, miner_user_guides);
	g_main_loop_run (main_loop);

	g_message ("Shutdown started");

	g_main_loop_unref (main_loop);
	g_object_unref (G_OBJECT (miner_user_guides));

	tracker_log_shutdown ();

	g_print ("\nOK\n\n");

	return EXIT_SUCCESS;
}
