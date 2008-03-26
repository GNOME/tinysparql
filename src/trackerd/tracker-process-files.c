/* Tracker - indexer and metadata database engine
 * Copyright (C) 2008, Mr Jamie McCracken (jamiemcc@gnome.org)
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

#include <string.h>
#include <signal.h>

#ifdef OS_WIN32
#include <windows.h>
#include <pthread.h>
#include "mingw-compat.h"
#endif

#include <glib.h>

#include <libtracker-common/tracker-config.h>
#include <libtracker-common/tracker-log.h>
#include "../xdgmime/xdgmime.h"

#include "tracker-apps.h"
#include "tracker-db.h"
#include "tracker-dbus.h"
#include "tracker-dbus-methods.h"
#include "tracker-cache.h"
#include "tracker-email.h"
#include "tracker-indexer.h"
#include "tracker-os-dependant.h"
#include "tracker-utils.h"
#include "tracker-watch.h"

static void
process_my_yield (void)
{
#ifndef OS_WIN32
	while (g_main_context_iteration (NULL, FALSE)) {
                /* FIXME: do something here? */
	}
#endif
}

static void
process_watch_list_add_dirs (Tracker  *tracker,
                             GSList   *dir_list, 
                             gboolean  check_dirs)
{
        if (!tracker->is_running) {
		return;
	}
        
	/* Add sub directories breadth first recursively to avoid
         * running out of file handles.
         */
	while (dir_list) {
                GSList *file_list = NULL;
                GSList *tmp;

		for (tmp = dir_list; tmp; tmp = tmp->next) {
                        gchar *str;

                        str = tmp->data;

			if (str && !tracker_file_is_no_watched (str)) {
				tracker->dir_list = g_slist_prepend (tracker->dir_list, g_strdup (str));

				if (!tracker_config_get_enable_watches (tracker->config) || 
                                    tracker_file_is_crawled (str)) {
                                        continue;
                                }

				if ((tracker_count_watch_dirs () + g_slist_length (dir_list)) < tracker->watch_limit) {
					if (!tracker_add_watch_dir (str, tracker->index_db) && tracker_is_directory (str)) {
						tracker_debug ("Watch failed for %s", str);
					}
				}
			}
		}

		g_slist_foreach (dir_list, (GFunc) tracker_get_dirs, &file_list);
		g_slist_foreach (dir_list, (GFunc) g_free, NULL);
		g_slist_free (dir_list);

		dir_list = file_list;
	}
}

