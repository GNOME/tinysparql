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

#include <string.h>

#include "tracker-dbus-methods.h"
#include "tracker-rdf-query.h"


void
tracker_dbus_method_search_text (DBusRec *rec)
{
	DBConnection *db_con;
	DBusError    dbus_error;
	DBusMessage  *reply;
	char	     **array;
	int	     row_count, i;
	int	     limit, query_id, offset;
	char	     *service;
	char	     ***res;
	char	     *str;

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;

/*
		<!-- searches specified service for entities that match the specified search_text. 
		     Returns uri of all hits. -->
		<method name="Text">
			<arg type="i" name="live_query_id" direction="in" />
			<arg type="s" name="service" direction="in" />
			<arg type="s" name="search_text" direction="in" />
			<arg type="i" name="offset" direction="in" />
			<arg type="i" name="max_hits" direction="in" />
			<arg type="as" name="result" direction="out" />
		</method>
*/

	dbus_error_init (&dbus_error);
	if (!dbus_message_get_args (rec->message, NULL, DBUS_TYPE_INT32, &query_id,
			       DBUS_TYPE_STRING, &service,
			       DBUS_TYPE_STRING, &str,
			       DBUS_TYPE_INT32, &offset,
			       DBUS_TYPE_INT32, &limit,
			       DBUS_TYPE_INVALID)) {
		tracker_set_error (rec, "DBusError: %s;%s", dbus_error.name, dbus_error.message);
		dbus_error_free (&dbus_error);
		return;
	}

	if (!service)  {
		tracker_set_error (rec, "No service was specified");
		return;
	}

	if (!tracker_is_valid_service (db_con, service)) {
		tracker_set_error (rec, "Invalid service %s or service has not been implemented yet", service);
		return;
	}

	if (!str || strlen (str) == 0) {
		tracker_set_error (rec, "No search term was specified");
		return;
	}

	if (limit < 0) {
		limit = 1024;
	}

	tracker_log ("Executing search with params %s, %s", service, str);

	res = tracker_db_search_text (db_con, service, str, offset, limit, FALSE, FALSE);

	row_count = 0;
	array = NULL;

	if (res) {

		row_count = tracker_get_row_count (res);

		if (row_count > 0) {
			char **row;

			array = g_new (char *, row_count);

			i = 0;

			while ((row = tracker_db_get_row (res, i))) {

				if (row && row[0] && row[1]) {
					array[i] = g_build_filename (row[0], row[1], NULL);
				}
				i++;
			}

		} else {
			tracker_log ("search returned no results");
		}

		tracker_db_free_result (res);

	} else {
		array = g_new (char *, 1);
		array[0] = NULL;
	}

	reply = dbus_message_new_method_return (rec->message);

	dbus_message_append_args (reply,
	  			  DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &array, row_count,
	  			  DBUS_TYPE_INVALID);

	for (i = 0; i < row_count; i++) {
		if (array[i]) {
			g_free (array[i]);
		}
	}

	g_free (array);

	dbus_connection_send (rec->connection, reply, NULL);
	dbus_message_unref (reply);
}



void
tracker_dbus_method_search_text_detailed (DBusRec *rec)
{
	DBConnection *db_con;
	DBusError    dbus_error;
	int	     limit, query_id, offset;
	char	     *service;
	char	     ***res;
	char	     *str;

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;


/*
		More detailed version of above. Searches specified service for entities that match the specified search_text. 
		Returns hits in array format [uri, service, mime]  -->
		<method name="TextDetailed">
			<arg type="i" name="live_query_id" direction="in" />
			<arg type="s" name="service" direction="in" />
			<arg type="s" name="search_text" direction="in" />
			<arg type="i" name="offset" direction="in" />
			<arg type="i" name="max_hits" direction="in" />
			<arg type="aas" name="result" direction="out" />
		</method>
*/

	dbus_error_init (&dbus_error);
	if (!dbus_message_get_args (rec->message, NULL, DBUS_TYPE_INT32, &query_id,
			       DBUS_TYPE_STRING, &service,
			       DBUS_TYPE_STRING, &str,
			       DBUS_TYPE_INT32, &offset,
			       DBUS_TYPE_INT32, &limit,
			       DBUS_TYPE_INVALID)) {
		tracker_set_error (rec, "DBusError: %s;%s", dbus_error.name, dbus_error.message);
		dbus_error_free (&dbus_error);
		return;
	}

	if (!service)  {
		tracker_set_error (rec, "No service was specified");
		return;
	}

	if (!tracker_is_valid_service (db_con, service)) {
		tracker_set_error (rec, "Invalid service %s or service has not been implemented yet", service);
		return;
	}

	if (!str || strlen (str) == 0) {
		tracker_set_error (rec, "No search term was specified");
		return;
	}

	if (limit < 0) {
		limit = 1024;
	}

	tracker_log ("Executing detailed search with params %s, %s, %d, %d", service, str, offset, limit);

	res = tracker_db_search_text (db_con, service, str, offset, limit, FALSE, TRUE);

	tracker_dbus_reply_with_query_result (rec, res);

	tracker_db_free_result (res);
}


