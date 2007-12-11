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

#include "tracker-dbus-methods.h"
#include "tracker-metadata.h"
#include "tracker-dbus-files.h"


void
tracker_dbus_method_files_exists (DBusRec *rec)
{
	DBusMessage  *reply;
	DBConnection *db_con;
	DBusError    dbus_error;
	char	     *uri;
	gboolean     auto_create;
	gboolean     file_valid;
	gboolean     result;
	guint32	     file_id;

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;

/*
		<!-- Determines if the file is in tracker's database. The option auto_create if set to TRUE will register the file in the database if not already present -->
		<method name="Exists">
			<arg type="s" name="uri" direction="in" />
			<arg type="b" name="auto_create" direction="in" />
			<arg type="b" name="result" direction="out" />
		</method>
*/

	dbus_error_init(&dbus_error);
	if (!dbus_message_get_args (rec->message, &dbus_error,
			       DBUS_TYPE_STRING, &uri,
			       DBUS_TYPE_BOOLEAN, &auto_create,
			       DBUS_TYPE_INVALID)) {
		tracker_set_error(rec, "DBusError: %s;%s", dbus_error.name, dbus_error.message);
		dbus_error_free(&dbus_error);
		return;
	}


	if (!uri)  {
		tracker_set_error (rec, "No file was specified");
		return;
	}

	file_id = tracker_db_get_file_id (db_con, uri);
	result = (file_id > 0);

	if (!result && auto_create) {
		char *str_file_id, *service;
		FileInfo *info;
	
		info = NULL;
		service = NULL;
		str_file_id = NULL;

		info = tracker_create_file_info (uri, 1, 0, 0);

		if (!tracker_file_is_valid (uri)) {
			file_valid = FALSE;
			info->mime = g_strdup ("unknown");
			service = g_strdup ("Files");
		} else {
			info->mime = tracker_get_mime_type (uri);
			service = tracker_get_service_type_for_mime (info->mime);
			info = tracker_get_file_info (info);
		}

		file_id = tracker_db_create_service (db_con, "Files", info);
		tracker_free_file_info (info);
		g_free (service);
		
	}

	reply = dbus_message_new_method_return (rec->message);

	dbus_message_append_args (reply,
				  DBUS_TYPE_BOOLEAN, &result,
				  DBUS_TYPE_INVALID);

	dbus_connection_send (rec->connection, reply, NULL);
	dbus_message_unref (reply);
}


