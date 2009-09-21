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
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "tracker.h"

#include "tracker-resources-glue.h"
#include "tracker-statistics-glue.h"

#define TRACKER_SERVICE			"org.freedesktop.Tracker1"
#define TRACKER_OBJECT			"/org/freedesktop/Tracker1"
#define TRACKER_INTERFACE_RESOURCES	"org.freedesktop.Tracker1.Resources"
#define TRACKER_INTERFACE_STATISTICS	"org.freedesktop.Tracker1.Statistics"

typedef struct {
	TrackerReplyArray callback;
	gpointer          data;
} CallbackArray;

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

static gboolean
start_service (DBusConnection     *connection,
               const char         *name)
{
	DBusError error;
	DBusMessage *request, *reply;
	guint32 flags;

	dbus_error_init (&error);

	flags = 0;

	request = dbus_message_new_method_call (DBUS_SERVICE_DBUS, DBUS_PATH_DBUS, DBUS_INTERFACE_DBUS, "StartServiceByName");
	dbus_message_append_args (request, DBUS_TYPE_STRING, &name, DBUS_TYPE_UINT32, &flags, DBUS_TYPE_INVALID);

	reply =	dbus_connection_send_with_reply_and_block (connection, request, -1, &error);
	dbus_message_unref (request);

	if (reply == NULL) {
		dbus_error_free (&error);
		return FALSE;
	}

	if (dbus_set_error_from_message (&error, reply)) {
		dbus_message_unref (reply);
		dbus_error_free (&error);
		return FALSE;
	}

	dbus_message_unref (reply);
	return TRUE;
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

	if (!start_service (dbus_g_connection_get_connection (connection), TRACKER_SERVICE)) {
		/* unable to start tracker-store */
		dbus_g_connection_unref (connection);
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

	if (!org_freedesktop_Tracker1_Statistics_get (client->proxy_statistics, 
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
	org_freedesktop_Tracker1_Resources_load (client->proxy_resources, 
                                                uri, 
                                                &*error);
}

GPtrArray *
tracker_resources_sparql_query (TrackerClient  *client, 
                                const gchar    *query, 
                                GError        **error)
{
	GPtrArray *table;

	if (!org_freedesktop_Tracker1_Resources_sparql_query (client->proxy_resources, 
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
	org_freedesktop_Tracker1_Resources_sparql_update (client->proxy_resources, 
                                                         query, 
                                                         &*error);
}

void
tracker_resources_batch_sparql_update (TrackerClient  *client, 
                                       const gchar    *query, 
                                       GError        **error)
{
	org_freedesktop_Tracker1_Resources_batch_sparql_update (client->proxy_resources, 
                                                               query, 
                                                               &*error);
}

void
tracker_resources_batch_commit (TrackerClient  *client, 
                                GError        **error)
{
	org_freedesktop_Tracker1_Resources_batch_commit (client->proxy_resources,
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
                org_freedesktop_Tracker1_Statistics_get_async (client->proxy_statistics, 
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
                org_freedesktop_Tracker1_Resources_load_async (client->proxy_resources, 
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
                org_freedesktop_Tracker1_Resources_sparql_query_async (client->proxy_resources, 
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
                org_freedesktop_Tracker1_Resources_sparql_update_async (client->proxy_resources, 
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
                org_freedesktop_Tracker1_Resources_batch_sparql_update_async (client->proxy_resources, 
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
                org_freedesktop_Tracker1_Resources_batch_commit_async (client->proxy_resources, 
                                                                      tracker_void_reply, 
                                                                      s);
}

/* tracker_search_metadata_by_text_async is used by GTK+ */

static void
tracker_search_reply (DBusGProxy *proxy,
                      GPtrArray  *OUT_result,
                      GError     *error,
                      gpointer    user_data)
{

	CallbackArray *s;
	gchar        **uris;
	gint           i;

	s = user_data;

	uris = g_new0 (gchar *, OUT_result->len + 1);
	for (i = 0; i < OUT_result->len; i++) {
		uris[i] = ((gchar **) OUT_result->pdata[i])[0];
	}

	(*(TrackerReplyArray) s->callback) (uris,
                                            error,
                                            s->data);

	g_ptr_array_foreach (OUT_result, (GFunc) g_free, NULL);
	g_ptr_array_free (OUT_result, TRUE);
	g_free (s);
}

static void
sparql_append_string_literal (GString     *sparql,
                              const gchar *str)
{
	g_string_append_c (sparql, '"');

	while (*str != '\0') {
		gsize len = strcspn (str, "\t\n\r\"\\");
		g_string_append_len (sparql, str, len);
		str += len;
		switch (*str) {
		case '\t':
			g_string_append (sparql, "\\t");
			break;
		case '\n':
			g_string_append (sparql, "\\n");
			break;
		case '\r':
			g_string_append (sparql, "\\r");
			break;
		case '"':
			g_string_append (sparql, "\\\"");
			break;
		case '\\':
			g_string_append (sparql, "\\\\");
			break;
		default:
			continue;
		}
		str++;
	}

	g_string_append_c (sparql, '"');
}

void
tracker_search_metadata_by_text_async (TrackerClient         *client,
                                       const gchar           *query,
                                       TrackerReplyArray      callback,
                                       gpointer               user_data)
{
	CallbackArray *s;
	GString *sparql;

	g_return_if_fail (client != NULL);
	g_return_if_fail (query != NULL);
	g_return_if_fail (callback != NULL);

	s = g_new0 (CallbackArray, 1);
	s->callback = callback;
	s->data = user_data;

	sparql = g_string_new ("SELECT ?file WHERE { ?file a nfo:FileDataObject ; fts:match ");
	sparql_append_string_literal (sparql, query);
	g_string_append (sparql, " }");

        client->pending_proxy = client->proxy_resources;
	client->pending_call =
                org_freedesktop_Tracker1_Resources_sparql_query_async (client->proxy_resources,
                                                                       sparql->str,
                                                                       tracker_search_reply,
                                                                       s);

	g_string_free (sparql, TRUE);
}

void
tracker_search_metadata_by_text_and_location_async (TrackerClient         *client,
                                                    const gchar           *query,
                                                    const gchar           *location,
                                                    TrackerReplyArray      callback,
                                                    gpointer               user_data)
{
	CallbackArray *s;
	GString *sparql;

	g_return_if_fail (client != NULL);
	g_return_if_fail (query != NULL);
	g_return_if_fail (location != NULL);
	g_return_if_fail (callback != NULL);

	s = g_new0 (CallbackArray, 1);
	s->callback = callback;
	s->data = user_data;

	sparql = g_string_new ("SELECT ?file WHERE { ?file a nfo:FileDataObject ; fts:match ");
	sparql_append_string_literal (sparql, query);
	g_string_append (sparql, " . FILTER (fn:starts-with(?file,");
	sparql_append_string_literal (sparql, location);
	g_string_append (sparql, ")) }");

        client->pending_proxy = client->proxy_resources;
	client->pending_call =
                org_freedesktop_Tracker1_Resources_sparql_query_async (client->proxy_resources,
                                                                       sparql->str,
                                                                       tracker_search_reply,
                                                                       s);

	g_string_free (sparql, TRUE);
}

void
tracker_search_metadata_by_text_and_mime_async (TrackerClient         *client,
                                                    const gchar           *query,
                                                    const gchar          **mimes,
                                                    TrackerReplyArray      callback,
                                                    gpointer               user_data)
{
	CallbackArray *s;
	GString *sparql;
	gint i;

	g_return_if_fail (client != NULL);
	g_return_if_fail (query != NULL);
	g_return_if_fail (mimes != NULL);
	g_return_if_fail (callback != NULL);

	s = g_new0 (CallbackArray, 1);
	s->callback = callback;
	s->data = user_data;

	sparql = g_string_new ("SELECT ?file WHERE { ?file a nfo:FileDataObject ; nie:mimeType ?mime ; fts:match ");
	sparql_append_string_literal (sparql, query);
	g_string_append (sparql, " . FILTER (");
	for (i = 0; mimes[i]; i++) {
		if (i > 0) {
			g_string_append (sparql, " || ");
		}
		g_string_append (sparql, "?mime = ");
		sparql_append_string_literal (sparql, mimes[i]);
	}
	g_string_append (sparql, ") }");

        client->pending_proxy = client->proxy_resources;
	client->pending_call =
                org_freedesktop_Tracker1_Resources_sparql_query_async (client->proxy_resources,
                                                                       sparql->str,
                                                                       tracker_search_reply,
                                                                       s);

	g_string_free (sparql, TRUE);
}

void
tracker_search_metadata_by_text_and_mime_and_location_async (TrackerClient         *client,
                                                    const gchar           *query,
                                                    const gchar          **mimes,
                                                    const gchar           *location,
                                                    TrackerReplyArray      callback,
                                                    gpointer               user_data)
{
	CallbackArray *s;
	GString *sparql;
	gint i;

	g_return_if_fail (client != NULL);
	g_return_if_fail (query != NULL);
	g_return_if_fail (mimes != NULL);
	g_return_if_fail (location != NULL);
	g_return_if_fail (callback != NULL);

	s = g_new0 (CallbackArray, 1);
	s->callback = callback;
	s->data = user_data;

	sparql = g_string_new ("SELECT ?file WHERE { ?file a nfo:FileDataObject ; nie:mimeType ?mime ; fts:match ");
	sparql_append_string_literal (sparql, query);
	g_string_append (sparql, " . FILTER (fn:starts-with(?file,");
	sparql_append_string_literal (sparql, location);
	g_string_append (sparql, ")");
	g_string_append (sparql, " && (");
	for (i = 0; mimes[i]; i++) {
		if (i > 0) {
			g_string_append (sparql, " || ");
		}
		g_string_append (sparql, "?mime = ");
		sparql_append_string_literal (sparql, mimes[i]);
	}
	g_string_append (sparql, ")");
	g_string_append (sparql, ") }");

        client->pending_proxy = client->proxy_resources;
	client->pending_call =
                org_freedesktop_Tracker1_Resources_sparql_query_async (client->proxy_resources,
                                                                       sparql->str,
                                                                       tracker_search_reply,
                                                                       s);

	g_string_free (sparql, TRUE);
}

