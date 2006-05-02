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
#include <stdlib.h>
#include "tracker-dbus-methods.h"

static char *
get_metadata (DBConnection *db_con, const char *service, const char *id, const char *key) 
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


static void  
set_metadata (DBConnection *db_con, const char *service, const char *id, const char *key, const char *value, gboolean overwrite)
{
	char *str_write;	

	if (overwrite) {
		str_write = "1";
	} else {
		str_write = "0";
	}

	tracker_exec_proc  (db_con->db, "SetMetadata", 5, service, id, key, value, str_write);	
	

}


void
tracker_dbus_method_metadata_set (DBusRec *rec)
{
	DBConnection 	*db_con;
	DBusMessage 	*reply;
	int 	 	service_id, i, key_count, value_count;
	char 		*uri, *service, *meta, *id, *value = NULL;
	char		**keys, **values;
	gboolean	is_local_file = FALSE;

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;
	
/*
		<method name="Set">
			<arg type="s" name="service" direction="in" />
			<arg type="s" name="id" direction="in" />
			<arg type="as" name="keys" direction="in" />
			<arg type="as" name="values" direction="in" />
		</method>
*/
		

	dbus_message_get_args  (rec->message, NULL,  DBUS_TYPE_STRING, &service, DBUS_TYPE_STRING, &uri, DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &keys, &key_count, DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &values, &value_count, DBUS_TYPE_INVALID);
		
	service_id = tracker_str_in_array (service, serice_index_array);
	if ( service_id == -1) {
		tracker_set_error (rec, "Invalid service %s", service);	
		return;
	}


	if (tracker_str_in_array (service, implemented_services) == -1) {
		tracker_set_error (rec, "Service %s has not been implemented yet", service);	
		return;
	}

	if (!uri || strlen (uri) == 0) {
		tracker_set_error (rec, "ID is invalid");
		return;
	}

	if (key_count == 0 || value_count == 0) {
		tracker_set_error (rec, "No metadata types or metadata values specified");
		return;
	}
	
	if (key_count != value_count ) {
		tracker_set_error (rec, "The number of specified keys does not match the supplied values");
		return;
	}

	id = tracker_db_get_id (db_con, service, uri);

	if (!id) {
		tracker_set_error (rec, "Entity with ID %s not found in database", uri);
		return;	
	}

	

	is_local_file = (uri[0] == '/');

	for (i=0; i<key_count; i++) {

		meta = keys[i];
		value = values[i];
		if (!meta || strlen (meta) < 3 || (strchr (meta, '.') == NULL) ) {
			tracker_set_error (rec, "Metadata type name %s is invalid. All names must be in the format 'class.name' ", meta);
			g_free (id);
			return;
		}


		

		set_metadata (db_con, service, id, meta, value, !is_local_file);

	}

	g_free (id);

	reply = dbus_message_new_method_return (rec->message);
	
	dbus_connection_send (rec->connection, reply, NULL);
	
	dbus_message_unref (reply);
	
}


