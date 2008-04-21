/* Tracker - indexer and metadata database engine
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2007, Jason Kivlighn (jkivlighn@gmail.com)
 * Copyright (C) 2007, Creative Commons (http://creativecommons.org) 
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

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <glib/gstdio.h>

#include <libtracker-common/tracker-log.h>
#include <libtracker-common/tracker-config.h>

#include "tracker-db.h"
#include "tracker-email.h"
#include "tracker-metadata.h"
#include "tracker-os-dependant.h"
#include "tracker-service-manager.h"
#include "tracker-process-files.h"

extern Tracker *tracker;

#define XMP_MIME_TYPE "application/rdf+xml"
#define STACK_SIZE 30
#define BLACK_LIST_SECONDS 3600
#define MAX_DURATION 180
#define MAX_CHANGE_TIMES 3

typedef struct {
	DBConnection	*db_con;
	char		*file_id;
	char 		*service;
	GHashTable	*table;
} DatabaseAction;

static void
free_metadata_list (GSList *list) 
{
	g_slist_foreach (list, (GFunc) g_free, NULL);
	g_slist_free (list);

}

gboolean
tracker_db_is_file_up_to_date (DBConnection *db_con, const char *uri, guint32 *id)
{
	TrackerDBResultSet *result_set;
	char	*path, *name;
	gint32	index_time;

	g_return_val_if_fail (db_con, FALSE);
	g_return_val_if_fail (tracker_check_uri (uri), FALSE);

	if (uri[0] == G_DIR_SEPARATOR) {
		name = g_path_get_basename (uri);
		path = g_path_get_dirname (uri);
	} else {
		name = tracker_get_vfs_name (uri);
		path = tracker_get_vfs_path (uri);
	}

	result_set = tracker_exec_proc (db_con, "GetServiceID", path, name, NULL);

	g_free (path);
	g_free (name);

	index_time = 0;
	*id = 0;

	if (result_set) {
		tracker_db_result_set_get (result_set,
					   0, id,
					   1, &index_time,
					   -1);

		g_object_unref (result_set);
	} else {
		return FALSE;
	}

	if (index_time < (gint32) tracker_get_file_mtime (uri)) {
		return FALSE;
	}

	return TRUE;
}


guint32
tracker_db_get_file_id (DBConnection *db_con, const char *uri)
{
	TrackerDBResultSet *result_set;
	char	*path, *name;
	guint32	id;

	g_return_val_if_fail (db_con, 0);
	g_return_val_if_fail (uri, 0);

	if (uri[0] == G_DIR_SEPARATOR) {
		name = g_path_get_basename (uri);
		path = g_path_get_dirname (uri);
	} else {
		name = tracker_get_vfs_name (uri);
		path = tracker_get_vfs_path (uri);
	}

	result_set = tracker_exec_proc (db_con->index, "GetServiceID", path, name, NULL);

	g_free (path);
	g_free (name);

	id = 0;

	if (result_set) {
		tracker_db_result_set_get (result_set, 0, &id, -1);
		g_object_unref (result_set);
	}

	return id;
}


FileInfo *
tracker_db_get_file_info (DBConnection *db_con, FileInfo *info)
{
	TrackerDBResultSet *result_set;
	gchar *path, *name;

	g_return_val_if_fail (db_con, info);
	g_return_val_if_fail (info, info);

	if (!tracker_process_files_is_file_info_valid (info)) {
		return NULL;
	}

	name = g_path_get_basename (info->uri);
	path = g_path_get_dirname (info->uri);

	//apath = tracker_escape_string (path);
	//aname = tracker_escape_string (name);

	result_set = tracker_exec_proc (db_con->index, "GetServiceID", path, name, NULL);

//	g_free (aname);
//	g_free (apath);

	g_free (name);
	g_free (path);

	if (result_set) {
		gint id, indextime, service_type_id;
		gboolean is_directory;

		tracker_db_result_set_get (result_set,
					   0, &id,
					   1, &indextime,
					   2, &is_directory,
					   3, &service_type_id,
					   -1);

		if (id > 0) {
			info->file_id = id;
			info->is_new = FALSE;
		}

		info->indextime = indextime;
		info->is_directory = is_directory;
		info->service_type_id = service_type_id;

		g_object_unref (result_set);
	}

	return info;
}



static void
add_embedded_keywords (DBConnection *db_con, const char *service, const char *file_id, const char *keyword_type, const char *keywords, GHashTable *table)
{
	char **array;

	if (!service) {
		return;
	}

	array = g_strsplit_set (keywords, "\t\n\v\f\r !\"#$%&'()*/<=>?[\\]^`{|}~+,.:;@\"[]", -1);

	tracker_db_insert_embedded_metadata (db_con, service, file_id, keyword_type, array, -1, table);

	g_strfreev (array);
}



static void
save_meta_table_data (gpointer mtype,
			 gpointer value,
			 gpointer user_data)
{
	DatabaseAction	*db_action;

	if (mtype == NULL || value == NULL) {
		return;
	}

	db_action = user_data;

	/* auto-tag keyword related metadata */
	if (tracker_db_metadata_is_child (db_action->db_con, mtype, "DC:Keywords")) {

		GSList *tmp;
		for (tmp = value; tmp; tmp = tmp->next) {

			if (tmp->data) {
				add_embedded_keywords (db_action->db_con, db_action->service, db_action->file_id, mtype, tmp->data, db_action->table);
			}
		}
		
	} else {
		gchar **array = tracker_gslist_to_string_list (value);

		tracker_db_insert_embedded_metadata (db_action->db_con, db_action->service, db_action->file_id, mtype, array, g_slist_length (value), db_action->table);

		g_strfreev (array);
	}

}


GHashTable *
tracker_db_save_metadata (DBConnection *db_con, GHashTable *table, GHashTable *index_table, const char *service, guint32 file_id, gboolean new_file)
{
	DatabaseAction db_action;

	g_return_val_if_fail (db_con, NULL);
	g_return_val_if_fail (table, NULL);

	if (file_id == 0) {
		return NULL;
	}

	db_action.db_con = db_con;
	db_action.table = index_table;
	db_action.file_id = tracker_uint_to_str (file_id);
	db_action.service = (char *) service;

	if (table) {
		g_hash_table_foreach (table, save_meta_table_data, &db_action);
	}

	g_free (db_action.file_id);

	return db_action.table;
}


void
tracker_db_save_thumbs (DBConnection *db_con, const char *small_thumb, const char *large_thumb, guint32 file_id)
{
	char *str_file_id;

	str_file_id = tracker_uint_to_str (file_id);

	g_return_if_fail (str_file_id);

	if (small_thumb) {
		char *small_thumb_file;

		small_thumb_file = tracker_escape_string (small_thumb);
/* 		tracker_db_set_metadata (db_con, "Files", str_file_id, "File.SmallThumbnailPath", small_thumb_file, TRUE, FALSE, TRUE); */
/* 		tracker_exec_proc (db_con, "SetMetadata", "Files", str_file_id, "File.SmallThumbnailPath", small_thumb_file, "1", NULL); */
		g_free (small_thumb_file);
	}

	if (large_thumb) {
		char *large_thumb_file;

		large_thumb_file = tracker_escape_string (large_thumb);
/* 		tracker_db_set_metadata (db_con, "Files", str_file_id, "File.LargeThumbnailPath", large_thumb_file, TRUE, FALSE, TRUE); */
		g_free (large_thumb_file);
	}

	g_free (str_file_id);
}


char **
tracker_db_get_files_in_folder (DBConnection *db_con, const char *folder_uri)
{
	TrackerDBResultSet *result_set;
	GPtrArray *array;

	g_return_val_if_fail (db_con, NULL);
	g_return_val_if_fail (db_con->index, NULL);
	g_return_val_if_fail (folder_uri, NULL);
	g_return_val_if_fail (folder_uri[0] != '\0', NULL);

	result_set = tracker_exec_proc (db_con->index, "SelectFileChild", folder_uri, NULL);
	array = g_ptr_array_new ();

	if (result_set) {
		gboolean valid = TRUE;
		gchar *name, *prefix;

		while (valid) {
			tracker_db_result_set_get (result_set,
						   1, &prefix,
						   2, &name,
						   -1);

			g_ptr_array_add (array, g_build_filename (prefix, name, NULL));

			g_free (prefix);
			g_free (name);
			valid = tracker_db_result_set_iter_next (result_set);
		}

		g_object_unref (result_set);
	}

	g_ptr_array_add (array, NULL);

	return (gchar **) g_ptr_array_free (array, FALSE);
}


gboolean
tracker_metadata_is_date (DBConnection *db_con, const char *meta)
{
	FieldDef *def;
	gboolean res;

	def = tracker_db_get_field_def (db_con, meta);

	if (!def) {
		tracker_error ("ERROR: failed to get info for metadata type %s", meta);
		return FALSE;
	}

	g_return_val_if_fail (def, FALSE);

	res = (def->type == DATA_DATE);

	tracker_db_free_field_def (def);

	return res;
}


FileInfo *
tracker_db_get_pending_file (DBConnection *db_con, const char *uri)
{
	TrackerDBResultSet *result_set;
	FileInfo *info;

	info = NULL;
	result_set = tracker_exec_proc (db_con->cache, "SelectPendingByUri", uri, NULL);

	if (result_set) {
		gboolean is_directory, is_new, extract_embedded, extract_contents;
		gint counter, service_type_id;
		gchar *mimetype;

		tracker_db_result_set_get (result_set,
					   2, &counter,
					   3, &mimetype,
					   4, &is_directory,
					   5, &is_new,
					   6, &extract_embedded,
					   7, &extract_contents,
					   8, &service_type_id,
					   -1);

		info = tracker_create_file_info (uri, counter, 0, 0);
		info->mime = mimetype;
		info->is_directory = is_directory;
		info->is_new = is_new;
		info->extract_embedded = extract_embedded;
		info->extract_contents = extract_contents;
		info->service_type_id = service_type_id;

		g_object_unref (result_set);
	}

	return info;
}


static void
make_pending_file (DBConnection *db_con, guint32 file_id, const char *uri, const char *moved_to_uri, const char *mime, int counter, TrackerChangeAction action, gboolean is_directory, gboolean is_new, int service_type_id)
{
	char *str_file_id, *str_action, *str_counter;

	g_return_if_fail (tracker_check_uri (uri));

	str_file_id = tracker_uint_to_str ( file_id);
	str_action = tracker_int_to_str (action);
	str_counter = tracker_int_to_str (counter);

	if (tracker->is_running) {

		gboolean move_file = (action == TRACKER_ACTION_FILE_MOVED_FROM || action == TRACKER_ACTION_DIRECTORY_MOVED_FROM);
			
		if (!move_file && ((counter > 0) || (g_async_queue_length (tracker->file_process_queue) > tracker->max_process_queue_size))) {

			/* tracker_log ("************ counter for pending file %s is %d ***********", uri, counter); */
			if (!mime) {
				tracker_db_insert_pending (db_con, str_file_id, str_action, str_counter, uri, "unknown", is_directory, is_new, service_type_id);
			} else {
				tracker_db_insert_pending (db_con, str_file_id, str_action, str_counter, uri, mime, is_directory, is_new, service_type_id);
			}

		} else {

			FileInfo *info;

			info = tracker_create_file_info (uri, action, 0, WATCH_OTHER);

			info->is_directory = is_directory;
			info->is_new = is_new;

			if (!mime) {
				info->mime = g_strdup ("unknown");
			} else {
				info->mime = g_strdup (mime);
			}

			if (action == TRACKER_ACTION_FILE_MOVED_FROM || action == TRACKER_ACTION_DIRECTORY_MOVED_FROM) {
				info->moved_to_uri = g_strdup (moved_to_uri);
			}

			if (action != TRACKER_ACTION_EXTRACT_METADATA) {
				g_async_queue_push (tracker->file_process_queue, info);
			} else {
				g_async_queue_push (tracker->file_metadata_queue, info);
			}
		}

	} else {
		g_free (str_file_id);
		g_free (str_action);
		g_free (str_counter);
		return;
	}

	/* tracker_log ("inserting pending file for %s with action %s", uri, tracker_actions[action]); */

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
	char *str_counter, *str_action;

	g_return_if_fail (tracker_check_uri (uri));

	str_counter = tracker_int_to_str (counter);
	str_action = tracker_int_to_str (action);

	if (tracker->is_running) {
		tracker_db_update_pending (db_con, str_counter, str_action, uri);
	}

	g_free (str_counter);
	g_free (str_action);
}


void
tracker_db_add_to_extract_queue (DBConnection *db_con, FileInfo *info)
{
	int i;

	g_return_if_fail (info);
	g_return_if_fail (tracker_check_uri (info->uri));

	i = g_async_queue_length (tracker->file_metadata_queue);

	if (i < tracker->max_extract_queue_size) {

		/* inc ref count to prevent it being deleted */
		info = tracker_inc_info_ref (info);

		g_async_queue_push (tracker->file_metadata_queue, info);

	} else {
		tracker_db_insert_pending_file (db_con, info->file_id, info->uri, NULL, info->mime, 0, TRACKER_ACTION_EXTRACT_METADATA, info->is_directory, info->is_new, info->service_type_id);
	}

	tracker_notify_meta_data_available ();
}

static void
refresh_file_change_queue (gpointer data, gpointer user_data)
{
	FileChange *change = (FileChange*)data;
	int *current = (int *)user_data;

	if ((*current - change->first_change_time) > MAX_DURATION) {
		g_queue_remove_all (tracker->file_change_queue, data);
		free_file_change (&change);
	}
}

static gint
uri_comp (gconstpointer a, gconstpointer b)
{
	FileChange *change = (FileChange *)a;
	char *valuea = change->uri;
	char *valueb = (char *)b;

	return strcmp (valuea, valueb);
}

static gint
file_change_sort_comp (gconstpointer a, gconstpointer b, gpointer user_data)
{
	FileChange *changea, *changeb;
	changea = (FileChange *)a;
	changeb = (FileChange *)b;

	if ((changea->num_of_change - changeb->num_of_change) == 0) {
		return changea->first_change_time - changeb->first_change_time;
	} else {
		return changea->num_of_change - changeb->num_of_change;
	}
}

static void
print_file_change_queue ()
{
	GList *head, *l;
	FileChange *change;
	gint count;

	head = g_queue_peek_head_link (tracker->file_change_queue);

	tracker_log ("File Change queue is:");
	count = 1;
	for (l = g_list_first (head); l != NULL; l = g_list_next (l)) {
		change = (FileChange*)l->data;
		tracker_info ("%d\t%s\t%d\t%d",
			 count++, change->uri,
			 change->first_change_time,
			 change->num_of_change);
	}
	
}

static void
index_blacklist_file (char *uri)
{
	FileInfo *info;

	info = tracker_create_file_info (uri, TRACKER_ACTION_FILE_CHECK, 0, WATCH_OTHER);

	info->is_directory = FALSE;
	
	info->is_new = FALSE;

	info->mime = g_strdup ("unknown");

	g_async_queue_push (tracker->file_process_queue, info);
	
	tracker_notify_file_data_available ();

}


static gboolean
index_black_list ()
{
        GSList *black_list;

        black_list = tracker_process_files_get_temp_black_list ();
	g_slist_foreach (black_list, 
                         (GFunc) index_blacklist_file, 
                         NULL);
        g_slist_free (black_list);

        tracker_process_files_set_temp_black_list (NULL);
	
	tracker->black_list_timer_active = FALSE;
	
	return FALSE;

}



static gboolean
check_uri_changed_frequently (const char *uri)
{
	GList *find;
	FileChange *change;
	time_t current;

	if (!tracker->file_change_queue) {
		/* init queue */
		tracker->file_change_queue = g_queue_new ();
	}

	current = time (NULL);

	/* remove items which are very old */
	g_queue_foreach (tracker->file_change_queue,
			 refresh_file_change_queue, &current);

	find = g_queue_find_custom (tracker->file_change_queue, uri, uri_comp);
	if (!find) {
		/* not found, add to in the queue */
				
		change = g_new0 (FileChange, 1);
		change->uri = g_strdup (uri);
		change->first_change_time = current;
		change->num_of_change = 1;
		if (g_queue_get_length (tracker->file_change_queue) == STACK_SIZE) {
			FileChange *tmp = (FileChange*) g_queue_pop_head (
						tracker->file_change_queue);
			free_file_change (&tmp);
		}
		g_queue_insert_sorted (tracker->file_change_queue, change,
					file_change_sort_comp, NULL);
		print_file_change_queue ();
		return FALSE;
	} else {
		change = (FileChange *) find->data;
		(change->num_of_change)++;
		g_queue_sort (tracker->file_change_queue,
			file_change_sort_comp, NULL);
		if (change->num_of_change < MAX_CHANGE_TIMES) {
			print_file_change_queue ();
			return FALSE;
		} else {
			print_file_change_queue ();
			
			/* add uri to blacklist */
			
			tracker_log ("blacklisting %s", change->uri);
			
                        tracker_process_files_append_temp_black_list (change->uri);
			
			if (!tracker->black_list_timer_active) {
				tracker->black_list_timer_active = TRUE;
				g_timeout_add_seconds (BLACK_LIST_SECONDS, (GSourceFunc) index_black_list, NULL);
			}
			
			g_queue_remove_all (tracker->file_change_queue, change);
			free_file_change (&change);
			
			return TRUE;
		}
	}

}

void
tracker_db_insert_pending_file (DBConnection *db_con, guint32 file_id, const char *uri, const char *moved_to_uri, const char *mime, int counter, TrackerChangeAction action, gboolean is_directory, gboolean is_new, int service_type_id)
{
	FileInfo *info;

	g_return_if_fail (tracker_check_uri (uri));

	/* check if uri changed too frequently */
	if (((action == TRACKER_ACTION_CHECK) ||
		(action == TRACKER_ACTION_FILE_CHECK) || (action == TRACKER_ACTION_WRITABLE_FILE_CLOSED)) &&
		check_uri_changed_frequently (uri)) {
		
		return;
	}
	
	
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
		make_pending_file (db_con, file_id, uri, moved_to_uri, mime, counter, action, is_directory, is_new, service_type_id);
	}
}


static void
restore_backup_data (gpointer mtype,
			 gpointer value,
			 gpointer user_data)
{
	DatabaseAction	*db_action;
	gchar          **string_list;

	if (mtype == NULL || value == NULL) {
		return;
	}

	db_action = user_data;

        string_list = tracker_gslist_to_string_list (value);
	tracker_log ("restoring keyword list with %d items", g_slist_length (value));

	tracker_db_set_metadata (db_action->db_con->index, db_action->service, db_action->file_id, mtype, string_list, g_slist_length (value), FALSE);

	g_strfreev (string_list);
}



void
tracker_db_index_service (DBConnection *db_con, FileInfo *info, const char *service, GHashTable *meta_table, const char *attachment_uri, const char *attachment_service,  gboolean get_embedded, gboolean get_full_text, gboolean get_thumbs)
{
	char		*str_file_id;
	const char	*uri;
	GHashTable	*index_table, *old_table;

	if (!service) {
		/* its an external service - TODO get external service name */
		if (service) {
			tracker_log ("External service %s not supported yet", service);
		} else {
			tracker_log ("External service not supported yet");
		}
		return;
	}

	if (!attachment_uri) {
		uri = info->uri;
	} else {
		uri = attachment_uri;
	}

	info->service_type_id = tracker_service_manager_get_id_for_service (service);

	if (info->service_type_id == -1) {
		tracker_log ("Service %s not supported yet", service);
		return;
	}

	if (info->mime == NULL) {
		info->mime = g_strdup("unknown");
	}

	if (info->is_new) {
		if (info->mime)
			tracker_info ("Indexing %s with service %s and mime %s (new)", uri, service, info->mime);
		else
			tracker_info ("Indexing %s with service %s (new)", uri, service);
	} else {
		if (info->mime)
			tracker_info ("Indexing %s with service %s and mime %s (existing)", uri, service, info->mime);
		else
			tracker_info ("Indexing %s with service %s (existing)", uri, service);
	}

	

	if (!info->is_new) {
		old_table = g_hash_table_new (g_str_hash, g_str_equal);
	} else {
		old_table = NULL;
	}

	index_table = g_hash_table_new (g_str_hash, g_str_equal);

	/* get embedded metadata filter */
	if (get_embedded && meta_table) {
		tracker_metadata_get_embedded (info->uri, info->mime, meta_table);
	}

	if (info->is_new) {

		char *old_uri = info->uri;
		info->uri = (char *) uri;

		if (attachment_service) {
			info->file_id = tracker_db_create_service (db_con->index, attachment_service, info);
		} else {
			info->file_id = tracker_db_create_service (db_con->index, service, info);
		}

		info->uri = old_uri;

		if (info->file_id == 0) {
			tracker_error ("ERROR: could not get file id for %s - unable to continue indexing this file", uri);
			return;
		}

		if (info->service_type_id == -1) {
			tracker_error ("ERROR: unknown service type for %s with service %s and mime %s", uri, service, info->mime);
		}
	}

	str_file_id = tracker_uint_to_str (info->file_id);

	if (get_thumbs && tracker_config_get_enable_thumbnails (tracker->config)) {
		char *small_thumb_file = NULL;

		small_thumb_file = tracker_metadata_get_thumbnail (info->uri, info->mime, "normal");

		g_free (small_thumb_file);

	}


	if (!info->is_new) {
		/* get original text for the differential indexer */
		old_table = tracker_db_get_file_contents_words (db_con->blob, info->file_id, old_table);
	}


	if (get_full_text && tracker_config_get_enable_content_indexing (tracker->config)) {
		char *file_as_text;

		file_as_text = tracker_metadata_get_text_file (info->uri, info->mime);

		if (file_as_text) {
			
			tracker_db_save_file_contents (db_con, index_table, old_table, file_as_text, info);
					
			/* clear up if text contents are in a temp file */
			if (g_str_has_prefix (file_as_text, tracker->sys_tmp_root_dir)) {
				g_unlink (file_as_text);
			}

			g_free (file_as_text);

		} else {
			get_full_text = FALSE;
		}

	}

	if (attachment_service) {
		info->service_type_id = tracker_service_manager_get_id_for_service (attachment_service);
	}

	/* save stuff to Db */
	

	
	if (!info->is_new) {
	
		/* update existing file entry */
		tracker_db_update_file (db_con, info);

		/* get original embedded metadata for the differential indexer */
		old_table = tracker_db_get_indexable_content_words (db_con, info->file_id, old_table, TRUE);

		/* delete any exisitng embedded metadata */
		tracker_exec_proc (db_con, "DeleteEmbeddedServiceMetadata1", str_file_id, NULL);
		tracker_exec_proc (db_con, "DeleteEmbeddedServiceMetadata2", str_file_id, NULL);
		tracker_exec_proc (db_con, "DeleteEmbeddedServiceMetadata3", str_file_id, NULL);

	}

	if (meta_table && (g_hash_table_size (meta_table) > 0)) {
		tracker_db_save_metadata (db_con->index, meta_table, index_table, service, info->file_id, info->is_new);
	}

	



	/* update full text indexes */
	if (info->is_new) {
		tracker_db_update_indexes_for_new_service (info->file_id, info->service_type_id, index_table);
	} else {
		tracker_db_update_differential_index (db_con, old_table, index_table, str_file_id, info->service_type_id);
	}

	tracker_word_table_free (index_table);
	tracker_word_table_free (old_table);


	/* check for backup user defined metadata */
	if (info->is_new) {
		TrackerDBResultSet *result_set;
		char *name = tracker_get_vfs_name (info->uri);
		char *path = tracker_get_vfs_path (info->uri);

		result_set = tracker_exec_proc (db_con->common, "GetBackupMetadata", path, name, NULL);

		if (result_set) {
			gboolean valid = TRUE;
			GHashTable *meta_table;
			DatabaseAction db_action;
			gchar *key, *value;

			meta_table = g_hash_table_new_full (g_str_hash, g_str_equal,
							    (GDestroyNotify) g_free,
							    (GDestroyNotify) free_metadata_list);

			while (valid) {
				tracker_db_result_set_get (result_set,
							   0, &key,
							   1, &value,
							   -1);

				tracker_log ("found backup metadata for %s\%s with key %s and value %s", path, name, key, value);
				tracker_add_metadata_to_table (meta_table, key, value);

				valid = tracker_db_result_set_iter_next (result_set);
			}

			g_object_unref (result_set);

			db_action.db_con = db_con;
			db_action.file_id = str_file_id;

			if (attachment_service) {
				db_action.service = (char *) attachment_service;
			} else {
				db_action.service = (char *) service;
			}

			g_hash_table_foreach (meta_table, restore_backup_data, &db_action);	

			g_hash_table_destroy (meta_table);
		}

		g_free (name);
		g_free (path);
	}


	g_free (str_file_id);
}




void
tracker_db_index_master_files (DBConnection *db_con, const gchar *dirname, const gchar *basename, const gchar *filename)
{
	GDir* dir = g_dir_open (dirname, 0, NULL);
	
	if (dir) {

		const gchar *curr_ext;
		const gchar *curr_filename;

		FileInfo *master_info;
		gchar *master_uri;

		while ((curr_filename = g_dir_read_name (dir)) != NULL) {
			curr_ext = strrchr (curr_filename, '.');
			if (!curr_ext) {
				curr_ext = &curr_filename[strlen (curr_filename)];
			}

			if (curr_ext+1 && strncmp (basename, curr_filename, curr_ext-curr_filename) == 0 &&
					strcmp (curr_ext+1, "xmp") != 0 &&
					!g_str_has_suffix (curr_ext+1, "~")) {

				tracker_debug ("master file, %s, about to be updated", curr_filename);

				master_uri = g_build_filename (dirname, curr_filename, NULL);
				master_info = tracker_create_file_info (master_uri, TRACKER_ACTION_FILE_CHANGED, 0, 0);
				master_info = tracker_db_get_file_info (db_con, master_info);
				g_free (master_uri);

				tracker_db_index_file (db_con, master_info, NULL, NULL);
			}
		}

		g_dir_close (dir);
	}
}


void
tracker_db_index_file (DBConnection *db_con, FileInfo *info, const char *attachment_uri, const char *attachment_service)
{

	GHashTable	*meta_table;
	const char	*ext;
	char		*filename, *dirname;
	char		*str_link_uri, *service_name;
	gboolean	is_file_indexable, service_has_metadata, is_external_service, service_has_fulltext, service_has_thumbs, is_sidecar;

	const char *uri;

	if (!attachment_uri) {
		uri = info->uri;
	} else {
		uri = attachment_uri;
	}

	if (info->mime) {
		g_free (info->mime);
	}

	if (info->is_directory) {
		service_name = g_strdup ("Folders");
		info->mime = g_strdup ("Folder");
		info->file_size = 0;
	} else {
		info->mime = tracker_get_mime_type (info->uri);

		if (!info->mime) {
			info->mime = g_strdup ("unknown");
		}

		tracker_info ("mime is %s for %s", info->mime, info->uri);

		service_name = tracker_service_manager_get_service_type_for_mime (info->mime);

	}

	if (info->is_link) {
		str_link_uri = g_build_filename (info->link_path, info->link_name, NULL);
	} else {
		str_link_uri = NULL;
	}

	if (!info->is_hidden) {

		meta_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) free_metadata_list);

		tracker_add_metadata_to_table  (meta_table, g_strdup ("File:NameDelimited"), g_strdup (uri));

		dirname = g_path_get_dirname (uri);
		filename = g_path_get_basename (uri);
		ext = strrchr (filename, '.');
		if (ext) {
			ext++;
			tracker_debug ("file extension is %s", ext);
			tracker_add_metadata_to_table  (meta_table, g_strdup ("File:Ext"), g_strdup (ext));
			is_sidecar = strcmp("xmp",ext) == 0;
		} else {
			is_sidecar = FALSE;
		}

		tracker_add_metadata_to_table  (meta_table, g_strdup ("File:Path"), g_strdup (dirname));
		tracker_add_metadata_to_table  (meta_table, g_strdup ("File:Name"), g_strdup (filename));

		if (str_link_uri) {
			tracker_add_metadata_to_table  (meta_table, g_strdup ("File:Link"), str_link_uri);
		} 

		tracker_add_metadata_to_table  (meta_table, g_strdup ("File:Mime"), g_strdup (info->mime));
		tracker_add_metadata_to_table  (meta_table, g_strdup ("File:Size"), tracker_uint_to_str (info->file_size));
		tracker_add_metadata_to_table  (meta_table, g_strdup ("File:Modified"), tracker_date_to_str (info->mtime));
		tracker_add_metadata_to_table  (meta_table, g_strdup ("File:Accessed"), tracker_date_to_str (info->atime));

                /* need to add special data for web history */
                if (attachment_service != NULL && strcmp(attachment_service,"WebHistory") == 0)  {
                	
                	gchar* meta_file = g_strconcat(dirname,"/.",filename,NULL);
                     
                     	FILE* fp = g_fopen(meta_file, "r");
                     
                     	if (fp != NULL) {
                          	char buf[512];
                          	
                          	fgets(buf,512,fp);  //get the first line, it is URL for this web history object
                          	tracker_debug("URL for this WebHistory is %s\n",buf);
                          	tracker_add_metadata_to_table  (meta_table, g_strdup ("Doc:URL"), g_strdup(buf));
                          	
                          	fgets(buf,512,fp);
                          	fgets(buf,512,fp);
                          	fgets(buf,512,fp);
                          	fgets(buf,512,fp);  // get the keywords for this file
                          	
                          	if (buf != NULL) {
                              		
                              		/* format like t:dc:keyword=xxx */
                              		gchar** keys = g_strsplit (buf,"=",0);
                              
                              		if (keys != NULL && strcmp(keys[0],"t:dc:keyword") == 0 && keys[1]) {

                                		char *doc_keyword = g_strdup (keys[1]);

	                        		tracker_debug("found keywords : %s\n",doc_keyword);
	                        	
                                  		tracker_add_metadata_to_table  (meta_table, g_strdup ("Doc:Keywords"), doc_keyword);
                                	}
                                
                                
                                	if (keys) g_strfreev(keys);
                                
                        	      	
                      		}

                        	fclose (fp);
               		}
                     	g_free (meta_file);
                }
                                


		is_external_service = g_str_has_prefix (info->mime, "service/");
		is_file_indexable = (!info->is_directory && 
                                     (strcmp (info->mime, "unknown") != 0) && 
                                     (strcmp (info->mime, "symlink") != 0) &&
                                     tracker_file_is_indexable (info->uri));

		service_has_metadata = 
                        (is_external_service ||
                         (is_file_indexable && 
                          tracker_service_manager_has_metadata (service_name))) &&
                        !is_sidecar;
		service_has_fulltext = 
                        (is_external_service ||
                         (is_file_indexable && 
                          tracker_service_manager_has_text (service_name))) && 
                        !is_sidecar;
		service_has_thumbs = 
                        (is_external_service ||
                         (is_file_indexable && 
                          tracker_service_manager_has_thumbnails (service_name)));

		#ifdef HAVE_EXEMPI
		if (!info->is_directory) {
			gchar *basename;

			if (ext) {
				basename = g_strndup (filename, (ext - filename -1));
			} else {
				basename = g_strdup (filename);
			}

			if (is_sidecar) {
				tracker_db_index_master_files (db_con, dirname, basename, filename);
			} else {
				gchar *sidecar_filename = g_strconcat (basename, ".xmp", NULL);
				gchar *sidecar_uri = g_build_filename (dirname, sidecar_filename, NULL);
	
				if (g_file_test (sidecar_uri, G_FILE_TEST_EXISTS)) {
					tracker_debug ("xmp sidecar found for %s", uri);
					tracker_metadata_get_embedded (sidecar_uri, XMP_MIME_TYPE, meta_table);
				}
	
				g_free (sidecar_filename);
				g_free (sidecar_uri);
			}
			g_free (basename);
		}
		#endif

 		tracker_debug ("file %s has fulltext %d with service %s", info->uri, service_has_fulltext, service_name); 
		tracker_db_index_service (db_con, info, service_name, meta_table, uri, attachment_service, service_has_metadata, service_has_fulltext, service_has_thumbs);

		g_hash_table_destroy (meta_table);

		g_free (filename);
		g_free (dirname);
	} else {
		tracker_db_index_service (db_con, info, service_name, NULL, uri, NULL, FALSE, FALSE, FALSE);

	}

	g_free (service_name);

	if (attachment_uri ) {
		tracker_unlink (info->uri);
	}

	tracker_dec_info_ref (info);
}


void
tracker_db_index_conversation (DBConnection *db_con, FileInfo *info)
{
	/* to do use offsets */

	tracker_db_index_file (db_con, info, NULL, "GaimConversations");
}

void 
tracker_db_index_webhistory(DBConnection *db_con, FileInfo *info)
{
	tracker_db_index_file (db_con, info, NULL, "WebHistory");
}

