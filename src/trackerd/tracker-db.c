/* Tracker
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
#include <time.h>

#include "tracker-db.h"


extern Tracker *tracker;


typedef struct {
	DBConnection 	*db_con;
	char 		*file_id;
} DatabaseAction;


char *
tracker_db_get_id (DBConnection *db_con, const char *service, const char *uri)
{
	int service_id;

	service_id = tracker_str_in_array (service, serice_index_array);

	if ( service_id == -1) {
		return NULL;
	}

	if (tracker_str_in_array (service, file_service_array) != -1) {
		int id;

		id = tracker_db_get_file_id (db_con, uri);

		if (id != -1) {
			return g_strdup_printf ("%d", id);
		}
	}

	return NULL;
}


int
tracker_db_get_file_id (DBConnection *db_con, const char *uri)
{
	char *path, *name;
	char ***res;
	int  id;

	if (!db_con || !uri) {
		return -1;
	}

	if (uri[0] == G_DIR_SEPARATOR) {
		name = g_path_get_basename (uri);
		path = g_path_get_dirname (uri);
	} else {
		name = tracker_get_vfs_name (uri);
		path = tracker_get_vfs_path (uri);
	}

	res = tracker_exec_proc (db_con, "GetServiceID", 2, path, name);

	id = -1;

	if (res) {
		char **row;

		row = tracker_db_get_row (res, 0);

		if (row && row[0]) {
			id = atoi (row[0]);
		} else {
			id = -1;
		}

		if (id < 1) {
			id = -1;
		}

		tracker_db_free_result (res);
	}

	g_free (path);
	g_free (name);

	return id;
}


FileInfo *
tracker_db_get_file_info (DBConnection *db_con, FileInfo *info)
{
	char *path, *name;
	char *apath, *aname;
	char ***res;

	if (!db_con || !info || !tracker_file_info_is_valid (info)) {
		return info;
	}

	name = g_path_get_basename (info->uri);
	path = g_path_get_dirname (info->uri);

	apath = tracker_escape_string (db_con, path);
	aname = tracker_escape_string (db_con, name);

	g_free (aname);
	g_free (apath);

	res = tracker_exec_proc (db_con, "GetServiceID", 2, path, name);

	if (res) {
		char **row;

		row = tracker_db_get_row (res, 0);

		if (row && row[0]) {
			info->file_id = atol (row[0]);
		}

		if (row && row[1]) {
			info->indextime = atoi (row[1]);
		}

		if (row && row[2]) {
			info->is_directory = (strcmp (row[2], "1") == 0) ;
		}

		tracker_db_free_result (res);
	}

	g_free (name);
	g_free (path);

	return info;
}


static void
get_meta_table_data (gpointer key,
   		     gpointer value,
		     gpointer user_data)
{
	DatabaseAction *db_action;
	char	       *mtype, *mvalue, *avalue, *evalue;

	mtype = (char *) key;
	avalue = (char *) value;

	if (mtype == NULL || avalue == NULL) {
		return;
	}

	db_action = user_data;

	evalue = NULL;

	if (tracker_metadata_is_date (db_action->db_con, mtype)) {
		char *dvalue;

		dvalue = tracker_format_date (avalue);

		if (dvalue) {

			time_t time;

			time = tracker_str_to_date (dvalue);

			g_free (dvalue);

			if (time == -1) {
				return;
			} else {
				evalue = tracker_long_to_str (time);
			}

		} else {
			return;
		}

	} else {
		evalue = g_strdup (avalue);
	}

	mvalue = tracker_escape_string (db_action->db_con, evalue);

	if (evalue) {
		g_free (evalue);
	}

	tracker_db_set_metadata (db_action->db_con, "Files", db_action->file_id, mtype, mvalue, TRUE);

	if (mvalue) {
		g_free (mvalue);
	}
}


void
tracker_db_save_metadata (DBConnection *db_con, GHashTable *table, long file_id)
{
	DatabaseAction db_action;

	g_return_if_fail (file_id != -1 || table || db_con);

	db_action.db_con = db_con;

	db_action.file_id = g_strdup_printf ("%ld", file_id);

	if (table) {
		g_hash_table_foreach (table, get_meta_table_data, &db_action);
	}

	g_free (db_action.file_id);
}


void
tracker_db_save_thumbs (DBConnection *db_con, const char *small_thumb, const char *large_thumb, long file_id)
{
	char *str_file_id;

	str_file_id = g_strdup_printf ("%ld", file_id);

	g_return_if_fail (str_file_id);

	if (small_thumb) {
		char *small_thumb_file;

		small_thumb_file = tracker_escape_string (db_con, small_thumb);
		tracker_db_set_metadata (db_con, "Files", str_file_id, "File.SmallThumbnailPath", small_thumb_file, TRUE);
//		tracker_exec_proc (db_con, "SetMetadata", 5, "Files", str_file_id, "File.SmallThumbnailPath", small_thumb_file, "1");
		g_free (small_thumb_file);
	}

	if (large_thumb) {
		char *large_thumb_file;

		large_thumb_file = tracker_escape_string (db_con, large_thumb);
		tracker_db_set_metadata (db_con, "Files", str_file_id, "File.LargeThumbnailPath", large_thumb_file, TRUE);
//		tracker_exec_proc (db_con, "SetMetadata", 5, "Files", str_file_id, "File.LargeThumbnailPath", large_thumb_file, "1");
		g_free (large_thumb_file);
	}

	g_free (str_file_id);
}


char **
tracker_db_get_files_in_folder (DBConnection *db_con, const char *folder_uri)
{
	char **array;
	char ***res;

	g_return_val_if_fail (db_con && folder_uri && (strlen (folder_uri) > 0), NULL);

	res = tracker_exec_proc (db_con, "SelectFileChild", 1, folder_uri);

	if (res) {
		int row_count;

		row_count = tracker_get_row_count (res);

		if (row_count > 0) {
			char **row;
			int  i;

			array = g_new (char *, row_count + 1);

			i = 0;

			while ((row = tracker_db_get_row (res, i))) {

				if (row[1] && row[2]) {
					array[i] = g_build_filename (row[1], row[2], NULL);

				} else {
					array[i] = NULL;
				}
				i++;
			}

			array [row_count] = NULL;

		} else {
			array = g_new (char *, 1);
			array[0] = NULL;
		}

		tracker_db_free_result (res);

	} else {
		array = g_new (char *, 1);
		array[0] = NULL;
	}

	return array;
}




gboolean
tracker_metadata_is_date (DBConnection *db_con, const char *meta)
{
	FieldDef *def;
	gboolean res;

	def = tracker_db_get_field_def (db_con, meta);

	res = (def->type == DATA_DATE);

	tracker_db_free_field_def (def);

	return res;
}


FileInfo *
tracker_db_get_pending_file (DBConnection *db_con, const char *uri)
{
	FileInfo *info;
	char	 ***res;

	info = NULL;

	res = tracker_exec_proc (db_con, "SelectPendingByUri", 1, uri);

	if (res) {
		char **row;

		row = tracker_db_get_row (res, 0);

		if (row && row[0] &&  row[1] && row[2] && row[3] && row[4]) {
			info = tracker_create_file_info (uri, atoi (row[2]), 0, 0);
			info->mime = g_strdup (row[3]);
			info->is_directory = (strcmp (row[4], "0") == 0);
		}

		tracker_db_free_result (res);
	}

	return info;
}


static void
make_pending_file (DBConnection *db_con, long file_id, const char *uri, const char *mime, int counter, TrackerChangeAction action, gboolean is_directory)
{
	char *str_file_id, *str_action, *str_counter;

	g_return_if_fail (uri);

	str_file_id = g_strdup_printf ("%ld", file_id);
	str_action = g_strdup_printf ("%d", action);
	str_counter = g_strdup_printf ("%d", counter);

	if (!mime) {
		if (tracker->is_running) {
			if ( (counter > 0)
	  		  || ((action == TRACKER_ACTION_EXTRACT_METADATA) && (g_async_queue_length (tracker->file_metadata_queue) > 50 ))
	  		  || ((action != TRACKER_ACTION_EXTRACT_METADATA) && (g_async_queue_length (tracker->file_process_queue) > 500 )) ) {

				tracker_db_insert_pending (db_con, str_file_id, str_action, str_counter, uri, "unknown", is_directory);
			} else {
				FileInfo *info;

				info = tracker_create_file_info (uri, action, 0, WATCH_OTHER);

				info->is_directory = is_directory;
				info->mime = g_strdup ("unknown");

				if (action != TRACKER_ACTION_EXTRACT_METADATA) {
					g_async_queue_push (tracker->file_process_queue, info);
				} else {
					g_async_queue_push (tracker->file_metadata_queue, info);
				}
			}
		} else {
			return;
		}

	} else {
		if (tracker->is_running) {
			tracker_db_insert_pending (db_con, str_file_id, str_action, str_counter, uri, mime, is_directory);
		} else {
			return;
		}
	}

	//tracker_log ("inserting pending file for %s with action %s", uri, tracker_actions[action]);

	/* signal respective thread that data is available and awake it if its asleep */
	if (action == TRACKER_ACTION_EXTRACT_METADATA) {
		tracker_notify_meta_data_available ();
	} else {
		tracker_notify_file_data_available ();
	}

	g_free (str_file_id);
	g_free (str_action);
	g_free (str_counter);
}


