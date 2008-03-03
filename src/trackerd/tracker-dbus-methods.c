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
#include <glib/gstdio.h>

#include "tracker-dbus-methods.h"
#include "tracker-metadata.h"
#include "tracker-rdf-query.h"

#include "config.h"

#ifdef OS_WIN32
#include "mingw-compat.h"
#endif


extern Tracker *tracker;

void
tracker_set_error (DBusRec 	  *rec,
	   	   const char	  *fmt,
	   	   ...)
{
	char	    *msg;
	va_list	    args;
	DBusMessage *reply;

	va_start (args, fmt);
	msg = g_strdup_vprintf (fmt, args);
	va_end (args);

	reply = dbus_message_new_error (rec->message,
					"org.freedesktop.Tracker.Error",
					msg);

	if (reply == NULL || !dbus_connection_send (rec->connection, reply, NULL)) {
		tracker_error ("WARNING: out of memory");
	}

	tracker_error ("ERROR: %s", msg);
	g_free (msg);

	dbus_message_unref (reply);
}


char *
tracker_get_metadata (DBConnection *db_con, const char *service, const char *id, const char *key)
{
	char ***res;
	char *value;

	g_return_val_if_fail (db_con && !tracker_is_empty_string (id), NULL);

	value = g_strdup (" ");

	res = tracker_db_get_metadata (db_con, service, id, key);

	if (res) {
		int  row_count;

		row_count = tracker_get_row_count (res);

		if (row_count > 0) {
			char **row;

			row = tracker_db_get_row (res, 0);

			if (row && row[0]) {
				g_free (value);
				value = g_strdup (row[0]);
			}

		} else {
			tracker_log ("Result set is empty");
		}

		tracker_db_free_result (res);
	}

	tracker_log ("Metadata %s is %s", key, value);

	return value;
}


guint32
tracker_get_file_id (DBConnection *db_con, const char *uri, gboolean create_record)
{
	int id;

	g_return_val_if_fail (db_con && !tracker_is_empty_string (uri), 0);

	id = tracker_db_get_file_id (db_con, uri);

	if (id == 0 && create_record) {
		char	    *uri_in_locale;

		uri_in_locale = g_filename_from_utf8 (uri, -1, NULL, NULL, NULL);

		if (!uri_in_locale) {
			tracker_error ("ERROR: info->uri could not be converted to locale format");
			return 0;
		}

		/* file not found in DB - so we must insert a new file record */

		
		char *str_file_id, *service;
		FileInfo *info;
	
		info = NULL;
		service = NULL;
		str_file_id = NULL;

		info = tracker_create_file_info (uri_in_locale, 1, 0, 0);

		if (!tracker_file_is_valid (uri_in_locale)) {
			info->mime = g_strdup ("unknown");
			service = g_strdup ("Files");
		} else {
			info->mime = tracker_get_mime_type (uri_in_locale);
			service = tracker_get_service_type_for_mime (info->mime);
			info = tracker_get_file_info (info);
		}

		id = tracker_db_create_service (db_con, "Files", info);
		tracker_free_file_info (info);
		g_free (service);

		g_free (uri_in_locale);

		
	}

	return id;
}


void
tracker_dbus_reply_with_query_result (DBusRec *rec, char ***res)
{
	DBusMessage	*reply;
	DBusMessageIter iter;
	DBusMessageIter iter2;
	char **row;
	int  k;

	reply = dbus_message_new_method_return (rec->message);

	dbus_message_iter_init_append (reply, &iter);

	dbus_message_iter_open_container (&iter,
					  DBUS_TYPE_ARRAY, 
					  "as",
					  &iter2);

	k = 0;

	while ((row = tracker_db_get_row (res, k)) != NULL) {

		DBusMessageIter iter_array;
		char 		**values;	

		k++;

		dbus_message_iter_open_container (&iter2,
						  DBUS_TYPE_ARRAY,
						  DBUS_TYPE_STRING_AS_STRING,
						  &iter_array);

		/* append fields to the array */
		for (values = row; *values; values++) {
			char *value;

			if (!tracker_is_empty_string (*values)) {
				value = *values;
				//tracker_log (value);
			} else {
				/* dbus does not like NULLs */
				value = " ";
			}

			dbus_message_iter_append_basic (&iter_array,
							DBUS_TYPE_STRING,
							&value);
		}


		dbus_message_iter_close_container (&iter2, &iter_array);

	}


	dbus_message_iter_close_container (&iter, &iter2);
	dbus_connection_send (rec->connection, reply, NULL);
	dbus_message_unref (reply);

}



