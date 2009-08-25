/* Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia

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

#include <string.h>
#include <stdlib.h>
#include <locale.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#if defined(__linux__)
#include <linux/sched.h>
#endif
#include <sched.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n.h>

#include <libtracker-common/tracker-ioprio.h>
#include <libtracker-common/tracker-log.h>
#include <libtracker-common/tracker-ontology.h>
#include <libtracker-common/tracker-file-utils.h>
#include <libtracker-common/tracker-thumbnailer.h>
#include <libtracker-common/tracker-storage.h>

#include <libtracker-db/tracker-db-manager.h>
#include <libtracker-db/tracker-db-dbus.h>

#include <libtracker-data/tracker-turtle.h>

#include "tracker-albumart.h"
#include "tracker-dbus.h"
#include "tracker-config.h"
#include "tracker-indexer.h"
#include "tracker-marshal.h"
#include "tracker-miner-applications.h"
#include "tracker-miner-files.h"

#define ABOUT								  \
	"Tracker " PACKAGE_VERSION "\n"

#define LICENSE								  \
	"This program is free software and comes without any warranty.\n" \
	"It is licensed under version 2 or later of the General Public "  \
	"License which can be viewed at:\n"				  \
	"\n"								  \
	"  http://www.gnu.org/licenses/gpl.txt\n"

static GMainLoop    *main_loop;
static GSList       *miners;
static GSList       *current_miner;

static gboolean      version;
static gint	     verbosity = -1;
static gint	     initial_sleep = -1;

static GOptionEntry  entries[] = {
	{ "version", 'V', 0,
	  G_OPTION_ARG_NONE, &version,
	  N_("Displays version information"),
	  NULL },
	{ "verbosity", 'v', 0,
	  G_OPTION_ARG_INT, &verbosity,
	  N_("Logging, 0 = errors only, "
	     "1 = minimal, 2 = detailed and 3 = debug (default = config)"),
	  NULL },
	{ "initial-sleep", 's', 0,
	  G_OPTION_ARG_INT, &initial_sleep,
	  N_("Initial sleep time in seconds, "
	     "0->1000 (default = config)"),
	  NULL },
	{ NULL }
};

static void
sanity_check_option_values (TrackerConfig *config)
{
	g_message ("General options:");
	g_message ("  Verbosity  ............................  %d",
		   tracker_config_get_verbosity (config));
	g_message ("  Initial Sleep  ........................  %d",
		   tracker_config_get_initial_sleep (config));

	g_message ("Indexer options:");
	g_message ("  Throttle level  .......................  %d",
		   tracker_config_get_throttle (config));
	g_message ("  Thumbnail indexing enabled  ...........  %s",
		   tracker_config_get_enable_thumbnails (config) ? "yes" : "no");
	g_message ("  Disable indexing on battery  ..........  %s (initially = %s)",
		   tracker_config_get_disable_indexing_on_battery (config) ? "yes" : "no",
		   tracker_config_get_disable_indexing_on_battery_init (config) ? "yes" : "no");

	if (tracker_config_get_low_disk_space_limit (config) == -1) {
		g_message ("  Low disk space limit  .................  Disabled");
	} else {
		g_message ("  Low disk space limit  .................  %d%%",
			   tracker_config_get_low_disk_space_limit (config));
	}
}

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
		g_main_loop_quit (main_loop);

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
	sigset_t	 empty_mask;

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
initialize_priority (void)
{
	/* Set disk IO priority and scheduling */
	tracker_ioprio_init ();

	/* Set process priority:
	 * The nice() function uses attribute "warn_unused_result" and
	 * so complains if we do not check its returned value. But it
	 * seems that since glibc 2.2.4, nice() can return -1 on a
	 * successful call so we have to check value of errno too.
	 * Stupid... 
	 */
	g_message ("Setting process priority");

	if (nice (19) == -1) {
		const gchar *str = g_strerror (errno);

		g_message ("Couldn't set nice value to 19, %s",
			   str ? str : "no error given");
	}
}

static void
miner_handle_next (void)
{
        if (!current_miner) {
                current_miner = miners;
        } else {
                current_miner = current_miner->next;
        }

        if (!current_miner) {
                return;
        }

        g_message ("Starting next miner...");

        tracker_miner_start (current_miner->data);
}

