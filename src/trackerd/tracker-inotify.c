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

#include <glib.h> 
#include <glib/gstdio.h>
#include <sys/types.h>
#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <linux/inotify.h>

#include "linux-inotify-syscalls.h"
#include "tracker-inotify.h"


/* project wide global vars */
GAsyncQueue 	*file_pending_queue;
GAsyncQueue 	*file_process_queue;
DBConnection	*main_thread_db_con;

/* list to temporarily store moved_from events so they can be matched against moved_to events */
static	GSList 		*move_list;
static	GQueue 		*inotify_queue;
static int inotify_monitor_fd = -1;
static int inotify_count = 0;

gboolean 
tracker_is_directory_watched (const char * dir, DBConnection *db_con)
{
	
	MYSQL_RES *res = NULL;
	MYSQL_ROW  row;

	if (!dir || strlen (dir) == 0 || dir[0] != '/') {
		return FALSE;
	}

	res = tracker_exec_proc  (db_con->db, "GetWatchID", 1, dir);	

	if (res) {

		row = mysql_fetch_row (res);

		if (row && row[0]) {
			if (atoi (row[0]) > -1)  {
				return TRUE;
			}
		}

		mysql_free_result (res);
	}
	


	return FALSE;

}


static gboolean
is_delete_event (TrackerChangeAction event_type)
{
	
	return  (event_type == TRACKER_ACTION_DELETE || event_type == TRACKER_ACTION_DELETE_SELF ||event_type == TRACKER_ACTION_FILE_DELETED ||event_type == TRACKER_ACTION_DIRECTORY_DELETED );

}