void
tracker_add_query_result_to_dict (char ***res, DBusMessageIter *iter_dict)
{
	int  row_count;

	g_return_if_fail (res);

	row_count = tracker_get_row_count (res);

	if (row_count > 0) {
		char **row;
		int  field_count;
		int  k;

		field_count = tracker_get_field_count (res);

		k = 0;

		while ((row = tracker_db_get_row (res, k)) != NULL) {
			DBusMessageIter iter_dict_entry;
			DBusMessageIter iter_var, iter_array;
			char		*key;
			int		i;

			k++;

			if (row[0]) {
				key = row[0];
			} else {
				continue;
			}

			dbus_message_iter_open_container (iter_dict,
						  	  DBUS_TYPE_DICT_ENTRY,
						  	  NULL,
							  &iter_dict_entry);

			dbus_message_iter_append_basic (&iter_dict_entry, DBUS_TYPE_STRING, &key);


			dbus_message_iter_open_container (&iter_dict_entry,
							  DBUS_TYPE_VARIANT,
							  DBUS_TYPE_ARRAY_AS_STRING
							  DBUS_TYPE_STRING_AS_STRING,
							  &iter_var);

			dbus_message_iter_open_container (&iter_var,
							  DBUS_TYPE_ARRAY,
							  DBUS_TYPE_STRING_AS_STRING,
							  &iter_array);

			/* append additional fields to the variant */
			for (i = 1; i < field_count; i++) {
				char *value;

				if (!tracker_is_empty_string (row[i])) {
					value = g_strdup (row[i]);
					} else {
					/* dbus does not like NULLs */
					value = g_strdup (" ");
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
		tracker_log ("Result set is empty");
	}
}


char **
tracker_get_query_result_as_array (char ***res, int *row_count)
{
	char **array;

	*row_count = tracker_get_row_count (res);

	if (*row_count > 0) {
		char **row;
		int  i;

		array = g_new (char *, *row_count);

		i = 0;

		while ((row = tracker_db_get_row (res, i))) {

			if (row && row[0]) {
				array[i] = g_strdup (row[0]);
			} else {
				array[i] = NULL;
			}
			i++;
		}

	} else {
		array = g_new (char *, 1);

		array[0] = NULL;
	}

	return array;
}


void
tracker_dbus_method_get_services (DBusRec *rec)
{
	DBConnection	*db_con;
	DBusError	dbus_error;
	DBusMessage	*reply;
	DBusMessageIter iter;
	DBusMessageIter iter_dict;
	gboolean	main_only;
	char		***res;

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;

	tracker_log ("Executing GetServices Dbus Call");

	dbus_error_init (&dbus_error);

	if (!dbus_message_get_args (rec->message, NULL, DBUS_TYPE_BOOLEAN, &main_only, DBUS_TYPE_INVALID)) {
		tracker_set_error (rec, "DBusError: %s;%s", dbus_error.name, dbus_error.message);
		dbus_error_free (&dbus_error);
		return;
	}
	
	res = tracker_exec_proc (db_con, "GetServices", 0);

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


void
tracker_dbus_method_get_stats (DBusRec *rec)
{
	DBConnection	*db_con;
	char		***res;

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;

	tracker_log ("Executing GetStats Dbus Call");

	res = tracker_exec_proc (db_con, "GetStats", 0);
	
	tracker_dbus_reply_with_query_result (rec, res);

	tracker_db_free_result (res);

}

void
tracker_dbus_method_get_status (DBusRec *rec)
{
	DBusMessage *reply = dbus_message_new_method_return (rec->message);
        
                         
        gchar* status = tracker_get_status ();

        dbus_message_append_args (reply,
                                  DBUS_TYPE_STRING, &status,
                                  DBUS_TYPE_INVALID);

	g_free (status);

        dbus_connection_send (rec->connection, reply, NULL);

	dbus_message_unref (reply);

}

void
tracker_dbus_method_get_version (DBusRec *rec)
{
	DBusMessage *reply;
	int	    i;

	g_return_if_fail (rec);

	i = TRACKER_VERSION_INT;

	reply = dbus_message_new_method_return (rec->message);

	dbus_message_append_args (reply,
	  			  DBUS_TYPE_INT32,
				  &i,
	  			  DBUS_TYPE_INVALID);

	dbus_connection_send (rec->connection, reply, NULL);

	dbus_message_unref (reply);
}

void
tracker_dbus_method_set_bool_option (DBusRec *rec)
{
	DBusMessage 	*reply;
	DBusError   	dbus_error;
	char 		*option = NULL;
	gboolean 	value = FALSE;

	g_return_if_fail (rec);

	dbus_error_init (&dbus_error);

	/*	<!-- sets boolean options in tracker - option can be one of "Pause", "EnableIndexing", "LowMemoryMode", "IndexFileContents", "EnableEvolution" -->
		<method name="SetBoolOption">
			<arg type="s" name="option" direction="in" />
			<arg type="b" name="value" direction="in" />
		</method>

	*/

	if (!dbus_message_get_args (rec->message, NULL, DBUS_TYPE_STRING, &option, DBUS_TYPE_BOOLEAN, &value, DBUS_TYPE_INVALID)) {
		tracker_set_error (rec, "DBusError: %s;%s", dbus_error.name, dbus_error.message);
		dbus_error_free (&dbus_error);
		return;
	}

	if (strcasecmp (option, "Pause") == 0) {
		tracker->pause_manual = value;

		tracker_dbus_send_index_status_change_signal ();
		
		if (value) {
			tracker_log ("trackerd is paused by user");
		} else {
			tracker_log ("trackerd is unpaused by user");
		}
		
	} else if (strcasecmp (option, "FastMerges") == 0) {
		tracker->fast_merges = value;
		tracker_log ("fast merges set to %d", value);
	} else if (strcasecmp (option, "EnableIndexing") == 0) {
		tracker->enable_indexing = value;
		tracker_log ("Enable indexing set to %d", value);
		tracker_dbus_send_index_status_change_signal ();


	} else if (strcasecmp (option, "EnableWatching") == 0) {
		tracker->enable_watching = value;
		tracker_log ("Enable Watching set to %d", value);
	} else if (strcasecmp (option, "LowMemoryMode") == 0) {
		tracker->use_extra_memory = !value;
		tracker_log ("Extra memory usage set to %d", !value);
	} else if (strcasecmp (option, "IndexFileContents") == 0) {
		tracker->enable_content_indexing = value;
		tracker_log ("Index file contents set to %d", value);	
	} else if (strcasecmp (option, "GenerateThumbs") == 0) {
		tracker->enable_thumbnails = value;
		tracker_log ("Generate thumbnails set to %d", value);	
	} else if (strcasecmp (option, "SkipMountPoints") == 0) {
		tracker->skip_mount_points = value;
		tracker_log ("Skip mounted directories set to %d", value);
	} else if (strcasecmp (option, "EnableEvolution") == 0) {
		tracker->index_evolution_emails = value;
		tracker_log ("evolution support set to %d", value);
	} else if (strcasecmp (option, "FastMerges") == 0) {
		tracker->fast_merges = value;
		tracker_log ("Fast merges set to %d", value);
	} else if (strcasecmp (option, "BatteryIndex") == 0) {
		tracker->index_on_battery = value;
		tracker_log ("Disable index on battery set to %d", !value);
	} else if (strcasecmp (option, "BatteryIndexInitial") == 0) {
		tracker->index_on_battery = value;
		tracker_log ("Disable initial index sweep on battery set to %d", !value);
	}

	tracker_notify_file_data_available ();

	reply = dbus_message_new_method_return (rec->message);

	dbus_connection_send (rec->connection, reply, NULL);

	dbus_message_unref (reply);
}


void
tracker_dbus_method_set_int_option (DBusRec *rec)
{
	DBusMessage 	*reply;
	DBusError   	dbus_error;
	char 		*option = NULL;
	int 		value = 0;

	g_return_if_fail (rec);

	dbus_error_init (&dbus_error);

	/*	<!-- sets integer based option values in tracker - option can be one of "Throttle", "IndexDelay" -->
		<method name="SetIntOption">
			<arg type="s" name="option" direction="in" />
			<arg type="i" name="value" direction="in" />
		</method>

	*/

	if (!dbus_message_get_args (rec->message, NULL, DBUS_TYPE_STRING, &option, DBUS_TYPE_INT32, &value, DBUS_TYPE_INVALID)) {
		tracker_set_error (rec, "DBusError: %s;%s", dbus_error.name, dbus_error.message);
		dbus_error_free (&dbus_error);
		return;
	}

	if (strcasecmp (option, "Throttle") == 0) {
		tracker->throttle = value;
		tracker_log ("throttle set to %d", value);
	} else if (strcasecmp (option, "MaxText") == 0) {
		tracker->max_index_text_length = value;
		tracker_log ("Maxinum amount of text set to %d", value);
	} else if (strcasecmp (option, "MaxWords") == 0) {
		tracker->max_words_to_index = value;
		tracker_log ("Maxinum number of unique words set to %d", value);
	} 

	reply = dbus_message_new_method_return (rec->message);

	dbus_connection_send (rec->connection, reply, NULL);

	dbus_message_unref (reply);
}


void
tracker_dbus_method_shutdown (DBusRec *rec)
{
	DBusMessage 	*reply;
	DBusError   	dbus_error;
	gboolean 	reindex = FALSE;

	g_return_if_fail (rec);

	dbus_error_init (&dbus_error);

	/*	<!-- shutdown tracker service with optional reindex -->
		<method name="Shutdown">
			<arg type="b" name="reindex" direction="in" />
		</method>

	*/

	if (!dbus_message_get_args (rec->message, NULL, DBUS_TYPE_BOOLEAN, &reindex, DBUS_TYPE_INVALID)) {
		tracker_set_error (rec, "DBusError: %s;%s", dbus_error.name, dbus_error.message);
		dbus_error_free (&dbus_error);
		return;
	}

	tracker_log ("attempting restart");

	tracker->reindex = reindex;

	g_timeout_add (500, (GSourceFunc) tracker_do_cleanup, NULL);

	reply = dbus_message_new_method_return (rec->message);

	dbus_connection_send (rec->connection, reply, NULL);

	dbus_message_unref (reply);
}			
 

void
tracker_dbus_method_prompt_index_signals (DBusRec *rec)
{
	DBusMessage 	*reply;

	g_return_if_fail (rec);

	tracker_dbus_send_index_status_change_signal ();

	tracker_dbus_send_index_progress_signal ("Files", "");
	tracker_dbus_send_index_progress_signal ("Emails", "");

	reply = dbus_message_new_method_return (rec->message);

	dbus_connection_send (rec->connection, reply, NULL);

	dbus_message_unref (reply);
}			



