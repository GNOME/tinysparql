/*
 * Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
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
 */

#ifndef __TRACKER_BUS_H__
#define __TRACKER_BUS_H__

#include <libtracker-sparql/tracker-sparql.h>
#include <libtracker-sparql/tracker-private.h>

#define TRACKER_TYPE_BUS_CONNECTION (tracker_bus_connection_get_type ())
G_DECLARE_FINAL_TYPE (TrackerBusConnection,
                      tracker_bus_connection,
                      TRACKER, BUS_CONNECTION,
                      TrackerSparqlConnection)

TrackerSparqlConnection * tracker_bus_connection_new (const gchar      *service,
                                                      const gchar      *object_path,
                                                      GDBusConnection  *conn,
                                                      GError          **error);

void tracker_bus_connection_new_async (const gchar         *service,
                                       const gchar         *object_path,
                                       GDBusConnection     *conn,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  cb,
                                       gpointer             user_data);

TrackerSparqlConnection * tracker_bus_connection_new_finish (GAsyncResult  *res,
                                                             GError       **error);

void tracker_bus_connection_perform_query_async (TrackerBusConnection *conn,
						 const gchar          *sparql,
						 GVariant             *arguments,
						 GCancellable         *cancellable,
						 GAsyncReadyCallback   callback,
						 gpointer              user_data);

TrackerSparqlCursor * tracker_bus_connection_perform_query_finish (TrackerBusConnection  *conn,
								   GAsyncResult          *res,
								   GError               **error);

TrackerSparqlCursor * tracker_bus_connection_perform_query (TrackerBusConnection  *conn,
							    const gchar           *sparql,
							    GVariant              *arguments,
							    GCancellable          *cancellable,
							    GError               **error);

void tracker_bus_connection_perform_serialize_async (TrackerBusConnection  *conn,
						     TrackerSerializeFlags  flags,
						     TrackerRdfFormat       format,
						     const gchar           *query,
						     GVariant              *arguments,
						     GCancellable          *cancellable,
						     GAsyncReadyCallback    callback,
						     gpointer               user_data);

GInputStream * tracker_bus_connection_perform_serialize_finish (TrackerBusConnection  *conn,
								GAsyncResult          *res,
								GError               **error);

#endif /* __TRACKER_BUS_H__ */
