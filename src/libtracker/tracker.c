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

#include "tracker-resources-glue.h"
#include "tracker-statistics-glue.h"

#define TRACKER_SERVICE			"org.freedesktop.Tracker"
#define TRACKER_OBJECT			"/org/freedesktop/Tracker"
#define TRACKER_INTERFACE_RESOURCES	"org.freedesktop.Tracker.Resources"
#define TRACKER_INTERFACE_STATISTICS	"org.freedesktop.Tracker.Statistics"

typedef struct {
	TrackerReplyGPtrArray callback;
	gpointer	      data;
} CallbackGPtrArray;

typedef struct {
	TrackerReplyVoid callback;
	gpointer	 data;
} CallbackVoid;

static void
tracker_GPtrArray_reply (DBusGProxy *proxy,  
                         GPtrArray  *OUT_result, 
                         GError     *error, 
                         gpointer    user_data)
{

	CallbackGPtrArray *s;

	s = user_data;

	(*(TrackerReplyGPtrArray) s->callback) (OUT_result, 
                                                error, 
                                                s->data);

	g_free (s);
}

static void
tracker_void_reply (DBusGProxy *proxy, 
                    GError     *error, 
                    gpointer    user_data)
{

	CallbackVoid *s;

	s = user_data;

	(*(TrackerReplyVoid) s->callback) (error, 
                                           s->data);

	g_free (s);
}

/* Copied from tracker-module-metadata.c */
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
tracker_connect (gboolean enable_warnings, gint timeout)
{
	DBusGConnection *connection;
	GError *error = NULL;
	TrackerClient *client = NULL;

	g_type_init ();

	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

	if (connection == NULL || error != NULL) {
		if (enable_warnings) {
			g_warning("Unable to connect to dbus: %s\n", error->message);
		}
		g_error_free (error);
		return NULL;
	}

	client = g_new0 (TrackerClient, 1);

	client->proxy_statistics = 
                dbus_g_proxy_new_for_name (connection,
                                           TRACKER_SERVICE,
                                           TRACKER_OBJECT "/Statistics",
                                           TRACKER_INTERFACE_STATISTICS);

	client->proxy_resources = 
                dbus_g_proxy_new_for_name (connection,
                                           TRACKER_SERVICE,
                                           TRACKER_OBJECT "/Resources",
                                           TRACKER_INTERFACE_RESOURCES);

	if (timeout > 0) {
		dbus_g_proxy_set_default_timeout (client->proxy_resources, timeout);
	}

	return client;
}

void
tracker_disconnect (TrackerClient *client)
{
        if (client->proxy_statistics) {
                g_object_unref (client->proxy_statistics);
        }

        if (client->proxy_resources) {
                g_object_unref (client->proxy_resources);
        }

	g_free (client);
}

void
tracker_cancel_last_call (TrackerClient *client)
{
        
	dbus_g_proxy_cancel_call (client->pending_proxy, 
                                  client->pending_call);
}

/* dbus synchronous calls */
GPtrArray *
tracker_statistics_get (TrackerClient  *client,  
                        GError        **error)
{
	GPtrArray *table;

	if (!org_freedesktop_Tracker_Statistics_get (client->proxy_statistics, 
                                                     &table, 
                                                     &*error)) {
		return NULL;
	}

	return table;
}

void
tracker_resources_load (TrackerClient  *client, 
                        const gchar    *uri, 
                        GError        **error)
{
	org_freedesktop_Tracker_Resources_load (client->proxy_resources, 
                                                uri, 
                                                &*error);
}

GPtrArray *
tracker_resources_sparql_query (TrackerClient  *client, 
                                const gchar    *query, 
                                GError        **error)
{
	GPtrArray *table;

	if (!org_freedesktop_Tracker_Resources_sparql_query (client->proxy_resources, 
                                                             query, 
                                                             &table, 
                                                             &*error)) {
		return NULL;
	}

	return table;
}

