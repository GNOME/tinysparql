/* Tracker
 * Copyright (C) 2005, Mr Jamie McCracken (jamiemcc@gnome.org)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#define DBUS_API_SUBJECT_TO_CHANGE
#define I_AM_MAIN

#include "config.h"

#include <locale.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <glib/gstdio.h>

#ifdef HAVE_INOTIFY
#   include "tracker-inotify.h"
#else
#   ifdef HAVE_FAM
#      include "tracker-fam.h"
#   endif
#endif

#ifndef HAVE_INOTIFY
#   ifndef HAVE_FAM
#      define POLL_ONLY
#   endif
#endif

#include "tracker-db.h"
#include "tracker-mbox.h"
#include "tracker-metadata.h"

#include "tracker-dbus-methods.h"
#include "tracker-dbus-metadata.h"
#include "tracker-dbus-keywords.h"
#include "tracker-dbus-search.h"
#include "tracker-dbus-files.h"

#include  "tracker-indexer.h"

Tracker		       *tracker;
DBConnection	       *main_thread_db_con;

static gboolean	       shutdown;
static DBusConnection  *main_connection;


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

#define FILE_POLL_PERIOD (30 * 60 * 1000)

char *type_array[] =   {"index", "string", "numeric", "date", NULL};


static void schedule_file_check (const char *uri, DBConnection *db_con);

static void delete_directory (DBConnection *db_con, FileInfo *info);

static void delete_file (DBConnection *db_con, FileInfo *info);

static void scan_directory (const char *uri, DBConnection *db_con);


#ifdef POLL_ONLY

#define MAX_FILE_WATCHES (-1)

 gboolean 	tracker_start_watching 		(void){return TRUE;}
 void     	tracker_end_watching 		(void){return;}

 gboolean 	tracker_add_watch_dir 		(const char *dir, DBConnection *db_con){return FALSE;}
 void     	tracker_remove_watch_dir 	(const char *dir, gboolean delete_subdirs, DBConnection *db_con) {return;}

 gboolean 	tracker_is_directory_watched 	(const char *dir, DBConnection *db_con) {return FALSE;}
 int		tracker_count_watch_dirs 	(void) {return 0;}
#endif /* POLL_ONLY */


static void
my_yield (void)
{
	while (g_main_context_iteration (NULL, FALSE)) {
		;
	}
}


static int
has_prefix (const char *str1, const char *str2)
{
	if (strcmp (str1, str2) == 0) {
		return 0;
	} else {
		char *compare_str;

		compare_str = g_strconcat (str1, G_DIR_SEPARATOR_S, NULL);

		if (g_str_has_prefix (str2, compare_str)) {
			return 0;
		}
		g_free (compare_str);
		return 1;
	}
}


static gboolean
do_cleanup (const char *sig_msg)
{
	if (tracker->log_file) {
		tracker_log ("Received signal '%s' so now shutting down", sig_msg);

		tracker_print_object_allocations ();
	}

	/* clear pending files and watch tables*/
	//tracker_db_clear_temp (main_thread_db_con);

	/* stop threads from further processing of events if possible */

	tracker_dbus_shutdown (main_connection);

	tracker_indexer_close (tracker->file_indexer);

	//tracker_indexer_close (file_indexer);
	//tracker_db_clear_temp (main_thread_db_con);

	/* wait for files thread to sleep */
	while (!g_mutex_trylock (tracker->files_signal_mutex)) {
		g_usleep (100);
	}

	g_mutex_unlock (tracker->files_signal_mutex);


	while (!g_mutex_trylock (tracker->metadata_signal_mutex)) {
		g_usleep (100);
	}

	g_mutex_unlock (tracker->metadata_signal_mutex);

	if (tracker->log_file) {
		tracker_log ("shutting down threads");
	}

	/* send signals to each thread to wake them up and then stop them */

	shutdown = TRUE;

	g_mutex_lock (tracker->poll_signal_mutex);
	g_cond_signal (tracker->poll_thread_signal);
	g_mutex_unlock (tracker->poll_signal_mutex);

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

	g_mutex_lock (tracker->poll_stopped_mutex);

	g_mutex_unlock (tracker->request_check_mutex);
	g_mutex_lock (tracker->request_stopped_mutex);

	g_mutex_unlock (tracker->metadata_check_mutex);
	g_mutex_lock (tracker->metadata_stopped_mutex);

	g_mutex_unlock (tracker->files_check_mutex);
	g_mutex_lock (tracker->files_stopped_mutex);

	tracker_end_email_watching ();

	tracker_db_close (main_thread_db_con);

	/* This must be called after all other db functions */
	tracker_db_finalize ();

	if (tracker->log_file) {
		tracker_log ("shutting down main thread");
	}

	g_main_loop_quit (tracker->loop);

	exit (EXIT_SUCCESS);

	return FALSE;
}


static void
poll_dir (const char *uri, DBConnection *db_con)
{
	char	**files, **files_p;
	GSList	*file_list, *tmp;

	if (!tracker->is_running) {
		return;
	}


	/* check for any deletions*/
	files = tracker_db_get_files_in_folder (db_con, uri);

	for (files_p = files; *files_p; files_p++) {
		FileInfo *info;
		char	 *str;

		str = *files_p;
		tracker_log ("polling %s", str);

		if (!tracker_file_is_valid (str)) {
			info = tracker_create_file_info (str, 1, 0, 0);
			info = tracker_db_get_file_info (db_con, info);

			if (!info->is_directory) {
				delete_file (db_con, info);
			} else {
				delete_directory (db_con, info);
			}
			tracker_free_file_info (info);
		}
	}

	g_strfreev (files);


	/* check for new subfolder additions */
	file_list = tracker_get_files (uri, TRUE);

	for (tmp = file_list; tmp; tmp = tmp->next) {
		FileInfo *info;
		char	 *str;

		if (!tracker->is_running) {
			g_slist_foreach (file_list, (GFunc) g_free, NULL);
			g_slist_free (file_list);
			return;
		}

		str = (char *) tmp->data;

		info = tracker_create_file_info (str, TRACKER_ACTION_DIRECTORY_CREATED, 0, 0);
		info = tracker_db_get_file_info (db_con, info);

		if (info->file_id == -1) {
			tracker_db_insert_pending_file (db_con, info->file_id, info->uri, info->mime, 0, info->action, info->is_directory, TRUE);
		} else {
			tracker_free_file_info (info);
		}
	}

	g_slist_foreach (file_list, (GFunc) g_free, NULL);
	g_slist_free (file_list);


	/* scan dir for changes in all other files */
	if (g_slist_find_custom (tracker->no_watch_directory_list, uri, (GCompareFunc) has_prefix) == NULL) {
		scan_directory (uri, db_con);
	} else {
		tracker_log ("blocked scan of directory %s as its in the no watch list", uri);
	}
}


static void
poll_directories (gpointer db_con)
{
	g_return_if_fail (db_con);

	tracker_log ("polling dirs");

	if (g_slist_length (tracker->poll_list) > 0) {
		g_slist_foreach (tracker->poll_list, (GFunc) poll_dir, db_con);
	}
}


