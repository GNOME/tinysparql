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

#include "config.h"

#include <string.h>

#ifdef OS_WIN32
#include "mingw-compat.h"
#endif

#include <glib/gstdio.h>

#include <libtracker-common/tracker-log.h>
#include <libtracker-common/tracker-config.h>

#include "tracker-dbus-methods.h"
#include "tracker-metadata.h"
#include "tracker-rdf-query.h"

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
	TrackerDBResultSet *result_set;
	char *value;

	g_return_val_if_fail (db_con && !tracker_is_empty_string (id), NULL);

	value = g_strdup (" ");

	result_set = tracker_db_get_metadata (db_con, service, id, key);

	if (result_set) {
		tracker_db_result_set_get (result_set, 0, &value, -1);
		g_object_unref (result_set);
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
			service = tracker_service_manager_get_service_type_for_mime (info->mime);
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
tracker_dbus_reply_with_query_result (DBusRec            *rec,
				      TrackerDBResultSet *result_set)
{
	DBusMessage	*reply;
	DBusMessageIter iter;
	DBusMessageIter iter2;
	gboolean valid = TRUE;
	gint columns;

	if (result_set) {
		columns = tracker_db_result_set_get_n_columns (result_set);
	}

	reply = dbus_message_new_method_return (rec->message);
	dbus_message_iter_init_append (reply, &iter);

	dbus_message_iter_open_container (&iter,
					  DBUS_TYPE_ARRAY, 
					  "as",
					  &iter2);

	while (result_set && valid) {
		DBusMessageIter iter_array;
		gint            i;
		GValue          transform = { 0, };

		g_value_init (&transform, G_TYPE_STRING);
		dbus_message_iter_open_container (&iter2,
						  DBUS_TYPE_ARRAY,
						  DBUS_TYPE_STRING_AS_STRING,
						  &iter_array);

		/* append fields to the array */
		for (i = 0; i < columns; i++) {
			GValue value = { 0, };
			const gchar *str;

			_tracker_db_result_set_get_value (result_set, i, &value);

			if (g_value_transform (&value, &transform)) {
				str = g_value_get_string (&transform);
			} else {
				str = "";
			}

			dbus_message_iter_append_basic (&iter_array,
							DBUS_TYPE_STRING,
							&str);
			g_value_unset (&value);
			g_value_reset (&transform);
		}

		dbus_message_iter_close_container (&iter2, &iter_array);
		valid = tracker_db_result_set_iter_next (result_set);
	}

	dbus_message_iter_close_container (&iter, &iter2);
	dbus_connection_send (rec->connection, reply, NULL);
	dbus_message_unref (reply);

}



void
tracker_add_query_result_to_dict (TrackerDBResultSet *result_set,
				  DBusMessageIter    *iter_dict)
{
	gint field_count;
	gboolean valid = TRUE;

	g_return_if_fail (result_set);

	field_count = tracker_db_result_set_get_n_columns (result_set);

	while (valid) {
		DBusMessageIter iter_dict_entry;
		DBusMessageIter iter_var, iter_array;
		char		*key;
		int		i;
		GValue          transform;

		g_value_init (&transform, G_TYPE_STRING);
		tracker_db_result_set_get (result_set, 0, &key, -1);

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
			GValue value;
			const gchar *str;

			_tracker_db_result_set_get_value (result_set, i, &value);

			if (g_value_transform (&value, &transform)) {
				str = g_value_get_string (&transform);
			} else {
				str = "";
			}

			dbus_message_iter_append_basic (&iter_array,
							DBUS_TYPE_STRING,
							&str);
			g_value_unset (&value);
			g_value_reset (&transform);
		}

		dbus_message_iter_close_container (&iter_var, &iter_array);
		dbus_message_iter_close_container (&iter_dict_entry, &iter_var);
		dbus_message_iter_close_container (iter_dict, &iter_dict_entry);

		valid = tracker_db_result_set_iter_next (result_set);
	}
}


char **
tracker_get_query_result_as_array (TrackerDBResultSet *result_set,
				   int                *row_count)
{
	gboolean valid = TRUE;
	char **array;
	gint i = 0;

	*row_count = tracker_db_result_set_get_n_rows (result_set);
	array = g_new (char *, *row_count);

	while (valid) {
		tracker_db_result_set_get (result_set, 0, &array[i], -1);
		valid = tracker_db_result_set_iter_next (result_set);
		i++;
	}

	return array;
}


