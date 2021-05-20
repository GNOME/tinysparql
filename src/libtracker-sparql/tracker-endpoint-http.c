/*
 * Copyright (C) 2020, Red Hat, Inc
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

#include "config.h"

#include "tracker-endpoint-http.h"
#include "tracker-serializer.h"
#include "tracker-private.h"

#include <libsoup/soup.h>

#define SERVER_HEADER "Tracker " PACKAGE_VERSION " (https://gitlab.gnome.org/GNOME/tracker/issues/)"

struct _TrackerEndpointHttp {
	TrackerEndpoint parent_instance;
	SoupServer *server;
	GTlsCertificate *certificate;
	guint port;
	GCancellable *cancellable;
};

typedef struct {
	TrackerEndpoint *endpoint;
#if SOUP_CHECK_VERSION (2, 99, 2)
        SoupServerMessage *message;
#else
	SoupMessage *message;
#endif
	GInputStream *istream;
	GTask *task;
	TrackerSerializerFormat format;
} Request;

enum {
	BLOCK_REMOTE_ADDRESS,
	N_SIGNALS
};

enum {
	PROP_0,
	PROP_HTTP_PORT,
	PROP_HTTP_CERTIFICATE,
	N_PROPS
};

#define XML_TYPE "application/sparql-results+xml"
#define JSON_TYPE "application/sparql-results+json"

static GParamSpec *props[N_PROPS];
static guint signals[N_SIGNALS];

static void tracker_endpoint_http_initable_iface_init (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (TrackerEndpointHttp, tracker_endpoint_http, TRACKER_TYPE_ENDPOINT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, tracker_endpoint_http_initable_iface_init))

static void
request_free (Request *request)
{
	g_clear_object (&request->istream);
	g_free (request);
}

static void
handle_request_in_thread (GTask        *task,
                          gpointer      source_object,
                          gpointer      task_data,
                          GCancellable *cancellable)
{
	Request *request = task_data;
	gchar *buffer[1000];
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
		if (count < 0) {
			g_task_return_error (task, error);
			break;
		}

		soup_message_body_append (message_body,
		                          SOUP_MEMORY_COPY,
		                          buffer, count);

		if ((gsize) count < sizeof (buffer)) {
			break;
		}
	}

	g_input_stream_close (request->istream, cancellable, NULL);
	soup_message_body_complete (message_body);
	g_task_return_boolean (task, TRUE);
}

static void
request_finished_cb (GObject      *object,
                     GAsyncResult *result,
                     gpointer      user_data)
{
	Request *request = user_data;
	TrackerEndpointHttp *endpoint_http;
	GError *error = NULL;

	endpoint_http = TRACKER_ENDPOINT_HTTP (request->endpoint);

	if (!g_task_propagate_boolean (G_TASK (result), &error)) {
#if SOUP_CHECK_VERSION (2, 99, 2)
                soup_server_message_set_status (request->message, 500,
                                                error ? error->message :
                                                "No error message");
#else
		soup_message_set_status_full (request->message, 500,
		                              error ? error->message :
		                              "No error message");
#endif
		g_clear_error (&error);
	} else {
#if SOUP_CHECK_VERSION (2, 99, 2)
                soup_server_message_set_status (request->message, 200, NULL);
#else
		soup_message_set_status (request->message, 200);
#endif
	}

	soup_server_unpause_message (endpoint_http->server, request->message);
	request_free (request);
}

static void
query_async_cb (GObject      *object,
                GAsyncResult *result,
                gpointer      user_data)
{
	TrackerEndpointHttp *endpoint_http;
	TrackerSparqlCursor *cursor;
	Request *request = user_data;
	GError *error = NULL;

	endpoint_http = TRACKER_ENDPOINT_HTTP (request->endpoint);
	cursor = tracker_sparql_connection_query_finish (TRACKER_SPARQL_CONNECTION (object),
	                                                 result, &error);
	if (error) {
#if SOUP_CHECK_VERSION (2, 99, 2)
                soup_server_message_set_status (request->message, 500, error->message);
#else
		soup_message_set_status_full (request->message, 500, error->message);
#endif
		soup_server_unpause_message (endpoint_http->server, request->message);
		request_free (request);
		return;
	}

	request->istream = tracker_serializer_new (cursor, request->format);
	request->task = g_task_new (endpoint_http, endpoint_http->cancellable,
	                            request_finished_cb, request);
	g_task_set_task_data (request->task, request, NULL);

	g_task_run_in_thread (request->task, handle_request_in_thread);
}

#if SOUP_CHECK_VERSION (2, 99, 2)
static gboolean
pick_format (SoupServerMessage       *message,
             TrackerSerializerFormat *format)
#else
static gboolean
pick_format (SoupMessage             *message,
             TrackerSerializerFormat *format)
#endif
{
	SoupMessageHeaders *request_headers, *response_headers;

#if SOUP_CHECK_VERSION (2, 99, 2)
        request_headers = soup_server_message_get_request_headers (message);
        response_headers = soup_server_message_get_response_headers (message);
#else
        request_headers = message->request_headers;
        response_headers = message->response_headers;
#endif

	if (soup_message_headers_header_contains (request_headers, "Accept", JSON_TYPE)) {
		soup_message_headers_set_content_type (response_headers, JSON_TYPE, NULL);
		*format = TRACKER_SERIALIZER_FORMAT_JSON;
		return TRUE;
	} else if (soup_message_headers_header_contains (request_headers, "Accept", XML_TYPE)) {
		soup_message_headers_set_content_type (response_headers, XML_TYPE, NULL);
		*format = TRACKER_SERIALIZER_FORMAT_XML;
		return TRUE;
	} else {
		return FALSE;
	}

	return FALSE;
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
	TrackerEndpoint *endpoint = user_data;
	TrackerSparqlConnection *conn;
	TrackerSerializerFormat format;
	GSocketAddress *remote_address;
	gboolean block = FALSE;
	const gchar *sparql;
	Request *request;

#if SOUP_CHECK_VERSION (2, 99, 2)
        remote_address = soup_server_message_get_remote_address (message);
#else
	remote_address = soup_client_context_get_remote_address (client);
#endif
	if (remote_address) {
		g_signal_emit (endpoint, signals[BLOCK_REMOTE_ADDRESS], 0,
		               remote_address, &block);
	}

	if (block) {
#if SOUP_CHECK_VERSION (2, 99, 2)
                soup_server_message_set_status (message, 500, "Remote address disallowed");
#else
		soup_message_set_status_full (message, 500, "Remote address disallowed");
#endif
		return;
	}

	sparql = g_hash_table_lookup (query, "query");
	if (!sparql) {
#if SOUP_CHECK_VERSION (2, 99, 2)
                soup_server_message_set_status (message, 500, "No query given");
#else
		soup_message_set_status_full (message, 500, "No query given");
#endif
		return;
	}

	if (!pick_format (message, &format)) {
#if SOUP_CHECK_VERSION (2, 99, 2)
                soup_server_message_set_status (message, 500, "No recognized accepted formats");
#else
		soup_message_set_status_full (message, 500, "No recognized accepted formats");
#endif
		return;
	}

	request = g_new0 (Request, 1);
	request->endpoint = endpoint;
	request->message = message;
	request->format = format;

	conn = tracker_endpoint_get_sparql_connection (endpoint);
	tracker_sparql_connection_query_async (conn,
	                                       sparql,
	                                       NULL,
	                                       query_async_cb,
	                                       request);

	soup_server_pause_message (server, message);
}

static gboolean
tracker_endpoint_http_initable_init (GInitable     *initable,
                                     GCancellable  *cancellable,
                                     GError       **error)
{
	TrackerEndpoint *endpoint = TRACKER_ENDPOINT (initable);
	TrackerEndpointHttp *endpoint_http = TRACKER_ENDPOINT_HTTP (endpoint);

	endpoint_http->server =
		soup_server_new ("tls-certificate", endpoint_http->certificate,
		                 "server-header", SERVER_HEADER,
		                 NULL);
	soup_server_add_handler (endpoint_http->server,
	                         "/sparql",
	                         server_callback,
	                         initable,
	                         NULL);

	return soup_server_listen_all (endpoint_http->server,
	                               endpoint_http->port,
	                               0, error);
}

static void
tracker_endpoint_http_initable_iface_init (GInitableIface *iface)
{
	iface->init = tracker_endpoint_http_initable_init;
}

static void
tracker_endpoint_http_finalize (GObject *object)
{
	TrackerEndpointHttp *endpoint_http = TRACKER_ENDPOINT_HTTP (object);

	g_cancellable_cancel (endpoint_http->cancellable);

	g_clear_object (&endpoint_http->cancellable);

	g_clear_object (&endpoint_http->server);

	G_OBJECT_CLASS (tracker_endpoint_http_parent_class)->finalize (object);
}

static void
tracker_endpoint_http_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
	TrackerEndpointHttp *endpoint_http = TRACKER_ENDPOINT_HTTP (object);

	switch (prop_id) {
	case PROP_HTTP_PORT:
		endpoint_http->port = g_value_get_uint (value);
		break;
	case PROP_HTTP_CERTIFICATE:
		endpoint_http->certificate = g_value_dup_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
tracker_endpoint_http_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
	TrackerEndpointHttp *endpoint_http = TRACKER_ENDPOINT_HTTP (object);

	switch (prop_id) {
	case PROP_HTTP_PORT:
		g_value_set_uint (value, endpoint_http->port);
		break;
	case PROP_HTTP_CERTIFICATE:
		g_value_set_object (value, endpoint_http->certificate);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
tracker_endpoint_http_class_init (TrackerEndpointHttpClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_endpoint_http_finalize;
	object_class->set_property = tracker_endpoint_http_set_property;
	object_class->get_property = tracker_endpoint_http_get_property;

	/**
	 * TrackerEndpointHttp::block-remote-address:
	 * @self: The #TrackerNotifier
	 * @address: The socket address of the remote connection
	 *
	 * Allows control over the connections stablished. The given
	 * address is that of the requesting peer.
	 *
	 * Returning %FALSE in this handler allows the connection,
	 * returning %TRUE blocks it. The default with no signal
	 * handlers connected is %FALSE.
	 */
	signals[BLOCK_REMOTE_ADDRESS] =
		g_signal_new ("block-remote-address",
		              TRACKER_TYPE_ENDPOINT_HTTP, 0, 0,
		              g_signal_accumulator_first_wins, NULL, NULL,
		              G_TYPE_BOOLEAN, 1, G_TYPE_SOCKET_ADDRESS);

	props[PROP_HTTP_PORT] =
		g_param_spec_uint ("http-port",
		                   "HTTP Port",
		                   "HTTP Port",
		                   0, G_MAXUINT,
		                   8080,
		                   G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
	props[PROP_HTTP_CERTIFICATE] =
		g_param_spec_object ("http-certificate",
		                     "HTTP certificate",
		                     "HTTP certificate",
		                     G_TYPE_TLS_CERTIFICATE,
		                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

	g_object_class_install_properties (object_class, N_PROPS, props);
}

static void
tracker_endpoint_http_init (TrackerEndpointHttp *endpoint)
{
	endpoint->cancellable = g_cancellable_new ();
}

/**
 * tracker_endpoint_http_new:
 * @sparql_connection: a #TrackerSparqlConnection
 * @port: HTTP port to listen to
 * @certificate: (nullable): certificate to use for encription, or %NULL
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @error: pointer to a #GError
 *
 * Sets up a Tracker endpoint to listen via HTTP, in the given @port.
 * If @certificate is not %NULL, HTTPS may be used to connect to the
 * endpoint.
 *
 * Returns: (transfer full): a #TrackerEndpointDBus object.
 *
 * Since: 3.1
 **/
TrackerEndpointHttp *
tracker_endpoint_http_new (TrackerSparqlConnection  *sparql_connection,
                           guint                     port,
                           GTlsCertificate          *certificate,
                           GCancellable             *cancellable,
                           GError                  **error)
{
	g_return_val_if_fail (TRACKER_IS_SPARQL_CONNECTION (sparql_connection), NULL);
	g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), NULL);
	g_return_val_if_fail (!certificate || G_IS_TLS_CERTIFICATE (certificate), NULL);
	g_return_val_if_fail (!error || !*error, NULL);

	return g_initable_new (TRACKER_TYPE_ENDPOINT_HTTP, cancellable, error,
	                       "http-port", port,
	                       "sparql-connection", sparql_connection,
	                       "http-certificate", certificate,
	                       NULL);
}
