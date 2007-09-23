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
#include "tracker-rdf-query.h"
#include "tracker-indexer.h"

extern Tracker *tracker;


void
tracker_dbus_method_search_get_hit_count (DBusRec *rec)
{
	DBConnection *db_con;
	DBusError    dbus_error;
	gchar	     *service;
	gchar	     *str;

	g_return_if_fail (rec);
	g_return_if_fail (rec->user_data);

	db_con = rec->user_data;

/*
	<!--  returns no of hits for the search_text on the servce -->
		<method name="GetHitCount">
			<arg type="s" name="service" direction="in" />
			<arg type="s" name="search_text" direction="in" />
			<arg type="i" name="result" direction="out" />
		</method>

	
*/

	dbus_error_init (&dbus_error);
	if (!dbus_message_get_args (rec->message, NULL, 
			       DBUS_TYPE_STRING, &service,
			       DBUS_TYPE_STRING, &str,
			       DBUS_TYPE_INVALID)) {
		tracker_set_error (rec, "DBusError: %s;%s", dbus_error.name, dbus_error.message);
		dbus_error_free (&dbus_error);
		return;
	}

	if (!service) {
		tracker_set_error (rec, "No service was specified");
		return;
	}

	if (!tracker_is_valid_service (db_con, service)) {
		tracker_set_error (rec, "Invalid service %s or service has not been implemented yet", service);
		return;
	}

	if (tracker_is_empty_string (str)) {
		tracker_set_error (rec, "No search term was specified");
		return;
	}

	//tracker_log ("Executing GetHitCount with params %s, %s", service, str);

	gchar **array;
	gint service_array[1];
	gint result;
	DBusMessage *reply;

	service_array[0] = tracker_get_id_for_service (service);

	db_con = tracker_db_get_service_connection (db_con, service);

	SearchQuery *query = tracker_create_query (db_con->word_index, service_array, 1, 0, 999999);

	array = tracker_parse_text_into_array (str);

	gchar **pstr;

	for (pstr = array; *pstr; pstr++) {
		tracker_add_query_word (query, *pstr, WordNormal);
	}

	g_strfreev (array);

	result = tracker_get_hit_count (query);

	tracker_free_query (query);

	reply = dbus_message_new_method_return (rec->message);

	dbus_message_append_args (reply,
	  			  DBUS_TYPE_INT32,
				  &result,
	  			  DBUS_TYPE_INVALID);

	dbus_connection_send (rec->connection, reply, NULL);

	dbus_message_unref (reply);
}


void
tracker_dbus_method_search_get_hit_count_all (DBusRec *rec)
{
	DBConnection *db_con;
	DBusError    dbus_error;
	gchar	     *str;

	g_return_if_fail (rec);
	g_return_if_fail (rec->user_data);

	db_con = rec->user_data;

/*
		<!--  returns [service name, no. of hits] for the search_text -->
		<method name="GetHitCountAll">
			<arg type="s" name="search_text" direction="in" />
			<arg type="aas" name="result" direction="out" />
		</method>
	
*/

	dbus_error_init (&dbus_error);
	if (!dbus_message_get_args (rec->message, NULL, 
			       DBUS_TYPE_STRING, &str,
			       DBUS_TYPE_INVALID)) {
		tracker_set_error (rec, "DBusError: %s;%s", dbus_error.name, dbus_error.message);
		dbus_error_free (&dbus_error);
		return;
	}

	if (tracker_is_empty_string (str)) {
		tracker_set_error (rec, "No search term was specified");
		return;
	}

	//tracker_log ("Executing detailed search with params %s, %s, %d, %d", service, str, offset, limit);

	gchar ***res, **array;

	SearchQuery *query = tracker_create_query (db_con->word_index, NULL, 0, 0, 999999);

	array = tracker_parse_text_into_array (str);

	gchar **pstr;

	for (pstr = array; *pstr; pstr++) {
		tracker_add_query_word (query, *pstr, WordNormal);	
	}

	g_strfreev (array);

	res = tracker_get_hit_counts (query);

	tracker_free_query (query);	

	tracker_dbus_reply_with_query_result (rec, res);
}


