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
#include "tracker-resources-glue.h"
#include "tracker-search-glue.h"

#define TRACKER_SERVICE			"org.freedesktop.Tracker"
#define TRACKER_OBJECT			"/org/freedesktop/Tracker"
#define TRACKER_INTERFACE		"org.freedesktop.Tracker"
#define TRACKER_INTERFACE_RESOURCES	"org.freedesktop.Tracker.Resources"
#define TRACKER_INTERFACE_SEARCH	"org.freedesktop.Tracker.Search"

typedef struct {
	TrackerArrayReply callback;
	gpointer	  data;
} ArrayCallBackStruct;

typedef struct {
	TrackerGPtrArrayReply callback;
	gpointer	  data;
} GPtrArrayCallBackStruct;

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


gchar *
tracker_sparql_escape (const gchar *str)
{
	gchar       *escaped_string;
	const gchar *p;
	gchar       *q;

	escaped_string = g_malloc (2 * strlen (str) + 1);

	p = str;
	q = escaped_string;
	while (*p != '\0') {
		switch (*p) {
		case '\t':
			*q++ = '\\';
			*q++ = 't';
			break;
		case '\n':
			*q++ = '\\';
			*q++ = 'n';
			break;
		case '\r':
			*q++ = '\\';
			*q++ = 'r';
			break;
		case '\b':
			*q++ = '\\';
			*q++ = 'b';
			break;
		case '\f':
			*q++ = '\\';
			*q++ = 'f';
			break;
		case '"':
			*q++ = '\\';
			*q++ = '"';
			break;
		case '\\':
			*q++ = '\\';
			*q++ = '\\';
			break;
		default:
			*q++ = *p;
			break;
		}
		p++;
	}
	*q = '\0';

	return escaped_string;
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

	if (connection == NULL || error != NULL) {
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

	proxy = dbus_g_proxy_new_for_name (connection,
			TRACKER_SERVICE,
			TRACKER_OBJECT "/Resources",
			TRACKER_INTERFACE_RESOURCES);

	client->proxy_resources = proxy;



	return client;

}

void
tracker_disconnect (TrackerClient *client)
{
	g_object_unref (client->proxy);
	g_object_unref (client->proxy_search);
	g_object_unref (client->proxy_resources);
	client->proxy = NULL;
	client->proxy_search = NULL;
	client->proxy_resources = NULL;

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


void
tracker_resources_load (TrackerClient *client, const char *uri, GError **error)
{
	org_freedesktop_Tracker_Resources_load (client->proxy_resources, uri, &*error);
}


GPtrArray *
tracker_resources_sparql_query (TrackerClient *client, const char *query, GError **error)
{
	GPtrArray *table;

	if (!org_freedesktop_Tracker_Resources_sparql_query (client->proxy_resources, query, &table, &*error)) {
		return NULL;
	}

	return table;
}


void
tracker_resources_sparql_update (TrackerClient *client, const char *query, GError **error)
{
	org_freedesktop_Tracker_Resources_sparql_update (client->proxy_resources, query, &*error);
}


void
tracker_resources_batch_sparql_update (TrackerClient *client, const char *query, GError **error)
{
	org_freedesktop_Tracker_Resources_batch_sparql_update (client->proxy_resources, query, &*error);
}


void
tracker_resources_batch_commit (TrackerClient *client, GError **error)
{
	org_freedesktop_Tracker_Resources_batch_commit (client->proxy_resources, &*error);
}


char *
tracker_search_get_snippet (TrackerClient *client, const char *uri, const char *search_text, GError **error)
{
	char *result;

	if (!org_freedesktop_Tracker_Search_get_snippet (client->proxy_search, uri, search_text, &result, &*error)) {
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
tracker_resources_load_async (TrackerClient *client, const char *uri, TrackerVoidReply callback, gpointer user_data)
{
	VoidCallBackStruct *callback_struct;

	callback_struct = g_new (VoidCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	client->last_pending_call = org_freedesktop_Tracker_Resources_load_async (client->proxy_resources, uri, tracker_void_reply, callback_struct);

}


void
tracker_resources_sparql_query_async (TrackerClient *client, const char *query, TrackerGPtrArrayReply callback, gpointer user_data)
{
	GPtrArrayCallBackStruct *callback_struct;

	callback_struct = g_new (GPtrArrayCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	client->last_pending_call = org_freedesktop_Tracker_Resources_sparql_query_async (client->proxy_resources, query, tracker_GPtrArray_reply, callback_struct);

}


void
tracker_resources_sparql_update_async (TrackerClient *client, const char *query, TrackerVoidReply callback, gpointer user_data)
{
	VoidCallBackStruct *callback_struct;

	callback_struct = g_new (VoidCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	client->last_pending_call = org_freedesktop_Tracker_Resources_sparql_update_async (client->proxy_resources, query, tracker_void_reply, callback_struct);

}


void
tracker_resources_batch_sparql_update_async (TrackerClient *client, const char *query, TrackerVoidReply callback, gpointer user_data)
{
	VoidCallBackStruct *callback_struct;

	callback_struct = g_new (VoidCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	client->last_pending_call = org_freedesktop_Tracker_Resources_batch_sparql_update_async (client->proxy_resources, query, tracker_void_reply, callback_struct);

}


void
tracker_resources_batch_commit_async (TrackerClient *client, TrackerVoidReply callback, gpointer user_data)
{
	VoidCallBackStruct *callback_struct;

	callback_struct = g_new (VoidCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	client->last_pending_call = org_freedesktop_Tracker_Resources_batch_commit_async (client->proxy_resources, tracker_void_reply, callback_struct);

}


void
tracker_search_get_snippet_async (TrackerClient *client, const char *uri, const char *search_text, TrackerStringReply callback, gpointer user_data)
{
	StringCallBackStruct *callback_struct;

	callback_struct = g_new (StringCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	client->last_pending_call = org_freedesktop_Tracker_Search_get_snippet_async (client->proxy_search, uri, search_text, tracker_string_reply, callback_struct);

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

