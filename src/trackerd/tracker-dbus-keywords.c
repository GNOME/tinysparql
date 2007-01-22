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

#include <string.h>

#include "tracker-dbus-methods.h"
#include "tracker-dbus-keywords.h"

/*
static void
update_keywords_metadata (DBConnection *db_con, const char* service, const char *path, const char *name)
{
	char ***res;
	char *tmp;
	char *id;
	char *keywords;

	tmp = g_build_filename (path, name, NULL);
	id = tracker_db_get_id (db_con, service, tmp);

	g_free (tmp);

	if (!id) {
		return;
	}

	res = tracker_exec_proc (db_con, "GetKeywords", 2, path, name);

	if (res) {
		GString *words;
		char	**row;
		int	i;

		words = g_string_new (" ");
		i = 0;

		while ((row = tracker_db_get_row (res, i))) {
			if (row[0]) {

				if (i != 0) {
					words = g_string_append (words, ",");
				}

				words = g_string_append (words, row[0]);
				i++;
			}
		}

		tracker_db_free_result (res);

		keywords = g_string_free (words, FALSE);


		tracker_db_update_keywords (db_con, service, id, keywords);

		g_free (keywords);

	} else {
		tracker_db_update_keywords (db_con, service, id, " ");
	}

	g_free (id);
}
*/

void
tracker_dbus_method_keywords_get_list (DBusRec *rec)
{
	DBConnection 	*db_con;
	DBusError    dbus_error;
	char 		*service;
	char		***res;

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;

/*
		<!-- gets a list of all unique keywords/tags that are in use by the specified service irrespective of the uri or id of the entity
		     Returns an array of string arrays with the keyword and the total usage count of the keyword as the string array
		-->
		<method name="GetList">
			<arg type="s" name="service" direction="in" />
			<arg type="aas" name="value" direction="out" />
		</method>

*/

	dbus_error_init(&dbus_error);
	if (!dbus_message_get_args (rec->message, NULL,
			       DBUS_TYPE_STRING, &service,
			       DBUS_TYPE_INVALID)) {
		tracker_set_error (rec, "DBusError: %s;%s", dbus_error.name, dbus_error.message);
		dbus_error_free (&dbus_error);
		return;
	}

	if (!tracker_is_valid_service (db_con, service)) {
		tracker_set_error (rec, "Invalid service %s or service has not been implemented yet", service);
		return;
	}

	res = tracker_db_get_keyword_list (db_con, service);

	tracker_dbus_reply_with_query_result (rec, res);

	tracker_db_free_result (res);
}