void
tracker_dbus_method_files_create (DBusRec *rec)
{
	DBConnection *db_con;
	DBusMessage  *reply;
	DBusError    dbus_error;
	char	     *uri, *name, *path, *mime, *service, *str_mtime, *str_size, *str_file_id;
	gboolean     is_dir;
	int	     size, mtime;
	guint32	     file_id;

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;

/*
		<!-- searches specified service for entities that match the specified search_text.
		     Returns id field of all hits. sort_by_relevance returns results sorted with the biggest hits first (as sorting is slower, you might want to disable this for fast queries) -->
		<method name="Create">
			<arg type="s" name="uri" direction="in" />
			<arg type="b" name="is_directory" direction="in" />
			<arg type="s" name="mime" direction="in" />
			<arg type="i" name="size" direction="in" />
			<arg type="i" name="mtime" direction="in" />
		</method>
*/

	dbus_error_init(&dbus_error);
	if (!dbus_message_get_args (rec->message, &dbus_error,
			       DBUS_TYPE_STRING, &uri,
			       DBUS_TYPE_BOOLEAN, &is_dir,
			       DBUS_TYPE_STRING, &mime,
			       DBUS_TYPE_INT32, &size,
			       DBUS_TYPE_INT32, &mtime,
			       DBUS_TYPE_INVALID)) {
		tracker_set_error(rec, "DBusError: %s;%s", dbus_error.name, dbus_error.message);
		dbus_error_free(&dbus_error);
		return;
	}

	if (!uri) {
		tracker_set_error (rec, "No file was specified");
		return;
	}

	
	FileInfo *info;
	
	info = NULL;
	service = NULL;
	str_file_id = NULL;

	info = tracker_create_file_info (uri, 1, 0, 0);

	info->mime = g_strdup (mime);
	service = tracker_get_service_type_for_mime (mime);
	info->is_directory = is_dir;
	info->file_size = size;
	info->mtime = mtime;

	str_mtime = tracker_int_to_str (mtime);
	str_size = tracker_int_to_str (size);
	name = NULL;
	
	if (info->uri[0] == G_DIR_SEPARATOR) {
		name = g_path_get_basename (info->uri);
		path = g_path_get_dirname (info->uri);
	} else {
		name = tracker_get_vfs_name (info->uri);
		path = tracker_get_vfs_path (info->uri);
	}


	file_id = tracker_db_create_service (db_con, service, info);
	tracker_free_file_info (info);
	str_file_id = tracker_uint_to_str (file_id);

	if (file_id != 0) {
		tracker_db_set_single_metadata (db_con, service, str_file_id, "File:Modified", str_mtime, FALSE);
		tracker_db_set_single_metadata (db_con, service, str_file_id, "File:Size", str_size, FALSE);
		tracker_db_set_single_metadata (db_con, service, str_file_id,  "File:Name", name, FALSE);
		tracker_db_set_single_metadata (db_con, service, str_file_id, "File:Path", path, FALSE);
		tracker_db_set_single_metadata (db_con, service, str_file_id, "File:Format", mime, FALSE);
		tracker_notify_file_data_available ();
	}

	g_free (service);
	g_free (str_mtime);
	g_free (str_size);
	g_free (name);
	g_free (path);
	g_free (str_file_id);

	reply = dbus_message_new_method_return (rec->message);
	dbus_message_append_args (reply, DBUS_TYPE_INVALID);
	dbus_connection_send (rec->connection, reply, NULL);
	dbus_message_unref (reply);
	
}


void
tracker_dbus_method_files_delete (DBusRec *rec)
{
	DBConnection *db_con;
	DBusMessage  *reply;
	DBusError    dbus_error;
	char	     *uri, *name, *path, *str_file_id;
	guint32	     file_id;
	gboolean     is_dir;
	char	     ***res;

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;

/*
		<!-- Removes the file entry from tracker's database-->
		<method name="Delete">
			<arg type="s" name="uri" direction="in" />
		</method>
*/

	dbus_error_init(&dbus_error);
	if (!dbus_message_get_args  (rec->message, &dbus_error,
				DBUS_TYPE_STRING, &uri,
				DBUS_TYPE_INVALID)) {
		tracker_set_error(rec, "DBusError: %s;%s", dbus_error.name, dbus_error.message);
		dbus_error_free(&dbus_error);
		return;
	}

	if (!uri) {
		tracker_set_error (rec, "No file was specified");
		return;
	}

	if (uri[0] == G_DIR_SEPARATOR) {
		name = g_path_get_basename (uri);
		path = g_path_get_dirname (uri);
	} else {
		name = tracker_get_vfs_name (uri);
		path = tracker_get_vfs_path (uri);
	}

	file_id = tracker_db_get_file_id (db_con, uri);
	str_file_id = tracker_uint_to_str (file_id);
	is_dir = FALSE;

	res = tracker_exec_proc (db_con, "GetServiceID", 2, path, name);

	if (res) {
		char **row;

		row = tracker_db_get_row (res, 0);

		if (row && row[2] ) {
			is_dir = (strcmp (row[2], "1") == 0);
		}

		tracker_db_free_result (res);
	}

	if (file_id != 0) {
		if (is_dir) {
			tracker_db_insert_pending_file (db_con, file_id, uri, NULL,  g_strdup ("unknown"), 0, TRACKER_ACTION_DIRECTORY_DELETED, TRUE, FALSE, -1);
		} else {
			tracker_db_insert_pending_file (db_con, file_id, uri, NULL,  g_strdup ("unknown"), 0, TRACKER_ACTION_FILE_DELETED, FALSE, FALSE, -1);
		}
	}

	g_free (name);
	g_free (path);
	g_free (str_file_id);

	reply = dbus_message_new_method_return (rec->message);
	dbus_message_append_args (reply, DBUS_TYPE_INVALID);
	dbus_connection_send (rec->connection, reply, NULL);
	dbus_message_unref (reply);
}