void
tracker_dbus_method_search_text (DBusRec *rec)
{
	DBConnection *db_con;
	DBusError    dbus_error;
	DBusMessage  *reply;
	gchar	     **array;
	gint	     row_count, i;
	gint	     limit, query_id, offset;
	gchar	     *service;
	gchar	     ***res;
	gchar	     *str;

	g_return_if_fail (rec);
	g_return_if_fail (rec->user_data);

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

	if (!service) {
		tracker_set_error (rec, "No service was specified");
		return;
	}

	if (!tracker_is_valid_service (db_con, service)) {
		tracker_set_error (rec, "Invalid service %s or service has not been implemented yet", service);
		return;
	}

	if (tracker_is_empty_string (str)) {
		tracker_set_error (rec, "No search term was specified");
		return;
	}

	if (limit < 1) {
		limit = 1024;
	}

	tracker_log ("Executing search with params %s, %s", service, str);

	db_con = tracker_db_get_service_connection (db_con, service);

	res = tracker_db_search_text (db_con, service, str, offset, limit, FALSE, FALSE);

	row_count = 0;
	array = NULL;

	if (res) {

		row_count = tracker_get_row_count (res);

		if (row_count > 0) {
			gchar **row;

			array = g_new (gchar *, row_count);

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
		array = g_new (gchar *, 1);
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
	gint	     limit, query_id, offset;
	gchar	     *service;
	gchar	     ***res;
	gchar	     *str;

	g_return_if_fail (rec);
	g_return_if_fail (rec->user_data);

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

	if (!service) {
		tracker_set_error (rec, "No service was specified");
		return;
	}

	if (!tracker_is_valid_service (db_con, service)) {
		tracker_set_error (rec, "Invalid service %s or service has not been implemented yet", service);
		return;
	}

	if (tracker_is_empty_string (str)) {
		tracker_set_error (rec, "No search term was specified");
		return;
	}

	if (limit < 1) {
		limit = 1024;
	}

	tracker_log ("Executing detailed search with params %s, %s, %d, %d", service, str, offset, limit);

	db_con = tracker_db_get_service_connection (db_con, service);

	res = tracker_db_search_text (db_con, service, str, offset, limit, FALSE, TRUE);

	if (tracker->verbosity > 0) {
		tracker_db_log_result (res);
	}

	tracker_dbus_reply_with_query_result (rec, res);

	//tracker_db_free_result (res);
}


void
tracker_dbus_method_search_get_snippet (DBusRec *rec)
{
	DBConnection *db_con;
	DBusError    dbus_error;
	DBusMessage  *reply;
	gchar	     *service, *uri, *str;
	gchar	     *snippet, *service_id;

	g_return_if_fail (rec);
	g_return_if_fail (rec->user_data);

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

	if (!service) {
		tracker_set_error (rec, "No service was specified");
		return;
	}

	if (!tracker_is_valid_service (db_con, service)) {
		tracker_set_error (rec, "Invalid service %s or service has not been implemented yet", service);
		return;
	}

	if (tracker_is_empty_string (str)) {
		tracker_set_error (rec, "No search term was specified");
		return;
	}

	//tracker_log ("Getting snippet with params %s, %s, %s", service, uri, str);
	db_con = tracker_db_get_service_connection (db_con, service);
	service_id = tracker_db_get_id (db_con, service, uri);

	if (!service_id) {
		g_free (service_id);
		tracker_set_error (rec, "Service uri %s not found", uri);
		return;		
	}

	gchar ***res;
	const gchar *txt;

	snippet = NULL;

	res = tracker_exec_proc (db_con->blob, "GetAllContents", 1, service_id);
	g_free (service_id);
	
	if (res) {
		if (res[0][0]) {
			txt = res[0][0];

			gchar **array = 	tracker_parse_text_into_array (str);

			if (array && array[0]) {
				snippet = tracker_get_snippet (txt, array, 120);
			}

			g_strfreev (array);

			tracker_db_free_result (res);
				
		}
		
	}

	/* do not pass NULL to dbus or it will crash */
	if (!snippet || !g_utf8_validate (snippet, -1, NULL) ) {
		snippet = g_strdup (" ");
	}

//	tracker_debug ("snippet is %s", snippet);

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
	gchar 		*text;
	gint		limit, query_id, offset;
	gboolean	sort;
	gchar		***res;

	g_return_if_fail (rec);
	g_return_if_fail (rec->user_data);

	db_con = rec->user_data;

	sort = FALSE;

/*
		<!-- searches all file based entities that match the specified search_text.
		     Returns dict/hashtable with the uri as key and the following fields as the variant part in order: file service category, File:Format, File:Size, File:Rank, File:Modified
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
	gchar 	     *service, *field, *text;
	gchar 	     **array;
	gint 	     limit, row_count = 0, offset;
	gchar	     ***res;

	g_return_if_fail (rec);
	g_return_if_fail (rec->user_data);

	db_con = rec->user_data;

/*
		<!-- searches a specific metadata field (field parameter) for a search term (search_text).
		     The result is an array of uri/id's
		 -->
		<method name="Metadata">
			<arg type="s" name="service" direction="in" />
			<arg type="s" name="field" direction="in" />
			<arg type="s" name="search_text" direction="in" />
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

//	res = tracker_db_search_metadata (db_con, service, field, text, offset, limit);
	res = NULL;

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
	gchar 	     *text, *service, *id;
	gchar	     ***res;

	g_return_if_fail (rec);
	g_return_if_fail (rec->user_data);

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
			<arg type="s" name="search_text" direction="in" />
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

	if (tracker_is_empty_string (id)) {
		tracker_set_error (rec, "Id field must have a value");
		return;
	}
	db_con = tracker_db_get_service_connection (db_con, service);
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
	gchar		**fields;
	gint		limit, row_count, query_id, offset;
	gchar		***res;
	gchar		*query, *search_text, *service, *keyword;
	gboolean	sort_results;

	g_return_if_fail (rec);
	g_return_if_fail (rec->user_data);

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

	if (limit < 1) {
		limit = 1024;
	}

	res = NULL;

	if (query) {
		gchar	 *str;
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
		db_con = tracker_db_get_service_connection (db_con, service);
		if (!tracker_is_empty_string (search_text)) {
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


/* int levenshtein ()
 * Original license: GNU Lesser Public License
 * from the Dixit project, (http://dixit.sourceforge.net/)
 * Author: Octavian Procopiuc <oprocopiuc@gmail.com>
 * Created: July 25, 2004
 * Copied into tracker, by Edward Duffy
 */

static int
levenshtein(char *source, char *target, int maxdist)
{
	char n, m;
	int l;
	l = strlen (source);
	if (l > 50)
		return -1;
	n = l;

	l = strlen (target);
	if (l > 50)
		return -1;
	m = l;

	if (maxdist == 0)
		maxdist = MAX(m, n);
	if (n == 0)
		return MIN(m, maxdist);
	if (m == 0)
		return MIN(n, maxdist);

	// Store the min. value on each column, so that, if it reaches
	// maxdist, we break early.
	char mincolval;

	char matrix[51][51];

	char j;
	char i;
	char cell;

	for (j = 0; j <= m; j++)
		matrix[0][(int)j] = j;

	for (i = 1; i <= n; i++) {

		mincolval = MAX(m, i);
		matrix[(int)i][0] = i;

		char s_i = source[i-1];

		for (j = 1; j <= m; j++) {

			char t_j = target[j-1];

			char cost = (s_i == t_j ? 0 : 1);

			char above = matrix[i-1][(int)j];
			char left = matrix[(int)i][j-1];
			char diag = matrix[i-1][j-1];
			cell = MIN(above + 1, MIN(left + 1, diag + cost));

			// Cover transposition, in addition to deletion,
			// insertion and substitution. This step is taken from:
			// Berghel, Hal ; Roach, David : "An Extension of Ukkonen's 
			// Enhanced Dynamic Programming ASM Algorithm"
			// (http://www.acm.org/~hlb/publications/asm/asm.html)

			if (i > 2 && j > 2) {
				char trans = matrix[i-2][j-2] + 1;
				if (source[i-2] != t_j)
					trans++;
				if (s_i != target[j-2])
					trans++;
				if (cell > trans)
					cell = trans;
			}

			mincolval = MIN(mincolval, cell);
			matrix[(int)i][(int)j] = cell;
		}

		if (mincolval >= maxdist)
			break;

	}

	if (i == n + 1)
		return (int) matrix[(int)n][(int)m];
	else
		return maxdist;
}


void
tracker_dbus_method_search_suggest (DBusRec *rec)
{
	DBusError	dbus_error;
	DBusMessage 	*reply;
	gchar		*term, *str;
	gint		maxdist;
	gint		dist, tsiz;
	gchar		*winner_str;
	gint		winner_dist;
	char		*tmp;
	int		hits;
	GTimeVal	start, current;

	/*
		<method name="Suggest">
			<arg type="s" name="search_text" direction="in" />
			<arg type="i" name="maxdist" direction="in" />
			<arg type="s" name="result" direction="out" />
		</method>
	*/

	dbus_error_init (&dbus_error);
	if (!dbus_message_get_args (rec->message, NULL,
			       DBUS_TYPE_STRING, &term,
			       DBUS_TYPE_INT32, &maxdist,
			       DBUS_TYPE_INVALID)) {
		tracker_set_error (rec, "DBusError: %s;%s", dbus_error.name, dbus_error.message);
		dbus_error_free (&dbus_error);
		return;
	}

	winner_str = NULL;
        winner_dist = -1;  /* to initialize winner_dist with something */


	Indexer *index = tracker->file_index;

	dpiterinit (index->word_index);

	g_get_current_time (&start);

	str = dpiternext (index->word_index, NULL);
	while (str != NULL) {
		dist = levenshtein (term, str, 0);
		if (dist != -1 && dist < maxdist) {
			hits = 0;
			if ((tmp = dpget (index->word_index, str, -1, 0, -1, &tsiz)) != NULL) {
				hits = tsiz / sizeof (WordDetails);
				free (tmp);
				if (tsiz % sizeof (WordDetails) != 0) {
					tracker_set_error (rec, "Possible data error from dpget  Aborting tracker_dbus_method_search_suggest.");
					g_free (str);
					if (winner_str) {
						g_free (winner_str);
					}
					return;
				}
			}
			if (hits > 0) {
				if (winner_str == NULL) {
					winner_str = strdup (str);
					winner_dist = dist;
				}
				else if (dist < winner_dist) {
					free (winner_str);
					winner_str = strdup (str);
					winner_dist = dist;
				}
			}
			else {
				tracker_log ("No hits for %s!", str);
			}
		}
		free (str);
		g_get_current_time (&current);
		if (current.tv_sec - start.tv_sec >= 2) { /* 2 second time out */
			tracker_log ("Timeout in tracker_dbus_method_search_suggest");
			break;
		}
		str = dpiternext (index->word_index, NULL);
	}

	if (winner_str == NULL) {
		winner_str = strdup (term);
	}

	tracker_log ("Suggested spelling for %s is %s.", term, winner_str);

	reply = dbus_message_new_method_return (rec->message);

	dbus_message_append_args (reply,
	  			  DBUS_TYPE_STRING, &winner_str,
	  			  DBUS_TYPE_INVALID);
	free (winner_str);

	dbus_connection_send (rec->connection, reply, NULL);

	dbus_message_unref (reply);

}
