/*
 * Copyright (C) 2022, Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#include "tracker-http-module.h"

#include <libsoup/soup.h>

#ifdef HAVE_AVAHI
#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-glib/glib-watch.h>
#endif

static void tracker_http_server_soup_error (TrackerHttpServer       *server,
                                            TrackerHttpRequest      *request,
                                            gint                     code,
                                            const gchar             *message);

GType
tracker_http_client_get_type (void)
{
	return g_type_from_name ("TrackerHttpClient");
}

GType
tracker_http_server_get_type (void)
{
	return g_type_from_name ("TrackerHttpServer");
}

static const gchar *mimetypes[] = {
	"application/sparql-results+json",
	"application/sparql-results+xml",
	"text/turtle",
	"application/trig",
	"application/ld+json",
};

G_STATIC_ASSERT (G_N_ELEMENTS (mimetypes) == TRACKER_N_SERIALIZER_FORMATS);

#define CONTENT_TYPE "application/sparql-query"
#define USER_AGENT "Tracker " PACKAGE_VERSION " (https://gitlab.gnome.org/GNOME/tracker/issues/)"

#if SOUP_CHECK_VERSION (2, 99, 2)
typedef SoupServerMessage TrackerSoupMessage;
#else
typedef SoupMessage TrackerSoupMessage;
#endif

/* Server */
struct _TrackerHttpRequest
{
	TrackerHttpServer *server;
	TrackerSoupMessage *message;
	GTask *task;
	GInputStream *istream;

	GSocketAddress *remote_address;
	gchar *path;
	GHashTable *params;
};

struct _TrackerHttpServerSoup
{
	TrackerHttpServer parent_instance;
	SoupServer *server;
	GCancellable *cancellable;

#ifdef HAVE_AVAHI
	AvahiGLibPoll *avahi_glib_poll;
	AvahiClient *avahi_client;
	AvahiEntryGroup *avahi_entry_group;
#endif
};

static void tracker_http_server_soup_initable_iface_init (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (TrackerHttpServerSoup,
                         tracker_http_server_soup,
                         TRACKER_TYPE_HTTP_SERVER,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                tracker_http_server_soup_initable_iface_init))

static TrackerHttpRequest *
request_new (TrackerHttpServer  *http_server,
             TrackerSoupMessage *message,
             GSocketAddress     *remote_address,
             const gchar        *path,
             GHashTable         *params)
{
	TrackerHttpRequest *request;

	request = g_new0 (TrackerHttpRequest, 1);
	request->server = http_server;
	request->message = g_object_ref (message);
	request->remote_address = g_object_ref (remote_address);
	request->path = g_strdup (path);

	if (params) {
		request->params = g_hash_table_ref (params);
	}

	return request;
}

static void
request_free (TrackerHttpRequest *request)
{
	g_clear_object (&request->istream);
	g_clear_object (&request->message);
	g_clear_object (&request->remote_address);
	g_free (request->path);
	g_clear_pointer (&request->params, g_hash_table_unref);
	g_free (request);
}

/* Read the "Accept" header of the request, and return the possible
 * serialization formats as a bitmask of TrackerSerializerFormat values.
 */
static guint
get_supported_formats (TrackerHttpRequest *request)
{
	SoupMessageHeaders *request_headers;
	TrackerSerializerFormat i;
	guint formats = 0;

#if SOUP_CHECK_VERSION (2, 99, 2)
        request_headers = soup_server_message_get_request_headers (request->message);
#else
        request_headers = request->message->request_headers;
#endif

        for (i = 0; i < TRACKER_N_SERIALIZER_FORMATS; i++) {
	        if (soup_message_headers_header_contains (request_headers, "Accept",
	                                                  mimetypes[i]))
		        formats |= 1 << i;
        }

	return formats;
}

static void
set_message_format (TrackerHttpRequest      *request,
                    TrackerSerializerFormat  format)
{
        SoupMessageHeaders *response_headers;

#if SOUP_CHECK_VERSION (2, 99, 2)
        response_headers = soup_server_message_get_response_headers (request->message);
#else
        response_headers = request->message->response_headers;
#endif
        soup_message_headers_set_content_type (response_headers, mimetypes[format], NULL);
}