void
tracker_dbus_method_files_get_service_type (DBusRec *rec)
{
	DBConnection *db_con;
	DBusError    dbus_error;
	DBusMessage  *reply;
	char	     *uri, *str_id, *mime, *result;
	guint32	     file_id;

/*
		<!-- Get the Service subtype for the file -->
		<method name="GetServiceType">
			<arg type="s" name="uri" direction="in" />
			<arg type="s" name="result" direction="out" />
		</method>
*/

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;

	dbus_error_init(&dbus_error);
	if (!dbus_message_get_args (rec->message, &dbus_error,
			       DBUS_TYPE_STRING, &uri,
			       DBUS_TYPE_INVALID)) {
		tracker_set_error(rec, "DBusError: %s;%s", dbus_error.name, dbus_error.message);
		dbus_error_free(&dbus_error);
		return;
	}

	if (!uri)  {
		tracker_set_error (rec, "No file was specified");
		return;
	}

	file_id = tracker_db_get_file_id (db_con, uri);

	if (file_id < 1) {
		tracker_set_error (rec, "File %s was not found in Tracker's database", uri);
		return;
	}

	str_id = tracker_uint_to_str (file_id);

	mime = tracker_get_metadata (db_con, "Files", str_id, "File:Mime");

	result = tracker_get_service_type_for_mime (mime);

	tracker_log ("Info for file %s is : id=%u, mime=%s, service=%s", uri, file_id, mime, result); 

	g_free (mime);

	g_free (str_id);

	reply = dbus_message_new_method_return (rec->message);

	dbus_message_append_args (reply,
				  DBUS_TYPE_STRING, &result,
				  DBUS_TYPE_INVALID);

	g_free (result);

	dbus_connection_send (rec->connection, reply, NULL);

	dbus_message_unref (reply);
}


void
tracker_dbus_method_files_get_text_contents (DBusRec *rec)
{
	DBConnection *db_con;
	DBusError    dbus_error;
	char	     *uri;
	int	     offset, max_length;

/*
		<!-- Get the "File.Content" field for a file and allows you to specify the offset and amount of text to retrieve  -->
		<method name="GetTextContents">
			<arg type="s" name="uri" direction="in" />
			<arg type="i" name="offset"  direction="in" />
			<arg type="i" name="max_length"  direction="in" />
			<arg type="s" name="result" direction="out" />
		</method>
*/

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;

	dbus_error_init(&dbus_error);
	if (!dbus_message_get_args  (rec->message, &dbus_error,
				DBUS_TYPE_STRING, &uri,
				DBUS_TYPE_INT32, &offset,
				DBUS_TYPE_INT32, &max_length,
				DBUS_TYPE_INVALID)) {
		tracker_set_error(rec, "DBusError: %s;%s", dbus_error.name, dbus_error.message);
		dbus_error_free(&dbus_error);
		return;
	}

	if (uri) {
		char *name, *path, *str_offset, *str_max_length;
		char ***res;

		if (uri[0] == G_DIR_SEPARATOR) {
			name = g_path_get_basename (uri);
			path = g_path_get_dirname (uri);
		} else {
			name = tracker_get_vfs_name (uri);
			path = tracker_get_vfs_path (uri);
		}

		str_offset = tracker_int_to_str (offset);
		str_max_length = tracker_int_to_str (max_length);

		res = tracker_exec_proc (db_con, "GetFileContents", 4, path, name, str_offset, str_max_length);

		g_free (str_offset);
		g_free (str_max_length);
		g_free (path);
		g_free (name);

		if (res) {
			char **row;

			row = tracker_db_get_row (res, 0);

			if (row && row[0]) {
				DBusMessage *reply;
				const char  *result;

				result = row[0];

				reply = dbus_message_new_method_return (rec->message);

				dbus_message_append_args (reply,
							  DBUS_TYPE_STRING, &result,
							  DBUS_TYPE_INVALID);

				dbus_connection_send (rec->connection, reply, NULL);
				dbus_message_unref (reply);
			}

			tracker_db_free_result (res);
		}
	}
}


