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
#if defined(__linux__)
#include <linux/sched.h>
#endif
#include <sched.h>

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
#include <libtracker-common/tracker-thumbnailer.h>

#include <libtracker-db/tracker-db-manager.h>
#include <libtracker-db/tracker-db-index.h>
#include <libtracker-db/tracker-db-index-manager.h>

#include <libtracker-data/tracker-data-manager.h>
#include <libtracker-data/tracker-turtle.h>
#include <libtracker-data/tracker-data-backup.h>

#include <tracker-push.h>

#include "tracker-crawler.h"
#include "tracker-dbus.h"
#include "tracker-indexer-client.h"
#include "tracker-main.h"
#include "tracker-monitor.h"
#include "tracker-processor.h"
#include "tracker-status.h"
#include "tracker-volume-cleanup.h"
#include "tracker-backup.h"
#include "tracker-daemon.h"

#ifdef G_OS_WIN32
#include <windows.h>
#include <pthread.h>
#include "mingw-compat.h"
#endif

#define ABOUT								  \
	"Tracker " PACKAGE_VERSION "\n"

#define LICENSE								  \
	"This program is free software and comes without any warranty.\n" \
	"It is licensed under version 2 or later of the General Public "  \
	"License which can be viewed at:\n"				  \
	"\n"								  \
	"  http://www.gnu.org/licenses/gpl.txt\n"

/* Throttle defaults */
#define THROTTLE_DEFAULT	    0
#define THROTTLE_DEFAULT_ON_BATTERY 5

#ifdef HAVE_HAL 

typedef struct {
	gchar    *udi;
	gchar    *mount_point;
	gboolean  no_crawling;
	gboolean  was_added;
} MountPointUpdate;

#endif /* HAVE_HAL */

typedef enum {
	TRACKER_RUNNING_NON_ALLOWED,
	TRACKER_RUNNING_READONLY,
	TRACKER_RUNNING_MAIN_INSTANCE
} TrackerRunningLevel;

typedef struct {
	GMainLoop	 *main_loop;
	gchar		 *log_filename;

	gchar		 *data_dir;
	gchar		 *user_data_dir;
	gchar		 *sys_tmp_dir;
	gchar            *ttl_backup_file;
	
	gboolean          public_interfaces_up;
	gboolean          mount_points_up;

	gboolean	  reindex_on_shutdown;
	gboolean          shutdown;

	gint              mount_points_to_set;

	TrackerProcessor *processor;
} TrackerMainPrivate;

/* Private */
static GStaticPrivate	     private_key = G_STATIC_PRIVATE_INIT;

/* Private command line parameters */
static gboolean		     version;
static gint		     verbosity = -1;
static gint		     initial_sleep = -1;
static gboolean		     low_memory;
static gchar	           **monitors_to_exclude;
static gchar	           **monitors_to_include;
static gchar	           **crawl_dirs;
static const gchar * const  *disable_modules;

static gboolean		     force_reindex;
static gboolean		     disable_indexing;
static gchar	            *language_code;

static GOptionEntry	     entries[] = {
	/* Daemon options */
	{ "version", 'V', 0,
	  G_OPTION_ARG_NONE, &version,
	  N_("Displays version information"),
	  NULL },
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
	  N_("Disable modules from being processed (you can do -d <module> -d <module>)"),
	  NULL },

	/* Indexer options */
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

	g_free (private->ttl_backup_file);
	g_free (private->log_filename);

	if (private->main_loop) {
		g_main_loop_unref (private->main_loop);
	}

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
		g_message ("Indexing disabled in config, running in read-only mode");
		return TRACKER_RUNNING_READONLY;
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
			runlevel = TRACKER_RUNNING_READONLY;
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
			runlevel = TRACKER_RUNNING_READONLY;
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
			runlevel = TRACKER_RUNNING_READONLY;
		}
#endif /* HAVE_HAL */
	}

	close (fd);

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
	g_message ("  Indexing enabled  .....................  %s",
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
			 "Disabled modules (config)");
}

static gboolean
shutdown_timeout_cb (gpointer user_data)
{
	g_critical ("Could not exit in a timely fashion - terminating...");
	exit (EXIT_FAILURE);

	return FALSE;
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
		tracker_shutdown ();

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
#endif /* G_OS_WIN32 */
}

