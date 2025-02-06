/*
 * Copyright (C) 2019, Red Hat, Inc
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

#pragma once

#if !defined (__LIBTRACKER_SPARQL_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "only <tinysparql.h> must be included directly."
#endif

#include <glib-object.h>
#include <tracker-connection.h>
#include <tracker-version.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_ENDPOINT tracker_endpoint_get_type()
TRACKER_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (TrackerEndpoint, tracker_endpoint, TRACKER, ENDPOINT, GObject)

TRACKER_AVAILABLE_IN_ALL
TrackerSparqlConnection * tracker_endpoint_get_sparql_connection (TrackerEndpoint *endpoint);

TRACKER_AVAILABLE_IN_3_7
void  tracker_endpoint_set_readonly (TrackerEndpoint *endpoint,
                                     gboolean         readonly);

TRACKER_AVAILABLE_IN_3_7
gboolean  tracker_endpoint_get_readonly (TrackerEndpoint *endpoint);

TRACKER_AVAILABLE_IN_3_7
void tracker_endpoint_set_allowed_services (TrackerEndpoint     *endpoint,
                                            const gchar * const *services);

TRACKER_AVAILABLE_IN_3_7
GStrv tracker_endpoint_get_allowed_services (TrackerEndpoint *endpoint);

TRACKER_AVAILABLE_IN_3_7
void tracker_endpoint_set_allowed_graphs (TrackerEndpoint     *endpoint,
                                          const gchar * const *graphs);

TRACKER_AVAILABLE_IN_3_7
GStrv tracker_endpoint_get_allowed_graphs (TrackerEndpoint *endpoint);

G_END_DECLS
