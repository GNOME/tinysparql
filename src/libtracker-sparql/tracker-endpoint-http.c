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

/**
 * TrackerEndpointHttp:
 *
 * `TrackerEndpointHttp` makes the RDF data in a [class@SparqlConnection]
 * accessible to other hosts via HTTP.
 *
 * This object is a [class@Endpoint] subclass that exports
 * a [class@SparqlConnection] so its RDF data is accessible via HTTP
 * requests on the given port. This endpoint implementation is compliant
 * with the [SPARQL protocol specifications](https://www.w3.org/TR/2013/REC-sparql11-protocol-20130321/)
 * and may interoperate with other implementations.
 *
 * ```c
 * // This host has "example.local" hostname
 * endpoint = tracker_endpoint_http_new (sparql_connection,
 *                                       8080,
 *                                       tls_certificate,
 *                                       NULL,
 *                                       &error);
 *
 * // From another host
 * connection = tracker_sparql_connection_remote_new ("http://example.local:8080/sparql");
 * ```
 *
 * Access to HTTP endpoints may be managed via the
 * [signal@EndpointHttp::block-remote-address] signal, the boolean
 * return value expressing whether the connection is blocked or not.
 * Inspection of the requester address is left up to the user. The
 * default value allows all requests independently of their provenance,
 * users are encouraged to add a handler.
 *
 * If the provided [class@Gio.TlsCertificate] is %NULL, the endpoint will allow
 * plain HTTP connections. Users are encouraged to provide a certificate
 * in order to use HTTPS.
 *
 * As a security measure, and in compliance specifications,
 * the HTTP endpoint does not handle database updates or modifications in any
 * way. The database content is considered to be entirely managed by the
 * process that creates the HTTP endpoint and owns the [class@SparqlConnection].
 *
 * A `TrackerEndpointHttp` may be created on a different thread/main
 * context from the one that created [class@SparqlConnection].
 *
 * Since: 3.1
 */

#include "config.h"

#include "tracker-endpoint-http.h"

#include "tracker-deserializer-resource.h"
#include "tracker-serializer.h"
#include "tracker-private.h"

#include <tracker-http.h>

const gchar *supported_formats[] = {
	"http://www.w3.org/ns/formats/SPARQL_Results_JSON",
	"http://www.w3.org/ns/formats/SPARQL_Results_XML",
	"http://www.w3.org/ns/formats/Turtle",
	"http://www.w3.org/ns/formats/TriG",
	"http://www.w3.org/ns/formats/JSON-LD",
};

static const gchar *mimetypes[] = {
	"application/sparql-results+json",
	"application/sparql-results+xml",
	"text/turtle",
	"application/trig",
	"application/ld+json",
};


G_STATIC_ASSERT (G_N_ELEMENTS (supported_formats) == TRACKER_N_SERIALIZER_FORMATS);

struct _TrackerEndpointHttp {
	TrackerEndpoint parent_instance;
	TrackerHttpServer *server;
	GTlsCertificate *certificate;
	guint port;
	GCancellable *cancellable;
};

