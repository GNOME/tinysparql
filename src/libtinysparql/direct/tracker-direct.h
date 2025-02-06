/*
 * Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
 * Copyright (C) 2017, Red Hat, Inc.
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

#pragma once

#include <tinysparql.h>

#include "core/tracker-data.h"
#include "direct/tracker-direct-batch.h"
#include "direct/tracker-direct-statement.h"

#define TRACKER_TYPE_DIRECT_CONNECTION         (tracker_direct_connection_get_type())
#define TRACKER_DIRECT_CONNECTION(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_DIRECT_CONNECTION, TrackerDirectConnection))
#define TRACKER_DIRECT_CONNECTION_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), TRACKER_TYPE_DIRECT_CONNECTION, TrackerDirectConnectionClass))
#define TRACKER_IS_DIRECT_CONNECTION(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_DIRECT_CONNECTION))
#define TRACKER_IS_DIRECT_CONNECTION_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),  TRACKER_TYPE_DIRECT_CONNECTION))
#define TRACKER_DIRECT_CONNECTION_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TRACKER_TYPE_DIRECT_CONNECTION, TrackerDirectConnectionClass))

typedef struct _TrackerDirectConnection TrackerDirectConnection;
typedef struct _TrackerDirectConnectionClass TrackerDirectConnectionClass;

struct _TrackerDirectConnectionClass
{
	TrackerSparqlConnectionClass parent_class;
};

struct _TrackerDirectConnection
{
	TrackerSparqlConnection parent_instance;
};

GType tracker_direct_connection_get_type (void) G_GNUC_CONST;

TrackerSparqlConnection *tracker_direct_connection_new (TrackerSparqlConnectionFlags   flags,
                                                        GFile                         *store,
                                                        GFile                         *ontology,
                                                        GError                       **error);
void tracker_direct_connection_new_async (TrackerSparqlConnectionFlags  flags,
                                          GFile                        *store,
                                          GFile                        *ontology,
                                          GCancellable                 *cancellable,
                                          GAsyncReadyCallback           cb,
                                          gpointer                      user_data);
TrackerSparqlConnection * tracker_direct_connection_new_finish (GAsyncResult  *res,
                                                                GError       **error);

TrackerDataManager *tracker_direct_connection_get_data_manager (TrackerDirectConnection *conn);

void tracker_direct_connection_update_timestamp (TrackerDirectConnection *conn);

/* Internal helper functions */
GError *translate_db_interface_error (GError *error);

gboolean tracker_direct_connection_update_batch (TrackerDirectConnection  *conn,
                                                 TrackerBatch             *batch,
                                                 GError                  **error);
void tracker_direct_connection_update_batch_async (TrackerDirectConnection  *conn,
                                                   TrackerBatch             *batch,
                                                   GCancellable             *cancellable,
                                                   GAsyncReadyCallback       callback,
                                                   gpointer                  user_data);
gboolean tracker_direct_connection_update_batch_finish (TrackerDirectConnection  *conn,
                                                        GAsyncResult             *res,
                                                        GError                  **error);

gboolean tracker_direct_connection_execute_update_statement (TrackerDirectConnection  *conn,
                                                             TrackerSparqlStatement   *stmt,
                                                             GHashTable               *parameters,
                                                             GError                  **error);
void tracker_direct_connection_execute_update_statement_async (TrackerDirectConnection  *conn,
                                                               TrackerSparqlStatement   *stmt,
                                                               GHashTable               *parameters,
                                                               GCancellable             *cancellable,
                                                               GAsyncReadyCallback       callback,
                                                               gpointer                  user_data);
gboolean tracker_direct_connection_execute_update_statement_finish (TrackerDirectConnection  *conn,
                                                                    GAsyncResult             *res,
                                                                    GError                  **error);

void tracker_direct_connection_execute_query_statement_async (TrackerDirectConnection *conn,
                                                              TrackerSparqlStatement  *stmt,
                                                              GHashTable              *parameters,
                                                              GCancellable            *cancellable,
                                                              GAsyncReadyCallback      callback,
                                                              gpointer                 user_data);

TrackerSparqlCursor * tracker_direct_connection_execute_query_statement_finish (TrackerDirectConnection  *conn,
                                                                                GAsyncResult             *res,
                                                                                GError                  **error);

void tracker_direct_connection_execute_serialize_statement_async (TrackerDirectConnection *conn,
                                                                  TrackerSparqlStatement  *stmt,
                                                                  GHashTable              *parameters,
                                                                  TrackerSerializeFlags    flags,
                                                                  TrackerRdfFormat         format,
                                                                  GCancellable            *cancellable,
                                                                  GAsyncReadyCallback      callback,
                                                                  gpointer                 user_data);

GInputStream * tracker_direct_connection_execute_serialize_statement_finish (TrackerDirectConnection  *conn,
                                                                             GAsyncResult             *res,
                                                                             GError                  **error);
