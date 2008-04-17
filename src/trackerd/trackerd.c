/* Tracker - indexer and metadata database engine
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
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

#ifndef DBUS_API_SUBJECT_TO_CHANGE
#define DBUS_API_SUBJECT_TO_CHANGE
#endif

#define I_AM_MAIN

#include "config.h"

#include <signal.h>
#include <errno.h>
#include <locale.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <glib/gpattern.h>

#ifdef IOPRIO_SUPPORT
#include "tracker-ioprio.h"
#endif

#include <libtracker-common/tracker-config.h>
#include <libtracker-common/tracker-language.h>
#include <libtracker-common/tracker-log.h>

#include "tracker-dbus.h"
#include "tracker-email.h"
#include "tracker-indexer.h"
#include "tracker-process-files.h"
#include "tracker-process-requests.h"
#include "tracker-watch.h"
#include "tracker-hal.h"

#include "tracker-service-manager.h"
  
#ifdef OS_WIN32
#include <windows.h>
#include <pthread.h>
#include "mingw-compat.h"
#endif

typedef struct {
	gchar	*uri;
	gint	mtime;
} IndexDir;

Tracker		       *tracker;
DBConnection	       *main_thread_db_con;
DBConnection	       *main_thread_cache_con;


/*
 *   The workflow to process files and notified file change events are as follows:
 *
 *   1) File scan or file notification change (as reported by FAM/iNotify).
 *   2) File Scheduler (we wait until a file's changes have stabilised (NB not neccesary with inotify))
 *   3) We process a file's basic metadata (stat) and determine what needs doing in a seperate thread.
 *   4) We extract CPU intensive embedded metadata/text/thumbnail in another thread and save changes to the DB
 *
 *
 *  Three threads are used to fully process a file event. Files or events to be processed are placed on
 *  asynchronous queues where another thread takes over the work.
 *
 *  The main thread is very lightweight and no cpu intensive or heavy file I/O (or heavy DB access) is permitted
 *  here after initialisation of the daemon. This ensures the main thread can handle events and DBUS
 *  requests in a timely low latency manner.
 *
 *  The File Process thread is for moderate CPU intensive load and I/O and involves calls to stat()
 *  and simple fast queries on the DB. The main thread queues files to be processed by this thread
 *  via the file_process async queue. As no heavily CPU intensive activity occurs here, we can quickly
 *  keep the DB representation of the watched file system up to date. Once a file has been processed
 *  here it is then placed on the file metadata queue which is handled by the File Metadata thread.
 *
 *  The File Metadata thread is a low priority thread to handle the highly CPU intensive parts.
 *  During this phase, embedded metadata is extracted from files and if a text filter and/or
 *  thumbnailer is available for the mime type of the file then these will be spawned synchronously.
 *  Finally all metadata (including file's text contents and path to thumbnails) is saved to the DB.
 *
 *  All responses including user initiated requests are queued by the main thread onto an
 *  asynchronous queue where potentially multiple threads are waiting to process them.
 */


gchar *type_array[] =   {"index", "string", "numeric", "date", NULL};

static gchar **no_watch_dirs = NULL;
static gchar **watch_dirs = NULL;
static gchar **crawl_dirs = NULL;
static gchar *language = NULL;
static gboolean disable_indexing = FALSE;
static gboolean reindex = FALSE;
static gboolean fatal_errors = FALSE;
static gboolean low_memory;
static gint throttle = -1;
static gint verbosity = 0;
static gint initial_sleep = -1; /* >= 0 is valid and will be set */