static void
miner_finished_cb (TrackerMinerProcess *miner_process,
                   gdouble              seconds_elapsed,
                   guint                total_directories_found,
                   guint                total_directories_ignored,
                   guint                total_files_found,
                   guint                total_files_ignored,
                   gpointer             user_data)
{
	g_message ("Finished mining in seconds:%f, total directories:%d, total files:%d",
                   seconds_elapsed,
                   total_directories_found + total_directories_ignored,
                   total_files_found + total_files_ignored);

        miner_handle_next ();
}

static void
daemon_availability_changed_cb (const gchar *name,
                                gboolean     available,
                                gpointer     user_data)
{
        if (!available) {
                /* tracker_indexer_stop (TRACKER_INDEXER (user_data)); */
        }
}

int
main (gint argc, gchar *argv[])
{
	TrackerConfig *config;
        TrackerMiner *miner_applications, *miner_files;
        TrackerStorage *storage;
	GOptionContext *context;
	GError *error = NULL;
	gchar *log_filename = NULL;

	g_type_init ();

	if (!g_thread_supported ()) {
		g_thread_init (NULL);
	}

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* Set timezone info */
	tzset ();

	/* Translators: this messagge will apper immediately after the
	 * usage string - Usage: COMMAND <THIS_MESSAGE>
	 */
	context = g_option_context_new (_("- start the tracker indexer"));

	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, &error);
	g_option_context_free (context);

        if (version) {
                g_print ("\n" ABOUT "\n" LICENSE "\n");
                return EXIT_SUCCESS;
        }

	g_print ("Initializing tracker-miner-fs...\n");

	initialize_signal_handler ();

	/* Check XDG spec locations XDG_DATA_HOME _MUST_ be writable. */
	if (!tracker_env_check_xdg_dirs ()) {
		return EXIT_FAILURE;
	}

	/* This makes sure we don't steal all the system's resources */
	initialize_priority ();

	/* Initialize logging */
	config = tracker_config_new ();

	if (verbosity > -1) {
		tracker_config_set_verbosity (config, verbosity);
	}

	if (initial_sleep > -1) {
		tracker_config_set_initial_sleep (config, initial_sleep);
	}

	/* Make sure we initialize DBus, this shows we are started
	 * successfully when called upon from the daemon.
	 */
#if 0
	if (!tracker_dbus_init ()) {
		return EXIT_FAILURE;
	}
#endif

	tracker_log_init (tracker_config_get_verbosity (config),
                          &log_filename);
	g_print ("Starting log:\n  File:'%s'\n", log_filename);
	g_free (log_filename);

	sanity_check_option_values (config);

	/* Set IO priority */
	tracker_ioprio_init ();

	/* nice() uses attribute "warn_unused_result" and so complains
	 * if we do not check its returned value. But it seems that
	 * since glibc 2.2.4, nice() can return -1 on a successful
	 * call so we have to check value of errno too. Stupid...
	 */
	if (nice (19) == -1 && errno) {
		const gchar *str;

		str = g_strerror (errno);
		g_message ("Couldn't set nice value to 19, %s",
			   str ? str : "no error given");
	}

#ifdef HAVE_HAL
	storage = tracker_storage_new ();
#else 
	storage = NULL;
#endif

        /* Create miner for applications */
        miner_applications = tracker_miner_applications_new ();
        miners = g_slist_append (miners, miner_applications);

        /* FIXME: use proper definition for applications dir */
        tracker_miner_process_add_directory (TRACKER_MINER_PROCESS (miner_applications),
                                             "/usr/share/applications/",
					     FALSE);

	g_signal_connect (miner_applications, "finished",
			  G_CALLBACK (miner_finished_cb),
			  NULL);

        /* Create miner for files */
        miner_files = tracker_miner_files_new (config);
        miners = g_slist_append (miners, miner_files);

	g_signal_connect (miner_files, "finished",
			  G_CALLBACK (miner_finished_cb),
			  NULL);

        miner_handle_next ();

	main_loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (main_loop);

	g_message ("Shutdown started");

	g_main_loop_unref (main_loop);
	g_object_unref (miner_applications);
	g_object_unref (config);

        if (storage) {
                g_object_unref (storage);
        }

        g_slist_foreach (miners, (GFunc) g_object_unref, NULL);
        g_slist_free (miners);
        
	tracker_log_shutdown ();

	g_print ("\nOK\n\n");

	return EXIT_SUCCESS;
}
