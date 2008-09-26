/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
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

#ifndef DBUS_API_SUBJECT_TO_CHANGE
#define DBUS_API_SUBJECT_TO_CHANGE
#endif

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <locale.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <libtracker-common/tracker-config.h>
#include <libtracker-common/tracker-file-utils.h>
#include <libtracker-common/tracker-hal.h>
#include <libtracker-common/tracker-ioprio.h>
#include <libtracker-common/tracker-language.h>
#include <libtracker-common/tracker-log.h>
#include <libtracker-common/tracker-module-config.h>
#include <libtracker-common/tracker-nfs-lock.h>
#include <libtracker-common/tracker-ontology.h>

#include <libtracker-db/tracker-db-manager.h>
#include <libtracker-db/tracker-db-index.h>
#include <libtracker-db/tracker-db-index-manager.h>

#include "tracker-crawler.h"
#include "tracker-dbus.h"
#include "tracker-indexer-client.h"
#include "tracker-main.h"
#include "tracker-monitor.h"
#include "tracker-processor.h"
#include "tracker-status.h"
#include "tracker-xesam-manager.h"

#ifdef G_OS_WIN32
#include <windows.h>
#include <pthread.h>
#include "mingw-compat.h"
#endif

#define ABOUT								  \
	"Tracker " VERSION "\n"						  \
	"Copyright (c) 2005-2008 Jamie McCracken (jamiemcc@gnome.org)\n"

#define LICENSE								  \
	"This program is free software and comes without any warranty.\n" \
	"It is licensed under version 2 or later of the General Public "  \
	"License which can be viewed at:\n"				  \
	"\n"								  \
	"  http://www.gnu.org/licenses/gpl.txt\n"

/* Throttle defaults */
#define THROTTLE_DEFAULT	    0
#define THROTTLE_DEFAULT_ON_BATTERY 5

typedef enum {
	TRACKER_RUNNING_NON_ALLOWED,
	TRACKER_RUNNING_READ_ONLY,
	TRACKER_RUNNING_MAIN_INSTANCE
} TrackerRunningLevel;

typedef struct {
	GMainLoop	 *main_loop;
	gchar		 *log_filename;

	gchar		 *data_dir;
	gchar		 *user_data_dir;
	gchar		 *sys_tmp_dir;

	gboolean	  reindex_on_shutdown;

	TrackerProcessor *processor;
} TrackerMainPrivate;

/* Private */
static GStaticPrivate	private_key = G_STATIC_PRIVATE_INIT;

/* Private command line parameters */
static gint		verbosity = -1;
static gint		initial_sleep = -1;
static gboolean		low_memory;
static gchar	      **monitors_to_exclude;
static gchar	      **monitors_to_include;
static gchar	      **crawl_dirs;
static gchar	      **disable_modules;

static gboolean		force_reindex;
static gboolean		disable_indexing;
static gchar	       *language_code;

static GOptionEntry	entries_daemon[] = {
	{ "verbosity", 'v', 0,
	  G_OPTION_ARG_INT, &verbosity,
	  N_("Logging, 0 = errors only, "
	     "1 = minimal, 2 = detailed and 3 = debug (default = 0)"),
	  NULL },
	{ "initial-sleep", 's', 0,
	  G_OPTION_ARG_INT, &initial_sleep,
	  N_("Seconds to wait before starting any crawling or indexing (default = 45)"),
	  NULL },
	{ "low-memory", 'm', 0,
	  G_OPTION_ARG_NONE, &low_memory,
	  N_("Minimizes the use of memory but may slow indexing down"),
	  NULL },
	{ "monitors-exclude-dirs", 'e', 0,
	  G_OPTION_ARG_STRING_ARRAY, &monitors_to_exclude,
	  N_("Directories to exclude for file change monitoring (you can do -e <path> -e <path>)"),
	  NULL },
	{ "monitors-include-dirs", 'i', 0,
	  G_OPTION_ARG_STRING_ARRAY, &monitors_to_include,
	  N_("Directories to include for file change monitoring (you can do -i <path> -i <path>)"),
	  NULL },
	{ "crawler-include-dirs", 'c', 0,
	  G_OPTION_ARG_STRING_ARRAY, &crawl_dirs,
	  N_("Directories to crawl to index files (you can do -c <path> -c <path>)"),
	  NULL },
	{ "disable-modules", 'd', 0,
	  G_OPTION_ARG_STRING_ARRAY, &disable_modules,
	  N_("Disable modules from being processed (you can do -d <module> -d <module)"),
	  NULL },
	{ NULL }
};