static GOptionEntry entries[] = {
	{"exclude-dir", 'e', 0, G_OPTION_ARG_STRING_ARRAY, &no_watch_dirs, N_("Directory to exclude from indexing"), N_("/PATH/DIR")},
	{"include-dir", 'i', 0, G_OPTION_ARG_STRING_ARRAY, &watch_dirs, N_("Directory to include in indexing"), N_("/PATH/DIR")},
	{"crawl-dir", 'c', 0, G_OPTION_ARG_STRING_ARRAY, &crawl_dirs, N_("Directory to crawl for indexing at start up only"), N_("/PATH/DIR")},
	{"no-indexing", 'n', 0, G_OPTION_ARG_NONE, &disable_indexing, N_("Disable any indexing or watching taking place"), NULL },
	{"verbosity", 'v', 0, G_OPTION_ARG_INT, &verbosity, N_("Value that controls the level of logging. Valid values are 0 (displays/logs only errors), 1 (minimal), 2 (detailed), and 3 (debug)"), N_("VALUE") },
	{"throttle", 't', 0, G_OPTION_ARG_INT, &throttle, N_("Value to use for throttling indexing. Value must be in range 0-99 (default 0) with lower values increasing indexing speed"), N_("VALUE") },
	{"low-memory", 'm', 0, G_OPTION_ARG_NONE, &low_memory, N_("Minimizes the use of memory but may slow indexing down"), NULL },
	{"initial-sleep", 's', 0, G_OPTION_ARG_INT, &initial_sleep, N_("Initial sleep time, just before indexing, in seconds"), NULL },
	{"language", 'l', 0, G_OPTION_ARG_STRING, &language, N_("Language to use for stemmer and stop words list (ISO 639-1 2 characters code)"), N_("LANG")},
	{"reindex", 'R', 0, G_OPTION_ARG_NONE, &reindex, N_("Force a re-index of all content"), NULL },
	{"fatal-errors", 'f', 0, G_OPTION_ARG_NONE, &fatal_errors, N_("Make tracker errors fatal"), NULL },
	{NULL}
};


static gint
get_update_count (DBConnection *db_con)
{

	gint  count = 0;
	gchar ***res = tracker_exec_proc (db_con, "GetUpdateCount", 0);

	if (res) {
		if (res[0] && res[0][0]) {
			count = atoi (res[0][0]);
		}
		tracker_db_free_result (res);
	}

	return count;
}

gboolean
tracker_die ()
{
	tracker_error ("trackerd has failed to exit on time - terminating...");
	exit (EXIT_FAILURE);
}


void
free_file_change (FileChange **user_data)
{
	FileChange *change = *user_data;
	g_free (change->uri);
	change->uri = NULL;
	change = NULL;
}

static void
free_file_change_queue (gpointer data, gpointer user_data)
{
	FileChange *change = (FileChange *)data;
	free_file_change (&change);
}


static void
reset_blacklist_file (char *uri)
{

	char *parent = g_path_get_dirname (uri);
	if (!parent) return;

	char *parent_name = g_path_get_basename (parent);
	if (!parent_name) return;

	char *parent_path = g_path_get_dirname (parent);
	if (!parent_path) return;	

	tracker_log ("resetting black list file %s", uri);

	/* reset mtime on parent folder of all outstanding black list files so they get indexed when next restarted */
	tracker_exec_proc (main_thread_db_con, "UpdateFileMTime", 3, "0", parent_path, parent_name);

	g_free (parent);
	g_free (parent_name);
	g_free (parent_path);		 


}

