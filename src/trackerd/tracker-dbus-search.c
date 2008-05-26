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

#include <libtracker-common/tracker-log.h>
#include <libtracker-common/tracker-config.h>

#include "tracker-dbus-methods.h"
#include "tracker-rdf-query.h"
#include "tracker-query-tree.h"
#include "tracker-indexer.h"
#include "tracker-service-manager.h"

extern Tracker *tracker;

void
tracker_dbus_method_search_get_hit_count (DBusRec *rec)
{
	TrackerQueryTree *tree;
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

	if (!tracker_service_manager_is_valid_service (service)) {
		tracker_set_error (rec, "Invalid service %s or service has not been implemented yet", service);
		return;
	}

	if (tracker_is_empty_string (str)) {
		tracker_set_error (rec, "No search term was specified");
		return;
	}

	//tracker_log ("Executing GetHitCount with params %s, %s", service, str);

	gint service_array[12];
	GArray *services;
	gint result;
	DBusMessage *reply;

	service_array[0] = tracker_service_manager_get_id_for_service (service);

	if (strcmp (service, "Files") == 0) {
		service_array[1] = tracker_service_manager_get_id_for_service ("Folders");
		service_array[2] = tracker_service_manager_get_id_for_service ("Documents");
		service_array[3] = tracker_service_manager_get_id_for_service ("Images");
		service_array[4] = tracker_service_manager_get_id_for_service ("Videos");
		service_array[5] = tracker_service_manager_get_id_for_service ("Music");
		service_array[6] = tracker_service_manager_get_id_for_service ("Text");
		service_array[7] = tracker_service_manager_get_id_for_service ("Development");
		service_array[8] = tracker_service_manager_get_id_for_service ("Other");
		service_array[9] = 0;
	} else if (strcmp (service, "Emails") == 0) {
		service_array[1] = tracker_service_manager_get_id_for_service ("EvolutionEmails");
		service_array[2] = tracker_service_manager_get_id_for_service ("KMailEmails");
		service_array[3] = tracker_service_manager_get_id_for_service ("ThunderbirdEmails");
		service_array[4] = tracker_service_manager_get_id_for_service ("ModestEmails");
		service_array[5] = 0;
 	} else if (strcmp (service, "Conversations") == 0) {
		service_array[1] = tracker_service_manager_get_id_for_service ("GaimConversations");
		service_array[2] = 0;
	}

	services = g_array_new (TRUE, TRUE, sizeof (gint));
	g_array_append_vals (services, service_array, G_N_ELEMENTS (service_array));

	db_con = tracker_db_get_service_connection (db_con, service);
	tree = tracker_query_tree_new (str, db_con->word_index, services);
	result = tracker_query_tree_get_hit_count (tree);

	reply = dbus_message_new_method_return (rec->message);

	dbus_message_append_args (reply,
	  			  DBUS_TYPE_INT32,
				  &result,
	  			  DBUS_TYPE_INVALID);

	dbus_connection_send (rec->connection, reply, NULL);

	dbus_message_unref (reply);
	g_object_unref (tree);
	g_array_free (services, TRUE);
}


