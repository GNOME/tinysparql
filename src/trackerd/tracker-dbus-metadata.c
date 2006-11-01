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

#include <stdlib.h>
#include <string.h>

#include "tracker-dbus-methods.h"
#include "tracker-dbus-metadata.h"


void
tracker_dbus_method_metadata_set (DBusRec *rec)
{
	DBConnection 	*db_con;
	DBusError		dbus_error;
	DBusMessage 	*reply;
	int 	 	i, key_count, value_count;
	char 		*uri, *service, *id;
	char		**keys, **values;
	gboolean	is_local_file;

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

	dbus_error_init (&dbus_error);
	if (!dbus_message_get_args (rec->message, NULL,
			       DBUS_TYPE_STRING, &service,
			       DBUS_TYPE_STRING, &uri,
			       DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &keys, &key_count,
			       DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &values, &value_count,
			       DBUS_TYPE_INVALID)) {
		tracker_set_error (rec, "DBusError: %s;%s", dbus_error.name, dbus_error.message);
		dbus_error_free (&dbus_error);
		return;
	}

	if (!tracker_is_valid_service (db_con, service)) {
		tracker_set_error (rec, "Invalid service %s or service has not been implemented yet", service);
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


	is_local_file = (uri[0] == G_DIR_SEPARATOR);

	for (i = 0; i < key_count; i++) {
		char *meta, *value;

		meta = keys[i];
		value = values[i];

		if (!meta || strlen (meta) < 3 || (strchr (meta, '.') == NULL) ) {
			tracker_set_error (rec, "Metadata type name %s is invalid. All names must be in the format 'class.name' ", meta);
			g_free (id);
			return;
		}

		if (strcmp (meta, "File.Keywords") == 0) {
			tracker_set_error (rec, "File.Keywords can only be set from the Keywords Interface");
			g_free (id);
			return;
		}

		tracker_db_set_metadata (db_con, service, id, meta, value, !is_local_file, TRUE, FALSE);
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
	DBusError		dbus_error;
	DBusMessage 	*reply;
	int 		i, key_count, table_count, row_count;
	char 		**keys, **array;
	char		*uri, *id, *str, *service;
	GString 	*sql;
	char		***res;
	char		**date_array;
	const char 	*table;

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;

	/*	<method name="Get">
			<arg type="s" name="service" direction="in" />
			<arg type="s" name="id" direction="in" />
			<arg type="as" name="keys" direction="in" />
			<arg type="as" name="values" direction="out" />
		</method>
	*/

	dbus_error_init (&dbus_error);
	if (!dbus_message_get_args (rec->message, NULL,
			       DBUS_TYPE_STRING, &service,
			       DBUS_TYPE_STRING, &uri,
			       DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &keys, &key_count,
			       DBUS_TYPE_INVALID)) {
		tracker_set_error (rec, "DBusError: %s;%s", dbus_error.name, dbus_error.message);
		dbus_error_free (&dbus_error);
		return;
	}

	if (!tracker_is_valid_service (db_con, service)) {
		tracker_set_error (rec, "Invalid service %s or service has not been implemented yet", service);
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


	date_array = g_new (char *, key_count+1);
	date_array [key_count] = NULL;

	/* build SELECT clause */
	sql = g_string_new (" SELECT DISTINCT ");

	table_count = 0;

	for (i = 0; i < key_count; i++) {
		char *metadata;

		FieldDef   *def;

		metadata = keys[i];

		def = tracker_db_get_field_def (db_con, metadata);

		if (def) {

			if (def->type == DATA_DATE) {
				date_array[i] = g_strdup ("1");
			} else {
				date_array[i] = g_strdup ("0");
			}

		} else {
			tracker_set_error (rec, "Invalid or non-existant metadata type %s was specified", metadata);
			g_string_free (sql, TRUE);
			g_free (id);
			return;
		}

		table_count++;

		if (table_count == 1) {
			g_string_append_printf (sql, " M%d.MetaDataValue ", table_count);
		} else {
			g_string_append_printf (sql, ", M%d.MetaDataValue ", table_count);
		}

		tracker_db_free_field_def (def);
	}


	/* build FROM clause */
	g_string_append (sql, "\n FROM Services F ");

	table_count = 0;

	for (i = 0; i < key_count; i++) {
		FieldDef *def;
		char	 *metadata;

		metadata = keys[i];
		
		def = tracker_db_get_field_def (db_con, metadata);

		if (def) {
			
			if (def->type == DATA_INDEX_STRING) {
				table = "ServiceIndexMetaData";
			} else if (def->type == DATA_STRING) {
				table = "ServiceMetaData";
			} else if (def->type == DATA_INDEX_BLOB) {
				table = "ServiceBlobMetaData";
			} else {
				table = "ServiceNumericMetaData";
			}

	
		} else {
			tracker_set_error (rec, "Invalid or non-existant metadata type %s was specified", metadata);
			g_string_free (sql, TRUE);
			return;
		}

		table_count++;

		g_string_append_printf (sql, " LEFT OUTER JOIN %s M%d ON M%d.ServiceID = F.ID AND M%d.MetaDataID = %s ", table, table_count, table_count, table_count, def->id);

		tracker_db_free_field_def (def);
	}


	/* build WHERE clause */

	g_string_append_printf (sql, " WHERE F.ID = %s", id );

	str = g_string_free (sql, FALSE);

	tracker_log (str);

	res = tracker_exec_sql_ignore_nulls (db_con, str);

	g_free (str);
	g_free (id);


	reply = dbus_message_new_method_return (rec->message);

	row_count = 0;

	if (res) {

		row_count = tracker_get_row_count (res);

		i = 0;

		if (row_count > 0) {
			char **row;

			array = g_new (char *, key_count);

			row = tracker_db_get_row (res, 0);

			for (i = 0; i < key_count; i++) {


				if (row[i]) {
					if (date_array[i][0] == '1') {
						array[i] = tracker_date_to_str (strtol (row[i], NULL, 10));
					} else {
						array[i] = g_strdup (row[i]);
					}
				} else {
					array[i] = g_strdup ("");
				}
			}

		} else {
			tracker_log ("result set is empty");
			row_count = 1;
			array = g_new (char *, 1);
			array[0] = g_strdup ("");

		}

		tracker_db_free_result (res);

	} else {
		row_count = 1;
		array = g_new (char *, 1);
		array[0] = g_strdup ("");

	}

	dbus_message_append_args (reply,
	  			  DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &array, key_count,
	  			  DBUS_TYPE_INVALID);

	tracker_free_array (array, row_count);
	tracker_free_array (date_array, key_count);

	dbus_connection_send (rec->connection, reply, NULL);

	dbus_message_unref (reply);
}


void
tracker_dbus_method_metadata_register_type (DBusRec *rec)
{
	DBConnection *db_con;
	DBusError    dbus_error;
	DBusMessage  *reply;
	char 	     *meta, *type_id;
	char	     *type;

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;

	dbus_error_init (&dbus_error);
	if (!dbus_message_get_args (rec->message, NULL,
			       DBUS_TYPE_STRING, &meta,
			       DBUS_TYPE_STRING, &type,
			       DBUS_TYPE_INVALID)) {
		tracker_set_error (rec, "DBusError: %s;%s", dbus_error.name, dbus_error.message);
		dbus_error_free (&dbus_error);
		return;
	}

	if (!meta || strlen (meta) < 3 || (strchr (meta, '.') == NULL) ) {
		tracker_set_error (rec, "Metadata name is invalid. All names must be in the format 'class.name' ");
		return;
	}

	if (strcmp ("index", type) == 0) {
		type_id = "0";
	} else if (strcmp ("string", type) == 0) {
		type_id = "1";
	} else if (strcmp ("numeric", type) == 0) {
		type_id = "2";
	} else if (strcmp ("date", type) == 0) {
		type_id = "3";
	} else {
		tracker_set_error (rec, "Invalid Metadata Type specified");
		return;
	}

	tracker_exec_proc (db_con, "InsertMetadataType", 4, meta, type_id, "0", "1");

	reply = dbus_message_new_method_return (rec->message);

	dbus_connection_send (rec->connection, reply, NULL);

	dbus_message_unref (reply);
}


void
tracker_dbus_method_metadata_get_type_details (DBusRec *rec)
{
	DBConnection *db_con;
	DBusError    dbus_error;
	DBusMessage  *reply;
	char	     *meta, *data_type;
	gboolean     is_embedded, is_writable;

/*
		<method name="GetTypeDetails">
			<arg type="s" name="name" direction="in" />
			<arg type="s" name="data_type" direction="out" />
			<arg type="b" name="is_embedded" direction="out" />
			<arg type="b" name="is_writable" direction="out" />
		</method>
*/

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;

	dbus_error_init (&dbus_error);
	if (!dbus_message_get_args (rec->message, NULL,
			       DBUS_TYPE_STRING, &meta,
			       DBUS_TYPE_INVALID)) {
		tracker_set_error (rec, "DBusError: %s;%s", dbus_error.name, dbus_error.message);
		dbus_error_free (&dbus_error);
		return;
	}

	data_type = NULL;
	is_embedded = FALSE;
	is_writable = FALSE;

	if (!meta) {
		tracker_set_error (rec, "Unknown metadata type %s", meta);
		return;
	}

	char ***res;

	res = tracker_exec_proc (db_con, "GetMetadataTypeInfo", 1, meta);

	if (!res) {
		tracker_set_error (rec, "Unknown metadata type %s", meta);
		return;
	}

	char **row;

	row = tracker_db_get_row (res, 0);

	if (!(row && row[1] && row[2] && row[3])) {
		tracker_set_error (rec, "Bad info for metadata type %s", meta);
		return;
	}

	int i;

	i = atoi (row[1]);

	if (i > 3 || i < 0) {
		tracker_set_error (rec, "Bad info for metadata type %s", meta);
		return;
	}

	data_type = type_array[i];
	is_embedded = (strcmp (row[2], "1") == 0);
	is_writable = (strcmp (row[3], "1") == 0);

	tracker_db_free_result (res);

	reply = dbus_message_new_method_return (rec->message);

	dbus_message_append_args (reply,
				  DBUS_TYPE_STRING, &data_type,
				  DBUS_TYPE_BOOLEAN, &is_embedded,
				  DBUS_TYPE_BOOLEAN, &is_writable,
				  DBUS_TYPE_INVALID);

	dbus_connection_send (rec->connection, reply, NULL);
	dbus_message_unref (reply);
}


void
tracker_dbus_method_metadata_get_registered_types (DBusRec *rec)
{
	DBConnection *db_con;
	DBusError    dbus_error;
	DBusMessage  *reply;
	char	     *class, **array;
	int	     row_count;

/*
		<!-- returns an array of all metadata types that are registered for a certain class -->
		<method name="GetRegisteredTypes">
			<arg type="s" name="metadata_class" direction="in" />
			<arg type="as" name="result" direction="out" />
		</method>
*/

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;

	dbus_error_init (&dbus_error);
	if (!dbus_message_get_args (rec->message, NULL,
			       DBUS_TYPE_STRING, &class,
			       DBUS_TYPE_INVALID)) {
		tracker_set_error (rec, "DBusError: %s;%s", dbus_error.name, dbus_error.message);
		dbus_error_free (&dbus_error);
		return;
	}

	array = NULL;
	row_count = 0;

	if (class) {
		char ***res;

		res = tracker_db_get_metadata_types (db_con, class, TRUE);

		if (res) {
			array = tracker_get_query_result_as_array (res, &row_count);
			tracker_db_free_result (res);
		}
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
tracker_dbus_method_metadata_get_writeable_types (DBusRec *rec)
{
	DBConnection *db_con;
	DBusError    dbus_error;
	DBusMessage  *reply;
	char	     *class, *class_formatted, **array;
	int	     row_count;

/*
		<!-- returns an array of all metadata types that are writeable and registered for a certain class
		     You can enter "*" as the class to get all metadat types for all classes that are writeable
		-->
		<method name="GetWriteableTypes">
			<arg type="s" name="metadata_class" direction="in" />
			<arg type="as" name="result" direction="out" />
		</method>
*/

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;

	dbus_error_init (&dbus_error);
	if (!dbus_message_get_args (rec->message, NULL,
			       DBUS_TYPE_STRING, &class,
			       DBUS_TYPE_INVALID)) {
		tracker_set_error (rec, "DBusError: %s;%s", dbus_error.name, dbus_error.message);
		dbus_error_free (&dbus_error);
		return;
	}

	array = NULL;
	row_count = 0;

	if (class) {
		char ***res;

		class_formatted = g_strconcat (class, ".*", NULL);

		res = tracker_db_get_metadata_types (db_con, class_formatted, TRUE);

		g_free (class_formatted);

		if (res) {
			array = tracker_get_query_result_as_array (res, &row_count);
			tracker_db_free_result (res);
		}
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
tracker_dbus_method_metadata_get_registered_classes (DBusRec *rec)
{
	DBConnection *db_con;
	DBusMessage  *reply;
	char	     **array;
	int	     row_count;
	char	     ***res;

/*
		<!-- returns an array of all metadata type classes that are registered -->
		<method name="GetRegisteredClasses">
			<arg type="as" name="result" direction="out" />
		</method>
*/

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;

	res = tracker_exec_proc (db_con, "SelectMetadataClasses", 0);

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