static void
initialize_priority (void)
{
	/* Set disk IO priority and scheduling */
	tracker_ioprio_init ();

	/* NOTE: We only set the nice() value when crawling, for all
	 * other times we don't have a nice() value. Check the
	 * tracker-status code to see where this is done.
	 */
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

	private->ttl_backup_file = 
		g_build_filename (private->user_data_dir, 
				  "tracker-userdata-backup.ttl",
				  NULL);

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

	/* g_message ("Checking directory exists:'%s'", private->user_data_dir); */
	g_mkdir_with_parents (private->user_data_dir, 00755);

	/* g_message ("Checking directory exists:'%s'", private->data_dir); */
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
}

static gboolean
initialize_databases (void)
{
	/* This means we doing the initial check that our dbs are up
	 * to date. Once we get finished from the indexer, we set
	 * this to FALSE.
	 */
	tracker_status_set_is_initial_check (TRUE);

	/* We set our first time indexing state here */
	if (!tracker_status_get_is_readonly () && force_reindex) {
		tracker_status_set_is_first_time_index (TRUE);
	}

	/* Check db integrity if not previously shut down cleanly */
	if (!tracker_status_get_is_readonly () &&
	    !tracker_status_get_is_first_time_index () &&
	    tracker_data_manager_get_db_option_int ("IntegrityCheck") == 1) {
		g_message ("Performing integrity check as the daemon was not shutdown cleanly");
		/* FIXME: Finish */
	}

	return TRUE;
}

static void
shutdown_databases (void)
{
	TrackerMainPrivate *private;

	private = g_static_private_get (&private_key);

	/* If we are reindexing, save the user metadata  */
	if (private->reindex_on_shutdown) {
		tracker_data_backup_save (private->ttl_backup_file, NULL);
	}
	/* Reset integrity status as threads have closed cleanly */
	tracker_data_manager_set_db_option_int ("IntegrityCheck", 0);
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
		tracker_db_index_manager_remove_all ();
	}
}

static const gchar *
get_ttl_backup_filename (void) 
{
	TrackerMainPrivate *private;

	private = g_static_private_get (&private_key);

	return private->ttl_backup_file;
}

static void
backup_user_metadata (TrackerConfig   *config, 
		      TrackerLanguage *language)
{
	TrackerDBIndex *file_index;
	TrackerDBIndex *email_index;
	TrackerMode     mode;
	const gchar    *mode_str;
	gboolean        is_first_time_index;

	g_message ("Saving metadata in %s", get_ttl_backup_filename ());
	
	/*
	 *  Init the DB stack to get the user metadata
	 */
	mode = tracker_mode_get ();
	mode_str = tracker_mode_to_string (mode);
	
	if (!tracker_db_manager_init (0, &is_first_time_index, TRUE, mode_str)) {
		return;
	}

	/*
	 * If some database is missing or the dbs dont exists, we dont need
	 * to backup anything.
	 */
	if (is_first_time_index) {
		tracker_db_manager_shutdown ();
		return;
	}
	
	tracker_db_index_manager_init (0,
				       tracker_config_get_min_bucket_count (config),
				       tracker_config_get_max_bucket_count (config));
	
	file_index = tracker_db_index_manager_get_index (TRACKER_DB_INDEX_FILE);
	email_index = tracker_db_index_manager_get_index (TRACKER_DB_INDEX_EMAIL);
	
	tracker_data_manager_init (config, language, file_index, email_index);
	
	/* Actual save of the metadata */
	tracker_data_backup_save (get_ttl_backup_filename (), NULL);
	
	/* Shutdown the DB stack */
	tracker_data_manager_shutdown ();
	tracker_db_index_manager_shutdown ();
	tracker_db_manager_shutdown ();
}

/*
 * TODO: Ugly hack counting signals because the indexer is sending two "Finished" signals
 *  and only the second really mean "finished processing modules".
 *
 * Saving the last backup file to help with debugging.
 */
