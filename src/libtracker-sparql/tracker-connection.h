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

#pragma once

#if !defined (__LIBTRACKER_SPARQL_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "only <tinysparql.h> must be included directly."
#endif

#include <gio/gio.h>

#include "tracker-enums.h"
#include "tracker-error.h"
#include "tracker-notifier.h"
#include "tracker-resource.h"
#include "tracker-version.h"

G_BEGIN_DECLS

/**
 * TrackerSparqlConnectionFlags:
 * @TRACKER_SPARQL_CONNECTION_FLAGS_NONE: No flags.
 * @TRACKER_SPARQL_CONNECTION_FLAGS_READONLY: Connection is readonly.
 * @TRACKER_SPARQL_CONNECTION_FLAGS_FTS_ENABLE_STEMMER: Word stemming is applied to FTS search terms.
 * @TRACKER_SPARQL_CONNECTION_FLAGS_FTS_ENABLE_UNACCENT: Unaccenting is applied to FTS search terms.
 * @TRACKER_SPARQL_CONNECTION_FLAGS_FTS_ENABLE_STOP_WORDS: FTS Search terms are filtered through a stop word list. This flag is deprecated since Tracker 3.6, and will do nothing.
 * @TRACKER_SPARQL_CONNECTION_FLAGS_FTS_IGNORE_NUMBERS: Ignore numbers in FTS search terms.
 * @TRACKER_SPARQL_CONNECTION_FLAGS_ANONYMOUS_BNODES: Treat blank nodes as specified in
 *   SPARQL 1.1 syntax. Namely, they cannot be used as URIs. This flag is available since Tracker 3.3.
 *
 * Connection flags to modify #TrackerSparqlConnection behavior.
 */
typedef enum {
	TRACKER_SPARQL_CONNECTION_FLAGS_NONE                  = 0,
	TRACKER_SPARQL_CONNECTION_FLAGS_READONLY              = 1 << 0,
	TRACKER_SPARQL_CONNECTION_FLAGS_FTS_ENABLE_STEMMER    = 1 << 1,
	TRACKER_SPARQL_CONNECTION_FLAGS_FTS_ENABLE_UNACCENT   = 1 << 2,
	TRACKER_SPARQL_CONNECTION_FLAGS_FTS_ENABLE_STOP_WORDS = 1 << 3,
	TRACKER_SPARQL_CONNECTION_FLAGS_FTS_IGNORE_NUMBERS    = 1 << 4,
	TRACKER_SPARQL_CONNECTION_FLAGS_ANONYMOUS_BNODES      = 1 << 5,
} TrackerSparqlConnectionFlags;

/**
 * TrackerSerializeFlags:
 * @TRACKER_SERIALIZE_FLAGS_NONE: No flags.
 *
 * Flags affecting serialization into a RDF data format.
 */
typedef enum {
	TRACKER_SERIALIZE_FLAGS_NONE = 0,
} TrackerSerializeFlags;

/**
 * TrackerDeserializeFlags:
 * @TRACKER_DESERIALIZE_FLAGS_NONE: No flags.
 *
 * Flags affecting deserialization from a RDF data format.
 */
typedef enum {
	TRACKER_DESERIALIZE_FLAGS_NONE = 0,
} TrackerDeserializeFlags;

#define TRACKER_TYPE_SPARQL_CONNECTION tracker_sparql_connection_get_type ()
#define TRACKER_SPARQL_TYPE_CONNECTION TRACKER_TYPE_SPARQL_CONNECTION

TRACKER_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (TrackerSparqlConnection,
                          tracker_sparql_connection,
                          TRACKER, SPARQL_CONNECTION,
                          GObject)

#include "tracker-batch.h"
#include "tracker-cursor.h"
#include "tracker-statement.h"
#include "tracker-namespace-manager.h"


TRACKER_AVAILABLE_IN_ALL
TrackerSparqlConnection * tracker_sparql_connection_new (TrackerSparqlConnectionFlags   flags,
                                                         GFile                         *store,
                                                         GFile                         *ontology,
                                                         GCancellable                  *cancellable,
                                                         GError                       **error);
TRACKER_AVAILABLE_IN_ALL
void tracker_sparql_connection_new_async (TrackerSparqlConnectionFlags   flags,
                                          GFile                         *store,
                                          GFile                         *ontology,
                                          GCancellable                  *cancellable,
                                          GAsyncReadyCallback            callback,
                                          gpointer                       user_data);
