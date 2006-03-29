/* Tracker
 * Copyright (C) 2005, Mr Jamie McCracken
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "tracker-dbus.h" 
#include "tracker-utils.h"

GAsyncQueue *user_request_queue;


static void  unregistered_func (DBusConnection *conn, gpointer data);

static DBusHandlerResult  message_func (DBusConnection *conn, DBusMessage *message, gpointer data);

static DBusObjectPathVTable tracker_vtable = {
	unregistered_func,
	message_func,
	NULL
};


DBusConnection *
tracker_dbus_init (void)
{
	DBusError		error;
	DBusConnection *	connection;

	dbus_error_init (&error);

	connection = dbus_bus_get (DBUS_BUS_SESSION, &error);

	if ((connection == NULL) || dbus_error_is_set (&error)) {
		tracker_log ("tracker_dbus_init() could not get the session bus");
		connection = NULL;
		goto out;
	}

	dbus_connection_setup_with_g_main (connection, NULL);

	if (!dbus_connection_register_object_path (connection, TRACKER_FILE_METADATA_OBJECT, &tracker_vtable, NULL)) {
		
		tracker_log ("could not register D-BUS handlers");
		connection = NULL;
		goto out;
	}

	dbus_error_init (&error);

	dbus_bus_request_name (connection, TRACKER_FILE_METADATA_SERVICE, 0, &error);

	if (dbus_error_is_set (&error)) {
		tracker_log ("could not acquire service name due to '%s'", error.message);
		connection = NULL;
		goto out;
	}

out:
	if (dbus_error_is_set (&error)) {
		dbus_error_free (&error);
	}
	return connection;
}


void
tracker_dbus_shutdown (DBusConnection *conn)
{

	if (!conn) {
		return;
	}
	
	dbus_connection_disconnect (conn);
	dbus_connection_unref (conn);
}

static void
unregistered_func (DBusConnection *conn,
		  gpointer        data)
{
}


static DBusHandlerResult
message_func (DBusConnection *conn,
	     DBusMessage     *message,
	     gpointer        data)
{
	DBusRec *rec;
	
	rec = g_new (DBusRec, 1);
	rec->connection = conn;
	rec->message = message;

	if (dbus_message_is_method_call (message, TRACKER_FILE_METADATA_INTERFACE, TRACKER_METHOD_PING)) {
		/* ref the message here because we are going to reply to it in a different thread */
		dbus_message_ref (message);
		rec->action = DBUS_ACTION_PING; 
		g_async_queue_push (user_request_queue, rec);

	} else if (dbus_message_is_method_call (message, TRACKER_FILE_METADATA_INTERFACE, TRACKER_METHOD_GET_METADATA)) {
		
		dbus_message_ref (message);
		rec->action = DBUS_ACTION_GET_METADATA;
		g_async_queue_push (user_request_queue, rec);


	} else if (dbus_message_is_method_call (message, TRACKER_FILE_METADATA_INTERFACE, TRACKER_METHOD_SET_METADATA)) {
		
		dbus_message_ref (message);
		rec->action = DBUS_ACTION_SET_METADATA;
		g_async_queue_push (user_request_queue, rec);


	} else if (dbus_message_is_method_call (message, TRACKER_FILE_METADATA_INTERFACE, TRACKER_METHOD_REGISTER_METADATA_TYPE)) {
		
		dbus_message_ref (message);
		rec->action = DBUS_ACTION_REGISTER_METADATA;
		g_async_queue_push (user_request_queue, rec);


	} else if (dbus_message_is_method_call (message, TRACKER_FILE_METADATA_INTERFACE, TRACKER_METHOD_GET_METADATA_FOR_FILES)) {
		
		dbus_message_ref (message);
		rec->action = DBUS_ACTION_GET_METADATA_FOR_FILES;
		g_async_queue_push (user_request_queue, rec);


	} else if (dbus_message_is_method_call (message, TRACKER_FILE_METADATA_INTERFACE, TRACKER_METHOD_SEARCH_METADATA_TEXT)) {
		
		dbus_message_ref (message);
		rec->action = DBUS_ACTION_SEARCH_METADATA_TEXT;
		g_async_queue_push (user_request_queue, rec);

	} else if (dbus_message_is_method_call (message, TRACKER_FILE_METADATA_INTERFACE, TRACKER_METHOD_SEARCH_FILES_BY_TEXT_MIME)) {
		
		dbus_message_ref (message);
		rec->action = DBUS_ACTION_SEARCH_FILES_BY_TEXT_MIME;
		g_async_queue_push (user_request_queue, rec);

	} else if (dbus_message_is_method_call (message, TRACKER_FILE_METADATA_INTERFACE, TRACKER_METHOD_SEARCH_FILES_BY_TEXT_MIME_LOCATION)) {
		
		dbus_message_ref (message);
		rec->action = DBUS_ACTION_SEARCH_FILES_BY_TEXT_MIME_LOCATION;
		g_async_queue_push (user_request_queue, rec);

	} else if (dbus_message_is_method_call (message, TRACKER_FILE_METADATA_INTERFACE, TRACKER_METHOD_SEARCH_FILES_BY_TEXT_LOCATION)) {
		
		dbus_message_ref (message);
		rec->action = DBUS_ACTION_SEARCH_FILES_BY_TEXT_LOCATION;
		g_async_queue_push (user_request_queue, rec);

	} else if (dbus_message_is_method_call (message, TRACKER_FILE_METADATA_INTERFACE, TRACKER_METHOD_SEARCH_FILES_QUERY)) {
		
		dbus_message_ref (message);
		rec->action = DBUS_ACTION_SEARCH_FILES_QUERY;
		g_async_queue_push (user_request_queue, rec);

	} else {
		g_free (rec);		
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}
	

	return DBUS_HANDLER_RESULT_HANDLED;

}