/* Get SPARQL query from message POST data, or NULL. */
static gchar *
get_sparql_from_message_body (TrackerSoupMessage *message)
{
	SoupMessageBody *body;
	GBytes *body_bytes;
	const gchar *body_data;
	gsize body_size;
#if SOUP_CHECK_VERSION (2, 99, 2)
#else
	SoupBuffer *buffer;
#endif

#if SOUP_CHECK_VERSION (2, 99, 2)
	body = soup_server_message_get_request_body (message);
	body_bytes = soup_message_body_flatten (body);
#else
	body = message->request_body;
	buffer = soup_message_body_flatten (body);
	body_bytes = soup_buffer_get_as_bytes (buffer);
#endif

	body_data = g_bytes_get_data (body_bytes, &body_size);

	if (g_utf8_validate_len (body_data, body_size, NULL)) {
		gchar *sparql = g_malloc (body_size + 1);

		g_utf8_strncpy (sparql, body_data, body_size);
		sparql[body_size] = 0;

		return sparql;
	}

	return NULL;
}

/* Handle POST messages if the body wasn't immediately available. */
static void
server_callback_got_message_body (TrackerSoupMessage *message,
                                  gpointer            user_data)
{
	TrackerHttpRequest *request = user_data;
	gchar *sparql;

	sparql = get_sparql_from_message_body (message);

	if (sparql) {
		if (!request->params) {
			request->params = g_hash_table_new (g_str_hash, g_str_equal);
		}

		g_hash_table_insert (request->params, "query", sparql);

		g_signal_emit_by_name (request->server, "request",
		                       request->remote_address,
		                       request->path,
		                       request->params,
		                       get_supported_formats (request),
		                       request);
	} else {
		tracker_http_server_soup_error (request->server, request, 400, "Missing query or invalid UTF-8 in POST request");
	}
}

#if SOUP_CHECK_VERSION (2, 99, 2)
static void
server_callback (SoupServer        *server,
	         SoupServerMessage *message,
                 const char        *path,
	         GHashTable        *query,
                 gpointer           user_data)
#else
static void
server_callback (SoupServer        *server,
                 SoupMessage       *message,
                 const char        *path,
                 GHashTable        *query,
                 SoupClientContext *client,
                 gpointer           user_data)
#endif
{
	TrackerHttpServer *http_server = user_data;
	GSocketAddress *remote_address;
	TrackerHttpRequest *request;
	const char *method;
	SoupMessageBody *body;

#if SOUP_CHECK_VERSION (2, 99, 2)
	remote_address = soup_server_message_get_remote_address (message);
#else
	remote_address = soup_client_context_get_remote_address (client);
#endif

	request = request_new (http_server, message, remote_address, path, query);

#if SOUP_CHECK_VERSION (3, 1, 3)
	soup_server_message_pause (message);
#else
	soup_server_pause_message (server, message);
#endif

#if SOUP_CHECK_VERSION (3, 1, 3)
	method = soup_server_message_get_method (message);
#else
	method = message->method;
#endif

	if (g_strcmp0 (method, SOUP_METHOD_POST) == 0) {
#if SOUP_CHECK_VERSION (2, 99, 2)
		body = soup_server_message_get_request_body (request->message);
#else
		body = request->message->request_body;
#endif

		if (body->data) {
			server_callback_got_message_body (message, request);
		} else {
			g_debug ("Received HTTP POST for %s with no body, awaiting data", path);
			g_signal_connect (message, "got-body", G_CALLBACK (server_callback_got_message_body), request);
		}
	} else {
		g_signal_emit_by_name (http_server, "request",
		                       remote_address,
		                       path,
		                       query,
		                       get_supported_formats (request),
		                       request);
	}
}

#ifdef HAVE_AVAHI
static void
avahi_entry_group_cb (AvahiEntryGroup      *entry_group,
                      AvahiEntryGroupState  state,
                      gpointer              user_data)
{

	TrackerHttpServerSoup *server = user_data;

	switch (state) {
	case AVAHI_ENTRY_GROUP_COLLISION:
	case AVAHI_ENTRY_GROUP_FAILURE:
		g_clear_pointer (&server->avahi_entry_group, avahi_entry_group_free);
		break;
	case AVAHI_ENTRY_GROUP_UNCOMMITED:
	case AVAHI_ENTRY_GROUP_REGISTERING:
	case AVAHI_ENTRY_GROUP_ESTABLISHED:
		break;
	}
}