void
tracker_dbus_method_files_search_text_contents (DBusRec *rec)
{
	DBConnection *db_con;
	DBusError    dbus_error;
	char	     *uri, *text;
	int	     max_length;

/*
		<!-- Retrieves a chunk of matching text of specified length that contains the search text in the File.Content field -->
		<method name="SearchTextContents">
			<arg type="s" name="uri" direction="in" />
			<arg type="s" name="text"  direction="in" />
			<arg type="i" name="length"  direction="in" />
			<arg type="s" name="result" direction="out" />
		</method>
*/

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;

	tracker_set_error (rec, "Method not implemented yet");
	return;

	/* ******************************************************************** */

	dbus_error_init(&dbus_error);
	if (!dbus_message_get_args  (rec->message, &dbus_error,
				DBUS_TYPE_STRING, &uri,
				DBUS_TYPE_STRING, &text,
				DBUS_TYPE_INT32, &max_length,
				DBUS_TYPE_INVALID)) {
		tracker_set_error(rec, "DBusError: %s;%s", dbus_error.name, dbus_error.message);
		dbus_error_free(&dbus_error);
		return;
	}

	if (uri) {
		char *path, *name, *str_max_length;
		char ***res;


		if (uri[0] == G_DIR_SEPARATOR) {
			name = g_path_get_basename (uri);
			path = g_path_get_dirname (uri);
		} else {
			name = tracker_get_vfs_name (uri);
			path = tracker_get_vfs_path (uri);
		}

		str_max_length = tracker_int_to_str (max_length);

		//tracker_exec_proc (db_con, "SearchFileContents", 4, path, name, text, str_max_length);

		g_free (str_max_length);
		g_free (path);
		g_free (name);

		if (res) {
			char **row;

			row = tracker_db_get_row (res, 0);

			if (row && row[0]) {
				DBusMessage *reply;
				const char  *result;

				result = row[0];

				reply = dbus_message_new_method_return (rec->message);

				dbus_message_append_args (reply,
							  DBUS_TYPE_STRING, &result,
							  DBUS_TYPE_INVALID);

				dbus_connection_send (rec->connection, reply, NULL);
				dbus_message_unref (reply);
			}

			tracker_db_free_result (res);
		}
	}
}