static void
poll_files_thread (void)
{
	sigset_t     signal_set;
	DBConnection *db_con;

        /* block all signals in this thread */
        sigfillset (&signal_set);
	pthread_sigmask (SIG_BLOCK, &signal_set, NULL);

	g_mutex_lock (tracker->poll_signal_mutex);
	g_mutex_lock (tracker->poll_stopped_mutex);

	/* set thread safe DB connection */
	tracker_db_thread_init ();

	db_con = tracker_db_connect ();

	while (TRUE) {

		/* make thread sleep if first part of the shutdown process has been activated */
		if (!tracker->is_running) {

			tracker_log ("poll thread going to deep sleep...");

			g_cond_wait (tracker->poll_thread_signal, tracker->poll_signal_mutex);

			/* determine if wake up call is new stuff or a shutdown signal */
			if (!shutdown) {
				continue;
			} else {
				break;
			}
		}

		poll_directories (db_con);

		/* sleep until notified again */
		tracker_log ("poll thread sleeping");
		g_cond_wait (tracker->poll_thread_signal, tracker->poll_signal_mutex);
		tracker_log ("poll thread awoken");

		/* determine if wake up call is a shutdown signal or a request to poll again */
		if (!shutdown) {
			continue;
		} else {
			break;
		}
	}

	tracker_db_close (db_con);
	tracker_db_thread_end ();

	tracker_log ("poll thread has exited successfully");

	g_mutex_unlock (tracker->poll_stopped_mutex);
}


static gboolean
start_poll (void)
{
	if (!tracker->file_poll_thread && g_slist_length (tracker->poll_list) > 0) {
		tracker->file_poll_thread = g_thread_create ((GThreadFunc) poll_files_thread, NULL, FALSE, NULL);
		tracker_log ("started polling");
	} else {

		/* wake up poll thread and start polling */
		if (tracker->file_poll_thread && g_slist_length (tracker->poll_list) > 0) {
			if (g_mutex_trylock (tracker->poll_signal_mutex)) {
				g_cond_signal (tracker->poll_thread_signal);
				g_mutex_unlock (tracker->poll_signal_mutex);
			}
		}
	}

	return TRUE;
}


static void
add_dirs_to_watch_list (GSList *dir_list, gboolean check_dirs, DBConnection *db_con)
{
	gboolean start_polling;

	if (!tracker->is_running) {
		return;
	}

	g_return_if_fail (dir_list != NULL);

	start_polling = FALSE;

	/* add sub directories breadth first recursively to avoid running out of file handles */
	while (g_slist_length (dir_list) > 0) {
		GSList *file_list;

		file_list = NULL;

		if (!start_polling && ((tracker_count_watch_dirs () + g_slist_length (dir_list)) < MAX_FILE_WATCHES)) {
			GSList *tmp;

			for (tmp = dir_list; tmp; tmp = tmp->next) {
				char *str;

				str = (char *) tmp->data;

				if (g_slist_find_custom (tracker->no_watch_directory_list, str, (GCompareFunc) has_prefix) == NULL) {

					/* use polling if FAM or Inotify fails */
					if (!tracker_add_watch_dir (str, db_con) && tracker_is_directory (str) && !tracker_is_dir_polled (str)) {
						if (tracker->is_running) {
							tracker_add_poll_dir (str);
						}
					}
				}
			}

		} else {
			g_slist_foreach (dir_list, (GFunc) tracker_add_poll_dir, NULL);
		}

		g_slist_foreach (dir_list, (GFunc) tracker_get_dirs, &file_list);

		if (check_dirs) {
			g_slist_foreach (file_list, (GFunc) schedule_file_check, db_con);
		}

		g_slist_foreach (dir_list, (GFunc) g_free, NULL);
		g_slist_free (dir_list);

		if (tracker_count_watch_dirs () > MAX_FILE_WATCHES) {
			start_polling = TRUE;
		}

		dir_list = file_list;
	}
}


static gboolean
watch_dir (const char* dir, DBConnection *db_con)
{
	char *dir_utf8;

	if (!tracker->is_running) {
		return TRUE;
	}

	g_assert (dir);

	if (!g_utf8_validate (dir, -1, NULL)) {

		dir_utf8 = g_filename_to_utf8 (dir, -1, NULL,NULL,NULL);
		if (!dir_utf8) {
			tracker_log ("******ERROR**** watch_dir could not be converted to utf8 format");
			return FALSE;
		}
	} else {
		dir_utf8 = g_strdup (dir);
	}

	if (!tracker_file_is_valid (dir_utf8)) {
		g_free (dir_utf8);
		return FALSE;
	}

	if (!tracker_is_directory (dir_utf8)) {
		g_free (dir_utf8);
		return FALSE;
	}

	if (g_slist_find_custom (tracker->no_watch_directory_list, dir_utf8, (GCompareFunc) has_prefix) == NULL) {
		GSList *mylist;

		mylist = NULL;

		mylist = g_slist_prepend (mylist, dir_utf8);

		add_dirs_to_watch_list (mylist, TRUE, db_con);
	}

	return TRUE;
}


static void
signal_handler (int signo)
{
	if (!tracker->is_running) {
		return;
	}

	static gboolean in_loop = FALSE;

  	/* avoid re-entrant signals handler calls */
	if (in_loop) {
		return;
	}

  	in_loop = TRUE;

  	switch (signo) {

  		case SIGSEGV:
	  	case SIGBUS:
		case SIGILL:
  		case SIGFPE:
  		case SIGPIPE:
		case SIGABRT:

			tracker->is_running = FALSE;
			tracker_end_watching ();

			g_timeout_add_full (G_PRIORITY_LOW,
			     		    1,
		 	    		    (GSourceFunc) do_cleanup,
			     		    g_strdup (g_strsignal (signo)), NULL
			   		    );

			exit (1);
    			break;

		case SIGTERM:
		case SIGINT:

			tracker->is_running = FALSE;
			tracker_end_watching ();

			g_timeout_add_full (G_PRIORITY_LOW,
			     		    1,
		 	    		    (GSourceFunc) do_cleanup,
			     		    g_strdup (g_strsignal (signo)), NULL
			   		    );

			break;

		default:
			if (tracker->log_file) {
	   			tracker_log ("Received signal %s ", g_strsignal (signo));
			}
			in_loop = FALSE;
    			break;
  	}
}


static void
delete_file (DBConnection *db_con, FileInfo *info)
{
	/* info struct may have been deleted in transit here so check if still valid and intact */
	g_return_if_fail (tracker_file_info_is_valid (info));

	/* if we dont have an entry in the db for the deleted file, we ignore it */
	if (info->file_id == -1) {
		return;
	}

	tracker_db_delete_file (db_con, info->file_id);

	tracker_log ("deleting file %s", info->uri);
}


