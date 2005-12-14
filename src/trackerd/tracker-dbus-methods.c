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
#include "tracker-rdf-query.h"

#define DATA_STRING_INDEXABLE 3


static int
get_row_count (char ***result)
{
	char ***rows;
	int i;

	if (!result) {
		return 0;
	}

	i = 0;

	for (rows = result; *rows; rows++) {
		i++;			
	}

	return i;

}

static void
result_to_file_array (char ***result)
{
	char ***rows;
	char **row;
	char *str = NULL, *tmp = NULL, *value;
	
	if (!result) {
		return;
	}

	for (rows = result; *rows; rows++) {
		if (*rows) {
			for (row = *rows; *row; row++) {
				value = *row;
				if (!value) {
					value = "NULL";
				}
				if (str) {
					tmp = g_strdup (str);
					g_free (str);
					str = g_strconcat (tmp, ", ", value, NULL);
					g_free (tmp);
				} else {
					str = g_strconcat (value, NULL);
				} 	
			}
			tracker_log (str);
			g_free (str);
			str = NULL;
			
		}
	}

}

static void
set_error (DBusRec 	  *rec,
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
					"org.freedesktop.tracker.error",
					msg);

	if (reply == NULL || !dbus_connection_send (rec->connection, reply, NULL)) {
		tracker_log ("Warning - out of memory");
	}
	
	tracker_log ("The following error has happened : %s", msg);
	g_free (msg);

	dbus_message_unref (reply);
}

static int
get_file_id (DBConnection *db_con, const char *uri, gboolean create_record)
{
	int 		id, result;
	char 		*path, *name;
	struct stat     finfo;

	g_return_val_if_fail (db_con && uri && (strlen(uri) > 0), -1);

	id = tracker_db_get_file_id (db_con, uri);

	if (id == -1 && create_record) {

		/* file not found in DB - so we must insert a new file record */

		name = g_path_get_basename (uri);
		path = g_path_get_dirname (uri);

		if (uri[0] == '/' && (lstat (uri, &finfo) != -1)) {
			
			char *is_dir, *is_link;

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

				
			result = tracker_db_exec_stmt (db_con->insert_file_stmt, 6, path, name, "0",
						       is_dir, is_link, "0");


		} else {
			/* we assume its a non-local vfs so dont care about whether its a dir or a link */
			result = tracker_db_exec_stmt (db_con->insert_file_stmt, 6, path, name, "1",
						       "0", "0", "0");
		}


		if (result == 1) {
			/* get file_ID of saved file */
			id = (int) mysql_stmt_insert_id (db_con->insert_file_stmt);
		}
		
		g_free (path);
		g_free (name);
	}

	return id;
}

static char *
get_metadata (DBConnection *db_con, const char *file_id, const char *meta) 
{
	FieldDef 	*def;
	int 	 	row_count = 0;
	char 		**row;	
	char 		***res_str = NULL;
	char 		*value;

	g_return_val_if_fail (db_con && file_id && (strlen (file_id) > 0), NULL);

	value = g_strdup (" ");

	def = tracker_db_get_field_def (db_con, meta);
	if (!def) {

		return value;
	}

	if (def->indexable) {

		res_str = tracker_db_exec_stmt_result (db_con->select_metadata_indexed_stmt, 2, file_id, def->id);

	} else if (def->type != DATA_INTEGER) {

		res_str = tracker_db_exec_stmt_result (db_con->select_metadata_stmt, 2, file_id, def->id);

	} else {

		res_str = tracker_db_exec_stmt_result  (db_con->select_metadata_integer_stmt, 2, file_id, def->id);

	}


	tracker_db_free_field_def (def);

	if (res_str) {

		row_count = get_row_count (res_str);

		if (row_count > 0) {

			row = *res_str;
			if (row && row[0]) {
				g_free (value);
				value = g_strdup (row[0]);				
			}
			
		} else {
			tracker_log ("result set is empty");
		}

		tracker_db_free_result (res_str);	
	} 

	tracker_log ("metadata %s is %s", meta, value);
	return value;

}


static void
set_metadata (DBConnection *db_con, const char *file_id, const char *meta, const char *value)
{
	FieldDef 	*def;
	int		result;

	g_return_if_fail (db_con && file_id && (strlen (file_id) > 0) && meta && (strlen (meta) > 0));

	def = tracker_db_get_field_def (db_con, meta);
	if (!def) {

		/* metadata field not found in database so we must create it */
		result = tracker_db_exec_stmt (db_con->insert_metadata_type_stmt, 4, meta, "0", "0","1" );
		if (result != 1) {
			return;
		} else {
			def = tracker_db_get_field_def (db_con, meta);
		}
	}


	/* save metadata based on type and whether metadata is indexable */
	if (def->indexable) {

		tracker_db_exec_stmt (db_con->insert_metadata_indexed_stmt, 3, file_id, def->id, value);

	} else if (def->type != DATA_INTEGER) {

		tracker_db_exec_stmt (db_con->insert_metadata_stmt, 3, file_id, def->id, value);

	} else {

		tracker_db_exec_stmt (db_con->insert_metadata_integer_stmt, 3, file_id, def->id, value);

	}

	tracker_db_free_field_def (def);

}