gboolean
tracker_do_cleanup (const gchar *sig_msg)
{
        GSList *black_list;

	tracker_set_status (tracker, STATUS_SHUTDOWN, 0, FALSE);

	if (sig_msg) {
		tracker_log ("Received signal '%s' so now shutting down", sig_msg);

		tracker_print_object_allocations ();
	}

	/* set kill timeout */
	g_timeout_add_full (G_PRIORITY_LOW,
	     		    20000,
 	    		    (GSourceFunc) tracker_die,
	     		    NULL, NULL
	   		    );


	/* stop threads from further processing of events if possible */

	tracker->in_flush = TRUE;

	//set_update_count (main_thread_db_con, tracker->update_count);

	/* wait for files thread to sleep */
	while (!g_mutex_trylock (tracker->files_signal_mutex)) {
		g_usleep (100);
	}

	g_mutex_unlock (tracker->files_signal_mutex);

	while (!g_mutex_trylock (tracker->metadata_signal_mutex)) {
		g_usleep (100);
	}

	g_mutex_unlock (tracker->metadata_signal_mutex);

	tracker_log ("shutting down threads");

	/* send signals to each thread to wake them up and then stop them */


	tracker->shutdown = TRUE;

	g_mutex_lock (tracker->request_signal_mutex);
	g_cond_signal (tracker->request_thread_signal);
	g_mutex_unlock (tracker->request_signal_mutex);

	g_mutex_lock (tracker->metadata_signal_mutex);
	g_cond_signal (tracker->metadata_thread_signal);
	g_mutex_unlock (tracker->metadata_signal_mutex);

	g_mutex_unlock (tracker->files_check_mutex);

	g_mutex_lock (tracker->files_signal_mutex);
	g_cond_signal (tracker->file_thread_signal);
	g_mutex_unlock (tracker->files_signal_mutex);


	/* wait for threads to exit and unlock check mutexes to prevent any deadlocks*/

	g_mutex_unlock (tracker->request_check_mutex);
	g_mutex_lock (tracker->request_stopped_mutex);

	g_mutex_unlock (tracker->metadata_check_mutex);
	g_mutex_lock (tracker->metadata_stopped_mutex);

	g_mutex_unlock (tracker->files_check_mutex);
	g_mutex_lock (tracker->files_stopped_mutex);


	tracker_indexer_close (tracker->file_index);
	tracker_indexer_close (tracker->file_update_index);
	tracker_indexer_close (tracker->email_index);

	tracker_email_end_email_watching ();

	/* reset integrity status as threads have closed cleanly */
	tracker_db_set_option_int (main_thread_db_con, "IntegrityCheck", 0);


	/* reset black list files */
        black_list = tracker_process_files_get_temp_black_list ();
	g_slist_foreach (black_list,
                         (GFunc) reset_blacklist_file, 
                         NULL);
        g_slist_free (black_list);

	tracker_db_close (main_thread_db_con);

	/* This must be called after all other db functions */
	tracker_db_finalize ();


	if (tracker->reindex) {
		tracker_remove_dirs (tracker->data_dir);
		g_mkdir_with_parents (tracker->data_dir, 00755);
	}

        if (tracker->hal) {
                g_object_unref (tracker->hal);
                tracker->hal = NULL;
        }

	tracker_debug ("Shutting down main thread");

	tracker_log_term ();

	/* remove sys tmp directory */
	if (tracker->sys_tmp_root_dir) {
		tracker_remove_dirs (tracker->sys_tmp_root_dir);
	}

	/* remove file change queue */
	if (tracker->file_change_queue) {
		g_queue_foreach (tracker->file_change_queue,
				 free_file_change_queue, NULL);
		g_queue_free (tracker->file_change_queue);
		tracker->file_change_queue = NULL;
	}

        if (tracker->language) {
                tracker_language_free (tracker->language);
        }

        if (tracker->config) {
                g_object_unref (tracker->config);
        }

	g_main_loop_quit (tracker->loop);

	exit (EXIT_SUCCESS);

	return FALSE;
}

static void
signal_handler (gint signo)
{
	static gboolean in_loop = FALSE;

  	/* die if we get re-entrant signals handler calls */
	if (in_loop) {
		exit (EXIT_FAILURE);
	}

  	switch (signo) {

  		case SIGSEGV:

			/* we are screwed if we get this so exit immediately! */
			exit (EXIT_FAILURE);

	  	case SIGBUS:
		case SIGILL:
  		case SIGFPE:
  		case SIGPIPE:
		case SIGABRT:
		case SIGTERM:
		case SIGINT:

		  	in_loop = TRUE;

			tracker->is_running = FALSE;
			tracker_end_watching ();

			g_timeout_add_full (G_PRIORITY_LOW,
			     		    1,
		 	    		    (GSourceFunc) tracker_do_cleanup,
			     		    g_strdup (g_strsignal (signo)), NULL
			   		    );


			default:
			if (g_strsignal (signo)) {
	   			tracker_log ("Received signal %s ", g_strsignal (signo));
		}
			break;
	}
}

static inline void
queue_dir (const gchar *uri)
{
	FileInfo *info = tracker_create_file_info (uri, TRACKER_ACTION_DIRECTORY_CHECK, 0, 0);
	g_async_queue_push (tracker->file_process_queue, info);
}


static void
unregistered_func (DBusConnection *connection, gpointer data)
{
}


static DBusHandlerResult
local_dbus_connection_monitoring_message_func (DBusConnection *connection, DBusMessage *message, gpointer data)
{
	/* DBus connection has been lost! */
	if (dbus_message_is_signal (message, DBUS_INTERFACE_LOCAL, "Disconnected")) {
		dbus_message_ref (message);

		tracker_error ("DBus connection has been lost, trackerd will now shutdown");

		tracker->is_running = FALSE;
		tracker_end_watching ();
		tracker_do_cleanup ("DBus connection lost");
	}

	return DBUS_HANDLER_RESULT_HANDLED;
}