static void
delete_directory (DBConnection *db_con, FileInfo *info)
{
	/* info struct may have been deleted in transit here so check if still valid and intact */
	g_return_if_fail (tracker_file_info_is_valid (info));

	/* if we dont have an entry in the db for the deleted directory, we ignore it */
	if (info->file_id == -1) {
		return;
	}

	tracker_db_delete_directory (db_con, info->file_id, info->uri);

	tracker_remove_watch_dir (info->uri, TRUE, db_con);

	tracker_remove_poll_dir (info->uri);

	tracker_log ("deleting directory %s and subdirs", info->uri);
}


static void
index_file (DBConnection *db_con, FileInfo *info)
{
	char	   *str_link_uri, *str_file_id;
	char	   *name, *path;
	GHashTable *meta_table;

	gboolean is_a_mbox, is_an_email_attachment;

	if (!tracker->is_running) {
		return;
	}

	/* the file being indexed or info struct may have been deleted in transit so check if still valid and intact */
	g_return_if_fail (tracker_file_info_is_valid (info));

	is_a_mbox = FALSE;
	is_an_email_attachment = FALSE;

	if (tracker_is_in_a_application_mail_dir (info->uri)) {
		if (tracker_is_a_handled_mbox_file (info->uri)) {
			is_a_mbox = TRUE;
		} else {
			/* we get a non mbox file */
			return;
		}
	} else if (tracker_is_an_email_attachment (info->uri)) {
		is_an_email_attachment = TRUE;
	}

	if (!tracker_file_is_valid (info->uri)) {
		tracker_log ("Warning - file %s no longer exists - abandoning index on this file", info->uri);
		return;
	}

	/* refresh DB data as previous stuff might be out of date by the time we get here */
	info = tracker_db_get_file_info (db_con, info);

	if (info->mime) {
		g_free (info->mime);
	}

	if (is_a_mbox || is_an_email_attachment) {
		if (info->is_directory) {
			tracker_log ("******ERROR**** a mbox or an email attachment is detected as directory");
			return;
		}

		info->mime = tracker_get_mime_type (info->uri);
	} else {
		if (!info->is_directory) {
			info->mime = tracker_get_mime_type (info->uri);
		} else {
			info->mime = g_strdup ("Folder");
			info->file_size = 0;
		}
	}

	if (info->is_link) {
		str_link_uri = g_build_filename (info->link_path, info->link_name, NULL);
	} else {
		str_link_uri = g_strdup (" ");
	}

	name = g_path_get_basename (info->uri);
	path = g_path_get_dirname (info->uri);

	if (!is_a_mbox) {
		char *str_mtime, *str_atime;

		str_mtime = tracker_date_to_str (info->mtime);
		str_atime = tracker_date_to_str (info->atime);

		meta_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

		g_hash_table_insert (meta_table, g_strdup ("File.Path"), g_strdup (path));
		g_hash_table_insert (meta_table, g_strdup ("File.Name"), g_strdup (name));
		g_hash_table_insert (meta_table, g_strdup ("File.Link"), g_strdup (str_link_uri));
		g_hash_table_insert (meta_table, g_strdup ("File.Format"), g_strdup (info->mime));
		g_hash_table_insert (meta_table, g_strdup ("File.Size"), tracker_long_to_str (info->file_size));
		g_hash_table_insert (meta_table, g_strdup ("File.Permissions"), g_strdup (info->permissions));
		g_hash_table_insert (meta_table, g_strdup ("File.Modified"), g_strdup (str_mtime));
		g_hash_table_insert (meta_table, g_strdup ("File.Accessed"), g_strdup (str_atime));

		g_free (str_mtime);
		g_free (str_atime);

	} else {
		meta_table = NULL;
	}

	str_file_id = tracker_long_to_str (info->file_id);

	/* work out whether to insert or update file (file_id of -1 means no existing record so insert) */
	if (info->file_id == -1) {
		char *service_name;

		if (is_a_mbox) {
			service_name = g_strdup ("Email");
		} else if (is_an_email_attachment) {
			service_name = g_strdup ("EmailAttachments");
		} else {
			if (info->is_directory) {
				service_name = g_strdup ("Folders");
			} else {
				service_name = tracker_get_service_type_for_mime (info->mime);
			}
		}

		tracker_db_create_service (db_con, path, name, service_name, info->is_directory, info->is_link, FALSE, 0, info->mtime);

		
		info->file_id = tracker_db_get_last_id (db_con);
		info->service_type_id = tracker_get_id_for_service (service_name);

 		//info->file_id = tracker_db_get_file_id (db_con, info->uri);

		//tracker_log ("created new service ID %d for %s with mime %s and service %s and mtime %d", info->file_id, info->uri, info->mime, service_name, info->mtime);

		g_free (service_name);


		info->is_new = TRUE;

	} else {
		info->is_new = FALSE;
		tracker_db_update_file (db_con, info->file_id, info->mtime);

		/* delete all derived metadata in DB for an updated file */
		//tracker_exec_proc (db_con, "DeleteEmbeddedServiceMetadata", 1, str_file_id);
	}


	if (info->file_id != -1) {
		if (is_a_mbox) {

			off_t	    offset;
			MailBox	    *mb;
			MailMessage *msg;

			offset = tracker_db_get_last_mbox_offset (db_con, info->uri);

			mb = tracker_mbox_parse_from_offset (info->uri, offset);

			while ((msg = tracker_mbox_parse_next (mb))) {

				if (!tracker->is_running) {
					tracker_free_mail_message (msg);
					tracker_close_mbox_and_free_memory (mb);

					g_free (name);
					g_free (path);

					g_free (str_file_id);
					g_free (str_link_uri);

					return;
				}

				tracker_db_update_mbox_offset (db_con, mb);

				tracker_db_save_email (db_con, msg);

				tracker_index_each_email_attachment (db_con, msg);

				tracker_free_mail_message (msg);
			}

			tracker_close_mbox_and_free_memory (mb);

		} else {

			if (info->is_new) {
				tracker_log ("saving basic metadata for *new* file %s", info->uri);
			} else {
				tracker_log ("saving basic metadata for *existing* file %s", info->uri);
			}

			tracker_db_save_metadata (db_con, meta_table, info->file_id, info->is_new);

			g_hash_table_destroy (meta_table);
		}
	}

	g_free (name);
	g_free (path);

	g_free (str_file_id);
	g_free (str_link_uri);


	if (!info->is_directory && info->file_id != -1 && !is_a_mbox && (info->service_type_id <= 7)) {
		tracker_db_insert_pending_file (db_con, info->file_id, info->uri, info->mime, 0, TRACKER_ACTION_EXTRACT_METADATA, info->is_directory, info->is_new);
		return;
	}
	

	if (info->file_id == -1) {
		tracker_log ("FILE %s NOT FOUND IN DB!", info->uri);
	} else {
		if (info->is_new) {
			tracker_db_update_indexes_for_new_service (db_con, info->file_id, NULL);
		}
	}
}