TRACKER_AVAILABLE_IN_ALL
TrackerSparqlConnection * tracker_sparql_connection_new_finish (GAsyncResult  *result,
                                                                GError       **error);

TRACKER_AVAILABLE_IN_ALL
TrackerSparqlConnection * tracker_sparql_connection_bus_new (const gchar      *service_name,
                                                             const gchar      *object_path,
                                                             GDBusConnection  *dbus_connection,
                                                             GError          **error);
TRACKER_AVAILABLE_IN_3_1
void tracker_sparql_connection_bus_new_async (const gchar         *service_name,
                                              const gchar         *object_path,
                                              GDBusConnection     *dbus_connection,
                                              GCancellable        *cancellable,
                                              GAsyncReadyCallback  callback,
                                              gpointer             user_data);
TRACKER_AVAILABLE_IN_3_1
TrackerSparqlConnection * tracker_sparql_connection_bus_new_finish (GAsyncResult  *result,
                                                                    GError       **error);

TRACKER_AVAILABLE_IN_ALL
TrackerSparqlConnection * tracker_sparql_connection_remote_new (const gchar *uri_base);

TRACKER_AVAILABLE_IN_ALL
TrackerSparqlCursor * tracker_sparql_connection_query (TrackerSparqlConnection  *connection,
                                                       const gchar              *sparql,
                                                       GCancellable             *cancellable,
                                                       GError                  **error);

TRACKER_AVAILABLE_IN_ALL
void tracker_sparql_connection_query_async (TrackerSparqlConnection *connection,
                                            const gchar             *sparql,
                                            GCancellable            *cancellable,
                                            GAsyncReadyCallback      callback,
                                            gpointer                 user_data);
TRACKER_AVAILABLE_IN_ALL
TrackerSparqlCursor * tracker_sparql_connection_query_finish (TrackerSparqlConnection  *connection,
                                                              GAsyncResult             *res,
                                                              GError                  **error);

TRACKER_AVAILABLE_IN_ALL
void tracker_sparql_connection_update (TrackerSparqlConnection  *connection,
                                       const gchar              *sparql,
                                       GCancellable             *cancellable,
                                       GError                  **error);
TRACKER_AVAILABLE_IN_ALL
void tracker_sparql_connection_update_async (TrackerSparqlConnection *connection,
                                             const gchar             *sparql,
                                             GCancellable            *cancellable,
                                             GAsyncReadyCallback      callback,
                                             gpointer                 user_data);
TRACKER_AVAILABLE_IN_ALL
void tracker_sparql_connection_update_finish (TrackerSparqlConnection  *connection,
                                              GAsyncResult             *res,
                                              GError                  **error);
TRACKER_AVAILABLE_IN_ALL
void tracker_sparql_connection_update_array_async (TrackerSparqlConnection  *connection,
                                                   gchar                   **sparql,
                                                   gint                      sparql_length,
                                                   GCancellable             *cancellable,
                                                   GAsyncReadyCallback       callback,
                                                   gpointer                  user_data);
TRACKER_AVAILABLE_IN_ALL
gboolean tracker_sparql_connection_update_array_finish (TrackerSparqlConnection  *connection,
                                                        GAsyncResult             *res,
                                                        GError                  **error);
TRACKER_AVAILABLE_IN_3_1
gboolean tracker_sparql_connection_update_resource (TrackerSparqlConnection  *connection,
                                                    const gchar              *graph,
                                                    TrackerResource          *resource,
                                                    GCancellable             *cancellable,
                                                    GError                  **error);
TRACKER_AVAILABLE_IN_3_1
void tracker_sparql_connection_update_resource_async (TrackerSparqlConnection *connection,
                                                      const gchar             *graph,
                                                      TrackerResource         *resource,
                                                      GCancellable            *cancellable,
                                                      GAsyncReadyCallback      callback,
                                                      gpointer                 user_data);
TRACKER_AVAILABLE_IN_3_1
gboolean tracker_sparql_connection_update_resource_finish (TrackerSparqlConnection  *connection,
                                                           GAsyncResult             *res,
                                                           GError                  **error);
