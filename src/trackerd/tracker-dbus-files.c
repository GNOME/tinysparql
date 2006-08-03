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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "tracker-dbus-methods.h"
#include "tracker-metadata.h"
#include "tracker-dbus-files.h"


void
tracker_dbus_method_files_exists (DBusRec *rec)
{	
	DBusMessage *reply;
	DBConnection *db_con;
	char *mime = NULL, *uri = NULL, *name, *path, *service;
	gboolean auto_create;
	gboolean file_valid = FALSE;
	gboolean result = FALSE;

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
	
	dbus_message_get_args  (rec->message, NULL, 
				DBUS_TYPE_STRING, &uri,
				DBUS_TYPE_BOOLEAN, &auto_create,			
				DBUS_TYPE_INVALID);

	
	if (!uri)  {
		tracker_set_error (rec, "No file was specified");	
		return;
	}

	long file_id = tracker_db_get_file_id (db_con, uri);
	result = (file_id > 0);

	if (!result && auto_create) {

		char *str_size;
		char *str_mtime;
		const char *str_is_dir;

		if (uri[0] == '/') {
			if (!tracker_file_is_valid (uri)) {
				file_valid = FALSE;	
			} else {
				file_valid = TRUE;
				name = g_path_get_basename (uri);
				path = g_path_get_dirname (uri);
				mime = tracker_get_mime_type (uri);
				service = tracker_get_service_type_for_mime (mime);
				FileInfo *info = tracker_create_file_info (uri, 1, 0, 0);
				info = tracker_get_file_info (info);
				if (info) {
					str_size = tracker_long_to_str (info->file_size);
					str_mtime = tracker_long_to_str (info->mtime);
					if (info->is_directory) {
						str_is_dir = "1";
					} else {
						str_is_dir = "0";
					}
					info = tracker_free_file_info (info);
				} 
				
			}
		

		} else {
			name = tracker_get_vfs_name (uri);
			path = tracker_get_vfs_path (uri);
			file_valid = TRUE;
			mime = g_strdup ("unknown");
			service = g_strdup ("VFS Files");
			str_size = g_strdup ("0");
			str_mtime = g_strdup ("0");
			str_is_dir = "0";
		
		}
		
		if (file_valid) {
			tracker_exec_proc  (db_con->db, "CreateService", 8, path, name, service, str_is_dir, "0", "0", "0",  str_mtime);	
		}

		file_id = tracker_db_get_file_id (db_con, uri);
		char *str_file_id = tracker_long_to_str (file_id);

		if (file_id > 0) {
			tracker_exec_proc  (db_con->db, "SetMetadata", 5,  service, str_file_id, "File.Modified",  str_mtime, "1");	
			tracker_exec_proc  (db_con->db, "SetMetadata", 5,  service, str_file_id, "File.Size",  str_size, "1");
			tracker_exec_proc  (db_con->db, "SetMetadata", 5,  service, str_file_id, "File.Name",  name, "1");		
			tracker_exec_proc  (db_con->db, "SetMetadata", 5,  service, str_file_id, "File.Path",  path, "1");
			tracker_exec_proc  (db_con->db, "SetMetadata", 5,  service, str_file_id, "File.Format",  mime, "1");
		}

		g_free (mime);
		g_free (service);
		g_free (str_mtime);
		g_free (str_size);
		g_free (name);
		g_free (path);
		g_free (str_file_id);
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
	char *mime = NULL, *uri = NULL, *name, *path;
	int mtime, size;
	gboolean is_dir;

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
	
	dbus_message_get_args  (rec->message, NULL, 
				DBUS_TYPE_STRING, &uri,
				DBUS_TYPE_BOOLEAN, &is_dir,
				DBUS_TYPE_STRING, &mime,  
				DBUS_TYPE_INT32, &size,
				DBUS_TYPE_INT32, &mtime,				
				DBUS_TYPE_INVALID);

	
	if (!uri)  {
		tracker_set_error (rec, "No file was specified");	
		return;
	}


	char *str_size = tracker_int_to_str (size);
	char *str_mtime = tracker_int_to_str (mtime);
	char *service = tracker_get_service_type_for_mime (mime);
	char *str_is_dir  = "0";
	
	if (is_dir) {
		str_is_dir = "1";
	}

	if (uri[0] == '/') {
		name = g_path_get_basename (uri);
		path = g_path_get_dirname (uri);
	} else {
		name = tracker_get_vfs_name (uri);
		path = tracker_get_vfs_path (uri);
	}

	tracker_exec_proc  (db_con->db, "CreateService", 8, path, name, service, str_is_dir, "0", "0", "0",  str_mtime);	

	long file_id = tracker_db_get_file_id (db_con, uri);
	char *str_file_id = tracker_long_to_str (file_id);

	if (file_id != -1) {
		tracker_exec_proc  (db_con->db, "SetMetadata", 5,  service, str_file_id, "File.Modified",  str_mtime, "1");	
		tracker_exec_proc  (db_con->db, "SetMetadata", 5,  service, str_file_id, "File.Size",  str_size, "1");
		tracker_exec_proc  (db_con->db, "SetMetadata", 5,  service, str_file_id, "File.Name",  name, "1");		
		tracker_exec_proc  (db_con->db, "SetMetadata", 5,  service, str_file_id, "File.Path",  path, "1");
		tracker_exec_proc  (db_con->db, "SetMetadata", 5,  service, str_file_id, "File.Format",  mime, "1");

	}

	g_free (service);
	g_free (str_mtime);
	g_free (str_size);
	g_free (name);
	g_free (path);
	g_free (str_file_id);
	g_free (str_is_dir);
}
	


void
tracker_dbus_method_files_delete (DBusRec *rec)
{
	DBConnection *db_con;
	char *uri = NULL, *path, *name;
	MYSQL_ROW  row = NULL;

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

	
	if (!uri)  {
		tracker_set_error (rec, "No file was specified");	
		return;
	}


	
	if (uri[0] == '/') {
		name = g_path_get_basename (uri);
		path = g_path_get_dirname (uri);
	} else {
		name = tracker_get_vfs_name (uri);
		path = tracker_get_vfs_path (uri);
	}

	long file_id = tracker_db_get_file_id (db_con, uri);
	char *str_file_id = tracker_long_to_str (file_id);
	gboolean is_dir = FALSE;

	MYSQL_RES *res = tracker_exec_proc  (db_con->db,  "GetServiceID", 2, path, name);

	if (res) {

		row = mysql_fetch_row (res);

		if (row && row[2] ) {
			is_dir = (strcmp (row[2], "1") == 0); 
		}
		mysql_free_result (res);
	}

	if (file_id != -1) {
		if (is_dir) {
			tracker_exec_proc  (db_con->db, "DeleteDirectory", 2, str_file_id, path);	
		} else {
			tracker_exec_proc  (db_con->db, "DeleteFile", 1, str_file_id);	
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
	DBusMessage *reply;
	char *uri;


/*
		<!-- Get the Service subtype for the file -->
		<method name="GetServiceType">
			<arg type="s" name="uri" direction="in" />
			<arg type="s" name="result" direction="out" />
		</method>
*/

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;
	
	dbus_message_get_args  (rec->message, NULL, 
				DBUS_TYPE_STRING, &uri, 
				DBUS_TYPE_INVALID);


	if (!uri)  {
		tracker_set_error (rec, "No file was specified");	
		return;
	}

	long id = tracker_db_get_file_id (db_con, uri);

	if (id < 1) {
		tracker_set_error (rec, "File %s was not found in Tracker's database", uri);	
		return;
	}

	char *str_id = tracker_long_to_str (id);

	char *mime = tracker_get_metadata (db_con, "Files", str_id, "File.Format"); 

	char *result = tracker_get_service_type_for_mime (mime);

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
	DBusMessage *reply;
	MYSQL_ROW  row = NULL;
	char *uri, *path, *name;
	const char *result;
	int offset, max_length;
	


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

		if (uri[0] == '/') {
			name = g_path_get_basename (uri);
			path = g_path_get_dirname (uri);
		} else {
			name = tracker_get_vfs_name (uri);
			path = tracker_get_vfs_path (uri);
		}

		char *str_offset = tracker_int_to_str (offset);
		char *str_max_length = tracker_int_to_str (max_length);

		MYSQL_RES *res = tracker_exec_proc  (db_con->db,  "GetFileContents", 4, path, name, str_offset, str_max_length);
			
		g_free (str_offset);
		g_free (str_max_length);
		g_free (path);
		g_free (name);	

		if (res) {
			row = mysql_fetch_row (res);
	
			if (row && row[0]) {
				
				result = row[0];
				reply = dbus_message_new_method_return (rec->message);
	
				dbus_message_append_args (reply,
							  DBUS_TYPE_STRING, &result,
							  DBUS_TYPE_INVALID);

				dbus_connection_send (rec->connection, reply, NULL);
				dbus_message_unref (reply);
			
			}	
			mysql_free_result (res);				

		}

	}

	

}


void
tracker_dbus_method_files_search_text_contents (DBusRec *rec)
{
	DBConnection *db_con;
	DBusMessage *reply;
	MYSQL_ROW  row = NULL;
	char *uri, *path, *name, *text;
	const char *result;
	int  max_length;
	


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
	
	dbus_message_get_args  (rec->message, NULL, 
				DBUS_TYPE_STRING, &uri, 
				DBUS_TYPE_STRING, &text,
				DBUS_TYPE_INT32, &max_length,
				DBUS_TYPE_INVALID);

	if (uri) {

		if (uri[0] == '/') {
			name = g_path_get_basename (uri);
			path = g_path_get_dirname (uri);
		} else {
			name = tracker_get_vfs_name (uri);
			path = tracker_get_vfs_path (uri);
		}


		char *str_max_length = tracker_int_to_str (max_length);

		MYSQL_RES *res = tracker_exec_proc  (db_con->db,  "SearchFileContents", 4, path, name, text, str_max_length);
			
		g_free (str_max_length);
		g_free (path);
		g_free (name);	

		if (res) {
			row = mysql_fetch_row (res);
	
			if (row && row[0]) {
				
				result = row[0];
				reply = dbus_message_new_method_return (rec->message);
	
				dbus_message_append_args (reply,
							  DBUS_TYPE_STRING, &result,
							  DBUS_TYPE_INVALID);

				dbus_connection_send (rec->connection, reply, NULL);
				dbus_message_unref (reply);
			
			}	
			mysql_free_result (res);				

		}

	}

	

}


void
tracker_dbus_method_files_get_mtime (DBusRec *rec)
{
	DBConnection *db_con;
	DBusMessage *reply;
	MYSQL_ROW  row = NULL;
	char *uri, *path, *name;
	int result;

	


/*
		<!-- returns mtime of file in seconds since epoch -->
		<method name="GetMTime">
			<arg type="s" name="uri" direction="in" />
			<arg type="i" name="result" direction="out" />
		</method>
*/

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;
	
	dbus_message_get_args  (rec->message, NULL, 
				DBUS_TYPE_STRING, &uri, 
				DBUS_TYPE_INVALID);

	if (uri) {

		if (uri[0] == '/') {
			name = g_path_get_basename (uri);
			path = g_path_get_dirname (uri);
		} else {
			name = tracker_get_vfs_name (uri);
			path = tracker_get_vfs_path (uri);
		}




		MYSQL_RES *res = tracker_exec_proc  (db_con->db,  "GetFileMTime", 2, path, name);
			

		g_free (path);
		g_free (name);	

		if (res) {
			row = mysql_fetch_row (res);
	
			if (row && row[0]) {
				
				result = atoi (row[0]);
				reply = dbus_message_new_method_return (rec->message);
	
				dbus_message_append_args (reply,
							  DBUS_TYPE_INT32, &result,
							  DBUS_TYPE_INVALID);

				dbus_connection_send (rec->connection, reply, NULL);
				dbus_message_unref (reply);
			
			}	
			mysql_free_result (res);				

		}

	}

	

}




void
tracker_dbus_method_files_get_by_service_type (DBusRec *rec)
{

	DBConnection 	*db_con;
	DBusMessage 	*reply;
	char 		*service;
	char 		**array = NULL;
	int 		limit, row_count = 0, query_id, offset;

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
		 

	dbus_message_get_args  (rec->message, NULL,   DBUS_TYPE_INT32, &query_id,
				DBUS_TYPE_STRING, &service, 
				DBUS_TYPE_INT32, &offset,
				DBUS_TYPE_INT32, &limit,
				DBUS_TYPE_INVALID);
		
	if (!tracker_is_valid_service (db_con, service)) {
		tracker_set_error (rec, "Invalid service %s or service has not been implemented yet", service);	
		return;
	}

	
	
	char *str_limit = tracker_int_to_str (limit);
	char *str_offset = tracker_int_to_str (offset);

	MYSQL_RES *res = tracker_exec_proc  (db_con->db,  "GetFilesByServiceType", 3, service, str_offset, str_limit);
		
	g_free (str_offset);
	g_free (str_limit);

				
	if (res) {
		array = tracker_get_query_result_as_array (res, &row_count);
		mysql_free_result (res);	
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

	DBConnection 	*db_con;
	DBusMessage 	*reply;
	char 		**array = NULL, **mimes = NULL;
	int 		limit, row_count = 0, query_id, offset;

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
		
	int n;

	dbus_message_get_args  (rec->message, NULL,   DBUS_TYPE_INT32, &query_id,
				DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &mimes, &n,
				DBUS_TYPE_INT32, &offset,
				DBUS_TYPE_INT32, &limit,
				DBUS_TYPE_INVALID);
		

	if (n < 1) {
		tracker_set_error (rec, "No mimes specified");	
		return;
	}

	int i;

	GString *str = g_string_new ("");
	str = g_string_append (str, mimes[0]);

	for (i=1; i<n; i++) {
		g_string_append_printf (str, ",%s", mimes[i]); 

	}


	char *str_mimes = g_string_free (str, FALSE);
	
	char *str_limit = tracker_int_to_str (limit);
	char *str_offset = tracker_int_to_str (offset);

	tracker_log ("Executing GetFilesByMimeType with param %s", str_mimes);

	MYSQL_RES *res = tracker_exec_proc  (db_con->db,  "GetFilesByMimeType", 3, str_mimes, str_offset, str_limit);
		
	g_free (str_mimes);
	g_free (str_limit);
	g_free (str_offset);
				
	if (res) {
		array = tracker_get_query_result_as_array (res, &row_count);
		mysql_free_result (res);	
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

	DBConnection 	*db_con;
	DBusMessage 	*reply;
	char 		**array = NULL, **mimes = NULL;
	int 		limit, row_count = 0, query_id, offset;

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
		
	int n;

	dbus_message_get_args  (rec->message, NULL,   DBUS_TYPE_INT32, &query_id,
				DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &mimes, &n,
				DBUS_TYPE_INT32, &offset,
				DBUS_TYPE_INT32, &limit,
				DBUS_TYPE_INVALID);
		

	char **null_array = tracker_make_array_null_terminated (mimes, n);
		
	char *keys = g_strjoinv (",", null_array);

	g_strfreev (null_array);

	char *str_mimes = g_strconcat ("'", keys, "'", NULL);
	
	g_free (keys);

	char *str_limit = tracker_int_to_str (limit);
	char *str_offset = tracker_int_to_str (offset);

	MYSQL_RES *res = tracker_exec_proc  (db_con->db,  "GetVFSFilesByMimeType", 3, str_mimes, str_offset, str_limit);
		
	g_free (str_mimes);
	g_free (str_offset);
	g_free (str_limit);
				
	if (res) {
		array = tracker_get_query_result_as_array (res, &row_count);
		mysql_free_result (res);	
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
	DBConnection 	*db_con;
	DBusMessage 	*reply;
	DBusMessageIter iter;
	DBusMessageIter iter_dict;
	int 		i, id, n, table_count, query_id;
	char 		**array, *folder,  *field, *str, *mid; 
	GString 	*sql;
	MYSQL_RES 	*res;


	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;
	
	dbus_message_get_args  (rec->message, NULL, DBUS_TYPE_INT32, &query_id, DBUS_TYPE_STRING, &folder, DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &array, &n, DBUS_TYPE_INVALID);

	id = tracker_get_file_id (db_con, folder, FALSE);	

	if (id == -1) {
		tracker_set_error (rec, "Cannot find folder %s in Tracker database", folder);
		return;
	}



	/* build SELECT clause */
	sql = g_string_new (" SELECT concat( F.Path, '/',  F.Name) as PathName ");

	table_count = 0;

	for (i=0; i<n; i++) {

		FieldDef 	*def;

		def = tracker_db_get_field_def (db_con, array[i]);
		if (def) {

			if (def->type == DATA_INDEX_STRING) {			
		
				field = g_strdup ("MetaDataIndexValue");
		
			} else if (def->type == DATA_STRING) {

				field = g_strdup ("MetaDataValue");

			} else {

				field = g_strdup ("MetaDataNumericValue");

			}
	
			tracker_db_free_field_def (def);

		} else {
			continue;	
		}

		table_count++;

		mid = tracker_int_to_str (table_count);

		str = g_strconcat (", M", mid, ".", field, NULL); 
		g_string_append (sql, str );
		
		if (field) {
			g_free (field);
		}

		g_free (mid);
		g_free (str);		

	}


	/* build FROM clause */
	g_string_append (sql, " FROM Services F ");
	
	table_count = 0;
	
	for (i=0; i<n; i++) {

		FieldDef 	*def;
		char 		*meta_id;

		def = tracker_db_get_field_def (db_con, array[i]);
		if (def) {
			meta_id = g_strdup( def->id);
			tracker_db_free_field_def (def);

		} else {
			continue;	
		}

		table_count++;

		mid = tracker_int_to_str (table_count);

		str = g_strconcat (" LEFT OUTER JOIN ServiceMetaData M", mid, " ON M", mid, ".ServiceID = F.ID ", " AND M", mid, ".MetaDataID = ", meta_id, NULL);
		g_string_append (sql, str );

		g_free (meta_id);
		g_free (mid);
		g_free (str);		

	}

	/* build WHERE clause */
	str = g_strconcat (" WHERE F.Path = '", folder, "' ", NULL); 

	g_string_append (sql, str);

	g_free (str);
	
	
	str = g_string_free (sql, FALSE);
	tracker_log (str);
	res = tracker_exec_sql (db_con->db, str);

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
		mysql_free_result (res);
	}


	dbus_message_iter_close_container (&iter, &iter_dict);

	dbus_connection_send (rec->connection, reply, NULL);

	dbus_message_unref (reply);

}





void 	
tracker_dbus_method_files_search_by_text_mime (DBusRec *rec)
{
	DBConnection *db_con;
	DBusMessage *reply;
	char **array = NULL;
	int row_count = 0, i = 0, n;
	char *str, *mime_list, *search_term = NULL;
	GString *mimes = NULL;
	gboolean use_boolean_search = TRUE;
	MYSQL_RES *res = NULL;
	MYSQL_ROW  row;


	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;
	
	dbus_message_get_args  (rec->message, NULL, DBUS_TYPE_STRING, &str, DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &array, &n, DBUS_TYPE_INVALID);

	/* build mimes string */
	for (i=0; i<n; i++) {
		if (array[i] && strlen (array[i]) > 0) {
			if (mimes) {
				g_string_append (mimes, ",");
				g_string_append (mimes, array[i]);
			} else {
				mimes = g_string_new (array[i]);
			}
			
		}
	}

	

	mime_list = g_string_free (mimes, FALSE);

	/* check search string for embedded special chars like hyphens and format appropriately */
	search_term = tracker_format_search_terms (str, &use_boolean_search);

	res = tracker_exec_proc  (db_con->db, "SearchTextMime", 2, search_term , mime_list);

	g_free (search_term);

	g_free (mime_list);

	if (res) {

		row_count = mysql_num_rows (res);

		if (row_count > 0) {
			
			array = g_new (char *, row_count);

			i = 0;

			while ((row = mysql_fetch_row (res))) {
				
				if (row && row[0] && row[1]) {
					array[i] = g_strconcat (row[0], "/",  row[1], NULL);
				}
				i++;
			}
			
		} else {
			tracker_log ("result set is empty");
		}

		mysql_free_result (res);
			
	} else {
		array = g_new (char *, 1);
		array[0] = NULL;
	}


	reply = dbus_message_new_method_return (rec->message);
	
	dbus_message_append_args (reply,
	  			  DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &array, row_count,
	  			  DBUS_TYPE_INVALID);

	for (i = 0; i < row_count; i++) {
        	g_free (array[i]);
	}

	dbus_connection_send (rec->connection, reply, NULL);
	dbus_message_unref (reply);
}

void 	
tracker_dbus_method_files_search_by_text_location (DBusRec *rec)
{
	DBConnection *db_con;
	DBusMessage *reply;
	char **array = NULL;
	int row_count = 0, i = 0;
	char *str, *location, *search_term = NULL;
	gboolean use_boolean_search = TRUE;
	MYSQL_RES *res = NULL;
	MYSQL_ROW  row;

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;
	
	dbus_message_get_args  (rec->message, NULL, DBUS_TYPE_STRING, &str, DBUS_TYPE_STRING, &location, DBUS_TYPE_INVALID);

	search_term = tracker_format_search_terms (str, &use_boolean_search);

	res = tracker_exec_proc  (db_con->db, "SearchTextLocation", 2, search_term , location);

	g_free (search_term);


	if (res) {

		row_count = mysql_num_rows (res);

		if (row_count > 0) {
			
			array = g_new (char *, row_count);

			i = 0;

			while ((row = mysql_fetch_row (res))) {
				
				if (row && row[0] && row[1]) {
					array[i] = g_strconcat (row[0], "/",  row[1], NULL);
				}
				i++;
			}
			
		} else {
			tracker_log ("result set is empty");
		}

		mysql_free_result (res);
			
	} else {
		array = g_new (char *, 1);
		array[0] = NULL;
	}


	reply = dbus_message_new_method_return (rec->message); 
	
	dbus_message_append_args (reply,
	  			  DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &array, row_count,
	  			  DBUS_TYPE_INVALID);

	for (i = 0; i < row_count; i++) {
        	g_free (array[i]);
	}

	dbus_connection_send (rec->connection, reply, NULL);
	dbus_message_unref (reply);
}

void
tracker_dbus_method_files_search_by_text_mime_location (DBusRec *rec)
{
	DBConnection *db_con;
	DBusMessage *reply;
	char **array = NULL;
	int row_count = 0, i = 0, n;
	char *str, *mime_list, *location, *search_term = NULL;
	GString *mimes = NULL;
	gboolean use_boolean_search = TRUE;
	MYSQL_RES *res = NULL;
	MYSQL_ROW  row;

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;
	
	dbus_message_get_args  (rec->message, NULL, DBUS_TYPE_STRING, &str, DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &array, &n, DBUS_TYPE_STRING, &location, DBUS_TYPE_INVALID);

	/* build mimes string */
	for (i=0; i<n; i++) {
		if (array[i] && strlen (array[i]) > 0) {
			if (mimes) {
				g_string_append (mimes, ",");
				g_string_append (mimes, array[i]);
			} else {
				mimes = g_string_new (array[i]);
			}
			//g_free (array[i]);
		}
	}

	

	mime_list = g_string_free (mimes, FALSE);

	search_term = tracker_format_search_terms (str, &use_boolean_search);

	//g_print ("search term is before %s and after %s\n\n", str, search_term);

	res = tracker_exec_proc  (db_con->db, "SearchTextLocation", 2, search_term , mime_list, location);

	g_free (search_term);

	g_free (mime_list);

	if (res) {

		row_count = mysql_num_rows (res);

		if (row_count > 0) {
			
			array = g_new (char *, row_count);

			i = 0;

			while ((row = mysql_fetch_row (res))) {
				
				if (row && row[0] && row[1]) {
					array[i] = g_strconcat (row[0], "/",  row[1], NULL);
				}
				i++;
			}
			
		} else {
			tracker_log ("result set is empty");
		}

		mysql_free_result (res);
			
	} else {
		array = g_new (char *, 1);
		array[0] = NULL;
	}



	reply = dbus_message_new_method_return (rec->message);
	
	dbus_message_append_args (reply,
	  			  DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &array, row_count,
	  			  DBUS_TYPE_INVALID);

	for (i = 0; i < row_count; i++) {
        	g_free (array[i]);
	}

	dbus_connection_send (rec->connection, reply, NULL);
	dbus_message_unref (reply);
}