static GSList *
remove_no_watch_dirs (GSList *list)
{
	const GSList *tmp;

	if (!tracker->is_running) {
		return NULL;
	}

	for (tmp = list; tmp; tmp = tmp->next) {
		char *str;

		str = (char *) tmp->data;

		if (g_slist_find_custom (tracker->no_watch_directory_list, str, (GCompareFunc) has_prefix) == NULL) {
			tracker_log ("removing %s from scan list", str);
			list = g_slist_remove (list, str);
			g_free (str);
			str = NULL;
		}
	}

	return list;
}


static void
schedule_file_check (const char *uri, DBConnection *db_con)
{
	if (!tracker->is_running) {
		return;
	}

	g_return_if_fail (uri);
	g_return_if_fail (db_con);

	/* keep mainloop responsive */
	my_yield ();
	tracker_db_insert_pending_file (db_con, -1, uri, "unknown", 0, TRACKER_ACTION_CHECK, 0, FALSE);
}


static void
scan_directory (const char *uri, DBConnection *db_con)
{
	GSList *file_list;

	if (!tracker->is_running) {
		return;
	}

	g_return_if_fail (db_con);
	g_return_if_fail (uri);
	g_return_if_fail (tracker_is_directory (uri));

	/* keep mainloop responsive */
	my_yield ();

	file_list = tracker_get_files (uri, FALSE);

	g_slist_foreach (file_list, (GFunc) schedule_file_check, db_con);
	g_slist_foreach (file_list, (GFunc) g_free, NULL);
	g_slist_free (file_list);

	/* recheck directory to update its mtime if its changed whilst scanning */
	schedule_file_check (uri, db_con);
}


static gboolean
start_watching (gpointer data)
{
	if (!tracker->is_running) {
		return FALSE;
	}

	if (!tracker_start_watching ()) {
		tracker_log ("File monitoring failed to start");
		do_cleanup ("File watching failure");
		exit (1);
	} else {

		/* start emails watching */
		if (tracker->index_emails) {
			tracker_watch_emails (main_thread_db_con);
		}

		if (data) {
			char *watch_folder;
			int  len;

			watch_folder = (char *) data;

			len = strlen (watch_folder);

			if (watch_folder[len-1] == G_DIR_SEPARATOR) {
 				watch_folder[len-1] = '\0';
			}

			watch_dir (watch_folder, main_thread_db_con);
			schedule_file_check (watch_folder, main_thread_db_con);
			g_free (watch_folder);

		} else {
			g_slist_foreach (tracker->watch_directory_roots_list, (GFunc) watch_dir, main_thread_db_con);
 			g_slist_foreach (tracker->watch_directory_roots_list, (GFunc) schedule_file_check, main_thread_db_con);
		}

		tracker_log ("waiting for file events...");
	}

	return FALSE;
}


static void
extract_metadata_thread (void)
{
	sigset_t     signal_set;
	DBConnection *db_con;

        /* block all signals in this thread */
        sigfillset (&signal_set);
        pthread_sigmask (SIG_BLOCK, &signal_set, NULL);

	g_mutex_lock (tracker->metadata_signal_mutex);
	g_mutex_lock (tracker->metadata_stopped_mutex);

	/* set thread safe DB connection */
	tracker_db_thread_init ();

	db_con = tracker_db_connect ();

	db_con->thread = "extract";

	tracker_db_prepare_queries (db_con);

	while (TRUE) {
		FileInfo *info;

		/* make thread sleep if first part of the shutdown process has been activated */
		if (!tracker->is_running) {

			tracker_log ("metadata thread going to deep sleep...");

			g_cond_wait (tracker->metadata_thread_signal, tracker->metadata_signal_mutex);

			/* determine if wake up call is new stuff or a shutdown signal */
			if (!shutdown) {
				continue;
			} else {
				break;
			}
		}

		info = g_async_queue_try_pop (tracker->file_metadata_queue);

		/* check pending table if we haven't got anything */
		if (!info) {
			char ***res;
			int  k;

			/* set mutex to indicate we are in "check" state */
			g_mutex_lock (tracker->metadata_check_mutex);

			if (tracker_db_has_pending_metadata (db_con)) {
				g_mutex_unlock (tracker->metadata_check_mutex);
			} else {
				//tracker_log ("metadata thread sleeping");

				/* we have no stuff to process so sleep until awoken by a new signal */
				g_cond_wait (tracker->metadata_thread_signal, tracker->metadata_signal_mutex);
				g_mutex_unlock (tracker->metadata_check_mutex);
				//tracker_log ("metadata thread awoken");
				/* determine if wake up call is new stuff or a shutdown signal */
				if (!shutdown) {
					continue;
				} else {
					break;
				}
			}

			res = tracker_db_get_pending_metadata (db_con);

			k = 0;

			if (res) {
				char **row;

				while ((row = tracker_db_get_row (res, k))) {
					FileInfo *info_tmp;

					if (!tracker->is_running) {
						break;
					}

					k++;
					info_tmp = tracker_create_file_info (row[1], atoi(row[2]), 0, WATCH_OTHER);
					info_tmp->file_id = atol (row[0]);
					info_tmp->mime = g_strdup (row[3]);
					info_tmp->is_new = (strcmp (row[5], "1") == 0);

					g_async_queue_push (tracker->file_metadata_queue, info_tmp);
				}

				if (tracker->is_running) {
					tracker_db_free_result (res);
				}
			}

			if (tracker->is_running) {
				tracker_db_remove_pending_metadata (db_con);
			}

			continue;
		}


		/* info struct may have been deleted in transit here so check if still valid and intact */
		if (!tracker_file_info_is_valid (info)) {
			continue;
		}

		if (info) {
			if (info->uri) {
				char *file_as_text;

				GHashTable *meta_table;

				if (info->is_new) {
					tracker_log ("Extracting Metadata for *new* file %s with mime %s", info->uri, info->mime);
				} else {
					tracker_log ("Extracting Metadata for *existing* file %s with mime %s", info->uri, info->mime);
				}

				/* refresh stat data in case its changed */
				info = tracker_get_file_info (info);

				if (info->file_id == -1) {
					info->file_id = tracker_db_get_file_id (db_con, info->uri);
				}

				meta_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

				tracker_metadata_get_embedded (info->uri, info->mime, meta_table);

				if (g_hash_table_size (meta_table) > 0) {
					tracker_db_save_metadata (db_con, meta_table, info->file_id, info->is_new);

					/* to do - emit dbus signal here for EmbeddedMetadataChanged */
				}

				g_hash_table_destroy (meta_table);

				if (tracker->do_thumbnails) {
					char *small_thumb_file;

					/* see if there is a thumbnailer script for the file's mime type */

					small_thumb_file = tracker_metadata_get_thumbnail (info->uri, info->mime, THUMB_SMALL);

					if (small_thumb_file) {
						char *large_thumb_file;

						large_thumb_file = tracker_metadata_get_thumbnail (info->uri, info->mime, THUMB_LARGE);

						if (large_thumb_file) {

							tracker_db_save_thumbs (db_con, small_thumb_file, large_thumb_file, info->file_id);

							/* to do - emit dbus signal ThumbNailChanged */
							g_free (large_thumb_file);
						}

						g_free (small_thumb_file);
					}
		
				}

				file_as_text = tracker_metadata_get_text_file (info->uri, info->mime);

				if (file_as_text) {
					const char *tmp_dir;
					char	   *dir;

					//tracker_log ("text file is %s", file_as_text);
			
					tracker_db_save_file_contents (db_con, file_as_text, info);

					tmp_dir = g_get_tmp_dir ();

					dir = g_strconcat (tmp_dir, G_DIR_SEPARATOR_S, NULL);

					/* clear up if text contents are in a temp file */
					if (g_str_has_prefix (file_as_text, dir)) {
						g_unlink (file_as_text);
					}

					g_free (dir);

					g_free (file_as_text);

				} else {
					if (info->is_new) {
						tracker_db_update_indexes_for_new_service (db_con, info->file_id, NULL);
					}
				}

				

				if (tracker_is_an_email_attachment (info->uri)) {
					tracker_unlink_email_attachment (info->uri);
				}
			}

			tracker_dec_info_ref (info);
		}
	}

	tracker_db_close (db_con);
	tracker_db_thread_end ();

	tracker_log ("metadata thread has exited successfully");
	g_mutex_unlock (tracker->metadata_stopped_mutex);
}