TRACKER_AVAILABLE_IN_3_1
TrackerBatch * tracker_sparql_connection_create_batch (TrackerSparqlConnection *connection);

TRACKER_DEPRECATED_IN_3_5
GVariant * tracker_sparql_connection_update_blank (TrackerSparqlConnection  *connection,
                                                   const gchar              *sparql,
                                                   GCancellable             *cancellable,
                                                   GError                  **error);
TRACKER_DEPRECATED_IN_3_5
void tracker_sparql_connection_update_blank_async (TrackerSparqlConnection *connection,
                                                   const gchar             *sparql,
                                                   GCancellable            *cancellable,
                                                   GAsyncReadyCallback      callback,
                                                   gpointer                 user_data);
TRACKER_DEPRECATED_IN_3_5
GVariant * tracker_sparql_connection_update_blank_finish (TrackerSparqlConnection  *connection,
                                                          GAsyncResult             *res,
                                                          GError                  **error);

TRACKER_AVAILABLE_IN_ALL
TrackerNamespaceManager * tracker_sparql_connection_get_namespace_manager (TrackerSparqlConnection *connection);

TRACKER_AVAILABLE_IN_ALL
TrackerSparqlStatement * tracker_sparql_connection_query_statement (TrackerSparqlConnection  *connection,
                                                                    const gchar              *sparql,
                                                                    GCancellable             *cancellable,
                                                                    GError                  **error);

TRACKER_AVAILABLE_IN_3_5
TrackerSparqlStatement * tracker_sparql_connection_update_statement (TrackerSparqlConnection  *connection,
                                                                     const gchar              *sparql,
                                                                     GCancellable             *cancellable,
                                                                     GError                  **error);

TRACKER_AVAILABLE_IN_ALL
TrackerNotifier * tracker_sparql_connection_create_notifier (TrackerSparqlConnection *connection);

TRACKER_AVAILABLE_IN_ALL
void tracker_sparql_connection_close (TrackerSparqlConnection *connection);

TRACKER_AVAILABLE_IN_3_3
void tracker_sparql_connection_serialize_async (TrackerSparqlConnection  *connection,
                                                TrackerSerializeFlags     flags,
                                                TrackerRdfFormat          format,
                                                const gchar              *query,
                                                GCancellable             *cancellable,
                                                GAsyncReadyCallback      callback,
                                                gpointer                 user_data);
TRACKER_AVAILABLE_IN_3_3
GInputStream * tracker_sparql_connection_serialize_finish (TrackerSparqlConnection  *connection,
                                                           GAsyncResult             *result,
                                                           GError                  **error);

TRACKER_AVAILABLE_IN_3_4
void tracker_sparql_connection_deserialize_async (TrackerSparqlConnection *connection,
                                                  TrackerDeserializeFlags  flags,
                                                  TrackerRdfFormat         format,
                                                  const gchar             *default_graph,
                                                  GInputStream            *stream,
                                                  GCancellable            *cancellable,
                                                  GAsyncReadyCallback      callback,
                                                  gpointer                 user_data);
TRACKER_AVAILABLE_IN_3_4
gboolean tracker_sparql_connection_deserialize_finish (TrackerSparqlConnection  *connection,
                                                       GAsyncResult             *result,
                                                       GError                  **error);

TRACKER_AVAILABLE_IN_ALL
void tracker_sparql_connection_close_async (TrackerSparqlConnection *connection,
                                            GCancellable            *cancellable,
                                            GAsyncReadyCallback      callback,
                                            gpointer                 user_data);

TRACKER_AVAILABLE_IN_ALL
gboolean tracker_sparql_connection_close_finish (TrackerSparqlConnection  *connection,
                                                 GAsyncResult             *res,
                                                 GError                  **error);

TRACKER_AVAILABLE_IN_3_3
TrackerSparqlStatement * tracker_sparql_connection_load_statement_from_gresource (TrackerSparqlConnection  *connection,
                                                                                  const gchar              *resource_path,
                                                                                  GCancellable             *cancellable,
                                                                                  GError                  **error);

TRACKER_AVAILABLE_IN_3_3
void tracker_sparql_connection_map_connection (TrackerSparqlConnection *connection,
					       const gchar             *handle_name,
					       TrackerSparqlConnection *service_connection);

G_END_DECLS