static GOptionEntry   entries_indexer[] = {
	{ "force-reindex", 'r', 0,
	  G_OPTION_ARG_NONE, &force_reindex,
	  N_("Force a re-index of all content"),
	  NULL },
	{ "disable-indexing", 'n', 0,
	  G_OPTION_ARG_NONE, &disable_indexing,
	  N_("Disable any indexing and monitoring"), NULL },
	{ "language", 'l', 0,
	  G_OPTION_ARG_STRING, &language_code,
	  N_("Language to use for stemmer and stop words "
	     "(ISO 639-1 2 characters code)"),
	  NULL },
	{ NULL }
};

static void
private_free (gpointer data)
{
	TrackerMainPrivate *private;

	private = data;

	if (private->processor) {
		g_object_unref (private->processor);
	}

	g_free (private->sys_tmp_dir);
	g_free (private->user_data_dir);
	g_free (private->data_dir);

	g_free (private->log_filename);

	g_main_loop_unref (private->main_loop);

	g_free (private);
}

static gchar *
get_lock_file (void)
{
	TrackerMainPrivate *private;
	gchar		   *lock_filename;
	gchar		   *filename;

	private = g_static_private_get (&private_key);

	filename = g_strconcat (g_get_user_name (), "_tracker_lock", NULL);

	lock_filename = g_build_filename (private->sys_tmp_dir,
					  filename,
					  NULL);
	g_free (filename);

	return lock_filename;
}

static TrackerRunningLevel
check_runtime_level (TrackerConfig *config,
		     TrackerHal    *hal)
{
	TrackerRunningLevel  runlevel;
	gchar		    *lock_file;
	gboolean	     use_nfs;
	gint		     fd;

	g_message ("Checking instances running...");

	if (!tracker_config_get_enable_indexing (config)) {
		g_message ("Indexing disabled, running in read-only mode");
		return TRACKER_RUNNING_READ_ONLY;
	}

	use_nfs = tracker_config_get_nfs_locking (config);

	lock_file = get_lock_file ();
	fd = g_open (lock_file, O_RDWR | O_CREAT, 0640);

	if (fd == -1) {
		const gchar *error_string;

		error_string = g_strerror (errno);
		g_critical ("Can not open or create lock file:'%s', %s",
			    lock_file,
			    error_string);
		g_free (lock_file);

		return TRACKER_RUNNING_NON_ALLOWED;
	}

	g_free (lock_file);

	if (lockf (fd, F_TLOCK, 0) < 0) {
		if (use_nfs) {
			g_message ("Already running, running in "
				   "read-only mode (with NFS)");
			runlevel = TRACKER_RUNNING_READ_ONLY;
		} else {
			g_message ("Already running, not allowed "
				   "multiple instances (without NFS)");
			runlevel = TRACKER_RUNNING_NON_ALLOWED;
		}
	} else {
		g_message ("This is the first/main instance");

		runlevel = TRACKER_RUNNING_MAIN_INSTANCE;

#ifdef HAVE_HAL
		if (!tracker_hal_get_battery_exists (hal) ||
		    !tracker_hal_get_battery_in_use (hal)) {
			return TRACKER_RUNNING_MAIN_INSTANCE;
		}

		if (!tracker_status_get_is_first_time_index () &&
		    tracker_config_get_disable_indexing_on_battery (config)) {
			g_message ("Battery in use");
			g_message ("Config is set to not index on battery");
			g_message ("Running in read only mode");
			runlevel = TRACKER_RUNNING_READ_ONLY;
		}

		/* Special case first time situation which are
		 * overwritten by the config option to disable or not
		 * indexing on battery initially.
		 */
		if (tracker_status_get_is_first_time_index () &&
		    tracker_config_get_disable_indexing_on_battery_init (config)) {
			g_message ("Battery in use & reindex is needed");
			g_message ("Config is set to not index on battery for initial index");
			g_message ("Running in read only mode");
			runlevel = TRACKER_RUNNING_READ_ONLY;
		}
#endif /* HAVE_HAL */
	}

	return runlevel;
}