/* determines whether an action applies to a file or a directory */
static void
verify_action (FileInfo *info)
{
	if (info->action == TRACKER_ACTION_CHECK) {
		if (info->is_directory) {
			info->action = TRACKER_ACTION_DIRECTORY_CHECK;
			info->counter = 0;
		} else {
			info->action = TRACKER_ACTION_FILE_CHECK;
		}

	} else {
		if (info->action == TRACKER_ACTION_DELETE || info->action == TRACKER_ACTION_DELETE_SELF) {

			/* we are in trouble if we cant find the deleted uri in the DB - assume its a directory (worst case) */
			if (info->file_id == -1) {
				info->is_directory = TRUE;
			}

			info->counter = 0;
			if (info->is_directory) {
				info->action = TRACKER_ACTION_DIRECTORY_DELETED;
			} else {
				info->action = TRACKER_ACTION_FILE_DELETED;
			}
		} else {
			if (info->action == TRACKER_ACTION_MOVED_FROM) {
				info->counter = 1;
				if (info->is_directory) {
					info->action = TRACKER_ACTION_DIRECTORY_MOVED_FROM;
				} else {
					info->action = TRACKER_ACTION_FILE_MOVED_FROM;
				}

			} else {

				if (info->action == TRACKER_ACTION_CREATE) {
					if (info->is_directory) {
						info->action = TRACKER_ACTION_DIRECTORY_CREATED;
						info->counter = 0; /* do not reschedule a created directory */
					} else {
						info->action = TRACKER_ACTION_FILE_CREATED;
					}

				} else {
					if (info->action == TRACKER_ACTION_FILE_MOVED_TO) {
						info->counter = 0;
						if (info->is_directory) {
							info->action = TRACKER_ACTION_DIRECTORY_MOVED_TO;
						} else {
							info->action = TRACKER_ACTION_FILE_MOVED_TO;
						}
					}
				}
			}
		}
	}
}


static void
process_files_thread (void)
{
	sigset_t     signal_set;
	DBConnection *db_con;
	GSList	     *moved_from_list; /* list to hold moved_from events whilst waiting for a matching moved_to event */

        /* block all signals in this thread */
        sigfillset (&signal_set);
        pthread_sigmask (SIG_BLOCK, &signal_set, NULL);

	g_mutex_lock (tracker->files_signal_mutex);
	g_mutex_lock (tracker->files_stopped_mutex);

	/* set thread safe DB connection */
	tracker_db_thread_init ();

	db_con = tracker_db_connect ();

	db_con->thread = "files";

	moved_from_list = NULL;

	while (TRUE) {
		FileInfo *info;
		gboolean need_index;

		need_index = FALSE;

		/* make thread sleep if first part of the shutdown process has been activated */
		if (!tracker->is_running) {

			tracker_log ("files thread going to deep sleep...");

			g_cond_wait (tracker->file_thread_signal, tracker->files_signal_mutex);

			/* determine if wake up call is new stuff or a shutdown signal */
			if (!shutdown) {
				continue;
			} else {
				break;
			}
		}

		info = g_async_queue_try_pop (tracker->file_process_queue);

		/* check pending table if we haven't got anything */
		if (!info) {
			char ***res;
			int  k;

			/* set mutex to indicate we are in "check" state */
			g_mutex_lock (tracker->files_check_mutex);


			if (tracker_db_has_pending_files (db_con)) {
				g_mutex_unlock (tracker->files_check_mutex);
			} else {
				//tracker_log ("File thread sleeping");

				/* we have no stuff to process so sleep until awoken by a new signal */
				g_cond_wait (tracker->file_thread_signal, tracker->files_signal_mutex);
				g_mutex_unlock (tracker->files_check_mutex);

				//tracker_log ("File thread awoken");

				/* determine if wake up call is new stuff or a shutdown signal */
				if (!shutdown) {
					continue;
				} else {
					break;
				}
			}

			res = tracker_db_get_pending_files (db_con);

			k = 0;

			if (res) {
				char **row;

				while ((row = tracker_db_get_row (res, k))) {
					FileInfo	    *info_tmp;
					TrackerChangeAction tmp_action;

					if (!tracker->is_running) {
						break;
					}

					k++;

					tmp_action = atoi(row[2]);

					

					if (tmp_action != TRACKER_ACTION_CHECK) {
						tracker_log ("processing %s with event %s", row[1], tracker_actions[tmp_action]);
					}

					info_tmp = tracker_create_file_info (row[1], tmp_action, 0, WATCH_OTHER);
					g_async_queue_push (tracker->file_process_queue, info_tmp);
				}

				if (tracker->is_running) {
					tracker_db_free_result (res);
				}
			}

			if (!tracker->is_running) {
				continue;
			}

			tracker_db_remove_pending_files (db_con);

			/* pending files are present but not yet ready as we are waiting til they stabilize so we should sleep for 100ms (only occurs when using FAM) */
			if (k == 0) {
				tracker_log ("files not ready so sleeping");
				g_usleep (100000);
			}

			continue;
		}


		/* info struct may have been deleted in transit here so check if still valid and intact */
		if (!tracker_file_info_is_valid (info)) {
			continue;
		}


		/* get file ID and other interesting fields from Database if not previously fetched or is newly created */

		if (info->file_id == -1 && info->action != TRACKER_ACTION_CREATE &&
		    info->action != TRACKER_ACTION_DIRECTORY_CREATED && info->action != TRACKER_ACTION_FILE_CREATED) {

			info = tracker_db_get_file_info (db_con, info);
		}


		/* Get more file info if db retrieval returned nothing */
		if (info->file_id == -1 && info->action != TRACKER_ACTION_DELETE &&
		    info->action != TRACKER_ACTION_DIRECTORY_DELETED && info->action != TRACKER_ACTION_FILE_DELETED) {

			info = tracker_get_file_info (info);

			info->is_new = TRUE;

		} else {
			info->is_new = FALSE;
		}

		/* preprocess ambiguous actions when we need to work out if its a file or a directory that the action relates to */
		verify_action (info);

		//tracker_log ("processing %s with action %s and counter %d ", info->uri, tracker_actions[info->action], info->counter);


		/* process deletions */

		if (info->action == TRACKER_ACTION_FILE_DELETED || info->action == TRACKER_ACTION_FILE_MOVED_FROM) {

			delete_file (db_con, info);

			if (info->action == TRACKER_ACTION_FILE_MOVED_FROM) {
				moved_from_list = g_slist_prepend (moved_from_list, info);
			} else {
				info = tracker_dec_info_ref (info);
			}

			continue;

		} else {
			if (info->action == TRACKER_ACTION_DIRECTORY_DELETED || info->action ==  TRACKER_ACTION_DIRECTORY_MOVED_FROM) {

				delete_directory (db_con, info);

				if (info->action == TRACKER_ACTION_DIRECTORY_MOVED_FROM) {
					moved_from_list = g_slist_prepend (moved_from_list, info);
				} else {
					info = tracker_dec_info_ref (info);
				}

				continue;
			}
		}

		/* get latest file info from disk */
		if (info->mtime == 0) {
			info = tracker_get_file_info (info);
		}

		/* check if file needs indexing */
		need_index = (info->mtime > info->indextime);

		switch (info->action) {

			case TRACKER_ACTION_FILE_CHECK:

				break;

			case TRACKER_ACTION_FILE_CHANGED:
			case TRACKER_ACTION_FILE_CREATED:
			case TRACKER_ACTION_WRITABLE_FILE_CLOSED:

				need_index = TRUE;

				break;

			case TRACKER_ACTION_FILE_MOVED_TO :

				need_index = FALSE;

				/* to do - look up corresponding info in moved_from_list and update path and name in DB */

				break;

			case TRACKER_ACTION_DIRECTORY_CHECK:

				if (need_index) {
					if (g_slist_find_custom (tracker->no_watch_directory_list, info->uri, (GCompareFunc) has_prefix) == NULL) {
						scan_directory (info->uri, db_con);
					} else {
						tracker_log ("blocked scan of directory %s as its in the no watch list", info->uri);
					}
				}

				break;

			case TRACKER_ACTION_DIRECTORY_REFRESH:

				if (g_slist_find_custom (tracker->no_watch_directory_list, info->uri, (GCompareFunc) has_prefix) == NULL) {
					scan_directory (info->uri, db_con);
				} else {
					tracker_log ("blocked scan of directory %s as its in the no watch list", info->uri);
				}

				break;

			case TRACKER_ACTION_DIRECTORY_CREATED:
			case TRACKER_ACTION_DIRECTORY_MOVED_TO:

				need_index = TRUE;
				tracker_log ("processing created directory %s", info->uri);

				/* add to watch folders (including subfolders) */
				watch_dir (info->uri, db_con);

				/* schedule a rescan for all files in folder to avoid race conditions */
				if (info->action == TRACKER_ACTION_DIRECTORY_CREATED) {
					if (g_slist_find_custom (tracker->no_watch_directory_list, info->uri, (GCompareFunc) has_prefix) == NULL) {
						scan_directory (info->uri, db_con);
					} else {
						tracker_log ("blocked scan of directory %s as its in the no watch list", info->uri);
					}
				}

				break;

			default:
				break;
		}

		if (need_index) {
			index_file (db_con, info);
		}

		tracker_dec_info_ref (info);
	}

	tracker_db_close (db_con);
	tracker_db_thread_end ();
	tracker_log ("files thread has exited successfully");
	g_mutex_unlock (tracker->files_stopped_mutex);
}