void
tracker_resources_sparql_update (TrackerClient  *client, 
                                 const gchar    *query, 
                                 GError        **error)
{
	org_freedesktop_Tracker_Resources_sparql_update (client->proxy_resources, 
                                                         query, 
                                                         &*error);
}

void
tracker_resources_batch_sparql_update (TrackerClient  *client, 
                                       const gchar    *query, 
                                       GError        **error)
{
	org_freedesktop_Tracker_Resources_batch_sparql_update (client->proxy_resources, 
                                                               query, 
                                                               &*error);
}

void
tracker_resources_batch_commit (TrackerClient  *client, 
                                GError        **error)
{
	org_freedesktop_Tracker_Resources_batch_commit (client->proxy_resources,
                                                        &*error);
}

void
tracker_statistics_get_async (TrackerClient         *client,  
                              TrackerReplyGPtrArray  callback, 
                              gpointer               user_data)
{
	CallbackGPtrArray *s;

	s = g_new0 (CallbackGPtrArray, 1);
	s->callback = callback;
	s->data = user_data;

        client->pending_proxy = client->proxy_statistics;
	client->pending_call = 
                org_freedesktop_Tracker_Statistics_get_async (client->proxy_statistics, 
                                                              tracker_GPtrArray_reply, 
                                                              s);
}

void
tracker_resources_load_async (TrackerClient     *client, 
                              const gchar       *uri, 
                              TrackerReplyVoid  callback, 
                              gpointer user_data)
{
	CallbackVoid *s;

	s = g_new0 (CallbackVoid, 1);
	s->callback = callback;
	s->data = user_data;

        client->pending_proxy = client->proxy_resources;
	client->pending_call = 
                org_freedesktop_Tracker_Resources_load_async (client->proxy_resources, 
                                                              uri, 
                                                              tracker_void_reply, 
                                                              s);
}

void
tracker_resources_sparql_query_async (TrackerClient         *client, 
                                      const gchar           *query, 
                                      TrackerReplyGPtrArray  callback, 
                                      gpointer               user_data)
{
	CallbackGPtrArray *s;

	s = g_new0 (CallbackGPtrArray, 1);
	s->callback = callback;
	s->data = user_data;

        client->pending_proxy = client->proxy_resources;
	client->pending_call = 
                org_freedesktop_Tracker_Resources_sparql_query_async (client->proxy_resources, 
                                                                      query, 
                                                                      tracker_GPtrArray_reply, 
                                                                      s);
}

void
tracker_resources_sparql_update_async (TrackerClient    *client, 
                                       const gchar      *query, 
                                       TrackerReplyVoid  callback, 
                                       gpointer          user_data)
{
	CallbackVoid *s;

	s = g_new0 (CallbackVoid, 1);
	s->callback = callback;
	s->data = user_data;

        client->pending_proxy = client->proxy_resources;
	client->pending_call = 
                org_freedesktop_Tracker_Resources_sparql_update_async (client->proxy_resources, 
                                                                       query, 
                                                                       tracker_void_reply, 
                                                                       s);
}

void
tracker_resources_batch_sparql_update_async (TrackerClient    *client, 
                                             const gchar      *query, 
                                             TrackerReplyVoid  callback, 
                                             gpointer          user_data)
{
	CallbackVoid *s;

	s = g_new0 (CallbackVoid, 1);
	s->callback = callback;
	s->data = user_data;

        client->pending_proxy = client->proxy_resources;
	client->pending_call = 
                org_freedesktop_Tracker_Resources_batch_sparql_update_async (client->proxy_resources, 
                                                                             query, 
                                                                             tracker_void_reply, 
                                                                             s);
}

void
tracker_resources_batch_commit_async (TrackerClient    *client, 
                                      TrackerReplyVoid  callback, 
                                      gpointer          user_data)
{
	CallbackVoid *s;

	s = g_new0 (CallbackVoid, 1);
	s->callback = callback;
	s->data = user_data;

        client->pending_proxy = client->proxy_resources;
	client->pending_call = 
                org_freedesktop_Tracker_Resources_batch_commit_async (client->proxy_resources, 
                                                                      tracker_void_reply, 
                                                                      s);
}