void
tracker_dbus_method_get_services (DBusRec *rec)
{
	TrackerDBResultSet *result_set;
	DBConnection	*db_con;
	DBusError	dbus_error;
	DBusMessage	*reply;
	DBusMessageIter iter;
	DBusMessageIter iter_dict;
	gboolean	main_only;

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;

	tracker_log ("Executing GetServices Dbus Call");

	dbus_error_init (&dbus_error);

	if (!dbus_message_get_args (rec->message, NULL, DBUS_TYPE_BOOLEAN, &main_only, DBUS_TYPE_INVALID)) {
		tracker_set_error (rec, "DBusError: %s;%s", dbus_error.name, dbus_error.message);
		dbus_error_free (&dbus_error);
		return;
	}
	
	result_set = tracker_exec_proc (db_con, "GetServices", 0);

	reply = dbus_message_new_method_return (rec->message);

	dbus_message_iter_init_append (reply, &iter);

	dbus_message_iter_open_container (&iter,
					  DBUS_TYPE_ARRAY,
					  DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
					  DBUS_TYPE_STRING_AS_STRING
					  DBUS_TYPE_VARIANT_AS_STRING
					  DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
					  &iter_dict);

	if (result_set) {
		tracker_add_query_result_to_dict (result_set, &iter_dict);
		g_object_unref (result_set);
	}

	dbus_message_iter_close_container (&iter, &iter_dict);
	dbus_connection_send (rec->connection, reply, NULL);
	dbus_message_unref (reply);
}


void
tracker_dbus_method_get_stats (DBusRec *rec)
{
	TrackerDBResultSet *result_set;
	DBConnection	*db_con;

	g_return_if_fail (rec && rec->user_data);

	db_con = rec->user_data;

	tracker_log ("Executing GetStats Dbus Call");

	result_set = tracker_exec_proc (db_con, "GetStats", 0);

	tracker_dbus_reply_with_query_result (rec, result_set);

	g_object_unref (result_set);
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

	/*	<!-- sets boolean options in tracker - option can be one of "Pause", "EnableIndexing", "LowMemoryMode", "IndexFileContents" -->
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
                tracker_config_set_fast_merges (tracker->config, value);
                tracker_log ("Fast merges set to %d", value);
	} else if (strcasecmp (option, "EnableIndexing") == 0) {
                tracker_config_set_enable_indexing (tracker->config, value);
		tracker_log ("Enable indexing set to %d", value);
                tracker_dbus_send_index_status_change_signal ();
	} else if (strcasecmp (option, "EnableWatching") == 0) {
                tracker_config_set_enable_watches (tracker->config, value);
		tracker_log ("Enable Watching set to %d", value);
	} else if (strcasecmp (option, "LowMemoryMode") == 0) {
                tracker_config_set_low_memory_mode (tracker->config, value);
		tracker_log ("Extra memory usage set to %d", !value);
	} else if (strcasecmp (option, "IndexFileContents") == 0) {
                tracker_config_set_enable_content_indexing (tracker->config, value);
		tracker_log ("Index file contents set to %d", value);	
	} else if (strcasecmp (option, "GenerateThumbs") == 0) {
                tracker_config_set_enable_thumbnails (tracker->config, value);
		tracker_log ("Generate thumbnails set to %d", value);	
	} else if (strcasecmp (option, "IndexMountedDirectories") == 0) {
                tracker_config_set_index_mounted_directories (tracker->config, value);
		tracker_log ("Index mounted directories set to %d", value);
	} else if (strcasecmp (option, "IndexRemovableDevices") == 0) {
                tracker_config_set_index_removable_devices (tracker->config, value);
		tracker_log ("Index removable media set to %d", value);
	} else if (strcasecmp (option, "BatteryIndex") == 0) {
                tracker_config_set_disable_indexing_on_battery (tracker->config, !value);
		tracker_log ("Disable index on battery set to %d", !value);
	} else if (strcasecmp (option, "BatteryIndexInitial") == 0) {
                tracker_config_set_disable_indexing_on_battery_init (tracker->config, !value);
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
                tracker_config_set_throttle (tracker->config, value);
		tracker_log ("throttle set to %d", value);
	} else if (strcasecmp (option, "MaxText") == 0) {
                tracker_config_set_max_text_to_index (tracker->config, value);
		tracker_log ("Maxinum amount of text set to %d", value);
	} else if (strcasecmp (option, "MaxWords") == 0) {
                tracker_config_set_max_words_to_index (tracker->config, value);
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



