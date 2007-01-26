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

#include <stdlib.h>
#include <string.h>
#include <time.h> 
#include <glib/gstdio.h>
#include "tracker-db.h"
#include "tracker-email.h"
#include "tracker-metadata.h"

extern Tracker *tracker;


typedef struct {
	DBConnection 	*db_con;
	char 		*file_id;
	GHashTable	*table;
} DatabaseAction;


char *
tracker_db_get_id (DBConnection *db_con, const char *service, const char *uri)
{
	int service_id;
	guint32 id;

	service_id = tracker_get_id_for_service (service);

	if ( service_id == -1) {
		return NULL;
	}
	
	id = tracker_db_get_file_id (db_con, uri);

	if (id != 0) {
		return tracker_uint_to_str (id);
	}

	return NULL;
}

gboolean
tracker_db_is_file_up_to_date (DBConnection *db_con, const char *uri, guint32 *id)
{
	char *path, *name;
	char ***res;
	gint32 index_time;

	if (!db_con || !uri || uri[0] != '/') {
		return FALSE;
	}

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
			*id = (guint32) atoll (row[0]);
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
	char *path, *name;
	char ***res;
	guint32  id;

	if (!db_con || !uri) {
		return 0;
	}

	if (uri[0] == G_DIR_SEPARATOR) {
		name = g_path_get_basename (uri);
		path = g_path_get_dirname (uri);
	} else {
		name = tracker_get_vfs_name (uri);
		path = tracker_get_vfs_path (uri);
	}

	res = tracker_exec_proc (db_con, "GetServiceID", 2, path, name);

	id = 0;

	if (res) {
		char **row;

		row = tracker_db_get_row (res, 0);

		if (row && row[0]) {
			id = atoi (row[0]);
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

	g_free (name);
	g_free (path);

	return info; 
}


static void
tracker_db_add_embedded_keywords (DBConnection *db_con, const char *file_id, const char *keyword_type, const char *keywords, gboolean is_new, GHashTable *table)
{
	char **array, **tags;
	char *tag;
	
	array = g_strsplit_set (keywords, "\t\n\v\f\r !\"#$%&'()*/<=>?[\\]^`{|}~+,.:;@\"[]", -1);

	if (!is_new) {
		tracker_db_update_index_multiple_metadata (db_con, "Files", file_id, keyword_type, array);
	}

	for (tags = array; *tags; ++tags) {

		tag = *tags;
		tag = g_strstrip (tag);

		if (strlen (tag) > 0) {
			if (tracker->verbosity > 0) {
				tracker_log ("Auto-tagging file with %s", tag);
			}
		}

		if (is_new) {
			tracker_db_insert_embedded_metadata (db_con, "Files", file_id, keyword_type, tag, table);
		} else {
			
			tracker_db_set_metadata (db_con, "Files", file_id, keyword_type, tag, FALSE, FALSE, TRUE);
		}
	}

	g_strfreev (array);

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

			//tracker_log ("processing date %s with format %s and time %ld", avalue, dvalue, time);

			g_free (dvalue);

			if (time == -1) {
				return;
			} else {
				evalue = tracker_int_to_str (time);
			//	tracker_log ("date is %s", evalue);
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

	

	/* auto-tag keyword related metadata */	
	if ( (strcasecmp (mtype, "Doc:Keywords") == 0) || (strcasecmp (mtype, "Image:Keywords") == 0) ) {
		tracker_db_add_embedded_keywords (db_action->db_con, db_action->file_id, mtype, mvalue, FALSE, NULL);
	} else {
		tracker_db_set_metadata (db_action->db_con, "Files", db_action->file_id, mtype, mvalue, FALSE, TRUE, TRUE);
	}


	if (mvalue) {
		g_free (mvalue);
	}
}

static void
get_meta_table_data_new (gpointer key,
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

			//tracker_log ("processing date %s with format %s and time %ld", avalue, dvalue, time);

			g_free (dvalue);

			if (time == -1) {
				return;
			} else {
				evalue = tracker_int_to_str (time);
			//	tracker_log ("date is %s", evalue);
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

	

	/* auto-tag keyword related metadata */	
	if ( (strcasecmp (mtype, "Doc:Keywords") == 0) || (strcasecmp (mtype, "Image:Keywords") == 0) ) {
		tracker_db_add_embedded_keywords (db_action->db_con, db_action->file_id, mtype, mvalue, TRUE, db_action->table);
	} else {
		tracker_db_insert_embedded_metadata (db_action->db_con, "Files", db_action->file_id, mtype, mvalue, db_action->table);
	}

	if (mvalue) {
		g_free (mvalue);
	}
}


GHashTable *
tracker_db_save_metadata (DBConnection *db_con, GHashTable *table, GHashTable *index_table, guint32 file_id, gboolean new_file)
{
	DatabaseAction db_action;
	
	if ((file_id == 0 || !table || !db_con)) {
		return NULL;
	}

	db_action.db_con = db_con;

	if (new_file && index_table) {
		db_action.table = index_table;
	} else {
		db_action.table = NULL;
	}

	db_action.file_id = tracker_uint_to_str (file_id);

	if (table) {
		
		if (new_file) {
			g_hash_table_foreach (table, get_meta_table_data_new, &db_action);
		} else {
			g_hash_table_foreach (table, get_meta_table_data, &db_action);
		}
		
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

		small_thumb_file = tracker_escape_string (db_con, small_thumb);
		//tracker_db_set_metadata (db_con, "Files", str_file_id, "File.SmallThumbnailPath", small_thumb_file, TRUE, FALSE, TRUE);
//		tracker_exec_proc (db_con, "SetMetadata", 5, "Files", str_file_id, "File.SmallThumbnailPath", small_thumb_file, "1");
		g_free (small_thumb_file);
	}

	if (large_thumb) {
		char *large_thumb_file;

		large_thumb_file = tracker_escape_string (db_con, large_thumb);
//		tracker_db_set_metadata (db_con, "Files", str_file_id, "File.LargeThumbnailPath", large_thumb_file, TRUE, FALSE, TRUE);
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

	if (!def) {
		tracker_log ("failed to get info for metadata type %s", meta);
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

	g_return_if_fail (uri && (uri[0] == '/'));

	str_file_id = tracker_uint_to_str ( file_id);
	str_action = tracker_int_to_str (action);
	str_counter = tracker_int_to_str (counter);

	if (tracker->is_running) {
		if ( (counter > 0)  
		  || ((action == TRACKER_ACTION_EXTRACT_METADATA) && (g_async_queue_length (tracker->file_metadata_queue) > tracker->max_extract_queue_size))
  		  || ((action != TRACKER_ACTION_EXTRACT_METADATA) && (g_async_queue_length (tracker->file_process_queue) > tracker->max_process_queue_size)) ) {

			//tracker_log ("************ counter for pending file %s is %d ***********", uri, counter);
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

	g_return_if_fail (uri && (uri[0] == '/'));

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

	g_return_if_fail (info && info->uri && (info->uri[0] == '/'));

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

	g_return_if_fail (uri && (uri[0] == '/'));

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

	if (tracker_get_id_for_service (service) != -1) {
		return TRUE;
	}

	return FALSE;
}



void
tracker_db_index_service (DBConnection *db_con, FileInfo *info, const char *service, GHashTable *meta_table, gboolean is_attachment, gboolean get_embedded, gboolean get_full_text, gboolean get_thumbs)
{
	char	*name, *path, *str_file_id;
	GHashTable *index_table;

	if (!service) {
		/* its an external service - TODO get external service name */
		tracker_log ("External service %s not supported yet", service);
		return;
	}


	info->service_type_id = tracker_get_id_for_service (service);
	if (info->service_type_id == -1) {
		tracker_log ("Service %s not supported yet", service);
		return;
	}


	if (tracker->verbosity > 0) {
		if (info->is_new) {
			tracker_log ("Indexing %s with service %s and mime %s (new)", info->uri, service, info->mime);
		} else {
			tracker_log ("Indexing %s with service %s and mime %s (existing)", info->uri, service, info->mime);
		}
	}

	str_file_id = tracker_uint_to_str (info->file_id);

	if (info->is_new) {
		index_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	} else {
		index_table = NULL;
	}



	/* get embedded metadata filter */
	if (get_embedded && meta_table) {	

		tracker_metadata_get_embedded (info->uri, info->mime, meta_table);
		/* to do - emit dbus signal here for EmbeddedMetadataChanged */
		
	} 

	if (info->is_new) {

		
		name = g_path_get_basename (info->uri);
		path = g_path_get_dirname (info->uri);

		if (is_attachment) {
			info->file_id = tracker_db_create_service (db_con, path, name, "EmailAttachments", info->mime, info->file_size, info->is_directory, info->is_link, info->offset, info->mtime, -1);
		} else {
			info->file_id = tracker_db_create_service (db_con, path, name, service, info->mime, info->file_size, info->is_directory, info->is_link, info->offset, info->mtime, -1);
		}

		g_free (name);
		g_free (path);

		if (info->file_id == 0) {
			tracker_log ("Error: Could not get file id for %s - unable to continue indexing this file", info->uri);
			return;
		}

		if (info->service_type_id == -1) {
			tracker_log ("******ERROR****** Unknown service type for %s with service %s and mime %s", info->uri, service, info->mime);
		}
	}

	if (get_thumbs && tracker->enable_thumbnails) {
		char *small_thumb_file = NULL, *large_thumb_file = NULL;

		small_thumb_file = tracker_metadata_get_thumbnail (info->uri, info->mime, "normal");

		if (small_thumb_file) {
			tracker_db_save_thumbs (db_con, small_thumb_file, large_thumb_file, info->file_id);
		}

		/* to do - emit dbus signal ThumbNailChanged */
		if (small_thumb_file) {
			g_free (small_thumb_file);
		}

	}

	if (get_full_text && tracker->enable_content_indexing) {
		char *file_as_text = tracker_metadata_get_text_file (info->uri, info->mime);

		if (file_as_text) {
			const char *tmp_dir;
			char	   *dir;

			tracker_db_save_file_contents (db_con, db_con->user_data2, index_table, file_as_text, info);
			
			tmp_dir = g_get_tmp_dir ();

			dir = g_strconcat (tmp_dir, G_DIR_SEPARATOR_S, NULL);

			/* clear up if text contents are in a temp file */
			if (g_str_has_prefix (file_as_text, dir)) {
				g_unlink (file_as_text);
			}

			g_free (dir);

			g_free (file_as_text);

		} else {
			get_full_text = FALSE;
		}

	}



	/* save stuff to Db */
	tracker_db_start_transaction (db_con);

	
	if (!info->is_new) {

		/* update existing file entry */
		tracker_db_update_file (db_con, info);

		/* mark metadata that needs to be deleted (IE all derived metadata in DB for an updated file) */
		tracker_exec_proc (db_con, "MarkEmbeddedServiceMetadata1", 1, str_file_id);
		tracker_exec_proc (db_con, "MarkEmbeddedServiceMetadata2", 1, str_file_id);
		tracker_exec_proc (db_con, "MarkEmbeddedServiceMetadata3", 1, str_file_id);
		tracker_exec_proc (db_con, "MarkEmbeddedServiceMetadata4", 1, str_file_id);
	}

	if (meta_table && (g_hash_table_size (meta_table) > 0)) {
		tracker_db_save_metadata (db_con, meta_table, index_table, info->file_id, info->is_new);
	}

	if (!info->is_new) {
		/* delete any old metadata that was not updated  */
		tracker_exec_proc (db_con, "DeleteEmbeddedServiceMetadata1", 1, str_file_id);
		tracker_exec_proc (db_con, "DeleteEmbeddedServiceMetadata2", 1, str_file_id);
		tracker_exec_proc (db_con, "DeleteEmbeddedServiceMetadata3", 1, str_file_id);
		tracker_exec_proc (db_con, "DeleteEmbeddedServiceMetadata4", 1, str_file_id);
	}

	/* update metadata display table */
	tracker_db_refresh_all_display_metadata (db_con, str_file_id);

	tracker_db_end_transaction (db_con);


	/* update full text indexes */
	if (info->is_new) {
		tracker_db_update_indexes_for_new_service (info->file_id, info->service_type_id, index_table);
	}

	if (index_table) {
		g_hash_table_destroy (index_table);
	}

	g_free (str_file_id);

}

void
tracker_db_index_file (DBConnection *db_con, FileInfo *info, gboolean is_attachment)
{
	GHashTable	*meta_table;
	const char 	*ext;
	char 		*str_link_uri, *service_name;

	char *services_with_metadata[] = {"Documents", "Music", "Videos", "Images", NULL};
	char *services_with_text[] = {"Documents", "Text Files", "Development Files", NULL};
	char *services_with_thumbs[] = {"Documents", "Images", "Videos", NULL};

	meta_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

	if (info->mime) {
		g_free (info->mime);
	}

	if (!info->is_directory) {
		info->mime = tracker_get_mime_type (info->uri);

		if (!info->mime) {
			info->mime = g_strdup ("unknown");
		}
	} else {
		info->mime = g_strdup ("Folder");
		info->file_size = 0;
	}

	if (info->is_link) {
		str_link_uri = g_build_filename (info->link_path, info->link_name, NULL);
	} else {
		str_link_uri = g_strdup (" ");
	}

	meta_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

	/* delimit file uri so hyphens and underscores are removed so that they can be indexed separately */
	if (strchr (info->uri, '_') || strchr (info->uri, '-')) {
		char	*delimited;

		delimited = g_strdup (info->uri);
		delimited =  g_strdelimit (delimited, "-_" , ' ');
		tracker_debug ("delimited file name is %s", delimited);
		g_hash_table_insert (meta_table, g_strdup ("File:NameDelimited"), g_strdup (delimited));
		g_free (delimited);
	}

	ext = strrchr (info->uri, '.');
	if (ext) {
		ext++;
		tracker_debug ("file extension is %s", ext);
		g_hash_table_insert (meta_table, g_strdup ("File:Ext"), g_strdup (ext));
	}

	g_hash_table_insert (meta_table, g_strdup ("File:Path"), g_path_get_dirname (info->uri));
	g_hash_table_insert (meta_table, g_strdup ("File:Name"), g_path_get_basename (info->uri));
	g_hash_table_insert (meta_table, g_strdup ("File:Link"), g_strdup (str_link_uri));
	g_hash_table_insert (meta_table, g_strdup ("File:Mime"), g_strdup (info->mime));
	g_hash_table_insert (meta_table, g_strdup ("File:Size"), tracker_uint_to_str (info->file_size));
	g_hash_table_insert (meta_table, g_strdup ("File:Permissions"), g_strdup (info->permissions));
	g_hash_table_insert (meta_table, g_strdup ("File:Modified"), tracker_date_to_str (info->mtime));
	g_hash_table_insert (meta_table, g_strdup ("File:Accessed"), tracker_date_to_str (info->atime));

	g_free (str_link_uri);

	if (info->is_directory) {
		service_name = g_strdup ("Folders");
	} else {
		service_name = tracker_get_service_type_for_mime (info->mime);
	}
			
	if (!info->mime) {
		info->mime = g_strdup ("unknown");
	}

	gboolean  is_file_indexable, service_has_metadata, is_external_service, service_has_fulltext, service_has_thumbs;

	is_external_service = g_str_has_prefix (info->mime, "service/");
	is_file_indexable = !info->is_directory && (strcmp (info->mime, "unknown") != 0) && (strcmp (info->mime, "symlink") != 0);
	
	service_has_metadata = is_external_service || (is_file_indexable && (tracker_str_in_array (service_name, services_with_metadata) != -1));
	service_has_fulltext = is_external_service || (is_file_indexable && (tracker_str_in_array (service_name, services_with_text) != -1));
	service_has_thumbs = is_external_service || (is_file_indexable && (tracker_str_in_array (service_name, services_with_thumbs) != -1));

	
//	tracker_log ("file %s has fulltext %d, %d, %d", info->uri, (tracker_str_in_array (service_name, services_with_text) != -1), service_has_fulltext, is_file_known);
	tracker_db_index_service (db_con, info, service_name, meta_table, is_attachment, service_has_metadata, service_has_fulltext, service_has_thumbs);


	if (is_attachment) {
		tracker_email_unlink_email_attachment (info->uri);
	}

	tracker_dec_info_ref (info);
	g_hash_table_destroy (meta_table);
	g_free (service_name);

	
	
}



static void 
index_conversation (DBConnection *db_con, FileInfo *info)
{

/* todo */
}


static void
index_application (DBConnection *db_con, FileInfo *info)
{

/* todo */
}


void
tracker_db_index_entity (DBConnection *db_con, FileInfo *info)
{

	g_return_if_fail (info->uri && (info->uri[0] == '/'));

	if (!info->is_directory) {
		/* sleep  to throttle back indexing */
		tracker_throttle (1000);
	}

	if (!tracker_file_is_valid (info->uri)) {
		tracker_log ("Warning - file %s no longer exists - abandoning index on this file", info->uri);
		return;
	}

	if (tracker_email_file_is_interesting (db_con, info)) {
		tracker_email_index_file (db_con, info);

	} else if (tracker_email_is_an_attachment (info)) {
		tracker_db_index_file (db_con, info, TRUE);
		
	} else if (!info->mime || strcmp (info->mime, "unknown") == 0) {
		g_free (info->mime);
		info->mime = tracker_get_service_for_uri (info->uri);
	}	

	if (strcmp (info->mime, "Files") == 0) {
		tracker_db_index_file (db_con, info, FALSE);
		
	} else if (strcmp (info->mime, "Conversations") == 0) {
		index_conversation (db_con, info);

	} else if (strcmp (info->mime, "Applications") == 0) {
		index_application (db_con, info);

	} else if (g_str_has_prefix (info->mime, "service/")) {
		tracker_db_index_service (db_con, info, NULL, NULL, FALSE, TRUE, TRUE, TRUE);
	}
	
}
