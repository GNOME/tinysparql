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

#define BUFFER_SIZE 4096

#ifdef G_ENABLE_DEBUG
static const GDebugKey tracker_debug_keys[] = {
  { "http", TRACKER_DEBUG_HTTP },
};
#endif /* G_ENABLE_DEBUG */

static gpointer
parse_debug_flags ()
{
	const gchar *env_string;
	guint flags = 0;

	env_string = g_getenv ("TINYSPARQL_DEBUG");
	if (env_string == NULL)
		env_string = g_getenv ("TRACKER_DEBUG");

	if (env_string != NULL) {
#ifdef G_ENABLE_DEBUG
		flags = g_parse_debug_string (env_string, tracker_debug_keys, G_N_ELEMENTS (tracker_debug_keys));
#else
		g_warning ("TINYSPARQL_DEBUG set but ignored because tracker isn't built with G_ENABLE_DEBUG");
#endif  /* G_ENABLE_DEBUG */
		env_string = NULL;
	}

	return GINT_TO_POINTER (flags);
}

guint
tracker_get_debug_flags (void)
{
	static GOnce once = G_ONCE_INIT;

	g_once (&once, parse_debug_flags, NULL);

	return GPOINTER_TO_INT (once.retval);
}

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
#define USER_AGENT "TinySPARQL " PACKAGE_VERSION " (https://gitlab.gnome.org/GNOME/tinysparql/issues/)"

/* Server */
struct _TrackerHttpRequest
{
	TrackerHttpServer *server;
	SoupServerMessage *message;
	GInputStream *istream;

	GSocketAddress *remote_address;
	gchar *path;
	GHashTable *params;
	GCancellable *cancellable;
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
             SoupServerMessage  *message,
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
	request->cancellable = g_cancellable_new ();

	if (params) {
		request->params = g_hash_table_ref (params);
	}

	return request;
}

static void
request_free (TrackerHttpRequest *request)
{
	g_signal_handlers_disconnect_by_data (request->message, request);

	g_clear_object (&request->istream);
	g_clear_object (&request->message);
	g_clear_object (&request->remote_address);
	g_object_unref (request->cancellable);
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

	request_headers = soup_server_message_get_request_headers (request->message);

	for (i = 0; i < TRACKER_N_SERIALIZER_FORMATS; i++) {
		if (soup_message_headers_header_contains (request_headers, "Accept", mimetypes[i]))
			formats |= 1 << i;
	}

	return formats;
}

static void
set_message_format (TrackerHttpRequest      *request,
                    const gchar*             mimetype)
{
	SoupMessageHeaders *response_headers;

	response_headers = soup_server_message_get_response_headers (request->message);
	soup_message_headers_set_content_type (response_headers, mimetype, NULL);
	soup_message_headers_append (response_headers, "Access-Control-Allow-Origin", "*");
}

/* Get SPARQL query from message POST data, or NULL. */
static gchar *
get_sparql_from_message_body (SoupServerMessage *message)
{
	SoupMessageBody *body;
	GBytes *body_bytes;
	const gchar *body_data;
	gsize body_size;
	gchar *sparql = NULL;

	body = soup_server_message_get_request_body (message);
	body_bytes = soup_message_body_flatten (body);

	body_data = g_bytes_get_data (body_bytes, &body_size);

	if (g_utf8_validate_len (body_data, body_size, NULL)) {
		sparql = g_malloc (body_size + 1);
		g_utf8_strncpy (sparql, body_data, body_size);
		sparql[body_size] = 0;
	}

	g_bytes_unref (body_bytes);

	return sparql;
}

/* Handle POST messages if the body wasn't immediately available. */
static void
server_callback_got_message_body (SoupServerMessage *message,
                                  gpointer           user_data)
{
	TrackerHttpRequest *request = user_data;
	gchar *sparql;
	const char *method;

	sparql = get_sparql_from_message_body (message);

#if SOUP_CHECK_VERSION (3, 1, 3)
	method = soup_server_message_get_method (message);
#else
	method = message->method;
#endif

	if (sparql) {
		if (!request->params) {
			request->params = g_hash_table_new (g_str_hash, g_str_equal);
		}

		g_hash_table_insert (request->params, "query", sparql);

		g_signal_emit_by_name (request->server, "request",
		                       request->remote_address,
		                       request->path,
		                       method,
		                       request->params,
		                       get_supported_formats (request),
		                       request);
	} else {
		tracker_http_server_soup_error (request->server, request, 400, "Missing query or invalid UTF-8 in POST request");
	}

	g_free (sparql);
}

static void
debug_http_reponse_response ()
{
	g_message ("Response sent successfully\n");
	g_print ("--------------------------------------------------------------------------\n");
}

static void
debug_http_response_error (gint         code,
                           const gchar *message)
{
	g_message ("Response error %d: %s\n", code, message);
	g_print ("--------------------------------------------------------------------------\n");
}