static gboolean
add_local_dbus_connection_monitoring (DBusConnection *connection)
{
	DBusObjectPathVTable dbus_daemon_vtable = {
		unregistered_func,
		local_dbus_connection_monitoring_message_func,
		NULL,
		NULL,
		NULL,
		NULL
	};

	if (!dbus_connection_register_object_path (connection, DBUS_PATH_LOCAL, &dbus_daemon_vtable, NULL)) {
		tracker_log ("could not register local D-BUS connection handler");
		return FALSE;

	} else {
		return TRUE;
	}
}


static void
set_defaults (void)
{
	tracker->grace_period = 0;

	tracker->reindex = FALSE;
	tracker->in_merge = FALSE;

	tracker->merge_limit = MERGE_LIMIT;

	tracker->index_status = INDEX_CONFIG;

	tracker->black_list_timer_active = FALSE;	

	tracker->pause_manual = FALSE;
	tracker->pause_battery = FALSE;
	tracker->pause_io = FALSE;

	tracker->use_nfs_safe_locking = FALSE;

	tracker->watch_limit = 0;
	tracker->index_counter = 0;
	tracker->index_count = 0;
	tracker->update_count = 0;

	tracker->max_index_text_length = MAX_INDEX_TEXT_LENGTH;
	tracker->max_process_queue_size = MAX_PROCESS_QUEUE_SIZE;
	tracker->max_extract_queue_size = MAX_EXTRACT_QUEUE_SIZE;

	tracker->flush_count = 0;

	tracker->index_numbers = FALSE;
	tracker->index_number_min_length = 6;
	tracker->strip_accents = TRUE;

	tracker->first_flush = TRUE;

	tracker->services_dir = g_build_filename (SHAREDIR, "tracker", "services", NULL);

	tracker->folders_count = 0;
	tracker->folders_processed = 0;
	tracker->mbox_count = 0;
	tracker->folders_processed = 0;
}

static void
log_option_list (GSList      *list,
		 const gchar *str)
{
	GSList *l;

	if (!list) {
		tracker_log ("%s: NONE!", str);
		return;
	}

	tracker_log ("%s:", str);

	for (l = list; l; l = l->next) {
		tracker_log ("  %s", l->data);
	}
}

static void
sanity_check_option_values (void)
{
        GSList *watch_directory_roots;
        GSList *crawl_directory_roots;
        GSList *no_watch_directory_roots;
        GSList *no_index_file_types;

        watch_directory_roots = tracker_config_get_watch_directory_roots (tracker->config);
        crawl_directory_roots = tracker_config_get_crawl_directory_roots (tracker->config);
        no_watch_directory_roots = tracker_config_get_no_watch_directory_roots (tracker->config);

        no_index_file_types = tracker_config_get_no_index_file_types (tracker->config);

	tracker_log ("Tracker configuration options:");
	tracker_log ("  Verbosity  ............................  %d", 
                     tracker_config_get_verbosity (tracker->config));
 	tracker_log ("  Low memory mode  ......................  %s", 
                     tracker_config_get_low_memory_mode (tracker->config) ? "yes" : "no");
 	tracker_log ("  Indexing enabled  .....................  %s", 
                     tracker_config_get_enable_indexing (tracker->config) ? "yes" : "no");
 	tracker_log ("  Watching enabled  .....................  %s", 
                     tracker_config_get_enable_watches (tracker->config) ? "yes" : "no");
 	tracker_log ("  File content indexing enabled  ........  %s", 
                     tracker_config_get_enable_content_indexing (tracker->config) ? "yes" : "no");
	tracker_log ("  Thumbnailing enabled  .................  %s", 
                     tracker_config_get_enable_thumbnails (tracker->config) ? "yes" : "no");
	tracker_log ("  Email client to index .................  %s",
		     tracker_config_get_email_client (tracker->config));

	tracker_log ("Tracker indexer parameters:");
	tracker_log ("  Indexer language code  ................  %s", 
                     tracker_config_get_language (tracker->config));
	tracker_log ("  Minimum index word length  ............  %d", 
                     tracker_config_get_min_word_length (tracker->config));
	tracker_log ("  Maximum index word length  ............  %d", 
                     tracker_config_get_max_word_length (tracker->config));
	tracker_log ("  Stemmer enabled  ......................  %s", 
                     tracker_config_get_enable_stemmer (tracker->config) ? "yes" : "no");

	tracker->word_count = 0;
	tracker->word_detail_count = 0;
	tracker->word_update_count = 0;

	tracker->file_word_table = g_hash_table_new (g_str_hash, g_str_equal);
	tracker->file_update_word_table = g_hash_table_new (g_str_hash, g_str_equal);
	tracker->email_word_table = g_hash_table_new (g_str_hash, g_str_equal);

	if (!tracker_config_get_low_memory_mode (tracker->config)) {
		tracker->memory_limit = 16000 *1024;
	
		tracker->max_process_queue_size = 5000;
		tracker->max_extract_queue_size = 5000;

		tracker->word_detail_limit = 2000000;
		tracker->word_detail_min = 0;
		tracker->word_count_limit = 500000;
		tracker->word_count_min = 0;
	} else {
		tracker->memory_limit = 8192 * 1024;

		tracker->max_process_queue_size = 500;
		tracker->max_extract_queue_size = 500;

		tracker->word_detail_limit = 50000;
		tracker->word_detail_min = 20000;
		tracker->word_count_limit = 5000;
		tracker->word_count_min = 500;
	}

        if (tracker->battery_udi) {
                /* Default is 0 */
                tracker_config_set_throttle (tracker->config, 5);
	}

	log_option_list (watch_directory_roots, "Watching directory roots");
	log_option_list (crawl_directory_roots, "Crawling directory roots");
	log_option_list (no_watch_directory_roots, "NOT watching directory roots");
	log_option_list (no_index_file_types, "NOT indexing file types");

        tracker_log ("Throttle level is %d\n", tracker_config_get_throttle (tracker->config));

	tracker->metadata_table = g_hash_table_new_full (g_str_hash,
                                                         g_str_equal, 
                                                         NULL, 
                                                         NULL);
	tracker->service_directory_table = g_hash_table_new_full (g_str_hash, 
                                                                  g_str_equal, 
                                                                  g_free, 
                                                                  g_free);
}

