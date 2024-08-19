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

#include "config.h"

#include <gio/gio.h>
#include <dlfcn.h>

#include "tracker-http.h"

static GType client_type = G_TYPE_NONE;
static GType server_type = G_TYPE_NONE;

/* Module loading */
#define LIBSOUP_2_SONAME "libsoup-2.4.so.1"

static void
ensure_types (void)
{
	const char *modules[3] = { 0 };
	gint i = 0;

	if (client_type != G_TYPE_NONE)
		return;

	g_assert (g_module_supported ());

	modules[0] = "libtracker-http-soup3.so";

	for (i = 0; modules[i]; i++) {
		GModule *remote_module;
		gchar *module_path;
		void (* init_func) (GType *client, GType *server);
		gchar *current_dir;

		current_dir = g_get_current_dir ();

		if (g_strcmp0 (current_dir, BUILDROOT) == 0) {
			/* Detect in-build runtime of this code, this may happen
			 * building introspection information or running tests.
			 * We want the in-tree modules to be loaded then.
			 */
			module_path = g_strdup_printf (BUILD_LIBDIR "/%s", modules[i]);
		} else {
			module_path = g_strdup_printf (PRIVATE_LIBDIR "/%s", modules[i]);
		}

		g_free (current_dir);

		if (!g_file_test (module_path, G_FILE_TEST_EXISTS)) {
			g_free (module_path);
			continue;
		}

		remote_module = g_module_open (module_path,
		                               G_MODULE_BIND_LAZY |
		                               G_MODULE_BIND_LOCAL);
		g_free (module_path);

		if (!remote_module) {
			g_printerr ("Could not load '%s': %s\n",
			            modules[i], g_module_error ());
			continue;
		}

		if (!g_module_symbol (remote_module, "initialize_types", (gpointer *) &init_func)) {
			g_printerr ("Could find init function: %s\n",
			            g_module_error ());
			g_clear_pointer (&remote_module, g_module_close);
			continue;
		}

		g_type_ensure (TRACKER_TYPE_HTTP_CLIENT);
		g_type_ensure (TRACKER_TYPE_HTTP_SERVER);

		init_func (&client_type, &server_type);

		g_module_make_resident (remote_module);
		g_module_close (remote_module);

		g_assert (client_type != G_TYPE_NONE);
		g_assert (server_type != G_TYPE_NONE);
		return;
	}

	g_assert_not_reached ();
}

/* HTTP server */
enum {
	PROP_0,
	PROP_HTTP_PORT,
	PROP_HTTP_CERTIFICATE,
	PROP_SERVER_MODE,
	N_SERVER_PROPS
};

enum {
	REQUEST,
	N_SERVER_SIGNALS,
};

typedef struct
{
	guint port;
	GTlsCertificate *certificate;
	TrackerHttpServerMode server_mode;
} TrackerHttpServerPrivate;

static GParamSpec *server_props[N_SERVER_PROPS] = { 0 };
static guint server_signals[N_SERVER_SIGNALS] = { 0 };

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (TrackerHttpServer,
                                     tracker_http_server,
                                     G_TYPE_OBJECT)