static char **
get_files_in_folder (DBConnection *db_con, const char *folder_uri)
{
	char 		***rows, ***res_str = NULL;
	char 		**row, **array = NULL;	
	int 		i, row_count;

	g_return_val_if_fail (db_con && folder_uri && (strlen (folder_uri) > 0), NULL);

	res_str = tracker_db_exec_stmt_result (db_con->select_file_child_stmt, 1, folder_uri);

	if (res_str) {

		row_count = get_row_count (res_str);
		tracker_log ("got row count %d", row_count);		
		if (row_count > 0) {

			array = g_new (char *, row_count +1);

			i = 0;

			for (rows = res_str; *rows; rows++) {
				row = *rows;
				if (row  && row[1] && row[2]) {
					array[i] = g_build_filename (row[1], row[2], NULL);
					tracker_log ("found file %s", array[i]);
				} else {
					array[i] = NULL;
				}
				i++;
			}
			array [row_count] = NULL;
			
		} else {
			tracker_log ("result set is empty");
			array = g_new (char *, 1);
			array[0] = NULL;
		}

		tracker_db_free_result (res_str);
			
	} else {
		array = g_new (char *, 1);
		array[0] = NULL;
	}

	return array;

} 

static void
add_metadata_to_dict (MYSQL_RES *res, DBusMessageIter *iter_dict, int metadata_count)
{
					
	char *key, *value;
	int i, row_count;
	MYSQL_ROW  row = NULL;

	row_count = mysql_num_rows (res);

	if (row_count > 0) {

		while (row = mysql_fetch_row (res)) {

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


void
tracker_dbus_method_get_metadata (DBusRec *rec)
{
	DBConnection 	*db_con;
	DBusMessage 	*reply;
	char 		*uri, *meta, *file_id, *value = NULL;
	int		id;


	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;
	
	dbus_message_get_args  (rec->message, NULL, DBUS_TYPE_STRING, &uri, DBUS_TYPE_STRING, &meta, DBUS_TYPE_INVALID);

	id = tracker_db_get_file_id (db_con, uri);
	if (id == -1) {

		set_error (rec, "File %s not found in DB", uri);
		return;		
	}
	
	file_id = g_strdup_printf ("%d", id);

	value = get_metadata (db_con, file_id, meta);

	g_free (file_id);

	reply = dbus_message_new_method_return (rec->message);
	
	dbus_message_append_args (reply, DBUS_TYPE_STRING, &value, DBUS_TYPE_INVALID);

	if (value) {
	       	g_free (value);
	}

	dbus_connection_send (rec->connection, reply, NULL);
	dbus_message_unref (reply);
}


void
tracker_dbus_method_set_metadata (DBusRec *rec)
{
	DBConnection 	*db_con;
	DBusMessage 	*reply;
	int 	 	id;
	char 		*uri, *meta, *file_id, *value;


	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;
	
	dbus_message_get_args  (rec->message, NULL, DBUS_TYPE_STRING, &uri, DBUS_TYPE_STRING, &meta, DBUS_TYPE_STRING, &value, DBUS_TYPE_INVALID);
		
	if (!uri || strlen (uri) == 0) {
		set_error (rec, "URI is invalid");
		return;
	}

	if (!meta || strlen (meta) < 3 || (strchr (meta, '.') == NULL) ) {
		set_error (rec, "Metadata type name is invalid. All names must be in the format 'class.name' ");
		return;
	}

	id = get_file_id (db_con, uri, TRUE);	

	if (id == -1) {
		set_error (rec, "Cannot find or create file");
		return;
	}

	file_id = g_strdup_printf ("%d", id);
	
	set_metadata (db_con, file_id, meta, value);

	g_free (file_id);

	reply = dbus_message_new_method_return (rec->message);
	
	dbus_connection_send (rec->connection, reply, NULL);
	
	dbus_message_unref (reply);
	
}


void
tracker_dbus_method_register_metadata_type (DBusRec *rec)
{
	DBConnection 	*db_con;
	DBusMessage 	*reply;
	char 		*meta, *type_id;
	int		type;


	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;
	
	dbus_message_get_args  (rec->message, NULL, DBUS_TYPE_STRING, &meta, DBUS_TYPE_INT32, &type, DBUS_TYPE_INVALID);
		
	if (!meta || strlen (meta) < 3 || (strchr (meta, '.') == NULL) ) {
		set_error (rec, "Metadata type name is invalid. All names must be in the format 'class.name' ");
		return;
	}

	
	if (type < 0 || type > 3) {
		set_error (rec, "Invalid metadata type id specified");
		return;
	}

	type_id = g_strdup_printf ("%d", type);

	if (type == DATA_STRING_INDEXABLE) {
		tracker_db_exec_stmt (db_con->insert_metadata_type_stmt, 4, meta, "0", "1", "1");
	} else {
		tracker_db_exec_stmt (db_con->insert_metadata_type_stmt, 4, meta, type_id, "0", "1");
	}

	g_free (type_id);

	reply = dbus_message_new_method_return (rec->message);
	
	dbus_connection_send (rec->connection, reply, NULL);
	
	dbus_message_unref (reply);
	
	
}


void
tracker_dbus_method_get_metadata_for_files_in_folder (DBusRec *rec)
{
	DBConnection 	*db_con;
	DBusMessage 	*reply;
	DBusMessageIter iter;
	DBusMessageIter iter_dict;
	int 		i, id, n, table_count;
	char 		**array, **values = NULL, *folder,  *field, *str, *mid; 
	GString 	*sql;
	MYSQL_RES 	*res;


	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;
	
	dbus_message_get_args  (rec->message, NULL, DBUS_TYPE_STRING, &folder, DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &array, &n, DBUS_TYPE_INVALID);

	id = get_file_id (db_con, folder, FALSE);	

	if (id == -1) {
		set_error (rec, "Cannot find folder %s in Tracker database", folder);
		return;
	}



	/* build SELECT clause */
	sql = g_string_new (" SELECT DISTINCT F.ID, F.Path, F.FileName ");

	table_count = 0;

	for (i=0; i<n; i++) {

		FieldDef 	*def;

		def = tracker_db_get_field_def (db_con, array[i]);
		if (def) {

			if (def->indexable) {			
		
				field = g_strdup ("MetaDataIndexValue");
		
			} else if (def->type != DATA_INTEGER) {

				field = g_strdup ("MetaDataValue");

			} else {

				field = g_strdup ("MetaDataIntegerValue");

			}
	
			tracker_db_free_field_def (def);

		} else {
			continue;	
		}

		table_count++;

		mid = g_strdup_printf ("%d", table_count);

		str = g_strconcat (", M", mid, ".", field, NULL); 
		g_string_append (sql, str );
		
		if (field) {
			g_free (field);
		}

		g_free (mid);
		g_free (str);		

	}


	/* build FROM clause */
	g_string_append (sql, " FROM Files F ");
	
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

		mid = g_strdup_printf ("%d", table_count);

		str = g_strconcat (" LEFT OUTER JOIN FileMetaData M", mid, " ON M", mid, ".FileID = F.ID ", " AND M", mid, ".MetaDataID = ", meta_id, NULL);
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
		add_metadata_to_dict (res, &iter_dict, n);
	}


	dbus_message_iter_close_container (&iter, &iter_dict);

	dbus_connection_send (rec->connection, reply, NULL);

	dbus_message_unref (reply);

}



void
tracker_dbus_method_search_by_text (DBusRec *rec)
{
	DBConnection *db_con;
	DBusMessage *reply;
	char **array = NULL, **row;	
	char ***res_str = NULL, ***rows;
	int row_count = 0, i = 0;
	char *str;


	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;
	
	dbus_message_get_args  (rec->message, NULL, DBUS_TYPE_STRING, &str, DBUS_TYPE_INVALID);

	res_str = tracker_db_exec_stmt_result (db_con->select_search_text_stmt, 1, str);

	if (res_str) {

		row_count = get_row_count (res_str);

		if (row_count > 0) {

			array = g_new (char *, row_count);

			i = 0;

			for (rows = res_str; *rows; rows++) {
				row = *rows;
				if (row && row[0] && row[1]) {
					array[i] = g_strconcat (row[0], "/",  row[1], NULL);
				}
				i++;
			}
			
		} else {
			tracker_log ("result set is empty");
		}

		tracker_db_free_result (res_str);
			
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
tracker_dbus_method_search_by_query (DBusRec *rec)
{
	DBConnection *db_con;
	DBusMessage *reply;
	char **array = NULL;				
	int row_count = 0, i = 0, table_count;
	MYSQL_ROW  row = NULL;
	char *str, *str2, *query;
	GString *sql;

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;
	
	dbus_message_get_args  (rec->message, NULL, DBUS_TYPE_STRING, &query, DBUS_TYPE_INVALID);

	if (query) {

		tracker_log ("executing rdf query %s", query);

		table_count = tracker_rdf_query_parse (db_con, query);

		if (table_count > 0) {
	
			sql = g_string_new (" CREATE Temporary Table TMP0 ENGINE = MEMORY SELECT T1.FileID FROM ");

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
					str2 = g_strconcat ("T1.FileID = T", str, ".FileID and ", NULL);
				} else {
					str2 = g_strconcat ("T1.FileID = T", str, ".FileID", NULL);
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

			str = g_strdup ("select F.Path, F.FileName from Files F, TMP0 T where F.ID = T.FileID");

			MYSQL_RES *res = tracker_exec_sql (db_con->db, str);
				
			g_free (str);

			if (res) {
				row_count = mysql_num_rows (res);
				if (row_count > 0) {
					array = g_new (char *, row_count);
					i = 0;

					while (row = mysql_fetch_row (res)) {

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