void
tracker_dbus_method_search_get_hit_count_all (DBusRec *rec)
{
	TrackerDBResultSet *result_set = NULL;
	TrackerQueryTree *tree;
	GArray       *hit_counts, *mail_hit_counts;
	DBConnection *db_con;
	DBusError    dbus_error;
	gchar	     *str;
	guint        i;

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

	tree = tracker_query_tree_new (str, db_con->word_index, NULL);
	hit_counts = tracker_query_tree_get_hit_counts (tree);

	tracker_query_tree_set_indexer (tree, tracker->email_index);
	mail_hit_counts = tracker_query_tree_get_hit_counts (tree);
	g_array_append_vals (hit_counts, mail_hit_counts->data, mail_hit_counts->len);
	g_array_free (mail_hit_counts, TRUE);

	for (i = 0; i < hit_counts->len; i++) {
		TrackerHitCount count;
		GValue value = { 0, };

		if (G_UNLIKELY (!result_set)) {
			result_set = _tracker_db_result_set_new (2);
		}

		count = g_array_index (hit_counts, TrackerHitCount, i);
		_tracker_db_result_set_append (result_set);

		g_value_init (&value, G_TYPE_STRING);
		g_value_take_string (&value, tracker_service_manager_get_service_by_id (count.service_type_id));
		_tracker_db_result_set_set_value (result_set, 0, &value);
		g_value_unset (&value);

		g_value_init (&value, G_TYPE_INT);
		g_value_set_int (&value, count.count);
		_tracker_db_result_set_set_value (result_set, 1, &value);
		g_value_unset (&value);
	}

	if (result_set) {
		tracker_db_result_set_rewind (result_set);
	}

	tracker_dbus_reply_with_query_result (rec, result_set);

	g_array_free (hit_counts, TRUE);
	g_object_unref (tree);

	if (result_set) {
		g_object_unref (result_set);
	}
}


