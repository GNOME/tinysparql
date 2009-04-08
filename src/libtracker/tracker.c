/* Tracker - indexer and metadata database engine
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include <string.h>

#include "tracker.h"

#include "tracker-daemon-glue.h"
#include "tracker-search-glue.h"

#define TRACKER_SERVICE			"org.freedesktop.Tracker"
#define TRACKER_OBJECT			"/org/freedesktop/Tracker"
#define TRACKER_INTERFACE		"org.freedesktop.Tracker"
#define TRACKER_INTERFACE_SEARCH	"org.freedesktop.Tracker.Search"
#define TRACKER_INTERFACE_MUSIC		"org.freedesktop.Tracker.Music"
#define TRACKER_INTERFACE_PLAYLISTS	"org.freedesktop.Tracker.PlayLists"

typedef struct {
	TrackerArrayReply callback;
	gpointer	  data;
} ArrayCallBackStruct;

typedef struct {
	TrackerGPtrArrayReply callback;
	gpointer	  data;
} GPtrArrayCallBackStruct;

typedef struct {
	TrackerHashTableReply	callback;
	gpointer		data;
} HashTableCallBackStruct;


typedef struct {
	TrackerBooleanReply callback;
	gpointer	  data;
} BooleanCallBackStruct;

typedef struct {
	TrackerStringReply callback;
	gpointer	  data;
} StringCallBackStruct;

typedef struct {
	TrackerIntReply callback;
	gpointer	  data;
} IntCallBackStruct;

typedef struct {
	TrackerVoidReply callback;
	gpointer	  data;
} VoidCallBackStruct;


const char *tracker_service_types[] = {
	"Files",
	"Folders",
	"Documents",
	"Images",
	"Music",
	"Videos",
	"Text",
	"Development",
	"Other",
	"VFS",
	"VFSFolders",
	"VFSDocuments",
	"VFSImages",
	"VFSMusic",
	"VFSVideos",
	"VFSText",
	"VFSDevelopment",
	"VFSOther",
	"Conversations",
	"Playlists",
	"Applications",
	"Contacts",
	"Emails",
	"EmailAttachments",
	"Appointments",
	"Tasks",
	"Bookmarks",
	"WebHistory",
	"Projects",
	NULL
};




const char *metadata_types[] = {
	"index",
	"string",
	"numeric",
	"date",
	"blob"
};


ServiceType
tracker_service_name_to_type (const char *service)
{
	const char **st;
	int i = 0;

	for (st=tracker_service_types; *st; st++) {

		if (g_ascii_strcasecmp (service, *st) == 0) {
			return i;
		}

		i++;
	}

	return SERVICE_OTHER_FILES;
}


char *
tracker_type_to_service_name (ServiceType s)
{
	return g_strdup (tracker_service_types[s]);
}



static void
tracker_array_reply (DBusGProxy *proxy, char **OUT_result, GError *error, gpointer user_data)
{

	ArrayCallBackStruct *callback_struct;

	callback_struct = user_data;

	(*(TrackerArrayReply) callback_struct->callback ) (OUT_result, error, callback_struct->data);

	g_free (callback_struct);
}


static void
tracker_hashtable_reply (DBusGProxy *proxy,  GHashTable *OUT_result, GError *error, gpointer user_data)
{

	HashTableCallBackStruct *callback_struct;

	callback_struct = user_data;

	(*(TrackerHashTableReply) callback_struct->callback ) (OUT_result, error, callback_struct->data);

	g_free (callback_struct);
}

static void
tracker_GPtrArray_reply (DBusGProxy *proxy,  GPtrArray *OUT_result, GError *error, gpointer user_data)
{

	GPtrArrayCallBackStruct *callback_struct;

	callback_struct = user_data;

	(*(TrackerGPtrArrayReply) callback_struct->callback ) (OUT_result, error, callback_struct->data);

	g_free (callback_struct);
}



static void
tracker_string_reply (DBusGProxy *proxy, char *OUT_result, GError *error, gpointer user_data)
{

	StringCallBackStruct *callback_struct;

	callback_struct = user_data;

	(*(TrackerStringReply) callback_struct->callback ) (OUT_result, error, callback_struct->data);

	g_free (callback_struct);
}


static void
tracker_int_reply (DBusGProxy *proxy, int OUT_result, GError *error, gpointer user_data)
{

	IntCallBackStruct *callback_struct;

	callback_struct = user_data;

	(*(TrackerIntReply) callback_struct->callback ) (OUT_result, error, callback_struct->data);

	g_free (callback_struct);
}

/*
static void
tracker_boolean_reply (DBusGProxy *proxy, gboolean OUT_result, GError *error, gpointer user_data)
{

	BooleanCallBackStruct *callback_struct;

	callback_struct = user_data;

	(*(TrackerBooleanReply) callback_struct->callback ) (OUT_result, error, callback_struct->data);

	g_free (callback_struct);
}
*/