void
tracker_dbus_method_files_get_mtime (DBusRec *rec)
{
	DBConnection *db_con;
	DBusError    dbus_error;
	char	     *uri;

/*
		<!-- returns mtime of file in seconds since epoch -->
		<method name="GetMTime">
			<arg type="s" name="uri" direction="in" />
			<arg type="i" name="result" direction="out" />
		</method>
*/

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;

	dbus_error_init(&dbus_error);
	if (!dbus_message_get_args (rec->message, &dbus_error,
			       DBUS_TYPE_STRING, &uri,
			       DBUS_TYPE_INVALID)) {
		tracker_set_error(rec, "DBusError: %s;%s", dbus_error.name, dbus_error.message);
		dbus_error_free(&dbus_error);
		return;
	}

	if (uri) {
		char *path, *name;
		char ***res;

		if (uri[0] == G_DIR_SEPARATOR) {
			name = g_path_get_basename (uri);
			path = g_path_get_dirname (uri);
		} else {
			name = tracker_get_vfs_name (uri);
			path = tracker_get_vfs_path (uri);
		}

		res = tracker_exec_proc (db_con, "GetFileMTime", 2, path, name);

		g_free (path);
		g_free (name);

		if (res) {
			char **row;

			row = tracker_db_get_row (res, 0);

			if (row && row[0]) {
				DBusMessage *reply;
				int	    result;

				result = atoi (row[0]);
				reply = dbus_message_new_method_return (rec->message);

				dbus_message_append_args (reply,
							  DBUS_TYPE_INT32, &result,
							  DBUS_TYPE_INVALID);

				dbus_connection_send (rec->connection, reply, NULL);
				dbus_message_unref (reply);
			}

			tracker_db_free_result (res);
		}
	}
}


void
tracker_dbus_method_files_get_by_service_type (DBusRec *rec)
{
	DBConnection *db_con;
	DBusError    dbus_error;
	DBusMessage  *reply;
	int 	     query_id, limit, offset, row_count;
	char 	     *service;
	char 	     **array;
	char	     ***res;

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;

/*
		<!--
		Retrieves all files that match a service description
		-->
		<method name="GetByServiceType">
			<arg type="i" name="live_query_id" direction="in" />
			<arg type="s" name="file_service" direction="in" />
			<arg type="i" name="offset" direction="in" />
			<arg type="i" name="max_hits" direction="in" />
			<arg type="as" name="result" direction="out" />
		</method>
*/

	dbus_error_init(&dbus_error);
	if (!dbus_message_get_args  (rec->message, &dbus_error,
				DBUS_TYPE_INT32, &query_id,
				DBUS_TYPE_STRING, &service,
				DBUS_TYPE_INT32, &offset,
				DBUS_TYPE_INT32, &limit,
				DBUS_TYPE_INVALID)) {
		tracker_set_error(rec, "DBusError: %s;%s", dbus_error.name, dbus_error.message);
		dbus_error_free(&dbus_error);
		return;
	}

	if (!tracker_is_valid_service (db_con, service)) {
		tracker_set_error (rec, "Invalid service %s or service has not been implemented yet", service);
		return;
	}


	res = tracker_db_get_files_by_service (db_con, service, offset, limit);

	array = NULL;
	row_count = 0;

	if (res) {
		array = tracker_get_query_result_as_array (res, &row_count);
		tracker_db_free_result (res);
	}

	reply = dbus_message_new_method_return (rec->message);

	dbus_message_append_args (reply,
				  DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &array, row_count,
				  DBUS_TYPE_INVALID);

	tracker_free_array (array, row_count);

	dbus_connection_send (rec->connection, reply, NULL);
	dbus_message_unref (reply);
}


void
tracker_dbus_method_files_get_by_mime_type (DBusRec *rec)
{
	DBConnection *db_con;
	DBusError    dbus_error;
	DBusMessage  *reply;
	int	     query_id, n, offset, limit, row_count;
	char	     **array, **mimes;
	char	     ***res;

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;

/*
		<!-- Retrieves all non-vfs files of the specified mime type(s) -->
		<method name="GetByMimeType">
			<arg type="i" name="live_query_id" direction="in" />
			<arg type="as" name="mime_types" direction="in" />
			<arg type="i" name="offset" direction="in" />
			<arg type="i" name="max_hits" direction="in" />
			<arg type="as" name="result" direction="out" />
		</method>
*/

	dbus_error_init(&dbus_error);
	if (!dbus_message_get_args  (rec->message, &dbus_error,
				DBUS_TYPE_INT32, &query_id,
				DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &mimes, &n,
				DBUS_TYPE_INT32, &offset,
				DBUS_TYPE_INT32, &limit,
				DBUS_TYPE_INVALID)) {
		tracker_set_error(rec, "DBusError: %s;%s", dbus_error.name, dbus_error.message);
		dbus_error_free(&dbus_error);
		return;
	}

	if (n < 1) {
		tracker_set_error (rec, "No mimes specified");
		return;
	}

	res = tracker_db_get_files_by_mime (db_con, mimes, n, offset, limit, FALSE);

	array = NULL;
	row_count = 0;

	if (res) {
		array = tracker_get_query_result_as_array (res, &row_count);
		tracker_db_free_result (res);
	}

	reply = dbus_message_new_method_return (rec->message);

	dbus_message_append_args (reply,
				  DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &array, row_count,
				  DBUS_TYPE_INVALID);

	tracker_free_array (array, row_count);

	dbus_connection_send (rec->connection, reply, NULL);
	dbus_message_unref (reply);
}