void print_parameter_entry (gpointer key,
                            gpointer value,
                            gpointer user_data)
{
	g_print ("%s : %s\n", (char *)key, (char *)value);
}

static void
debug_http_request (SoupServerMessage *message,
                    const char        *path,
                    GHashTable        *query)
{
	SoupMessageHeadersIter iter;
	SoupMessageBody *request_body;
	const char *name, *value;

	g_print ("--------------------------------------------------------------------------\n");

	g_message ("%s %s HTTP/1.%d\n", soup_server_message_get_method (message), path,
		soup_server_message_get_http_version (message));

	if (query != NULL && g_hash_table_size(query)) {
		g_message ("Query Parameters:\n");
		g_hash_table_foreach (query, print_parameter_entry, NULL);
	} else {
		g_message ("Query Parameters: (empty)\n");
	}

	soup_message_headers_iter_init (&iter, soup_server_message_get_request_headers (message));
	g_message ("Request Headers:\n");
	while (soup_message_headers_iter_next (&iter, &name, &value))
		g_print ("%s: %s\n", name, value);

	request_body = soup_server_message_get_request_body (message);
	if (request_body->length) {
		g_message ("Request Body:\n");
		g_print ("%s\n", request_body->data);
	} else {
		g_message ("Request Body: (empty)\n");
	}
}


static void
webide_server_callback (SoupServer        *server,
                        SoupServerMessage *message,
                        const char        *path,
                        GHashTable        *query,
                        gpointer           user_data)
{
	TrackerHttpServer *http_server = user_data;
	GSocketAddress *remote_address;
	TrackerHttpRequest *request;
	const char *method;
	SoupMessageHeaders *response_headers;
	SoupMessageBody *body;

	TRACKER_NOTE (HTTP, debug_http_request (message, path, query));

	remote_address = soup_server_message_get_remote_address (message);

	request = request_new (http_server, message, remote_address, path, query);

	response_headers = soup_server_message_get_response_headers (request->message);
	soup_message_headers_set_encoding (response_headers, SOUP_ENCODING_CHUNKED);

	body = soup_server_message_get_response_body (request->message);
	soup_message_body_set_accumulate (body, FALSE);

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

	g_signal_emit_by_name (http_server, "request",
	                       remote_address,
	                       path,
	                       method,
	                       query,
	                       get_supported_formats (request),
	                       request);
}

static void
sparql_server_callback (SoupServer        *server,
                        SoupServerMessage *message,
                        const char        *path,
                        GHashTable        *query,
                        gpointer           user_data)
{
	TrackerHttpServer *http_server = user_data;
	GSocketAddress *remote_address;
	TrackerHttpRequest *request;
	const char *method;
	SoupMessageHeaders *response_headers;
	SoupMessageBody *body;

	TRACKER_NOTE (HTTP, debug_http_request (message, path, query));

	remote_address = soup_server_message_get_remote_address (message);

	request = request_new (http_server, message, remote_address, path, query);

	response_headers = soup_server_message_get_response_headers (request->message);
	soup_message_headers_set_encoding (response_headers, SOUP_ENCODING_CHUNKED);

	body = soup_server_message_get_response_body (request->message);
	soup_message_body_set_accumulate (body, FALSE);

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
		body = soup_server_message_get_request_body (request->message);

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
		                       method,
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
	TrackerHttpServerMode server_mode;

	g_object_get (initable,
	              "http-certificate", &certificate,
	              "http-port", &port,
	              "server-mode", &server_mode,
	              NULL);

	server->server =
		soup_server_new ("tls-certificate", certificate,
		                 "server-header", USER_AGENT,
		                 NULL);

	if (server_mode == TRACKER_HTTP_SERVER_MODE_SPARQL_ENDPOINT) {
		soup_server_add_handler (server->server,
		                         "/sparql",
		                         sparql_server_callback,
		                         initable,
		                         NULL);
	} else if (server_mode == TRACKER_HTTP_SERVER_MODE_WEB_IDE) {
		soup_server_add_handler (server->server,
		                         "/",
		                         webide_server_callback,
		                         initable,
		                         NULL);
	} else {
		g_set_error (error,
		             G_IO_ERROR,
		             G_IO_ERROR_FAILED,
		             "Unhandled server mode %d",
		             server_mode);
		return FALSE;
	}

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

	TRACKER_NOTE (HTTP, debug_http_response_error (code, message));

	soup_server_message_set_status (request->message, code, message);
	SoupMessageHeaders *response_headers = soup_server_message_get_response_headers (request->message);
	soup_message_headers_append (response_headers, "Access-Control-Allow-Origin", "http://localhost:8080");

#if SOUP_CHECK_VERSION (3, 1, 3)
	soup_server_message_unpause (request->message);
#else
	soup_server_unpause_message (server_soup->server, request->message);
#endif
	request_free (request);
}