void
tracker_dbus_method_metadata_get (DBusRec *rec)
{
	DBConnection 	*db_con;
	DBusMessage 	*reply;
	int 		service_id, i, key_count, table_count, row_count = 0;
	char 		**keys = NULL, **array,  *uri, *id,  *field, *str, *mid, *service; 
	GString 	*sql;
	MYSQL_RES 	*res;
	MYSQL_ROW  	row = NULL;
	char		**date_array;

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;

	/*	<method name="Get">
			<arg type="s" name="service" direction="in" />
			<arg type="s" name="id" direction="in" />
			<arg type="as" name="keys" direction="in" />
			<arg type="as" name="values" direction="out" />
		</method>
	*/
	
	dbus_message_get_args  (rec->message, NULL, DBUS_TYPE_STRING, &service, DBUS_TYPE_STRING, &uri, DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &keys, &key_count, DBUS_TYPE_INVALID);

	service_id = tracker_str_in_array (service, serice_index_array);
	if ( service_id == -1) {
		tracker_set_error (rec, "Invalid service %s", service);	
		return;
	}


	if (tracker_str_in_array (service, implemented_services) == -1) {
		tracker_set_error (rec, "Service %s has not been implemented yet", service);	
		return;
	}

	if (key_count == 0) {
		tracker_set_error (rec, "No Metadata was specified");
		return;	
	}

	id = tracker_db_get_id (db_con, service, uri);

	if (!id) {
		tracker_set_error (rec, "Entity with ID %s not found in database", uri);
		return;	
	}


	date_array = g_new( char *, key_count);


	/* build SELECT clause */
	sql = g_string_new (" SELECT DISTINCT ");

	table_count = 0;

	for (i=0; i<key_count; i++) {
		
		char *metadata = keys[i];

		FieldDef 	*def;

		def = tracker_db_get_field_def (db_con, metadata);
		
		if (def) {

			if (def->type == DATA_INDEX_STRING) {			
		
				field = g_strdup ("MetaDataIndexValue");
		
			} else if (def->type == DATA_STRING) {

				field = g_strdup ("MetaDataValue");

			} else {

				field = g_strdup ("MetaDataNumericValue");

			}
			
			if (def->type == DATA_DATE) {
				date_array[i] = g_strdup ("1");
			} else {
				date_array[i] = g_strdup ("0");
			}			

			tracker_db_free_field_def (def);

		} else {
			tracker_set_error (rec, "Invalid or non-existant metadata type %s was specified", metadata);	
			g_string_free (sql, TRUE);
			g_free (id);
			return;
		}

		table_count++;

		mid = g_strdup_printf ("%d", table_count);

		if (table_count == 1) {
			str = g_strconcat (" M", mid, ".", field, NULL); 
		} else {
			str = g_strconcat (", M", mid, ".", field, NULL); 
		}
	
		g_string_append (sql, str );
		
		if (field) {
			g_free (field);
		}

		g_free (mid);
		g_free (str);	

	}


	/* build FROM clause */
	g_string_append (sql, " FROM Services F");

	table_count = 0;
	
	
	for (i=0; i<key_count; i++) {

		FieldDef 	*def;
		char 		*meta_id;
	
		char *metadata = keys[i];

		def = tracker_db_get_field_def (db_con, metadata);
		if (def) {
			meta_id = g_strdup( def->id);
			tracker_db_free_field_def (def);

		} else {
			tracker_set_error (rec, "Invalid or non-existant metadata type %s was specified", metadata);	
			g_string_free (sql, TRUE);
			g_free (id);
			return;	
		}

		table_count++;

		mid = g_strdup_printf ("%d", table_count);

		str = g_strconcat (" LEFT OUTER JOIN "," ServiceMetaData", " M", mid, " ON M", mid, ".", "ServiceID", " = F.ID ", " AND M", mid, ".MetaDataID = ", meta_id, NULL);
		g_string_append (sql, str );

		g_free (meta_id);
		g_free (mid);
		g_free (str);	



	}

	/* build WHERE clause */
	str = g_strconcat (" WHERE F.ID = ", id, " ", NULL); 

	g_string_append (sql, str);

	g_free (str);
	
	
	str = g_string_free (sql, FALSE);
	tracker_log (str);
	res = tracker_exec_sql (db_con->db, str);

	g_free (str);

	g_free (id);
		
		
	reply = dbus_message_new_method_return (rec->message);

	row_count = 0;

	if (res) {

		row_count = mysql_num_rows (res);

		i = 0;

		if (row_count > 0) {

			array = g_new (char *, key_count);

			row = mysql_fetch_row (res);

			for (i = 0; i < key_count; i++) {


				if (row[i]) {
					if (date_array[i][0] == '1') {
						array[i] = tracker_date_to_str (strtol (row[i], NULL, 10));
					} else {
						array[i] = g_strdup (row[i]);	
					}
				} else {
					array[i] = NULL;
				}

			
			}
			
		} else {
			tracker_log ("result set is empty");
			array = g_new (char *, 1);
			array[0] = NULL;
		}

		mysql_free_result (res);
			
	} else {
		array = g_new (char *, 1);
		array[0] = NULL;
	}

	
	dbus_message_append_args (reply,
	  			  DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &array, key_count,
	  			  DBUS_TYPE_INVALID);

	if (row_count > 0) {

		for (i = 0; i < key_count; i++) {
        		g_free (array[i]);
		}

		g_free (array);
	}

	for (i = 0; i < key_count; i++) {
       		g_free (date_array[i]);
	}

	g_free (date_array);



	dbus_connection_send (rec->connection, reply, NULL);

	dbus_message_unref (reply);

}




void
tracker_dbus_method_register_metadata_type (DBusRec *rec)
{
	DBConnection 	*db_con;
	DBusMessage 	*reply;
	char 		*meta, *type_id;
	char		*type;
	int 		i;


	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;
	
	dbus_message_get_args  (rec->message, NULL, DBUS_TYPE_STRING, &meta, DBUS_TYPE_STRING, &type, DBUS_TYPE_INVALID);
		
	if (!meta || strlen (meta) < 3 || (strchr (meta, '.') == NULL) ) {
		tracker_set_error (rec, "Metadata name is invalid. All names must be in the format 'class.name' ");
		return;
	}

	i = tracker_str_in_array (type, type_array);
	if (i == -1) {
		tracker_set_error (rec, "Invalid metadata data type specified");
		return;
	}


	type_id = g_strdup_printf ("%d", i);
	tracker_exec_proc  (db_con->db, "InsertMetadataType", 4, meta, type_id, "0", "1");
	g_free (type_id);




	reply = dbus_message_new_method_return (rec->message);
	
	dbus_connection_send (rec->connection, reply, NULL);
	
	dbus_message_unref (reply);
	
	
}
