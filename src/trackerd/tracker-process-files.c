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
#include "tracker-hal.h"
#include "tracker-indexer.h"
#include "tracker-os-dependant.h"
#include "tracker-utils.h"
#include "tracker-watch.h"
#include "tracker-service.h"
#include "tracker-process-files.h"

static GSList       *ignore_pattern_list;
static GSList       *temp_black_list;
static GSList       *crawl_directories;
        
static gchar       **ignore_pattern;

static const gchar  *ignore_suffix[] = {
        "~", ".o", ".la", ".lo", ".loT", ".in", 
        ".csproj", ".m4", ".rej", ".gmo", ".orig", 
        ".pc", ".omf", ".aux", ".tmp", ".po", 
        ".vmdk",".vmx",".vmxf",".vmsd",".nvram", 
        ".part", NULL
};

static const gchar  *ignore_prefix[] = { 
        "autom4te", "conftest.", "confstat", 
        "config.", NULL 
};

static const gchar  *ignore_name[] = { 
        "po", "CVS", "aclocal", "Makefile", "CVS", 
        "SCCS", "ltmain.sh","libtool", "config.status", 
        "conftest", "confdefs.h", NULL
};

static void
process_my_yield (void)
{
#ifndef OS_WIN32
	while (g_main_context_iteration (NULL, FALSE)) {
                /* FIXME: do something here? */
	}
#endif
}

static GSList *
process_get_files (Tracker    *tracker,
                   const char *dir, 
                   gboolean    dir_only, 
                   gboolean    skip_ignored_files, 
                   const char *filter_prefix)
{
	GDir   *dirp;
	GSList *files;
	char   *dir_in_locale;

	dir_in_locale = g_filename_from_utf8 (dir, -1, NULL, NULL, NULL);

	if (!dir_in_locale) {
		tracker_error ("ERROR: dir could not be converted to utf8 format");
		g_free (dir_in_locale);
		return NULL;
	}

	files = NULL;

   	if ((dirp = g_dir_open (dir_in_locale, 0, NULL))) {
		const gchar *name;

   		while ((name = g_dir_read_name (dirp))) {
                        gchar *filename;
			gchar *built_filename;

			if (!tracker->is_running) {
				g_free (dir_in_locale);
				g_dir_close (dirp);
				return NULL;
			}

			filename = g_filename_to_utf8 (name, -1, NULL, NULL, NULL);

			if (!filename) {
				continue;
			}

			if (filter_prefix && !g_str_has_prefix (filename, filter_prefix)) {
				g_free (filename);
				continue;
			}

			if (skip_ignored_files && 
                            tracker_process_files_should_be_ignored (filename)) {
				g_free (filename);
				continue;
			}

			built_filename = g_build_filename (dir, filename, NULL);
			g_free (filename);

 			if (!tracker_file_is_valid (built_filename)) {
				g_free (built_filename);
				continue;
			}

                        if (!tracker_process_files_should_be_crawled (tracker, built_filename)) {
                                g_free (built_filename);
                                continue;
                        }

			if (!dir_only || tracker_is_directory (built_filename)) {
				if (tracker_process_files_should_be_watched (tracker->config, built_filename)) {
					files = g_slist_prepend (files, built_filename);
                                } else {
                                        g_free (built_filename);
                                }
                        } else {
                                g_free (built_filename);
			}
		}

 		g_dir_close (dirp);
	}

	g_free (dir_in_locale);

	if (!tracker->is_running) {
		if (files) {
			g_slist_foreach (files, (GFunc) g_free, NULL);
			g_slist_free (files);
		}

		return NULL;
	}

	return files;
}

static void
process_get_directories (Tracker     *tracker, 
                         const char  *dir, 
                         GSList     **files)
{
	GSList *l;

        l = process_get_files (tracker, dir, TRUE, TRUE, NULL);

        if (*files) {
                *files = g_slist_concat (*files, l);
        } else {
                *files = l;
	}
}

