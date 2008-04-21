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

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef HAVE_INOTIFY_LINUX
#include <linux/inotify.h>
#include "linux-inotify-syscalls.h"
#else
#include <sys/inotify.h>
#endif

#include <glib.h>
#include <glib/gstdio.h>

#include <libtracker-common/tracker-log.h>

#include "tracker-watch.h"
#include "tracker-process-files.h"

#define INOTIFY_WATCH_LIMIT "/proc/sys/fs/inotify/max_user_watches"

/* project wide global vars */

extern Tracker	    *tracker;
extern DBConnection *main_thread_db_con;

/* list to temporarily store moved_from events so they can be matched against moved_to events */
static GSList 	    *move_list;
static GQueue 	    *inotify_queue;
static int           inotify_monitor_fd = -1;
static int           inotify_count = 0;

static GIOChannel   *gio;


static gboolean process_moved_events ();


gboolean
tracker_is_directory_watched (const char * dir, DBConnection *db_con)
{
	TrackerDBResultSet *result_set;
	gint id;

	g_return_val_if_fail (dir != NULL && dir[0] == G_DIR_SEPARATOR, FALSE);

	if (!tracker->is_running) {
		return FALSE;
	}

	result_set = tracker_exec_proc (db_con->cache, "GetWatchID", dir, NULL);

	if (!result_set) {
		return FALSE;
	}

	tracker_db_result_set_get (result_set, 0, &id, -1);
	g_object_unref (result_set);

	return (id >= 0);
}


static gboolean
is_delete_event (TrackerChangeAction event_type)
{
	return (event_type == TRACKER_ACTION_DELETE ||
		event_type == TRACKER_ACTION_DELETE_SELF ||
		event_type == TRACKER_ACTION_FILE_DELETED ||
		event_type == TRACKER_ACTION_DIRECTORY_DELETED);
}



static void
process_event (const char *uri, gboolean is_dir, TrackerChangeAction action, guint32 cookie)
{
	FileInfo *info;

	g_return_if_fail (uri && (uri[0] == '/'));

	info = tracker_create_file_info (uri, action, 1, WATCH_OTHER);

	if (!tracker_process_files_is_file_info_valid (info)) {
		return;
	}

	info->is_directory = is_dir;

	if (is_delete_event (action)) {
		char *parent;

		parent = g_path_get_dirname (info->uri);

		if (tracker_file_is_valid (parent)) {
			g_async_queue_push (tracker->file_process_queue, info);
			tracker_notify_file_data_available ();
		} else {
			info = tracker_free_file_info (info);
		}

		g_free (parent);
		return;
	}

	/* we are not interested in create events for non-folders (we use writable file closed instead) */
	if (action == TRACKER_ACTION_DIRECTORY_CREATED) {

		info->action = TRACKER_ACTION_DIRECTORY_CREATED;
		info->is_directory = TRUE;
		tracker_db_insert_pending_file (main_thread_db_con, info->file_id, info->uri,  NULL, info->mime, 0, info->action, info->is_directory, TRUE, -1);
		info = tracker_free_file_info (info);
		return;

	} else if (action == TRACKER_ACTION_FILE_CREATED) {
		tracker_add_io_grace (info->uri);
		info = tracker_free_file_info (info);
		return;

	} else	if (action == TRACKER_ACTION_DIRECTORY_MOVED_FROM || action == TRACKER_ACTION_FILE_MOVED_FROM) {
		tracker_add_io_grace (info->uri);
		info->cookie = cookie;
		info->counter = 1;
		move_list = g_slist_prepend (move_list, info);
		g_timeout_add_full (G_PRIORITY_LOW,
		    350,
		    (GSourceFunc) process_moved_events,
		    NULL, NULL
		    );
		return;

	} else if (action == TRACKER_ACTION_FILE_MOVED_TO || action == TRACKER_ACTION_DIRECTORY_MOVED_TO) {
		FileInfo *moved_to_info;
		GSList   *tmp;

		moved_to_info = info;
		tracker_add_io_grace (info->uri);
		for (tmp = move_list; tmp; tmp = tmp->next) {
			FileInfo *moved_from_info;

			moved_from_info = (FileInfo *) tmp->data;

			if (!moved_from_info) {
				tracker_error ("ERROR: bad FileInfo struct found in move list. Skipping...");
				continue;
			}

			if ((cookie > 0) && (moved_from_info->cookie == cookie)) {

				tracker_info ("found matching inotify pair from %s to %s", moved_from_info->uri, moved_to_info->uri);

				tracker->grace_period = 2;
				tracker->request_waiting = TRUE;

				if (!tracker_is_directory (moved_to_info->uri)) {
					tracker_db_insert_pending_file (main_thread_db_con, moved_from_info->file_id, moved_from_info->uri, moved_to_info->uri, moved_from_info->mime, 0, TRACKER_ACTION_FILE_MOVED_FROM, FALSE, TRUE, -1);
					
//					tracker_db_move_file (main_thread_db_con, moved_from_info->uri, moved_to_info->uri);
				} else {
					tracker_db_insert_pending_file (main_thread_db_con, moved_from_info->file_id, moved_from_info->uri, moved_to_info->uri, moved_from_info->mime, 0, TRACKER_ACTION_DIRECTORY_MOVED_FROM, TRUE, TRUE, -1);
//					tracker_db_move_directory (main_thread_db_con, moved_from_info->uri, moved_to_info->uri);
				}

				move_list = g_slist_remove (move_list, tmp->data);

				return;
			}
		}

		/* matching pair not found so treat as a create action */
		tracker_debug ("no matching pair found for inotify move event for %s", info->uri);
		if (tracker_is_directory (info->uri)) {
			info->action = TRACKER_ACTION_DIRECTORY_CREATED;
		} else {
			info->action = TRACKER_ACTION_WRITABLE_FILE_CLOSED;
		}
		tracker_db_insert_pending_file (main_thread_db_con, info->file_id, info->uri,  NULL, info->mime, 10, info->action, info->is_directory, TRUE, -1);
		info = tracker_free_file_info (info);
		return;

	} else if (action == TRACKER_ACTION_WRITABLE_FILE_CLOSED) {
		tracker_add_io_grace (info->uri);
		tracker_info ("File %s has finished changing", info->uri);
		tracker_db_insert_pending_file (main_thread_db_con, info->file_id, info->uri,  NULL, info->mime, 0, info->action, info->is_directory, TRUE, -1);
		info = tracker_free_file_info (info);
		return;

	}

	tracker_log ("WARNING: not processing event %s for uri %s", tracker_actions[info->action], info->uri);
	tracker_free_file_info (info);
}


