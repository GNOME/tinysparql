/* Tracker - FAM interface
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
#include <fam.h>

//#include "tracker-global.h"
#include "tracker-fam.h"


GAsyncQueue 	*file_pending_queue;
GAsyncQueue 	*file_process_queue;
DBConnection	*main_thread_db_con;

typedef struct {
	char 	   *watch_directory;
	FAMRequest *fr;
	WatchTypes watch_type;
} DirWatch;

static gint fam_watch_id = 0;
static FAMConnection fc;

/* table to store all active directory watches  */
static GHashTable *watch_table;



gboolean 
tracker_is_directory_watched (const char * dir, DBConnection *db_con)
{
	DirWatch *watch;

	if (!dir) {
		return FALSE;
	}

	watch = g_hash_table_lookup (watch_table, dir);

	return (watch != NULL);
}


WatchTypes
tracker_get_directory_watch_type (const char * dir)
{
	DirWatch *watch;

	watch = g_hash_table_lookup (watch_table, dir);

	if (watch != NULL) {
		return watch->watch_type;
	}
	return WATCH_OTHER;

}

static void
free_watch_func (gpointer user_data) 
{
	
	DirWatch *fw = user_data;
	if (fw->fr) {
		g_free (fw->fr);
	}

	if (fw->watch_directory) {
		g_free (fw->watch_directory);
	}

	if (fw) {
		g_free (fw);
	}
}

void
tracker_end_watching (void) 
{

	if (&fc) {
		FAMClose (&fc);
	}

	if (fam_watch_id > 0) {
		g_source_remove (fam_watch_id);
		fam_watch_id = 0;
	}

	g_hash_table_destroy  (watch_table);
}

static gboolean
is_delete_event (TrackerChangeAction event_type)
{
	
	return  (event_type == TRACKER_ACTION_DELETE || event_type == TRACKER_ACTION_DELETE_SELF ||event_type == TRACKER_ACTION_FILE_DELETED ||event_type == TRACKER_ACTION_DIRECTORY_DELETED );

}



static gboolean
fam_callback (GIOChannel *source,
	      GIOCondition condition,
	      gpointer data)
{
	int counter = 1;

	while (&fc != NULL && FAMPending (&fc)) {

		FAMEvent ev;
		DirWatch *watch;
		TrackerChangeAction event_type;
		

		if (FAMNextEvent(&fc, &ev) != 1) {
			tracker_end_watching ();
			return FALSE;
		}

		watch = (DirWatch *) ev.userdata;
		event_type = TRACKER_ACTION_IGNORE;

		switch (ev.code) {

			case FAMChanged:
				event_type = TRACKER_ACTION_CHECK;
				counter = 1;
				break;
			case FAMDeleted:
				event_type = TRACKER_ACTION_DELETE;
				counter = 0;
				break;
			case FAMCreated:
				event_type = TRACKER_ACTION_CREATE;		
				counter =  1;
				break;

			case FAMStartExecuting:
			case FAMStopExecuting:
			case FAMAcknowledge:
			case FAMExists:
			case FAMEndExist:
			case FAMMoved:
				continue;	
				break;

		}

		if (event_type != TRACKER_ACTION_IGNORE) {
			char *file_uri, *file_utf8_uri = NULL;
			FileInfo *info;

			if ( !ev.filename || strlen(ev.filename) == 0) {
				continue;
			}

			if (ev.filename[0] == '/') {
				file_uri = g_strdup (ev.filename);
			} else {

				/* ignore hidden files	*/
				if ( ev.filename[0] == '.') {
					continue;
				} else {
					file_uri = g_build_filename (watch->watch_directory, ev.filename, NULL);
				}
			}

			file_utf8_uri = g_filename_to_utf8 (file_uri, -1, NULL,NULL,NULL);

			g_free (file_uri);

			if (!file_utf8_uri) {
				tracker_log ("******ERROR**** FAM file uri could not be converted to utf8 format");
				continue;
			}

			int i = strlen (file_utf8_uri);

			i--;
			
			/* ignore trash files */
			if (file_utf8_uri [i] == '~') {
				continue;
			}

			info = tracker_create_file_info (file_utf8_uri, event_type, counter, WATCH_OTHER);
			if (tracker_file_info_is_valid (info)) {
				
				/* process deletions immediately - schedule (make pending) all others */
				if (is_delete_event (event_type)) {
					char *parent = g_path_get_dirname (info->uri);
						
					if (tracker_is_directory_watched (parent, NULL) || tracker_is_directory_watched (info->uri, NULL)) {
						g_async_queue_push (file_process_queue, info);	
					} else {
						info = tracker_free_file_info (info);					
					}
				} else { 

					tracker_db_insert_pending_file 	(main_thread_db_con, info->file_id, info->uri, info->mime, info->counter, info->action, info->is_directory);
					info = tracker_free_file_info (info);
			//		g_async_queue_push (file_pending_queue, info);	
				}

				/*tracker_log ("FAM: %s with event %s (pending queue length: %d, process queue length: %d)", 
					     file_utf8_uri, tracker_actions[event_type],
					     g_async_queue_length (file_process_queue), g_async_queue_length (file_process_queue));	*/
			}

			g_free (file_utf8_uri);
		}
	}

	return TRUE;

}