static void
tracker_void_reply (DBusGProxy *proxy, GError *error, gpointer user_data)
{

	VoidCallBackStruct *callback_struct;

	callback_struct = user_data;

	(*(TrackerVoidReply) callback_struct->callback ) (error, callback_struct->data);

	g_free (callback_struct);
}





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
			TRACKER_SERVICE,
			TRACKER_OBJECT,
			TRACKER_INTERFACE);

	if (!proxy) {
		if (enable_warnings) {
			g_warning ("could not create proxy");
		}
		return NULL;
	}


	client = g_new (TrackerClient, 1);
	client->proxy = proxy;

	proxy = dbus_g_proxy_new_for_name (connection,
			TRACKER_SERVICE,
			TRACKER_OBJECT "/Search",
			TRACKER_INTERFACE_SEARCH);

	client->proxy_search = proxy;


	return client;

}

void
tracker_disconnect (TrackerClient *client)
{
	g_object_unref (client->proxy);
	g_object_unref (client->proxy_search);
	client->proxy = NULL;
	client->proxy_search = NULL;

	g_free (client);
}



void
tracker_cancel_last_call (TrackerClient *client)
{
	dbus_g_proxy_cancel_call (client->proxy, client->last_pending_call);
}



/* dbus synchronous calls */


int
tracker_get_version (TrackerClient *client, GError **error)
{
	int version;

	org_freedesktop_Tracker_get_version (client->proxy, &version, &*error);

	return version;
}

char *
tracker_get_status (TrackerClient *client, GError **error)
{
	char *status ;
	org_freedesktop_Tracker_get_status (client->proxy, &status, &*error);
	return status;
}


GPtrArray *
tracker_get_stats (TrackerClient *client,  GError **error)
{
	GPtrArray *table;

	if (!org_freedesktop_Tracker_get_stats (client->proxy, &table, &*error)) {
		return NULL;
	}

	return table;


}


void
tracker_set_bool_option (TrackerClient *client, const char *option, gboolean value, GError **error)
{
	org_freedesktop_Tracker_set_bool_option (client->proxy, option, value,	&*error);
}

void
tracker_set_int_option (TrackerClient *client, const char *option, int value, GError **error)
{
	org_freedesktop_Tracker_set_int_option (client->proxy, option, value,  &*error);
}


void
tracker_shutdown (TrackerClient *client, gboolean reindex, GError **error)
{
	org_freedesktop_Tracker_shutdown (client->proxy, reindex,  &*error);
}


void
tracker_prompt_index_signals (TrackerClient *client, GError **error)
{
	org_freedesktop_Tracker_prompt_index_signals (client->proxy, &*error);
}


char *
tracker_search_get_snippet (TrackerClient *client, ServiceType service, const char *uri, const char *search_text, GError **error)
{
	char *result;
	const char *service_str = tracker_service_types[service];

	if (!org_freedesktop_Tracker_Search_get_snippet (client->proxy_search, service_str, uri, search_text, &result, &*error)) {
		return NULL;
	}

	return result;


}