static char *
str_get_after_prefix (const char *source,
		       const char *delimiter)
{
	char *prefix_start,  *str;
	
	if (source == NULL) {
		return NULL;
	}
	
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
	FileInfo *info, *moved_from_info, *moved_to_info;

	g_return_if_fail (uri);
	
	info = tracker_create_file_info (uri, action, 1, WATCH_OTHER);

	if (tracker_file_info_is_valid (info)) {
		info->is_directory = is_dir;	

		if (is_delete_event (action)) {
			
			char *parent = g_path_get_dirname (info->uri);
			
			if (tracker_file_is_valid (parent)) {
				g_async_queue_push (file_process_queue, info);	
			} else {
				info = tracker_free_file_info (info);
			}
			
			if (parent) {
				g_free (parent);
			}

			return;
		}

		/* we are not interested in create events for non-folders (we use writable file closed instead) */
		if (action == TRACKER_ACTION_DIRECTORY_CREATED) {
			info->action = TRACKER_ACTION_DIRECTORY_CREATED;
			info->is_directory = TRUE;
			tracker_db_insert_pending_file 	(main_thread_db_con, info->file_id, info->uri, info->mime, 0, info->action, info->is_directory);
			info = tracker_free_file_info (info);
			return;
			
		
		} else if (action == TRACKER_ACTION_FILE_CREATED) {
			info = tracker_free_file_info (info);
			return;

		} else	if (action == TRACKER_ACTION_DIRECTORY_MOVED_FROM || action == TRACKER_ACTION_FILE_MOVED_FROM) {
				info->cookie = cookie;
				info->counter = 1;
				move_list = g_slist_prepend (move_list, info);
				return;

		} else if (action == TRACKER_ACTION_FILE_MOVED_TO || action == TRACKER_ACTION_DIRECTORY_MOVED_TO) {

			GSList *tmp;
			
			tmp = move_list;
			moved_to_info = info;
			while (tmp) {
		
				moved_from_info = (FileInfo *)tmp->data;
				if (!moved_from_info) {
					tracker_log ("bad FileInfo struct found in move list. Skipping...");
					continue;
				}

				if ((cookie > 0) && (moved_from_info->cookie == cookie)) {

					tracker_log ("found matching inotify pair for from %s to %s", moved_from_info->uri, moved_to_info->uri);
					move_list = g_slist_remove  (move_list, tmp->data);
					
					moved_from_info = tracker_db_get_file_info (main_thread_db_con, moved_from_info);



					/* if orig file not in DB, treat it as a create action */
					if (moved_from_info->file_id == -1) {
						tracker_log ("warning original file %s not found in DB", moved_from_info->uri);
						break;
					}

						
					char *str_file_id = g_strdup_printf ("%ld", moved_from_info->file_id);
					char *index_time = g_strdup_printf ("%ld", moved_from_info->indextime);
					char *name = g_path_get_basename (moved_to_info->uri);
					char *path = g_path_get_dirname (moved_to_info->uri);

					

					
					/* update db so that fileID reflects new uri */
					tracker_exec_proc  (main_thread_db_con->db, "UpdateFileMove", 4, str_file_id, path, name, index_time);
					
					/* update File.Path and File.Filename metadata */
					tracker_exec_proc  (main_thread_db_con->db, "SetMetadata", 5, "Files", str_file_id, "File.Path", path, "1");
					tracker_exec_proc  (main_thread_db_con->db, "SetMetadata", 5, "Files", str_file_id, "File.Name", name, "1");
	
					g_free (str_file_id);
					g_free (index_time);
					g_free (name);
					g_free (path);	

					if (tracker_is_directory (moved_to_info->uri)) {
						
						/* update all childs of the moved directory */
						char *modified_path = g_strconcat (moved_to_info->uri, "/", NULL);
						char *old_path = g_strconcat (moved_from_info->uri, "/", NULL);
						char *match_path = g_strconcat (old_path, "%", NULL);
						
						MYSQL_RES *res = NULL;
						MYSQL_ROW  row;

						tracker_log ("moved file is a dir");

						/* stop watching old dir, start watching new dir */
						tracker_remove_watch_dir (moved_from_info->uri, TRUE, main_thread_db_con);
						tracker_remove_poll_dir (moved_from_info->uri);
				
						if (tracker_count_watch_dirs () < MAX_FILE_WATCHES) { 
							tracker_add_watch_dir (moved_to_info->uri, main_thread_db_con); 
						} else {
							tracker_add_poll_dir (moved_to_info->uri);
						}


						/* update all changed File.Path metadata */
						tracker_exec_proc  (main_thread_db_con->db, "UpdateFileMovePath", 2, moved_to_info->uri, moved_from_info->uri);


						/* for each subfolder, we must do the same as above */						

						/* get all sub folders that were moved and add watches */
						res = tracker_exec_proc  (main_thread_db_con->db, "SelectFileSubFolders", 1, moved_from_info->uri);

						if (res) {

							while ((row = mysql_fetch_row (res))) {

								
								if (row  && row[0] && row[1] && row[2]) {

									char *dir_name = g_strconcat (row[1], "/", row[2], NULL);
							
									char *sep = str_get_after_prefix (dir_name, old_path);
      									
									if (sep) {
										char *new_path = g_strconcat (moved_to_info->uri, "/", sep, NULL);
										g_free (sep);

										tracker_log ("moving subfolder %s to %s", dir_name, new_path);

										/* update all changed File.Path metadata for all files in this subfolder*/
										tracker_exec_proc  (main_thread_db_con->db, "UpdateFileMovePath", 2, new_path, dir_name);

										/* update all subfolders and contained files to new path */
										tracker_exec_proc  (main_thread_db_con->db, "UpdateFileMoveChild", 2, new_path, dir_name);

										
										if (tracker_count_watch_dirs () < MAX_FILE_WATCHES) { 
											tracker_add_watch_dir (new_path, main_thread_db_con); 
										} else {
											tracker_add_poll_dir (new_path);
										}
										g_free (new_path);
										g_free (dir_name);
											
									}
								} 
							}

							mysql_free_result (res);

						}	


						/* update uri path of all files in moved folder */		
						tracker_exec_proc  (main_thread_db_con->db, "UpdateFileMoveChild", 2, moved_to_info->uri, moved_from_info->uri);
							
						g_free (modified_path);
						g_free (old_path);
						g_free (match_path);
				

					}
						
					moved_from_info = tracker_free_file_info (moved_from_info); 
					moved_to_info = tracker_free_file_info (moved_to_info);
					info = NULL;
					return;
				}		
				tmp = tmp->next;
			}
			
			/* matching pair not found so treat as a create action */
			tracker_log ("no matching pair found");
			if (tracker_is_directory (info->uri)) {
				info->action = TRACKER_ACTION_DIRECTORY_CREATED;
			} else {
				info->action = TRACKER_ACTION_FILE_CREATED;
			}
			tracker_db_insert_pending_file 	(main_thread_db_con, info->file_id, info->uri, info->mime, 0, info->action, info->is_directory);
			info = tracker_free_file_info (info);
			return;

		} else if (action == TRACKER_ACTION_WRITABLE_FILE_CLOSED) {
			tracker_log ("File %s has finished changing", info->uri);
			tracker_db_insert_pending_file 	(main_thread_db_con, info->file_id, info->uri, info->mime, 0, info->action, info->is_directory);
			info = tracker_free_file_info (info);
			return;
		
		}
		tracker_log ("not processing event %s for uri %s", tracker_actions[info->action], info->uri);	
		info = tracker_free_file_info (info);
			
	}
}