void
tracker_dbus_method_search_get_snippet (DBusRec *rec)
{
	DBConnection *db_con;
	DBusError    dbus_error;
	DBusMessage  *reply;
	char	     *service, *uri, *str;
	char	     *snippet, *service_id;

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;

/*
		<!-- Returns a search snippet of text with matchinhg text enclosed in bold tags -->
		<method name="GetSnippet">
			<arg type="s" name="service" direction="in" />
			<arg type="s" name="id" direction="in" />
			<arg type="s" name="search_text" direction="in" />
			<arg type="s" name="result" direction="out" />
		</method>

*/

	dbus_error_init (&dbus_error);
	if (!dbus_message_get_args (rec->message, NULL, 
			       DBUS_TYPE_STRING, &service,
			       DBUS_TYPE_STRING, &uri,
			       DBUS_TYPE_STRING, &str,
			       DBUS_TYPE_INVALID)) {
		tracker_set_error (rec, "DBusError: %s;%s", dbus_error.name, dbus_error.message);
		dbus_error_free (&dbus_error);
		return;
	}

	if (!service)  {
		tracker_set_error (rec, "No service was specified");
		return;
	}

	if (!tracker_is_valid_service (db_con, service)) {
		tracker_set_error (rec, "Invalid service %s or service has not been implemented yet", service);
		return;
	}

	if (!str || strlen (str) == 0) {
		tracker_set_error (rec, "No search term was specified");
		return;
	}


	//tracker_log ("Getting snippet with params %s, %s, %s", service, uri, str);

	service_id = tracker_db_get_id (db_con, service, uri);

	if (!service_id) {
		g_free (service_id);
		tracker_set_error (rec, "Service uri %s not found", uri);
		return;		
	}

	char ***res;
	const char *txt;

	snippet = NULL;

	res = tracker_exec_proc (db_con->user_data2, "GetFileContents", 1, service_id);
	g_free (service_id);
	
	if (res) {
		if (res[0][0]) {
			txt = res[0][0];

			char **array = 	tracker_parse_text_into_array (str);

			if (array && array[0]) {
				snippet = tracker_get_snippet (txt, array, 120);
			}

			g_strfreev (array);
				
		}
		tracker_db_free_result (res);
	}

	/* do not pass NULL to dbus or it will crash */
	if (!snippet) {
		snippet = g_strdup (" ");
	}

	reply = dbus_message_new_method_return (rec->message);

	dbus_message_append_args (reply,
	  			  DBUS_TYPE_STRING, &snippet,
	  			  DBUS_TYPE_INVALID);

	dbus_connection_send (rec->connection, reply, NULL);
	dbus_message_unref (reply);

	g_free (snippet);
}



