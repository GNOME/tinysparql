/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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
#include <gio/gio.h>

#ifndef G_OS_WIN32
#include <sys/resource.h>
#endif

#include <libtracker-common/tracker-log.h>
#include <libtracker-common/tracker-dbus.h>
#include <libtracker-common/tracker-os-dependant.h>
#include <libtracker-common/tracker-ioprio.h>

#include "tracker-albumart.h"
#include "tracker-config.h"
#include "tracker-main.h"
#include "tracker-dbus.h"
#include "tracker-extract.h"

#define ABOUT	  \
	"Tracker " PACKAGE_VERSION "\n"

#define LICENSE	  \
	"This program is free software and comes without any warranty.\n" \
	"It is licensed under version 2 or later of the General Public " \
	"License which can be viewed at:\n" \
	"\n" \
	"  http://www.gnu.org/licenses/gpl.txt\n"

#define QUIT_TIMEOUT 30 /* 1/2 minutes worth of seconds */

static GMainLoop *main_loop;
static guint quit_timeout_id = 0;

static gint verbosity = -1;
static gchar *filename;
static gchar *mime_type;
static gboolean disable_shutdown;
static gboolean force_internal_extractors;
static gchar *force_module;
static gboolean version;

static TrackerConfig *config;

static GOptionEntry entries[] = {
	{ "verbosity", 'v', 0,
	  G_OPTION_ARG_INT, &verbosity,
	  N_("Logging, 0 = errors only, "
	     "1 = minimal, 2 = detailed and 3 = debug (default = 0)"),
	  NULL },
	{ "file", 'f', 0,
	  G_OPTION_ARG_FILENAME, &filename,
	  N_("File to extract metadata for"),
	  N_("FILE") },
	{ "mime", 't', 0,
	  G_OPTION_ARG_STRING, &mime_type,
	  N_("MIME type for file (if not provided, this will be guessed)"),
	  N_("MIME") },
	/* Debug run is used to avoid that the mainloop exits, so that
	 * as a developer you can be relax when running the tool in gdb */
	{ "disable-shutdown", 'd', 0,
	  G_OPTION_ARG_NONE, &disable_shutdown,
	  N_("Disable shutting down after 30 seconds of inactivity"),
	  NULL },
	{ "force-internal-extractors", 'i', 0,
	  G_OPTION_ARG_NONE, &force_internal_extractors,
	  N_("Force internal extractors over 3rd parties like libstreamanalyzer"),
	  NULL },
	{ "force-module", 'm', 0,
	  G_OPTION_ARG_STRING, &force_module,
	  N_("Force a module to be used for extraction (e.g. \"foo\" for \"foo.so\")"),
	  N_("MODULE") },
	{ "version", 'V', 0,
	  G_OPTION_ARG_NONE, &version,
	  N_("Displays version information"),
	  NULL },
	{ NULL }
};

