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

#include "tracker-dbus.h"
#include "tracker-utils.h"

extern Tracker *tracker;


static void  unregistered_func (DBusConnection *conn, gpointer data);

static DBusHandlerResult  message_func (DBusConnection *conn, DBusMessage *message, gpointer data);

static DBusObjectPathVTable tracker_vtable = {
	unregistered_func,
	message_func,
	NULL,
	NULL,
	NULL,
	NULL
};

DBusConnection *
tracker_dbus_init (void)
{
	DBusError      error;
	DBusConnection *connection;

	dbus_error_init (&error);

	connection = dbus_bus_get (DBUS_BUS_SESSION, &error);

	if ((connection == NULL) || dbus_error_is_set (&error)) {
		tracker_error ("ERROR: tracker_dbus_init() could not get the session bus");
		
		connection = NULL;
		goto out;
	}

	dbus_connection_setup_with_g_main (connection, NULL);

	if (!dbus_connection_register_object_path (connection, TRACKER_OBJECT, &tracker_vtable, NULL)) {
		tracker_error ("ERROR: could not register D-BUS handlers");
		connection = NULL;
		goto out;
	}

	dbus_error_init (&error);

	dbus_bus_request_name (connection, TRACKER_SERVICE, 0, &error);

	if (dbus_error_is_set (&error)) {
		tracker_error ("ERROR: could not acquire service name due to '%s'", error.message);
		connection = NULL;
		goto out;
	}

out:
	if (dbus_error_is_set (&error)) {
		tracker_error ("DBUS ERROR: %s occurred with message %s", error.name, error.message);
		dbus_error_free (&error);
	}
	
	if (connection != NULL) {
		dbus_connection_set_exit_on_disconnect (connection, FALSE);
	}

	return connection;
}


void
tracker_dbus_shutdown (DBusConnection *conn)
{
	if (!conn) {
		return;
	}

	dbus_connection_close (conn);
	dbus_connection_unref (conn);
}


void
tracker_dbus_send_index_status_change_signal ()
{
	DBusMessage *msg;
	dbus_uint32_t serial = 0;
	char *status = tracker_get_status ();

	msg = dbus_message_new_signal (TRACKER_OBJECT, TRACKER_INTERFACE, TRACKER_SIGNAL_INDEX_STATUS_CHANGE);
				
	if (!msg || !tracker->dbus_con) {
		return;
    	}

	/*
		<signal name="IndexStateChange">
			<arg type="s" name="state" />
			<arg type="b" name="initial_index" />
			<arg type="b" name="in_merge" />
 			<arg type="b" name="is_manual_paused" />
                        <arg type="b" name="is_battery_paused" />
                        <arg type="b" name="is_io_paused" />
		</signal>
	*/

	dbus_message_append_args (msg, 
				  DBUS_TYPE_STRING, &status,
				  DBUS_TYPE_BOOLEAN, &tracker->first_time_index,
				  DBUS_TYPE_BOOLEAN, &tracker->in_merge,
				  DBUS_TYPE_BOOLEAN, &tracker->pause_manual,
				  DBUS_TYPE_BOOLEAN, &tracker->pause_battery,
				  DBUS_TYPE_BOOLEAN, &tracker->pause_io,
				  DBUS_TYPE_INVALID);

	g_free (status);

	dbus_message_set_no_reply (msg, TRUE);

	if (!dbus_connection_send (tracker->dbus_con, msg, &serial)) {
		tracker_error ("Raising the index status changed signal failed");
		return;
	}

	dbus_connection_flush (tracker->dbus_con);

    	dbus_message_unref (msg);
  

}

