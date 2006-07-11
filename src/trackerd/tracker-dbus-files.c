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
	int 		limit, row_count = 0;

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;
	
/*
		<!--
		Retrieves all files that match a service description
		-->
		<method name="GetByServiceType">
			<arg type="s" name="file_service" direction="in" />
			<arg type="i" name="max_hits" direction="in" />
			<arg type="as" name="result" direction="out" />
		</method>
*/
		

	dbus_message_get_args  (rec->message, NULL,  
				DBUS_TYPE_STRING, &service, 
				DBUS_TYPE_INT32, &limit,
				DBUS_TYPE_INVALID);
		
	if (!tracker_is_valid_service (db_con, service)) {
		tracker_set_error (rec, "Invalid service %s or service has not been implemented yet", service);	
		return;
	}

	
	
	char *str_limit = tracker_int_to_str (limit);

	MYSQL_RES *res = tracker_exec_proc  (db_con->db,  "GetFilesByServiceType", 2, service, str_limit);
		

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
	char 		*service;
	char 		**array = NULL, *mimes = NULL;
	int 		limit, row_count = 0;

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;
	
/*
		<!-- Retrieves all non-vfs files of the specified mime type(s) -->
		<method name="GetByMimeType">
			<arg type="as" name="mime_types" direction="in" />
			<arg type="i" name="max_hits" direction="in" />
			<arg type="as" name="result" direction="out" />
		</method>	
*/
		
	int n;

	dbus_message_get_args  (rec->message, NULL,  
				DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &mimes, &n,
				DBUS_TYPE_INT32, &limit,
				DBUS_TYPE_INVALID);
		
	if (!tracker_is_valid_service (db_con, service)) {
		tracker_set_error (rec, "Invalid service %s or service has not been implemented yet", service);	
		return;
	}

	char **null_array = tracker_make_array_null_terminated (mimes, n);
		
	char *keys = g_strjoinv (",", null_array);

	g_strfreev (null_array);

	char *str_mimes = g_strconcat ("'", keys, "'", NULL);
	
	g_free (keys);

	char *str_limit = tracker_int_to_str (limit);

	MYSQL_RES *res = tracker_exec_proc  (db_con->db,  "GetFilesByMimeType", 2, str_mimes, str_limit);
		
	g_free (str_mimes);
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
tracker_dbus_method_files_get_by_mime_type_vfs (DBusRec *rec)
{

	DBConnection 	*db_con;
	DBusMessage 	*reply;
	char 		*service;
	char 		**array = NULL, *mimes = NULL;
	int 		limit, row_count = 0;

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;
	
/*
		<!-- Retrieves all vfs files of the specified mime type(s) -->
		<method name="GetByMimeTypeVFS">
			<arg type="as" name="mime_types" direction="in" />
			<arg type="i" name="max_hits" direction="in" />
			<arg type="as" name="result" direction="out" />
		</method>
*/
		
	int n;

	dbus_message_get_args  (rec->message, NULL,  
				DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &mimes, &n,
				DBUS_TYPE_INT32, &limit,
				DBUS_TYPE_INVALID);
		
	if (!tracker_is_valid_service (db_con, service)) {
		tracker_set_error (rec, "Invalid service %s or service has not been implemented yet", service);	
		return;
	}

	char **null_array = tracker_make_array_null_terminated (mimes, n);
		
	char *keys = g_strjoinv (",", null_array);

	g_strfreev (null_array);

	char *str_mimes = g_strconcat ("'", keys, "'", NULL);
	
	g_free (keys);

	char *str_limit = tracker_int_to_str (limit);

	MYSQL_RES *res = tracker_exec_proc  (db_con->db,  "GetVFSFilesByMimeType", 2, str_mimes, str_limit);
		
	g_free (str_mimes);
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