void
tracker_dbus_method_keywords_get (DBusRec *rec)
{
	DBConnection 	*db_con;
	DBusError    dbus_error;
	DBusMessage 	*reply;
	char 		*id, *uri, *service;
	char 		**array;
	char		***res;
	int 		row_count;

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

	dbus_error_init(&dbus_error);
	if (!dbus_message_get_args (rec->message, NULL,
			       DBUS_TYPE_STRING, &service,
			       DBUS_TYPE_STRING, &uri,
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
		tracker_set_error (rec, "Uri is invalid");
		return;
	}

	id = tracker_db_get_id (db_con, service, uri);

	if (!id) {
		tracker_set_error (rec, "Entity %s not found in database", uri);
		return;
	}

	res = tracker_db_get_metadata_values (db_con, service, id, "DC:Keywords");

	g_free (id);

	row_count = 0;
	array = NULL;

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
tracker_dbus_method_keywords_add (DBusRec *rec)
{
	DBConnection 	*db_con;
	DBusError    dbus_error;
	DBusMessage 	*reply;
	char 		*id, *uri, *service;
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

	dbus_error_init (&dbus_error);

	if (!dbus_message_get_args (rec->message, NULL,
			       DBUS_TYPE_STRING, &service,
			       DBUS_TYPE_STRING, &uri,
			       DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &array, &row_count,
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
		tracker_set_error (rec, "URI is invalid");
		return;
	}

	id = tracker_db_get_id (db_con, service, uri);

	if (!id) {
		tracker_set_error (rec, "Entity %s not found in database", uri);
		return;
	}

		
	if (array && (row_count > 0)) {
		int i;

		for (i = 0; i < row_count; i++) {
			if (array[i]) {
				tracker_db_set_metadata (db_con, service, id, "DC:Keywords", array[i], TRUE, TRUE, FALSE);
			}
		}
	}

	dbus_free_string_array(array);

	tracker_log ("adding keywords to %s with id %s", uri, id);

	g_free (id);

	reply = dbus_message_new_method_return (rec->message);

	dbus_connection_send (rec->connection, reply, NULL);

	dbus_message_unref (reply);
}


void
tracker_dbus_method_keywords_remove (DBusRec *rec)
{
	DBConnection 	*db_con;
	DBusError    dbus_error;
	DBusMessage 	*reply;
	char 		*id, *uri, *service;
	char 		**array;
	int 		row_count = 0;

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;

	array = NULL;

/*
		<!-- removes all specified keywords/tags for specified service and id -->
		<method name="Remove">
			<arg type="s" name="service" direction="in" />
			<arg type="s" name="id" direction="in" />
			<arg type="as" name="keywords" direction="in" />
		</method>
*/

	dbus_error_init(&dbus_error);
	if (!dbus_message_get_args (rec->message, NULL,
			       DBUS_TYPE_STRING, &service,
			       DBUS_TYPE_STRING, &uri,
			       DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &array, &row_count,
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

	id = tracker_db_get_id (db_con, service, uri);

	if (!id) {
		tracker_set_error (rec, "Entity %s not found in database", uri);
		return;
	}

	if (array && (row_count > 0)) {
		int i;

		for (i = 0; i < row_count; i++) {
			if (array[i]) {
				tracker_log ("deleting keyword %s from %s with ID %s", array[i], uri, id);
				tracker_db_delete_metadata_value (db_con, service, id, "DC:Keywords", array[i], FALSE);
			}
		}
	}

	dbus_free_string_array (array);

	g_free (id);

	reply = dbus_message_new_method_return (rec->message);

	dbus_connection_send (rec->connection, reply, NULL);

	dbus_message_unref (reply);
}


void
tracker_dbus_method_keywords_remove_all (DBusRec *rec)
{
	DBConnection 	*db_con;
	DBusError    dbus_error;
	DBusMessage 	*reply;
	char 		*id, *uri, *service;

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;

/*

		<!-- removes all keywords/tags for specified service and id -->
		<method name="RemoveAll">
			<arg type="s" name="service" direction="in" />
			<arg type="s" name="id" direction="in" />
		</method>
*/

	dbus_error_init(&dbus_error);
	if (!dbus_message_get_args (rec->message, NULL,
			       DBUS_TYPE_STRING, &service,
			       DBUS_TYPE_STRING, &uri,
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
		tracker_set_error (rec, "URI is invalid");
		return;
	}

	id = tracker_db_get_id (db_con, service, uri);

	if (!id) {
		tracker_set_error (rec, "Entity %s not found in database", uri);
		return;
	}

	tracker_db_delete_metadata (db_con, service, id, "DC:Keywords", TRUE);
	
	g_free (id);
	
	reply = dbus_message_new_method_return (rec->message);

	dbus_connection_send (rec->connection, reply, NULL);

	dbus_message_unref (reply);
}


void
tracker_dbus_method_keywords_search (DBusRec *rec)
{
	DBConnection 	*db_con;
	DBusError    dbus_error;
	DBusMessage 	*reply;
	char 		*service;
	char 		**array;
	int 		row_count, limit, query_id, offset;
	char		***res;
	GString		*str_words, *str_select, *str_where;
	char		*query_sel, *query_where, *query;
	int		i;

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

	dbus_error_init(&dbus_error);
	if (!dbus_message_get_args (rec->message, NULL,
			       DBUS_TYPE_INT32, &query_id,
			       DBUS_TYPE_STRING, &service,
			       DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &array, &row_count,
			       DBUS_TYPE_INT32, &offset,
			       DBUS_TYPE_INT32, &limit,
			       DBUS_TYPE_INVALID)) {
		tracker_set_error (rec, "DBusError: %s;%s", dbus_error.name, dbus_error.message);
		dbus_error_free (&dbus_error);
		return;
	}

	if (!tracker_is_valid_service (db_con, service)) {
		tracker_set_error (rec, "Invalid service %s or service has not been implemented yet", service);
		return;
	}

	if (offset < 0) {
		offset = 0;
	}

	if (row_count < 1) {

		tracker_set_error (rec, "No keywords supplied");
		return;
	}

	str_words = g_string_new ("");
	g_string_append_printf (str_words, "'%s'", array[0]);

	for (i = 1; i < row_count; i++) {
		g_string_append_printf (str_words, ", '%s'", array[i]);
	}

	tracker_log ("executing keyword search on %s", str_words->str);

	str_select = g_string_new (" Select distinct S.Path || '");

	str_select = g_string_append (str_select, G_DIR_SEPARATOR_S);

	str_select = g_string_append (str_select, "' || S.Name as EntityName from Services S, ServiceKeywordMetaData M ");

	char *related_metadata = tracker_get_related_metadata_names (db_con, "DC:Keywords");

	str_where = g_string_new ("");

	g_string_append_printf (str_where, " where S.ID = M.ServiceID and M.MetaDataID in (%s) and M.MetaDataValue in (%s) ", related_metadata, str_words->str);

	g_free (related_metadata);

	g_string_free (str_words, TRUE);

	int smin, smax;
	char *str_min, *str_max;
	
	smin = tracker_get_id_for_service (service);

	if (smin == 0) {
		smax = 8;
	} else {
		smax = smin;
	}

	str_min = tracker_int_to_str (smin);
	str_max = tracker_int_to_str (smax);


	g_string_append_printf (str_where, "  and  (S.ServiceTypeID between %s and %s) ", str_min, str_max);


	g_free (str_min);
	g_free (str_max);

	g_string_append_printf (str_where, " Limit %d,%d", offset, limit);


	query_sel = g_string_free (str_select, FALSE);
	query_where = g_string_free (str_where, FALSE);
	query = g_strconcat (query_sel, query_where, NULL);

	tracker_log (query);
	res = tracker_exec_sql (db_con, query);

	g_free (query_sel);
	g_free (query_where);
	g_free (query);

	dbus_free_string_array (array);
	row_count = 0;
	array = NULL;
	
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