static void
processor_finished_cb (TrackerProcessor *processor, 
		      gpointer          user_data)
{
	GError *error = NULL;
	
	if (!tracker_status_get_is_initial_check ()) {
		gchar *rebackup;

		g_debug ("Uninstalling initial processor finished callback");

		g_signal_handlers_disconnect_by_func (processor, 
						      processor_finished_cb, 
						      user_data);

		if (g_file_test (get_ttl_backup_filename (), G_FILE_TEST_EXISTS)) {
			org_freedesktop_Tracker_Indexer_restore_backup (tracker_dbus_indexer_get_proxy (),
									get_ttl_backup_filename (),
									&error);

			if (error) {
				g_message ("Could not restore backup, %s",
					   error->message);
				g_error_free (error);
				return;
			}

			rebackup = g_strdup_printf ("%s.old",
						    get_ttl_backup_filename ());
			g_rename (get_ttl_backup_filename (), rebackup);
			g_free (rebackup);
		}
	}
}

#ifdef HAVE_HAL

static MountPointUpdate *
mount_point_update_new (const gchar *udi,
			const gchar *mount_point,
			gboolean     no_crawling,
			gboolean     was_added)
{
	MountPointUpdate *mpu;

	mpu = g_slice_new0 (MountPointUpdate);

	mpu->udi = g_strdup (udi);
	mpu->mount_point = g_strdup (mount_point);

	mpu->no_crawling = no_crawling;
	mpu->was_added = was_added;

	return mpu;
}

static void
mount_point_update_free (MountPointUpdate *mpu)
{
	if (!mpu) {
		return;
	}

	g_free (mpu->mount_point);
	g_free (mpu->udi);

	g_slice_free (MountPointUpdate, mpu);
}

static void
mount_point_set (MountPointUpdate *mpu)
{
	TrackerMainPrivate *private;

	g_message ("Indexer has now set the state for the volume with UDI:");
	g_message ("  %s", mpu->udi);

	/* See if we have any more callbacks here, if we don't we can
	 * signal the stats to update.
	 */
	private = g_static_private_get (&private_key);

	private->mount_points_to_set--;

	if (private->mount_points_to_set < 1) {
		/* This is a special case, because we don't get the
		 * "Finished" signal from the indexer when we set something
		 * in the volumes table, we have to signal all clients from
		 * here that the statistics may have changed.
		 */
		if (private->public_interfaces_up) {
			tracker_daemon_signal_statistics ();
		}

		/* Unpause the indexer */
		tracker_status_set_is_paused_for_dbus (FALSE);
	} else if (private->mount_points_up) {
		g_message ("Statistics not being signalled, %d %s remaining", 
			   private->mount_points_to_set,
			   private->mount_points_to_set == 1 ? "volume" : "volumes");
	}

	/* Make sure we crawl any new mount points or stop crawling
	 * any mount points. We do it this way instead of listening
	 * for the same HAL signals in the processor because the
	 * processor checks state and at the time, we are PAUSED which
	 * causes us state machine problems. 
	 *
	 * We check the processor is created here because there is a
	 * race condition where the processor isn't created yet and we
	 * tell it about added/removed mount points. It will
	 * automatically find those out when it is created, it is only
	 * needed here for AFTER it is created.
	 *
	 * This is the easiest way to do it.
	 */
	if (!mpu->no_crawling && private->processor) {
		if (mpu->was_added) {
			tracker_processor_mount_point_added (private->processor,
							     mpu->udi,
							     mpu->mount_point);
		} else {
			tracker_processor_mount_point_removed (private->processor,
							       mpu->udi,
							       mpu->mount_point);
		}
	}
}

static void
mount_point_set_cb (DBusGProxy *proxy,
		    GError     *error,
		    gpointer    user_data)
{
	MountPointUpdate *mpu;

	mpu = user_data;

	if (error) {
		g_critical ("Indexer couldn't set volume state for:'%s' in database, %s",
			    mpu->udi,
			    error ? error->message : "no error given");

		g_error_free (error);

		tracker_shutdown ();
	} else {
		mount_point_set (mpu);
	}

	mount_point_update_free (mpu);
}