static void
log_option_list (GSList      *list,
		 const gchar *str)
{
	GSList *l;

	g_message ("%s:", str);

	if (!list) {
		g_message ("  DEFAULT");
		return;
	}

	for (l = list; l; l = l->next) {
		g_message ("  %s", (gchar*) l->data);
	}
}

static void
sanity_check_option_values (TrackerConfig *config)
{
	g_message ("General options:");
	g_message ("  Initial sleep  ........................  %d (seconds)",
		   tracker_config_get_initial_sleep (config));
	g_message ("  Verbosity  ............................  %d",
		   tracker_config_get_verbosity (config));
	g_message ("  Low memory mode  ......................  %s",
		   tracker_config_get_low_memory_mode (config) ? "yes" : "no");


	g_message ("Daemon options:");
	g_message ("  Throttle level  .......................  %d",
		   tracker_config_get_throttle (config));
	g_message ("  Indexing enabled	.....................  %s",
		   tracker_config_get_enable_indexing (config) ? "yes" : "no");
	g_message ("  Monitoring enabled  ...................  %s",
		   tracker_config_get_enable_watches (config) ? "yes" : "no");

	log_option_list (tracker_config_get_watch_directory_roots (config),
			 "Monitor directories included");
	log_option_list (tracker_config_get_no_watch_directory_roots (config),
			 "Monitor directories excluded");
	log_option_list (tracker_config_get_crawl_directory_roots (config),
			 "Crawling directories");
	log_option_list (tracker_config_get_no_index_file_types (config),
			 "File types excluded from indexing");
	log_option_list (tracker_config_get_disabled_modules (config),
			 "Disabled modules");
}

static gboolean
shutdown_timeout_cb (gpointer user_data)
{
	g_critical ("Could not exit in a timely fashion - terminating...");
	exit (EXIT_FAILURE);

	return FALSE;
}