static AvahiStringList *
create_txt (TrackerHttpServerSoup *server,
            AvahiClient           *client)
{
	AvahiStringList *txt = NULL;
	gchar *metadata, *path;
	GTlsCertificate *certificate;
	guint port;

	g_object_get (server,
	              "http-certificate", &certificate,
	              "http-port", &port,
	              NULL);

	metadata = g_strdup_printf ("%s://%s:%d/sparql",
	                            certificate != NULL ? "https" : "http",
	                            avahi_client_get_host_name_fqdn (client),
	                            port);
	if (certificate) {
		/* Pass on full URI to express this is an HTTPS endpoint */
		path = g_strdup (metadata);
	} else {
		path = g_strdup ("/sparql");
	}

	txt = avahi_string_list_add_pair (txt, "path", path);
	txt = avahi_string_list_add_pair (txt, "metadata", metadata);
	g_free (metadata);
	g_free (path);

	/* FIXME: The "vocabs" TXT record is supposed to contain the namespaces
	 * used by the endpoint. However there is a size limit to these records,
	 * obviously exceeded by the multiple Nepomuk namespaces.
	 *
	 * Perhaps we could just fill in this record in the cases that the
	 * resulting string is short enough. Just leave an empty string for
	 * everyone, for now.
	 */
	txt = avahi_string_list_add_pair (txt, "vocabs", "");

	txt = avahi_string_list_add_pair (txt, "binding", "HTTP");
	txt = avahi_string_list_add_pair (txt, "protovers", "1.1");
	txt = avahi_string_list_add_pair (txt, "txtvers", "1");

	g_clear_object (&certificate);

	return txt;
}

static void
publish_endpoint (TrackerHttpServerSoup *server,
                  AvahiClient           *client)
{
	AvahiStringList *txt;
	guint port;
	gchar *name;

	if (!server->avahi_entry_group) {
		server->avahi_entry_group =
			avahi_entry_group_new (client,
			                       avahi_entry_group_cb,
			                       server);
		if (!server->avahi_entry_group)
			goto error;
	}

	g_object_get (server,
	              "http-port", &port,
	              NULL);

	name = g_strdup_printf ("Tracker SPARQL endpoint (port: %d)", port);
	txt = create_txt (server, client);

	if (avahi_entry_group_add_service_strlst (server->avahi_entry_group,
	                                          AVAHI_IF_UNSPEC,
	                                          AVAHI_PROTO_UNSPEC,
	                                          0,
	                                          name,
	                                          "_sparql._tcp",
	                                          NULL,
	                                          NULL,
	                                          port,
	                                          txt) < 0)
		goto error;

	avahi_string_list_free (txt);
	g_free (name);

	if (server->avahi_entry_group &&
	    avahi_entry_group_commit (server->avahi_entry_group) < 0)
		goto error;

	return;
 error:
	g_clear_pointer (&server->avahi_entry_group, avahi_entry_group_free);
}

static void
avahi_client_cb (AvahiClient      *client,
                 AvahiClientState  state,
                 gpointer          user_data)
{
	TrackerHttpServerSoup *server = user_data;

	switch (state) {
	case AVAHI_CLIENT_S_RUNNING:
		publish_endpoint (server, client);
		break;
	case AVAHI_CLIENT_S_COLLISION:
	case AVAHI_CLIENT_FAILURE:
		g_clear_pointer (&server->avahi_entry_group, avahi_entry_group_free);
		g_clear_pointer (&server->avahi_client, avahi_client_free);
		g_clear_pointer (&server->avahi_glib_poll, avahi_glib_poll_free);
		break;
	case AVAHI_CLIENT_S_REGISTERING:
	case AVAHI_CLIENT_CONNECTING:
		break;
	}
}
#endif /* HAVE_AVAHI */