void
tracker_dbus_method_search_files_by_text (DBusRec *rec)
{
	DBConnection 	*db_con;
	DBusError    dbus_error;
	DBusMessage 	*reply;
	DBusMessageIter iter;
	DBusMessageIter iter_dict;
	char 		*text;
	int		limit, query_id, offset;
	gboolean	sort;
	char		***res;

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;

	sort = FALSE;

/*
		<!-- searches all file based entities that match the specified search_text.
		     Returns dict/hashtable with the uri as key and the following fields as the variant part in order: file service category, File.Format, File.Size, File.Rank, File.Modified
		     If group_results is True then results are sorted and grouped by service type.
		     -->
		<method name="FilesByText">
			<arg type="i" name="live_query_id" direction="in" />
			<arg type="s" name="search_text" direction="in" />
			<arg type="i" name="max_hits" direction="in" />
			<arg type="b" name="group_results" direction="in" />
			<arg type="a{sv}" name="result" direction="out" />
		</method>

*/

	dbus_error_init (&dbus_error);
	if (!dbus_message_get_args (rec->message, NULL,
			       DBUS_TYPE_INT32, &query_id,
			       DBUS_TYPE_STRING, &text,
			       DBUS_TYPE_INT32, &offset,
			       DBUS_TYPE_INT32, &limit,
			       DBUS_TYPE_BOOLEAN, &sort,
			       DBUS_TYPE_INVALID)) {
		tracker_set_error (rec, "DBusError: %s;%s", dbus_error.name, dbus_error.message);
		dbus_error_free (&dbus_error);
		return;
	}

	res = tracker_db_search_files_by_text (db_con, text, offset, limit, sort);

	if (res) {

		reply = dbus_message_new_method_return (rec->message);

		dbus_message_iter_init_append (reply, &iter);

		dbus_message_iter_open_container (&iter,
						  DBUS_TYPE_ARRAY,
						  DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
						  DBUS_TYPE_STRING_AS_STRING
						  DBUS_TYPE_VARIANT_AS_STRING
						  DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
						  &iter_dict);

		tracker_add_query_result_to_dict (res, &iter_dict);
		tracker_db_free_result (res);

	} else {
		return;
	}

	dbus_message_iter_close_container (&iter, &iter_dict);
	dbus_connection_send (rec->connection, reply, NULL);
	dbus_message_unref (reply);
}