static void
create_index (gboolean need_data)
{
	tracker->first_time_index = TRUE;

	/* create files db and emails db */
	DBConnection *db_con_tmp = tracker_db_connect ();

	/* reset stats for embedded services if they are being reindexed */
	if (!need_data) {
		tracker_log ("*** DELETING STATS *** ");
		tracker_db_exec_no_reply (db_con_tmp, "update ServiceTypes set TypeCount = 0 where Embedded = 1");
	}

	tracker_db_close (db_con_tmp);
	g_free (db_con_tmp);

	/* create databases */

	db_con_tmp = tracker_db_connect_file_content ();
	tracker_db_close (db_con_tmp);
	g_free (db_con_tmp);

	db_con_tmp = tracker_db_connect_email_content ();
	tracker_db_close (db_con_tmp);
	g_free (db_con_tmp);

	db_con_tmp = tracker_db_connect_emails ();
	tracker_db_close (db_con_tmp);
	g_free (db_con_tmp);

}

gint
main (gint argc, gchar *argv[])
{
	gint               lfp;
#ifndef OS_WIN32
  	struct sigaction   act;
	sigset_t 	   empty_mask;
#endif
	gchar 		  *lock_file, *str, *lock_str;
	GOptionContext    *context = NULL;
	GError            *error = NULL;
	gchar             *example;
	gboolean 	   need_index, need_data;
	DBConnection      *db_con;
	gchar             *tmp_dir;
	gchar             *old_tracker_dir;
	gchar             *log_filename;

        g_type_init ();
        
	if (!g_thread_supported ())
		g_thread_init (NULL);

	dbus_g_thread_init ();

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
        textdomain (GETTEXT_PACKAGE);

	/* set timezone info */
	tzset ();

        /* Translators: this messagge will apper immediately after the  */
        /* usage string - Usage: COMMAND <THIS_MESSAGE>     */
	context = g_option_context_new (_("- start the tracker daemon"));
        example = g_strconcat ("-i ", _("DIRECTORY"), " -i ", _("DIRECTORY"),
			       " -e ", _("DIRECTORY"), " -e ", _("DIRECTORY"),
			       NULL);

#ifdef HAVE_RECENT_GLIB
        /* Translators: this message will appear after the usage string */
        /* and before the list of options, showing an usage example.    */
        g_option_context_set_summary (context,
                                      g_strconcat(_("To include or exclude multiple directories "
                                                    "at the same time, join multiple options like:"),

                                                  "\n\n\t",
                                                  example, NULL));

#endif /* HAVE_RECENT_GLIB */

	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, &error);

	g_option_context_free (context);
	g_free (example);

	g_print ("\n\nTracker version %s Copyright (c) 2005-2007 by Jamie McCracken (jamiemcc@gnome.org)\n\n", TRACKER_VERSION);
	g_print ("This program is free software and comes without any warranty.\nIt is licensed under version 2 or later of the General Public License which can be viewed at http://www.gnu.org/licenses/gpl.txt\n\n");

	g_print ("Initialising tracker...\n");