static gboolean
tracker_http_server_initable_init (GInitable     *initable,
                                   GCancellable  *cancellable,
                                   GError       **error)
{
	TrackerHttpServerSoup *server = TRACKER_HTTP_SERVER_SOUP (initable);
	GTlsCertificate *certificate;
	guint port;

	g_object_get (initable,
	              "http-certificate", &certificate,
	              "http-port", &port,
	              NULL);

	server->server =
		soup_server_new ("tls-certificate", certificate,
		                 "server-header", USER_AGENT,
		                 NULL);
	soup_server_add_handler (server->server,
	                         "/sparql",
	                         server_callback,
	                         initable,
	                         NULL);
	g_clear_object (&certificate);

#ifdef HAVE_AVAHI
	server->avahi_glib_poll =
		avahi_glib_poll_new (g_main_context_get_thread_default (),
		                     G_PRIORITY_DEFAULT);
	if (server->avahi_glib_poll) {
		server->avahi_client =
			avahi_client_new (avahi_glib_poll_get (server->avahi_glib_poll),
			                  AVAHI_CLIENT_IGNORE_USER_CONFIG |
			                  AVAHI_CLIENT_NO_FAIL,
			                  avahi_client_cb,
			                  initable,
			                  NULL);
	}
#endif

	return soup_server_listen_all (server->server,
	                               port,
	                               0, error);
}

static void
tracker_http_server_soup_initable_iface_init (GInitableIface *iface)
{
	iface->init = tracker_http_server_initable_init;
}

static void
tracker_http_server_soup_error (TrackerHttpServer       *server,
                                TrackerHttpRequest      *request,
                                gint                     code,
                                const gchar             *message)
{
	G_GNUC_UNUSED TrackerHttpServerSoup *server_soup =
		TRACKER_HTTP_SERVER_SOUP (server);

	g_assert (request->server == server);

#if SOUP_CHECK_VERSION (2, 99, 2)
	soup_server_message_set_status (request->message, code, message);
#else
	soup_message_set_status_full (request->message, code, message);
#endif

#if SOUP_CHECK_VERSION (3, 1, 3)
	soup_server_message_unpause (request->message);
#else
	soup_server_unpause_message (server_soup->server, request->message);
#endif
	request_free (request);
}

static void
handle_write_in_thread (GTask        *task,
                        gpointer      source_object,
                        gpointer      task_data,
                        GCancellable *cancellable)
{
	TrackerHttpRequest *request = task_data;
	gchar buffer[1000];
	SoupMessageBody *message_body;
	GError *error = NULL;
	gssize count;

#if SOUP_CHECK_VERSION (2, 99, 2)
        message_body = soup_server_message_get_response_body (request->message);
#else
        message_body = request->message->response_body;
#endif

	for (;;) {
		count = g_input_stream_read (request->istream,
		                             buffer, sizeof (buffer),
		                             cancellable, &error);
		if (count < 0)
			break;

		soup_message_body_append (message_body,
		                          SOUP_MEMORY_COPY,
		                          buffer, count);

		if ((gsize) count < sizeof (buffer)) {
			break;
		}
	}

	g_input_stream_close (request->istream, cancellable, NULL);
	g_clear_object (&request->istream);
	soup_message_body_complete (message_body);

	if (error)
		g_task_return_error (task, error);
	else
		g_task_return_boolean (task, TRUE);

	g_object_unref (task);
}

static void
write_finished_cb (GObject      *object,
                   GAsyncResult *result,
                   gpointer      user_data)
{
	TrackerHttpRequest *request = user_data;
	G_GNUC_UNUSED TrackerHttpServerSoup *server =
		TRACKER_HTTP_SERVER_SOUP (request->server);
	GError *error = NULL;

	if (!g_task_propagate_boolean (G_TASK (result), &error)) {
		tracker_http_server_soup_error (request->server,
		                                request,
		                                500,
		                                error->message);
		g_clear_error (&error);
	} else {
#if SOUP_CHECK_VERSION (2, 99, 2)
                soup_server_message_set_status (request->message, 200, NULL);
#else
		soup_message_set_status (request->message, 200);
#endif

#if SOUP_CHECK_VERSION (3, 1, 3)
		soup_server_message_unpause (request->message);
#else
		soup_server_unpause_message (server->server, request->message);
#endif

		request_free (request);
	}
}