static void
tracker_http_server_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
	TrackerHttpServer *server = TRACKER_HTTP_SERVER (object);
	TrackerHttpServerPrivate *priv =
		tracker_http_server_get_instance_private (server);

	switch (prop_id) {
	case PROP_HTTP_PORT:
		priv->port = g_value_get_uint (value);
		break;
	case PROP_HTTP_CERTIFICATE:
		priv->certificate = g_value_dup_object (value);
		break;
	case PROP_SERVER_MODE:
		priv->server_mode = g_value_get_uint (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
tracker_http_server_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
	TrackerHttpServer *server = TRACKER_HTTP_SERVER (object);
	TrackerHttpServerPrivate *priv =
		tracker_http_server_get_instance_private (server);

	switch (prop_id) {
	case PROP_HTTP_PORT:
		g_value_set_uint (value, priv->port);
		break;
	case PROP_HTTP_CERTIFICATE:
		g_value_set_object (value, priv->certificate);
		break;
	case PROP_SERVER_MODE:
		g_value_set_uint (value, priv->server_mode);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
tracker_http_server_class_init (TrackerHttpServerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = tracker_http_server_set_property;
	object_class->get_property = tracker_http_server_get_property;

	server_signals[REQUEST] =
		g_signal_new ("request",
		              TRACKER_TYPE_HTTP_SERVER, 0, 0,
		              NULL, NULL, NULL,
		              G_TYPE_NONE, 6,
		              G_TYPE_SOCKET_ADDRESS,
		              G_TYPE_STRING,
		              G_TYPE_STRING,
		              G_TYPE_HASH_TABLE,
		              G_TYPE_UINT,
		              G_TYPE_POINTER);

	server_props[PROP_HTTP_PORT] =
		g_param_spec_uint ("http-port",
		                   "HTTP Port",
		                   "HTTP Port",
		                   0, G_MAXUINT,
		                   8080,
		                   G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
	server_props[PROP_HTTP_CERTIFICATE] =
		g_param_spec_object ("http-certificate",
		                     "HTTP certificate",
		                     "HTTP certificate",
		                     G_TYPE_TLS_CERTIFICATE,
		                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
	server_props[PROP_SERVER_MODE] =
		g_param_spec_uint ("server-mode",
		                   "Server Mode",
		                   "Server Mode",
		                   TRACKER_HTTP_SERVER_MODE_SPARQL_ENDPOINT,
		                   TRACKER_N_HTTP_SERVER_MODES,
		                   TRACKER_HTTP_SERVER_MODE_SPARQL_ENDPOINT,
		                   G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
	g_object_class_install_properties (object_class,
	                                   N_SERVER_PROPS,
	                                   server_props);
}

static void
tracker_http_server_init (TrackerHttpServer *server)
{
}

TrackerHttpServer *
tracker_http_server_new (guint                   port,
                         GTlsCertificate        *certificate,
                         TrackerHttpServerMode   server_mode,
                         GCancellable           *cancellable,
                         GError                **error)
{
	ensure_types ();

	return g_initable_new (server_type,
	                       cancellable, error,
	                       "http-port", port,
	                       "http-certificate", certificate,
	                       "server-mode", server_mode,
	                       NULL);
}

void
tracker_http_server_response (TrackerHttpServer       *server,
                              TrackerHttpRequest      *request,
                              const gchar*             mimetype,
                              GInputStream            *content)
{
	TRACKER_HTTP_SERVER_GET_CLASS (server)->response (server,
	                                                  request,
	                                                  mimetype,
	                                                  content);
}

void
tracker_http_server_error (TrackerHttpServer  *server,
                           TrackerHttpRequest *request,
                           gint                code,
                           const gchar        *message)
{
	TRACKER_HTTP_SERVER_GET_CLASS (server)->error (server,
	                                               request,
	                                               code,
	                                               message);
}

void 
tracker_http_server_error_content (TrackerHttpServer  *server,
                                  TrackerHttpRequest  *request,
                                  gint                 code,
                                  const gchar*         mimetype,
                                  GInputStream        *content)
{
	TRACKER_HTTP_SERVER_GET_CLASS (server)->error_content (server,
	                                                       request,
	                                                       code,
	                                                       mimetype,
	                                                       content);
}

/* HTTP client */
G_DEFINE_ABSTRACT_TYPE (TrackerHttpClient, tracker_http_client, G_TYPE_OBJECT)

static void
tracker_http_client_class_init (TrackerHttpClientClass *klass)
{
}

static void
tracker_http_client_init (TrackerHttpClient *server)
{
}

TrackerHttpClient *
tracker_http_client_new (void)
{
	ensure_types ();

	return g_object_new (client_type, NULL);
}

void
tracker_http_client_send_message_async (TrackerHttpClient   *client,
                                        const gchar         *uri,
                                        const gchar         *query,
                                        guint                formats,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
	TRACKER_HTTP_CLIENT_GET_CLASS (client)->send_message_async (client,
	                                                            uri,
	                                                            query,
	                                                            formats,
	                                                            cancellable,
	                                                            callback,
	                                                            user_data);
}

GInputStream *
tracker_http_client_send_message_finish (TrackerHttpClient        *client,
                                         GAsyncResult             *res,
                                         TrackerSerializerFormat  *format,
                                         GError                  **error)
{
	return TRACKER_HTTP_CLIENT_GET_CLASS (client)->send_message_finish (client,
	                                                                    res,
	                                                                    format,
	                                                                    error);
}

GInputStream *
tracker_http_client_send_message (TrackerHttpClient        *client,
                                  const gchar              *uri,
                                  const gchar              *query,
                                  guint                     formats,
                                  GCancellable             *cancellable,
                                  TrackerSerializerFormat  *format,
                                  GError                  **error)
{
	return TRACKER_HTTP_CLIENT_GET_CLASS (client)->send_message (client,
	                                                             uri,
	                                                             query,
	                                                             formats,
	                                                             cancellable,
	                                                             format,
	                                                             error);
}