static void
mount_point_added_cb (TrackerHal  *hal,
		      const gchar *udi,
		      const gchar *mount_point,
		      gpointer	   user_data)
{
	TrackerMainPrivate *private;
	MountPointUpdate   *mpu;
	
	private = g_static_private_get (&private_key);

	private->mount_points_to_set++;

	tracker_status_set_is_paused_for_dbus (TRUE);

	g_message ("Indexer is being notified about added volume with UDI:");
	g_message ("  %s", udi);

	mpu = mount_point_update_new (udi, mount_point, FALSE, TRUE);
	org_freedesktop_Tracker_Indexer_volume_update_state_async (tracker_dbus_indexer_get_proxy (), 
								   udi,
								   mount_point,
								   TRUE,
								   mount_point_set_cb,
								   mpu);
}

static void
mount_point_removed_cb (TrackerHal  *hal,
			const gchar *udi,
			const gchar *mount_point,
			gpointer     user_data)
{
	TrackerMainPrivate *private;
	MountPointUpdate   *mpu;

	private = g_static_private_get (&private_key);

	private->mount_points_to_set++;

	tracker_status_set_is_paused_for_dbus (TRUE);

	g_message ("Indexer is being notified about removed volume with UDI:");
	g_message ("  %s", udi);

	mpu = mount_point_update_new (udi, mount_point, FALSE, FALSE);
	org_freedesktop_Tracker_Indexer_volume_update_state_async (tracker_dbus_indexer_get_proxy (), 
								   udi,
								   mount_point,
								   FALSE,
								   mount_point_set_cb,
								   mpu);
}

static void
set_up_mount_points (TrackerHal *hal)
{
	TrackerMainPrivate *private;
	GError *error = NULL;
	GList *roots, *l;

	private = g_static_private_get (&private_key);

	tracker_status_set_is_paused_for_dbus (TRUE);

	private->mount_points_up = FALSE;

	g_message ("Indexer is being notified to disable all volumes");
	org_freedesktop_Tracker_Indexer_volume_disable_all (tracker_dbus_indexer_get_proxy (), &error);

	if (error) {
		g_critical ("Indexer couldn't disable all volumes, %s",
			    error ? error->message : "no error given");
		g_error_free (error);
		tracker_shutdown ();
		return;
	}

	g_message ("  Done");

	roots = tracker_hal_get_removable_device_udis (hal);

	if (roots) {
		g_message ("Indexer is being notified about volume states with UDIs:");

		l = roots;

		while (l && !private->shutdown) {
			MountPointUpdate *mpu;

			private->mount_points_to_set++;
			mpu = mount_point_update_new (l->data,
						      tracker_hal_udi_get_mount_point (hal, l->data),
						      TRUE,
						      tracker_hal_udi_get_is_mounted (hal, l->data));

			g_message ("  %s", mpu->udi);

			org_freedesktop_Tracker_Indexer_volume_update_state (tracker_dbus_indexer_get_proxy (),
									     mpu->udi,
									     mpu->mount_point,
									     mpu->was_added,
									     &error);

			if (error) {
				g_critical ("Indexer couldn't set volume state for:'%s' in database, %s",
					    mpu->udi,
					    error ? error->message : "no error given");

				g_error_free (error);

				tracker_shutdown ();
				break;
			} else {
				mount_point_set (mpu);
			}

			mount_point_update_free (mpu);
			l = l->next;
		}

		g_list_free (roots);
	} else {
		g_message ("Indexer does not need to be notified of volume states, none to set");

		tracker_status_set_is_paused_for_dbus (FALSE);
	}

	private->mount_points_up = TRUE;
}

#endif /* HAVE_HAL */