void
tracker_dbus_method_search_text (DBusRec *rec)
{
	TrackerDBResultSet *result_set;
	DBConnection *db_con;
	DBusError    dbus_error;
	DBusMessage  *reply;
	gchar	     **array;
	gint	     row_count, i;
	gint	     limit, query_id, offset;
	gchar	     *service;
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

	if (!tracker_service_manager_is_valid_service (service)) {
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

	result_set = tracker_db_search_text (db_con, service, str, offset, limit, FALSE, FALSE);

	row_count = 0;
	array = NULL;

	if (result_set) {
		gboolean valid = TRUE;
		gchar *prefix, *name;

		row_count = tracker_db_result_set_get_n_rows (result_set);
		array = g_new (gchar *, row_count);
		i = 0;

		while (valid) {
			tracker_db_result_set_get (result_set,
						   0, &prefix,
						   1, &name,
						   -1);

			array[i] = g_build_filename (prefix, name, NULL);
			valid = tracker_db_result_set_iter_next (result_set);
			i++;

			g_free (prefix);
			g_free (name);
		}

		g_object_unref (result_set);
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
	TrackerDBResultSet *result_set;
	DBConnection *db_con;
	DBusError    dbus_error;
	gint	     limit, query_id, offset;
	gchar	     *service;
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

	if (!tracker_service_manager_is_valid_service (service)) {
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

	result_set = tracker_db_search_text (db_con, service, str, offset, limit, FALSE, TRUE);

	/*
	if (tracker_config_get_verbosity (tracker->config) > 0) {
		tracker_db_log_result (res);
	}
	*/

	tracker_dbus_reply_with_query_result (rec, result_set);

	if (result_set) {
		g_object_unref (result_set);
	}
}


void
tracker_dbus_method_search_get_snippet (DBusRec *rec)
{
	TrackerDBResultSet *result_set;
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

	if (!tracker_service_manager_is_valid_service (service)) {
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

	snippet = NULL;

	result_set = tracker_exec_proc (db_con->blob, "GetAllContents", service_id, NULL);
	g_free (service_id);
	
	if (result_set) {
		gchar **array, *text;

		tracker_db_result_set_get (result_set, 0, &text, -1);
		array = tracker_parse_text_into_array (str);

		if (array && array[0]) {
			snippet = tracker_get_snippet (text, array, 120);
		}

		g_strfreev (array);
		g_free (text);
		g_object_unref (result_set);
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
	TrackerDBResultSet *result_set;
	DBConnection 	*db_con;
	DBusError    dbus_error;
	DBusMessage 	*reply;
	DBusMessageIter iter;
	DBusMessageIter iter_dict;
	gchar 		*text;
	gint		limit, query_id, offset;
	gboolean	sort;

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

	result_set = tracker_db_search_files_by_text (db_con, text, offset, limit, sort);

	if (!result_set)
		return;

	reply = dbus_message_new_method_return (rec->message);

	dbus_message_iter_init_append (reply, &iter);

	dbus_message_iter_open_container (&iter,
					  DBUS_TYPE_ARRAY,
					  DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
					  DBUS_TYPE_STRING_AS_STRING
					  DBUS_TYPE_VARIANT_AS_STRING
					  DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
					  &iter_dict);

	tracker_add_query_result_to_dict (result_set, &iter_dict);
	g_object_unref (result_set);

	dbus_message_iter_close_container (&iter, &iter_dict);
	dbus_connection_send (rec->connection, reply, NULL);
	dbus_message_unref (reply);
}


void
tracker_dbus_method_search_metadata (DBusRec *rec)
{
	TrackerDBResultSet *result_set;
	DBConnection *db_con;
	DBusError    dbus_error;
	DBusMessage  *reply;
	gchar 	     *service, *field, *text;
	gchar 	     **array;
	gint 	     limit, row_count = 0, offset;

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

	if (!tracker_service_manager_is_valid_service (service)) {
		tracker_set_error (rec, "Invalid service %s or service has not been implemented yet", service);
		return;
	}

//	result_set = tracker_db_search_metadata (db_con, service, field, text, offset, limit);
	result_set = NULL;

	array = NULL;

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
tracker_dbus_method_search_matching_fields (DBusRec *rec)
{
	TrackerDBResultSet *result_set;
	DBConnection *db_con;
	DBusError    dbus_error;
	gchar 	     *text, *service, *id;

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

	if (!tracker_service_manager_is_valid_service (service)) {
		tracker_set_error (rec, "Invalid service %s or service has not been implemented yet", service);
		return;
	}

	if (tracker_is_empty_string (id)) {
		tracker_set_error (rec, "Id field must have a value");
		return;
	}
	db_con = tracker_db_get_service_connection (db_con, service);
	result_set = tracker_db_search_matching_metadata (db_con, service, id, text);

	if (result_set) {
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

		tracker_add_query_result_to_dict (result_set, &iter_dict);
		g_object_unref (result_set);

		dbus_message_iter_close_container (&iter, &iter_dict);
		dbus_connection_send (rec->connection, reply, NULL);
		dbus_message_unref (reply);
	}
}


void
tracker_dbus_method_search_query (DBusRec *rec)
{
	TrackerDBResultSet *result_set;
	DBConnection	*db_con;
	DBusError    dbus_error;
	gchar		**fields;
	gint		limit, row_count, query_id, offset;
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

	if (!tracker_service_manager_is_valid_service (service)) {
		tracker_set_error (rec, "Invalid service %s or service has not been implemented yet", service);
		return;
	}

	if (limit < 1) {
		limit = 1024;
	}

	result_set = NULL;

	if (query) {
		gchar	 *str;
		GError	 *error = NULL;

		tracker_log ("executing rdf query %s\n with search term %s and keyword %s", query, search_text, keyword);

		str = tracker_rdf_query_to_sql (db_con, query, service, fields, row_count, search_text, keyword, sort_results, offset, limit, error);

		if (error || !str) {
			if (error) {
				tracker_set_error (rec, "Invalid rdf query produced following error: %s", error->message);
				g_error_free (error);
			} else {
				tracker_set_error (rec, "Invalid rdf query");
			}
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

		result_set = tracker_db_interface_execute_query (db_con->db, NULL, str);

		g_free (str);

	} else {
		return;
	}

	tracker_dbus_reply_with_query_result (rec, result_set);

	if (result_set) {
	        g_object_unref (result_set);
	}
}




void
tracker_dbus_method_search_suggest (DBusRec *rec)
{
	DBusError	dbus_error;
	DBusMessage 	*reply;
	gchar		*term;
	gint		maxdist;
	gchar		*winner_str;

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

	Indexer *index = tracker->file_index;

        winner_str = tracker_indexer_get_suggestion (index, term, maxdist);

        if (!winner_str) {
                tracker_set_error (rec, "Possible data error in index. Aborting tracker_dbus_method_search_suggest.");
                return;
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
