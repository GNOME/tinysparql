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

#include <libtracker-common/tracker-log.h>

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

	if (!tracker_service_manager_is_valid_service (service)) {
		tracker_set_error (rec, "Invalid service %s or service has not been implemented yet", service);
		return;
	}

	db_con = tracker_db_get_service_connection (db_con, service);

	if (tracker_is_empty_string (uri)) {
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

		if (!meta || strlen (meta) < 3 || (strchr (meta, ':') == NULL) ) {
			tracker_set_error (rec, "Metadata type name %s is invalid. All names must be registered in tracker", meta);
			g_free (id);
			return;
		}

		tracker_db_set_single_metadata (db_con, service, id, meta, value, TRUE);
		tracker_notify_file_data_available ();
	}

	g_free (id);
	dbus_free_string_array (keys);
	dbus_free_string_array (values);

	reply = dbus_message_new_method_return (rec->message);

	dbus_connection_send (rec->connection, reply, NULL);

	dbus_message_unref (reply);
}

#include "tracker-rdf-query.h"


void
tracker_dbus_method_metadata_get (DBusRec *rec)
{
	TrackerDBResultSet *result_set;
	DBConnection 	*db_con;
	DBusError	dbus_error;
	DBusMessage 	*reply;
	int 		i, key_count, row_count;
	char 		**keys, **array;
	char		*uri, *id, *str, *service, *res_service;
	GString 	*sql, *sql_join;


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

	if (!tracker_service_manager_is_valid_service (service)) {
		tracker_set_error (rec, "Invalid service %s or service has not been implemented yet", service);
		return;
	}

	if (key_count == 0) {
		tracker_set_error (rec, "No Metadata was specified");
		return;
	}

	

	db_con = tracker_db_get_service_connection (db_con, service);

	id = tracker_db_get_id (db_con, service, uri);

	if (!id) {
		tracker_set_error (rec, "Entity with ID %s and service %s was not found in database", uri, service);
		return;
	}


	res_service = tracker_db_get_service_for_entity (db_con, id);
	
	if (!res_service) {
		tracker_set_error (rec, "Service info cannot be found for entity %s", uri);
		return;
	}

	/* build SELECT clause */
	sql = g_string_new (" SELECT DISTINCT ");

	sql_join = g_string_new (" FROM Services S  ");


	for (i = 0; i < key_count; i++) {

		FieldData *field = tracker_db_get_metadata_field (db_con, res_service, keys[i], i, TRUE, FALSE);

		if (!field) {
			tracker_set_error (rec, "Invalid or non-existant metadata type %s was specified", keys[i]);
			g_string_free (sql, TRUE);
			return;
			
		}

		if (i==0) {
			g_string_append_printf (sql, " %s", field->select_field);
		} else {
			g_string_append_printf (sql, ", %s", field->select_field);
		}
		if (field->needs_join) {
			g_string_append_printf (sql_join, "\n LEFT OUTER JOIN %s %s ON (S.ID = %s.ServiceID and %s.MetaDataID = %s) ", field->table_name, field->alias, field->alias, field->alias, field->id_field);
		}

		tracker_free_metadata_field (field);
	}

	g_string_append (sql, sql_join->str);

	g_string_free (sql_join, TRUE);

	dbus_free_string_array (keys);
	g_free (res_service);

	/* build WHERE clause */

	g_string_append_printf (sql, " WHERE S.ID = %s", id );

	str = g_string_free (sql, FALSE);

	tracker_log (str);

	result_set = tracker_db_interface_execute_query (db_con->db, NULL, str);

	g_free (str);
	g_free (id);


	reply = dbus_message_new_method_return (rec->message);

	row_count = 0;

	if (result_set) {
		GValue transform = { 0, };

		row_count = key_count;
		array = g_new (char *, key_count);

		g_value_init (&transform, G_TYPE_STRING);

		for (i = 0; i < key_count; i++) {
			GValue value = { 0, };

			_tracker_db_result_set_get_value (result_set, i, &value);

			if (g_value_transform (&value, &transform)) {
				array[i] = g_value_dup_string (&transform);
			} else {
				array[i] = g_strdup ("");
			}

			g_value_unset (&value);
			g_value_reset (&transform);
		}

		g_object_unref (result_set);
	} else {
		row_count = 1;
		array = g_new (char *, 1);
		array[0] = g_strdup ("");

	}

	dbus_message_append_args (reply,
	  			  DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &array, key_count,
	  			  DBUS_TYPE_INVALID);

	tracker_free_array (array, row_count);

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

	if (!meta || strlen (meta) < 3 || (strchr (meta, ':') == NULL) ) {
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

	tracker_exec_proc (db_con, "InsertMetadataType", meta, type_id, "0", "1", NULL);

	reply = dbus_message_new_method_return (rec->message);

	dbus_connection_send (rec->connection, reply, NULL);

	dbus_message_unref (reply);
}


void
tracker_dbus_method_metadata_get_type_details (DBusRec *rec)
{
	TrackerDBResultSet *result_set;
	DBConnection *db_con;
	DBusError    dbus_error;
	DBusMessage  *reply;
	char	     *meta, *data_type;
	gboolean     is_embedded, is_writable;
	gint         id;

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

	result_set = tracker_exec_proc (db_con, "GetMetadataTypeInfo", meta, NULL);

	if (!result_set) {
		tracker_set_error (rec, "Unknown metadata type %s", meta);
		return;
	}

	tracker_db_result_set_get (result_set,
				   1, &id,
				   2, &is_embedded,
				   3, &is_writable,
				   -1);

	data_type = type_array[id];
	g_object_unref (result_set);

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
		TrackerDBResultSet *result_set;

		result_set = tracker_db_get_metadata_types (db_con, class, TRUE);

		if (result_set) {
			array = tracker_get_query_result_as_array (result_set, &row_count);
			g_object_unref (result_set);
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
		TrackerDBResultSet *result_set;

		class_formatted = g_strconcat (class, ".*", NULL);

		result_set = tracker_db_get_metadata_types (db_con, class_formatted, TRUE);

		g_free (class_formatted);

		if (result_set) {
			array = tracker_get_query_result_as_array (result_set, &row_count);
			g_object_unref (result_set);
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
	TrackerDBResultSet *result_set;
	DBConnection *db_con;
	DBusMessage  *reply;
	char	     **array;
	int	     row_count;

/*
		<!-- returns an array of all metadata type classes that are registered -->
		<method name="GetRegisteredClasses">
			<arg type="as" name="result" direction="out" />
		</method>
*/

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;

	result_set = tracker_exec_proc (db_con, "SelectMetadataClasses", NULL);

	array = NULL;
	row_count = 0;

	if (result_set) {
		array = tracker_get_query_result_as_array (result_set, &row_count);
		g_object_unref (result_set);
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
tracker_dbus_method_metadata_get_unique_values (DBusRec *rec)
{
	DBConnection *db_con;
	DBusError    dbus_error;
	gchar 	     **meta_types = NULL;
	gchar        *service;
	gchar        *query = NULL;
	gint         meta_count;
	gboolean     order_desc;
	gint 	     limit, offset;

	FieldDef     *def;
	TrackerDBResultSet *result_set;
	GString      *select;
	GString      *from;
	GString      *where;
	GString      *group;
	GString      *order;
	char	     *str_offset, *str_limit;
	gchar        *sql;

	int          i;

/*
		<!-- returns an array of all unique values of given metadata type -->
		<method name="GetUniqueValues">
		        <arg type="s" name="service" direction="in" />
			<arg type="as" name="meta_types" direction="in" />
			<arg type="s" name="query" direction="in" />
		        <arg type="b" name="descending" direction="in" />
			<arg type="i" name="offset" direction="in" />
			<arg type="i" name="max_hits" direction="in" />
			<arg type="aas" name="result" direction="out" />
		</method>
*/

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;

	result_set = NULL;

        dbus_error_init (&dbus_error);
        if (!dbus_message_get_args (rec->message, NULL,
			       DBUS_TYPE_STRING, &service,
                               DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &meta_types, &meta_count,
			       DBUS_TYPE_STRING, &query,
			       DBUS_TYPE_BOOLEAN, &order_desc,
                               DBUS_TYPE_INT32, &offset,
                               DBUS_TYPE_INT32, &limit,
                               DBUS_TYPE_INVALID)) {
                tracker_set_error (rec, "DBusError: %s;%s", dbus_error.name, dbus_error.message);
                dbus_error_free (&dbus_error);
	        return;
        }

	if (!tracker_service_manager_is_valid_service (service)) {
		tracker_set_error (rec, "Invalid service %s or service has not been implemented yet", service);
		return;
	}

	db_con = tracker_db_get_service_connection (db_con, service);

	if (limit < 1) {
		limit = 1024;
	}

	if(!meta_count) {
		tracker_set_error (rec, "ERROR: No metadata type specified");
		return;
	}

	str_offset = tracker_int_to_str (offset);
	str_limit = tracker_int_to_str (limit);

	select = g_string_new ("SELECT ");
	from   = g_string_new ("\nFROM Services S ");
	where  = g_string_new ("\nWHERE ");
	order  = g_string_new ("\nORDER BY ");
	group  = g_string_new ("\nGROUP BY ");

	for (i=0;i<meta_count;i++) {
		def = tracker_db_get_field_def (db_con, meta_types[i]);

		if (!def) {
			tracker_set_error (rec, "ERROR: metadata not found for type %s", meta_types[i]);
			return;
		}
	  
		if (i) {
			g_string_append_printf (where, " AND ");
			g_string_append_printf (select, ", ");
			g_string_append_printf (group, ", ");
			g_string_append_printf (order, ", ");
		}
		
		switch (def->type) {
		  
		case DATA_INDEX:
		case DATA_STRING:
		case DATA_DOUBLE:
			g_string_append_printf (select, "D%d.MetaDataDisplay", i);
			g_string_append_printf (from, "INNER JOIN ServiceMetaData D%d ON (S.ID = D%d.ServiceID) ", i,i);
			g_string_append_printf (where, "(D%d.MetaDataID = %s)", i, def->id);
			g_string_append_printf (group, "D%d.MetaDataDisplay", i);
			if (order_desc) {
				g_string_append_printf (order, "D%d.MetaDataDisplay DESC", i);
			} else {
				g_string_append_printf (order, "D%d.MetaDataDisplay ASC", i);
			}
			break;
			
		case DATA_INTEGER:
		case DATA_DATE:
			g_string_append_printf (select, "D%d.MetaDataValue", i);
			g_string_append_printf (from, "INNER JOIN ServiceNumericMetaData D%d ON (S.ID = D%d.ServiceID) ", i, i);
			g_string_append_printf (where, "(D%d.MetaDataID = %s)", i, def->id);
			g_string_append_printf (group, "D%d.MetaDataValue", i);
			if (order_desc) {
				g_string_append_printf (order, "D%d.MetaDataValue DESC", i);
			} else {
				g_string_append_printf (order, "D%d.MetaDataValue ASC", i);
			}
			break;
			
		case DATA_KEYWORD:
			g_string_append_printf (select, "D%d.MetaDataValue", i);
			g_string_append_printf (from, "INNER JOIN ServiceKeywordMetaData D%d ON (S.ID = D%d.ServiceID) ", i, i);
			g_string_append_printf (where, "(D%d.MetaDataID = %s)", i,def->id);
			g_string_append_printf (group, "D%d.MetaDataValue", i);
			if (order_desc) {
				g_string_append_printf (order, "D%d.MetaDataValue DESC", i);
			} else {
				g_string_append_printf (order, "D%d.MetaDataValue ASC", i);
			}
			break;
	    
		default: 
		        tracker_error ("ERROR: metadata could not be retrieved as type %d is not supported", def->type);
			g_string_free (select, TRUE);
			g_string_free (from, TRUE);
			g_string_free (where, TRUE);
			g_string_free (group, TRUE);
			g_string_free (order, TRUE);
		}
	}
	
	g_string_append_printf (select, ", COUNT (*) ");
	
	if (query) {
		char *rdf_where;
		char *rdf_from;
		GError *error = NULL;
		
		tracker_rdf_filter_to_sql (db_con, query, service, &rdf_from, &rdf_where, error);
		
		if (error) {
			tracker_set_error (rec, "ERROR: Parse error: %s", error->message);
			g_error_free (error);
			return;
		}
		
		g_string_append_printf (from, " %s ", rdf_from);
		g_string_append_printf (where, " AND %s", rdf_where);

		g_free (rdf_from);
		g_free (rdf_where);
	}

	g_string_append_printf (order, " LIMIT %s,%s", str_offset, str_limit);
	sql = g_strconcat (select->str, " ", from->str, " ", where->str, " ", group->str, " ", order->str, NULL);

	g_string_free (select, TRUE);
	g_string_free (from, TRUE);
	g_string_free (where, TRUE);
	g_string_free (group, TRUE);
	g_string_free (order, TRUE);
	g_free (str_offset);
	g_free (str_limit);

	g_message ("Unique value query executed:\n%s", sql);

	result_set =  tracker_db_interface_execute_query (db_con->db, NULL, sql);

	g_free (sql);

	tracker_dbus_reply_with_query_result (rec, result_set);

	if (result_set) {
	        g_object_unref (result_set);
	}
}
