/* Tracker
 * Copyright (C) 2005, Mr Jamie McCracken (jamiemcc@gnome.org)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,i
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


#include "tracker-dbus-methods.h"
#include "tracker-metadata.h"
#include "tracker-rdf-query.h"


void
tracker_set_error (DBusRec 	  *rec,
	   	   const char	  *fmt, 
	   	   ...)
{
	char *msg;
    	va_list args;
	DBusMessage *reply;

	va_start (args, fmt);
  	msg = g_strdup_vprintf (fmt, args);
  	va_end (args);

	reply = dbus_message_new_error (rec->message,
					"org.freedesktop.Tracker.Error",
					msg);

	if (reply == NULL || !dbus_connection_send (rec->connection, reply, NULL)) {
		tracker_log ("Warning - out of memory");
	}
	
	tracker_log ("The following error has happened : %s", msg);
	g_free (msg);

	dbus_message_unref (reply);
}



char *
tracker_get_metadata (DBConnection *db_con, const char *service, const char *id, const char *key) 
{

	int 	 	row_count = 0;
	char 		*value;
	MYSQL_RES 	*res = NULL;
	MYSQL_ROW  	row;


	g_return_val_if_fail (db_con && id && (strlen (id) > 0), NULL);

	value = g_strdup (" ");

	res = tracker_exec_proc  (db_con->db, "GetMetadata", 3, service, id, key);	

	if (res) {

		row_count = mysql_num_rows (res);
	
		if (row_count > 0) {

			row = mysql_fetch_row (res);

			if (row && row[0]) {
				g_free (value);
				value = g_strdup (row[0]);				
			}
			
		} else {
			tracker_log ("result set is empty");
		}

		mysql_free_result (res);
	} 

	tracker_log ("metadata %s is %s", key, value);
	return value;

}


void  
tracker_set_metadata (DBConnection *db_con, const char *service, const char *id, const char *key, const char *value, gboolean overwrite)
{
	char *str_write;	

	if (overwrite) {
		str_write = "1";
	} else {
		str_write = "0";
	}

	tracker_exec_proc  (db_con->db, "SetMetadata", 5, service, id, key, value, str_write);	
	

}




int
tracker_get_file_id (DBConnection *db_con, const char *uri, gboolean create_record)
{
	int 		id, result;
	char 		*path, *name;
	struct stat     finfo;

	g_return_val_if_fail (db_con && uri && (strlen(uri) > 0), -1);

	id = tracker_db_get_file_id (db_con, uri);

	if (id == -1 && create_record) {

		/* file not found in DB - so we must insert a new file record */



		if (uri[0] == '/' && (lstat (uri, &finfo) != -1)) {
			
			char *is_dir, *is_link;

			name = g_path_get_basename (uri);
			path = g_path_get_dirname (uri);

			if (S_ISDIR (finfo.st_mode)) {
				is_dir = "1";
			} else {
				is_dir = "0";
			}
	 

			if (S_ISLNK (finfo.st_mode)) {
				is_link = "1";
			} else {
				is_link = "0";
			}

			char *mime = tracker_get_mime_type (uri);

			char *service_name = tracker_get_service_type_for_mime (mime);

			char *str_mtime = g_strdup_printf ("%ld", finfo.st_mtime);

			tracker_exec_proc  (db_con->db, "CreateService", 8, path, name, service_name, is_dir, is_link, "0", "0", str_mtime);

			g_free (service_name);

			g_free (mime);

			g_free (str_mtime);

			result = tracker_db_get_file_id (db_con, uri);
			

		} else {

			/* we assume its a non-local vfs so dont care about whether its a dir or a link */

			name = tracker_get_vfs_name (uri);
			path = tracker_get_vfs_path (uri);

			tracker_exec_proc  (db_con->db, "CreateService", 8, path, name, "VFSFiles", "0", "0", "0", "0", "unknown");

			result = tracker_db_get_file_id (db_con, uri);
		}


		id = result;
		
		g_free (path);
		g_free (name);
	}

	return id;
}