static gboolean
process_moved_events ()
{
	const GSList *tmp;

	if (!tracker->is_running) {
		return FALSE;
	}

	if (!move_list) {
		return TRUE;
	}

	for (tmp = move_list; tmp; tmp = tmp->next) {
		FileInfo *info;

		info = (FileInfo *) tmp->data;

		/* make it a DELETE if we have not received a corresponding MOVED_TO event after a certain period */
		if ((info->counter < 1) && ((info->action == TRACKER_ACTION_FILE_MOVED_FROM) || (info->action == TRACKER_ACTION_DIRECTORY_MOVED_FROM))) {

			/* make sure file no longer exists before issuing a "delete" */

			if (!tracker_file_is_valid (info->uri)) {
				if (info->action == TRACKER_ACTION_DIRECTORY_MOVED_FROM) {
					process_event (info->uri, TRUE, TRACKER_ACTION_DIRECTORY_DELETED, 0);
				} else {
					process_event (info->uri, FALSE, TRACKER_ACTION_FILE_DELETED, 0);
				}
			}

			move_list = g_slist_remove (move_list, info);
			tracker_free_file_info (info);
			continue;

		} else {
			info->counter--;
		}
	}

	if (!move_list)
		return FALSE;

	return TRUE;
}


static TrackerChangeAction
get_event (guint32 event_type)
{
	if (event_type & IN_DELETE) {
		if (event_type & IN_ISDIR) {
			return TRACKER_ACTION_DIRECTORY_DELETED;
		} else {
			return TRACKER_ACTION_FILE_DELETED;
		}
	}

	if (event_type & IN_DELETE_SELF) {
		if (event_type & IN_ISDIR) {
			return TRACKER_ACTION_DIRECTORY_DELETED;
		} else {
			return TRACKER_ACTION_FILE_DELETED;
		}
	}


	if (event_type & IN_MOVED_FROM) {
		if (event_type & IN_ISDIR) {
			return TRACKER_ACTION_DIRECTORY_MOVED_FROM;
		} else {
			return TRACKER_ACTION_FILE_MOVED_FROM;
		}
	}

	if (event_type & IN_MOVED_TO) {
		if (event_type & IN_ISDIR) {
			return TRACKER_ACTION_DIRECTORY_MOVED_TO;
		} else {
			return TRACKER_ACTION_FILE_MOVED_TO;
		}
	}


	if (event_type & IN_CLOSE_WRITE) {
		return TRACKER_ACTION_WRITABLE_FILE_CLOSED;
	}


	if (event_type & IN_CREATE) {
		if (event_type & IN_ISDIR) {
			return TRACKER_ACTION_DIRECTORY_CREATED;
		} else {
			return TRACKER_ACTION_FILE_CREATED;
		}
	}

	return TRACKER_ACTION_IGNORE;
}

