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

#ifndef TRACKER_HTTP_H
#define TRACKER_HTTP_H

#include <gio/gio.h>

#ifndef MODULE
#include <libtracker-sparql/tracker-enums-private.h>
#endif

typedef struct _TrackerHttpRequest TrackerHttpRequest;

#define TRACKER_TYPE_HTTP_SERVER (tracker_http_server_get_type ())
G_DECLARE_DERIVABLE_TYPE (TrackerHttpServer,
                          tracker_http_server,
                          TRACKER, HTTP_SERVER,
                          GObject)

struct _TrackerHttpServerClass {
	GObjectClass parent_class;

	void (* response) (TrackerHttpServer       *server,
	                   TrackerHttpRequest      *request,
	                   TrackerSerializerFormat  format,
	                   GInputStream            *content);
	void (* error) (TrackerHttpServer  *server,
	                TrackerHttpRequest *request,
	                gint                code,
	                const gchar        *message);
};

TrackerHttpServer * tracker_http_server_new (guint             port,
                                             GTlsCertificate  *certificate,
                                             GCancellable     *cancellable,
                                             GError          **error);

void tracker_http_server_response (TrackerHttpServer       *server,
                                   TrackerHttpRequest      *request,
                                   TrackerSerializerFormat  format,
                                   GInputStream            *content);

void tracker_http_server_error (TrackerHttpServer       *server,
                                TrackerHttpRequest      *request,
                                gint                     code,
                                const gchar             *message);

#define TRACKER_TYPE_HTTP_CLIENT (tracker_http_client_get_type ())
G_DECLARE_DERIVABLE_TYPE (TrackerHttpClient,
                          tracker_http_client,
                          TRACKER, HTTP_CLIENT,
                          GObject)

struct _TrackerHttpClientClass {
	GObjectClass parent_class;

	void (* send_message_async) (TrackerHttpClient   *client,
	                             const gchar         *uri,
	                             const gchar         *query,
	                             guint                formats,
	                             GCancellable        *cancellable,
	                             GAsyncReadyCallback  callback,
	                             gpointer             user_data);
	GInputStream * (* send_message_finish) (TrackerHttpClient        *client,
	                                        GAsyncResult             *res,
	                                        TrackerSerializerFormat  *format,
	                                        GError                  **error);
	GInputStream * (* send_message) (TrackerHttpClient        *client,
	                                 const gchar              *uri,
	                                 const gchar              *query,
	                                 guint                     formats,
	                                 GCancellable             *cancellable,
	                                 TrackerSerializerFormat  *format,
	                                 GError                  **error);
};

TrackerHttpClient * tracker_http_client_new (void);

GInputStream *
tracker_http_client_send_message (TrackerHttpClient        *client,
                                  const gchar              *uri,
                                  const gchar              *query,
                                  guint                     formats,
                                  GCancellable             *cancellable,
                                  TrackerSerializerFormat  *format,
                                  GError                  **error);

void
tracker_http_client_send_message_async (TrackerHttpClient   *client,
                                        const gchar         *uri,
                                        const gchar         *query,
                                        guint                formats,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data);

GInputStream *
tracker_http_client_send_message_finish (TrackerHttpClient        *client,
                                         GAsyncResult             *res,
                                         TrackerSerializerFormat  *format,
                                         GError                  **error);

#endif /* TRACKER_HTTP_H */