static void
tracker_http_server_soup_response (TrackerHttpServer       *server,
                                   TrackerHttpRequest      *request,
                                   TrackerSerializerFormat  format,
                                   GInputStream            *content)
{
	TrackerHttpServerSoup *server_soup =
		TRACKER_HTTP_SERVER_SOUP (server);

	g_assert (request->server == server);

	set_message_format (request, format);

	request->istream = content;
	request->task = g_task_new (server, server_soup->cancellable,
	                            write_finished_cb, request);

	g_task_set_task_data (request->task, request, NULL);
	g_task_run_in_thread (request->task, handle_write_in_thread);
}

static void
tracker_http_server_soup_finalize (GObject *object)
{
	TrackerHttpServerSoup *server =
		TRACKER_HTTP_SERVER_SOUP (object);

	g_cancellable_cancel (server->cancellable);
	g_object_unref (server->cancellable);

	g_clear_object (&server->server);

#ifdef HAVE_AVAHI
	g_clear_pointer (&server->avahi_entry_group, avahi_entry_group_free);
	g_clear_pointer (&server->avahi_client, avahi_client_free);
	g_clear_pointer (&server->avahi_glib_poll, avahi_glib_poll_free);
#endif

	G_OBJECT_CLASS (tracker_http_server_soup_parent_class)->finalize (object);
}

static void
tracker_http_server_soup_class_init (TrackerHttpServerSoupClass *klass)
{
	TrackerHttpServerClass *server_class = TRACKER_HTTP_SERVER_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_http_server_soup_finalize;

	server_class->response = tracker_http_server_soup_response;
	server_class->error = tracker_http_server_soup_error;
}

static void
tracker_http_server_soup_init (TrackerHttpServerSoup *server)
{
	server->cancellable = g_cancellable_new ();
}

/* Client */
struct _TrackerHttpClientSoup
{
	TrackerHttpClient parent_instance;
	SoupSession *session;
};

G_DEFINE_TYPE (TrackerHttpClientSoup, tracker_http_client_soup,
               TRACKER_TYPE_HTTP_CLIENT)

static gboolean
get_content_type_format (SoupMessage              *message,
                         TrackerSerializerFormat  *format,
                         GError                  **error)
{
	SoupMessageHeaders *response_headers;
	gint status_code;
	const gchar *content_type;
	TrackerSerializerFormat i;

#if SOUP_CHECK_VERSION (2, 99, 2)
	status_code = soup_message_get_status (message);
	response_headers = soup_message_get_response_headers (message);
#else
	status_code = message->status_code;
	response_headers = message->response_headers;
#endif

	if (status_code != SOUP_STATUS_OK) {
		g_set_error (error,
		             G_IO_ERROR,
		             G_IO_ERROR_FAILED,
		             "Unhandled status code %d",
		             status_code);
		return FALSE;
	}

	content_type = soup_message_headers_get_content_type (response_headers, NULL);

	for (i = 0; i < TRACKER_N_SERIALIZER_FORMATS; i++) {
		if (g_strcmp0 (content_type, mimetypes[i]) == 0) {
			*format = i;
			return TRUE;
		}
	}

	g_set_error (error,
	             G_IO_ERROR,
	             G_IO_ERROR_FAILED,
	             "Unhandled content type '%s'",
	             soup_message_headers_get_content_type (response_headers, NULL));
	return FALSE;
}

static void
send_message_cb (GObject      *source,
                 GAsyncResult *res,
                 gpointer      user_data)
{
	GTask *task = user_data;
	GInputStream *stream;
	GError *error = NULL;
	SoupMessage *message;

	stream = soup_session_send_finish (SOUP_SESSION (source), res, &error);
	message = g_task_get_task_data (task);

	if (stream) {
		TrackerSerializerFormat format;

		if (!get_content_type_format (message, &format, &error)) {
			g_task_return_error (task, error);
		} else {
			g_task_set_task_data (task, GUINT_TO_POINTER (format), NULL);
			g_task_return_pointer (task, stream, g_object_unref);
		}
	} else {
		g_task_return_error (task, error);
	}

	g_object_unref (task);
}

static void
add_accepted_formats (SoupMessageHeaders *headers,
                      guint               formats)
{
	TrackerSerializerFormat i;

	for (i = 0; i < TRACKER_N_SERIALIZER_FORMATS; i++) {
		if ((formats & (1 << i)) == 0)
			continue;

		soup_message_headers_append (headers, "Accept", mimetypes[i]);
	}
}