static gboolean
process_inotify_events (void)
{
	while (g_queue_get_length (inotify_queue) > 0) {
		TrackerDBResultSet   *result_set;
		TrackerChangeAction  action_type;
		char		     *str = NULL, *filename = NULL, *monitor_name = NULL, *str_wd;
		char		     *file_utf8_uri = NULL, *dir_utf8_uri = NULL;
		guint		     cookie;

		struct inotify_event *event;

		if (!tracker->is_running) {
			return FALSE;
		}

		event = g_queue_pop_head (inotify_queue);

		if (!event) {
			continue;
		}

		action_type = get_event (event->mask);

		if (action_type == TRACKER_ACTION_IGNORE) {
			g_free (event);
			continue;
		}

		if (event->len > 1) {
			filename = event->name;
		} else {
			filename = NULL;
		}

		cookie = event->cookie;

		/* get watch name as monitor */

		str_wd = g_strdup_printf ("%d", event->wd);

		result_set = tracker_exec_proc (main_thread_db_con->cache, "GetWatchUri", str_wd, NULL);

		g_free (str_wd);

		if (result_set) {
			tracker_db_result_set_get (result_set, 0, &monitor_name, -1);
			g_object_unref (result_set);

			if (!monitor_name) {
				g_free (event);
				continue;
			}
		} else {
			g_free (event);
			continue;
		}

		if (tracker_is_empty_string (filename)) {
			//tracker_log ("WARNING: inotify event has no filename");
			g_free (event);
			continue;
		}

		file_utf8_uri = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);

		if (tracker_is_empty_string (file_utf8_uri)) {
			tracker_error ("ERROR: file uri could not be converted to utf8 format");
			g_free (event);
			continue;
		}

		if (file_utf8_uri[0] == G_DIR_SEPARATOR) {
			str = g_strdup (file_utf8_uri);
			dir_utf8_uri = NULL;
		} else {

			dir_utf8_uri = g_filename_to_utf8 (monitor_name, -1, NULL, NULL, NULL);

			if (!dir_utf8_uri) {
				tracker_error ("Error: file uri could not be converted to utf8 format");
				g_free (file_utf8_uri);
				g_free (event);
				continue;
			}

			str = g_build_filename(dir_utf8_uri, file_utf8_uri, NULL);
		}

		if (str && str[0] == '/' && 
                    (!tracker_process_files_should_be_ignored (str) || 
                     action_type == TRACKER_ACTION_DIRECTORY_MOVED_FROM) && 
                    tracker_process_files_should_be_crawled (tracker, str) && 
                    tracker_process_files_should_be_watched (tracker->config, str)) {
			process_event (str, tracker_is_directory (str), action_type, cookie);
		} else {
			tracker_debug ("ignoring action %d on file %s", action_type, str);
		}

		if (monitor_name) {
			g_free (monitor_name);
		}

		if (str) {
			g_free (str);
		}

		if (file_utf8_uri) {
			g_free (file_utf8_uri);
		}

		if (dir_utf8_uri) {
			g_free (dir_utf8_uri);
		}
		g_free (event);
	}

	return FALSE;
}


static gboolean
inotify_watch_func (GIOChannel *source, GIOCondition condition, gpointer data)
{
	char   buffer[16384];
	size_t buffer_i;
	size_t r;
	int    fd;

	fd = g_io_channel_unix_get_fd (source);

	r = read (fd, buffer, 16384);

	if (r <= 0) {
		tracker_error ("ERROR: inotify system failure - unable to watch files");
		return FALSE;
	}

	buffer_i = 0;

	while (buffer_i < (size_t)r) {
		struct inotify_event *pevent, *event;
		size_t event_size;

		/* Parse events and process them ! */

		if (!tracker->is_running) {
			return FALSE;
		}

		pevent = (struct inotify_event *) &buffer[buffer_i];
		event_size = sizeof (struct inotify_event) + pevent->len;
		event = g_memdup (pevent, event_size);
		g_queue_push_tail (inotify_queue, event);
		buffer_i += event_size;
	}

	g_idle_add ((GSourceFunc) process_inotify_events, NULL);

	return TRUE;
}