void
tracker_dbus_method_files_get_by_mime_type_vfs (DBusRec *rec)
{
	DBConnection *db_con;
	DBusError    dbus_error;
	DBusMessage  *reply;
	int	     query_id, n, offset, limit, row_count;
	char	     **array, **mimes;
	char	     ***res;

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;

/*
		<!-- Retrieves all vfs files of the specified mime type(s) -->
		<method name="GetByMimeTypeVfs">
			<arg type="i" name="live_query_id" direction="in" />
			<arg type="as" name="mime_types" direction="in" />
			<arg type="i" name="offset" direction="in" />
			<arg type="i" name="max_hits" direction="in" />
			<arg type="as" name="result" direction="out" />
		</method>
*/

	dbus_error_init(&dbus_error);
	if (!dbus_message_get_args  (rec->message, &dbus_error,
				DBUS_TYPE_INT32, &query_id,
				DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &mimes, &n,
				DBUS_TYPE_INT32, &offset,
				DBUS_TYPE_INT32, &limit,
				DBUS_TYPE_INVALID)) {
		tracker_set_error(rec, "DBusError: %s;%s", dbus_error.name, dbus_error.message);
		dbus_error_free(&dbus_error);
		return;
	}

	res = tracker_db_get_files_by_mime (db_con, mimes, n, offset, limit, TRUE);

	array = NULL;
	row_count = 0;

	if (res) {
		array = tracker_get_query_result_as_array (res, &row_count);
		tracker_db_free_result (res);
	}

	reply = dbus_message_new_method_return (rec->message);

	dbus_message_append_args (reply,
				  DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &array, row_count,
				  DBUS_TYPE_INVALID);

	tracker_free_array (array, row_count);

	dbus_connection_send (rec->connection, reply, NULL);
	dbus_message_unref (reply);
}