static gboolean
process_watch_dir (Tracker     *tracker,
                   const gchar *dir)
{
	gchar *dir_utf8;

	if (!tracker->is_running) {
		return TRUE;
	}

	if (!dir) {
		return FALSE;
	}

	if (!g_utf8_validate (dir, -1, NULL)) {
		dir_utf8 = g_filename_to_utf8 (dir, -1, NULL,NULL,NULL);
		if (!dir_utf8) {
			tracker_error ("ERROR: watch_dir could not be converted to utf8 format");
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

	if (tracker_file_is_crawled (dir_utf8)) {
		g_free (dir_utf8);
		return FALSE;
	}

	if (!tracker_file_is_no_watched (dir_utf8)) {
		GSList *mylist = NULL;

		mylist = g_slist_prepend (mylist, dir_utf8);
		process_watch_list_add_dirs (tracker, mylist, TRUE);
	}

	return TRUE;
}

static void
process_watch_dir_foreach (const gchar  *dir, 
                           Tracker      *tracker)
{
        process_watch_dir (tracker, dir);
}

static void
process_schedule_dir_check_foreach (const gchar  *uri, 
                                    Tracker      *tracker)
{
	if (!tracker->is_running) {
		return;
	}

	g_return_if_fail (tracker_check_uri (uri));
	g_return_if_fail (tracker->index_db);

	tracker_db_insert_pending_file (tracker->index_db, 0, uri, NULL, "unknown", 0, 
                                        TRACKER_ACTION_DIRECTORY_REFRESH, TRUE, FALSE, -1);
}

static inline void
process_directory_list (Tracker  *tracker, 
                        GSList   *list, 
                        gboolean  recurse)
{
	tracker->dir_list = NULL;

	if (!list) {
		return;
	}

	g_slist_foreach (list, 
                         (GFunc) process_watch_dir_foreach, 
                         tracker);
	g_slist_foreach (list, 
                         (GFunc) process_schedule_dir_check_foreach, 
                         tracker);

	if (recurse && tracker->dir_list) {
		g_slist_foreach (tracker->dir_list, 
                                 (GFunc) process_schedule_dir_check_foreach, 
                                 tracker);
	}

	if (tracker->dir_list) {
		g_slist_foreach (tracker->dir_list, (GFunc) g_free, NULL);
		g_slist_free (tracker->dir_list);
                tracker->dir_list = NULL;
	}
}

static void
process_schedule_file_check_foreach (const gchar  *uri, 
                                     Tracker      *tracker)
{
	if (!tracker->is_running) {
		return;
	}

	g_return_if_fail (tracker_check_uri (uri));
	g_return_if_fail (tracker->index_db);

	/* Keep mainloop responsive */
	process_my_yield ();

	if (!tracker_is_directory (uri)) {
		tracker_db_insert_pending_file (tracker->index_db, 0, uri, NULL, "unknown", 0, TRACKER_ACTION_CHECK, 0, FALSE, -1);
	} else {
		process_schedule_dir_check_foreach (uri, tracker);
	}
}

static void
process_scan_directory (Tracker     *tracker,
                        const gchar *uri)
{
	GSList *file_list;

	if (!tracker->is_running) {
		return;
	}

	g_return_if_fail (tracker->index_db);
	g_return_if_fail (tracker_check_uri (uri));
	g_return_if_fail (tracker_is_directory (uri));

	/* Keep mainloop responsive */
	process_my_yield ();

	file_list = tracker_get_files (uri, FALSE);
	tracker_debug ("Scanning %s for %d files", uri, g_slist_length(file_list));

	g_slist_foreach (file_list, 
                         (GFunc) process_schedule_file_check_foreach, 
                         tracker);
	g_slist_foreach (file_list, 
                         (GFunc) g_free, 
                         NULL);
	g_slist_free (file_list);

	/* Recheck directory to update its mtime if its changed whilst
         * scanning.
         */
	process_schedule_dir_check_foreach (uri, tracker);
	tracker_debug ("Finished scanning");
}

static void
process_action_verify (FileInfo *info)
{
        /* Determines whether an action applies to a file or a
         * directory.
         */

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
			if (info->file_id == 0) {
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
process_index_entity (Tracker  *tracker, 
                      FileInfo *info)
{
        ServiceDef *def;
	gchar      *service_info;
	gchar      *str;

	g_return_if_fail (info);
	g_return_if_fail (tracker_check_uri (info->uri));

	if (!tracker_file_is_valid (info->uri)) {
		return;
	}

	if (!info->is_directory) {
		/* Sleep to throttle back indexing */
		tracker_throttle (100);
	}

	service_info = tracker_get_service_for_uri (info->uri);

	if (!service_info) {
		tracker_error ("ERROR: cannot find service for path %s", info->uri);
		return;
	}

        str = g_utf8_strdown (service_info, -1);
	def = g_hash_table_lookup (tracker->service_table, str);
	g_free (str);

	if (!def) {
		if (service_info) {
			tracker_error ("ERROR: unknown service %s", service_info);
		} else {
			tracker_error ("ERROR: unknown service");
		}
		g_free (service_info);
		return;
	}

	if (info->is_directory) {
		info->is_hidden = !def->show_service_directories;
		tracker_db_index_file (tracker->index_db, info, NULL, NULL);
		g_free (service_info);
		return;
	} else {
		info->is_hidden = !def->show_service_files;
	}

	if (g_str_has_suffix (service_info, "Emails")) {
                DBConnection *db_con;

                db_con = tracker->index_db;

		if (!tracker_email_index_file (db_con, info, service_info)) {
			g_free (service_info);
			return;
		}
	} else if (strcmp (service_info, "Files") == 0) {
		tracker_db_index_file (tracker->index_db, info, NULL, NULL);
        } else if (strcmp (service_info, "WebHistory") ==0 ) {
                tracker_db_index_webhistory (tracker->index_db, info);
	} else if (g_str_has_suffix (service_info, "Conversations")) {
		tracker_db_index_conversation (tracker->index_db, info);
	} else if (strcmp (service_info, "Applications") == 0) {
		tracker_db_index_application (tracker->index_db, info);
	} else {
		tracker_db_index_service (tracker->index_db, info, NULL, NULL, NULL, FALSE, TRUE, TRUE, TRUE);
	}

	g_free (service_info);
}

static void
process_delete_file (Tracker  *tracker, 
                     FileInfo *info)
{
	/* Info struct may have been deleted in transit here so check
         * if still valid and intact.
         */
	g_return_if_fail (tracker_file_info_is_valid (info));

	/* If we dont have an entry in the db for the deleted file, we
         * ignore it.
         */
	if (info->file_id == 0) {
		return;
	}

	tracker_db_delete_file (tracker->index_db, info->file_id);

	tracker_log ("Deleting file %s", info->uri);
}

static void
process_delete_dir (Tracker  *tracker, 
                    FileInfo *info)
{
	/* Info struct may have been deleted in transit here so check
         * if still valid and intact.
         */
	g_return_if_fail (tracker_file_info_is_valid (info));

	/* If we dont have an entry in the db for the deleted
         * directory, we ignore it.
         */
	if (info->file_id == 0) {
		return;
	}

	tracker_db_delete_directory (tracker->index_db, info->file_id, info->uri);

	tracker_remove_watch_dir (info->uri, TRUE, tracker->index_db);

	tracker_log ("Deleting dir %s and subdirs", info->uri);
}

static void
process_delete_dir_check (Tracker     *tracker,
                          const gchar *uri)
{
	gchar **files;
        gchar **p;

	/* Check for any deletions*/
	files = tracker_db_get_files_in_folder (tracker->index_db, uri);

	for (p = files; *p; p++) {
		gchar *str = *p;

		if (!tracker_file_is_valid (str)) {
                        FileInfo *info;

			info = tracker_create_file_info (str, 1, 0, 0);
			info = tracker_db_get_file_info (tracker->index_db, info);

			if (!info->is_directory) {
				process_delete_file (tracker, info);
			} else {
				process_delete_dir (tracker, info);
			}
			tracker_free_file_info (info);
		}
	}

	g_strfreev (files);
}

static void
process_add_dirs_to_list (Tracker *tracker, 
                          GSList  *dir_list)
{
	GSList *new_dir_list = NULL;
        GSList *l;

	if (!tracker->is_running) {
		return;
	}

	for (l = dir_list; l; l = l->next) {
		if (l) {
			new_dir_list = g_slist_prepend (new_dir_list, 
                                                        g_strdup (l->data));
		}
	}

	/* Add sub directories breadth first recursively to avoid
         * running out of file handles.
         */
	while (new_dir_list) {
                GSList *file_list = NULL;

		for (l = new_dir_list; l; l = l->next) {
                        gchar *str = l->data;

			if (str && !tracker_file_is_no_watched (str)) {
				tracker->dir_list = g_slist_prepend (tracker->dir_list, g_strdup (str));
			}
		}

		g_slist_foreach (new_dir_list, (GFunc) tracker_get_dirs, &file_list);
		g_slist_foreach (new_dir_list, (GFunc) g_free, NULL);
		g_slist_free (new_dir_list);
		new_dir_list = file_list;
	}
}

static inline void
process_queue_files_foreach (const gchar *uri, 
                             gpointer     user_data)
{
        Tracker  *tracker;
	FileInfo *info;
        
        tracker = (Tracker*) user_data;
        info = tracker_create_file_info (uri, TRACKER_ACTION_CHECK, 0, 0);
	g_async_queue_push (tracker->file_process_queue, info);
}

static void
process_check_directory (Tracker     *tracker,
                         const gchar *uri)
{
	GSList *file_list = NULL;

	if (!tracker->is_running) {
		return;
	}

	g_return_if_fail (tracker_check_uri (uri));
	g_return_if_fail (tracker_is_directory (uri));

	file_list = tracker_get_files (uri, FALSE);
	tracker_debug ("Checking %s for %d files", uri, g_slist_length (file_list));

	g_slist_foreach (file_list, (GFunc) process_queue_files_foreach, tracker);
	g_slist_foreach (file_list, (GFunc) g_free, NULL);
	g_slist_free (file_list);

        process_queue_files_foreach (uri, tracker);

	if (tracker->index_status != INDEX_EMAILS) {
                tracker->folders_processed++;
        }
}

/* 
 * Actual Indexing functions 
 */
static void
process_index_config (Tracker *tracker) 
{
        tracker_log ("Starting config indexing");
}

static void
process_index_applications (Tracker *tracker) 
{
        GSList       *list;
        DBConnection *db_con;

        tracker_log ("Starting application indexing");
       
        db_con = tracker->index_db;

        tracker_db_start_index_transaction (db_con);
        tracker_db_start_transaction (db_con->cache);
        
        tracker_applications_add_service_directories ();
        
        list = tracker_get_service_dirs ("Applications");

        tracker_add_root_directories (list);
        process_directory_list (tracker, list, FALSE);

        tracker_db_end_transaction (db_con->cache);
        
        g_slist_free (list);
}

static void
process_index_files (Tracker *tracker)
{
        DBConnection *db_con;
        GSList       *watch_directory_roots;
        GSList       *no_watch_directory_roots;
        gint          initial_sleep;

        tracker_log ("Starting file indexing...");
        
        db_con = tracker->index_db;

        initial_sleep = tracker_config_get_initial_sleep (tracker->config);
       
        tracker->pause_io = TRUE;

        tracker_dbus_send_index_status_change_signal ();
        tracker_db_end_index_transaction (db_con);

        /* Sleep for N secs before watching/indexing any of the major services */
        tracker_log ("Sleeping for %d secs before starting...", initial_sleep);
        
        while (initial_sleep > 0) {
                g_usleep (G_USEC_PER_SEC);
                
                initial_sleep --;
                
                if (!tracker->is_running || tracker->shutdown) {
                        return;
                }		
        }
        
        tracker->pause_io = FALSE;
        tracker_dbus_send_index_status_change_signal ();
        
        tracker->dir_list = NULL;
        
        tracker_db_start_index_transaction (db_con);
	
        /* Delete all stuff in the no watch dirs */
        watch_directory_roots = 
                tracker_config_get_watch_directory_roots (tracker->config);
        
        no_watch_directory_roots = 
                tracker_config_get_no_watch_directory_roots (tracker->config);
        
        if (no_watch_directory_roots) {
                GSList *l;
                
                tracker_log ("Deleting entities in no watch directories...");
                
                for (l = no_watch_directory_roots; l; l = l->next) {
                        guint32 f_id = tracker_db_get_file_id (db_con, l->data);
                        
                        if (f_id > 0) {
                                tracker_db_delete_directory (db_con, f_id, l->data);
                        }
                }
        }
        
        if (!watch_directory_roots) {
                return;
        }
        
        tracker_db_start_transaction (db_con->cache);
        tracker_add_root_directories (watch_directory_roots);
        
        /* index watched dirs first */
        g_slist_foreach (watch_directory_roots, 
                         (GFunc) process_watch_dir_foreach, 
                         tracker);
        
        g_slist_foreach (tracker->dir_list, 
                         (GFunc) process_schedule_dir_check_foreach, 
                         tracker);
        
        if (tracker->dir_list) {
                g_slist_foreach (tracker->dir_list, 
                                 (GFunc) g_free, 
                                 NULL);
                g_slist_free (tracker->dir_list);
                tracker->dir_list = NULL;
        }
        
        g_slist_foreach (watch_directory_roots, 
                         (GFunc) process_schedule_dir_check_foreach, 
                         tracker);
        
        if (tracker->dir_list) {
                g_slist_foreach (tracker->dir_list, 
                                 (GFunc) g_free, 
                                 NULL);
                g_slist_free (tracker->dir_list);
                tracker->dir_list = NULL;
        }
        
        tracker_db_end_transaction (db_con->cache);
        tracker_dbus_send_index_progress_signal ("Files", "");
}

static void
process_index_crawl_files (Tracker *tracker)
{
        DBConnection *db_con;
        GSList       *crawl_directory_roots;
        
        db_con = tracker->index_db;

        tracker_log ("Starting directory crawling...");
        tracker->dir_list = NULL;
        
        crawl_directory_roots = 
                tracker_config_get_crawl_directory_roots (tracker->config);
        
        if (!crawl_directory_roots) {
                return;
        }
        
        tracker_db_start_transaction (db_con->cache);
        tracker_add_root_directories (crawl_directory_roots);
        
        process_add_dirs_to_list (tracker, crawl_directory_roots);
        
        g_slist_foreach (tracker->dir_list, 
                         (GFunc) process_schedule_dir_check_foreach, 
                         tracker);
        
        if (tracker->dir_list) {
                g_slist_foreach (tracker->dir_list, (GFunc) g_free, NULL);
                g_slist_free (tracker->dir_list);
                tracker->dir_list = NULL;
        }
        
        g_slist_foreach (crawl_directory_roots, 
                         (GFunc) process_schedule_dir_check_foreach, 
                         tracker);
        
        if (tracker->dir_list) {
                g_slist_foreach (tracker->dir_list, (GFunc) g_free, NULL);
                g_slist_free (tracker->dir_list);
                tracker->dir_list = NULL;
        }
        
        tracker_db_end_transaction (db_con->cache);
}

static void
process_index_conversations (Tracker *tracker)
{
        gchar    *gaim, *purple;
        gboolean  has_logs = FALSE;
        GSList   *list = NULL;
        
        gaim = g_build_filename (g_get_home_dir(), ".gaim", "logs", NULL);
        purple = g_build_filename (g_get_home_dir(), ".purple", "logs", NULL);
        
        if (tracker_file_is_valid (gaim)) {
                has_logs = TRUE;
                tracker_add_service_path ("GaimConversations", gaim);
                list = g_slist_prepend (NULL, gaim);
        }

        if (tracker_file_is_valid (purple)) {
                has_logs = TRUE;
                tracker_add_service_path ("GaimConversations", purple);
                list = g_slist_prepend (NULL, purple);
        }
        
        if (has_logs) {
                DBConnection *db_con;
                
                db_con = tracker->index_db;

                tracker_log ("Starting chat log indexing...");
                tracker_db_start_transaction (db_con->cache);
                tracker_add_root_directories (list);
                process_directory_list (tracker, list, TRUE);
                tracker_db_end_transaction (db_con->cache);
                g_slist_free (list);
        }
        
        g_free (gaim);
        g_free (purple);
}

static void
process_index_webhistory (Tracker *tracker)
{
        GSList *list = NULL;
        gchar  *firefox_dir;

        firefox_dir = g_build_filename (g_get_home_dir(), ".xesam/Firefox/ToIndex", NULL);

        if (tracker_file_is_valid (firefox_dir)) {
                DBConnection *db_con;
                
                db_con = tracker->index_db;

                list = g_slist_prepend( NULL, firefox_dir);
                
                tracker_log ("Starting Firefox web history indexing...");
                tracker_add_service_path ("WebHistory", firefox_dir);
                
                tracker_db_start_transaction (db_con->cache);		
                tracker_add_root_directories (list);
                process_directory_list (tracker, list, TRUE);
                tracker_db_end_transaction (db_con->cache);
                g_slist_free (list);
        }

        g_free (firefox_dir);
} 

static void
process_index_emails (Tracker *tracker)
{
        DBConnection  *db_con;
        TrackerConfig *config;
        gboolean       index_evolution_emails;
        gboolean       index_kmail_emails;
        gboolean       index_thunderbird_emails;
       
        db_con = tracker->index_db;
        config = tracker->config;

        tracker_db_end_index_transaction (db_con);
        tracker_cache_flush_all ();
	
        tracker_indexer_merge_indexes (INDEX_TYPE_FILES);
	
        if (tracker->shutdown) {
                return;
        }
	
        tracker->index_status = INDEX_EMAILS;
        
        tracker_dbus_send_index_progress_signal ("Emails", "");
        
        if (tracker->word_update_count > 0) {
                tracker_indexer_apply_changes (tracker->file_index, tracker->file_update_index, TRUE);
        }
        
        tracker_db_start_index_transaction (tracker->index_db);
              
        index_evolution_emails = tracker_config_get_index_evolution_emails (config);
        index_kmail_emails = tracker_config_get_index_kmail_emails (config);
        index_thunderbird_emails = tracker_config_get_index_thunderbird_emails (config);
        
        if (index_evolution_emails ||
            index_kmail_emails ||
            index_thunderbird_emails) {
                tracker_email_add_service_directories (db_con->emails);
                tracker_log ("Starting email indexing...");
                
                tracker_db_start_transaction (db_con->cache);
                
                if (index_evolution_emails) {
                        GSList *list;

                        list = tracker_get_service_dirs ("EvolutionEmails");
                        tracker_add_root_directories (list);
                        process_directory_list (tracker, list, TRUE);
                        g_slist_free (list);
                        
                        /* If initial indexing has not finished reset
                         * mtime on all email stuff so they are
                         * rechecked 
                         */
                        if (tracker_db_get_option_int (db_con->common, "InitialIndex") == 1) {
                                gchar *sql;

                                sql = g_strdup_printf ("update Services set mtime = 0 where path like '%s/.evolution/%%'", 
                                                       g_get_home_dir ());
                                tracker_exec_sql (tracker->index_db, sql);
                                g_free (sql);
                        }
                }
                
                if (index_kmail_emails) {
                        GSList *list;

                        list = tracker_get_service_dirs ("KMailEmails");
                        tracker_add_root_directories (list);
                        process_directory_list (tracker, list, TRUE);
                        g_slist_free (list);
                }
                
                if (index_thunderbird_emails) {
                        GSList *list;

                        list = tracker_get_service_dirs ("ThunderbirdEmails");
                        tracker_add_root_directories (list);
                        process_directory_list (tracker, list, TRUE);
                        g_slist_free (list);
                }
                
                tracker_db_end_transaction (db_con->cache);
        }
}

static gboolean
process_files (Tracker *tracker)
{
        DBConnection *db_con;

        db_con = tracker->index_db;

        /* Check dir_queue in case there are
         * directories waiting to be indexed.
         */
        if (g_async_queue_length (tracker->dir_queue) > 0) {
                gchar *uri;
                
                uri = g_async_queue_try_pop (tracker->dir_queue);
                
                if (uri) {
                        process_check_directory (tracker, uri);
                        g_free (uri);
                        return TRUE;
                }
        }
        
        if (tracker->index_status != INDEX_FINISHED) {
                g_mutex_unlock (tracker->files_check_mutex);
                
                switch (tracker->index_status) {
                case INDEX_CONFIG:
                        process_index_config (tracker);
                        break;
                        
                case INDEX_APPLICATIONS: 
                        process_index_applications (tracker);
                        break;
                        
                case INDEX_FILES: 
                        process_index_files (tracker);
                        break;
                        
                case INDEX_CRAWL_FILES:
                        process_index_crawl_files (tracker);
                        break;
                        
                case INDEX_CONVERSATIONS:
                        process_index_conversations (tracker);
                        break;
                        
                case INDEX_WEBHISTORY: 
                        process_index_webhistory (tracker);
                        break;
                        
                case INDEX_EXTERNAL:
                        break;
                        
                case INDEX_EMAILS:
                        process_index_emails (tracker);
                        break;
			
                case INDEX_FINISHED:
                        break;
                }
                
                tracker->index_status++;
                return TRUE;
        }
        
        tracker_db_end_index_transaction (db_con);
        tracker_cache_flush_all ();
        tracker_db_refresh_all (db_con);
        tracker_indexer_merge_indexes (INDEX_TYPE_FILES);
	
        if (tracker->shutdown) {
                return FALSE;
        }
        
        if (tracker->word_update_count > 0) {
                tracker_indexer_apply_changes (tracker->file_index, 
                                               tracker->file_update_index, 
                                               TRUE);
        }
        
        tracker_indexer_merge_indexes (INDEX_TYPE_EMAILS);
        
        if (tracker->shutdown) {
                return FALSE;
        }
        
        tracker->index_status = INDEX_FILES;
        tracker_dbus_send_index_progress_signal ("Files","");
        tracker->index_status = INDEX_FINISHED;
        
        if (tracker->is_running && tracker->first_time_index) {
                tracker->status = STATUS_OPTIMIZING;
                tracker->do_optimize = FALSE;
                
                tracker->first_time_index = FALSE;
		
                tracker_dbus_send_index_finished_signal ();
                tracker_db_set_option_int (db_con, "InitialIndex", 0);
                
                tracker->update_count = 0;
                
                tracker_log ("Updating database stats, please wait...");
                
                tracker_db_start_transaction (db_con);
                tracker_db_exec_no_reply (db_con, "ANALYZE");
                tracker_db_end_transaction (db_con);
                
                tracker_db_start_transaction (db_con->emails);
                tracker_db_exec_no_reply (db_con->emails, "ANALYZE");
                tracker_db_end_transaction (db_con->emails);
                
                tracker_log ("Finished optimizing, waiting for new events...");
        }
        
        /* We have no stuff to process so
         * sleep until awoken by a new
         * signal.
         */
        tracker->status = STATUS_IDLE;
        tracker_dbus_send_index_status_change_signal ();
        
        g_cond_wait (tracker->file_thread_signal, 
                     tracker->files_signal_mutex);
        
        tracker->grace_period = 0;
        
        /* Determine if wake up call is new
         * stuff or a shutdown signal.
         */
        if (!tracker->shutdown) {
                tracker_db_start_index_transaction (db_con);
                return TRUE;
        }

        return FALSE;
}

static gboolean 
process_action (Tracker  *tracker,
                FileInfo *info)
{
        DBConnection *db_con;
        gboolean      need_index;

        db_con = tracker->index_db;
        need_index = info->mtime > info->indextime;
        
        switch (info->action) {
        case TRACKER_ACTION_FILE_CHECK:
                break;
                
        case TRACKER_ACTION_FILE_CHANGED:
        case TRACKER_ACTION_FILE_CREATED:
        case TRACKER_ACTION_WRITABLE_FILE_CLOSED:
                need_index = TRUE;
                break;
                
        case TRACKER_ACTION_FILE_MOVED_FROM:
                need_index = FALSE;
                tracker_log ("Starting moving file %s to %s", info->uri, info->moved_to_uri);
                tracker_db_move_file (db_con, info->uri, info->moved_to_uri);
                break;
                
        case TRACKER_ACTION_DIRECTORY_REFRESH:
                if (need_index && !tracker_file_is_no_watched (info->uri)) {
                        g_async_queue_push (tracker->dir_queue, g_strdup (info->uri));
                        
                        if (tracker->index_status != INDEX_EMAILS) {
                                tracker->folders_count++;
                        }
                }
                
                need_index = FALSE;
                break;
                
        case TRACKER_ACTION_DIRECTORY_CHECK:
                if (need_index && !tracker_file_is_no_watched (info->uri)) {
                        g_async_queue_push (tracker->dir_queue, g_strdup (info->uri));
			
                        if (info->indextime > 0) {
                                process_delete_dir_check (tracker, info->uri);
                        }
                }
                
                break;
                
        case TRACKER_ACTION_DIRECTORY_MOVED_FROM:
                tracker_db_move_directory (db_con, info->uri, info->moved_to_uri);
                need_index = FALSE;
                break;
                
        case TRACKER_ACTION_DIRECTORY_CREATED:
                need_index = TRUE;
                tracker_debug ("Processing created directory %s", info->uri);
                
                /* Schedule a rescan for all files in folder
                 * to avoid race conditions.
                 */
                if (!tracker_file_is_no_watched (info->uri)) {
                        /* Add to watch folders (including
                         * subfolders).
                         */
                        process_watch_dir (tracker, info->uri);
                        process_scan_directory (tracker, info->uri);
                } else {
                        tracker_debug ("Blocked scan of directory %s as its in the no watch list", 
                                       info->uri);
                }
                
                break;
                
        default:
                break;
        }

        return need_index;
}

static gboolean
process_action_prechecks (Tracker  *tracker, 
                          FileInfo *info)
{
        DBConnection *db_con;

        /* Info struct may have been deleted in transit here
         * so check if still valid and intact.
         */
        if (!tracker_file_info_is_valid (info)) {
                return TRUE;
        }

        db_con = tracker->index_db;
        
        if (info->file_id == 0 && 
            info->action != TRACKER_ACTION_CREATE &&
            info->action != TRACKER_ACTION_DIRECTORY_CREATED && 
            info->action != TRACKER_ACTION_FILE_CREATED) {
                info = tracker_db_get_file_info (db_con, info);
                
                /* Get more file info if db retrieval returned nothing */
                if (info->file_id == 0) {
                        info = tracker_get_file_info (info);
                        info->is_new = TRUE;
                } else {
                        info->is_new = FALSE;
                }
        } else {
                info->is_new = TRUE;
        }
        
        tracker_debug ("Processing %s with action %s and counter %d ",
                       info->uri, 
                       tracker_actions[info->action], 
                       info->counter);
        
        /* Preprocess ambiguous actions when we need to work
         * out if its a file or a directory that the action
         * relates to.
         */
        process_action_verify (info);
        
        if (info->action != TRACKER_ACTION_DELETE &&
            info->action != TRACKER_ACTION_DIRECTORY_DELETED &&
            info->action != TRACKER_ACTION_FILE_DELETED) {
                if (!tracker_file_is_valid (info->uri) ) {
                        gboolean invalid = TRUE;
                        
                        if (info->moved_to_uri) {
                                invalid = !tracker_file_is_valid (info->moved_to_uri);
                        }
                        
                        if (invalid) {
                                tracker_free_file_info (info);
                                return TRUE;
                        }
                }
                
                /* Get file ID and other interesting fields
                 * from Database if not previously fetched or
                 * is newly created.
                 */
        } else {
                if (info->action == TRACKER_ACTION_FILE_DELETED) {
                        process_delete_file (tracker, info);
                        info = tracker_dec_info_ref (info);
                        return TRUE;
                } else {
                        if (info->action == TRACKER_ACTION_DIRECTORY_DELETED) {
                                process_delete_file (tracker, info);
                                process_delete_dir (tracker, info);
                                info = tracker_dec_info_ref (info);
                                return TRUE;
                        }
                }
        }	
        
        /* Get latest file info from disk */
        if (info->mtime == 0) {
                info = tracker_get_file_info (info);
        }

        return FALSE;
}

static void
process_block_signals (void)
{
	sigset_t signal_set;

        /* Block all signals in this thread */
        sigfillset (&signal_set);
        
#ifndef OS_WIN32
        pthread_sigmask (SIG_BLOCK, &signal_set, NULL);
#endif
}

/* This is the thread entry point for the indexer to start processing
 * files and all other categories for processing.
 */
gpointer
tracker_process_files (gpointer data)
{
	Tracker      *tracker;
        DBConnection *db_con;
	GSList	     *moved_from_list; /* List to hold moved_from
                                        * events whilst waiting for a
                                        * matching moved_to event.
                                        */
	gboolean      pushed_events;
        gboolean      first_run;

        tracker = (Tracker*) data;

        process_block_signals ();

	g_mutex_lock (tracker->files_signal_mutex);
	g_mutex_lock (tracker->files_stopped_mutex);

	/* Set thread safe DB connection */
	tracker_db_thread_init ();

	tracker->index_db = tracker_db_connect_all (TRUE);
	tracker->index_status = INDEX_CONFIG;

	pushed_events = FALSE;
	first_run = TRUE;
	moved_from_list = NULL;

	tracker_log ("Starting indexing...");

	tracker->index_time_start = time (NULL);

	while (TRUE) {
		FileInfo *info;
		gboolean  need_index;

		db_con = tracker->index_db;

		if (!tracker_cache_process_events (tracker->index_db, TRUE) ) {
			tracker->status = STATUS_SHUTDOWN;
			tracker_dbus_send_index_status_change_signal ();
			break;	
		}

		if (tracker->status != STATUS_INDEXING) {
			tracker->status = STATUS_INDEXING;
			tracker_dbus_send_index_status_change_signal ();
		}
						
		info = g_async_queue_try_pop (tracker->file_process_queue);

		/* Check pending table if we haven't got anything */
		if (!info) {
			gchar ***res;
			gint     k;

			if (!tracker_db_has_pending_files (tracker->index_db)) {
                                gboolean should_continue;

                                /* Set mutex to indicate we are in "check" state */
                                g_mutex_lock (tracker->files_check_mutex);
                                should_continue = process_files (tracker);
                                g_mutex_unlock (tracker->files_check_mutex);
                                
                                if (should_continue) {
                                        continue;
                                }

                                if (tracker->shutdown) {
                                        break;
                                }
                        }

			res = tracker_db_get_pending_files (tracker->index_db);

			k = 0;
			pushed_events = FALSE;

			if (res) {
				gchar **row;

				tracker->status = STATUS_PENDING;

				while ((row = tracker_db_get_row (res, k))) {
					FileInfo	    *info_tmp;
					TrackerChangeAction  tmp_action;

					if (!tracker->is_running) {
						tracker_db_free_result (res);
						break;
					}

					k++;

					tmp_action = atoi (row[2]);

					info_tmp = tracker_create_file_info (row[1], tmp_action, 0, WATCH_OTHER);
					g_async_queue_push (tracker->file_process_queue, info_tmp);
					pushed_events = TRUE;
				}

				tracker_db_free_result (res);
			}

			if (!tracker->is_running) {
				continue;
			}

			tracker_db_remove_pending_files (tracker->index_db);

			/* Pending files are present but not yet ready
                         * as we are waiting til they stabilize so we
                         * should sleep for 100ms (only occurs when
                         * using FAM or inotify move/create). 
                         */
			if (!pushed_events && k == 0) {
				g_usleep (100000);
			}

			continue;
		}

		tracker->status = STATUS_INDEXING;

                if (process_action_prechecks (tracker, info)) {
                        continue;
                }
                
		/* Check if file needs indexing */
                need_index = process_action (tracker, info);

		if (need_index) {
			if (tracker_db_regulate_transactions (tracker->index_db, 250)) {
				if (tracker_config_get_verbosity (tracker->config) == 1) {
					tracker_log ("indexing #%d - %s", tracker->index_count, info->uri);
				}

				tracker_dbus_send_index_progress_signal ("Files", info->uri);
			}

			process_index_entity (tracker, info);
			tracker->index_db = tracker->index_db;	
		}

		tracker_dec_info_ref (info);
	}

	xdg_mime_shutdown ();

	tracker_db_close_all (tracker->index_db);
	tracker_db_thread_end ();

	g_mutex_unlock (tracker->files_stopped_mutex);

        return NULL;
}