static gboolean
start_processor_cb (gpointer user_data)
{
	TrackerMainPrivate *private;

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
	GError			   *error = NULL;
	TrackerMainPrivate	   *private;
	TrackerConfig		   *config;
	TrackerLanguage		   *language;
	TrackerHal		   *hal;
	TrackerDBIndex		   *file_index;
	TrackerDBIndex		   *email_index;
	TrackerRunningLevel	    runtime_level;
	TrackerDBManagerFlags	    flags = 0;
	TrackerDBIndexManagerFlags  index_flags = 0;
	TrackerMode                 mode;
	const gchar                *mode_str;
	gboolean		    is_first_time_index;

	g_type_init ();

	if (!g_thread_supported ()) {
		g_thread_init (NULL);
	}

	private = g_new0 (TrackerMainPrivate, 1);
	g_static_private_set (&private_key,
			      private,
			      private_free);

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
	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, &error);
	g_option_context_free (context);

	if (error) {
		g_printerr ("Invalid arguments, %s\n", error->message);
		g_clear_error (&error);
		return EXIT_FAILURE;
	}

	if (version) {
		/* Print information */
		g_print ("\n" ABOUT "\n" LICENSE "\n");
		return EXIT_SUCCESS;
	}

	g_print ("Initializing trackerd...\n");

	initialize_signal_handler ();

	/* Check XDG spec locations XDG_DATA_HOME _MUST_ be writable. */
	if (!tracker_env_check_xdg_dirs ()) {
		return EXIT_FAILURE;
	}

	/* This makes sure we don't steal all the system's resources */
	initialize_priority ();

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

	if (!tracker_dbus_init (config)) {
		return EXIT_FAILURE;
	}

	/* Initialize other subsystems */
	tracker_log_init (private->log_filename, tracker_config_get_verbosity (config));
	g_print ("Starting log:\n  File:'%s'\n", private->log_filename);

	sanity_check_option_values (config);

	tracker_nfs_lock_init (tracker_config_get_nfs_locking (config));

#ifdef HAVE_HAL
	hal = tracker_hal_new ();

	g_signal_connect (hal, "mount-point-added",
			  G_CALLBACK (mount_point_added_cb),
			  NULL);
	g_signal_connect (hal, "mount-point-removed",
			  G_CALLBACK (mount_point_removed_cb),
			  NULL);
#endif /* HAVE_HAL */

	tracker_status_init (config, hal);

	tracker_module_config_init ();

	tracker_turtle_init ();
	tracker_thumbnailer_init (config);

	flags |= TRACKER_DB_MANAGER_REMOVE_CACHE;
	index_flags |= TRACKER_DB_INDEX_MANAGER_READONLY;

	if (force_reindex) {
		backup_user_metadata (config, language);

		flags |= TRACKER_DB_MANAGER_FORCE_REINDEX;
		index_flags |= TRACKER_DB_INDEX_MANAGER_FORCE_REINDEX;
	}

	if (tracker_config_get_low_memory_mode (config)) {
		flags |= TRACKER_DB_MANAGER_LOW_MEMORY_MODE;
	}

	mode = tracker_mode_get ();
	mode_str = tracker_mode_to_string (mode);
	
	if (!tracker_db_manager_init (flags, &is_first_time_index, TRUE, mode_str)) {
		return EXIT_FAILURE;
	}

	if (!tracker_db_index_manager_init (index_flags,
					    tracker_config_get_min_bucket_count (config),
					    tracker_config_get_max_bucket_count (config))) {
		return EXIT_FAILURE;
	}

	tracker_status_set_is_first_time_index (is_first_time_index);

	/*
	 * Check instances running
	 */
	runtime_level = check_runtime_level (config, hal);

	switch (runtime_level) {
	case TRACKER_RUNNING_NON_ALLOWED:
		return EXIT_FAILURE;

	case TRACKER_RUNNING_READONLY:
		tracker_status_set_is_readonly (TRUE);
		break;

	case TRACKER_RUNNING_MAIN_INSTANCE:
		tracker_status_set_is_readonly (FALSE);
		break;
	default:
		break;
	}

	/*
	 * Set up databases
	 */
	if (!initialize_databases ()) {
		return EXIT_FAILURE;
	}

	file_index = tracker_db_index_manager_get_index (TRACKER_DB_INDEX_FILE);
	email_index = tracker_db_index_manager_get_index (TRACKER_DB_INDEX_EMAIL);

	if (!TRACKER_IS_DB_INDEX (file_index) ||
	    !TRACKER_IS_DB_INDEX (email_index)) {
		g_critical ("Could not create indexer for all indexes (file, email)");
		return EXIT_FAILURE;
	}

	tracker_data_manager_init (config, language, file_index, email_index);
	tracker_volume_cleanup_init ();

#ifdef HAVE_HAL
	/* We set up the throttle and mount points here. For the mount
	 * points, this means contacting the Indexer. This means that
	 * we have to have already initialised the databases if we
	 * are going to do that.
	 */
	set_up_mount_points (hal);