#ifndef OS_WIN32
	/* trap signals */
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
#endif

	tracker = g_new0 (Tracker, 1);

	/* Set up directories */
	tracker->pid = (int) getpid ();
	tmp_dir = g_strdup_printf ("Tracker-%s.%d", g_get_user_name (), tracker->pid);
	tracker->sys_tmp_root_dir = g_build_filename (g_get_tmp_dir (), tmp_dir, NULL);
	g_free (tmp_dir);

	tracker->root_dir = g_build_filename (g_get_user_data_dir  (), "tracker", NULL);
	tracker->data_dir = g_build_filename (g_get_user_cache_dir (), "tracker", NULL);
	tracker->config_dir = g_strdup (g_get_user_config_dir ());
	tracker->user_data_dir = g_build_filename (tracker->root_dir, "data", NULL);
	
        /* Set up the config */
        tracker->config = tracker_config_new ();
        tracker->language = tracker_language_new (tracker->config);

	/* Set up the log */
	log_filename = g_build_filename (tracker->root_dir, "tracker.log", NULL);
	tracker_unlink (log_filename);

	tracker_log_init (log_filename, 
                          tracker_config_get_verbosity (tracker->config), 
                          fatal_errors);
	tracker_log ("Starting log");

        /* Set up the DBus IPC */
	tracker->dbus_con = tracker_dbus_init ();

	add_local_dbus_connection_monitoring (tracker->dbus_con);

	tracker_set_status (tracker, STATUS_INIT, 0, FALSE);

 	tracker->is_running = FALSE;
	tracker->shutdown = FALSE;

	tracker->files_check_mutex = g_mutex_new ();
	tracker->metadata_check_mutex = g_mutex_new ();
	tracker->request_check_mutex = g_mutex_new ();

	tracker->files_stopped_mutex = g_mutex_new ();
	tracker->metadata_stopped_mutex = g_mutex_new ();
	tracker->request_stopped_mutex = g_mutex_new ();

	tracker->file_metadata_thread = NULL;

	tracker->file_thread_signal = g_cond_new ();
	tracker->metadata_thread_signal = g_cond_new ();
	tracker->request_thread_signal = g_cond_new ();

	tracker->metadata_signal_mutex = g_mutex_new ();
	tracker->files_signal_mutex = g_mutex_new ();
	tracker->request_signal_mutex = g_mutex_new ();

	tracker->scheduler_mutex = g_mutex_new ();

	/* Remove an existing one */
	if (g_file_test (tracker->sys_tmp_root_dir, G_FILE_TEST_EXISTS)) {
		tracker_remove_dirs (tracker->sys_tmp_root_dir);
	}

	/* Remove old tracker dirs */
        old_tracker_dir = g_build_filename (g_get_home_dir (), ".Tracker", NULL);

	if (g_file_test (old_tracker_dir ,G_FILE_TEST_EXISTS)) {
		tracker_remove_dirs (old_tracker_dir);
	}

	g_free (old_tracker_dir);

        /* Create other directories we need */
	if (!g_file_test (tracker->user_data_dir, G_FILE_TEST_EXISTS)) {
		g_mkdir_with_parents (tracker->user_data_dir, 00755);
	}

	if (!g_file_test (tracker->data_dir, G_FILE_TEST_EXISTS)) {
		g_mkdir_with_parents (tracker->data_dir, 00755);
	}

        tracker->email_attachements_dir = g_build_filename (tracker->sys_tmp_root_dir, "Attachments", NULL);
	g_mkdir_with_parents (tracker->email_attachements_dir, 00700);
	tracker_log ("Made email attachments directory %s\n", tracker->email_attachements_dir);

	need_index = FALSE;
	need_data = FALSE;

	//tracker->cached_word_table_mutex = g_mutex_new ();

	tracker->dir_queue = g_async_queue_new ();

	tracker->xesam_dir = g_build_filename (g_get_home_dir (), ".xesam", NULL);

	if (reindex || tracker_db_needs_setup ()) {
		tracker_remove_dirs (tracker->data_dir);
		g_mkdir_with_parents (tracker->data_dir, 00755);
		need_index = TRUE;
	}


	need_data = tracker_db_needs_data ();

	umask (077);

	str = g_strconcat (g_get_user_name (), "_tracker_lock", NULL);

	

	/* check if setup for NFS usage (and enable atomic NFS safe locking) */
	//lock_str = tracker_get_config_option ("NFSLocking");
	lock_str = NULL;

	if (lock_str != NULL) {

		tracker->use_nfs_safe_locking = (strcmp (str, "1") == 0);

		/* place lock file in tmp dir to allow multiple sessions on NFS */
		lock_file = g_build_filename (tracker->sys_tmp_root_dir, str, NULL);

		g_free (lock_str);

	} else {

		tracker->use_nfs_safe_locking = FALSE;

		/* place lock file in home dir to prevent multiple sessions on NFS (as standard locking might be broken on NFS) */
		lock_file = g_build_filename (tracker->root_dir, str, NULL);
	}

	g_free (str);

	/* prevent muliple instances  */
	tracker->readonly = FALSE;

	lfp = g_open (lock_file, O_RDWR|O_CREAT, 0640);
	g_free (lock_file);

	if (lfp < 0) {
                g_warning ("Cannot open or create lockfile - exiting");
		exit (1);
	}

	if (lockf (lfp, F_TLOCK, 0) <0) {
		g_warning ("Tracker daemon is already running - attempting to run in readonly mode");
		tracker->readonly = TRUE;
	}

	/* Set child's niceness to 19 */
        errno = 0;
        /* nice() uses attribute "warn_unused_result" and so complains if we do not check its
           returned value. But it seems that since glibc 2.2.4, nice() can return -1 on a
           successful call so we have to check value of errno too. Stupid... */
        if (nice (19) == -1 && errno) {
                g_printerr ("ERROR: trying to set nice value\n");
        }