void
tracker_add_query_result_to_dict (MYSQL_RES *res, DBusMessageIter *iter_dict)
{
					
	char *key, *value;
	int i, row_count, field_count;
	MYSQL_ROW  row = NULL;

	g_return_if_fail (res);

	row_count = mysql_num_rows (res);

	if (row_count > 0) {

		field_count =  mysql_num_fields (res);

		while ((row = mysql_fetch_row (res)) != NULL) {

			if (row[0]) {
				key = row[0];
			} else {
				continue;
			}
			
			DBusMessageIter iter_dict_entry;

			dbus_message_iter_open_container (iter_dict,
						  	  DBUS_TYPE_DICT_ENTRY,
						  	  NULL,
							  &iter_dict_entry);
			
			dbus_message_iter_append_basic (&iter_dict_entry, DBUS_TYPE_STRING, &key);


			DBusMessageIter iter_var, iter_array;

			dbus_message_iter_open_container (&iter_dict_entry,
							  DBUS_TYPE_VARIANT,
							  DBUS_TYPE_ARRAY_AS_STRING
							  DBUS_TYPE_STRING_AS_STRING,
							  &iter_var);

			dbus_message_iter_open_container (&iter_var,
							  DBUS_TYPE_ARRAY,
							  DBUS_TYPE_STRING_AS_STRING,
							  &iter_array);
	
			/* append additional fields to the variant */
			for (i = 1; i < (field_count); i++) {

				
				if (row[i]) {
					value = g_strdup (row[i]);
				} else {
					/* dbus does not like NULLs */
					value = g_strdup (" ");
				}

				dbus_message_iter_append_basic (&iter_array, 
								DBUS_TYPE_STRING,
								&value);

				g_free (value);
			}

			dbus_message_iter_close_container (&iter_var, &iter_array);
			dbus_message_iter_close_container (&iter_dict_entry, &iter_var);
			dbus_message_iter_close_container (iter_dict, &iter_dict_entry);
		}	
		


	} else {
		tracker_log ("result set is empty");
	}


}


char **
tracker_get_query_result_as_array (MYSQL_RES *res, int *row_count)
{
					
	char **array;
	int i;
	MYSQL_ROW  row = NULL;

	*row_count = mysql_num_rows (res);

	if (*row_count > 0) {

		array = g_new (char *, *row_count);

		i = 0;

		while ((row = mysql_fetch_row (res))) {
				
			if (row && row[0]) {
				array[i] = g_strdup (row[0]);
			} else {
				array[i] = NULL;
			}
			i++;
		}
			
	
	} else {
		array = g_new (char *, 1);
		array[0] = NULL;
	}

	return array;

}











void
tracker_dbus_method_get_services (DBusRec *rec)
{
	DBConnection *db_con;
	DBusMessage *reply;
	DBusMessageIter iter;
	DBusMessageIter iter_dict;
	gboolean main_only;
	MYSQL_RES *res = NULL;

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;
	
	tracker_log ("Executing GetServices Dbus Call");
	
	dbus_message_get_args  (rec->message, NULL, DBUS_TYPE_BOOLEAN, &main_only, DBUS_TYPE_INVALID);
	
	if (main_only) {
		res = tracker_exec_proc  (db_con->db, "GetServices", 1, "1" );	
	} else {
		res = tracker_exec_proc  (db_con->db, "GetServices", 1, "0");	
	}

	
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
tracker_dbus_method_get_stats (DBusRec *rec)
{
	DBConnection *db_con;
	DBusMessage *reply;
	DBusMessageIter iter;
	DBusMessageIter iter_dict;
	MYSQL_RES *res = NULL;

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;
	
	tracker_log ("Executing GetStats Dbus Call");
	
	res = tracker_exec_proc  (db_con->db, "GetStats", 0);	
	
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
tracker_dbus_method_get_version (DBusRec *rec)
{
	DBusMessage *reply;
	int i;	

	g_return_if_fail (rec && rec->user_data);

	i = TRACKER_VERSION_INT;

	reply = dbus_message_new_method_return (rec->message);

	dbus_message_append_args (reply,
	  			  DBUS_TYPE_INT32, 
				  &i,
	  			  DBUS_TYPE_INVALID);
	
	dbus_connection_send (rec->connection, reply, NULL);

	dbus_message_unref (reply);

}