void
tracker_dbus_method_files_get_metadata_for_files_in_folder (DBusRec *rec)
{
	DBConnection	*db_con;
	DBusError		dbus_error;
	int		i, query_id, folder_name_len, file_id, n;
	char		*tmp_folder, *folder, *str;
	char		**array;
	GString		*sql;
	char		***res;
	FieldDef	*defs[255];
	gboolean 	needs_join[255];

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;

/*
		<!-- Retrieves all non-vfs files in a folder complete with all requested metadata fields. An array of stringarrays is outpout with uri and field metadata as part of the array  -->
		<method name="GetMetadataForFilesInFolder">
			<arg type="i" name="live_query_id" direction="in" />
			<arg type="s" name="uri" direction="in" />
			<arg type="as" name="fields" direction="in" />
			<arg type="aas" name="values" direction="out" />
		</method>
*/


	dbus_error_init(&dbus_error);
	if (!dbus_message_get_args (rec->message, &dbus_error,
			       DBUS_TYPE_INT32, &query_id,
			       DBUS_TYPE_STRING, &tmp_folder,
			       DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &array, &n,
			       DBUS_TYPE_INVALID)) {
		tracker_set_error(rec, "DBusError: %s;%s", dbus_error.name, dbus_error.message);
		dbus_error_free(&dbus_error);
		return;
	}


	for (i = 0; i < n; i++) {
		defs[i] =   tracker_db_get_field_def (db_con, array[i]);

		if (!defs[i]) {
			tracker_set_error(rec, "Error: Metadata field %s was not found", array[i]);
			dbus_error_free(&dbus_error);
		}

	}


	folder_name_len = strlen (tmp_folder);

	folder_name_len--;

	if (folder_name_len != 0 && tmp_folder[folder_name_len] == G_DIR_SEPARATOR) {
		/* remove trailing 'G_DIR_SEPARATOR' */
		folder = g_strndup (tmp_folder, folder_name_len);
	} else {
		folder = g_strdup (tmp_folder);
	}

	file_id = tracker_get_file_id (db_con, folder, FALSE);

	if (file_id == 0) {
		tracker_set_error (rec, "Cannot find folder %s in Tracker database", folder);
		return;
	}

	/* build SELECT clause */
	sql = g_string_new (" SELECT (F.Path || ");

	g_string_append_printf (sql, "'%s' || F.Name) as PathName ", G_DIR_SEPARATOR_S);

	for (i = 1; i <= n; i++) {

		char *my_field = tracker_db_get_field_name ("Files", array[i-1]);

		if (my_field) {
			g_string_append_printf (sql, ", F.%s ", my_field);
			g_free (my_field);
			needs_join[i-1] = FALSE;
		} else {
			char *disp_field = tracker_db_get_display_field (defs[i]);
			g_string_append_printf (sql, ", M%d.%s ", i, disp_field);
			g_free (disp_field);
			needs_join[i-1] = TRUE;
		}
	}


	/* build FROM clause */
	g_string_append (sql, " FROM Services F ");

	for (i = 0; i < n; i++) {

		char *table;

		if (!needs_join[i]) {
			continue;
		}

		table = tracker_get_metadata_table (defs[i]->type);

		g_string_append_printf (sql, " LEFT OUTER JOIN %s M%d ON F.ID = M%d.ServiceID AND M%d.MetaDataID = %s ", table, i+1, i+1, i+1, defs[i]->id);

		g_free (table);

	}

	dbus_free_string_array(array);
	
	/* build WHERE clause */

	g_string_append_printf (sql, " WHERE F.Path = '%s' ", folder);

	str = g_string_free (sql, FALSE);

	tracker_debug (str);

	res = tracker_exec_sql_ignore_nulls (db_con, str);

	g_free (str);

	tracker_dbus_reply_with_query_result (rec, res);

	tracker_db_free_result (res);
}


void
tracker_dbus_method_files_search_by_text_mime (DBusRec *rec)
{
	DBConnection *db_con;
	DBusError    dbus_error;
	DBusMessage  *reply;
	char	     *str;
	char	     **array;
	int	     n, row_count;
	char	     ***res;

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;

	dbus_error_init(&dbus_error);
	if (!dbus_message_get_args (rec->message, &dbus_error,
			       DBUS_TYPE_STRING, &str,
			       DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &array, &n,
			       DBUS_TYPE_INVALID)) {
		tracker_set_error(rec, "DBusError: %s;%s", dbus_error.name, dbus_error.message);
		dbus_error_free(&dbus_error);
		return;
	}

	res = tracker_db_search_text_mime (db_con, str, array, n);

	array = NULL;
	row_count = 0;

	if (res) {

		row_count = tracker_get_row_count (res);

		if (row_count > 0) {
			char **row;
			int  i;

			array = g_new (char *, row_count);

			i = 0;

			while ((row = tracker_db_get_row (res, i))) {

				if (row && row[0] && row[1]) {
					array[i] = g_build_filename (row[0], row[1], NULL);
				}
				i++;
			}

		} else {
			tracker_log ("Result set is empty");
		}

		tracker_db_free_result (res);

	} else {
		array = g_new (char *, 1);

		array[0] = NULL;
	}


	reply = dbus_message_new_method_return (rec->message);

	dbus_message_append_args (reply,
				  DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &array, row_count,
				  DBUS_TYPE_INVALID);

	tracker_free_array (array, row_count);

	dbus_connection_send (rec->connection, reply, NULL);
	dbus_message_unref (reply);
}