#ifdef IOPRIO_SUPPORT
	ioprio ();
#endif

	
	/* deal with config options with defaults, config file and option params */
	set_defaults ();

	tracker->battery_udi = NULL;

	if (error) {
		g_printerr ("invalid arguments: %s\n", error->message);
		return 1;
	}

	if (watch_dirs) {
                tracker_config_add_watch_directory_roots (tracker->config, watch_dirs);
	}

	if (crawl_dirs) {
                tracker_config_add_crawl_directory_roots (tracker->config, crawl_dirs);
	}

	if (no_watch_dirs) {
                tracker_config_add_no_watch_directory_roots (tracker->config, no_watch_dirs);
	}

	if (disable_indexing) {
		tracker_config_set_enable_indexing (tracker->config, FALSE);
	}

	if (language) {
		tracker_config_set_language (tracker->config, language);
	}

	if (throttle != -1) {
		tracker_config_set_throttle (tracker->config, throttle);
	}

	if (low_memory) {
		tracker_config_set_low_memory_mode (tracker->config, TRUE);
	}

	if (verbosity != 0) {
		tracker_config_set_verbosity (tracker->config, verbosity);
	}

	if (initial_sleep >= 0) {
		tracker_config_set_initial_sleep (tracker->config, initial_sleep);
	}

	tracker->fatal_errors = fatal_errors;

	sanity_check_option_values ();

        /* Initialize the service manager */
        tracker_service_manager_init ();

	/* set thread safe DB connection */
	tracker_db_thread_init ();

	if (!tracker_db_initialize (tracker->data_dir)) {
		tracker_log ("ERROR: failed to initialise database engine - exiting...");
		return 1;
	}

	/* create cache db */
	DBConnection *db2 = tracker_db_connect_cache ();
	tracker_db_close (db2);


	if (need_data) {
		tracker_create_db ();
	}

	/* create database if needed */
	if (!tracker->readonly && need_index) {

		create_index (need_data);

	} else {
		tracker->first_time_index = FALSE;
	}

	
	db_con = tracker_db_connect ();
	db_con->thread = "main";


	/* check db integrity if not previously shut down cleanly */
	if (!tracker->readonly && !need_index && tracker_db_get_option_int (db_con, "IntegrityCheck") == 1) {

		tracker_log ("performing integrity check as trackerd was not shutdown cleanly");


/*		turn off corruption check as it can hog cpu for long time 

		if (!tracker_db_integrity_check (db_con) || !tracker_indexer_repair ("file-index.db") || !tracker_indexer_repair ("email-index.db")) {
			tracker_error ("db or index corruption detected - prepare for reindex...");
			tracker_db_close (db_con);	

			tracker_remove_dirs (tracker->data_dir);
			g_mkdir_with_parents (tracker->data_dir, 00755);
			create_index (need_data);
			db_con = tracker_db_connect ();
			db_con->thread = "main";

		}
*/
	} 

	
	if (!tracker->readonly) {
		tracker_db_set_option_int (db_con, "IntegrityCheck", 1);
	} 

	if (tracker->first_time_index) {
		tracker_db_set_option_int (db_con, "InitialIndex", 1);
	}

	db_con->cache = tracker_db_connect_cache ();
	db_con->common = tracker_db_connect_common ();
	db_con->index = db_con;

	main_thread_db_con = db_con;
	
	/* move final file to index file if present and no files left to merge */
	char *final_index_name = g_build_filename (tracker->data_dir, "file-index-final", NULL);
	
	if (g_file_test (final_index_name, G_FILE_TEST_EXISTS) && !tracker_indexer_has_tmp_merge_files (INDEX_TYPE_FILES)) {
	
		char *file_index_name = g_build_filename (tracker->data_dir, "file-index.db", NULL);
	
		tracker_log ("overwriting %s with %s", file_index_name, final_index_name);	

		rename (final_index_name, file_index_name);
		
		g_free (file_index_name);
	}
	
	g_free (final_index_name);
	
	final_index_name = g_build_filename (tracker->data_dir, "email-index-final", NULL);
	
	if (g_file_test (final_index_name, G_FILE_TEST_EXISTS) && !tracker_indexer_has_tmp_merge_files (INDEX_TYPE_EMAILS)) {
	
		char *file_index_name = g_build_filename (tracker->data_dir, "email-index.db", NULL);
	
		tracker_log ("overwriting %s with %s", file_index_name, final_index_name);	
	
		rename (final_index_name, file_index_name);
		
		g_free (file_index_name);
	}
	
	g_free (final_index_name);
	
	

	Indexer *index = tracker_indexer_open ("file-index.db", TRUE);
	tracker->file_index = index;

	index = tracker_indexer_open ("file-update-index.db", FALSE);
	tracker->file_update_index = index;

	index = tracker_indexer_open ("email-index.db", TRUE);
	tracker->email_index = index;

	db_con->word_index = tracker->file_index;

	tracker->update_count = get_update_count (main_thread_db_con);

	tracker_db_get_static_data (db_con);

	tracker->file_metadata_queue = g_async_queue_new ();

	if (!tracker->readonly) {
		tracker->file_process_queue = g_async_queue_new ();
	}

	tracker->user_request_queue = g_async_queue_new ();

	tracker_email_init ();

  	tracker->loop = g_main_loop_new (NULL, TRUE);

#ifdef HAVE_HAL 
        /* Create tracker HAL object */
 	tracker->hal = tracker_hal_new ();       
#endif

	/* this var is used to tell the threads when to quit */
	tracker->is_running = TRUE;

        g_thread_create_full ((GThreadFunc) tracker_process_requests, 
                              tracker,
                              (gulong) tracker_config_get_thread_stack_size (tracker->config),
                              FALSE, 
                              FALSE, 
                              G_THREAD_PRIORITY_NORMAL,  
                              NULL);

	if (!tracker->readonly) {
		if (!tracker_start_watching ()) {
			tracker_error ("ERROR: file monitoring failed to start");
			tracker_do_cleanup ("File watching failure");
			exit (1);
		}

                g_thread_create_full ((GThreadFunc) tracker_process_files, 
                                      tracker,
                                      (gulong) tracker_config_get_thread_stack_size (tracker->config),
                                      FALSE, 
                                      FALSE, 
                                      G_THREAD_PRIORITY_NORMAL, 
                                      NULL);
	}
	
	g_main_loop_run (tracker->loop);

	return EXIT_SUCCESS;
}
