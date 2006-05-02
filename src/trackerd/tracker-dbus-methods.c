/* Tracker
 * Copyright (C) 2005, Mr Jamie McCracken (jamiemcc@gnome.org)
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
tracker_add_metadata_to_dict (MYSQL_RES *res, DBusMessageIter *iter_dict, int metadata_count)
{
					
	char *key, *value;
	int i, row_count;
	MYSQL_ROW  row = NULL;

	row_count = mysql_num_rows (res);

	if (row_count > 0) {

		while ((row = mysql_fetch_row (res)) != NULL) {

			if (row[0] && row[1] &&row[2]) {
				key = g_strconcat (row[1], "/",  row[2], NULL);
			} else {
				continue;
			}
			
			DBusMessageIter iter_dict_entry;

			dbus_message_iter_open_container (iter_dict,
						  	  DBUS_TYPE_DICT_ENTRY,
						  	  NULL,
							  &iter_dict_entry);
			
			dbus_message_iter_append_basic (&iter_dict_entry, DBUS_TYPE_STRING, &key);

			g_free (key);

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


			/* append requested metadata */
			for (i = 3; i < (metadata_count + 3); i++) {

				/* dbus does not like NULLs */
				value = g_strdup (" ");

				if (row[i]) {
					value = g_strdup (row[i]);
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

	mysql_free_result (res);

}

static char *
format_search_terms (const char *str, gboolean *do_bool_search)
{

	*do_bool_search = FALSE;

	/* if already has quotes then do nothing */
	if (strchr (str, '"') || strchr (str, '*')) {
		*do_bool_search = TRUE;
		return g_strdup (str);
	}

	if (!strstr (str, "-")) {
		return g_strdup (str);
	}

	char **terms = g_strsplit (str, " ", -1);
	
	if (terms) {
		GString *search_term = g_string_new (" ");
		char **st;
		for (st = terms; *st; st++) {
			if (strchr (*st, '-')) {
				*do_bool_search = TRUE;
				char *st_1 = g_strconcat ("\"", *st, "\"", NULL);
				g_string_append (search_term, st_1);
				g_free (st_1);
			} else {
				g_string_append (search_term, *st);
			}
		}
		g_strfreev (terms);
		return g_string_free (search_term, FALSE);		
	}

	return g_strdup (str);
}





void
tracker_dbus_method_get_metadata_for_files_in_folder (DBusRec *rec)
{
	DBConnection 	*db_con;
	DBusMessage 	*reply;
	DBusMessageIter iter;
	DBusMessageIter iter_dict;
	int 		i, id, n, table_count;
	char 		**array, *folder,  *field, *str, *mid; 
	GString 	*sql;
	MYSQL_RES 	*res;


	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;
	
	dbus_message_get_args  (rec->message, NULL, DBUS_TYPE_STRING, &folder, DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &array, &n, DBUS_TYPE_INVALID);

	id = tracker_get_file_id (db_con, folder, FALSE);	

	if (id == -1) {
		tracker_set_error (rec, "Cannot find folder %s in Tracker database", folder);
		return;
	}



	/* build SELECT clause */
	sql = g_string_new (" SELECT DISTINCT F.ID, F.Path, F.Name ");

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
		tracker_add_metadata_to_dict (res, &iter_dict, n);
	}


	dbus_message_iter_close_container (&iter, &iter_dict);

	dbus_connection_send (rec->connection, reply, NULL);

	dbus_message_unref (reply);

}



void
tracker_dbus_method_search_metadata_text (DBusRec *rec)
{
	DBConnection *db_con;
	DBusMessage *reply;
	char **array = NULL;	
	int row_count = 0, i = 0;
	int limit;
	char *str = NULL, *service = NULL, *search_term = NULL;
	gboolean sort_results = FALSE, use_boolean_search = FALSE;
	MYSQL_RES *res = NULL;
	MYSQL_ROW  row;

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;
	
	dbus_message_get_args  (rec->message, NULL, DBUS_TYPE_STRING, &service,
				DBUS_TYPE_STRING, &str,  DBUS_TYPE_INT32, &limit,
				DBUS_TYPE_BOOLEAN, &sort_results,
				DBUS_TYPE_INVALID);

	


	if (!service)  {
		tracker_set_error (rec, "No service was specified");	
		return;
	}

	if (strcmp (service, "Files") != 0) {
		tracker_set_error (rec, "Invalid service %s or service has not been implemented yet", service);	
		return;
	}

	if ( !str || strlen (str) == 0) {
		tracker_set_error (rec, "No search term was specified");	
		return;
	}

	if (limit < 0) {
		limit = 1024;
	}

	/* check search string for embedded special chars like hyphens and format appropriately */
	search_term = format_search_terms (str, &use_boolean_search);

	char *str_limit = tracker_int_to_str (limit);
	char *str_sort = tracker_int_to_str (sort_results);
	char *str_bool =  tracker_int_to_str (use_boolean_search);
	
	tracker_log ("Executing search with params %s, %s. %s, %s, %s", service, search_term, str_limit, str_sort, str_bool );
	
	res = tracker_exec_proc  (db_con->db, "SearchText", 5, service, search_term, str_limit, str_sort, str_bool);	

	g_free (search_term);
	g_free (str_limit);
	g_free (str_bool);
	g_free (str_sort);
	
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
			tracker_log ("search returned no results" );
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

	g_free (array);

	dbus_connection_send (rec->connection, reply, NULL);
	dbus_message_unref (reply);
}

