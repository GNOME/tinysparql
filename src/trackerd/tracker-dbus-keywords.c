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
#include "tracker-dbus-keywords.h"


static void
update_keywords_metadata (DBConnection 	*db_con, const char *path, const char *name) 
{
	char *tmp = g_strconcat (path, "/", name, NULL);
	char *id = tracker_db_get_id (db_con, "Files", tmp);
	int i;

	g_free (tmp);

	if (!id) {
		return;	
	}
	
	MYSQL_RES *res = tracker_exec_proc  (db_con->db,  "GetKeywords", 2, path, name);

	if (res) {

		MYSQL_ROW  row;

		GString *words = g_string_new (" ");
		
		i = 0;
		
			
				
		while ((row = mysql_fetch_row (res))) {

			if (row[0]) {

				if (i!=0) {
					words = g_string_append (words, "," );					
				} 

				words = g_string_append (words, row[0]);
				i++;
				
			}
			
		}

		mysql_free_result (res);

		char *keywords = g_string_free (words, FALSE);
		
		tracker_set_metadata (db_con, "Files", id, "File.Keywords", keywords , TRUE);

		g_free (keywords);

	
	}

	g_free (id);
}


void
tracker_dbus_method_keywords_get_list (DBusRec *rec)
{
	DBConnection 	*db_con;
	DBusMessage 	*reply;
	DBusMessageIter iter;
	DBusMessageIter iter_dict;
	char 		*service;


	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;
	
/*
		<!-- gets a list of all unique keywords/tags that are in use by the specified service irrespective of the uri or id of the entity
		     Returns dict/hashtable with the keyword as the key and the total usage count of the keyword as the variant part
		-->
		<method name="GetList">
			<arg type="s" name="service" direction="in" />
			<arg type="a{sv}" name="value" direction="out" />
		</method>
*/
		

	dbus_message_get_args  (rec->message, NULL,  DBUS_TYPE_STRING, &service,  DBUS_TYPE_INVALID);
		
	if (!tracker_is_valid_service (db_con, service)) {
		tracker_set_error (rec, "Invalid service %s or service has not been implemented yet", service);	
		return;
	}


	MYSQL_RES *res = tracker_exec_proc  (db_con->db,  "GetKeywordList", 1, service);
				
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
tracker_dbus_method_keywords_get (DBusRec *rec)
{
	DBConnection 	*db_con;
	DBusMessage 	*reply;
	char 		*uri, *service,*name, *path;
	char 		**array = NULL;
	int 		row_count = 0;

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;
	
/*
		<!-- gets all unique keywords/tags for specified service and id -->
		<method name="Get">
			<arg type="s" name="service" direction="in" />
			<arg type="s" name="id" direction="in" />
			<arg type="as" name="value" direction="out" />
		</method>
*/
		

	dbus_message_get_args  (rec->message, NULL,  DBUS_TYPE_STRING, &service, DBUS_TYPE_STRING, &uri,  DBUS_TYPE_INVALID);
		
	if (!tracker_is_valid_service (db_con, service)) {
		tracker_set_error (rec, "Invalid service %s or service has not been implemented yet", service);	
		return;
	}

	
	if (!uri || strlen (uri) == 0) {
		tracker_set_error (rec, "Uri is invalid");
		return;
	}


	if (uri[0] == '/') {
		name = g_path_get_basename (uri);
		path = g_path_get_dirname (uri);
	} else {
		name = tracker_get_vfs_name (uri);
		path = tracker_get_vfs_path (uri);
	}

	MYSQL_RES *res = tracker_exec_proc  (db_con->db,  "GetKeywords", 2, path, name);

	g_free (name);
	g_free (path);
				
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
tracker_dbus_method_keywords_add (DBusRec *rec)
{
	DBConnection 	*db_con;
	DBusMessage 	*reply;
	char 		*uri, *service, *name, *path;
	char 		**array = NULL;
	int 		row_count = 0;

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;
	
/*
		<!-- Adds new keywords/tags for specified service and id -->
		<method name="Add">
			<arg type="s" name="service" direction="in" />
			<arg type="s" name="id" direction="in" />
			<arg type="as" name="values" direction="in" />
		</method>
*/
		

	dbus_message_get_args  (rec->message, NULL,  DBUS_TYPE_STRING, &service, DBUS_TYPE_STRING, &uri, DBUS_TYPE_ARRAY, DBUS_TYPE_STRING,  &array, &row_count, DBUS_TYPE_INVALID);
		
	if (!tracker_is_valid_service (db_con, service)) {
		tracker_set_error (rec, "Invalid service %s or service has not been implemented yet", service);	
		return;
	}

	
	if (!uri || strlen (uri) == 0) {
		tracker_set_error (rec, "URI is invalid");
		return;
	}

	if (uri[0] == '/') {
		name = g_path_get_basename (uri);
		path = g_path_get_dirname (uri);
	} else {
		name = tracker_get_vfs_name (uri);
		path = tracker_get_vfs_path (uri);
	}

	int i;

	if (array && (row_count > 0)) {

		for (i = 0; i < row_count; i++) {
			if (array[i]) {
				tracker_exec_proc  (db_con->db,  "AddKeyword", 3, path, name, array[i]);	
			}
		}
	}


	update_keywords_metadata (db_con, path, name);

	g_free (name);
	g_free (path);

	reply = dbus_message_new_method_return (rec->message);
	
	dbus_connection_send (rec->connection, reply, NULL);
	
	dbus_message_unref (reply);
}

void
tracker_dbus_method_keywords_remove (DBusRec *rec)
{
	DBConnection 	*db_con;
	DBusMessage 	*reply;
	char 		*uri, *service, *name, *path;
	char 		**array = NULL;
	int 		row_count = 0;

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;
	
/*
		<!-- removes all specified keywords/tags for specified service and id -->
		<method name="Remove">
			<arg type="s" name="service" direction="in" />
			<arg type="s" name="id" direction="in" />
			<arg type="as" name="keywords" direction="in" />
		</method>
*/
		

	dbus_message_get_args  (rec->message, NULL,  DBUS_TYPE_STRING, &service, DBUS_TYPE_STRING, &uri, DBUS_TYPE_ARRAY, DBUS_TYPE_STRING,  &array, &row_count, DBUS_TYPE_INVALID);
		
	if (!tracker_is_valid_service (db_con, service)) {
		tracker_set_error (rec, "Invalid service %s or service has not been implemented yet", service);	
		return;
	}



	
	if (!uri || strlen (uri) == 0) {
		tracker_set_error (rec, "ID is invalid");
		return;
	}


	if (uri[0] == '/') {
		name = g_path_get_basename (uri);
		path = g_path_get_dirname (uri);
	} else {
		name = tracker_get_vfs_name (uri);
		path = tracker_get_vfs_path (uri);
	}

	int i;

	if (array && (row_count > 0)) {

		for (i = 0; i < row_count; i++) {
			if (array[i]) {
				tracker_exec_proc  (db_con->db,  "RemoveKeyword", 3, path, name, array[i]);	
			}
		}
	}

	update_keywords_metadata (db_con, path, name);

	g_free (name);
	g_free (path);

	reply = dbus_message_new_method_return (rec->message);
	
	dbus_connection_send (rec->connection, reply, NULL);
	
	dbus_message_unref (reply);
}

void
tracker_dbus_method_keywords_remove_all (DBusRec *rec)
{
	DBConnection 	*db_con;
	DBusMessage 	*reply;
	char 		*uri, *service,  *name, *path;


	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;
	
/*

		<!-- removes all keywords/tags for specified service and id -->
		<method name="RemoveAll">
			<arg type="s" name="service" direction="in" />
			<arg type="s" name="id" direction="in" />
		</method>
*/
		

	dbus_message_get_args  (rec->message, NULL,  DBUS_TYPE_STRING, &service, DBUS_TYPE_STRING, &uri, DBUS_TYPE_INVALID);
		
	if (!tracker_is_valid_service (db_con, service)) {
		tracker_set_error (rec, "Invalid service %s or service has not been implemented yet", service);	
		return;
	}


	
	if (!uri || strlen (uri) == 0) {
		tracker_set_error (rec, "URI is invalid");
		return;
	}

	if (uri[0] == '/') {
		name = g_path_get_basename (uri);
		path = g_path_get_dirname (uri);
	} else {
		name = tracker_get_vfs_name (uri);
		path = tracker_get_vfs_path (uri);
	}

	tracker_exec_proc  (db_con->db,  "RemoveAllKeywords", 2, path, name);	

	update_keywords_metadata (db_con, path, name);

	g_free (name);
	g_free (path);

	reply = dbus_message_new_method_return (rec->message);
	
	dbus_connection_send (rec->connection, reply, NULL);
	
	dbus_message_unref (reply);
}



void
tracker_dbus_method_keywords_search (DBusRec *rec)
{
	DBConnection 	*db_con;
	DBusMessage 	*reply;
	char 		*service;
	char 		**array = NULL;
	int 		row_count = 0, limit, query_id;
	MYSQL_RES 	*res;

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;
	
/*
		<!-- searches specified service for matching keyword/tag and returns an array of matching id values for the service-->
		<method name="Search">
			<arg type="i" name="live_query_id" direction="in" />
			<arg type="s" name="service" direction="in" />
			<arg type="as" name="keywords" direction="in" />
			<arg type="i" name="max_hits" direction="in" />
			<arg type="as" name="result" direction="out" />
		</method>
*/
		

	dbus_message_get_args  (rec->message, NULL, DBUS_TYPE_INT32, &query_id, DBUS_TYPE_STRING, &service, DBUS_TYPE_ARRAY, DBUS_TYPE_STRING,  &array, &row_count, DBUS_TYPE_INT32, &limit,  DBUS_TYPE_INVALID);
		
	if (!tracker_is_valid_service (db_con, service)) {
		tracker_set_error (rec, "Invalid service %s or service has not been implemented yet", service);	
		return;
	}

	
	
	if (row_count < 1) {

		tracker_set_error (rec, "No keywords supplied");
		return;
	}

	if (row_count == 1) {
		res = tracker_exec_proc  (db_con->db,  "SearchKeywords", 3, service, array[0], limit);
	} else {
		char **null_array = tracker_make_array_null_terminated (array, row_count);
		
		char *keys = g_strjoinv (",", null_array);

		g_strfreev (null_array);

		char *limit_str = tracker_int_to_str (limit);

		char *query = g_strconcat ("Select Concat(S.Path, '/', S.Name) as EntityName from Services S, ServiceKeywords K where K.ServiceID = S.ID AND (S.ServiceTypeID between pMinServiceTypeID and pMaxServiceTypeID) and FIND_IN_SET(K.Keyword, '", keys, "') limit ", limit_str, NULL);

		g_free (limit_str);
		res = tracker_exec_sql (db_con->db, query);

		g_free (query);
		g_free (keys);

	}		



	
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