static void
process_user_request_queue_thread (void)
{
	sigset_t     signal_set;
	DBConnection *db_con;

        /* block all signals in this thread */
        sigfillset (&signal_set);
        pthread_sigmask (SIG_BLOCK, &signal_set, NULL);

	g_mutex_lock (tracker->request_signal_mutex);
	g_mutex_lock (tracker->request_stopped_mutex);

	/* set thread safe DB connection */
	tracker_db_thread_init ();

	db_con = tracker_db_connect ();

	db_con->thread = "request";

	tracker_db_prepare_queries (db_con);

	while (TRUE) {
		DBusRec	    *rec;
		DBusMessage *reply;

		/* make thread sleep if first part of the shutdown process has been activated */
		if (!tracker->is_running) {

			tracker_log ("request thread going to deep sleep...");

			g_cond_wait (tracker->request_thread_signal, tracker->request_signal_mutex);

			/* determine if wake up call is new stuff or a shutdown signal */
			if (!shutdown) {
				continue;
			} else {
				break;
			}
		}

		/* lock check mutex to prevent race condition when a request is submitted after popping queue but prior to sleeping */
		g_mutex_lock (tracker->request_check_mutex);

		rec = g_async_queue_try_pop (tracker->user_request_queue);

		if (!rec) {
			//tracker_log ("request thread sleeping");
			g_cond_wait (tracker->request_thread_signal, tracker->request_signal_mutex);
			g_mutex_unlock (tracker->request_check_mutex);
			//tracker_log ("request thread awoken");

			/* determine if wake up call is new stuff or a shutdown signal */
			if (!shutdown) {
				continue;
			} else {
				break;
			}
		}

		/* thread will not sleep without another iteration so race condition no longer applies */
		g_mutex_unlock (tracker->request_check_mutex);

		rec->user_data = db_con;

		switch (rec->action) {

			case DBUS_ACTION_PING:

				reply = dbus_message_new_method_return (rec->message);

				gboolean result = TRUE;

				dbus_message_append_args (reply,
				  			  DBUS_TYPE_BOOLEAN, &result,
				  			  DBUS_TYPE_INVALID);

				dbus_connection_send (rec->connection, reply, NULL);
				dbus_message_unref (reply);

				break;

			case DBUS_ACTION_GET_STATS:

				tracker_dbus_method_get_stats (rec);

				break;

			case DBUS_ACTION_GET_SERVICES:

				tracker_dbus_method_get_services (rec);

				break;

			case DBUS_ACTION_GET_VERSION:

				tracker_dbus_method_get_version (rec);

				break;



			case DBUS_ACTION_METADATA_GET:

				tracker_dbus_method_metadata_get (rec);

				break;

			case DBUS_ACTION_METADATA_SET:

				tracker_dbus_method_metadata_set(rec);

				break;

			case DBUS_ACTION_METADATA_REGISTER_TYPE:

				tracker_dbus_method_metadata_register_type (rec);

				break;



			case DBUS_ACTION_METADATA_GET_TYPE_DETAILS:

				tracker_dbus_method_metadata_get_type_details (rec);

				break;

			case DBUS_ACTION_METADATA_GET_REGISTERED_TYPES:

				tracker_dbus_method_metadata_get_registered_types (rec);

				break;

			case DBUS_ACTION_METADATA_GET_WRITEABLE_TYPES:

				tracker_dbus_method_metadata_get_writeable_types (rec);

				break;

			case DBUS_ACTION_METADATA_GET_REGISTERED_CLASSES:

				tracker_dbus_method_metadata_get_registered_classes (rec);

				break;



			case DBUS_ACTION_KEYWORDS_GET_LIST:

				tracker_dbus_method_keywords_get_list (rec);

				break;

			case DBUS_ACTION_KEYWORDS_GET:

				tracker_dbus_method_keywords_get (rec);

				break;

			case DBUS_ACTION_KEYWORDS_ADD:

				tracker_dbus_method_keywords_add (rec);

				break;

			case DBUS_ACTION_KEYWORDS_REMOVE:

				tracker_dbus_method_keywords_remove (rec);

				break;

			case DBUS_ACTION_KEYWORDS_REMOVE_ALL:

				tracker_dbus_method_keywords_remove_all (rec);

				break;

			case DBUS_ACTION_KEYWORDS_SEARCH:

				tracker_dbus_method_keywords_search (rec);

				break;



			case DBUS_ACTION_SEARCH_TEXT:

				tracker_dbus_method_search_text (rec);

				break;

			case DBUS_ACTION_SEARCH_FILES_BY_TEXT:

				tracker_dbus_method_search_files_by_text (rec);

				break;

			case DBUS_ACTION_SEARCH_METADATA:

				tracker_dbus_method_search_metadata (rec);

				break;

			case DBUS_ACTION_SEARCH_MATCHING_FIELDS:

				tracker_dbus_method_search_matching_fields (rec);

				break;

			case DBUS_ACTION_SEARCH_QUERY:

				tracker_dbus_method_search_query (rec);

				break;



			case DBUS_ACTION_FILES_EXISTS:

				tracker_dbus_method_files_exists (rec);

				break;

			case DBUS_ACTION_FILES_CREATE:

				tracker_dbus_method_files_create (rec);

				break;



			case DBUS_ACTION_FILES_DELETE:

				tracker_dbus_method_files_delete (rec);

				break;

			case DBUS_ACTION_FILES_GET_SERVICE_TYPE:

				tracker_dbus_method_files_get_service_type (rec);

				break;

			case DBUS_ACTION_FILES_GET_TEXT_CONTENTS:

				tracker_dbus_method_files_get_text_contents (rec);

				break;

			case DBUS_ACTION_FILES_SEARCH_TEXT_CONTENTS:

				tracker_dbus_method_files_search_text_contents (rec);

				break;

			case DBUS_ACTION_FILES_GET_BY_SERVICE_TYPE:

				tracker_dbus_method_files_get_by_service_type (rec);

				break;

			case DBUS_ACTION_FILES_GET_BY_MIME_TYPE:

				tracker_dbus_method_files_get_by_mime_type (rec);

				break;


			case DBUS_ACTION_FILES_GET_BY_MIME_TYPE_VFS:

				tracker_dbus_method_files_get_by_mime_type_vfs (rec);

				break;

			case DBUS_ACTION_FILES_GET_MTIME:

				tracker_dbus_method_files_get_mtime (rec);

				break;

			case DBUS_ACTION_FILES_GET_METADATA_FOLDER_FILES:

				tracker_dbus_method_files_get_metadata_for_files_in_folder (rec);

				break;



			case DBUS_ACTION_FILES_SEARCH_BY_TEXT_MIME:

				tracker_dbus_method_files_search_by_text_mime (rec);

				break;

			case DBUS_ACTION_FILES_SEARCH_BY_TEXT_MIME_LOCATION:

				tracker_dbus_method_files_search_by_text_mime_location(rec);

				break;

			case DBUS_ACTION_FILES_SEARCH_BY_TEXT_LOCATION:

				tracker_dbus_method_files_search_by_text_location (rec);

				break;


			default:
				break;
		}

		dbus_message_unref (rec->message);

		g_free (rec);
	}

	tracker_db_close (db_con);
	tracker_db_thread_end ();

	tracker_log ("request thread has exited successfully");

	/* unlock mutex so we know thread has exited */
	g_mutex_unlock (tracker->request_check_mutex);
	g_mutex_unlock (tracker->request_stopped_mutex);
}