void
tracker_dbus_send_index_progress_signal (const char *uri)
{
	DBusMessage *msg;
	dbus_uint32_t serial = 0;

	msg = dbus_message_new_signal (TRACKER_OBJECT, TRACKER_INTERFACE, TRACKER_SIGNAL_INDEX_PROGRESS);
				
	if (!msg || !tracker->dbus_con) {
		return;
    	}

	char *service = "Files";

	if (tracker->index_status == INDEX_EMAILS) {
		service = "Emails";
	}

	
	/*
	
		<signal name="IndexProgress">
			<arg type="s" name="service"/>
			<arg type="s" name="current_uri" />
			<arg type="i" name="index_count"/>
			<arg type="i" name="folders_processed"/>
			<arg type="i" name="folders_total"/>
		</signal>
	*/

	dbus_message_append_args (msg, 
				  DBUS_TYPE_STRING, &service,
				  DBUS_TYPE_STRING, &uri,
				  DBUS_TYPE_INT32, &tracker->index_count,
				  DBUS_TYPE_INT32, &tracker->folders_processed,
				  DBUS_TYPE_INT32, &tracker->folders_count,	
				  DBUS_TYPE_INVALID);

	dbus_message_set_no_reply (msg, TRUE);

	if (!dbus_connection_send (tracker->dbus_con, msg, &serial)) {
		tracker_error ("Raising the index status changed signal failed");
		return;
	}

	dbus_connection_flush (tracker->dbus_con);

    	dbus_message_unref (msg);
  

}






void
tracker_dbus_send_index_finished_signal ()
{
	DBusMessage *msg;
	dbus_uint32_t serial = 0;
	int i =  time (NULL) - tracker->index_time_start;

	msg = dbus_message_new_signal (TRACKER_OBJECT, TRACKER_INTERFACE, TRACKER_SIGNAL_INDEX_FINISHED);
				
	if (!msg || !tracker->dbus_con) {
		return;
    	}

	/*
		<signal name="IndexFinished">
			<arg type="i" name="time_taken"/>
		</signal>
	*/

	dbus_message_append_args (msg,
				  DBUS_TYPE_INT32, &i,
				  DBUS_TYPE_INVALID);

	dbus_message_set_no_reply (msg, TRUE);

	if (!dbus_connection_send (tracker->dbus_con, msg, &serial)) {
		tracker_error ("Raising the index status changed signal failed");
		return;
	}

	dbus_connection_flush (tracker->dbus_con);

    	dbus_message_unref (msg);
  

}


static void
unregistered_func (DBusConnection *conn,
		   gpointer        data)
{
}