static void
signal_handler (gint signo)
{
	static gboolean in_loop = FALSE;

	/* Die if we get re-entrant signals handler calls */
	if (in_loop) {
		exit (EXIT_FAILURE);
	}

	switch (signo) {
	case SIGSEGV:
		/* We are screwed if we get this so exit immediately! */
		exit (EXIT_FAILURE);

	case SIGBUS:
	case SIGILL:
	case SIGFPE:
	case SIGPIPE:
	case SIGABRT:
	case SIGTERM:
	case SIGINT:
		in_loop = TRUE;
		tracker_shutdown ();

	default:
		if (g_strsignal (signo)) {
			g_message ("Received signal:%d->'%s'",
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
	struct sigaction   act;
	sigset_t	   empty_mask;

	sigemptyset (&empty_mask);
	act.sa_handler = signal_handler;
	act.sa_mask    = empty_mask;
	act.sa_flags   = 0;

	sigaction (SIGTERM, &act, NULL);
	sigaction (SIGILL,  &act, NULL);
	sigaction (SIGBUS,  &act, NULL);
	sigaction (SIGFPE,  &act, NULL);
	sigaction (SIGHUP,  &act, NULL);
	sigaction (SIGSEGV, &act, NULL);
	sigaction (SIGABRT, &act, NULL);
	sigaction (SIGUSR1, &act, NULL);
	sigaction (SIGINT,  &act, NULL);
#endif /* G_OS_WIN32 */
}

static void
initialize_locations (void)
{
	TrackerMainPrivate *private;
	gchar		   *filename;

	private = g_static_private_get (&private_key);

	/* Public locations */
	private->user_data_dir =
		g_build_filename (g_get_user_data_dir (),
				  "tracker",
				  "data",
				  NULL);

	private->data_dir =
		g_build_filename (g_get_user_cache_dir (),
				  "tracker",
				  NULL);

	filename = g_strdup_printf ("tracker-%s", g_get_user_name ());
	private->sys_tmp_dir =
		g_build_filename (g_get_tmp_dir (),
				  filename,
				  NULL);
	g_free (filename);

	/* Private locations */
	private->log_filename =
		g_build_filename (g_get_user_data_dir (),
				  "tracker",
				  "trackerd.log",
				  NULL);
}

static void
initialize_directories (void)
{
	TrackerMainPrivate *private;
	gchar		   *filename;

	private = g_static_private_get (&private_key);

	/* NOTE: We don't create the database directories here, the
	 * tracker-db-manager does that for us.
	 */

	g_message ("Checking directory exists:'%s'", private->user_data_dir);
	g_mkdir_with_parents (private->user_data_dir, 00755);

	g_message ("Checking directory exists:'%s'", private->data_dir);
	g_mkdir_with_parents (private->data_dir, 00755);

	/* Remove old tracker dirs */
	filename = g_build_filename (g_get_home_dir (), ".Tracker", NULL);

	if (g_file_test (filename, G_FILE_TEST_EXISTS)) {
		tracker_path_remove (filename);
	}

	g_free (filename);

	/* Remove database if we are reindexing */
	filename = g_build_filename (private->sys_tmp_dir, "Attachments", NULL);
	g_mkdir_with_parents (filename, 00700);
	g_free (filename);

	/* Remove existing log files */
	tracker_file_unlink (private->log_filename);
}

static gboolean
initialize_databases (void)
{
	/*
	 * Create SQLite databases
	 */
	if (!tracker_status_get_is_readonly () && force_reindex) {
		TrackerDBInterface *iface;

		tracker_status_set_is_first_time_index (TRUE);

		/* Reset stats for embedded services if they are being reindexed */

		/* Here it doesn't matter which one we ask, as long as it has common.db
		 * attached. The service ones are cached connections, so we can use
		 * those instead of asking for an individual-file connection (like what
		 * the original code had) */

		/* iface = tracker_db_manager_get_db_interfaceX (TRACKER_DB_COMMON); */

		iface = tracker_db_manager_get_db_interface_by_service (TRACKER_DB_FOR_FILE_SERVICE);

		g_message ("*** DELETING STATS *** ");
		tracker_db_exec_no_reply (iface,
					  "update ServiceTypes set TypeCount = 0 where Embedded = 1");

	}

	/* Check db integrity if not previously shut down cleanly */
	if (!tracker_status_get_is_readonly () &&
	    !tracker_status_get_is_first_time_index () &&
	    tracker_db_get_option_int ("IntegrityCheck") == 1) {
		g_message ("Performing integrity check as the daemon was not shutdown cleanly");
		/* FIXME: Finish */
	}

	if (!tracker_status_get_is_readonly ()) {
		tracker_db_set_option_int ("IntegrityCheck", 1);
	}

	if (tracker_status_get_is_first_time_index ()) {
		tracker_db_set_option_int ("InitialIndex", 1);
	}

	return TRUE;
}


static void
shutdown_indexer (void)
{
}

static void
shutdown_databases (void)
{
	/* Reset integrity status as threads have closed cleanly */
	tracker_db_set_option_int ("IntegrityCheck", 0);
}

static void
shutdown_locations (void)
{
	/* Nothing to do here, this is done by the private free func */
}

static void
shutdown_directories (void)
{
	TrackerMainPrivate *private;

	private = g_static_private_get (&private_key);

	/* If we are reindexing, just remove the databases */
	if (private->reindex_on_shutdown) {
		tracker_db_manager_remove_all ();
	}
}

#ifdef HAVE_HAL

static void
set_up_throttle (TrackerHal    *hal,
		 TrackerConfig *config)
{
	gint throttle;

	/* If on a laptop battery and the throttling is default (i.e.
	 * 0), then set the throttle to be higher so we don't kill
	 * the laptop battery.
	 */
	throttle = tracker_config_get_throttle (config);

	if (tracker_hal_get_battery_in_use (hal)) {
		g_message ("We are running on battery");

		if (throttle == THROTTLE_DEFAULT) {
			tracker_config_set_throttle (config,
						     THROTTLE_DEFAULT_ON_BATTERY);
			g_message ("Setting throttle from %d to %d",
				   throttle,
				   THROTTLE_DEFAULT_ON_BATTERY);
		} else {
			g_message ("Not setting throttle, it is currently set to %d",
				   throttle);
		}
	} else {
		g_message ("We are not running on battery");

		if (throttle == THROTTLE_DEFAULT_ON_BATTERY) {
			tracker_config_set_throttle (config,
						     THROTTLE_DEFAULT);
			g_message ("Setting throttle from %d to %d",
				   throttle,
				   THROTTLE_DEFAULT);
		} else {
			g_message ("Not setting throttle, it is currently set to %d",
				   throttle);
		}
	}
}

static void
notify_battery_in_use_cb (GObject *gobject,
			  GParamSpec *arg1,
			  gpointer user_data)
{
	set_up_throttle (TRACKER_HAL (gobject),
			 TRACKER_CONFIG (user_data));
}

#endif /* HAVE_HAL */

static gboolean
start_cb (gpointer user_data)
{
	TrackerMainPrivate *private;

	if (!tracker_status_get_is_running ()) {
		return FALSE;
	}

	private = g_static_private_get (&private_key);

	if (private->processor) {
		tracker_processor_start (private->processor);
	}

	return FALSE;
}

gint
main (gint argc, gchar *argv[])
{
	GOptionContext		   *context = NULL;
	GOptionGroup		   *group;
	GError			   *error = NULL;
	TrackerMainPrivate	   *private;
	TrackerConfig		   *config;
	TrackerLanguage		   *language;
	TrackerHal		   *hal;
	TrackerDBIndex		   *file_index;
	TrackerDBIndex		   *file_update_index;
	TrackerDBIndex		   *email_index;
	TrackerRunningLevel	    runtime_level;
	TrackerDBManagerFlags	    flags = 0;
	TrackerDBIndexManagerFlags  index_flags = 0;
	gboolean		    is_first_time_index;

	g_type_init ();

	private = g_new0 (TrackerMainPrivate, 1);
	g_static_private_set (&private_key,
			      private,
			      private_free);

	if (!g_thread_supported ())
		g_thread_init (NULL);

	dbus_g_thread_init ();

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* Set timezone info */
	tzset ();

	/* Translators: this messagge will apper immediately after the
	 * usage string - Usage: COMMAND <THIS_MESSAGE>
	 */
	context = g_option_context_new (_("- start the tracker daemon"));

	/* Daemon group */
	group = g_option_group_new ("daemon",
				    _("Daemon Options"),
				    _("Show daemon options"),
				    NULL,
				    NULL);
	g_option_group_add_entries (group, entries_daemon);
	g_option_context_add_group (context, group);

	/* Indexer group */
	group = g_option_group_new ("indexer",
				    _("Indexer Options"),
				    _("Show indexer options"),
				    NULL,
				    NULL);
	g_option_group_add_entries (group, entries_indexer);
	g_option_context_add_group (context, group);

	g_option_context_parse (context, &argc, &argv, &error);
	g_option_context_free (context);

	if (error) {
		g_printerr ("Invalid arguments, %s\n", error->message);
		g_clear_error (&error);
		return EXIT_FAILURE;
	}

	/* Print information */
	g_print ("\n" ABOUT "\n" LICENSE "\n");
	g_print ("Initializing trackerd...\n");

	initialize_signal_handler ();

	/* Check XDG spec locations XDG_DATA_HOME _MUST_ be writable. */
	if (!tracker_env_check_xdg_dirs ()) {
		return EXIT_FAILURE;
	}

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

	/* This makes sure we have all the locations like the data
	 * dir, user data dir, etc all configured.
	 *
	 * The initialize_directories() function makes sure everything
	 * exists physically and/or is reset depending on various
	 * options (like if we reindex, we remove the data dir).
	 */
	initialize_locations ();

	/* Initialize major subsystems */
	config = tracker_config_new ();
	language = tracker_language_new (config);

#ifdef HAVE_HAL
	hal = tracker_hal_new ();

	g_signal_connect (hal, "notify::battery-in-use",
			  G_CALLBACK (notify_battery_in_use_cb),
			  config);

	set_up_throttle (hal, config);
#endif /* HAVE_HAL */

	/* Daemon command line arguments */
	if (verbosity > -1) {
		tracker_config_set_verbosity (config, verbosity);
	}

	if (initial_sleep > -1) {
		tracker_config_set_initial_sleep (config, initial_sleep);
	}

	if (low_memory) {
		tracker_config_set_low_memory_mode (config, TRUE);
	}

	if (monitors_to_exclude) {
		tracker_config_add_no_watch_directory_roots (config, monitors_to_exclude);
	}

	if (monitors_to_include) {
		tracker_config_add_watch_directory_roots (config, monitors_to_include);
	}

	if (crawl_dirs) {
		tracker_config_add_crawl_directory_roots (config, crawl_dirs);
	}

	if (disable_modules) {
		tracker_config_add_disabled_modules (config, disable_modules);
	}

	/* Indexer command line arguments */
	if (disable_indexing) {
		tracker_config_set_enable_indexing (config, FALSE);
	}

	if (language_code) {
		tracker_config_set_language (config, language_code);
	}

	initialize_directories ();

	/* Initialize other subsystems */
	tracker_status_init (config);

	tracker_log_init (private->log_filename, tracker_config_get_verbosity (config));
	g_print ("Starting log:\n  File:'%s'\n", private->log_filename);

	sanity_check_option_values (config);

	tracker_nfs_lock_init (tracker_config_get_nfs_locking (config));

	if (!tracker_dbus_init (config)) {
		return EXIT_FAILURE;
	}

	tracker_module_config_init ();

	flags |= TRACKER_DB_MANAGER_REMOVE_CACHE;
	index_flags |= TRACKER_DB_INDEX_MANAGER_READONLY;

	if (force_reindex) {
		flags |= TRACKER_DB_MANAGER_FORCE_REINDEX;
		index_flags |= TRACKER_DB_INDEX_MANAGER_FORCE_REINDEX;
	}

	if (tracker_config_get_low_memory_mode (config)) {
		flags |= TRACKER_DB_MANAGER_LOW_MEMORY_MODE;
	}

	tracker_db_manager_init (flags, &is_first_time_index);
	tracker_status_set_is_first_time_index (is_first_time_index);

	if (!tracker_db_index_manager_init (index_flags,
					    tracker_config_get_min_bucket_count (config),
					    tracker_config_get_max_bucket_count (config))) {
		return EXIT_FAILURE;
	}

	/*
	 * Check instances running
	 */
	runtime_level = check_runtime_level (config, hal);

	switch (runtime_level) {
	case TRACKER_RUNNING_NON_ALLOWED:
		return EXIT_FAILURE;

	case TRACKER_RUNNING_READ_ONLY:
		tracker_status_set_is_readonly (TRUE);
		break;

	case TRACKER_RUNNING_MAIN_INSTANCE:
		tracker_status_set_is_readonly (FALSE);
		break;
	}

	if (!initialize_databases ()) {
		return EXIT_FAILURE;
	}

	file_index = tracker_db_index_manager_get_index (TRACKER_DB_INDEX_FILE);
	file_update_index = tracker_db_index_manager_get_index (TRACKER_DB_INDEX_FILE_UPDATE);
	email_index = tracker_db_index_manager_get_index (TRACKER_DB_INDEX_EMAIL);

	if (!TRACKER_IS_DB_INDEX (file_index) ||
	    !TRACKER_IS_DB_INDEX (file_update_index) ||
	    !TRACKER_IS_DB_INDEX (email_index)) {
		g_critical ("Could not create indexer for all indexes (file, file-update, email)");
		return EXIT_FAILURE;
	}

	tracker_db_init (config, language, file_index, email_index);
	tracker_xesam_manager_init ();

	private->processor = tracker_processor_new (config, hal);

	/* Make Tracker available for introspection */
	if (!tracker_dbus_register_objects (config,
					    language,
					    file_index,
					    email_index,
					    private->processor)) {
		return EXIT_FAILURE;
	}

	g_message ("Waiting for DBus requests...");

	/* Set our status as running, if this is FALSE, threads stop
	 * doing what they do and shutdown.
	 */
	tracker_status_set_is_running (TRUE);

	if (!tracker_status_get_is_readonly ()) {
		gint seconds;

		seconds = tracker_config_get_initial_sleep (config);

		if (seconds > 0) {
			g_message ("Waiting %d seconds before starting",
				   seconds);
			g_timeout_add (seconds * 1000, start_cb, NULL);
		} else {
			g_idle_add (start_cb, NULL);
		}
	} else {
		/* We set the state here because it is not set in the
		 * processor otherwise.
		 */
		g_message ("Running in read-only mode, not starting crawler/indexing");
		tracker_status_set_and_signal (TRACKER_STATUS_IDLE);
	}

	if (tracker_status_get_is_running ()) {
		private->main_loop = g_main_loop_new (NULL, FALSE);
		g_main_loop_run (private->main_loop);
	}

#if 0
	/* We can block on this since we are likely to block on
	 * shutting down otherwise anyway.
	 */
	org_freedesktop_Tracker_Indexer_pause_for_duration (tracker_dbus_indexer_get_proxy (),
							    2,
							    NULL);
#endif

	/*
	 * Shutdown the daemon
	 */
	g_message ("Shutdown started");

	tracker_status_set_and_signal (TRACKER_STATUS_SHUTDOWN);

	g_timeout_add_full (G_PRIORITY_LOW, 5000, shutdown_timeout_cb, NULL, NULL);

	g_message ("Waiting for indexer to finish");
	org_freedesktop_Tracker_Indexer_shutdown (tracker_dbus_indexer_get_proxy (), &error);

	if (error) {
		g_message ("Could not shutdown the indexer, %s", error->message);
		g_message ("Continuing anyway...");
		g_error_free (error);
	}

	g_message ("Cleaning up");
	if (private->processor) {
		/* We do this instead of let the private data free
		 * itself later so we can clean up references to this
		 * elsewhere.
		 */
		g_object_unref (private->processor);
		private->processor = NULL;
	}

	shutdown_indexer ();
	shutdown_databases ();
	shutdown_directories ();

	/* Shutdown major subsystems */
	tracker_xesam_manager_shutdown ();
	tracker_dbus_shutdown ();
	tracker_db_manager_shutdown ();
	tracker_db_index_manager_shutdown ();
	tracker_db_shutdown ();
	tracker_module_config_shutdown ();
	tracker_nfs_lock_shutdown ();
	tracker_status_shutdown ();
	tracker_log_shutdown ();

#ifdef HAVE_HAL
	g_signal_handlers_disconnect_by_func (hal,
					      notify_battery_in_use_cb,
					      config);

	g_object_unref (hal);
#endif /* HAVE_HAL */

	g_object_unref (language);
	g_object_unref (config);

	shutdown_locations ();

	g_print ("\nOK\n\n");

	return EXIT_SUCCESS;
}

void
tracker_shutdown (void)
{
	TrackerMainPrivate *private;

	private = g_static_private_get (&private_key);

	tracker_status_set_is_running (FALSE);

	tracker_processor_stop (private->processor);

	g_main_loop_quit (private->main_loop);
}

const gchar *
tracker_get_sys_tmp_dir (void)
{
	TrackerMainPrivate *private;

	private = g_static_private_get (&private_key);

	return private->sys_tmp_dir;
}

void
tracker_set_reindex_on_shutdown (gboolean value)
{
	TrackerMainPrivate *private;

	private = g_static_private_get (&private_key);

	private->reindex_on_shutdown = value;
}
