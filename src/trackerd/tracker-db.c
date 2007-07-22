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

#include "tracker-db.h"
#include "tracker-email.h"
#include "tracker-metadata.h"


extern Tracker *tracker;

#define XMP_MIME_TYPE "application/rdf+xml"

typedef struct {
	DBConnection	*db_con;
	char		*file_id;
	char 		*service;
	GHashTable	*table;
} DatabaseAction;



gboolean
tracker_db_is_file_up_to_date (DBConnection *db_con, const char *uri, guint32 *id)
{
	char	*path, *name;
	char	***res;
	gint32	index_time;

	g_return_val_if_fail (db_con, FALSE);
	g_return_val_if_fail (uri, FALSE);
	g_return_val_if_fail (uri[0] != '/', FALSE);

	if (uri[0] == G_DIR_SEPARATOR) {
		name = g_path_get_basename (uri);
		path = g_path_get_dirname (uri);
	} else {
		name = tracker_get_vfs_name (uri);
		path = tracker_get_vfs_path (uri);
	}

	res = tracker_exec_proc (db_con, "GetServiceID", 2, path, name);

	g_free (path);
	g_free (name);

	index_time = 0;
	*id = 0;

	if (res) {
		char **row;

		row = tracker_db_get_row (res, 0);

		if (row && row[0]) {
			long long tmp_id;

			tmp_id = atoll (row[0]);

			if (tmp_id > G_MAXUINT32) {
				tracker_error ("ERROR: file id is too big (> G_MAXUINT32)! Is database corrupted?");
				tracker_db_free_result (res);
				return FALSE;

			} else {
				*id = (guint32) tmp_id;
			}
		}

		if (row && row[1]) {
			index_time = atoi (row[1]);
		}

		tracker_db_free_result (res);

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
	char	*path, *name;
	char	***res;
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

	res = tracker_exec_proc (db_con, "GetServiceID", 2, path, name);

	g_free (path);
	g_free (name);

	id = 0;

	if (res) {
		char **row;

		row = tracker_db_get_row (res, 0);

		if (row && row[0]) {
			id = atoi (row[0]);
		}

		tracker_db_free_result (res);
	}

	return id;
}


FileInfo *
tracker_db_get_file_info (DBConnection *db_con, FileInfo *info)
{
	char *path, *name;
	char *apath, *aname;
	char ***res;

	g_return_val_if_fail (db_con, info);
	g_return_val_if_fail (info, info);

	if (!tracker_file_info_is_valid (info)) {
		return info;
	}

	name = g_path_get_basename (info->uri);
	path = g_path_get_dirname (info->uri);

	apath = tracker_escape_string (path);
	aname = tracker_escape_string (name);

	g_free (aname);
	g_free (apath);

	res = tracker_exec_proc (db_con, "GetServiceID", 2, path, name);

	g_free (name);
	g_free (path);

	if (res) {
		char **row;

		row = tracker_db_get_row (res, 0);

		if (row && row[0]) {
			info->file_id = atol (row[0]);
			info->is_new = FALSE;
		}

		if (row && row[1]) {
			info->indextime = atoi (row[1]);
		}

		if (row && row[2]) {
			info->is_directory = (strcmp (row[2], "1") == 0) ;
		}

		if (row && row[3]) {
			info->service_type_id = atoi (row[3]);
		}


		tracker_db_free_result (res);
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
		char **array = tracker_list_to_array (value);

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
/* 		tracker_exec_proc (db_con, "SetMetadata", 5, "Files", str_file_id, "File.SmallThumbnailPath", small_thumb_file, "1"); */
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
	char **array;
	char ***res;

	g_return_val_if_fail (db_con, NULL);
	g_return_val_if_fail (folder_uri, NULL);
	g_return_val_if_fail (folder_uri[0] != '\0', NULL);

	res = tracker_exec_proc (db_con, "SelectFileChild", 1, folder_uri);

	if (res) {
		int row_count;

		row_count = tracker_get_row_count (res);

		if (row_count > 0) {
			char	**row;
			int	i;

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
	FileInfo *info;
	char	 ***res;

	info = NULL;

	res = tracker_exec_proc (db_con, "SelectPendingByUri", 1, uri);

	if (res) {
		char **row;

		row = tracker_db_get_row (res, 0);

		if (row && row[0] && row[1] && row[2] && row[3] && row[4] && row[5] && row[6] && row[7] && row[8]) {
			info = tracker_create_file_info (uri, atoi (row[2]), 0, 0);
			info->mime = g_strdup (row[3]);
			info->is_directory = (strcmp (row[4], "1") == 0);
			info->is_new = (strcmp (row[5], "1") == 0);
			info->extract_embedded = (strcmp (row[6], "1") == 0);
			info->extract_contents = (strcmp (row[7], "1") == 0);
			info->service_type_id = atoi (row[8]);
		}

		tracker_db_free_result (res);
	}

	return info;
}


static void
make_pending_file (DBConnection *db_con, guint32 file_id, const char *uri, const char *mime, int counter, TrackerChangeAction action, gboolean is_directory, gboolean is_new, int service_type_id)
{
	char *str_file_id, *str_action, *str_counter;

	g_return_if_fail (uri);
	g_return_if_fail (uri[0] == '/');

	str_file_id = tracker_uint_to_str ( file_id);
	str_action = tracker_int_to_str (action);
	str_counter = tracker_int_to_str (counter);

	if (tracker->is_running) {
		if ( (counter > 0)
		     || ((action == TRACKER_ACTION_EXTRACT_METADATA) && (g_async_queue_length (tracker->file_metadata_queue) > tracker->max_extract_queue_size))
		     || ((action != TRACKER_ACTION_EXTRACT_METADATA) && (g_async_queue_length (tracker->file_process_queue) > tracker->max_process_queue_size)) ) {

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

	g_return_if_fail (uri);
	g_return_if_fail (uri[0] == '/');

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
	g_return_if_fail (info->uri);
	g_return_if_fail (info->uri[0] == '/');

	i = g_async_queue_length (tracker->file_metadata_queue);

	if (i < tracker->max_extract_queue_size) {

		/* inc ref count to prevent it being deleted */
		info = tracker_inc_info_ref (info);

		g_async_queue_push (tracker->file_metadata_queue, info);

	} else {
		tracker_db_insert_pending_file (db_con, info->file_id, info->uri, info->mime, 0, TRACKER_ACTION_EXTRACT_METADATA, info->is_directory, info->is_new, info->service_type_id);
	}

	tracker_notify_meta_data_available ();
}


void
tracker_db_insert_pending_file (DBConnection *db_con, guint32 file_id, const char *uri, const char *mime, int counter, TrackerChangeAction action, gboolean is_directory, gboolean is_new, int service_type_id)
{
	FileInfo *info;

	g_return_if_fail (uri);
	g_return_if_fail (uri[0] == '/');

	/* if a check action then then discard if up to date
	if (action == TRACKER_ACTION_CHECK || action == TRACKER_ACTION_FILE_CHECK || action == TRACKER_ACTION_DIRECTORY_CHECK) {

		guint32 id;

		if (tracker_db_is_file_up_to_date (db_con, uri, &id)) {
			return;
		}

		if (file_id == 0) {
			file_id = id;
		}

	}
*/
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
		make_pending_file (db_con, file_id, uri, mime, counter, action, is_directory, is_new, service_type_id);
	}
}


gboolean
tracker_is_valid_service (DBConnection *db_con, const char *service)
{
	return tracker_get_id_for_service (service) != -1;
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

	info->service_type_id = tracker_get_id_for_service (service);

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

	str_file_id = tracker_uint_to_str (info->file_id);

	if (!info->is_new) {
		old_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	} else {
		old_table = NULL;
	}

	index_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

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

	if (get_thumbs && tracker->enable_thumbnails) {
		char *small_thumb_file = NULL;

		small_thumb_file = tracker_metadata_get_thumbnail (info->uri, info->mime, "normal");

		g_free (small_thumb_file);

	}


	if (!info->is_new) {
		/* get original text for the differential indexer */
		old_table = tracker_db_get_file_contents_words (db_con->blob, info->file_id, old_table);
	}


	if (get_full_text && tracker->enable_content_indexing) {
		char *file_as_text;

		file_as_text = tracker_metadata_get_text_file (info->uri, info->mime);

		if (file_as_text) {
			
			tracker_db_save_file_contents (db_con->index, db_con->blob, index_table, old_table, file_as_text, info);
					
			/* clear up if text contents are in a temp file */
			if (g_str_has_prefix (file_as_text, tracker->sys_tmp_root_dir)) {
				g_unlink (file_as_text);
			}

		} else {
			get_full_text = FALSE;
		}

	}

	if (attachment_service) {
		info->service_type_id = tracker_get_id_for_service (attachment_service);
	}

	/* save stuff to Db */
	

	
	if (!info->is_new) {
	
		/* update existing file entry */
		tracker_db_update_file (db_con, info);

		/* get original embedded metadata for the differential indexer */
		old_table = tracker_db_get_indexable_content_words (db_con, info->file_id, old_table, TRUE);

		/* delete any exisitng embedded metadata */
		tracker_exec_proc (db_con, "DeleteEmbeddedServiceMetadata1", 1, str_file_id);
		tracker_exec_proc (db_con, "DeleteEmbeddedServiceMetadata2", 1, str_file_id);
		tracker_exec_proc (db_con, "DeleteEmbeddedServiceMetadata3", 1, str_file_id);

	}

	if (meta_table && (g_hash_table_size (meta_table) > 0)) {
		tracker_db_start_transaction (db_con->index);
		tracker_db_save_metadata (db_con->index, meta_table, index_table, service, info->file_id, info->is_new);
		tracker_db_end_transaction (db_con->index);
	}

	



	/* update full text indexes */
	if (info->is_new) {
		tracker_db_update_indexes_for_new_service (info->file_id, info->service_type_id, index_table);
	} else {
		tracker_db_update_differential_index (old_table, index_table, str_file_id, info->service_type_id);
	}

	if (index_table) {
		g_hash_table_destroy (index_table);
	}

	if (old_table) {
		g_hash_table_destroy (old_table);
	}	


	/* check for backup user defined metadata */
	if (info->is_new) {

		char *name = tracker_get_vfs_name (info->uri);
		char *path = tracker_get_vfs_path (info->uri);

		char ***result_set = tracker_exec_proc (db_con->common, "GetBackupMetadata", 2, path, name); 

		g_free (name);
		g_free (path);

		if (result_set) {
			char **row;
			int  k;

			k = 0;

			while ((row = tracker_db_get_row (result_set, k))) {

				k++;

				if (row[0] && row[1]) {
					if (attachment_service) {
						tracker_db_set_single_metadata (db_con->index, attachment_service, str_file_id, row[0], row[1]);
					} else {
						tracker_db_set_single_metadata (db_con->index, service, str_file_id, row[0], row[1]);
					}
				}

			}
			tracker_db_free_result (result_set);			

		}
	}


	g_free (str_file_id);
}


static void
free_metadata_list (GSList *list) 
{
	g_slist_foreach (list, (GFunc) g_free, NULL);
	g_slist_free (list);

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
	char *services_with_metadata[] = {"Documents", "Music", "Videos", "Images", NULL};
	char *services_with_text[] = {"Documents", "Development", "Text", NULL};
	char *services_with_thumbs[] = {"Documents", "Images", "Videos", NULL};

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

		service_name = tracker_get_service_type_for_mime (info->mime);

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

		is_external_service = g_str_has_prefix (info->mime, "service/");
		is_file_indexable = (!info->is_directory && (strcmp (info->mime, "unknown") != 0) && (strcmp (info->mime, "symlink") != 0) && tracker_file_is_indexable (info->uri));

		service_has_metadata = (is_external_service ||
					(is_file_indexable && (tracker_str_in_array (service_name, services_with_metadata) != -1))) && !is_sidecar;
		service_has_fulltext = (is_external_service ||
					(is_file_indexable && (tracker_str_in_array (service_name, services_with_text) != -1))) && !is_sidecar;
		service_has_thumbs = (is_external_service ||
				      (is_file_indexable && (tracker_str_in_array (service_name, services_with_thumbs) != -1)));

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