#endif /* HAVE_HAL */

	if (private->shutdown) {
		goto shutdown;
	}

	/* 
	 * Start public interfaces (DBus, push modules, etc)
	 */
	private->processor = tracker_processor_new (config, hal);

	if (force_reindex &&
	    g_file_test (get_ttl_backup_filename (), G_FILE_TEST_EXISTS)) {
		g_debug ("Setting callback for crawling finish detection");
		g_signal_connect (private->processor, "finished", 
				  G_CALLBACK (processor_finished_cb), 
				  NULL);
	}

	/* Make Tracker available for introspection */
	if (!tracker_dbus_register_objects (config,
					    language,
					    file_index,
					    email_index,
					    private->processor)) {
		return EXIT_FAILURE;
	}
	
	tracker_push_init (config);
	
	private->public_interfaces_up = TRUE;

	/* Signal to clients current statistics */
	tracker_daemon_signal_statistics ();

	g_message ("Waiting for DBus requests...");

	/* 
	 * Start processor
	 */
	if (!tracker_status_get_is_readonly ()) {
		gint seconds;
		
		seconds = tracker_config_get_initial_sleep (config);
		
		if (seconds > 0) {
			g_message ("Waiting %d seconds before starting processor",
				   seconds);
			g_timeout_add_seconds (seconds, start_processor_cb, NULL);
		} else {
			g_idle_add (start_processor_cb, NULL);
		}
	} else {
		/* We set the state here because it is not set in the
		 * processor otherwise.
		 */
		g_message ("Running in read-only mode, not starting crawler/indexing");
		tracker_status_set_and_signal (TRACKER_STATUS_IDLE);
	}

	/* 
	 * Start main loop
	 */
	if (!private->shutdown) {
		private->main_loop = g_main_loop_new (NULL, FALSE);
		g_main_loop_run (private->main_loop);
	}

shutdown:
	/*
	 * Shutdown main loop
	 */
	g_message ("Shutdown started");

	tracker_status_set_and_signal (TRACKER_STATUS_SHUTDOWN);

	g_timeout_add_full (G_PRIORITY_LOW, 5000, shutdown_timeout_cb, NULL, NULL);

	g_message ("Cleaning up");
	if (private->processor) {
		/* We do this instead of let the private data free
		 * itself later so we can clean up references to this
		 * elsewhere.
		 */
		g_object_unref (private->processor);
		private->processor = NULL;
	}

	shutdown_databases ();
	shutdown_directories ();

	/* Shutdown major subsystems */
	if (private->public_interfaces_up) {
		tracker_push_shutdown ();
	}

	tracker_volume_cleanup_shutdown ();
	tracker_dbus_shutdown ();
	tracker_db_manager_shutdown ();
	tracker_db_index_manager_shutdown ();
	tracker_data_manager_shutdown ();
	tracker_module_config_shutdown ();
	tracker_nfs_lock_shutdown ();
	tracker_status_shutdown ();
	tracker_turtle_shutdown ();
	tracker_thumbnailer_shutdown ();
	tracker_log_shutdown ();

#ifdef HAVE_HAL
	g_signal_handlers_disconnect_by_func (hal,
					      mount_point_added_cb,
					      NULL);
	g_signal_handlers_disconnect_by_func (hal,
					      mount_point_removed_cb,
					      NULL);

	g_object_unref (hal);
#endif /* HAVE_HAL */

	g_object_unref (language);
	g_object_unref (config);

	shutdown_locations ();

	g_static_private_free (&private_key);

	g_print ("\nOK\n\n");

	return EXIT_SUCCESS;
}

void
tracker_shutdown (void)
{
	TrackerMainPrivate *private;

	private = g_static_private_get (&private_key);

	if (private) {
		if (private->processor) {
			tracker_processor_stop (private->processor);
		}

		if (private->main_loop) {
			g_main_loop_quit (private->main_loop);
		}

		private->shutdown = TRUE;
	}
}

const gchar *
tracker_get_data_dir (void)
{
	TrackerMainPrivate *private;

	private = g_static_private_get (&private_key);

	return private->data_dir;
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

