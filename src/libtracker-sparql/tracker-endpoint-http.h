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

#ifndef TRACKER_ENDPOINT_HTTP_H
#define TRACKER_ENDPOINT_HTTP_H

#if !defined (__LIBTRACKER_SPARQL_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "only <libtracker-sparql/tracker-sparql.h> must be included directly."
#endif

#include <libtracker-sparql/tracker-endpoint.h>
#include <libtracker-sparql/tracker-version.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_ENDPOINT_HTTP         (tracker_endpoint_http_get_type())
#define TRACKER_ENDPOINT_HTTP(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_ENDPOINT_HTTP, TrackerEndpointHttp))
#define TRACKER_ENDPOINT_HTTP_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), TRACKER_TYPE_ENDPOINT_HTTP, TrackerEndpointHttpClass))
#define TRACKER_IS_ENDPOINT_HTTP(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_ENDPOINT_HTTP))
#define TRACKER_IS_ENDPOINT_HTTP_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),  TRACKER_TYPE_ENDPOINT_HTTP))
#define TRACKER_ENDPOINT_HTTP_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TRACKER_TYPE_ENDPOINT_HTTP, TrackerEndpointHttpClass))

/**
 * TrackerEndpointHttp:
 *
 * The <structname>TrackerEndpointHttp</structname> object represents a public
 * connection to a #TrackerSparqlConnection on a HTTP port.
 */
typedef struct _TrackerEndpointHttp TrackerEndpointHttp;

G_DEFINE_AUTOPTR_CLEANUP_FUNC (TrackerEndpointHttp, g_object_unref)

TRACKER_AVAILABLE_IN_3_1
GType tracker_endpoint_http_get_type (void) G_GNUC_CONST;

TRACKER_AVAILABLE_IN_3_1
TrackerEndpointHttp * tracker_endpoint_http_new (TrackerSparqlConnection  *sparql_connection,
                                                 guint                     port,
                                                 GTlsCertificate          *certificate,
                                                 GCancellable             *cancellable,
                                                 GError                  **error);

G_END_DECLS

#endif /* TRACKER_ENDPOINT_HTTP_H */