void
tracker_dbus_method_search_metadata (DBusRec *rec)
{
	DBConnection *db_con;
	DBusError    dbus_error;
	DBusMessage  *reply;
	char 	     *service, *field, *text;
	char 	     **array;
	int 	     limit, row_count = 0, offset;
	char	     ***res;

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;

/*
		<!-- searches a specific metadata field (field parameter) for a search term (search_text).
		     The result is an array of uri/id's
		 -->
		<method name="Metadata">
			<arg type="s" name="service" direction="in" />
			<arg type="s" name="field" direction="in" />
			<arg type="s" name="search_text"  direction="in" />
			<arg type="i" name="max_hits" direction="in" />
			<arg type="as" name="result" direction="out" />
		</method>
*/

	dbus_error_init (&dbus_error);
	if (!dbus_message_get_args (rec->message, NULL,
			       DBUS_TYPE_STRING, &service,
			       DBUS_TYPE_STRING, &field,
			       DBUS_TYPE_STRING, &text,
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

	res = tracker_db_search_metadata (db_con, service, field, text, offset, limit);

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
tracker_dbus_method_search_matching_fields (DBusRec *rec)
{
	DBConnection *db_con;
	DBusError    dbus_error;
	char 	     *text, *service, *id;
	char	     ***res;

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;

/*
		<!-- Retrieves matching metadata fields for a search term on a specific service and entity.
		     The result is a dict/hashtable with the metadata name as the key and the corresponding metadata value as the variant
		     If the metadata result is a large field (like File.Content) then only a small chunk of the matching text is returned
		     Only indexable metadata fields are searched and returned.
		 -->
		<method name="MatchingFields">
			<arg type="s" name="service" direction="in" />
			<arg type="s" name="id" direction="in" />
			<arg type="s" name="search_text"  direction="in" />
			<arg type="a{sv}" name="result" direction="out" />
		</method>

*/

	dbus_error_init (&dbus_error);
	if (!dbus_message_get_args (rec->message, NULL,
			       DBUS_TYPE_STRING, &service,
			       DBUS_TYPE_STRING, &id,
			       DBUS_TYPE_STRING, &text,
			       DBUS_TYPE_INVALID)) {
		tracker_set_error (rec, "DBusError: %s;%s", dbus_error.name, dbus_error.message);
		dbus_error_free (&dbus_error);
		return;
	}

	if (!tracker_is_valid_service (db_con, service)) {
		tracker_set_error (rec, "Invalid service %s or service has not been implemented yet", service);
		return;
	}

	if (strlen (id) == 0) {
		tracker_set_error (rec, "Id field must have a value");
		return;
	}

	res = tracker_db_search_matching_metadata (db_con, service, id, text);

	if (res) {
		DBusMessage	*reply;
		DBusMessageIter iter;
		DBusMessageIter iter_dict;

		reply = dbus_message_new_method_return (rec->message);

		dbus_message_iter_init_append (reply, &iter);

		dbus_message_iter_open_container (&iter,
						  DBUS_TYPE_ARRAY,
						  DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
						  DBUS_TYPE_STRING_AS_STRING
						  DBUS_TYPE_VARIANT_AS_STRING
						  DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
						  &iter_dict);

		tracker_add_query_result_to_dict (res, &iter_dict);
		tracker_db_free_result (res);

		dbus_message_iter_close_container (&iter, &iter_dict);
		dbus_connection_send (rec->connection, reply, NULL);
		dbus_message_unref (reply);
	}
}


void
tracker_dbus_method_search_query (DBusRec *rec)
{
	DBConnection	*db_con;
	DBusError    dbus_error;
	char		**fields;
	int		limit, row_count, query_id, offset;
	char		***res;
	char		*query, *search_text, *service, *keyword;
	gboolean	sort_results;

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;

	sort_results = FALSE;
/*
		<!-- searches specified service for matching entities.
		     The service parameter specifies the service which the query will be performed on
		     The fields parameter specifies an array of aditional metadata fields to return in addition to the id field (which is returned as the "key" in the resultant dict/hashtable) and the service category. This can be null			
		     The optional search_text paramter specifies the text to search for in a full text search of all indexed fields - this parameter can be null if the query_condition is not null (in which case only the query condition is used to find matches)
		     The optional keyword search - a single keyword may be used here to filter the results.				
		     The optional query_condition parameter specifies an xml-based rdf query condition which is used to filter out the results - this parameter can be null if the search_text is not null (in which case only the search_text parameter is used to find matches)
		     The Offset parameter sets the start row of the returned result set (useful for paging/cursors). A value of 0 should be passed to get rows from the beginning.
		     The max_hits parameter limits the size of the result set.
		     The sort_by_service parameter optionally sorts results by their service category (if FALSE no service sorting is done)
		     The result is a hashtable/dict with the id of the matching entity as the key fields.
		     The variant part of the result is the service category followed by list of supplied fields as specified in the fields parameter
		-->
		<method name="Query">
			<arg type="i" name="live_query_id" direction="in" />
			<arg type="s" name="service" direction="in" />
			<arg type="as" name="fields" direction="in" />
			<arg type="s" name="search_text" direction="in" />
			<arg type="s" name="keyword" direction="in" />
			<arg type="s" name="query_condition" direction="in" />
			<arg type="b" name="sort_by_service" direction="in" />
			<arg type="i" name="offset" direction="in" />
			<arg type="i" name="max_hits" direction="in" />
			<arg type="aas" name="result" direction="out" />
		</method>
*/
	dbus_error_init (&dbus_error);
	if (!dbus_message_get_args (rec->message, NULL,
			       DBUS_TYPE_INT32, &query_id,
			       DBUS_TYPE_STRING, &service,
			       DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &fields, &row_count,
			       DBUS_TYPE_STRING, &search_text,
			       DBUS_TYPE_STRING, &keyword,
			       DBUS_TYPE_STRING, &query,
			       DBUS_TYPE_BOOLEAN, &sort_results,
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

	if (limit < 0) {
		limit = 1024;
	}

	res = NULL;

	if (query) {
		char	 *str;
		GError	 *error = NULL;

		tracker_log ("executing rdf query %s\n with search term %s and keyword %s", query, search_text, keyword);

		str = tracker_rdf_query_to_sql (db_con, query, service, fields, row_count, search_text, keyword, sort_results, offset, limit, error);

		if (error) {
			tracker_set_error (rec, "Invalid rdf query produced following error: %s", error->message);
			g_error_free (error);		
			return;
		}

		/*if (!str) {
			g_free (search_term);
			reply = dbus_message_new_method_return (rec->message);
			dbus_message_iter_init_append (reply, &iter);

			dbus_message_iter_open_container (&iter,
							  DBUS_TYPE_ARRAY,
							  DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
							  DBUS_TYPE_STRING_AS_STRING
							  DBUS_TYPE_VARIANT_AS_STRING
							  DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
							  &iter_dict);
			dbus_message_iter_close_container (&iter, &iter_dict);

			dbus_connection_send (rec->connection, reply, NULL);
			dbus_message_unref (reply);
			g_return_if_fail (str); 
		}*/

		tracker_log ("translated rdf query is \n%s\n", str);

		if (search_text && (strlen (search_text) > 0)) {
			tracker_db_search_text (db_con, service, search_text, 0, 999999, TRUE, FALSE);
		}

		res = tracker_exec_sql_ignore_nulls (db_con, str);

		g_free (str);

	} else {
		return;
	}

	tracker_dbus_reply_with_query_result (rec, res);

	tracker_db_free_result (res);
}