gboolean
tracker_start_watching (void)
{
	g_return_val_if_fail (inotify_monitor_fd == -1, FALSE);

	inotify_monitor_fd = inotify_init ();

	g_return_val_if_fail (inotify_monitor_fd >= 0, FALSE);

	inotify_queue = g_queue_new ();

	if (tracker->watch_limit == 0) {
		tracker->watch_limit = 8191;
		if (g_file_test (INOTIFY_WATCH_LIMIT, G_FILE_TEST_EXISTS)) {
			gchar   *limit;
			gsize    size;
			if (g_file_get_contents (INOTIFY_WATCH_LIMIT, &limit, &size, NULL)) {

				/* leave 500 watches for other users */
				tracker->watch_limit = atoi (limit) - 500;

				tracker_log ("Setting inotify watch limit to %d.", tracker->watch_limit);
				g_free (limit);
			}
		}
	}

	gio = g_io_channel_unix_new (inotify_monitor_fd);
	g_io_add_watch (gio, G_IO_IN, inotify_watch_func, NULL);
	g_io_channel_set_flags (gio, G_IO_FLAG_NONBLOCK, NULL);

	/* periodically process unmatched moved_from events */

	return TRUE;
}


int
tracker_count_watch_dirs (void)
{
	return inotify_count;
}


gboolean
tracker_add_watch_dir (const char *dir, DBConnection *db_con)
{
	char *dir_in_locale;
	static gboolean limit_exceeded_msg = FALSE;

	g_return_val_if_fail (dir, FALSE);
	g_return_val_if_fail (dir[0] == '/', FALSE);

	if (!tracker->is_running) {
		return FALSE;
	}

	if (tracker_is_directory_watched (dir, db_con)) {
		return FALSE;
	}

	if (tracker_count_watch_dirs () >= (int) tracker->watch_limit) {

		if (!limit_exceeded_msg) {
			tracker_log ("Inotify Watch Limit has been exceeded - for best results you should increase number of inotify watches on your system");
			limit_exceeded_msg = TRUE;
		}

		return FALSE;
	}

	dir_in_locale = g_filename_from_utf8 (dir, -1, NULL, NULL, NULL);

	/* check directory permissions are okay */
	if (g_access (dir_in_locale, F_OK) == 0 && g_access (dir_in_locale, R_OK) == 0) {
		const guint32 mask = (IN_CLOSE_WRITE | IN_MOVE | IN_CREATE | IN_DELETE| IN_DELETE_SELF | IN_MOVE_SELF);
		int   wd;
		char *str_wd;

		wd = inotify_add_watch (inotify_monitor_fd, dir_in_locale, mask);

		g_free (dir_in_locale);

		if (wd < 0) {
			tracker_error ("ERROR: Inotify watch on %s has failed", dir);
			return FALSE;
		}

		str_wd = g_strdup_printf ("%d", wd);
		tracker_exec_proc (db_con->cache, "InsertWatch", dir, str_wd, NULL);
		g_free (str_wd);
		inotify_count++;
		tracker_log ("Watching directory %s (total watches = %d)", dir, inotify_count);
		return TRUE;
	}

	g_free (dir_in_locale);

	return FALSE;
}


static gboolean
delete_watch (const char *dir, DBConnection *db_con)
{
	TrackerDBResultSet *result_set;
	int wd;

	g_return_val_if_fail (dir != NULL && dir[0] == G_DIR_SEPARATOR, FALSE);

	result_set = tracker_exec_proc (db_con->cache, "GetWatchID", dir, NULL);

	wd = -1;

	if (!result_set) {
		tracker_log ("WARNING: watch id not found for uri %s", dir);
		return FALSE;
	}

	tracker_db_result_set_get (result_set, 0, &wd, -1);
	g_object_unref (result_set);

	tracker_exec_proc (db_con->cache, "DeleteWatch", dir, NULL);

	if (wd > -1) {
		inotify_rm_watch (inotify_monitor_fd, wd);
		inotify_count--;
	}

	return TRUE;
}


void
tracker_remove_watch_dir (const char *dir, gboolean delete_subdirs, DBConnection *db_con)
{
	TrackerDBResultSet *result_set;
	gboolean valid = TRUE;
	int wd;

	g_return_if_fail (dir != NULL && dir[0] == G_DIR_SEPARATOR);

	delete_watch (dir, db_con);

	if (!delete_subdirs) {
		return;
	}

	result_set = tracker_db_get_sub_watches (db_con, dir);

	wd = -1;

	if (!result_set) {
		return;
	}

	while (valid) {
		tracker_db_result_set_get (result_set, 0, &wd, -1);
		valid = tracker_db_result_set_iter_next (result_set);

		if (wd < 0) {
			continue;
		}

		inotify_rm_watch (inotify_monitor_fd, wd);
		inotify_count--;
	}

	g_object_unref (result_set);
	tracker_db_delete_sub_watches (db_con, dir);
}


void
tracker_end_watching (void)
{
	if (gio) {
		g_io_channel_shutdown (gio, TRUE, NULL);
	}
}