static void
process_watch_directories (Tracker *tracker,
                           GSList  *dirs)
{
        GSList *list;

        if (!tracker->is_running) {
		return;
	}
        
	/* Add sub directories breadth first recursively to avoid
         * running out of file handles.
         */
        list = dirs;
        
	while (list) {
                GSList *files = NULL;
                GSList *l;

		for (l = list; l; l = l->next) {
                        gchar *dir;
                        guint  watches;

                        if (!l->data) {
                                continue;
                        }

                        if (!g_utf8_validate (l->data, -1, NULL)) {
                                dir = g_filename_to_utf8 (l->data, -1, NULL,NULL,NULL);
                                if (!dir) {
                                        tracker_error ("Directory to watch was not valid UTF-8 and couldn't be converted either");
                                        continue;
                                }
                        } else {
                                dir = g_strdup (l->data);
                        }

                        if (!tracker_file_is_valid (dir)) {
                                g_free (dir);
                                continue;
                        }
                        
                        if (!tracker_is_directory (dir)) {
                                g_free (dir);
                                continue;
                        }
                                  
			if (!tracker_process_files_should_be_watched (tracker->config, dir) || 
                            !tracker_process_files_should_be_watched (tracker->config, dir)) {
                                continue;
                        }

                        crawl_directories = g_slist_prepend (crawl_directories, dir);
                        
                        if (!tracker_config_get_enable_watches (tracker->config)) {
                                continue;
                        }
                        
                        watches = tracker_count_watch_dirs () + g_slist_length (list);
                        
                        if (watches < tracker->watch_limit) {
                                if (!tracker_add_watch_dir (dir, tracker->index_db)) {
                                        tracker_debug ("Watch failed for %s", dir);
                                }
                        }
		}

                for (l = list; l; l = l->next) {
                        process_get_directories (tracker, l->data, &files);
                }

                /* Don't free original list */
                if (list != dirs) {
                        g_slist_foreach (list, (GFunc) g_free, NULL);
                        g_slist_free (list);
                }

		list = files;
	}
}

static void
process_schedule_directory_check_foreach (const gchar  *uri, 
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
		tracker_db_insert_pending_file (tracker->index_db, 0, uri, NULL, "unknown", 0, 
                                                TRACKER_ACTION_CHECK, 0, FALSE, -1);
	} else {
		process_schedule_directory_check_foreach (uri, tracker);
	}
}

static inline void
process_directory_list (Tracker  *tracker, 
                        GSList   *list, 
                        gboolean  recurse)
{
	crawl_directories = NULL;

	if (!list) {
		return;
	}

        process_watch_directories (tracker, list);

	g_slist_foreach (list, 
                         (GFunc) process_schedule_directory_check_foreach, 
                         tracker);

	if (recurse && crawl_directories) {
		g_slist_foreach (crawl_directories, 
                                 (GFunc) process_schedule_directory_check_foreach, 
                                 tracker);
	}

	if (crawl_directories) {
		g_slist_foreach (crawl_directories, (GFunc) g_free, NULL);
		g_slist_free (crawl_directories);
                crawl_directories = NULL;
	}
}

