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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "tracker-dbus-methods.h"
#include "tracker-rdf-query.h"

void
tracker_dbus_method_search_text (DBusRec *rec)
{
	DBConnection *db_con;
	DBusMessage *reply;
	char **array = NULL;	
	int row_count = 0, i = 0;
	int limit, query_id, offset;
	char *str = NULL, *service = NULL, *search_term = NULL;
	gboolean sort_results = FALSE, use_boolean_search = FALSE;
	char ***res = NULL;
	char**  row;

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;


/*
		<!-- searches specified service for entities that match the specified search_text. 
		     Returns id field of all hits. sort_by_relevance returns results sorted with the biggest hits first (as sorting is slower, you might want to disable this for fast queries) -->
		<method name="Text">
			<arg type="i" name="live_query_id" direction="in" />
			<arg type="s" name="service" direction="in" />
			<arg type="s" name="search_text" direction="in" />
			<arg type="i" name="offset" direction="in" />
			<arg type="i" name="max_hits" direction="in" />
			<arg type="b" name="sort_by_relevance" direction="in" />
			<arg type="as" name="result" direction="out" />
		</method>
*/
	
	dbus_message_get_args  (rec->message, NULL, DBUS_TYPE_INT32, &query_id,
				DBUS_TYPE_STRING, &service,
				DBUS_TYPE_STRING, &str,  
				DBUS_TYPE_INT32, &offset,
				DBUS_TYPE_INT32, &limit,
				DBUS_TYPE_BOOLEAN, &sort_results,
				DBUS_TYPE_INVALID);

	


	if (!service)  {
		tracker_set_error (rec, "No service was specified");	
		return;
	}

	if (!tracker_is_valid_service (db_con, service)) {
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
	search_term = tracker_format_search_terms (str, &use_boolean_search);

	char *str_limit = tracker_int_to_str (limit);
	char *str_offset = tracker_int_to_str (offset);
	char *str_sort = tracker_int_to_str (sort_results);
	char *str_bool =  tracker_int_to_str (use_boolean_search);
	
	tracker_log ("Executing search with params %s, %s. %s, %s, %s", service, search_term, str_limit, str_sort, str_bool );
	
	res = tracker_exec_proc  (db_con, "SearchText", 6, service, search_term, str_offset, str_limit, str_sort, str_bool);	

	g_free (search_term);
	g_free (str_limit);
	g_free (str_offset);
	g_free (str_bool);
	g_free (str_sort);
	
	if (res) {

		row_count = tracker_get_row_count (res);

		if (row_count > 0) {
			
			array = g_new (char *, row_count);

			i = 0;

			while ((row = tracker_db_get_row (res, i))) {
				
				if (row && row[0] && row[1]) {
					array[i] = g_strconcat (row[0], "/",  row[1], NULL);
				}
				i++;
			}
			
		} else {
			tracker_log ("search returned no results" );
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
        	g_free (array[i]);
	}

	g_free (array);

	dbus_connection_send (rec->connection, reply, NULL);
	dbus_message_unref (reply);
}
	



void
tracker_dbus_method_search_files_by_text (DBusRec *rec)
{
	DBConnection 	*db_con;
	DBusMessage 	*reply;
	DBusMessageIter iter;
	DBusMessageIter iter_dict;
	char 		*text;
	int		limit, query_id, offset;
	gboolean	sort = FALSE;

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;
	
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
		

	dbus_message_get_args  (rec->message, NULL, DBUS_TYPE_INT32, &query_id, 
			 	DBUS_TYPE_STRING, &text, 
				DBUS_TYPE_INT32, &offset,
				DBUS_TYPE_INT32, &limit,
				DBUS_TYPE_BOOLEAN, &sort,
				DBUS_TYPE_INVALID);

	char *str_offset = tracker_int_to_str (offset);
	char *str_limit = tracker_int_to_str (limit);
	char *str_sort = tracker_int_to_str (sort);
	
	gboolean na;
	char *search_term = tracker_format_search_terms (text, &na);

	char ***res = tracker_exec_proc  (db_con,  "SearchFilesText", 4, search_term, str_offset, str_limit, str_sort);
		
	g_free (search_term);			
	g_free (str_limit);
	g_free (str_offset);
	g_free (str_sort);

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
	DBConnection 	*db_con;
	DBusMessage 	*reply;
	char 		 *service, *field, *text;
	char 		**array = NULL;
	int 		limit, row_count = 0, offset;

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
		

	dbus_message_get_args  (rec->message, NULL,  
				DBUS_TYPE_STRING, &service, 
				DBUS_TYPE_STRING, &field,  
				DBUS_TYPE_STRING, &text, 
				DBUS_TYPE_INT32, &offset,
				DBUS_TYPE_INT32, &limit,
				DBUS_TYPE_INVALID);
		
	if (!tracker_is_valid_service (db_con, service)) {
		tracker_set_error (rec, "Invalid service %s or service has not been implemented yet", service);	
		return;
	}

	
	char *str_offset = tracker_int_to_str (offset);	
	char *str_limit = tracker_int_to_str (limit);
	gboolean na;
	char *search_term = tracker_format_search_terms (text, &na);

	char ***res = tracker_exec_proc  (db_con,  "SearchMetaData", 5, service, field, search_term, str_offset, str_limit);
		
	g_free (search_term);		
	g_free (str_limit);
	g_free (str_offset);

				
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
	DBConnection 	*db_con;
	DBusMessage 	*reply;
	DBusMessageIter iter;
	DBusMessageIter iter_dict;
	char 		*text, *service, *id, *path, *name;

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
		

	dbus_message_get_args  (rec->message, NULL,  
			 	DBUS_TYPE_STRING, &service, 
				DBUS_TYPE_STRING, &id, 
				DBUS_TYPE_STRING, &text, 
				DBUS_TYPE_INVALID);


	if (!tracker_is_valid_service (db_con, service)) {
		tracker_set_error (rec, "Invalid service %s or service has not been implemented yet", service);	
		return;
	}

	if (strlen (id) == 0) {
		tracker_set_error (rec, "Id field must have a value");	
		return;
	}

	

	if (id[0] == '/') {
		name = g_path_get_basename (id);
		path = g_path_get_dirname (id);
	} else {
		name = tracker_get_vfs_name (id);
		path = tracker_get_vfs_path (id);
	}

	gboolean na;
	char *search_term = tracker_format_search_terms (text, &na);

	char ***res = tracker_exec_proc  (db_con,  "SearchMatchingMetaData", 4, service, path, name, search_term);
	
	g_free (search_term);			
	g_free (name);
	g_free (path);



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

		dbus_message_iter_close_container (&iter, &iter_dict);
		dbus_connection_send (rec->connection, reply, NULL);
		dbus_message_unref (reply);

	}

	
}


void
tracker_dbus_method_search_query (DBusRec *rec)
{
	DBConnection *db_con;
	DBusMessage *reply;
	DBusMessageIter iter;
	DBusMessageIter iter_dict;
	char **fields = NULL;				
	int  limit, row_count, query_id, offset;
	char ***res = NULL;
	char *str, *query, *search_text, *service;
	gboolean sort_results = FALSE;
	GError *error;

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;
	
/*

		<!-- searches specified service for matching entities.
		     The service parameter specifies the service which the query will be performed on
		     The fields parameter specifies an array of aditional metadata fields to return in addition to the id field (which is returned as the "key" in the resultant dict/hashtable) and the service category. This can be null			
		     The search_text paramter specifies the text to search for in a full text search of all indexed fields - this parameter can be null if the query_condition is not null (in which case only the query condition is used to find matches)
		     The query_condition parameter specifies an xml-based rdf query condition which is used to filter out the results - this parameter can be null if the search_text is not null (in which case only the search_text parameter is used to find matches)
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
			<arg type="s" name="query_condition" direction="in" />
			<arg type="b" name="sort_by_service" direction="in" />
			<arg type="i" name="offset" direction="in" />
			<arg type="i" name="max_hits" direction="in" />
			<arg type="a{sv}" name="result" direction="out" />
		</method>
*/
 
	dbus_message_get_args  (rec->message, NULL, DBUS_TYPE_INT32, &query_id,
				DBUS_TYPE_STRING, &service,
				DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &fields, &row_count,
				DBUS_TYPE_STRING, &search_text,
				DBUS_TYPE_STRING, &query,
				DBUS_TYPE_BOOLEAN, &sort_results,
				DBUS_TYPE_INT32, &offset,
				DBUS_TYPE_INT32, &limit,
				DBUS_TYPE_INVALID);

	if (!tracker_is_valid_service (db_con, service)) {
		tracker_set_error (rec, "Invalid service %s or service has not been implemented yet", service);	
		return;
	}

	if (limit < 0) {
		limit = 1024;
	}

	if (query) {

		tracker_log ("executing rdf query %s\n", query);
		error = NULL;

		gboolean na;
		char *search_term = tracker_format_search_terms (search_text, &na);

		str = tracker_rdf_query_to_sql (db_con, query, service, fields, row_count, search_term, sort_results, offset, limit , NULL);

		if (! str) {
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
			g_return_if_fail (str); /* show us a warning */
		}

		g_free (search_term);
		tracker_log ("translated rdf query is %s\n", str);
		res = tracker_exec_sql (db_con, str);
		
		g_free (str);
	} else {
		return;
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
		tracker_db_free_result (res);
	}

	dbus_message_iter_close_container (&iter, &iter_dict);
	dbus_connection_send (rec->connection, reply, NULL);
	dbus_message_unref (reply);

}