void 	
tracker_dbus_method_search_files_by_text_mime (DBusRec *rec)
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
	search_term = format_search_terms (str, &use_boolean_search);

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
tracker_dbus_method_search_files_by_text_location (DBusRec *rec)
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

	search_term = format_search_terms (str, &use_boolean_search);

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
tracker_dbus_method_search_files_by_text_mime_location (DBusRec *rec)
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

	search_term = format_search_terms (str, &use_boolean_search);

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

void
tracker_dbus_method_search_files_query (DBusRec *rec)
{
	DBConnection *db_con;
	DBusMessage *reply;
	char **array = NULL;				
	int row_count = 0, i = 0, table_count, limit;
	MYSQL_ROW  row = NULL;
	char *str, *str2, *query, *limit_str;
	GString *sql;
	gboolean sort_results;

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;
	
	dbus_message_get_args  (rec->message, NULL, DBUS_TYPE_STRING, &query, DBUS_TYPE_INT32, &limit,
				DBUS_TYPE_BOOLEAN, &sort_results, DBUS_TYPE_INVALID);

	if (limit < 0) {
		limit = 1024;
	}


	if (query) {

		tracker_log ("executing rdf query %s", query);

		table_count = tracker_rdf_query_parse (db_con, query);

		if (table_count > 0) {
	
			sql = g_string_new (" CREATE Temporary Table TMP0 ENGINE = MEMORY SELECT T1.ServiceID FROM ");

			for (i=1; i<table_count+1; i++) {
			
				str = g_strdup_printf ("%d", i);

				if (i != table_count) {
					str2 = g_strconcat ("TMP", str, " T", str, ", ", NULL);
				} else {
					str2 = g_strconcat ("TMP", str, " T", str, NULL);
				}
				g_string_append (sql, str2);
				g_free (str);
				g_free (str2);
			}

			for (i=2; i<table_count+1; i++) {
			
				str = g_strdup_printf ("%d", i);

				if (i == 2) {
					g_string_append (sql, " WHERE ");
				}

				if (i != table_count) {
					str2 = g_strconcat ("T1.ServiceID = T", str, ".ServiceID and ", NULL);
				} else {
					str2 = g_strconcat ("T1.ServiceID = T", str, ".ServiceID", NULL);
				}
				g_string_append (sql, str2);
				g_free (str);
				g_free (str2);
			}


			str = g_string_free (sql, FALSE);
			
			tracker_exec_sql (db_con->db, str);
			
			g_free (str);

			/* drop temporary tables */
			for (i=1; i<table_count+1; i++) {
			
				str = g_strdup_printf ("%d", i);
				str2 = g_strconcat ("DROP TEMPORARY TABLE TMP", str, NULL);
				tracker_exec_sql (db_con->db, str2);
				g_free (str);
				g_free (str2);
			}

			limit_str = g_strdup_printf ("%d", limit);
			str = g_strconcat ("select F.Path, F.Name from Services F, TMP0 T where F.ID = T.ServiceID LIMIT ", limit_str, NULL);
			g_free (limit_str);
	
			MYSQL_RES *res = tracker_exec_sql (db_con->db, str);
				
			g_free (str);

			if (res) {
				row_count = mysql_num_rows (res);
				if (row_count > 0) {
					array = g_new (char *, row_count);
					i = 0;

					while ((row = mysql_fetch_row (res)) != NULL) {

						if (row[0] && row[1]) {
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

			str = g_strdup ("DROP TEMPORARY TABLE TMP0");

			tracker_exec_sql (db_con->db, str);
				
			g_free (str);

		}

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
