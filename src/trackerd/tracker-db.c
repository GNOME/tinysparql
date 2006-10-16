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
tracker_db_add_embedded_keywords (DBConnection *db_con, const char *file_id, const char *keywords)
{
	char **array, **tags;
	char *tag;
	
	array = g_strsplit_set (keywords, "\t\n\v\f\r !\"#$%&'()*/<=>?[\\]^`{|}~+,.:;@\"[]", -1);

	for (tags = array; *tags; ++tags) {

		tag = *tags;
		tag = g_strstrip (tag);

		if (strlen (tag) > 2) {
			tracker_log ("Auto-tagging file with %s", tag);
			tracker_exec_proc (db_con, "AddEmbeddedKeyword", 2, file_id, tag);
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

	tracker_db_set_metadata (db_action->db_con, "Files", db_action->file_id, mtype, mvalue, TRUE, TRUE, TRUE);

	/* auto-tag keyword related metadata */	
	if ( (strcasecmp (mtype, "Doc.Keywords") == 0) || (strcasecmp (mtype, "Image.Keywords") == 0) ) {
		tracker_db_add_embedded_keywords (db_action->db_con, db_action->file_id, mvalue);
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

	tracker_db_set_metadata (db_action->db_con, "Files", db_action->file_id, mtype, mvalue, TRUE, FALSE, TRUE);

	/* auto-tag keyword related metadata */	
	if ( (strcasecmp (mtype, "Doc.Keywords") == 0) || (strcasecmp (mtype, "Image.Keywords") == 0) ) {
		tracker_db_add_embedded_keywords (db_action->db_con, db_action->file_id, mvalue);
	}

	if (mvalue) {
		g_free (mvalue);
	}
}

void
tracker_db_save_metadata (DBConnection *db_con, GHashTable *table, guint32 file_id, gboolean new_file)
{
	DatabaseAction db_action;

	g_return_if_fail (file_id != 0 || table || db_con);

	db_action.db_con = db_con;

	db_action.file_id = tracker_uint_to_str ( file_id);

	if (table) {
		tracker_db_start_transaction (db_con);
		if (new_file) {
			g_hash_table_foreach (table, get_meta_table_data_new, &db_action);
		} else {
			g_hash_table_foreach (table, get_meta_table_data, &db_action);
		}
		tracker_db_end_transaction (db_con);
	}

	g_free (db_action.file_id);
}


off_t
tracker_db_get_last_mbox_offset (DBConnection *db_con, const char *mbox_uri)
{
	/* FIXME */
	return 0;
}


void
tracker_db_update_mbox_offset (DBConnection *db_con, MailBox *mb)
{
	/* FIXME
	 * new offset is in mb->next_email_offset
	 */
}


void
tracker_db_save_email (DBConnection *db_con, MailMessage *mm)
{
	GString	     *s;
	const GSList *tmp;
	char	     *to_print;

	if (!mm) {
		return;
	}

	/*
	 * FIXME
	 */

	s = g_string_new ("");

	g_string_append_printf (s,
				"Saving email with mbox's uri \"%s\" and id \"%s\":\n"
				"- uri: %s\n"
				"- offset: %lld\n"
				"- deleted?: %d\n"
				"- junk?: %d\n",
				mm->parent_mbox->mbox_uri, mm->message_id,
				mm->uri,
				mm->offset,
				mm->deleted,
				mm->junk);

	g_string_append (s, "- references: ");
	for (tmp = mm->references; tmp; tmp = g_slist_next (tmp)) {
		g_string_append_printf (s, "%s * ", (char *) tmp->data);
	}
	g_string_append (s, "\n");

	g_string_append (s, "- reply-to-id: ");
	for (tmp = mm->reply_to_id; tmp; tmp = g_slist_next (tmp)) {
		g_string_append_printf (s, "%s * ", (char *) tmp->data);
	}
	g_string_append (s, "\n");

	g_string_append_printf (s,
				"- date: %s\n"
				"- mail_from: %s\n",
				ctime (&mm->date),
				mm->mail_from);

	g_string_append (s, "- mail_to: ");
	for (tmp = mm->mail_to; tmp; tmp = g_slist_next (tmp)) {
		Person *p;
		p = (Person *) tmp->data;
		g_string_append_printf (s, "name:%s, addr:%s ** ", p->name, p->addr);
	}
	g_string_append (s, "\n");

	g_string_append (s, "- mail_cc: ");
	for (tmp = mm->mail_cc; tmp; tmp = g_slist_next (tmp)) {
		Person *p;
		p = (Person *) tmp->data;
		g_string_append_printf (s, "name:%s, addr:%s ** ", p->name, p->addr);
	}
	g_string_append (s, "\n");

	g_string_append (s, "- mail_bcc: ");
	for (tmp = mm->mail_bcc; tmp; tmp = g_slist_next (tmp)) {
		Person *p;
		p = (Person *) tmp->data;
		g_string_append_printf (s, "name:%s, addr:%s ** ", p->name, p->addr);
	}
	g_string_append (s, "\n");

	g_string_append_printf (s,
				"- subject: %s\n"
				"- content_type: %s\n"
				"- body: %s\n"
				"- path_to_attachments: %s\n",
				mm->subject,
				mm->content_type,
				mm->body,
				mm->path_to_attachments);

	g_string_append (s, "- attachments: ");
	for (tmp = mm->attachments; tmp; tmp = g_slist_next (tmp)) {
		MailAttachment *ma;

		ma = (MailAttachment *) tmp->data;

		g_string_append_printf (s, "%s * ", ma->attachment_name);
	}
	g_string_append (s, "\n");


	to_print = g_string_free (s, FALSE);

	tracker_log ("%s\n", to_print);

	g_free (to_print);
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
		tracker_db_set_metadata (db_con, "Files", str_file_id, "File.SmallThumbnailPath", small_thumb_file, TRUE, FALSE, TRUE);
//		tracker_exec_proc (db_con, "SetMetadata", 5, "Files", str_file_id, "File.SmallThumbnailPath", small_thumb_file, "1");
		g_free (small_thumb_file);
	}

	if (large_thumb) {
		char *large_thumb_file;

		large_thumb_file = tracker_escape_string (db_con, large_thumb);
		tracker_db_set_metadata (db_con, "Files", str_file_id, "File.LargeThumbnailPath", large_thumb_file, TRUE, FALSE, TRUE);
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


