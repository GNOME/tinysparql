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

#include <tracker-endpoint.h>
#include <tracker-version.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_ENDPOINT_DBUS         (tracker_endpoint_dbus_get_type())
#define TRACKER_ENDPOINT_DBUS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_ENDPOINT_DBUS, TrackerEndpointDBus))
#define TRACKER_ENDPOINT_DBUS_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), TRACKER_TYPE_ENDPOINT_DBUS, TrackerEndpointDBusClass))
#define TRACKER_IS_ENDPOINT_DBUS(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_ENDPOINT_DBUS))
#define TRACKER_IS_ENDPOINT_DBUS_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),  TRACKER_TYPE_ENDPOINT_DBUS))
#define TRACKER_ENDPOINT_DBUS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TRACKER_TYPE_ENDPOINT_DBUS, TrackerEndpointDBusClass))

typedef struct _TrackerEndpointDBus TrackerEndpointDBus;

G_DEFINE_AUTOPTR_CLEANUP_FUNC (TrackerEndpointDBus, g_object_unref)

TRACKER_AVAILABLE_IN_ALL
GType tracker_endpoint_dbus_get_type (void) G_GNUC_CONST;

TRACKER_AVAILABLE_IN_ALL
TrackerEndpointDBus *
tracker_endpoint_dbus_new (TrackerSparqlConnection  *sparql_connection,
                           GDBusConnection          *dbus_connection,
                           const gchar              *object_path,
                           GCancellable             *cancellable,
                           GError                  **error);

G_END_DECLS