static gboolean
quit_timeout_cb (gpointer user_data)
{
	quit_timeout_id = 0;

	if (!disable_shutdown) {
		if (main_loop) {
			g_main_loop_quit (main_loop);
		}
	} else {
		g_debug ("Would have quit the mainloop");
	}

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
initialize_directories (void)
{
	gchar *user_data_dir;

	/* NOTE: We don't create the database directories here, the
	 * tracker-db-manager does that for us.
	 */

	user_data_dir = g_build_filename (g_get_user_data_dir (),
	                                  "tracker",
	                                  NULL);

	/* g_message ("Checking directory exists:'%s'", user_data_dir); */
	g_mkdir_with_parents (user_data_dir, 00755);

	g_free (user_data_dir);
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
	case SIGABRT:
	case SIGALRM:
		_exit (EXIT_FAILURE);
		break;
	case SIGTERM:
	case SIGINT:
		in_loop = TRUE;
		disable_shutdown = FALSE;
		quit_timeout_cb (NULL);
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
	sigaction (SIGALRM,  &act, NULL);
	sigaction (SIGABRT, &act, NULL);
#endif /* G_OS_WIN32 */
}

static void
log_handler (const gchar    *domain,
             GLogLevelFlags  log_level,
             const gchar    *message,
             gpointer        user_data)
{
	if (!tracker_log_should_handle (log_level, verbosity)) {
		return;
	}

	switch (log_level) {
	case G_LOG_LEVEL_WARNING:
	case G_LOG_LEVEL_CRITICAL:
	case G_LOG_LEVEL_ERROR:
	case G_LOG_FLAG_RECURSION:
	case G_LOG_FLAG_FATAL:
		g_fprintf (stderr, "%s\n", message);
		fflush (stderr);
		break;
	case G_LOG_LEVEL_MESSAGE:
	case G_LOG_LEVEL_INFO:
	case G_LOG_LEVEL_DEBUG:
	case G_LOG_LEVEL_MASK:
	default:
		g_fprintf (stdout, "%s\n", message);
		fflush (stdout);
		break;
	}
}

TrackerConfig *
tracker_main_get_config (void)
{
	return config;
}


static int
run_standalone (void)
{
	TrackerExtract *object;
	GFile *file;
	gchar *uri;
	guint log_handler_id;

	/* Set log handler for library messages */
	log_handler_id = g_log_set_handler (NULL,
	                                    G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL,
	                                    log_handler,
	                                    NULL);

	g_log_set_default_handler (log_handler, NULL);

	/* Set the default verbosity if unset */
	if (verbosity == -1) {
		verbosity = 3;
	}

	tracker_albumart_init ();

	/* This makes sure we don't steal all the system's resources */
	initialize_priority ();

	file = g_file_new_for_commandline_arg (filename);
	uri = g_file_get_uri (file);

	object = tracker_extract_new (disable_shutdown,
	                              force_internal_extractors,
	                              force_module);

	if (!object) {
		g_free (uri);
		return EXIT_FAILURE;
	}

	tracker_memory_setrlimits ();

	tracker_extract_get_metadata_by_cmdline (object, uri, mime_type);

	g_object_unref (object);
	g_object_unref (file);
	g_free (uri);

	if (log_handler_id != 0) {
		/* Unset log handler */
		g_log_remove_handler (NULL, log_handler_id);
	}

	tracker_albumart_shutdown ();

	return EXIT_SUCCESS;
}

int
main (int argc, char *argv[])
{
	GOptionContext *context;
	GError         *error = NULL;
	TrackerExtract *object;
	gchar          *log_filename = NULL;
	GMainLoop      *my_main_loop;

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* Translators: this message will appear immediately after the  */
	/* usage string - Usage: COMMAND [OPTION]... <THIS_MESSAGE>     */
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

	if (force_internal_extractors && force_module) {
		gchar *help;

		g_printerr ("%s\n\n",
		            _("Options --force-internal-extractors and --force-module can't be used together"));

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

	if (!filename) {
		g_print ("  Shutdown after 30 seconds of inactivitiy is %s\n",
		         disable_shutdown ? "disabled" : "enabled");
	}

	initialize_signal_handler ();

	g_type_init ();

	if (!g_thread_supported ()) {
		g_thread_init (NULL);
	}

	dbus_g_thread_init ();

	g_set_application_name ("tracker-extract");

	setlocale (LC_ALL, "");

	config = tracker_config_new ();

	/* Set conditions when we use stand alone settings */
	if (filename) {
		return run_standalone ();
	}

	/* Initialize subsystems */
	initialize_directories ();

	/* Extractor command line arguments */
	if (verbosity > -1) {
		tracker_config_set_verbosity (config, verbosity);
	}

	tracker_log_init (tracker_config_get_verbosity (config), &log_filename);
	g_print ("Starting log:\n  File:'%s'\n", log_filename);
	g_free (log_filename);

	/* This makes sure we don't steal all the system's resources */
	initialize_priority ();

	if (!tracker_dbus_init ()) {
		g_object_unref (config);
		tracker_log_shutdown ();

		return EXIT_FAILURE;
	}

	object = tracker_extract_new (disable_shutdown,
	                              force_internal_extractors,
	                              force_module);

	if (!object) {
		g_object_unref (config);
		tracker_log_shutdown ();

		return EXIT_FAILURE;
	}

	tracker_memory_setrlimits ();

	/* Make Tracker available for introspection */
	if (!tracker_dbus_register_objects (object)) {
		g_object_unref (object);
		g_object_unref (config);
		tracker_log_shutdown ();

		return EXIT_FAILURE;
	}

#ifdef HAVE_DBUS_FD_PASSING
	tracker_dbus_connection_add_filter (tracker_extract_connection_filter,
	                                    object);
#endif

	g_message ("Waiting for D-Bus requests...");

	tracker_albumart_init ();

	/* Main loop */
	main_loop = g_main_loop_new (NULL, FALSE);
	tracker_main_quit_timeout_reset ();
	g_main_loop_run (main_loop);

	my_main_loop = main_loop;
	main_loop = NULL;
	g_main_loop_unref (my_main_loop);

	g_message ("Shutdown started");

	/* Shutdown subsystems */
	tracker_albumart_shutdown ();
	tracker_dbus_shutdown ();
	tracker_log_shutdown ();

	g_object_unref (config);

	return EXIT_SUCCESS;
}