gboolean 
tracker_start_watching (void) 
{

	GIOChannel *ioc;

	if (FAMOpen2 (&fc, "Tracker") != 0) {
		return FALSE;
	}

	ioc = g_io_channel_unix_new (FAMCONNECTION_GETFD (&fc));
	
	fam_watch_id = g_io_add_watch (ioc,
				       G_IO_IN | G_IO_HUP | G_IO_ERR,
				       fam_callback, &fc);
	g_io_channel_unref (ioc);

	watch_table =  g_hash_table_new_full (g_str_hash, g_str_equal, g_free,  free_watch_func);

	return TRUE;
}
 


gboolean
tracker_add_watch_dir (const char *dir, DBConnection *db_con) 
{
	
	DirWatch *fwatch;
	FAMRequest *fr;
	int rc;

	
	if (!tracker_file_is_valid (dir)) {
		return FALSE;
	}

	if (!tracker_is_directory (dir)) {
		return FALSE;
	}


	if (g_hash_table_size (watch_table) >= MAX_FILE_WATCHES) {
		tracker_log ("Watch Limit has been exceeded - unable to watch any more directories");
		return FALSE;	
	}


	/* check directory permissions are okay */
	if (access (dir, F_OK) == 0 && access (dir, R_OK) == 0) {
		fwatch = g_new (DirWatch, 1);
		fr = g_new (FAMRequest, 1);
		fwatch->watch_directory = g_strdup (dir);
		fwatch->fr = fr;
		rc = -1;
		rc = FAMMonitorDirectory (&fc, dir, fr, fwatch);
	 	if (rc > 0) {
			tracker_log ("FAM watch on %s has failed", dir);
			free_watch_func (fwatch);
			return FALSE;
		} else {
			g_hash_table_insert (watch_table, g_strdup (dir), fwatch);
			tracker_log ("Watching directory %s (total watches = %d)", dir, g_hash_table_size (watch_table));
			return TRUE;
		}
	}
	return FALSE;
}

static gboolean
delete_watch 	(gpointer       key,
		 gpointer       value,
		 gpointer       user_data) 
{

	DirWatch *fwatch = value;
	const char *dir = user_data;
	char *dir_part;
		
	
	if (fwatch && dir) {
		dir_part = g_strconcat (dir, "/", NULL);
		/* need to delete subfolders of dir as well */ 
		if ((strcmp (fwatch->watch_directory, dir) == 0) || (g_str_has_prefix (fwatch->watch_directory, dir_part))) { 
			FAMCancelMonitor (&fc, fwatch->fr);
			g_free (dir_part);
			return TRUE;
		}
		g_free (dir_part);
	}
	return FALSE;
}


int	
tracker_count_watch_dirs (void)
{
	return g_hash_table_size (watch_table);
}


void
tracker_remove_watch_dir (const char *dir, gboolean delete_subdirs, DBConnection	*db_con) 
{
	char *str;

	str = g_strdup (dir);

	if (delete_subdirs) {
		g_hash_table_foreach_remove (watch_table, delete_watch, str);
	} else {
		DirWatch *fwatch = g_hash_table_lookup (watch_table, dir);
		FAMCancelMonitor (&fc, fwatch->fr);
		g_hash_table_remove (watch_table, dir);
		
	}
	g_free (str);
}




