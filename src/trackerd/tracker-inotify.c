/* Tracker - Inotify interface
 * Copyright (C) 2005, Mr Jamie McCracken
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>

#include "tracker-inotify.h"


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
	char ***res;
	char **row;

	g_return_val_if_fail (dir != NULL && dir[0] == G_DIR_SEPARATOR, FALSE);

	if (!tracker->is_running) {
		return FALSE;
	}

	res = tracker_exec_proc (db_con, "GetWatchID", 1, dir);

	if (!res) {
		return FALSE;
	}

	row = tracker_db_get_row (res, 0);

	if (!row || !row[0] || atoi (row[0]) < 0) {
		tracker_db_free_result (res);
		return FALSE;
	}

	tracker_db_free_result (res);
	return TRUE;
}


static gboolean
is_delete_event (TrackerChangeAction event_type)
{
	return (event_type == TRACKER_ACTION_DELETE ||
		event_type == TRACKER_ACTION_DELETE_SELF ||
		event_type == TRACKER_ACTION_FILE_DELETED ||
		event_type == TRACKER_ACTION_DIRECTORY_DELETED);
}


static char *
str_get_after_prefix (const char *source,
		      const char *delimiter)
{
	char *prefix_start, *str;

	g_return_val_if_fail (source != NULL, NULL);

	if (delimiter == NULL) {
		return g_strdup (source);
	}

	prefix_start = strstr (source, delimiter);

	if (prefix_start == NULL) {
		return NULL;
	}

	str = prefix_start + strlen (delimiter);

	return g_strdup (str);
}

static void
process_event (const char *uri, gboolean is_dir, TrackerChangeAction action, guint32 cookie)
{
	FileInfo *info;

	g_return_if_fail (uri && (uri[0] == '/'));

	info = tracker_create_file_info (uri, action, 1, WATCH_OTHER);

	if (!tracker_file_info_is_valid (info)) {
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
		tracker_db_insert_pending_file (main_thread_db_con, info->file_id, info->uri, info->mime, 0, info->action, info->is_directory, TRUE, -1);
		info = tracker_free_file_info (info);
		return;

	} else if (action == TRACKER_ACTION_FILE_CREATED) {

		info = tracker_free_file_info (info);
		return;

	} else	if (action == TRACKER_ACTION_DIRECTORY_MOVED_FROM || action == TRACKER_ACTION_FILE_MOVED_FROM) {

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

		for (tmp = move_list; tmp; tmp = tmp->next) {
			FileInfo *moved_from_info;

			moved_from_info = (FileInfo *) tmp->data;

			if (!moved_from_info) {
				tracker_log ("bad FileInfo struct found in move list. Skipping...");
				continue;
			}

			if ((cookie > 0) && (moved_from_info->cookie == cookie)) {
				char *str_file_id, *name, *path;

				tracker_log ("found matching inotify pair for from %s to %s", moved_from_info->uri, moved_to_info->uri);
				move_list = g_slist_remove (move_list, tmp->data);

				moved_from_info = tracker_db_get_file_info (main_thread_db_con, moved_from_info);

				/* if orig file not in DB, treat it as a create action */
				if (moved_from_info->file_id == 0) {
					tracker_log ("warning original file %s not found in DB", moved_from_info->uri);
					break;
				}

				str_file_id = tracker_uint_to_str (moved_from_info->file_id);
				name = g_path_get_basename (moved_to_info->uri);
				path = g_path_get_dirname (moved_to_info->uri);

				/* update db so that fileID reflects new uri */
				tracker_db_update_file_move (main_thread_db_con, moved_from_info->file_id, path, name, moved_from_info->indextime);

				/* update File.Path and File.Filename metadata */
				tracker_db_set_metadata (main_thread_db_con, "Files", str_file_id, "File.Path", path, TRUE, TRUE, TRUE);
				tracker_db_set_metadata (main_thread_db_con, "Files", str_file_id, "File.Name", name, TRUE, TRUE, TRUE);

				g_free (str_file_id);
				g_free (name);
				g_free (path);

				if (tracker_is_directory (moved_to_info->uri)) {
					char *modified_path, *old_path, *match_path;
					char ***res;

					/* update all childs of the moved directory */
					modified_path = g_strconcat (moved_to_info->uri, G_DIR_SEPARATOR_S, NULL);
					old_path = g_strconcat (moved_from_info->uri, G_DIR_SEPARATOR_S, NULL);
					match_path = g_strconcat (old_path, "%", NULL);

					tracker_log ("moved file is a dir");

					/* stop watching old dir, start watching new dir */
					tracker_remove_watch_dir (moved_from_info->uri, TRUE, main_thread_db_con);
					tracker_remove_poll_dir (moved_from_info->uri);

					if (tracker_count_watch_dirs () < (int) tracker->watch_limit) {
						tracker_add_watch_dir (moved_to_info->uri, main_thread_db_con);
					} else {
						tracker_add_poll_dir (moved_to_info->uri);
					}

					/* update all changed File.Path metadata */
					tracker_exec_proc (main_thread_db_con, "UpdateFileMovePath", 2, moved_to_info->uri, moved_from_info->uri);


					/* for each subfolder, we must do the same as above */

					/* get all sub folders that were moved and add watches */
					res = tracker_db_get_file_subfolders (main_thread_db_con, moved_from_info->uri);

					if (res) {
						char **row;
						int  k;

						k = 0;

						while ((row = tracker_db_get_row (res, k))) {

							char *dir_name, *sep, *new_path;
							k++;

							if (!row || !row[0] || !row[1] || !row[2]) {
								continue;
							}

							dir_name = g_build_filename (row[1], row[2], NULL);

							sep = str_get_after_prefix (dir_name, old_path);

							if (!sep) {
								g_free (dir_name);
								continue;
							}

							new_path = g_build_filename (moved_to_info->uri, sep, NULL);
							g_free (sep);

							tracker_log ("moving subfolder %s to %s", dir_name, new_path);

							/* update all changed File.Path metadata for all files in this subfolder*/
							tracker_exec_proc (main_thread_db_con, "UpdateFileMovePath", 2, new_path, dir_name);

							/* update all subfolders and contained files to new path */
							tracker_exec_proc (main_thread_db_con, "UpdateFileMoveChild", 2, new_path, dir_name);

							if (tracker_count_watch_dirs () < (int) tracker->watch_limit) {
								tracker_add_watch_dir (new_path, main_thread_db_con);
							} else {
								tracker_add_poll_dir (new_path);
							}
							g_free (new_path);
							g_free (dir_name);

						}

						tracker_db_free_result (res);
					}

					/* update uri path of all files in moved folder */
					tracker_exec_proc (main_thread_db_con, "UpdateFileMoveChild", 2, moved_to_info->uri, moved_from_info->uri);

					g_free (modified_path);
					g_free (old_path);
					g_free (match_path);
				}

				moved_from_info = tracker_free_file_info (moved_from_info);
				moved_to_info = tracker_free_file_info (moved_to_info);
				info = NULL;
				return;
			}
		}

		/* matching pair not found so treat as a create action */
		tracker_log ("no matching pair found for inotify move event for %s", info->uri);
		if (tracker_is_directory (info->uri)) {
			info->action = TRACKER_ACTION_DIRECTORY_CREATED;
		} else {
			info->action = TRACKER_ACTION_FILE_CREATED;
		}
		tracker_db_insert_pending_file (main_thread_db_con, info->file_id, info->uri, info->mime, 1, info->action, info->is_directory, TRUE, -1);
		info = tracker_free_file_info (info);
		return;

	} else if (action == TRACKER_ACTION_WRITABLE_FILE_CLOSED) {

		tracker_log ("File %s has finished changing", info->uri);
		tracker_db_insert_pending_file (main_thread_db_con, info->file_id, info->uri, info->mime, 0, info->action, info->is_directory, TRUE, -1);
		info = tracker_free_file_info (info);
		return;

	}

	tracker_log ("not processing event %s for uri %s", tracker_actions[info->action], info->uri);
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
		TrackerChangeAction  action_type;
		char		     *str, *filename, *monitor_name, *str_wd;
		char		     *file_utf8_uri, *dir_utf8_uri;
		guint		     cookie;
		char		     ***res;

		struct inotify_event *event;

		if (!tracker->is_running) {
			return FALSE;
		}

		event = g_queue_pop_head (inotify_queue);

		if (!event) {
			continue;
		}

		action_type = get_event (event->mask);

		if (event->len > 1) {
			filename = event->name;
		} else {
			filename = NULL;
		}

		cookie = event->cookie;

		/* get watch name as monitor */

		str_wd = g_strdup_printf ("%d", event->wd);

		res = tracker_exec_proc (main_thread_db_con, "GetWatchUri", 1, str_wd);

		g_free (str_wd);

		if (res) {
			char **row;

			row = tracker_db_get_row (res, 0);

			if (row && row[0]) {
				monitor_name = g_strdup (row[0]);
			} else {
				monitor_name = NULL;
				g_free (event);
				continue;
			}

			tracker_db_free_result (res);
		} else {
			g_free (event);
			continue;
		}

		if (action_type == TRACKER_ACTION_IGNORE) {
			g_free (event);
			//tracker_log ("inotify event has no action");
			continue;
		}

		if ( !filename || strlen(filename) == 0) {
			//tracker_log ("inotify event has no filename");
			g_free (event);
			continue;
		}

		file_utf8_uri = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);

		if (!file_utf8_uri || strlen(file_utf8_uri) == 0) {
			tracker_log ("******ERROR**** file uri could not be converted to utf8 format");
			g_free (event);
			continue;
		}

		if (file_utf8_uri[0] == G_DIR_SEPARATOR) {
			str = g_strdup (file_utf8_uri);
			dir_utf8_uri = NULL;
		} else {

			dir_utf8_uri = g_filename_to_utf8 (monitor_name, -1, NULL, NULL, NULL);

			if (!dir_utf8_uri) {
				tracker_log ("******ERROR**** file uri could not be converted to utf8 format");
				g_free (file_utf8_uri);
				g_free (event);
				continue;
			}

			str = g_build_filename(dir_utf8_uri, file_utf8_uri, NULL);
		}


		if (!tracker_ignore_file (file_utf8_uri)) {
			process_event (str, tracker_is_directory (file_utf8_uri), action_type, cookie);

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
		tracker_log ("inotify system failure - unable to watch files");
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

	g_return_val_if_fail (dir != NULL && dir[0] == G_DIR_SEPARATOR, FALSE);

	if (!tracker->is_running) {
		return FALSE;
	}

	if (tracker_count_watch_dirs () >= (int) tracker->watch_limit) {
		tracker_log ("Inotify Watch Limit has been exceeded - unable to watch any more directories");
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
			tracker_log ("Inotify watch on %s has failed", dir);
			return FALSE;
		}

		str_wd = g_strdup_printf ("%d", wd);
		tracker_exec_proc (db_con, "InsertWatch", 2, dir, str_wd);
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
	char ***res;
	int	    wd;
	char  **row;

	g_return_val_if_fail (dir != NULL && dir[0] == G_DIR_SEPARATOR, FALSE);

	res = tracker_exec_proc (db_con, "GetWatchID", 1, dir);

	wd = -1;

	if (!res) {
		tracker_log ("WARNING : watch id not found for uri %s", dir);
		return FALSE;
	}

	row = tracker_db_get_row (res, 0);

	if (!row || !row[0]) {
		tracker_log ("WARNING : watch id not found for uri %s", dir);
		return FALSE;
	}

	wd = atoi (row[0]);

	tracker_db_free_result (res);

	tracker_exec_proc (db_con, "DeleteWatch", 1, dir);

	if (wd > -1) {
		inotify_rm_watch (inotify_monitor_fd, wd);
		inotify_count--;
	}

	return TRUE;
}


void
tracker_remove_watch_dir (const char *dir, gboolean delete_subdirs, DBConnection *db_con)
{
	char ***res;
	char **row;
	int  k;
	int  wd;

	g_return_if_fail (dir != NULL && dir[0] == G_DIR_SEPARATOR);

	delete_watch (dir, db_con);

	if (!delete_subdirs) {
		return;
	}

	res = tracker_db_get_sub_watches (db_con, dir);

	wd = -1;

	if (!res) {
		return;
	}

	for (k = 0; (row = tracker_db_get_row (res, k)) && row[0]; k++) {

		wd = atoi (row[0]);

		if (wd < 0) {
			continue;
		}

		inotify_rm_watch (inotify_monitor_fd, wd);
		inotify_count--;
	}

	tracker_db_free_result (res);

	tracker_db_delete_sub_watches (db_con, dir);
}


void
tracker_end_watching (void)
{
	if (gio) {
		g_io_channel_shutdown (gio, TRUE, NULL);
	}
}