void
tracker_dbus_method_files_search_by_text_location (DBusRec *rec)
{
	DBConnection *db_con;
	DBusError    dbus_error;
	DBusMessage  *reply;
	char	     *str, *location;
	char	     **array;
	int	     row_count;
	char	     ***res;

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;

	dbus_error_init(&dbus_error);
	if (!dbus_message_get_args (rec->message, &dbus_error,
			       DBUS_TYPE_STRING, &str,
			       DBUS_TYPE_STRING, &location,
			       DBUS_TYPE_INVALID)) {
		tracker_set_error(rec, "DBusError: %s;%s", dbus_error.name, dbus_error.message);
		dbus_error_free(&dbus_error);
		return;
	}

	res = tracker_db_search_text_location (db_con, str, location);

	array = NULL;
	row_count = 0;

	if (res) {

		row_count = tracker_get_row_count (res);

		if (row_count > 0) {
			char **row;
			int  i;

			array = g_new (char *, row_count);

			i = 0;

			while ((row = tracker_db_get_row (res, i))) {

				if (row && row[0] && row[1]) {
					array[i] = g_build_filename (row[0], row[1], NULL);
				}
				i++;
			}

		} else {
			tracker_log ("Result set is empty");
		}

		tracker_db_free_result (res);

	} else {
		array = g_new (char *, 1);

		array[0] = NULL;
	}


	reply = dbus_message_new_method_return (rec->message);

	dbus_message_append_args (reply,
				  DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &array, row_count,
				  DBUS_TYPE_INVALID);

	tracker_free_array (array, row_count);

	dbus_connection_send (rec->connection, reply, NULL);
	dbus_message_unref (reply);
}


void
tracker_dbus_method_files_search_by_text_mime_location (DBusRec *rec)
{
	DBConnection *db_con;
	DBusError    dbus_error;
	DBusMessage  *reply;
	char	     *str, *location;
	char	     **array;
	int	     n, row_count;
	char	     ***res;

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;

	dbus_error_init(&dbus_error);
	if (!dbus_message_get_args (rec->message, &dbus_error,
			       DBUS_TYPE_STRING, &str,
			       DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &array, &n,
			       DBUS_TYPE_STRING, &location,
			       DBUS_TYPE_INVALID)) {
		tracker_set_error(rec, "DBusError: %s;%s", dbus_error.name, dbus_error.message);
		dbus_error_free(&dbus_error);
		return;
	}

	res = tracker_db_search_text_mime_location (db_con, str, array, n, location);

	array = NULL;
	row_count = 0;

	if (res) {

		row_count = tracker_get_row_count (res);

		if (row_count > 0) {
			char **row;
			int  i;

			array = g_new (char *, row_count);

			i = 0;

			while ((row = tracker_db_get_row (res, i))) {

				if (row && row[0] && row[1]) {
					array[i] = g_build_filename (row[0], row[1], NULL);
				}
				i++;
			}

		} else {
			tracker_log ("Result set is empty");
		}

		tracker_db_free_result (res);

	} else {
		array = g_new (char *, 1);

		array[0] = NULL;
	}


	reply = dbus_message_new_method_return (rec->message);

	dbus_message_append_args (reply,
				  DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &array, row_count,
				  DBUS_TYPE_INVALID);

	tracker_free_array (array, row_count);

	dbus_connection_send (rec->connection, reply, NULL);
	dbus_message_unref (reply);
}
