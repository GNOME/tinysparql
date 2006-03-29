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

#define TRACKER_Tracker_SERVICE                 "org.freedesktop.Tracker"
#define TRACKER_Tracker_OBJECT			"/org/freedesktop/tracker"
#define TRACKER_Tracker_INTERFACE		"org.freedesktop.Tracker"

typedef struct {
	TrackerArrayReply callback;
	gpointer	  data;
} ArrayCallBackStruct;


char *metadata_types[] = {
	"index",
	"string",
	"int",
	"date",
};

char *service_types[] = {
"Files",
"Documents",
"Images",
"Music",
"Playlists",
"Applications",
"People",
"Emails",
"Conversations",
"Appointments",
"Tasks",
"Bookmarks",
"History",
"Projects"
};


TrackerClient *
tracker_connect (gboolean enable_warnings)
{
	DBusGConnection *connection;
	GError *error = NULL;
	TrackerClient *client = NULL;
	DBusGProxy *proxy;

	g_type_init ();

	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	
	if (connection == NULL)	{
		if (enable_warnings) {
			g_warning("Unable to connect to dbus: %s\n", error->message);
		}
		g_error_free (error);
		return NULL;
	}

	proxy = dbus_g_proxy_new_for_name (connection,
			TRACKER_Tracker_SERVICE,
			TRACKER_Tracker_OBJECT,
			TRACKER_Tracker_INTERFACE);

	if (!proxy) {
		if (enable_warnings) {
			g_warning ("could not create proxy");
		}
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



char *
tracker_get_metadata (TrackerClient *client, ServiceType service, const char *id, const char *key, GError *error)
{
	char *str;

	char *service_str = service_types[service];

	if (!org_freedesktop_Tracker_get_metadata (client->proxy, service_str, id, key, &str, &error)) {
		return NULL;
	}
	
	return str;

}

void
tracker_set_metadata (TrackerClient *client, ServiceType service, const char *id, const char *key, const char *value, GError *error)
{

	char *service_str = service_types[service];

	org_freedesktop_Tracker_set_metadata (client->proxy, service_str, id, key, value, &error);
		
	
}

void		
tracker_register_metadata_type (TrackerClient *client, const char *name, MetadataTypes type, GError *error)
{
	char *type_str = metadata_types[type];

	org_freedesktop_Tracker_register_metadata_type (client->proxy, name, type_str, &error);
}


GHashTable *	
tracker_get_metadata_for_files_in_folder (TrackerClient *client, const char *uri, const char **keys, GError *error)
{
	GHashTable *table;

	if (!org_freedesktop_Tracker_get_metadata_for_files_in_folder (client->proxy, uri, keys, &table, &error)) {
		return NULL;
	}

	return table;
}


char **
tracker_search_metadata_text (TrackerClient *client, ServiceType service, const char *text, int max_hits, gboolean sort_by_relevance, GError *error)
{

	char **strs;
	char *service_str = service_types[service];

	if (!org_freedesktop_Tracker_search_metadata_text (client->proxy, service_str, text, max_hits, sort_by_relevance, &strs, &error)) {
		return NULL;
	}
	return strs;
	
}



char **
tracker_search_metadata_by_text (TrackerClient *client, const char *query,  GError *error)
{

	char **strs;

	if (!org_freedesktop_Tracker_search_metadata_text (client->proxy, "Files", query, 512, FALSE, &strs, &error)) {
		return NULL;
	}
	return strs;
	
}


char **
tracker_search_metadata_by_text_and_mime (TrackerClient *client, const char *query, const char **mimes, GError *error)
{
	char **strs;

	if (!org_freedesktop_Tracker_search_files_by_text_and_mime (client->proxy, query, mimes, &strs, &error)) {
		return NULL;
	}
	return strs;

}


char **
tracker_search_metadata_by_text_and_mime_and_location (TrackerClient *client, const char *query, const char **mimes, const char *location, GError *error)
{
	char **strs;

	if (!org_freedesktop_Tracker_search_files_by_text_and_mime_and_location (client->proxy, query, mimes, location, &strs, &error)) {
		return NULL;
	}
	return strs;

}



char **
tracker_search_metadata_by_text_and_location (TrackerClient *client, const char *query, const char *location, GError *error)
{
	char **strs;

	if (!org_freedesktop_Tracker_search_files_by_text_and_location (client->proxy, query, location, &strs, &error)) {
		return NULL;
	}
	return strs;

}

char **
tracker_search_files_by_query (TrackerClient *client, const char *query, int max_hits, gboolean sort_by_relevance, GError *error)
{

	char **strs;

	if (!org_freedesktop_Tracker_search_file_query  (client->proxy, query, max_hits, sort_by_relevance, &strs, &error)) {
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

	client->last_pending_call = org_freedesktop_Tracker_search_metadata_text_async (client->proxy, "Files", query, 512, FALSE, tracker_array_reply, callback_struct);
	
}

void
tracker_search_metadata_text_async (TrackerClient *client, ServiceType service, const char *text, int max_hits, gboolean sort_by_relevance, TrackerArrayReply callback, gpointer user_data) 
{
	ArrayCallBackStruct *callback_struct;

	callback_struct = g_new (ArrayCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	char *service_str = service_types[service];

	client->last_pending_call = org_freedesktop_Tracker_search_metadata_text_async (client->proxy, service_str, text, max_hits, sort_by_relevance, tracker_array_reply, callback_struct);
	
}

void
tracker_search_metadata_by_text_and_mime_async (TrackerClient *client, const char *query, const char **mimes, TrackerArrayReply callback, gpointer user_data)
{
	ArrayCallBackStruct *callback_struct;

	callback_struct = g_new (ArrayCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	client->last_pending_call = org_freedesktop_Tracker_search_files_by_text_and_mime_async (client->proxy, query, mimes, tracker_array_reply, callback_struct);
}


void
tracker_search_metadata_by_text_and_mime_and_location_async (TrackerClient *client, const char *query, const char **mimes, const char *location, TrackerArrayReply callback, gpointer user_data)
{
	ArrayCallBackStruct *callback_struct;

	callback_struct = g_new (ArrayCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	client->last_pending_call = org_freedesktop_Tracker_search_files_by_text_and_mime_and_location_async (client->proxy, query, mimes, location, tracker_array_reply, callback_struct);
}

void
tracker_search_metadata_by_text_and_location_async (TrackerClient *client, const char *query, const char *location, TrackerArrayReply callback, gpointer user_data)
{
	ArrayCallBackStruct *callback_struct;

	callback_struct = g_new (ArrayCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	client->last_pending_call = org_freedesktop_Tracker_search_files_by_text_and_location_async (client->proxy, query, location, tracker_array_reply, callback_struct);
}



void
tracker_search_files_by_query_async (TrackerClient *client, const char *query, int max_hits, gboolean sort_by_relevance, TrackerArrayReply callback, gpointer user_data)
{
	ArrayCallBackStruct *callback_struct;

	callback_struct = g_new (ArrayCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	client->last_pending_call = org_freedesktop_Tracker_search_metadata_text_async (client->proxy, "Files", query, max_hits, sort_by_relevance, tracker_array_reply, callback_struct);
}