char *
tracker_search_suggest (TrackerClient *client, const char *search_term, int maxdist, GError **error)
{
	gchar *result;
	if (org_freedesktop_Tracker_Search_suggest (client->proxy_search, search_term, maxdist, &result, &*error)) {
		return result;
	}
	return NULL;
}


void
tracker_get_version_async (TrackerClient *client, TrackerIntReply callback, gpointer user_data)
{

	IntCallBackStruct *callback_struct;

	callback_struct = g_new (IntCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	client->last_pending_call = org_freedesktop_Tracker_get_version_async (client->proxy, tracker_int_reply, callback_struct);

}

void
tracker_get_status_async (TrackerClient *client, TrackerStringReply callback, gpointer user_data)
{

	StringCallBackStruct *callback_struct;

	callback_struct = g_new (StringCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	client->last_pending_call = org_freedesktop_Tracker_get_status_async (client->proxy, tracker_string_reply, callback_struct);

}


void
tracker_get_stats_async	(TrackerClient *client,  TrackerGPtrArrayReply callback, gpointer user_data)
{
	GPtrArrayCallBackStruct *callback_struct;

	callback_struct = g_new (GPtrArrayCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	client->last_pending_call = org_freedesktop_Tracker_get_stats_async (client->proxy, tracker_GPtrArray_reply, callback_struct);

}



void
tracker_set_bool_option_async (TrackerClient *client, const char *option, gboolean value, TrackerVoidReply callback, gpointer user_data)
{
	VoidCallBackStruct *callback_struct;

	callback_struct = g_new (VoidCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	client->last_pending_call = org_freedesktop_Tracker_set_bool_option_async  (client->proxy, option, value, tracker_void_reply, callback_struct);
}

void
tracker_set_int_option_async (TrackerClient *client, const char *option, int value, TrackerVoidReply callback, gpointer user_data)
{
	VoidCallBackStruct *callback_struct;

	callback_struct = g_new (VoidCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	client->last_pending_call = org_freedesktop_Tracker_set_int_option_async  (client->proxy, option, value, tracker_void_reply, callback_struct);
}


void
tracker_shutdown_async (TrackerClient *client, gboolean reindex, TrackerVoidReply callback, gpointer user_data)
{
	VoidCallBackStruct *callback_struct;

	callback_struct = g_new (VoidCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	client->last_pending_call = org_freedesktop_Tracker_shutdown_async  (client->proxy, reindex, tracker_void_reply, callback_struct);
}


void
tracker_prompt_index_signals_async (TrackerClient *client, TrackerVoidReply callback, gpointer user_data)
{
	VoidCallBackStruct *callback_struct;

	callback_struct = g_new (VoidCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	client->last_pending_call = org_freedesktop_Tracker_prompt_index_signals_async	(client->proxy, tracker_void_reply, callback_struct);
}


void
tracker_search_get_snippet_async (TrackerClient *client, ServiceType service, const char *uri, const char *search_text, TrackerStringReply callback, gpointer user_data)
{
	StringCallBackStruct *callback_struct;
	const char *service_str;

	callback_struct = g_new (StringCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	service_str = tracker_service_types[service];

	client->last_pending_call = org_freedesktop_Tracker_Search_get_snippet_async (client->proxy_search, service_str, uri, search_text, tracker_string_reply, callback_struct);

}


void
tracker_search_suggest_async (TrackerClient *client, const char *search_text, int maxdist, TrackerStringReply callback, gpointer user_data)
{

	StringCallBackStruct *callback_struct;

	callback_struct = g_new (StringCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	client->last_pending_call = org_freedesktop_Tracker_Search_suggest_async (client->proxy_search, search_text, maxdist,  tracker_string_reply, callback_struct);

}