static void
process_scan_directory (Tracker     *tracker,
                        const gchar *uri)
{
	GSList *files;

	if (!tracker->is_running) {
		return;
	}

	g_return_if_fail (tracker->index_db);
	g_return_if_fail (tracker_check_uri (uri));
	g_return_if_fail (tracker_is_directory (uri));

	/* Keep mainloop responsive */
	process_my_yield ();

        files = process_get_files (tracker, uri, FALSE, TRUE, NULL);

	tracker_debug ("Scanning %s for %d files", uri, g_slist_length (files));

	g_slist_foreach (files, 
                         (GFunc) process_schedule_file_check_foreach, 
                         tracker);
	g_slist_foreach (files, 
                         (GFunc) g_free, 
                         NULL);
	g_slist_free (files);

	/* Recheck directory to update its mtime if its changed whilst
         * scanning.
         */
	process_schedule_directory_check_foreach (uri, tracker);
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
        TrackerService *def;
	gchar      *service_info;

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

	def = tracker_service_manager_get_service (service_info);

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
		info->is_hidden = !tracker_service_get_show_service_directories (def);
		tracker_db_index_file (tracker->index_db, info, NULL, NULL);
		g_free (service_info);
		return;
	} else {
		info->is_hidden = !tracker_service_get_show_service_files (def);
	}

	if (g_str_has_suffix (service_info, "Emails")) {
                DBConnection *db_con;

                db_con = tracker->index_db;

		if (!tracker_email_index_file (db_con->emails, info)) {
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
process_index_delete_file (Tracker  *tracker, 
                           FileInfo *info)
{
	/* Info struct may have been deleted in transit here so check
         * if still valid and intact.
         */
	g_return_if_fail (tracker_process_files_is_file_info_valid (info));

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
process_index_delete_directory (Tracker  *tracker, 
                                FileInfo *info)
{
	/* Info struct may have been deleted in transit here so check
         * if still valid and intact.
         */
	g_return_if_fail (tracker_process_files_is_file_info_valid (info));

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
process_index_delete_directory_check (Tracker     *tracker,
                                      const gchar *uri)
{
	gchar **files;
        gchar **p;

	/* Check for any deletions*/
	files = tracker_db_get_files_in_folder (tracker->index_db, uri);

        if (!files) {
                return;
        }

	for (p = files; *p; p++) {
		gchar *str = *p;

		if (!tracker_file_is_valid (str)) {
                        FileInfo *info;

			info = tracker_create_file_info (str, 1, 0, 0);
			info = tracker_db_get_file_info (tracker->index_db, info);

			if (!info->is_directory) {
				process_index_delete_file (tracker, info);
			} else {
				process_index_delete_directory (tracker, info);
			}
			tracker_free_file_info (info);
		}
	}

	g_strfreev (files);
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
	GSList *files;

	if (!tracker->is_running) {
		return;
	}

	g_return_if_fail (tracker_check_uri (uri));
	g_return_if_fail (tracker_is_directory (uri));

        files = process_get_files (tracker, uri, FALSE, TRUE, NULL);
	tracker_debug ("Checking %s for %d files", uri, g_slist_length (files));

	g_slist_foreach (files, (GFunc) process_queue_files_foreach, tracker);
	g_slist_foreach (files, (GFunc) g_free, NULL);
	g_slist_free (files);

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
        tracker_db_interface_start_transaction (db_con->cache->db);
        
        tracker_applications_add_service_directories ();
        
        list = tracker_get_service_dirs ("Applications");
        process_directory_list (tracker, list, FALSE);

        tracker_db_interface_end_transaction (db_con->cache->db);
        
        g_slist_free (list);
}

static void
process_index_get_remote_roots (Tracker  *tracker,
                                GSList  **mounted_directory_roots, 
                                GSList  **removable_device_roots)
{
        GSList *l1 = NULL;
        GSList *l2 = NULL;

#ifdef HAVE_HAL        
        l1 = tracker_hal_get_mounted_directory_roots (tracker->hal);
        l2 = tracker_hal_get_removable_device_roots (tracker->hal);
#endif 
        
        /* The options to index removable media and the index mounted
         * directories are both mutually exclusive even though
         * removable media is mounted on a directory.
         *
         * Since we get ALL mounted directories from HAL, we need to
         * remove those which are removable device roots.
         */
        if (l2) {
                GSList *l;
                GSList *list = NULL;
                       
                for (l = l1; l; l = l->next) {
                        if (g_slist_find_custom (l2, l->data, (GCompareFunc) strcmp)) {
                                continue;
                        } 
                        
                        list = g_slist_prepend (list, l->data);
                }

                *mounted_directory_roots = g_slist_reverse (list);
        } else {
                *mounted_directory_roots = NULL;
        }

        *removable_device_roots = g_slist_copy (l2);
}

static void
process_index_get_roots (Tracker  *tracker,
                         GSList  **included,
                         GSList  **excluded)
{
        GSList *watch_directory_roots;
        GSList *no_watch_directory_roots;
        GSList *mounted_directory_roots;
        GSList *removable_device_roots;

        *included = NULL;
        *excluded = NULL;

        process_index_get_remote_roots (tracker,
                                        &mounted_directory_roots, 
                                        &removable_device_roots);        
        
        /* Delete all stuff in the no watch dirs */
        watch_directory_roots = 
                tracker_config_get_watch_directory_roots (tracker->config);
        
        no_watch_directory_roots = 
                tracker_config_get_no_watch_directory_roots (tracker->config);

        /* Create list for enabled roots based on config */
        *included = g_slist_concat (*included, g_slist_copy (watch_directory_roots));
        
        /* Create list for disabled roots based on config */
        *excluded = g_slist_concat (*excluded, g_slist_copy (no_watch_directory_roots));

        /* Add or remove roots which pertain to removable media */
        if (tracker_config_get_index_removable_devices (tracker->config)) {
                *included = g_slist_concat (*included, g_slist_copy (removable_device_roots));
        } else {
                *excluded = g_slist_concat (*excluded, g_slist_copy (removable_device_roots));
        }

        /* Add or remove roots which pertain to mounted directories */
        if (tracker_config_get_index_mounted_directories (tracker->config)) {
                *included = g_slist_concat (*included, g_slist_copy (mounted_directory_roots));
        } else {
                *excluded = g_slist_concat (*excluded, g_slist_copy (mounted_directory_roots));
        }
}

static void
process_index_files (Tracker *tracker)
{
        DBConnection *db_con;
        GSList       *index_include;
        GSList       *index_exclude;
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

        /* FIXME: Is this safe? shouldn't we free first? */
        crawl_directories = NULL;
        
        tracker_db_start_index_transaction (db_con);
	
        process_index_get_roots (tracker, &index_include, &index_exclude);

        if (index_exclude) {
                GSList *l;
                
                tracker_log ("Deleting entities where indexing is disabled or are not watched:");
                
                for (l = index_exclude; l; l = l->next) {
                        guint32 id;

                        tracker_log ("  %s", l->data);

                        id = tracker_db_get_file_id (db_con, l->data);
                        
                        if (id > 0) {
                                tracker_db_delete_directory (db_con, id, l->data);
                        }
                }

                g_slist_free (index_exclude);
        }
        
        if (!index_include) {
                tracker_log ("No directory roots to index!");
                return;
        }
        
        tracker_db_interface_start_transaction (db_con->cache->db);
        
        /* Index watched dirs first */
        process_watch_directories (tracker, index_include);
       
        g_slist_foreach (crawl_directories, 
                         (GFunc) process_schedule_directory_check_foreach, 
                         tracker);
        
        if (crawl_directories) {
                g_slist_foreach (crawl_directories, 
                                 (GFunc) g_free, 
                                 NULL);
                g_slist_free (crawl_directories);
                crawl_directories = NULL;
        }
        
        g_slist_foreach (index_include, 
                         (GFunc) process_schedule_directory_check_foreach, 
                         tracker);
        
        if (crawl_directories) {
                g_slist_foreach (crawl_directories, 
                                 (GFunc) g_free, 
                                 NULL);
                g_slist_free (crawl_directories);
                crawl_directories = NULL;
        }
        
        tracker_db_interface_end_transaction (db_con->cache->db);
        tracker_dbus_send_index_progress_signal ("Files", "");

        g_slist_free (index_include);
}

static void
process_index_crawl_add_directories (Tracker *tracker, 
                                     GSList  *dirs)
{
	GSList *new_dirs = NULL;
        GSList *l;

	if (!tracker->is_running) {
		return;
	}

	for (l = dirs; l; l = l->next) {
		if (!l->data) {
                        continue;
                }

                new_dirs = g_slist_prepend (new_dirs, g_strdup (l->data));
	}

	/* Add sub directories breadth first recursively to avoid
         * running out of file handles.
         */
	while (new_dirs) {
                GSList *files = NULL;

		for (l = new_dirs; l; l = l->next) {
                        if (!l->data) {
                                continue;
                        }

			if (tracker_process_files_should_be_watched (tracker->config, l->data)) {
				crawl_directories = g_slist_prepend (crawl_directories, g_strdup (l->data));
			}
		}

                for (l = new_dirs; l; l = l->next) {
                        process_get_directories (tracker, l->data, &files);
                }

		g_slist_foreach (new_dirs, (GFunc) g_free, NULL);
		g_slist_free (new_dirs);

		new_dirs = files;
	}
}

static void
process_index_crawl_files (Tracker *tracker)
{
        DBConnection *db_con;
        GSList       *crawl_directory_roots;
        
        db_con = tracker->index_db;

        tracker_log ("Starting directory crawling...");

        crawl_directories = NULL;
        crawl_directory_roots = 
                tracker_config_get_crawl_directory_roots (tracker->config);
        
        if (!crawl_directory_roots) {
                return;
        }
        
        tracker_db_interface_start_transaction (db_con->cache->db);
        
        process_index_crawl_add_directories (tracker, crawl_directory_roots);
        
        g_slist_foreach (crawl_directories, 
                         (GFunc) process_schedule_directory_check_foreach, 
                         tracker);
        
        if (crawl_directories) {
                g_slist_foreach (crawl_directories, (GFunc) g_free, NULL);
                g_slist_free (crawl_directories);
                crawl_directories = NULL;
        }
        
        g_slist_foreach (crawl_directory_roots, 
                         (GFunc) process_schedule_directory_check_foreach, 
                         tracker);
        
        if (crawl_directories) {
                g_slist_foreach (crawl_directories, (GFunc) g_free, NULL);
                g_slist_free (crawl_directories);
                crawl_directories = NULL;
        }
        
        tracker_db_interface_end_transaction (db_con->cache->db);
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
                tracker_db_interface_start_transaction (db_con->cache->db);
                process_directory_list (tracker, list, TRUE);
                tracker_db_interface_end_transaction (db_con->cache->db);
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
                
                tracker_db_interface_start_transaction (db_con->cache->db);
                process_directory_list (tracker, list, TRUE);
                tracker_db_interface_end_transaction (db_con->cache->db);
                g_slist_free (list);
        }

        g_free (firefox_dir);
} 

static void
process_index_emails (Tracker *tracker)
{
        DBConnection  *db_con;
        TrackerConfig *config;
       
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

	if (tracker_config_get_email_client (tracker->config)) {
		const gchar *name;

                tracker_email_add_service_directories (db_con->emails);
                tracker_log ("Starting email indexing...");
                
                tracker_db_interface_start_transaction (db_con->cache->db);

		name = tracker_email_get_name ();

                if (name) {
                        GSList *list;

                        list = tracker_get_service_dirs (name);
                        process_directory_list (tracker, list, TRUE);
                        g_slist_free (list);
                }
                
                tracker_db_interface_end_transaction (db_con->cache->db);
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
                tracker_set_status (tracker, STATUS_OPTIMIZING, 0, FALSE);
                tracker->do_optimize = FALSE;
                
                tracker->first_time_index = FALSE;
		
                tracker_dbus_send_index_finished_signal ();
                tracker_db_set_option_int (db_con, "InitialIndex", 0);
                
                tracker->update_count = 0;
                
                tracker_log ("Updating database stats, please wait...");
                
                tracker_db_interface_start_transaction (db_con->db);
                tracker_db_exec_no_reply (db_con, "ANALYZE");
                tracker_db_interface_end_transaction (db_con->db);
                
                tracker_db_interface_start_transaction (db_con->emails->db);
                tracker_db_exec_no_reply (db_con->emails, "ANALYZE");
                tracker_db_interface_end_transaction (db_con->emails->db);
                
                tracker_log ("Finished optimizing, waiting for new events...");
        }
        
        /* We have no stuff to process so
         * sleep until awoken by a new
         * signal.
         */
        tracker_set_status (tracker, STATUS_IDLE, 0, TRUE);
        
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
                if (need_index && 
                    tracker_process_files_should_be_watched (tracker->config, info->uri)) {
                        g_async_queue_push (tracker->dir_queue, g_strdup (info->uri));
                        
                        if (tracker->index_status != INDEX_EMAILS) {
                                tracker->folders_count++;
                        }
                }
                
                need_index = FALSE;
                break;
                
        case TRACKER_ACTION_DIRECTORY_CHECK:
                if (need_index && 
                    tracker_process_files_should_be_watched (tracker->config, info->uri)) {
                        g_async_queue_push (tracker->dir_queue, g_strdup (info->uri));
			
                        if (info->indextime > 0) {
                                process_index_delete_directory_check (tracker, info->uri);
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
                if (tracker_process_files_should_be_watched (tracker->config, info->uri)) {
                        GSList *list;

                        /* Add to watch folders (including
                         * subfolders).
                         */
                        list = g_slist_prepend (NULL, info->uri);

                        process_watch_directories (tracker, list);
                        process_scan_directory (tracker, info->uri);

                        g_slist_free (list);
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
        if (!tracker_process_files_is_file_info_valid (info)) {
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
            info->action != TRACKER_ACTION_DIRECTORY_UNMOUNTED &&
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
                        process_index_delete_file (tracker, info);
                        info = tracker_dec_info_ref (info);
                        return TRUE;
                } else {
                        if (info->action == TRACKER_ACTION_DIRECTORY_DELETED ||
                            info->action == TRACKER_ACTION_DIRECTORY_UNMOUNTED) {
                                process_index_delete_file (tracker, info);
                                process_index_delete_directory (tracker, info);
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

#ifdef HAVE_HAL

static void
process_mount_point_added_cb (TrackerHal  *hal,
                              const gchar *mount_point,
                              Tracker     *tracker)
{
        GSList *list;
        
        tracker_log ("** TRAWLING THROUGH NEW MOUNT POINT '%s'", mount_point);
        
        list = g_slist_prepend (NULL, (gchar*) mount_point);
        process_directory_list (tracker, list, TRUE);
        g_slist_free (list);
}

static void
process_mount_point_removed_cb (TrackerHal  *hal,
                                const gchar *mount_point,
                                Tracker     *tracker)
{
        tracker_log ("** CLEANING UP OLD MOUNT POINT '%s'", mount_point);
        
        process_index_delete_directory_check (tracker, mount_point); 
}

#endif /* HAVE_HAL */

static inline gboolean
process_is_in_path (const gchar *uri, 
                    const gchar *path)
{
	gchar    *str;
        gboolean  result;

        str = g_strconcat (path, G_DIR_SEPARATOR_S, NULL);
	result = g_str_has_prefix (uri, str);
	g_free (str);

	return result;
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

        /* When initially run, we set up variables */
        if (!ignore_pattern_list) {
                GSList *no_index_file_types;
                
                no_index_file_types = tracker_config_get_no_index_file_types (tracker->config);

                if (no_index_file_types) {
                        GPatternSpec  *spec;
                        gchar        **p;

                        ignore_pattern = tracker_gslist_to_string_list (no_index_file_types);
                        
                        for (p = ignore_pattern; *p; p++) {
                                spec = g_pattern_spec_new (*p);
                                ignore_pattern_list = g_slist_prepend (ignore_pattern_list, spec);
                        }
                        
                        ignore_pattern_list = g_slist_reverse (ignore_pattern_list);
                }
        }

        g_signal_connect (tracker->hal, "mount-point-added", 
                          G_CALLBACK (process_mount_point_added_cb),
                          tracker);
        g_signal_connect (tracker->hal, "mount-point-removed", 
                          G_CALLBACK (process_mount_point_removed_cb),
                          tracker);

        /* Start processing */
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
			tracker_set_status (tracker, STATUS_SHUTDOWN, 0, TRUE);
			break;	
		}

		tracker_set_status (tracker, STATUS_INDEXING, 0, TRUE);

		info = g_async_queue_try_pop (tracker->file_process_queue);

		/* Check pending table if we haven't got anything */
		if (!info) {
			TrackerDBResultSet *result_set;
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

			result_set = tracker_db_get_pending_files (tracker->index_db);

			k = 0;
			pushed_events = FALSE;

			if (result_set) {
				gboolean is_valid = TRUE;

				tracker_set_status (tracker, STATUS_PENDING, 0, FALSE);

				while (is_valid) {
					FileInfo	    *info_tmp;
					TrackerChangeAction  tmp_action;
					gchar               *uri;

					if (!tracker->is_running) {
						g_object_unref (result_set);
						break;
					}

					tracker_db_result_set_get (result_set,
								   1, &uri,
								   2, &tmp_action,
								   -1);

					info_tmp = tracker_create_file_info (uri, tmp_action, 0, WATCH_OTHER);
					g_async_queue_push (tracker->file_process_queue, info_tmp);
					is_valid = tracker_db_result_set_iter_next (result_set);
					pushed_events = TRUE;

					g_free (uri);
				}

				g_object_unref (result_set);
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
			if (!pushed_events) {
				g_usleep (100000);
			}

			continue;
		}

		tracker_set_status (tracker, STATUS_INDEXING, 0, TRUE);

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

        g_signal_handlers_disconnect_by_func (tracker->hal, 
                                              process_mount_point_added_cb,
                                              tracker);
        g_signal_handlers_disconnect_by_func (tracker->hal, 
                                              process_mount_point_removed_cb,
                                              tracker);

	xdg_mime_shutdown ();

	tracker_db_close_all (tracker->index_db);

	g_mutex_unlock (tracker->files_stopped_mutex);

        return NULL;
}

gboolean
tracker_process_files_should_be_watched (TrackerConfig *config,
                                         const gchar   *uri)
{
        GSList *no_watch_directory_roots;
	GSList *l;

        g_return_val_if_fail (TRACKER_IS_CONFIG (config), FALSE);
        g_return_val_if_fail (uri != NULL, FALSE);

	if (!tracker_check_uri (uri)) {
		return FALSE;
	}

	if (process_is_in_path (uri, g_get_tmp_dir ())) {
		return FALSE;
	}

	if (process_is_in_path (uri, "/proc")) {
		return FALSE;
	}

	if (process_is_in_path (uri, "/dev")) {
		return FALSE;
	}

	if (process_is_in_path (uri, "/tmp")) {
		return FALSE;
	}

        no_watch_directory_roots = tracker_config_get_no_watch_directory_roots (config);

	for (l = no_watch_directory_roots; l; l = l->next) {
                if (!l->data) {
                        continue;
                }

		/* Check if equal or a prefix with an appended '/' */
		if (strcmp (uri, l->data) == 0) {
			tracker_log ("Blocking watch of %s (already being watched)", uri);
			return FALSE;
		}

		if (process_is_in_path (uri, l->data)) {
			tracker_log ("Blocking watch of %s (already a watch in parent path)", uri);
			return FALSE;
		}
	}

	return TRUE;
}

gboolean
tracker_process_files_should_be_crawled (Tracker     *tracker,
                                         const gchar *uri)
{
        GSList   *crawl_directory_roots;
        GSList   *mounted_directory_roots = NULL;
        GSList   *removable_device_roots = NULL;
	GSList   *l;
        gboolean  index_mounted_directories;
        gboolean  index_removable_devices;
        gboolean  should_be_crawled = TRUE;

        g_return_val_if_fail (tracker != NULL, FALSE);
        g_return_val_if_fail (uri != NULL, FALSE);
        g_return_val_if_fail (uri[0] == G_DIR_SEPARATOR, FALSE);

        index_mounted_directories = tracker_config_get_index_mounted_directories (tracker->config);
        index_removable_devices = tracker_config_get_index_removable_devices (tracker->config);
        
        if (!index_mounted_directories || !index_removable_devices) {
                process_index_get_remote_roots (tracker,
                                                &mounted_directory_roots, 
                                                &removable_device_roots);        
        }

        l = tracker_config_get_crawl_directory_roots (tracker->config);

        crawl_directory_roots = g_slist_copy (l);

        if (!index_mounted_directories) {
                crawl_directory_roots = g_slist_concat (crawl_directory_roots, 
                                                        mounted_directory_roots);
        }

        if (!index_removable_devices) {
                crawl_directory_roots = g_slist_concat (crawl_directory_roots, 
                                                        removable_device_roots);
        }

	for (l = crawl_directory_roots; l && should_be_crawled; l = l->next) {
		/* Check if equal or a prefix with an appended '/' */
		if (strcmp (uri, l->data) == 0) {
			should_be_crawled = FALSE;
		}

		if (process_is_in_path (uri, l->data)) {
			should_be_crawled = FALSE;
		}
	}

        g_slist_free (crawl_directory_roots);

        tracker_log ("Indexer %s %s", 
                     should_be_crawled ? "crawling" : "blocking",
                     uri);

	return should_be_crawled;
}

gboolean
tracker_process_files_should_be_ignored (const char *uri)
{
	GSList       *l;
	gchar        *name = NULL;
	const gchar **p;
        gboolean      should_be_ignored = TRUE;

	if (tracker_is_empty_string (uri)) {
		goto done;
	}

	name = g_path_get_basename (uri);

	if (!name || name[0] == '.') {
		goto done;
	}

	if (process_is_in_path (uri, g_get_tmp_dir ())) {
		goto done;
	}

	if (process_is_in_path (uri, "/proc")) {
		goto done;
	}

	if (process_is_in_path (uri, "/dev")) {
		goto done;
	}

	if (process_is_in_path (uri, "/tmp")) {
		goto done;
	}

	/* Test suffixes */
	for (p = ignore_suffix; *p; p++) {
		if (g_str_has_suffix (name, *p)) {
                        goto done;
		}
	}

	/* Test prefixes */
	for (p = ignore_prefix; *p; p++) {
		if (g_str_has_prefix (name, *p)) {
                        goto done;
		}
	}

	/* Test exact names */
	for (p = ignore_name; *p; p++) {
		if (strcmp (name, *p) == 0) {
                        goto done;
		}
	}

	/* Test ignore types */
	if (ignore_pattern_list) {
                for (l = ignore_pattern_list; l; l = l->next) {
                        if (g_pattern_match_string (l->data, name)) {
                                goto done;
                        }
                }
	}
	
	/* Test tmp black list */
	for (l = temp_black_list; l; l = l->next) {
		if (!l->data) {
                        continue;
                }

		if (strcmp (uri, l->data) == 0) {
                        goto done;
		}
	}

        should_be_ignored = FALSE;

done:
	g_free (name);

	return should_be_ignored;
}

GSList *
tracker_process_files_get_temp_black_list (void)
{
        GSList *l;

        l = g_slist_copy (temp_black_list);
        
        return temp_black_list;
}

void
tracker_process_files_set_temp_black_list (GSList *black_list)
{
        tracker_process_files_free_temp_black_list ();
        temp_black_list = black_list;
}

void
tracker_process_files_free_temp_black_list (void)
{
        g_slist_foreach (temp_black_list, 
                         (GFunc) g_free,
                         NULL);

        g_slist_free (temp_black_list);

        temp_black_list = NULL;
}

void
tracker_process_files_append_temp_black_list (const gchar *str)
{
        g_return_if_fail (str != NULL);

        temp_black_list = g_slist_append (temp_black_list, g_strdup (str));
}

void
tracker_process_files_get_all_dirs (Tracker     *tracker, 
                                    const char  *dir, 
                                    GSList     **files)
{
	GSList *l;

        l = process_get_files (tracker, dir, TRUE, FALSE, NULL);

        if (*files) {
                *files = g_slist_concat (*files, l);
        } else {
                *files = l;
	}
}

GSList *
tracker_process_files_get_files_with_prefix (Tracker    *tracker,
                                             const char *dir, 
                                             const char *prefix)
{
	return process_get_files (tracker, dir, FALSE, FALSE, prefix);
}

gboolean
tracker_process_files_is_file_info_valid (FileInfo *info)
{
        g_return_val_if_fail (info != NULL, FALSE);
        g_return_val_if_fail (info->uri != NULL, FALSE);

        if (!g_utf8_validate (info->uri, -1, NULL)) {
                tracker_log ("Expected UTF-8 validation of FileInfo URI");
                return FALSE;
        }

        if (info->action == TRACKER_ACTION_IGNORE) {
                return FALSE;
        }
                               
        return TRUE;
}