static void
unregistered_func (DBusConnection *connection,
		   gpointer        data)
{
}


static DBusHandlerResult
local_dbus_connection_monitoring_message_func (DBusConnection *connection,
					       DBusMessage    *message,
					       gpointer        data)
{
	/* DBus connection has been lost! */
	if (dbus_message_is_signal (message, DBUS_INTERFACE_LOCAL, "Disconnected")) {
		dbus_message_ref (message);

		tracker_log ("DBus connection has been lost, trackerd will shutdown");

		tracker->is_running = FALSE;
		tracker_end_watching ();
		do_cleanup ("DBus connection lost");
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



void
log_handler (const gchar *domain, GLogLevelFlags levels, const char* message, gpointer data)
{
	FILE		*fd;
	time_t		now;
	char		buffer1[64], buffer2[20];
	char		*output;
	struct tm	*loctime;
	GTimeVal	start;

	if (message) {
		g_print ("%s\n", message);
	}

	/* ensure file logging is thread safe */
	g_mutex_lock (tracker->log_access_mutex);

	fd = g_fopen (tracker->log_file, "a");

	if (!fd) {
		g_mutex_unlock (tracker->log_access_mutex);
		g_warning ("could not open %s", tracker->log_file);
		return;
	}

	g_get_current_time (&start);

	now = time ((time_t *) NULL);

	loctime = localtime (&now);

	strftime (buffer1, 64, "%d %b %Y, %H:%M:%S:", loctime);

	g_sprintf (buffer2, "%ld", start.tv_usec / 1000);

	output = g_strconcat (buffer1, buffer2, " - ", message, NULL);

	g_fprintf (fd, "%s\n", output);
	g_free (output);

	fclose (fd);

	g_mutex_unlock (tracker->log_access_mutex);
}


int
main (int argc, char **argv)
{
	int 		lfp;
  	struct 		sigaction act;
	sigset_t 	empty_mask;
	char 		*prefix, *lock_file, *str, *lock_str, *tracker_data_dir;

	gboolean 	need_setup;
	DBConnection 	*db_con;
	char		***res;

	setlocale (LC_ALL, "");

	/* set timezone info */
	tzset ();

	g_print ("\n\nTracker version %s Copyright (c) 2005-2006 by Jamie McCracken (jamiemcc@gnome.org)\n\n", TRACKER_VERSION);
	g_print ("This program is free software and comes without any warranty.\nIt is licensed under version 2 of the General Public License which can be viewed at http://www.gnu.org/licenses/gpl.txt\n\n");

	g_print ("Initialising tracker...\n");

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

	g_type_init ();

	if (!g_thread_supported ()) {
		g_thread_init (NULL);
	}

	g_log_set_handler (NULL, G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION, (GLogFunc) log_handler, NULL);

	dbus_g_thread_init ();

	need_setup = FALSE;

	shutdown = FALSE;

	tracker = g_new (Tracker, 1);

	tracker->watch_directory_roots_list = NULL;
	tracker->no_watch_directory_list = NULL;
	tracker->poll_list = NULL;
	tracker->use_nfs_safe_locking = FALSE;

 	tracker->is_running = FALSE;

	tracker->poll_access_mutex = g_mutex_new ();

	tracker->files_check_mutex = g_mutex_new ();
	tracker->metadata_check_mutex = g_mutex_new ();
	tracker->request_check_mutex = g_mutex_new ();

	tracker->files_stopped_mutex = g_mutex_new ();
	tracker->metadata_stopped_mutex = g_mutex_new ();
	tracker->request_stopped_mutex = g_mutex_new ();
	tracker->poll_stopped_mutex = g_mutex_new ();

	tracker->file_metadata_thread = NULL;
	tracker->file_process_thread = NULL;
	tracker->user_request_thread = NULL;;
	tracker->file_poll_thread = NULL;

	tracker->file_thread_signal = g_cond_new ();
	tracker->metadata_thread_signal = g_cond_new ();
	tracker->request_thread_signal = g_cond_new ();
	tracker->poll_thread_signal = g_cond_new ();

	tracker->metadata_signal_mutex = g_mutex_new ();
	tracker->files_signal_mutex = g_mutex_new ();
	tracker->request_signal_mutex = g_mutex_new ();
	tracker->poll_signal_mutex = g_mutex_new ();

	tracker->log_access_mutex = g_mutex_new ();
	tracker->scheduler_mutex = g_mutex_new ();


	/* check user data files */

	need_setup = tracker_db_needs_setup ();

	umask (077);

	prefix = g_build_filename (g_get_home_dir (), ".Tracker", NULL);
	str = g_strconcat (g_get_user_name (), "_tracker_lock", NULL);
	tracker->log_file = g_build_filename (prefix, "tracker.log", NULL);

	/* check if setup for NFS usage (and enable atomic NFS safe locking) */
	//lock_str = tracker_get_config_option ("NFSLocking");
	lock_str = NULL;
	if (lock_str != NULL) {
		const char *tmp_dir;

		tracker->use_nfs_safe_locking = (strcmp (str, "1") == 0);

		tmp_dir = g_get_tmp_dir ();

		/* place lock file in tmp dir to allow multiple sessions on NFS */
		lock_file = g_build_filename (tmp_dir, str, NULL);

		g_free (lock_str);

	} else {

		tracker->use_nfs_safe_locking = FALSE;

		/* place lock file in home dir to prevent multiple sessions on NFS (as standard locking might be broken on NFS) */
		lock_file = g_build_filename (prefix, str, NULL);
	}


	g_free (prefix);
	g_free (str);

	/* prevent muliple instances  */
	lfp = g_open (lock_file, O_RDWR|O_CREAT, 0640);
	g_free (lock_file);

	if (lfp < 0) {
		g_warning ("Cannot open or create lockfile - exiting");
		exit (1);
	}

	if (lockf (lfp, F_TLOCK, 0) <0) {
		g_warning ("Tracker daemon is already running - exiting");
		exit (0);
	}

	nice (10);


	/* reset log file */
	if (g_file_test (tracker->log_file, G_FILE_TEST_EXISTS)) {
		g_unlink (tracker->log_file);
	}





	tracker_load_config_file ();

	tracker_data_dir = g_build_filename (g_get_home_dir (), ".Tracker", "data", NULL);

	if (!tracker_db_initialize (tracker_data_dir)) {
		tracker_log ("Failed to initialise database engine - exiting...");
		return 1;
	}

	g_free (tracker_data_dir);

	/* create database if needed */
	if (need_setup) {
		tracker_create_db ();
	}

	/* set thread safe DB connection */
	tracker_db_thread_init ();

	db_con = tracker_db_connect ();
	db_con->thread = "main";


	if (tracker_update_db (db_con)) {

		/* refresh connection as DB might have been rebuilt */
		tracker_db_close (db_con);
		db_con = tracker_db_connect ();
		db_con->thread = "main";
		tracker_db_load_stored_procs (db_con);
	}


	/* clear pending files and watch tables*/
	tracker_db_clear_temp (db_con);

	if (!need_setup) {
		res = tracker_exec_proc (db_con, "GetStats", 0);

		if (res && res[0][0]) {
			char **row;
			int  k;

			tracker_log ("-----------------------");
			tracker_log ("Fetching index stats...");

			k = 0;

			while ((row = tracker_db_get_row (res, k))) {

				k++;

				if (row && row[0] && row[1]) {
					tracker_log ("%s : %s", row[0], row[1]);
				
				}
			}

			tracker_db_free_result (res);
		}
		tracker_log ("-----------------------\n");
	}


	tracker_db_check_tables (db_con);

	main_thread_db_con = db_con;

	tracker->file_scheduler = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	tracker->file_metadata_queue = g_async_queue_new ();
	tracker->file_process_queue = g_async_queue_new ();
	tracker->user_request_queue = g_async_queue_new ();

	tracker->parser = g_new (TextParser, 1);

	tracker->parser->min_word_length = 3;
	tracker->parser->max_word_length = 30;
	tracker->parser->use_stemmer = TRUE;
	tracker->parser->stem_language = STEM_ENG;
	tracker->parser->stop_words = NULL;



	/* periodically poll directories for changes */
	g_timeout_add_full (G_PRIORITY_LOW,
			    FILE_POLL_PERIOD,
		 	    (GSourceFunc) start_poll,
			    NULL, NULL
			    );



  	tracker->loop = g_main_loop_new (NULL, TRUE);

	main_connection = tracker_dbus_init ();

	add_local_dbus_connection_monitoring (main_connection);


	/* this var is used to tell the threads when to quit */
	tracker->is_running = TRUE;


	/* schedule the watching of directories so as not to delay start up time*/

	if (argc >= 2) {
		g_timeout_add_full (G_PRIORITY_LOW,
				    500,
			 	    (GSourceFunc) start_watching,
				    g_strdup (argv[1]), NULL
				    );
	} else {
		g_timeout_add_full (G_PRIORITY_LOW,
				    500,
			 	    (GSourceFunc) start_watching,
				    NULL, NULL
				    );
	}



	/* execute events and user requests to be processed and indexed in their own threads */
	tracker->file_process_thread =  g_thread_create ((GThreadFunc) process_files_thread, NULL, TRUE, NULL);
	tracker->file_metadata_thread = g_thread_create ((GThreadFunc) extract_metadata_thread, NULL, TRUE, NULL);
	tracker->user_request_thread =  g_thread_create ((GThreadFunc) process_user_request_queue_thread, NULL, TRUE, NULL);

	g_main_loop_run (tracker->loop);

	/* the following should never be reached in practice */
	tracker_log ("we should never get this message");

	tracker_db_close (db_con);

	tracker_db_thread_end ();

	tracker_dbus_shutdown (main_connection);

	do_cleanup (" ");

	return EXIT_SUCCESS;
}
