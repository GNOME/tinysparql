/* Tracker
 * Copyright (C) 2005, Mr Jamie McCracken (jamiemcc@gnome.org)
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

#include "tracker-client.h"
#include "tracker.h"

#define TRACKER_FILE_METADATA_SERVICE                   "org.freedesktop.file.metadata"
#define TRACKER_FILE_METADATA_OBJECT			"/org/freedesktop/file/metadata"
#define TRACKER_FILE_METADATA_INTERFACE			"org.freedesktop.file.metadata"

typedef struct {
	TrackerArrayReply callback;
	gpointer	  data;
} ArrayCallBackStruct;

TrackerClient *
tracker_connect (gboolean integrate_main_loop)
{
	DBusGConnection *connection;
	GError *error = NULL;
	TrackerClient *client = NULL;
	DBusGProxy *proxy;

	g_type_init ();

	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	
	if (connection == NULL)	{
		g_warning("Unable to connect to dbus: %s\n", error->message);
		g_error_free (error);
		return NULL;
	}

	if (integrate_main_loop) {
		dbus_connection_setup_with_g_main (connection, NULL);
	}

	proxy = dbus_g_proxy_new_for_name (connection,
			TRACKER_FILE_METADATA_SERVICE,
			TRACKER_FILE_METADATA_OBJECT,
			TRACKER_FILE_METADATA_INTERFACE);

	if (!proxy) {
		g_warning ("could not create proxy");
		return NULL;
	}

	
	client = g_new (TrackerClient, 1);
	client->proxy = proxy;

	return client;

}

void
tracker_disconnect (TrackerClient *client)
{
	g_object_unref (client->proxy);
	client->proxy = NULL;
	g_free (client);
}



void
tracker_cancel_last_call (TrackerClient *client)
{
	dbus_g_proxy_cancel_call (client->proxy, client->last_pending_call);
}



/*synchronous calls */

char *
tracker_get_metadata (TrackerClient *client, const char *uri, const char *key, GError *error)
{
	char *str;

	org_freedesktop_file_metadata_get_metadata   (client->proxy, uri, key, &str, &error);
	
	return str;

}

void
tracker_set_metadata (TrackerClient *client, const char *uri, const char *key, const char *value, GError *error)
{
	org_freedesktop_file_metadata_set_metadata   (client->proxy, uri, key, value, &error);
}

void		
tracker_register_metadata_type (TrackerClient *client, const char *name, MetadataTypes type, GError *error)
{
	org_freedesktop_file_metadata_register_metadata_type (client->proxy, name, type, &error);
}


GHashTable *	
tracker_get_metadata_for_files_in_folder (TrackerClient *client, const char *uri, const char **keys, GError *error)
{
	GHashTable *table;

	if (!org_freedesktop_file_metadata_get_metadata_for_files_in_folder (client->proxy, uri, keys, &table, &error)) {
		return NULL;
	}

	return table;
}





char **
tracker_search_metadata_by_text (TrackerClient *client, const char *query,  GError *error)
{

	char **strs;

	if (!org_freedesktop_file_metadata_search_metadata_by_text  (client->proxy, query, &strs, &error)) {
		return NULL;
	}
	return strs;
	
}


char **
tracker_search_metadata_by_text_and_mime (TrackerClient *client, const char *query, const char **mimes, GError *error)
{
	char **strs;

	if (!org_freedesktop_file_metadata_search_metadata_by_text_and_mime (client->proxy, query, mimes, &strs, &error)) {
		return NULL;
	}
	return strs;

}


char **
tracker_search_metadata_by_text_and_mime_and_location (TrackerClient *client, const char *query, const char **mimes, const char *location, GError *error)
{
	char **strs;

	if (!org_freedesktop_file_metadata_search_metadata_by_text_and_mime_and_location (client->proxy, query, mimes, location, &strs, &error)) {
		return NULL;
	}
	return strs;

}



char **
tracker_search_metadata_by_text_and_location (TrackerClient *client, const char *query, const char *location, GError *error)
{
	char **strs;

	if (!org_freedesktop_file_metadata_search_metadata_by_text_and_location (client->proxy, query, location, &strs, &error)) {
		return NULL;
	}
	return strs;

}

char **
tracker_search_metadata_by_query (TrackerClient *client, const char *query,  GError *error)
{

	char **strs;

	if (!org_freedesktop_file_metadata_search_metadata_by_query  (client->proxy, query, &strs, &error)) {
	
		g_warning ("method failed: %s", error->message);
		g_error_free (error);
		return NULL;
	}
	return strs;
	
}



/* asynchronous calls */

static void
tracker_array_reply (DBusGProxy *proxy, char **OUT_result, GError *error, gpointer user_data)
{

	ArrayCallBackStruct *callback_struct;

	callback_struct = user_data;

	(*(TrackerArrayReply) callback_struct->callback ) (OUT_result, error, callback_struct->data);

	g_free (callback_struct);
}


void
tracker_search_metadata_by_text_async 	(TrackerClient *client, const char *query, TrackerArrayReply callback, gpointer user_data) 
{
	ArrayCallBackStruct *callback_struct;

	callback_struct = g_new (ArrayCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	client->last_pending_call = org_freedesktop_file_metadata_search_metadata_by_text_async (client->proxy, query, tracker_array_reply, callback_struct);
	
}


void
tracker_search_metadata_by_text_and_mime_async (TrackerClient *client, const char *query, const char **mimes, TrackerArrayReply callback, gpointer user_data)
{
	ArrayCallBackStruct *callback_struct;

	callback_struct = g_new (ArrayCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	client->last_pending_call = org_freedesktop_file_metadata_search_metadata_by_text_and_mime_async (client->proxy, query, mimes, tracker_array_reply, callback_struct);
}


void
tracker_search_metadata_by_text_and_mime_and_location_async (TrackerClient *client, const char *query, const char **mimes, const char *location, TrackerArrayReply callback, gpointer user_data)
{
	ArrayCallBackStruct *callback_struct;

	callback_struct = g_new (ArrayCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	client->last_pending_call = org_freedesktop_file_metadata_search_metadata_by_text_and_mime_and_location_async (client->proxy, query, mimes, location, tracker_array_reply, callback_struct);
}

void
tracker_search_metadata_by_text_and_location_async (TrackerClient *client, const char *query, const char *location, TrackerArrayReply callback, gpointer user_data)
{
	ArrayCallBackStruct *callback_struct;

	callback_struct = g_new (ArrayCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	client->last_pending_call = org_freedesktop_file_metadata_search_metadata_by_text_and_location_async (client->proxy, query, location, tracker_array_reply, callback_struct);
}



void
tracker_search_metadata_by_query_async 	(TrackerClient *client, const char *query, TrackerArrayReply callback, gpointer user_data) 
{
	ArrayCallBackStruct *callback_struct;

	callback_struct = g_new (ArrayCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	client->last_pending_call = org_freedesktop_file_metadata_search_metadata_by_text_async (client->proxy, query, tracker_array_reply, callback_struct);
}