static DBusHandlerResult
message_func (DBusConnection *conn,
	      DBusMessage    *message,
	      gpointer        data)
{
	DBusRec *rec;

	rec = g_new (DBusRec, 1);
	rec->connection = conn;
	rec->message = message;

	if (!tracker->is_running) {
		dbus_message_ref (message);
		rec->action = DBUS_ACTION_NONE;

		return DBUS_HANDLER_RESULT_HANDLED;
	}


	if (dbus_message_is_method_call (message, TRACKER_INTERFACE, TRACKER_METHOD_PING)) {
		/* ref the message here because we are going to reply to it in a different thread */
		dbus_message_ref (message);
		rec->action = DBUS_ACTION_PING;


	} else if (dbus_message_is_method_call (message, TRACKER_INTERFACE, TRACKER_METHOD_GET_STATS)) {

		dbus_message_ref (message);
		rec->action = DBUS_ACTION_GET_STATS;


	} else if (dbus_message_is_method_call (message, TRACKER_INTERFACE, TRACKER_METHOD_GET_SERVICES)) {

		dbus_message_ref (message);
		rec->action = DBUS_ACTION_GET_SERVICES;


	} else if (dbus_message_is_method_call (message, TRACKER_INTERFACE, TRACKER_METHOD_GET_VERSION)) {

		dbus_message_ref (message);
		rec->action = DBUS_ACTION_GET_VERSION;


        } else if (dbus_message_is_method_call (message, TRACKER_INTERFACE, TRACKER_METHOD_GET_STATUS)) {

                dbus_message_ref (message);
                rec->action = DBUS_ACTION_GET_STATUS;


        } else if (dbus_message_is_method_call (message, TRACKER_INTERFACE, TRACKER_METHOD_SET_BOOL_OPTION)) {

                dbus_message_ref (message);
                rec->action = DBUS_ACTION_SET_BOOL_OPTION;


        } else if (dbus_message_is_method_call (message, TRACKER_INTERFACE, TRACKER_METHOD_SET_INT_OPTION)) {

                dbus_message_ref (message);
                rec->action = DBUS_ACTION_SET_INT_OPTION;

        } else if (dbus_message_is_method_call (message, TRACKER_INTERFACE, TRACKER_METHOD_SHUTDOWN)) {

                dbus_message_ref (message);
                rec->action = DBUS_ACTION_SHUTDOWN;

        } else if (dbus_message_is_method_call (message, TRACKER_INTERFACE, TRACKER_METHOD_PROMPT_INDEX_SIGNALS)) {

                dbus_message_ref (message);
                rec->action = DBUS_ACTION_PROMPT_INDEX_SIGNALS;

	} else if (dbus_message_is_method_call (message, TRACKER_INTERFACE_METADATA, TRACKER_METHOD_METADATA_GET)) {

		dbus_message_ref (message);
		rec->action = DBUS_ACTION_METADATA_GET;


	} else if (dbus_message_is_method_call (message, TRACKER_INTERFACE_METADATA, TRACKER_METHOD_METADATA_SET)) {

		dbus_message_ref (message);
		rec->action = DBUS_ACTION_METADATA_SET;


	} else if (dbus_message_is_method_call (message, TRACKER_INTERFACE_METADATA, TRACKER_METHOD_METADATA_REGISTER_TYPE)) {

		dbus_message_ref (message);
		rec->action = DBUS_ACTION_METADATA_REGISTER_TYPE;


	} else if (dbus_message_is_method_call (message, TRACKER_INTERFACE_METADATA, TRACKER_METHOD_METADATA_GET_TYPE_DETAILS )) {

		dbus_message_ref (message);
		rec->action = DBUS_ACTION_METADATA_GET_TYPE_DETAILS;


	} else if (dbus_message_is_method_call (message, TRACKER_INTERFACE_METADATA, TRACKER_METHOD_METADATA_GET_REGISTERED_TYPES)) {

		dbus_message_ref (message);
		rec->action = DBUS_ACTION_METADATA_GET_REGISTERED_TYPES;



	} else if (dbus_message_is_method_call (message, TRACKER_INTERFACE_METADATA, TRACKER_METHOD_METADATA_GET_WRITEABLE_TYPES)) {

		dbus_message_ref (message);
		rec->action = DBUS_ACTION_METADATA_GET_WRITEABLE_TYPES;



	} else if (dbus_message_is_method_call (message, TRACKER_INTERFACE_METADATA, TRACKER_METHOD_METADATA_GET_REGISTERED_CLASSES)) {

		dbus_message_ref (message);
		rec->action = DBUS_ACTION_METADATA_GET_REGISTERED_CLASSES;



	} else if (dbus_message_is_method_call (message, TRACKER_INTERFACE_KEYWORDS, TRACKER_METHOD_KEYWORDS_GET_LIST)) {

		dbus_message_ref (message);
		rec->action = DBUS_ACTION_KEYWORDS_GET_LIST;



	} else if (dbus_message_is_method_call (message, TRACKER_INTERFACE_KEYWORDS, TRACKER_METHOD_KEYWORDS_GET)) {

		dbus_message_ref (message);
		rec->action = DBUS_ACTION_KEYWORDS_GET;



	} else if (dbus_message_is_method_call (message, TRACKER_INTERFACE_KEYWORDS, TRACKER_METHOD_KEYWORDS_ADD)) {

		dbus_message_ref (message);
		rec->action = DBUS_ACTION_KEYWORDS_ADD;


	} else if (dbus_message_is_method_call (message, TRACKER_INTERFACE_KEYWORDS, TRACKER_METHOD_KEYWORDS_REMOVE)) {

		dbus_message_ref (message);
		rec->action = DBUS_ACTION_KEYWORDS_REMOVE;


	} else if (dbus_message_is_method_call (message, TRACKER_INTERFACE_KEYWORDS, TRACKER_METHOD_KEYWORDS_REMOVE_ALL)) {

		dbus_message_ref (message);
		rec->action = DBUS_ACTION_KEYWORDS_REMOVE_ALL;


	} else if (dbus_message_is_method_call (message, TRACKER_INTERFACE_KEYWORDS, TRACKER_METHOD_KEYWORDS_SEARCH)) {

		dbus_message_ref (message);
		rec->action = DBUS_ACTION_KEYWORDS_SEARCH;




	} else if (dbus_message_is_method_call (message, TRACKER_INTERFACE_SEARCH, TRACKER_METHOD_SEARCH_GET_HIT_COUNT)) {

		dbus_message_ref (message);
		rec->action = DBUS_ACTION_SEARCH_GET_HIT_COUNT;


	} else if (dbus_message_is_method_call (message, TRACKER_INTERFACE_SEARCH, TRACKER_METHOD_SEARCH_GET_HIT_COUNT_ALL)) {

		dbus_message_ref (message);
		rec->action = DBUS_ACTION_SEARCH_GET_HIT_COUNT_ALL;


	} else if (dbus_message_is_method_call (message, TRACKER_INTERFACE_SEARCH, TRACKER_METHOD_SEARCH_TEXT)) {

		dbus_message_ref (message);
		rec->action = DBUS_ACTION_SEARCH_TEXT;

	} else if (dbus_message_is_method_call (message, TRACKER_INTERFACE_SEARCH, TRACKER_METHOD_SEARCH_TEXT_DETAILED)) {

		dbus_message_ref (message);
		rec->action = DBUS_ACTION_SEARCH_TEXT_DETAILED;


	} else if (dbus_message_is_method_call (message, TRACKER_INTERFACE_SEARCH, TRACKER_METHOD_SEARCH_GET_SNIPPET)) {

		dbus_message_ref (message);
		rec->action = DBUS_ACTION_SEARCH_GET_SNIPPET;




	} else if (dbus_message_is_method_call (message, TRACKER_INTERFACE_SEARCH, TRACKER_METHOD_SEARCH_FILES_BY_TEXT )) {

		dbus_message_ref (message);
		rec->action = DBUS_ACTION_SEARCH_FILES_BY_TEXT;



	} else if (dbus_message_is_method_call (message, TRACKER_INTERFACE_SEARCH, TRACKER_METHOD_SEARCH_METADATA)) {

		dbus_message_ref (message);
		rec->action = DBUS_ACTION_SEARCH_METADATA;



	} else if (dbus_message_is_method_call (message, TRACKER_INTERFACE_SEARCH, TRACKER_METHOD_SEARCH_MATCHING_FIELDS)) {

		dbus_message_ref (message);
		rec->action = DBUS_ACTION_SEARCH_MATCHING_FIELDS;



	} else if (dbus_message_is_method_call (message, TRACKER_INTERFACE_SEARCH, TRACKER_METHOD_SEARCH_QUERY)) {

		dbus_message_ref (message);
		rec->action = DBUS_ACTION_SEARCH_QUERY;



	} else if (dbus_message_is_method_call (message, TRACKER_INTERFACE_SEARCH, TRACKER_METHOD_SEARCH_SUGGEST)) {

		dbus_message_ref (message);
		rec->action = DBUS_ACTION_SEARCH_SUGGEST;




	} else if (dbus_message_is_method_call (message, TRACKER_INTERFACE_FILES, TRACKER_METHOD_FILES_EXISTS)) {

		dbus_message_ref (message);
		rec->action = DBUS_ACTION_FILES_EXISTS;


	} else if (dbus_message_is_method_call (message, TRACKER_INTERFACE_FILES, TRACKER_METHOD_FILES_CREATE)) {

		dbus_message_ref (message);
		rec->action = DBUS_ACTION_FILES_CREATE;



	} else if (dbus_message_is_method_call (message, TRACKER_INTERFACE_FILES, TRACKER_METHOD_FILES_DELETE)) {

		dbus_message_ref (message);
		rec->action = DBUS_ACTION_FILES_DELETE;


	} else if (dbus_message_is_method_call (message, TRACKER_INTERFACE_FILES, TRACKER_METHOD_FILES_GET_SERVICE_TYPE)) {

		dbus_message_ref (message);
		rec->action = DBUS_ACTION_FILES_GET_SERVICE_TYPE;


	} else if (dbus_message_is_method_call (message, TRACKER_INTERFACE_FILES, TRACKER_METHOD_FILES_GET_TEXT_CONTENTS )) {

		dbus_message_ref (message);
		rec->action = DBUS_ACTION_FILES_GET_TEXT_CONTENTS ;



	} else if (dbus_message_is_method_call (message, TRACKER_INTERFACE_FILES, TRACKER_METHOD_FILES_SEARCH_TEXT_CONTENTS)) {

		dbus_message_ref (message);
		rec->action = DBUS_ACTION_FILES_SEARCH_TEXT_CONTENTS;



	} else if (dbus_message_is_method_call (message, TRACKER_INTERFACE_FILES, TRACKER_METHOD_FILES_GET_BY_SERVICE_TYPE)) {

		dbus_message_ref (message);
		rec->action = DBUS_ACTION_FILES_GET_BY_SERVICE_TYPE;



	} else if (dbus_message_is_method_call (message, TRACKER_INTERFACE_FILES, TRACKER_METHOD_FILES_GET_BY_MIME_TYPE)) {

		dbus_message_ref (message);
		rec->action = DBUS_ACTION_FILES_GET_BY_MIME_TYPE;



	} else if (dbus_message_is_method_call (message, TRACKER_INTERFACE_FILES, TRACKER_METHOD_FILES_GET_BY_MIME_TYPE_VFS)) {

		dbus_message_ref (message);
		rec->action = DBUS_ACTION_FILES_GET_BY_MIME_TYPE_VFS;



	} else if (dbus_message_is_method_call (message, TRACKER_INTERFACE_FILES, TRACKER_METHOD_FILES_REFRESH_METADATA)) {

		dbus_message_ref (message);
		rec->action = DBUS_ACTION_FILES_REFRESH_METADATA;


	} else if (dbus_message_is_method_call (message, TRACKER_INTERFACE_FILES, TRACKER_METHOD_FILES_GET_MTIME)) {

		dbus_message_ref (message);
		rec->action = DBUS_ACTION_FILES_GET_MTIME;



	} else if (dbus_message_is_method_call (message, TRACKER_INTERFACE_FILES, TRACKER_METHOD_FILES_GET_METADATA_FOLDER_FILES)) {

		dbus_message_ref (message);
		rec->action = DBUS_ACTION_FILES_GET_METADATA_FOLDER_FILES;



	} else if (dbus_message_is_method_call (message, TRACKER_INTERFACE_FILES, TRACKER_METHOD_FILES_SEARCH_BY_TEXT_MIME)) {

		dbus_message_ref (message);
		rec->action = DBUS_ACTION_FILES_SEARCH_BY_TEXT_MIME;


	} else if (dbus_message_is_method_call (message, TRACKER_INTERFACE_FILES, TRACKER_METHOD_FILES_SEARCH_BY_TEXT_MIME_LOCATION)) {

		dbus_message_ref (message);
		rec->action = DBUS_ACTION_FILES_SEARCH_BY_TEXT_MIME_LOCATION;


	} else if (dbus_message_is_method_call (message, TRACKER_INTERFACE_FILES, TRACKER_METHOD_FILES_SEARCH_BY_TEXT_LOCATION)) {

		dbus_message_ref (message);
		rec->action = DBUS_ACTION_FILES_SEARCH_BY_TEXT_LOCATION;


	} else {
		g_free (rec);
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}


	g_async_queue_push (tracker->user_request_queue, rec);

	tracker_notify_request_data_available ();

	return DBUS_HANDLER_RESULT_HANDLED;
}