void
tracker_db_update_pending_file (DBConnection *db_con, const char *uri, int counter, TrackerChangeAction action)
{
	char *str_counter;
	char *str_action;

	str_counter = g_strdup_printf ("%d", counter);
	str_action = g_strdup_printf ("%d", action);

	if (tracker->is_running) {
		tracker_db_update_pending (db_con, str_counter, str_action, uri);
	}

	g_free (str_counter);
	g_free (str_action);
}


void
tracker_db_insert_pending_file (DBConnection *db_con, long file_id, const char *uri, const char *mime, int counter, TrackerChangeAction action, gboolean is_directory)
{
	FileInfo *info;

	/* check if uri already has a pending action and update accordingly */
	info = tracker_db_get_pending_file (db_con, uri);

	if (info) {
		switch (action) {

		case TRACKER_ACTION_FILE_CHECK:

			/* update counter for any existing event in the file_scheduler */

			if ((info->action == TRACKER_ACTION_FILE_CHECK) ||
			    (info->action == TRACKER_ACTION_FILE_CREATED) ||
			    (info->action == TRACKER_ACTION_FILE_CHANGED)) {

				tracker_db_update_pending_file (db_con, uri, counter, action);
			}

			break;

		case TRACKER_ACTION_FILE_CHANGED:

			tracker_db_update_pending_file (db_con, uri, counter, action);

			break;

		case TRACKER_ACTION_WRITABLE_FILE_CLOSED:

			tracker_db_update_pending_file (db_con, uri, 0, action);

			break;

		case TRACKER_ACTION_FILE_DELETED:
		case TRACKER_ACTION_FILE_CREATED:
		case TRACKER_ACTION_DIRECTORY_DELETED:
		case TRACKER_ACTION_DIRECTORY_CREATED:

			/* overwrite any existing event in the file_scheduler */
			tracker_db_update_pending_file (db_con, uri, 0, action);

			break;

		case TRACKER_ACTION_EXTRACT_METADATA:

			/* we only want to continue extracting metadata if file is not being changed/deleted in any way */
			if (info->action == TRACKER_ACTION_FILE_CHECK) {
				tracker_db_update_pending_file (db_con, uri, 0, action);
			}

			break;

		default:
			break;
		}

		tracker_free_file_info (info);

	} else {
		make_pending_file (db_con, file_id, uri, mime, counter, action, is_directory);
	}
}


gboolean
tracker_is_valid_service (DBConnection *db_con, const char *service)
{
	char	 ***res;
	gboolean result;

	res = tracker_exec_proc (db_con, "ValidService", 1, service);

	if (res) {
		char **row;

		row = tracker_db_get_row (res, 0);

		if (row && row[0]) {
			if (strcmp (row[0], "1") == 0) {
				result = TRUE;
			}
		}

		tracker_db_free_result (res);
	}

	return result;
}


gboolean
tracker_db_index_id_exists (DBConnection *db_con, unsigned int id)
{
	char	 ***res;
	char	 *id_str;
	gboolean result;

	result = FALSE;

	id_str = tracker_int_to_str (id);

	res = tracker_exec_proc (db_con, "IndexIDExists", 1, id_str);

	g_free (id_str);

	if (res) {
		char **row;

		row = tracker_db_get_row (res, 0);

		if (row && row[0]) {
			if (strcmp (row[0], "1") == 0) {
				result = TRUE;
			}
		}

		tracker_db_free_result (res);
	}

	return result;
}