typedef struct {
	TrackerEndpoint *endpoint;
	TrackerHttpRequest *request;
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
query_async_cb (GObject      *object,
                GAsyncResult *result,
                gpointer      user_data)
{
	TrackerEndpointHttp *endpoint_http;
	TrackerSparqlCursor *cursor;
	TrackerSparqlConnection *conn;
	Request *request = user_data;
	GInputStream *stream;
	GError *error = NULL;

	endpoint_http = TRACKER_ENDPOINT_HTTP (request->endpoint);
	cursor = tracker_sparql_connection_query_finish (TRACKER_SPARQL_CONNECTION (object),
	                                                 result, &error);
	if (error) {
		tracker_http_server_error (endpoint_http->server,
		                           request->request,
		                           400,
		                           error->message);
		request_free (request);
		g_error_free (error);
		return;
	}

	conn = tracker_sparql_cursor_get_connection (cursor);
	stream = tracker_serializer_new (cursor,
	                                 tracker_sparql_connection_get_namespace_manager (conn),
	                                 request->format);
	/* Consumes the input stream */
	tracker_http_server_response (endpoint_http->server,
	                              request->request,
	                              mimetypes[request->format],
	                              stream);
	request_free (request);
	g_object_unref (cursor);
}

static gboolean
pick_format (guint                    formats,
             TrackerSerializerFormat *format)
{
	TrackerSerializerFormat i;
	const gchar *test_format;

	test_format = g_getenv ("TRACKER_TEST_PREFERRED_CURSOR_FORMAT");
	if (test_format && g_ascii_isdigit (*test_format)) {
		int f = atoi (test_format);

		if ((formats & (1 << f)) != 0) {
			*format = f;
			return TRUE;
		}
	}

	for (i = 0; i < TRACKER_N_SERIALIZER_FORMATS; i++) {
		if ((formats & (1 << i)) != 0) {
			*format = i;
			return TRUE;
		}
	}

	return FALSE;
}

static void
add_supported_formats (TrackerResource *resource,
                       const gchar     *property)
{
	gint i;

	for (i = 0; i < TRACKER_N_SERIALIZER_FORMATS; i++)
		tracker_resource_add_uri (resource, property, supported_formats[i]);
}

static TrackerResource *
create_service_description (TrackerEndpointHttp      *endpoint,
                            TrackerNamespaceManager **manager)
{
	TrackerResource *resource;

	*manager = tracker_namespace_manager_new ();
	tracker_namespace_manager_add_prefix (*manager, "rdf",
	                                      "http://www.w3.org/1999/02/22-rdf-syntax-ns#");
	tracker_namespace_manager_add_prefix (*manager, "sd",
	                                      "http://www.w3.org/ns/sparql-service-description#");
	tracker_namespace_manager_add_prefix (*manager, "format",
	                                      "http://www.w3.org/ns/formats/");

	resource = tracker_resource_new (NULL);
	tracker_resource_set_uri (resource, "rdf:type", "sd:Service");
	/* tracker_resource_set_uri (resource, "sd:endpoint", endpoint_uri); */
	tracker_resource_set_uri (resource, "sd:supportedLanguage", "sd:SPARQL11Query");

	tracker_resource_add_uri (resource, "sd:feature", "sd:EmptyGraphs");
	tracker_resource_add_uri (resource, "sd:feature", "sd:BasicFederatedQuery");
	tracker_resource_add_uri (resource, "sd:feature", "sd:UnionDefaultGraph");

	add_supported_formats (resource, "sd:resultFormat");
	add_supported_formats (resource, "sd:inputFormat");

	return resource;
}

static void
sparql_server_request_cb (TrackerHttpServer  *server,
                          GSocketAddress     *remote_address,
                          const gchar        *path,
                          const gchar        *method,
                          GHashTable         *params,
                          guint               formats,
                          TrackerHttpRequest *request,
                          gpointer            user_data)
{
	TrackerEndpoint *endpoint = user_data;
	TrackerSparqlConnection *conn;
	TrackerSerializerFormat format;
	const gchar *sparql = NULL;
	Request *data;
	gboolean block = FALSE;

	if (remote_address) {
		g_signal_emit (endpoint, signals[BLOCK_REMOTE_ADDRESS], 0,
		               remote_address, &block);
	}

	if (block) {
		tracker_http_server_error (server, request, 400,
		                           "Remote address disallowed");
		return;
	}

	if (params)
		sparql = g_hash_table_lookup (params, "query");

	if (sparql) {
		gchar *query;

		if (!pick_format (formats, &format)) {
			tracker_http_server_error (server, request, 400,
			                           "No recognized accepted formats");
			return;
		}

		data = g_new0 (Request, 1);
		data->endpoint = endpoint;
		data->request = request;
		data->format = format;

		query = g_strdup (sparql);
		tracker_endpoint_rewrite_query (TRACKER_ENDPOINT (endpoint), &query);

		conn = tracker_endpoint_get_sparql_connection (endpoint);
		tracker_sparql_connection_query_async (conn,
		                                       query,
		                                       NULL,
		                                       query_async_cb,
		                                       data);
		g_free (query);
	} else {
		TrackerNamespaceManager *namespaces;
		TrackerResource *description;
		TrackerSparqlCursor *deserializer;
		GInputStream *serializer;

		if (!pick_format (formats, &format))
			format = TRACKER_SERIALIZER_FORMAT_TTL;

		/* Requests to with no query return a RDF description
		 * about the HTTP endpoint.
		 */
		description = create_service_description (TRACKER_ENDPOINT_HTTP (endpoint),
		                                          &namespaces);

		deserializer = tracker_deserializer_resource_new (description,
		                                                  namespaces, NULL);
		serializer = tracker_serializer_new (TRACKER_SPARQL_CURSOR (deserializer),
		                                     namespaces, format);
		g_object_unref (deserializer);
		g_object_unref (description);
		g_object_unref (namespaces);

		/* Consumes the serializer */
		tracker_http_server_response (server,
		                              request,
		                              mimetypes[format],
		                              serializer);
	}
}

static gboolean
tracker_endpoint_http_initable_init (GInitable     *initable,
                                     GCancellable  *cancellable,
                                     GError       **error)
{
	TrackerEndpoint *endpoint = TRACKER_ENDPOINT (initable);
	TrackerEndpointHttp *endpoint_http = TRACKER_ENDPOINT_HTTP (endpoint);

	endpoint_http->server =
		tracker_http_server_new (endpoint_http->port,
		                         endpoint_http->certificate,
		                         TRACKER_HTTP_SERVER_MODE_SPARQL_ENDPOINT,
		                         cancellable,
		                         error);
	if (!endpoint_http->server)
		return FALSE;

	g_signal_connect (endpoint_http->server, "request",
	                  G_CALLBACK (sparql_server_request_cb), initable);
	return TRUE;
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
	 * @self: The `TrackerEndpointHttp`
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

	/**
	 * TrackerEndpointHttp:http-port:
	 *
	 * HTTP port used to listen requests.
	 */
	props[PROP_HTTP_PORT] =
		g_param_spec_uint ("http-port",
		                   "HTTP Port",
		                   "HTTP Port",
		                   0, G_MAXUINT,
		                   8080,
		                   G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
	/**
	 * TrackerEndpointHttp:http-certificate:
	 *
	 * [class@Gio.TlsCertificate] to encrypt the communication.
	 */
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
 * @sparql_connection: The [class@SparqlConnection] being made public
 * @port: HTTP port to handle incoming requests
 * @certificate: (nullable): Optional [type@Gio.TlsCertificate] to use for encription
 * @cancellable: (nullable): Optional [type@Gio.Cancellable]
 * @error: Error location
 *
 * Sets up a Tracker endpoint to listen via HTTP, in the given @port.
 * If @certificate is not %NULL, HTTPS may be used to connect to the
 * endpoint.
 *
 * Returns: (transfer full): a `TrackerEndpointHttp` object.
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
	                       "readonly", TRUE,
	                       "http-port", port,
	                       "sparql-connection", sparql_connection,
	                       "http-certificate", certificate,
	                       NULL);
}
