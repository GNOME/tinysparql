/*
 * Copyright (C) 2024, Divyansh Jain <divyanshjain.2206@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include "config.h"

#include "tracker-webide.h"

#include <tracker-http.h>


const gchar *get_request_mimetypes[] = {
	"text/html",
	"text/css",
	"text/javascript",
	"image/x-icon",
};

const gchar *get_request_file_extension[] = {
	"html",
	"css",
	"js",
	"ico",
};

G_STATIC_ASSERT (G_N_ELEMENTS (get_request_mimetypes) == G_N_ELEMENTS (get_request_file_extension));

struct _TrackerWebide {
	GObject parent_instance;
	TrackerHttpServer *server;
	GTlsCertificate *certificate;
	guint port;
	GCancellable *cancellable;
};

enum {
	PROP_0,
	PROP_HTTP_PORT,
	PROP_HTTP_CERTIFICATE,
	N_PROPS
};

static GParamSpec *props[N_PROPS];

static void tracker_webide_initable_iface_init (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (TrackerWebide,
                         tracker_webide,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                tracker_webide_initable_iface_init))

static const gchar *
get_mimetype_from_path (const gchar *path)
{
	gchar *extension;
	guint i;

	extension = strrchr (path, '.');
	if (extension == NULL)
		return NULL;

	extension++;

	for (i = 0; i < G_N_ELEMENTS (get_request_file_extension); i++) {
		if (g_strcmp0 (extension, get_request_file_extension[i]) == 0)
			return get_request_mimetypes[i];
	}

	return NULL;
}

static gboolean
tracker_get_request_validate_path (const gchar *path)
{
	const gchar *slash = g_strrstr (path + 1, "/");
	if (slash != NULL)
		return FALSE;

	const gchar *dot = g_strrstr (path, ".");
	if (dot == NULL)
		return FALSE;

	return TRUE;
}

static void
tracker_get_input_stream_from_path (const gchar   *path,
                                    GInputStream **in)
{
	GFile *file;
	GFile *base;
	GFileInputStream *file_in;

	base = g_file_new_for_uri ("resource:///org/freedesktop/tracker/web-ide/dist");
	file = g_file_get_child (base, &path[1]);

	file_in = g_file_read (file, NULL, NULL);
	*in = G_INPUT_STREAM (file_in);

	g_clear_object (&file);
	g_clear_object (&base);
}

static void
tracker_response_error_page_not_found (TrackerHttpServer    *server,
                                       TrackerHttpRequest   *request)
{
	GInputStream *in;

	tracker_get_input_stream_from_path ("/404.html", &in);
	tracker_http_server_error_content(server, request, 404, "text/html", G_INPUT_STREAM (in));
}

static void
webide_server_request_cb (TrackerHttpServer  *server,
                          GSocketAddress     *remote_address,
                          const gchar        *path,
                          const gchar        *method,
                          GHashTable         *params,
                          guint               formats,
                          TrackerHttpRequest *request,
                          gpointer            user_data)
{
	if (g_strcmp0 (method, "GET") != 0) {
		tracker_http_server_error (server, request, 405, "Method Not Allowed");
		return;
	}
	
	GInputStream *in;
	const gchar* mime_type;

	if (g_strcmp0 (path, "/") == 0) {
		webide_server_request_cb (server,
		                          remote_address,
		                          "/index.html",
		                          method,
		                          params,
		                          formats,
		                          request,
		                          user_data);
		return;
	}

	if (!tracker_get_request_validate_path (path)) {
		tracker_response_error_page_not_found (server, request);
		return;
	}

	tracker_get_input_stream_from_path (path, &in);

	if (!in) {
		g_debug ("File not found");
		tracker_response_error_page_not_found (server, request);
		return;
	}

	mime_type = get_mimetype_from_path (path);

	if (!mime_type) {
		tracker_response_error_page_not_found (server, request);
		return;
	}

	tracker_http_server_response (server, request, mime_type, G_INPUT_STREAM (in));
}

static gboolean
tracker_webide_initable_init (GInitable     *initable,
                              GCancellable  *cancellable,
                              GError       **error)
{
	TrackerWebide *webide = TRACKER_WEBIDE (initable);

	webide->server =
		tracker_http_server_new (webide->port,
		                         webide->certificate,
		                         TRACKER_HTTP_SERVER_MODE_WEB_IDE,
		                         cancellable,
		                         error);
	if (!webide->server)
		return FALSE;

	g_signal_connect (webide->server, "request",
	                  G_CALLBACK (webide_server_request_cb), initable);
	return TRUE;
}

static void
tracker_webide_initable_iface_init (GInitableIface *iface)
{
	iface->init = tracker_webide_initable_init;
}

static void
tracker_webide_finalize (GObject *object)
{
	TrackerWebide *webide = TRACKER_WEBIDE (object);

	g_cancellable_cancel (webide->cancellable);

	g_clear_object (&webide->cancellable);

	g_clear_object (&webide->server);

	G_OBJECT_CLASS (tracker_webide_parent_class)->finalize (object);
}

static void
tracker_webide_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
	TrackerWebide *webide = TRACKER_WEBIDE (object);

	switch (prop_id) {
	case PROP_HTTP_PORT:
		webide->port = g_value_get_uint (value);
		break;
	case PROP_HTTP_CERTIFICATE:
		webide->certificate = g_value_dup_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
tracker_webide_class_init (TrackerWebideClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_webide_finalize;
	object_class->set_property = tracker_webide_set_property;

	props[PROP_HTTP_PORT] =
		g_param_spec_uint ("http-port",
		                   "HTTP Port",
		                   "HTTP Port",
		                   0, G_MAXUINT,
		                   8080,
		                   G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);

	props[PROP_HTTP_CERTIFICATE] =
		g_param_spec_object ("http-certificate",
		                     "HTTP certificate",
		                     "HTTP certificate",
		                     G_TYPE_TLS_CERTIFICATE,
		                     G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);

	g_object_class_install_properties (object_class, N_PROPS, props);
}

static void
tracker_webide_init (TrackerWebide *webide)
{
	webide->cancellable = g_cancellable_new ();
}

TrackerWebide *
tracker_webide_new (guint             port,
                    GTlsCertificate  *certificate,
                    GCancellable     *cancellable,
                    GError          **error)
{
	g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), NULL);
	g_return_val_if_fail (!certificate || G_IS_TLS_CERTIFICATE (certificate), NULL);
	g_return_val_if_fail (!error || !*error, NULL);

	return g_initable_new (TRACKER_TYPE_WEBIDE, cancellable, error,
	                       "http-port", port,
	                       "http-certificate", certificate,
	                       NULL);
}
