/* Tracker
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
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
	char	     *uri;
	gboolean     auto_create;
	gboolean     file_valid;
	gboolean     result;
	long	     file_id;

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

	dbus_message_get_args (rec->message, NULL,
			       DBUS_TYPE_STRING, &uri,
			       DBUS_TYPE_BOOLEAN, &auto_create,
			       DBUS_TYPE_INVALID);

	if (!uri)  {
		tracker_set_error (rec, "No file was specified");
		return;
	}

	file_id = tracker_db_get_file_id (db_con, uri);
	result = (file_id > 0);

	if (!result && auto_create) {
		char *name, *path, *mime, *service, *str_mtime, *str_size, *str_file_id;

		name = NULL;
		path = NULL;
		mime = NULL;
		service = NULL;
		str_mtime = NULL;
		str_size = NULL;
		str_file_id = NULL;

		if (uri[0] == G_DIR_SEPARATOR) {
			if (!tracker_file_is_valid (uri)) {
				file_valid = FALSE;
			} else {
				FileInfo *info;

				file_valid = TRUE;
				name = g_path_get_basename (uri);
				path = g_path_get_dirname (uri);
				mime = tracker_get_mime_type (uri);
				service = tracker_get_service_type_for_mime (mime);
				info = tracker_create_file_info (uri, 1, 0, 0);
				info = tracker_get_file_info (info);

				if (info) {
					str_size = tracker_long_to_str (info->file_size);
					str_mtime = tracker_long_to_str (info->mtime);

					tracker_free_file_info (info);
				}
			}

		} else {
			file_valid = TRUE;
			name = tracker_get_vfs_name (uri);
			path = tracker_get_vfs_path (uri);
			mime = g_strdup ("unknown");
			service = g_strdup ("VFS Files");
			str_mtime = g_strdup ("0");
			str_size = g_strdup ("0");
		}

		if (file_valid) {
			tracker_db_create_service (db_con, path, name, service, mime, FALSE, FALSE, FALSE, 0, 0);
		}

		file_id = tracker_db_get_file_id (db_con, uri);
		str_file_id = tracker_long_to_str (file_id);

		if (file_id > 0) {
			tracker_db_set_metadata (db_con, service, str_file_id, "File.Name", name, TRUE, TRUE);
			tracker_db_set_metadata (db_con, service, str_file_id, "File.Path", path, TRUE, TRUE);
			tracker_db_set_metadata (db_con, service, str_file_id, "File.Format", mime, TRUE, TRUE);
			tracker_db_set_metadata (db_con, service, str_file_id, "File.Modified", str_mtime, TRUE, TRUE);
			tracker_db_set_metadata (db_con, service, str_file_id, "File.Size", str_size, TRUE, TRUE);
		}

		if (name) {
			g_free (name);
		}
		if (path) {
			g_free (path);
		}
		if (mime) {
			g_free (mime);
		}
		if (service) {
			g_free (service);
		}
		if (str_mtime) {
			g_free (str_mtime);
		}
		if (str_size) {
			g_free (str_size);
		}
		if (str_file_id) {
			g_free (str_file_id);
		}
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
	char	     *uri, *name, *path, *mime, *service, *str_mtime, *str_size, *str_file_id;
	gboolean     is_dir;
	int	     size, mtime;
	long	     file_id;

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

	dbus_message_get_args (rec->message, NULL,
			       DBUS_TYPE_STRING, &uri,
			       DBUS_TYPE_BOOLEAN, &is_dir,
			       DBUS_TYPE_STRING, &mime,
			       DBUS_TYPE_INT32, &size,
			       DBUS_TYPE_INT32, &mtime,
			       DBUS_TYPE_INVALID);

	if (!uri) {
		tracker_set_error (rec, "No file was specified");
		return;
	}

	service = tracker_get_service_type_for_mime (mime);
	str_mtime = tracker_int_to_str (mtime);
	str_size = tracker_int_to_str (size);

	if (uri[0] == G_DIR_SEPARATOR) {
		name = g_path_get_basename (uri);
		path = g_path_get_dirname (uri);
	} else {
		name = tracker_get_vfs_name (uri);
		path = tracker_get_vfs_path (uri);
	}

	tracker_db_create_service (db_con, path, name, service, mime, is_dir, FALSE, FALSE, 0, mtime);

	file_id = tracker_db_get_file_id (db_con, uri);
	str_file_id = tracker_long_to_str (file_id);

	if (file_id != -1) {
		tracker_db_set_metadata (db_con, service, str_file_id, "File.Modified", str_mtime, TRUE, TRUE);
		tracker_db_set_metadata (db_con, service, str_file_id, "File.Size", str_size, TRUE, TRUE);
		tracker_db_set_metadata (db_con, service, str_file_id,  "File.Name", name, TRUE, TRUE);
		tracker_db_set_metadata (db_con, service, str_file_id, "File.Path", path, TRUE, TRUE);
		tracker_db_set_metadata (db_con, service, str_file_id, "File.Format", mime, TRUE, TRUE);
	}

	g_free (service);
	g_free (str_mtime);
	g_free (str_size);
	g_free (name);
	g_free (path);
	g_free (str_file_id);
}


void
tracker_dbus_method_files_delete (DBusRec *rec)
{
	DBConnection *db_con;
	char	     *uri, *name, *path, *str_file_id;
	long	     file_id;
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

	dbus_message_get_args  (rec->message, NULL,
				DBUS_TYPE_STRING, &uri,
				DBUS_TYPE_INVALID);

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
	str_file_id = tracker_long_to_str (file_id);
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

	if (file_id != -1) {
		if (is_dir) {
			tracker_db_delete_directory (db_con, file_id, path);
		} else {
			tracker_db_delete_file (db_con, file_id);
		}
	}

	g_free (name);
	g_free (path);
	g_free (str_file_id);
}


void
tracker_dbus_method_files_get_service_type (DBusRec *rec)
{
	DBConnection *db_con;
	DBusMessage  *reply;
	char	     *uri, *str_id, *mime, *result;
	long	     file_id;

/*
		<!-- Get the Service subtype for the file -->
		<method name="GetServiceType">
			<arg type="s" name="uri" direction="in" />
			<arg type="s" name="result" direction="out" />
		</method>
*/

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;

	dbus_message_get_args (rec->message, NULL,
			       DBUS_TYPE_STRING, &uri,
			       DBUS_TYPE_INVALID);

	if (!uri)  {
		tracker_set_error (rec, "No file was specified");
		return;
	}

	file_id = tracker_db_get_file_id (db_con, uri);

	if (file_id < 1) {
		tracker_set_error (rec, "File %s was not found in Tracker's database", uri);
		return;
	}

	str_id = tracker_long_to_str (file_id);

	mime = tracker_get_metadata (db_con, "Files", str_id, "File.Format");

	result = tracker_get_service_type_for_mime (mime);

	tracker_log ("info for file %s is : id=%ld, mime=%s, service=%s", uri, file_id, mime, result); 

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

	dbus_message_get_args  (rec->message, NULL,
				DBUS_TYPE_STRING, &uri,
				DBUS_TYPE_INT32, &offset,
				DBUS_TYPE_INT32, &max_length,
				DBUS_TYPE_INVALID);

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

		//char ***res = tracker_exec_proc (db_con, "GetFileContents", 4, path, name, str_offset, str_max_length);

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

	dbus_message_get_args  (rec->message, NULL,
				DBUS_TYPE_STRING, &uri,
				DBUS_TYPE_STRING, &text,
				DBUS_TYPE_INT32, &max_length,
				DBUS_TYPE_INVALID);

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

	dbus_message_get_args (rec->message, NULL,
			       DBUS_TYPE_STRING, &uri,
			       DBUS_TYPE_INVALID);

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

	dbus_message_get_args  (rec->message, NULL,
				DBUS_TYPE_INT32, &query_id,
				DBUS_TYPE_STRING, &service,
				DBUS_TYPE_INT32, &offset,
				DBUS_TYPE_INT32, &limit,
				DBUS_TYPE_INVALID);

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

	dbus_message_get_args  (rec->message, NULL,
				DBUS_TYPE_INT32, &query_id,
				DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &mimes, &n,
				DBUS_TYPE_INT32, &offset,
				DBUS_TYPE_INT32, &limit,
				DBUS_TYPE_INVALID);

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

	dbus_message_get_args  (rec->message, NULL,
				DBUS_TYPE_INT32, &query_id,
				DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &mimes, &n,
				DBUS_TYPE_INT32, &offset,
				DBUS_TYPE_INT32, &limit,
				DBUS_TYPE_INVALID);

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
	DBusMessage	*reply;
	DBusMessageIter iter;
	DBusMessageIter iter_dict;
	int		i, query_id, folder_name_len, file_id, n, table_count;
	char		*tmp_folder, *folder, *str;
	char		**array;
	GString		*sql;
	char		***res;

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;

	dbus_message_get_args (rec->message, NULL,
			       DBUS_TYPE_INT32, &query_id,
			       DBUS_TYPE_STRING, &tmp_folder,
			       DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &array, &n,
			       DBUS_TYPE_INVALID);

	folder_name_len = strlen (tmp_folder);

	folder_name_len--;

	if (folder_name_len != 0 && tmp_folder[folder_name_len] == G_DIR_SEPARATOR) {
		/* remove trailing 'G_DIR_SEPARATOR' */
		folder = g_strndup (tmp_folder, folder_name_len);
	} else {
		folder = g_strdup (tmp_folder);
	}

	file_id = tracker_get_file_id (db_con, folder, FALSE);

	if (file_id == -1) {
		tracker_set_error (rec, "Cannot find folder %s in Tracker database", folder);
		return;
	}

	/* build SELECT clause */
	sql = g_string_new (" SELECT concat( F.Path, '");

	sql = g_string_append (sql, G_DIR_SEPARATOR_S);

	sql = g_string_append (sql, "',  F.Name) as PathName ");

	table_count = 0;

	for (i = 0; i < n; i++) {
		FieldDef   *def;
		char	   *mid, *s;
		const char *field;

		def = tracker_db_get_field_def (db_con, array[i]);

		if (def) {
			if (def->type == DATA_INDEX_STRING) {
				field = "MetaDataIndexValue";
			} else if (def->type == DATA_STRING) {
				field = "MetaDataValue";
			} else {
				field = "MetaDataNumericValue";
			}

			tracker_db_free_field_def (def);
		} else {
			continue;
		}

		table_count++;

		mid = tracker_int_to_str (table_count);

		s = g_strconcat (", M", mid, ".", field, NULL);
		g_string_append (sql, s);

		g_free (mid);
		g_free (s);
	}


	/* build FROM clause */
	g_string_append (sql, " FROM Services F ");

	table_count = 0;

	for (i = 0; i < n; i++) {
		FieldDef *def;
		char	 *meta_id, *mid, *s;

		def = tracker_db_get_field_def (db_con, array[i]);

		if (def) {
			meta_id = g_strdup (def->id);
			tracker_db_free_field_def (def);
		} else {
			continue;
		}

		table_count++;

		mid = tracker_int_to_str (table_count);

		s = g_strconcat (" LEFT OUTER JOIN ServiceMetaData M", mid, " ON M", mid, ".ServiceID = F.ID ", " AND M", mid, ".MetaDataID = ", meta_id, NULL);
		g_string_append (sql, s);

		g_free (meta_id);
		g_free (mid);
		g_free (s);
	}

	/* build WHERE clause */
	str = g_strconcat (" WHERE F.Path = '", folder, "' ", NULL);

	g_string_append (sql, str);

	g_free (str);


	str = g_string_free (sql, FALSE);
	tracker_log (str);
	res = tracker_exec_sql (db_con, str);

	g_free (str);

	reply = dbus_message_new_method_return (rec->message);

	dbus_message_iter_init_append (reply, &iter);

	dbus_message_iter_open_container (&iter,
					  DBUS_TYPE_ARRAY,
					  DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
					  DBUS_TYPE_STRING_AS_STRING
					  DBUS_TYPE_VARIANT_AS_STRING
					  DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
					  &iter_dict);

	if (res) {
		tracker_add_query_result_to_dict (res, &iter_dict);
		tracker_db_free_result (res);
	}

	dbus_message_iter_close_container (&iter, &iter_dict);

	dbus_connection_send (rec->connection, reply, NULL);
	dbus_message_unref (reply);
}


void
tracker_dbus_method_files_search_by_text_mime (DBusRec *rec)
{
	DBConnection *db_con;
	DBusMessage  *reply;
	char	     *str;
	char	     **array;
	int	     n, row_count;
	char	     ***res;

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;

	dbus_message_get_args (rec->message, NULL,
			       DBUS_TYPE_STRING, &str,
			       DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &array, &n,
			       DBUS_TYPE_INVALID);

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
			tracker_log ("result set is empty");
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
	DBusMessage  *reply;
	char	     *str, *location;
	char	     **array;
	int	     row_count;
	char	     ***res;

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;

	dbus_message_get_args (rec->message, NULL,
			       DBUS_TYPE_STRING, &str,
			       DBUS_TYPE_STRING, &location,
			       DBUS_TYPE_INVALID);

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
			tracker_log ("result set is empty");
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
	DBusMessage  *reply;
	char	     *str, *location;
	char	     **array;
	int	     n, row_count;
	char	     ***res;

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;

	dbus_message_get_args (rec->message, NULL,
			       DBUS_TYPE_STRING, &str,
			       DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &array, &n,
			       DBUS_TYPE_STRING, &location,
			       DBUS_TYPE_INVALID);

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
			tracker_log ("result set is empty");
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