static void
on_bytes_read (GObject      *source,
               GAsyncResult *res,
               gpointer      user_data)
{
	TrackerHttpRequest *request = user_data;
	SoupMessageBody *message_body;
	GBytes *bytes;
	GError *error = NULL;

	bytes = g_input_stream_read_bytes_finish (G_INPUT_STREAM (source),
	                                          res, &error);
	if (error) {
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
			tracker_http_server_soup_error (request->server,
			                                request,
			                                500,
			                                error->message);
		}

		g_error_free (error);
		return;
	}

	message_body = soup_server_message_get_response_body (request->message);

	if (g_bytes_get_size (bytes) > 0) {
		soup_message_body_append_bytes (message_body, bytes);

#if SOUP_CHECK_VERSION (3, 1, 3)
		soup_server_message_unpause (request->message);
#else
		soup_server_unpause_message (server->server, request->message);
#endif
	} else {
		g_input_stream_close (request->istream, request->cancellable, NULL);
		soup_message_body_complete (message_body);

#if SOUP_CHECK_VERSION (3, 1, 3)
		soup_server_message_unpause (request->message);
#else
		soup_server_unpause_message (server->server, request->message);
#endif

		request_free (request);
	}

	g_bytes_unref (bytes);
}

static void
next_write (TrackerHttpRequest *request)
{
	g_input_stream_read_bytes_async (request->istream,
	                                 BUFFER_SIZE,
	                                 G_PRIORITY_DEFAULT,
	                                 request->cancellable,
	                                 on_bytes_read,
	                                 request);
}

static void
on_message_finished (SoupServerMessage  *message,
                     TrackerHttpRequest *request)
{
	g_cancellable_cancel (request->cancellable);
	request_free (request);
}

static void
on_chunk_written (SoupServerMessage  *message,
                  TrackerHttpRequest *request)
{
	next_write (request);
}

static void
tracker_http_server_soup_response (TrackerHttpServer       *server,
                                   TrackerHttpRequest      *request,
                                   const gchar*             mimetype,
                                   GInputStream            *content)
{
	g_assert (request->server == server);

	TRACKER_NOTE (HTTP, debug_http_reponse_response ());

	set_message_format (request, mimetype);
	soup_server_message_set_status (request->message, 200, NULL);
	request->istream = content;

	g_signal_connect (request->message, "finished", G_CALLBACK (on_message_finished), request);
	g_signal_connect (request->message, "wrote-chunk", G_CALLBACK (on_chunk_written), request);
	next_write (request);
}

static void
tracker_http_server_soup_error_content (TrackerHttpServer       *server,
                                        TrackerHttpRequest      *request,
                                        gint                    code,
                                        const gchar*            mimetype,
                                        GInputStream            *content)
{
	g_assert (request->server == server);

	set_message_format (request, mimetype);
	soup_server_message_set_status (request->message, code, NULL);
	request->istream = content;

	g_signal_connect (request->message, "finished", G_CALLBACK (on_message_finished), request);
	g_signal_connect (request->message, "wrote-chunk", G_CALLBACK (on_chunk_written), request);
	next_write (request);
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
	server_class->error_content = tracker_http_server_soup_error_content;
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

	status_code = soup_message_get_status (message);
	response_headers = soup_message_get_response_headers (message);

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
	GBytes *query_encoded;

	message = soup_message_new ("POST", uri);

	headers = soup_message_get_request_headers (message);

	soup_message_headers_append (headers, "User-Agent", USER_AGENT);
	add_accepted_formats (headers, formats);

	query_encoded = g_bytes_new (query, strlen (query));
	soup_message_set_request_body_from_bytes (message, "application/sparql-query", query_encoded);
	g_bytes_unref (query_encoded);

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

	soup_session_send_async (client_soup->session,
	                         message,
	                         G_PRIORITY_DEFAULT,
	                         cancellable,
	                         send_message_cb,
	                         task);
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

	stream = soup_session_send (client_soup->session,
	                            message,
	                            cancellable,
	                            error);
	if (!stream)
		goto out;

	if (!get_content_type_format (message, format, error)) {
		g_clear_object (&stream);
		goto out;
	}

 out:
	g_clear_object (&message);

	return stream;
}

static void
tracker_http_client_soup_finalize (GObject *object)
{
	TrackerHttpClientSoup *client = TRACKER_HTTP_CLIENT_SOUP (object);

	g_clear_object (&client->session);

	G_OBJECT_CLASS (tracker_http_client_soup_parent_class)->finalize (object);
}

static void
tracker_http_client_soup_class_init (TrackerHttpClientSoupClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	TrackerHttpClientClass *client_class =
		TRACKER_HTTP_CLIENT_CLASS (klass);

	object_class->finalize = tracker_http_client_soup_finalize;

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