static SoupMessage *
create_message (const gchar *uri,
                const gchar *query,
                guint        formats)
{
	SoupMessage *message;
	SoupMessageHeaders *headers;
#if SOUP_CHECK_VERSION (2, 99, 2)
	GBytes *query_encoded;
#endif

	message = soup_message_new ("POST", uri);

#if SOUP_CHECK_VERSION (2, 99, 2)
	headers = soup_message_get_request_headers (message);
#else
	headers = message->request_headers;
#endif

	soup_message_headers_append (headers, "User-Agent", USER_AGENT);
	add_accepted_formats (headers, formats);

#if SOUP_CHECK_VERSION (2, 99, 2)
	query_encoded = g_bytes_new (query, strlen (query));
	soup_message_set_request_body_from_bytes (message, "application/sparql-query", query_encoded);
#else
	soup_message_set_request (message, "application/sparql-query", SOUP_MEMORY_COPY, query, strlen (query));
#endif

	return message;
}

static void
tracker_http_client_soup_send_message_async (TrackerHttpClient   *client,
                                             const gchar         *uri,
                                             const gchar         *query,
                                             guint                formats,
                                             GCancellable        *cancellable,
                                             GAsyncReadyCallback  callback,
                                             gpointer             user_data)
{
	TrackerHttpClientSoup *client_soup = TRACKER_HTTP_CLIENT_SOUP (client);
	SoupMessage *message;
	GTask *task;

	task = g_task_new (client, cancellable, callback, user_data);

	message = create_message (uri, query, formats);
	g_task_set_task_data (task, message, g_object_unref);

#if SOUP_CHECK_VERSION (2, 99, 2)
	soup_session_send_async (client_soup->session,
	                         message,
	                         G_PRIORITY_DEFAULT,
	                         cancellable,
	                         send_message_cb,
	                         task);
#else
	soup_session_send_async (client_soup->session,
	                         message,
	                         cancellable,
	                         send_message_cb,
	                         task);
#endif
}

static GInputStream *
tracker_http_client_soup_send_message_finish (TrackerHttpClient        *client,
                                              GAsyncResult             *res,
                                              TrackerSerializerFormat  *format,
                                              GError                  **error)
{
	if (format)
		*format = GPOINTER_TO_UINT (g_task_get_task_data (G_TASK (res)));

	return g_task_propagate_pointer (G_TASK (res), error);
}

static GInputStream *
tracker_http_client_soup_send_message (TrackerHttpClient        *client,
                                       const gchar              *uri,
                                       const gchar              *query,
                                       guint                     formats,
                                       GCancellable             *cancellable,
                                       TrackerSerializerFormat  *format,
                                       GError                  **error)
{
	TrackerHttpClientSoup *client_soup = TRACKER_HTTP_CLIENT_SOUP (client);
	SoupMessage *message;
	GInputStream *stream;

	message = create_message (uri, query, formats);

#if SOUP_CHECK_VERSION (2, 99, 2)
	stream = soup_session_send (client_soup->session,
	                            message,
	                            cancellable,
	                            error);
#else
	stream = soup_session_send (client_soup->session,
	                            message,
	                            cancellable,
	                            error);
#endif
	if (!stream)
		return NULL;

	if (!get_content_type_format (message, format, error)) {
		g_clear_object (&stream);
		return NULL;
	}

	return stream;
}

static void
tracker_http_client_soup_class_init (TrackerHttpClientSoupClass *klass)
{
	TrackerHttpClientClass *client_class =
		TRACKER_HTTP_CLIENT_CLASS (klass);

	client_class->send_message_async = tracker_http_client_soup_send_message_async;
	client_class->send_message_finish = tracker_http_client_soup_send_message_finish;
	client_class->send_message = tracker_http_client_soup_send_message;
}

static void
tracker_http_client_soup_init (TrackerHttpClientSoup *client)
{
	client->session = soup_session_new ();
}

void
initialize_types (GType *client,
                  GType *server)
{
	*client = TRACKER_TYPE_HTTP_CLIENT_SOUP;
	*server = TRACKER_TYPE_HTTP_SERVER_SOUP;
}
