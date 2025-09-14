/*
 * Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
 * Copyright (C) 2019, Red Hat Inc.
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

#include "tracker-version-generated.h"
#include "tracker-cursor.h"
#include "tracker-endpoint-dbus.h"
#include "tracker-enums-private.h"
#include "tracker-deserializer.h"

struct _TrackerSparqlConnectionClass
{
	GObjectClass parent_class;

        TrackerSparqlCursor * (* query) (TrackerSparqlConnection  *connection,
                                         const gchar              *sparql,
                                         GCancellable             *cancellable,
                                         GError                  **error);
	void (* query_async) (TrackerSparqlConnection *connection,
	                      const gchar             *sparql,
	                      GCancellable            *cancellable,
	                      GAsyncReadyCallback      callback,
	                      gpointer                 user_data);
        TrackerSparqlCursor * (* query_finish) (TrackerSparqlConnection  *connection,
                                                GAsyncResult             *res,
                                                GError                  **error);
        void (* update) (TrackerSparqlConnection  *connection,
                         const gchar              *sparql,
                         GCancellable             *cancellable,
                         GError                  **error);
        void (* update_async) (TrackerSparqlConnection *connection,
                               const gchar             *sparql,
                               GCancellable            *cancellable,
                               GAsyncReadyCallback      callback,
                               gpointer                 user_data);
        void (* update_finish) (TrackerSparqlConnection  *connection,
                                GAsyncResult             *res,
                                GError                  **error);
        void (* update_array_async) (TrackerSparqlConnection  *connection,
                                     gchar                   **sparql,
                                     gint                      sparql_length,
                                     GCancellable             *cancellable,
                                     GAsyncReadyCallback       callback,
                                     gpointer                  user_data);
        gboolean (* update_array_finish) (TrackerSparqlConnection  *connection,
                                          GAsyncResult             *res,
                                          GError                  **error);
        GVariant* (* update_blank) (TrackerSparqlConnection  *connection,
                                    const gchar              *sparql,
                                    GCancellable             *cancellable,
                                    GError                  **error);
        void (* update_blank_async) (TrackerSparqlConnection *connection,
                                     const gchar             *sparql,
                                     GCancellable            *cancellable,
                                     GAsyncReadyCallback      callback,
                                     gpointer                 user_data);
        GVariant* (* update_blank_finish) (TrackerSparqlConnection  *connection,
                                           GAsyncResult             *res,
                                           GError                  **error);
        TrackerNamespaceManager * (* get_namespace_manager) (TrackerSparqlConnection *connection);
        TrackerSparqlStatement * (* query_statement) (TrackerSparqlConnection  *connection,
                                                      const gchar              *sparql,
                                                      GCancellable             *cancellable,
                                                      GError                  **error);
        TrackerSparqlStatement * (* update_statement) (TrackerSparqlConnection  *connection,
                                                       const gchar              *sparql,
                                                       GCancellable             *cancellable,
                                                       GError                  **error);
	TrackerNotifier * (* create_notifier) (TrackerSparqlConnection *connection);

	void (* close) (TrackerSparqlConnection *connection);
        void (* close_async) (TrackerSparqlConnection *connection,
                              GCancellable            *cancellable,
                              GAsyncReadyCallback      callback,
                              gpointer                 user_data);
        gboolean (* close_finish) (TrackerSparqlConnection  *connection,
                                   GAsyncResult             *res,
                                   GError                  **error);

	gboolean (* update_resource) (TrackerSparqlConnection  *connection,
				      const gchar              *graph,
				      TrackerResource          *resource,
				      GCancellable             *cancellable,
				      GError                  **error);
	void (* update_resource_async) (TrackerSparqlConnection *connection,
					const gchar             *graph,
					TrackerResource         *resource,
					GCancellable            *cancellable,
					GAsyncReadyCallback      callback,
					gpointer                 user_data);
	gboolean (* update_resource_finish) (TrackerSparqlConnection  *connection,
					     GAsyncResult             *res,
					     GError                  **error);
	TrackerBatch * (* create_batch) (TrackerSparqlConnection *connection);

	gboolean (* lookup_dbus_service) (TrackerSparqlConnection  *connection,
	                                  const gchar              *dbus_name,
	                                  const gchar              *dbus_path,
	                                  gchar                   **name,
	                                  gchar                   **path);

	void (* serialize_async) (TrackerSparqlConnection  *connection,
	                          TrackerSerializeFlags     flags,
	                          TrackerRdfFormat          format,
	                          const gchar              *query,
	                          GCancellable             *cancellable,
	                          GAsyncReadyCallback      callback,
	                          gpointer                 user_data);
	GInputStream * (* serialize_finish) (TrackerSparqlConnection  *connection,
	                                     GAsyncResult             *res,
	                                     GError                  **error);
	void (* deserialize_async) (TrackerSparqlConnection *connection,
	                            TrackerDeserializeFlags  flags,
	                            TrackerRdfFormat         format,
	                            const gchar             *default_graph,
	                            GInputStream            *stream,
	                            GCancellable            *cancellable,
	                            GAsyncReadyCallback      callback,
	                            gpointer                 user_data);
	gboolean (* deserialize_finish) (TrackerSparqlConnection  *connection,
	                                 GAsyncResult             *res,
	                                 GError                  **error);
	void (* map_connection) (TrackerSparqlConnection  *connection,
	                         const gchar              *handle_name,
	                         TrackerSparqlConnection  *service_connection);
};

struct _TrackerSparqlCursorClass
{
	GObjectClass parent_class;

	TrackerSparqlValueType (* get_value_type) (TrackerSparqlCursor *cursor,
	                                           gint                 column);
        const gchar* (* get_variable_name) (TrackerSparqlCursor *cursor,
                                            gint                 column);
	const gchar* (* get_string) (TrackerSparqlCursor  *cursor,
	                             gint                  column,
	                             const gchar         **langtag,
	                             glong                *length);
        gboolean (* next) (TrackerSparqlCursor  *cursor,
                           GCancellable         *cancellable,
                           GError              **error);
        void (* next_async) (TrackerSparqlCursor *cursor,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data);
        gboolean (* next_finish) (TrackerSparqlCursor  *cursor,
                                  GAsyncResult         *res,
                                  GError              **error);
        void (* rewind) (TrackerSparqlCursor* cursor);
        void (* close) (TrackerSparqlCursor* cursor);
        gint64 (* get_integer) (TrackerSparqlCursor *cursor,
                                gint                 column);
        gdouble (* get_double) (TrackerSparqlCursor *cursor,
                                gint                 column);
        gboolean (* get_boolean) (TrackerSparqlCursor *cursor,
                                  gint                 column);
        GDateTime* (* get_datetime) (TrackerSparqlCursor *cursor,
                                     gint                 column);
        gboolean (* is_bound) (TrackerSparqlCursor *cursor,
                               gint                 column);
        gint (* get_n_columns) (TrackerSparqlCursor *cursor);
};

struct _TrackerEndpointClass {
	GObjectClass parent_class;
};

struct _TrackerEndpointDBus {
	TrackerEndpoint parent_instance;
	GDBusConnection *dbus_connection;
	gchar *object_path;
	guint register_id;
	GDBusNodeInfo *node_info;
	GCancellable *cancellable;
	TrackerNotifier *notifier;
};

typedef struct _TrackerEndpointDBusClass TrackerEndpointDBusClass;

struct _TrackerEndpointDBusClass {
	struct _TrackerEndpointClass parent_class;
};

typedef struct _TrackerEndpointHttpClass TrackerEndpointHttpClass;

struct _TrackerEndpointHttpClass {
	struct _TrackerEndpointClass parent_class;
};

typedef struct _TrackerEndpointLocalClass TrackerEndpointLocalClass;

struct _TrackerEndpointLocalClass {
	struct _TrackerEndpointClass parent_class;
};

struct _TrackerResourceClass
{
	GObjectClass parent_class;
};

struct _TrackerSparqlStatementClass
{
	GObjectClass parent_class;

        void (* bind_int) (TrackerSparqlStatement *stmt,
                           const gchar            *name,
                           gint64                  value);
        void (* bind_boolean) (TrackerSparqlStatement *stmt,
                               const gchar            *name,
                               gboolean                value);
        void (* bind_string) (TrackerSparqlStatement *stmt,
                              const gchar            *name,
                              const gchar            *value);
        void (* bind_double) (TrackerSparqlStatement *stmt,
                              const gchar            *name,
                              gdouble                 value);
        void (* bind_datetime) (TrackerSparqlStatement *stmt,
                                const gchar            *name,
                                GDateTime              *value);
	void (* bind_langstring) (TrackerSparqlStatement *stmt,
	                          const gchar            *name,
	                          const gchar            *value,
	                          const gchar            *langtag);

        TrackerSparqlCursor * (* execute) (TrackerSparqlStatement  *stmt,
                                           GCancellable            *cancellable,
                                           GError                 **error);
        void (* execute_async) (TrackerSparqlStatement *stmt,
                                GCancellable           *cancellable,
                                GAsyncReadyCallback     callback,
                                gpointer                user_data);
        TrackerSparqlCursor * (* execute_finish) (TrackerSparqlStatement  *stmt,
                                                  GAsyncResult            *res,
                                                  GError                 **error);
	void (* clear_bindings) (TrackerSparqlStatement *stmt);

        void (* serialize_async) (TrackerSparqlStatement *stmt,
                                  TrackerSerializeFlags   flags,
                                  TrackerRdfFormat        format,
                                  GCancellable           *cancellable,
                                  GAsyncReadyCallback     callback,
                                  gpointer                user_data);
        GInputStream * (* serialize_finish) (TrackerSparqlStatement  *stmt,
                                             GAsyncResult            *res,
                                             GError                 **error);

        gboolean (* update) (TrackerSparqlStatement  *stmt,
                             GCancellable            *cancellable,
                             GError                 **error);
        void (* update_async) (TrackerSparqlStatement *stmt,
                               GCancellable           *cancellable,
                               GAsyncReadyCallback     callback,
                               gpointer                user_data);
        gboolean (* update_finish) (TrackerSparqlStatement  *stmt,
                                    GAsyncResult            *res,
                                    GError                 **error);
};

struct _TrackerNotifierClass {
	GObjectClass parent_class;

	void (* events) (TrackerNotifier *notifier,
	                 const GPtrArray *events);
};

struct _TrackerBatchClass {
	GObjectClass parent_class;

	void (* add_sparql) (TrackerBatch *batch,
			     const gchar  *sparql);
	void (* add_resource) (TrackerBatch    *batch,
			       const gchar     *graph,
			       TrackerResource *resource);
	void (* add_statement) (TrackerBatch           *batch,
	                        TrackerSparqlStatement *stmt,
	                        guint                   n_values,
	                        const gchar            *variable_names[],
	                        const GValue            values[]);
	void (* add_rdf) (TrackerBatch            *batch,
	                  TrackerDeserializeFlags  flags,
	                  TrackerRdfFormat         format,
	                  const gchar             *default_graph,
	                  GInputStream            *stream);
	void (* add_dbus_fd) (TrackerBatch *batch,
	                      GInputStream *istream);
	gboolean (* execute) (TrackerBatch  *batch,
			      GCancellable  *cancellable,
			      GError       **error);
	void (* execute_async) (TrackerBatch        *batch,
				GCancellable        *cancellable,
				GAsyncReadyCallback  callback,
				gpointer             user_data);
	gboolean (* execute_finish) (TrackerBatch  *batch,
				     GAsyncResult  *res,
				     GError       **error);
};

struct _TrackerSerializerClass {
	GInputStreamClass parent_class;
};

struct _TrackerDeserializerClass {
	TrackerSparqlCursorClass parent_class;

	gboolean (* get_parser_location) (TrackerDeserializer *deserializer,
	                                  goffset             *line_no,
	                                  goffset             *column_no);
};

struct _TrackerNamespaceManager {
	GObject parent;
};

typedef struct {
	GHashTableIter prop_iter;
	TrackerResource *cur_resource;
	const gchar *cur_prop;
	GPtrArray *cur_values;
	guint idx;
} TrackerResourceIterator;

gboolean
tracker_sparql_connection_lookup_dbus_service (TrackerSparqlConnection  *connection,
                                               const gchar              *dbus_name,
                                               const gchar              *dbus_path,
                                               gchar                   **name,
                                               gchar                   **path);

gboolean tracker_sparql_connection_set_error_on_closed (TrackerSparqlConnection  *connection,
                                                        GError                  **error);

gboolean tracker_sparql_connection_report_async_error_on_closed (TrackerSparqlConnection *connection,
                                                                 GAsyncReadyCallback      callback,
                                                                 gpointer                 user_data);

void tracker_sparql_cursor_set_connection (TrackerSparqlCursor     *cursor,
                                           TrackerSparqlConnection *connection);
GError * _translate_internal_error (GError *error);

void tracker_namespace_manager_seal (TrackerNamespaceManager *namespaces);

void tracker_resource_iterator_init (TrackerResourceIterator *iter,
                                     TrackerResource         *resource);
gboolean tracker_resource_iterator_next (TrackerResourceIterator  *iter,
                                         const gchar             **property,
                                         const GValue            **value);
const gchar * tracker_resource_get_identifier_internal (TrackerResource *resource);
gboolean tracker_resource_is_blank_node (TrackerResource *resource);

void tracker_endpoint_rewrite_query (TrackerEndpoint  *endpoint,
                                     gchar           **query);

gboolean tracker_endpoint_is_graph_filtered (TrackerEndpoint *endpoint,
                                             const gchar     *graph);

TrackerSparqlStatement * tracker_endpoint_cache_select_sparql (TrackerEndpoint  *endpoint,
                                                               const gchar      *sparql,
                                                               GCancellable     *cancellable,
                                                               GError          **error);

void tracker_batch_add_dbus_fd (TrackerBatch *batch,
                                GInputStream *istream);

GBytes * tracker_sparql_make_langstring (const gchar *str,
                                         const gchar *langtag);

gboolean tracker_rdf_format_pick_for_file (GFile            *file,
                                           TrackerRdfFormat *format_out);

void tracker_ensure_resources (void);