static gboolean
process_moved_events ()
{
	FileInfo *info;
	const GSList *tmp;

	if (!move_list) {
		return TRUE;
	}

	
	tmp = move_list;

	while (tmp) {

		info = (FileInfo *)tmp->data;
		
		/* make it a DELETE if we have not received a corresponding MOVED_TO event after a certain period */
		if ((info->counter < 1) && ((info->action == TRACKER_ACTION_FILE_MOVED_FROM)|| (info->action == TRACKER_ACTION_DIRECTORY_MOVED_FROM))) {
			if (info->action == TRACKER_ACTION_DIRECTORY_MOVED_FROM) {
				process_event (info->uri, TRUE, TRACKER_ACTION_DIRECTORY_DELETED, 0);
			} else {
				process_event (info->uri, FALSE, TRACKER_ACTION_FILE_DELETED, 0);
			}
			tmp = tmp->next;
			move_list = g_slist_remove (move_list, info);
			info = tracker_free_file_info (info);
			continue;

		} else {
			info->counter--;
		}
		tmp = tmp->next;
	}

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



static void
free_inotify_event (struct inotify_event *event)
{
	g_return_if_fail (event);

	g_free (event);

}


static gboolean
process_inotify_events (void)
{
	
	while (g_queue_get_length (inotify_queue) > 0) {

		TrackerChangeAction action_type;
		char *str, *filename, *monitor_name;	
		char *file_utf8_uri = NULL;
		char *dir_utf8_uri = NULL;
		guint cookie;

		struct inotify_event *event;

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
		
		char *str_wd = g_strdup_printf ("%d", event->wd);

		MYSQL_RES *res = NULL;
		MYSQL_ROW  row;

		res = tracker_exec_proc (main_thread_db_con->db, "GetWatchUri", 1, str_wd);	

		g_free (str_wd);

		if (res) {

			row = mysql_fetch_row (res);

			if (row && row[0]) {
				monitor_name = g_strdup (row[0]);
			} else {
				monitor_name = NULL;
				free_inotify_event (event);
				continue;
			}
			mysql_free_result (res);
		}
		


		if (action_type == TRACKER_ACTION_IGNORE) {
			free_inotify_event (event);
			//tracker_log ("inotify event has no action");
			continue;
		}

		if ( !filename || strlen(filename) == 0) {
			//tracker_log ("inotify event has no filename");
			free_inotify_event (event);
			continue;
		}

		file_utf8_uri = g_filename_to_utf8 (filename, -1, NULL,NULL,NULL);

		if (!file_utf8_uri || strlen(file_utf8_uri) == 0) {
			tracker_log ("******ERROR**** file uri could not be converted to utf8 format");
			free_inotify_event (event);
			continue;
		}

		if (file_utf8_uri[0] == '/') {
			str = g_strdup (file_utf8_uri);
		} else {
	
			dir_utf8_uri = g_filename_to_utf8 (monitor_name, -1, NULL,NULL,NULL);

			if (!dir_utf8_uri) {
				tracker_log ("******ERROR**** file uri could not be converted to utf8 format");
				g_free (file_utf8_uri);
				free_inotify_event (event);
				continue;
			}

			str = g_strconcat (dir_utf8_uri, "/", file_utf8_uri, NULL);
		}


		if (!tracker_ignore_file (file_utf8_uri)) {
			process_event (str, tracker_is_directory (file_utf8_uri) , action_type, cookie);
			
		}
		g_free (monitor_name);
		g_free (str);	
		g_free (file_utf8_uri);
	
		if (dir_utf8_uri) {
			g_free (dir_utf8_uri);	
		}

		free_inotify_event (event);
	}
	return FALSE;
}

static gboolean
inotify_watch_func( GIOChannel *source, GIOCondition condition, gpointer data )
{
	char buffer[16384];
	size_t buffer_i;
	struct inotify_event *pevent, *event;
	size_t r;
	size_t event_size;
	int fd, count = 0;

 	fd = g_io_channel_unix_get_fd (source);

	r = read (fd, buffer, 16384);

	if (r <= 0) {
		tracker_log ("inotify system failure - unable to watch files");
		return FALSE;
	}
 
	buffer_i = 0;
	
	while (buffer_i < (size_t)r) {
		/* Parse events and process them ! */
		pevent = (struct inotify_event *)&buffer[buffer_i];
		event_size = sizeof(struct inotify_event) + pevent->len;
		event = malloc(event_size);
		memmove(event, pevent, event_size);
		g_queue_push_tail (inotify_queue, event);
		buffer_i += event_size;
		count++;
	}

	g_idle_add ((GSourceFunc) process_inotify_events, NULL);
	return TRUE;
}

gboolean 
tracker_start_watching (void) 
{

	GIOChannel *gio;

 	if (inotify_monitor_fd != -1) {
		return FALSE;
	}

	inotify_monitor_fd = inotify_init ();

  	if (inotify_monitor_fd < 0) {
		return FALSE;
	}

	inotify_queue = g_queue_new ();

	gio = g_io_channel_unix_new (inotify_monitor_fd);
	g_io_add_watch (gio, G_IO_IN, inotify_watch_func, NULL);
	g_io_channel_set_flags (gio, G_IO_FLAG_NONBLOCK, NULL);


	/* periodically process unmatched moved_from events */
	g_timeout_add_full (G_PRIORITY_LOW, 
			    350,
		 	    (GSourceFunc) process_moved_events,	
			    NULL, NULL	
			   );

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
	guint32	mask = IN_CLOSE_WRITE | IN_MOVE	| IN_CREATE | IN_DELETE| IN_DELETE_SELF | IN_MOVE_SELF;
	int wd;
	char *str_wd;

	if (!dir || strlen (dir) == 0 || dir[0] != '/') {
		return FALSE;
	}

	if (tracker_count_watch_dirs () >= MAX_FILE_WATCHES) {
		tracker_log ("Intoify Watch Limit has been exceeded - unable to watch any more directories");
		return FALSE;	
	}

	/* check directory permissions are okay */
	if (access (dir, F_OK) == 0 && access (dir, R_OK) == 0) {

		wd = inotify_add_watch (inotify_monitor_fd, dir, mask);

	 	if (wd < 0) {
			tracker_log ("Inotify watch on %s has failed", dir);
			return FALSE;
		} else {
			str_wd = g_strdup_printf ("%d", wd);	
			tracker_exec_proc (db_con->db, "InsertWatch",  2, dir, str_wd);
			g_free (str_wd);
			inotify_count++;
			tracker_log ("Watching directory %s (total watches = %d)", dir, inotify_count);
			return TRUE;
		}
	}
	return FALSE;
}


static gboolean
delete_watch 	(const char *dir, DBConnection *db_con)
{
	MYSQL_RES *res = NULL;
	MYSQL_ROW  row;

	int wd = -1;

	if (!dir || strlen (dir) == 0 || dir[0] != '/') {
		return FALSE;
	}


	res = tracker_exec_proc  (db_con->db, "GetWatchID", 1,  dir);	

	gboolean found = FALSE;	

	if (res) {
		row = mysql_fetch_row (res);
		if (row && row[0]) {
			wd = atoi (row[0]);
			found = TRUE;
		} else {
			found = FALSE;
		}
		mysql_free_result (res);
	} else {
		found = FALSE;
	}	

	if (!found) {
		tracker_log ("WARNING : watch id not found for uri %s", dir);
		return FALSE;
	}
	
	tracker_exec_proc  (db_con->db, "DeleteWatch", 1,  dir);	

	if (wd > -1) {
		inotify_rm_watch (inotify_monitor_fd, wd);
		inotify_count--;
	}

	return TRUE;
	
}




void
tracker_remove_watch_dir (const char *dir, gboolean delete_subdirs, DBConnection *db_con) 
{
	if (!dir || strlen (dir) == 0 || dir[0] != '/') {
		return;
	}

	delete_watch (dir, db_con); 

	if (delete_subdirs) {

		int wd = -1;
		MYSQL_RES *res = NULL;
		MYSQL_ROW  row;	

		res = tracker_exec_proc  (db_con->db, "GetSubWatches", 1,  dir);		
	

		if (res) {

			while ((row = mysql_fetch_row (res))) {
				if (row  && row[0]) {
					wd = atoi (row[0]);
					if (wd > -1) {
						inotify_rm_watch (inotify_monitor_fd, wd);
						inotify_count--;
					}

				} 
			}

			mysql_free_result (res);
			tracker_exec_proc  (db_con->db, "DeleteSubWatches", 1,  dir);
		}	



	}

	


}

void
tracker_end_watching (void) 
{
}



