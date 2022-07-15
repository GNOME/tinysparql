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

/**
 * SECTION: tracker-sparql-connection
 * @short_description: Connection to SPARQL triple store
 * @title: TrackerSparqlConnection
 * @stability: Stable
 * @include: tracker-sparql.h
 *
 * #TrackerSparqlConnection is an object that represents a connection to a
 * SPARQL triple store. This store may be local and private (see
 * tracker_sparql_connection_new()), or it may be a remote connection to a
 * public endpoint (See tracker_sparql_connection_bus_new() and
 * tracker_sparql_connection_remote_new()).
 *
 * A #TrackerSparqlConnection is private to the calling process, it can be
 * exposed publicly via a #TrackerEndpoint, see tracker_endpoint_dbus_new().
 *
 * Updates on a connection are performed via the tracker_sparql_connection_update()
 * family of calls. tracker_sparql_connection_update_array() may be used for batched
 * updates. All functions have asynchronous variants.
 *
 * Queries on a connection are performed via tracker_sparql_connection_query()
 * and tracker_sparql_connection_query_statement(). The first call receives a
 * query string and returns a #TrackerSparqlCursor to iterate the results. The
 * second call returns a #TrackerSparqlStatement object that may be reused for
 * repeatable queries with variable parameters. tracker_sparql_statement_execute()
 * will returns a #TrackerSparqlCursor.
 *
 * Depending on the ontology definition, #TrackerSparqlConnection may emit
 * notifications whenever changes happen in the stored data. These notifications
 * can be processed via a #TrackerNotifier obtained with
 * tracker_sparql_connection_create_notifier().
 *
 * After use, a #TrackerSparqlConnection should be closed. See
 * tracker_sparql_connection_close() and tracker_sparql_connection_close_async().
 *
 * A #TrackerSparqlConnection may be used from multiple threads, asynchronous
 * database updates are executed sequentially on arrival order, asynchronous
 * queries are executed in a thread pool.
 */
#include "config.h"

#include "tracker-connection.h"
#include "tracker-private.h"
#include "tracker-debug.h"

#include "bus/tracker-bus.h"
#include "direct/tracker-direct.h"
#include "remote/tracker-remote.h"

G_DEFINE_ABSTRACT_TYPE (TrackerSparqlConnection, tracker_sparql_connection,
                        G_TYPE_OBJECT)

static void
tracker_sparql_connection_init (TrackerSparqlConnection *connection)
{
}

static void
tracker_sparql_connection_dispose (GObject *object)
{
	tracker_sparql_connection_close (TRACKER_SPARQL_CONNECTION (object));

	G_OBJECT_CLASS (tracker_sparql_connection_parent_class)->dispose (object);
}

static void
tracker_sparql_connection_class_init (TrackerSparqlConnectionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = tracker_sparql_connection_dispose;

	/* Initialize debug flags */
	tracker_get_debug_flags ();
}

gboolean
tracker_sparql_connection_lookup_dbus_service (TrackerSparqlConnection  *connection,
                                               const gchar              *dbus_name,
                                               const gchar              *dbus_path,
                                               gchar                   **name,
                                               gchar                   **path)
{
	TrackerSparqlConnectionClass *connection_class;

	g_return_val_if_fail (TRACKER_IS_SPARQL_CONNECTION (connection), FALSE);
	g_return_val_if_fail (dbus_name != NULL, FALSE);

	connection_class = TRACKER_SPARQL_CONNECTION_GET_CLASS (connection);
	if (!connection_class->lookup_dbus_service)
		return FALSE;

	return TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->lookup_dbus_service (connection,
	                                                                              dbus_name,
	                                                                              dbus_path,
	                                                                              name,
	                                                                              path);
}

/**
 * tracker_sparql_connection_query:
 * @connection: a #TrackerSparqlConnection
 * @sparql: string containing the SPARQL query
 * @cancellable: a #GCancellable used to cancel the operation
 * @error: #GError for error reporting.
 *
 * Executes a SPARQL query on. The API call is completely synchronous, so
 * it may block.
 *
 * The @sparql query should be built with #TrackerResource, or
 * its parts correctly escaped using tracker_sparql_escape_string(),
 * otherwise SPARQL injection is possible.
 *
 * Returns: (transfer full): a #TrackerSparqlCursor if results were found.
 * On error, #NULL is returned and the @error is set accordingly.
 * Call g_object_unref() on the returned cursor when no longer needed.
 */
TrackerSparqlCursor *
tracker_sparql_connection_query (TrackerSparqlConnection  *connection,
                                 const gchar              *sparql,
                                 GCancellable             *cancellable,
                                 GError                  **error)
{
	TrackerSparqlCursor *cursor;

	g_return_val_if_fail (TRACKER_IS_SPARQL_CONNECTION (connection), NULL);
	g_return_val_if_fail (sparql != NULL, NULL);
	g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), NULL);
	g_return_val_if_fail (!error || !*error, NULL);

	cursor = TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->query (connection,
	                                                                  sparql,
	                                                                  cancellable,
	                                                                  error);
	if (cursor)
		tracker_sparql_cursor_set_connection (cursor, connection);

	return cursor;
}

/**
 * tracker_sparql_connection_query_async:
 * @connection: a #TrackerSparqlConnection
 * @sparql: string containing the SPARQL query
 * @cancellable: a #GCancellable used to cancel the operation
 * @callback: user-defined #GAsyncReadyCallback to be called when
 *            asynchronous operation is finished.
 * @user_data: user-defined data to be passed to @callback
 *
 * Executes asynchronously a SPARQL query.
 */
void
tracker_sparql_connection_query_async (TrackerSparqlConnection *connection,
                                       const gchar             *sparql,
                                       GCancellable            *cancellable,
                                       GAsyncReadyCallback      callback,
                                       gpointer                 user_data)
{
	g_return_if_fail (TRACKER_IS_SPARQL_CONNECTION (connection));
	g_return_if_fail (sparql != NULL);
	g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

	TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->query_async (connection,
	                                                               sparql,
	                                                               cancellable,
	                                                               callback,
	                                                               user_data);
}

/**
 * tracker_sparql_connection_query_finish:
 * @connection: a #TrackerSparqlConnection
 * @res: a #GAsyncResult with the result of the operation
 * @error: #GError for error reporting.
 *
 * Finishes the asynchronous SPARQL query operation.
 *
 * Returns: (transfer full): a #TrackerSparqlCursor if results were found.
 * On error, #NULL is returned and the @error is set accordingly.
 * Call g_object_unref() on the returned cursor when no longer needed.
 */
TrackerSparqlCursor *
tracker_sparql_connection_query_finish (TrackerSparqlConnection  *connection,
                                        GAsyncResult             *res,
                                        GError                  **error)
{
	TrackerSparqlCursor *cursor;

	g_return_val_if_fail (TRACKER_IS_SPARQL_CONNECTION (connection), NULL);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (res), NULL);
	g_return_val_if_fail (!error || !*error, NULL);

	cursor = TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->query_finish (connection,
	                                                                         res,
	                                                                         error);
	if (cursor)
		tracker_sparql_cursor_set_connection (cursor, connection);

	return cursor;
}

/**
 * tracker_sparql_connection_update:
 * @connection: a #TrackerSparqlConnection
 * @sparql: string containing the SPARQL update query
 * @cancellable: a #GCancellable used to cancel the operation
 * @error: #GError for error reporting.
 *
 * Executes a SPARQL update. The API call is completely
 * synchronous, so it may block.
 *
 * The @sparql query should be built with #TrackerResource, or
 * its parts correctly escaped using tracker_sparql_escape_string(),
 * otherwise SPARQL injection is possible.
 */
void
tracker_sparql_connection_update (TrackerSparqlConnection  *connection,
                                  const gchar              *sparql,
                                  GCancellable             *cancellable,
                                  GError                  **error)
{
	g_return_if_fail (TRACKER_IS_SPARQL_CONNECTION (connection));
	g_return_if_fail (sparql != NULL);
	g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (!error || !*error);

	TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->update (connection,
	                                                          sparql,
	                                                          cancellable,
	                                                          error);
}

/**
 * tracker_sparql_connection_update_async:
 * @connection: a #TrackerSparqlConnection
 * @sparql: string containing the SPARQL update query
 * @cancellable: a #GCancellable used to cancel the operation
 * @callback: user-defined #GAsyncReadyCallback to be called when
 *            asynchronous operation is finished.
 * @user_data: user-defined data to be passed to @callback
 *
 * Executes asynchronously a SPARQL update.
 */
void
tracker_sparql_connection_update_async (TrackerSparqlConnection *connection,
                                        const gchar             *sparql,
                                        GCancellable            *cancellable,
                                        GAsyncReadyCallback      callback,
                                        gpointer                 user_data)
{
	g_return_if_fail (TRACKER_IS_SPARQL_CONNECTION (connection));
	g_return_if_fail (sparql != NULL);
	g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

	TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->update_async (connection,
	                                                                sparql,
	                                                                cancellable,
	                                                                callback,
	                                                                user_data);
}

/**
 * tracker_sparql_connection_update_finish:
 * @connection: a #TrackerSparqlConnection
 * @res: a #GAsyncResult with the result of the operation
 * @error: #GError for error reporting.
 *
 * Finishes the asynchronous SPARQL update operation.
 */
void
tracker_sparql_connection_update_finish (TrackerSparqlConnection  *connection,
                                         GAsyncResult             *res,
                                         GError                  **error)
{
	g_return_if_fail (TRACKER_IS_SPARQL_CONNECTION (connection));
	g_return_if_fail (G_IS_ASYNC_RESULT (res));
	g_return_if_fail (!error || !*error);

	TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->update_finish (connection,
	                                                                 res,
	                                                                 error);
}

/**
 * tracker_sparql_connection_update_array_async:
 * @connection: a #TrackerSparqlConnection
 * @sparql: an array of strings containing the SPARQL update queries
 * @sparql_length: the amount of strings you pass as @sparql
 * @cancellable: a #GCancellable used to cancel the operation
 * @callback: user-defined #GAsyncReadyCallback to be called when
 *            asynchronous operation is finished.
 * @user_data: user-defined data to be passed to @callback
 *
 * Executes asynchronously an array of SPARQL updates. All updates in the
 * array are handled within a single transaction.
 */
void
tracker_sparql_connection_update_array_async (TrackerSparqlConnection  *connection,
                                              gchar                   **sparql,
                                              gint                      sparql_length,
                                              GCancellable             *cancellable,
                                              GAsyncReadyCallback       callback,
                                              gpointer                  user_data)
{
	g_return_if_fail (TRACKER_IS_SPARQL_CONNECTION (connection));
	g_return_if_fail (sparql != NULL || sparql_length == 0);
	g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

	TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->update_array_async (connection,
	                                                                      sparql,
	                                                                      sparql_length,
	                                                                      cancellable,
	                                                                      callback,
	                                                                      user_data);
}

/**
 * tracker_sparql_connection_update_array_finish:
 * @connection: a #TrackerSparqlConnection
 * @res: a #GAsyncResult with the result of the operation
 * @error: #GError for error reporting.
 *
 * Finishes the asynchronous SPARQL update_array operation.
 *
 * Returns: #TRUE if there were no errors.
 */
gboolean
tracker_sparql_connection_update_array_finish (TrackerSparqlConnection  *connection,
                                               GAsyncResult             *res,
                                               GError                  **error)
{
	g_return_val_if_fail (TRACKER_IS_SPARQL_CONNECTION (connection), FALSE);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (res), FALSE);
	g_return_val_if_fail (!error || !*error, FALSE);

	return TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->update_array_finish (connection,
	                                                                              res,
	                                                                              error);

}

/**
 * tracker_sparql_connection_update_blank:
 * @connection: a #TrackerSparqlConnection
 * @sparql: string containing the SPARQL update query
 * @cancellable: a #GCancellable used to cancel the operation
 * @error: #GError for error reporting.
 *
 * Executes a SPARQL update and returns the URNs of the generated nodes,
 * if any. The API call is completely synchronous, so it may block.
 *
 * The @sparql query should be built with #TrackerResource, or
 * its parts correctly escaped using tracker_sparql_escape_string(),
 * otherwise SPARQL injection is possible.
 *
 * The format string of the `GVariant` is `aaa{ss}` (an array of an array
 * of dictionaries). The first array represents each INSERT that may exist in
 * the SPARQL string. The second array represents each new node for a given
 * WHERE clause. The last array holds a string pair with the blank node name
 * (e.g. `foo` for the blank node `_:foo`) and the URN that was generated for
 * it. For most updates the first two outer arrays will only contain one item.
 *
 * Returns: a #GVariant with the generated URNs, which should be freed with
 * g_variant_unref() when no longer used.
 */
GVariant *
tracker_sparql_connection_update_blank (TrackerSparqlConnection  *connection,
                                        const gchar              *sparql,
                                        GCancellable             *cancellable,
                                        GError                  **error)
{
	g_return_val_if_fail (TRACKER_IS_SPARQL_CONNECTION (connection), NULL);
	g_return_val_if_fail (sparql != NULL, NULL);
	g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), NULL);
	g_return_val_if_fail (!error || !*error, NULL);

	return TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->update_blank (connection,
	                                                                       sparql,
	                                                                       cancellable,
	                                                                       error);
}

/**
 * tracker_sparql_connection_update_blank_async:
 * @connection: a #TrackerSparqlConnection
 * @sparql: string containing the SPARQL update query
 * @cancellable: a #GCancellable used to cancel the operation
 * @callback: user-defined #GAsyncReadyCallback to be called when
 *            asynchronous operation is finished.
 * @user_data: user-defined data to be passed to @callback
 *
 * Executes asynchronously a SPARQL update with blank nodes. See
 * the tracker_sparql_connection_update_blank() documentation to
 * see the differences with tracker_sparql_connection_update().
 */
void
tracker_sparql_connection_update_blank_async (TrackerSparqlConnection *connection,
                                              const gchar             *sparql,
                                              GCancellable            *cancellable,
                                              GAsyncReadyCallback      callback,
                                              gpointer                 user_data)
{
	g_return_if_fail (TRACKER_IS_SPARQL_CONNECTION (connection));
	g_return_if_fail (sparql != NULL);
	g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

	TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->update_blank_async (connection,
	                                                                      sparql,
	                                                                      cancellable,
	                                                                      callback,
	                                                                      user_data);
}

/**
 * tracker_sparql_connection_update_blank_finish:
 * @connection: a #TrackerSparqlConnection
 * @res: a #GAsyncResult with the result of the operation
 * @error: #GError for error reporting.
 *
 * Finishes the asynchronous SPARQL update operation, and returns
 * the URNs of the generated nodes, if any. See the
 * tracker_sparql_connection_update_blank() documentation for the interpretation
 * of the returned #GVariant.
 *
 * Returns: a #GVariant with the generated URNs, which should be freed with
 * g_variant_unref() when no longer used.
 */
GVariant *
tracker_sparql_connection_update_blank_finish (TrackerSparqlConnection  *connection,
                                               GAsyncResult             *res,
                                               GError                  **error)
{
	g_return_val_if_fail (TRACKER_IS_SPARQL_CONNECTION (connection), NULL);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (res), NULL);
	g_return_val_if_fail (!error || !*error, NULL);

	return TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->update_blank_finish (connection,
	                                                                        res,
	                                                                        error);
}

/**
 * tracker_sparql_connection_update_resource:
 * @connection: a #TrackerSparqlConnection
 * @graph: (nullable): RDF graph where the resource should be inserted/updated, or %NULL for the default graph
 * @resource: a #TrackerResource
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @error: pointer to a #GError, or %NULL
 *
 * Inserts a resource as described by @resource, on the graph described by @graph.
 * This operation blocks until done.
 *
 * Returns: #TRUE if there were no errors.
 *
 * Since: 3.1
 **/
gboolean
tracker_sparql_connection_update_resource (TrackerSparqlConnection  *connection,
                                           const gchar              *graph,
                                           TrackerResource          *resource,
                                           GCancellable             *cancellable,
                                           GError                  **error)
{
	g_return_val_if_fail (TRACKER_IS_SPARQL_CONNECTION (connection), FALSE);
	g_return_val_if_fail (TRACKER_IS_RESOURCE (resource), FALSE);
	g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);
	g_return_val_if_fail (!error || !*error, FALSE);

	return TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->update_resource (connection,
	                                                                          graph,
	                                                                          resource,
	                                                                          cancellable,
	                                                                          error);
}

/**
 * tracker_sparql_connection_update_resource_async:
 * @connection: a #TrackerSparqlConnection
 * @graph: (nullable): RDF graph where the resource should be inserted/updated, or %NULL for the default graph
 * @resource: a #TrackerResource
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @callback: the #GAsyncReadyCallback called when the operation completes
 * @user_data: data passed to @callback
 *
 * Inserts a resource as described by @resource, on the graph described by @graph.
 * This operation is executed asynchronously, when finished @callback will be
 * executed.
 *
 * Since: 3.1
 **/
void
tracker_sparql_connection_update_resource_async (TrackerSparqlConnection *connection,
                                                 const gchar             *graph,
                                                 TrackerResource         *resource,
                                                 GCancellable            *cancellable,
                                                 GAsyncReadyCallback      callback,
                                                 gpointer                 user_data)
{
	g_return_if_fail (TRACKER_IS_SPARQL_CONNECTION (connection));
	g_return_if_fail (TRACKER_IS_RESOURCE (resource));
	g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (callback != NULL);

	TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->update_resource_async (connection,
	                                                                         graph,
	                                                                         resource,
	                                                                         cancellable,
	                                                                         callback,
	                                                                         user_data);
}

/**
 * tracker_sparql_connection_update_resource_finish:
 * @connection: a #TrackerSparqlConnection
 * @res: a #GAsyncResult with the result of the operation
 * @error: pointer to a #GError, or %NULL
 *
 * Finishes a tracker_sparql_connection_update_resource_async() operation.
 *
 * Returns: #TRUE if there were no errors.
 *
 * Since: 3.1
 **/
gboolean
tracker_sparql_connection_update_resource_finish (TrackerSparqlConnection  *connection,
                                                  GAsyncResult             *res,
                                                  GError                  **error)
{
	g_return_val_if_fail (TRACKER_IS_SPARQL_CONNECTION (connection), FALSE);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (res), FALSE);
	g_return_val_if_fail (!error || !*error, FALSE);

	return TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->update_resource_finish (connection,
	                                                                                 res,
	                                                                                 error);
}

/**
 * tracker_sparql_connection_get_namespace_manager:
 * @connection: a #TrackerSparqlConnection
 *
 * Retrieves a #TrackerNamespaceManager that contains all
 * prefixes in the ontology of @connection.
 *
 * Returns: (transfer none): a #TrackerNamespaceManager for this
 * connection. This object is owned by @connection and must not be freed.
 */
TrackerNamespaceManager *
tracker_sparql_connection_get_namespace_manager (TrackerSparqlConnection *connection)
{
	TrackerNamespaceManager *manager;

	g_return_val_if_fail (TRACKER_IS_SPARQL_CONNECTION (connection), NULL);

	manager = TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->get_namespace_manager (connection);
	tracker_namespace_manager_seal (manager);

	return manager;
}

/**
 * tracker_sparql_connection_query_statement:
 * @connection: a #TrackerSparqlConnection
 * @sparql: the SPARQL query
 * @cancellable: a #GCancellable used to cancel the operation, or %NULL
 * @error: a #TrackerSparqlError or %NULL if no error occured
 *
 * Prepares the given @sparql as a #TrackerSparqlStatement.
 *
 * Returns: (transfer full) (nullable): a prepared statement
 */
TrackerSparqlStatement *
tracker_sparql_connection_query_statement (TrackerSparqlConnection  *connection,
                                           const gchar              *sparql,
                                           GCancellable             *cancellable,
                                           GError                  **error)
{
	g_return_val_if_fail (TRACKER_IS_SPARQL_CONNECTION (connection), NULL);
	g_return_val_if_fail (sparql != NULL, NULL);
	g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), NULL);
	g_return_val_if_fail (!error || !*error, NULL);

	return TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->query_statement (connection,
	                                                                          sparql,
	                                                                          cancellable,
	                                                                          error);
}

/**
 * tracker_sparql_connection_create_notifier:
 * @connection: a #TrackerSparqlConnection
 *
 * Creates a new #TrackerNotifier to notify about changes in @connection.
 * See #TrackerNotifier documentation for information about how to use this
 * object.
 *
 * Returns: (transfer full): a newly created notifier. Free with g_object_unref()
 *          when no longer needed.
 **/
TrackerNotifier *
tracker_sparql_connection_create_notifier (TrackerSparqlConnection *connection)
{
	g_return_val_if_fail (TRACKER_IS_SPARQL_CONNECTION (connection), NULL);

	return TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->create_notifier (connection);
}

/**
 * tracker_sparql_connection_close:
 * @connection: a #TrackerSparqlConnection
 *
 * Closes a SPARQL connection. No other API calls than g_object_unref()
 * should happen after this call.
 *
 * This call is blocking. All pending updates will be flushed, and the
 * store databases will be closed orderly. All ongoing SELECT queries
 * will be cancelled. Notifiers will no longer emit events.
 *
 * Since: 3.0
 */
void
tracker_sparql_connection_close (TrackerSparqlConnection *connection)
{
	g_return_if_fail (TRACKER_IS_SPARQL_CONNECTION (connection));

	TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->close (connection);
}

/**
 * tracker_sparql_connection_close_async:
 * @connection: a #TrackerSparqlConnection
 * @cancellable: a #GCancellable, or %NULL
 * @callback: user-defined #GAsyncReadyCallback to be called when
 *            asynchronous operation is finished.
 * @user_data: user-defined data to be passed to @callback
 *
 * Closes a connection asynchronously. No other API calls than g_object_unref()
 * should happen after this call. See tracker_sparql_connection_close() for more
 * information.
 *
 * Since: 3.0
 **/
void
tracker_sparql_connection_close_async (TrackerSparqlConnection *connection,
                                       GCancellable            *cancellable,
                                       GAsyncReadyCallback      callback,
                                       gpointer                 user_data)
{
	g_return_if_fail (TRACKER_IS_SPARQL_CONNECTION (connection));

	TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->close_async (connection,
	                                                               cancellable,
	                                                               callback,
	                                                               user_data);
}

/**
 * tracker_sparql_connection_close_finish:
 * @connection: a #TrackerSparqlConnection
 * @res: the #GAsyncResult
 * @error: pointer to a #GError
 *
 * Finishes the asynchronous connection close.
 *
 * Returns: %FALSE if some error occurred, %TRUE otherwise
 *
 * Since: 3.0
 **/
gboolean
tracker_sparql_connection_close_finish (TrackerSparqlConnection  *connection,
                                        GAsyncResult             *res,
                                        GError                  **error)
{
	g_return_val_if_fail (TRACKER_IS_SPARQL_CONNECTION (connection), FALSE);

	return TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->close_finish (connection,
	                                                                       res, error);
}

/**
 * tracker_sparql_connection_create_batch:
 * @connection: a #TrackerSparqlConnection
 *
 * Creates a new batch to store and execute update commands. If the connection
 * is readonly or cannot issue SPARQL updates, %NULL will be returned.
 *
 * Returns: (transfer full): (nullable): A new #TrackerBatch
 **/
TrackerBatch *
tracker_sparql_connection_create_batch (TrackerSparqlConnection *connection)
{
	g_return_val_if_fail (TRACKER_IS_SPARQL_CONNECTION (connection), NULL);

	if (!TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->create_batch)
		return NULL;

	return TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->create_batch (connection);
}

/**
 * tracker_sparql_connection_load_statement_from_gresource:
 * @connection: a #TrackerSparqlConnection
 * @resource_path: the resource path of the file to parse.
 * @cancellable: a #GCancellable, or %NULL
 * @error: return location for an error, or %NULL
 *
 * Prepares a #TrackerSparqlStatement for the SPARQL query contained as a resource
 * file at @resource_path. SPARQL Query files typically have the .rq extension.
 *
 * Returns: (transfer full) (nullable): a prepared statement
 *
 * Since: 3.3
 **/
TrackerSparqlStatement *
tracker_sparql_connection_load_statement_from_gresource (TrackerSparqlConnection  *connection,
                                                         const gchar              *resource_path,
                                                         GCancellable             *cancellable,
                                                         GError                  **error)
{
	TrackerSparqlStatement *stmt;
	GBytes *query;

	g_return_val_if_fail (TRACKER_IS_SPARQL_CONNECTION (connection), NULL);
	g_return_val_if_fail (resource_path && *resource_path, NULL);
	g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), NULL);
	g_return_val_if_fail (!error || !*error, NULL);

	query = g_resources_lookup_data (resource_path,
	                                 G_RESOURCE_LOOKUP_FLAGS_NONE,
	                                 error);
	if (!query)
		return NULL;

	stmt = TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->query_statement (connection,
	                                                                          g_bytes_get_data (query,
	                                                                                            NULL),
	                                                                          cancellable,
	                                                                          error);
	g_bytes_unref (query);

	return stmt;
}

/**
 * tracker_sparql_connection_serialize_async:
 * @connection: a #TrackerSparqlConnection
 * @flags: serialization flags
 * @format: output RDF format
 * @query: SPARQL query
 * @cancellable: a #GCancellable
 * @callback: the #GAsyncReadyCallback called when the operation completes
 * @user_data: data passed to @callback
 *
 * Serializes data into the specified RDF format. @query must be either a
 * `DESCRIBE` or `CONSTRUCT` query. This is an asynchronous operation,
 * @callback will be invoked when the data is available for reading.
 *
 * The SPARQL endpoint may not support the specified format, in that case
 * an error will be raised.
 *
 * The @flags argument is reserved for future expansions, currently
 * %TRACKER_SERIALIZE_FLAGS_NONE must be passed.
 *
 * Since: 3.3
 **/
void
tracker_sparql_connection_serialize_async (TrackerSparqlConnection *connection,
                                           TrackerSerializeFlags    flags,
                                           TrackerRdfFormat         format,
                                           const gchar             *query,
                                           GCancellable            *cancellable,
                                           GAsyncReadyCallback      callback,
                                           gpointer                 user_data)
{
	g_return_if_fail (TRACKER_IS_SPARQL_CONNECTION (connection));
	g_return_if_fail (flags == TRACKER_SERIALIZE_FLAGS_NONE);
	g_return_if_fail (format < TRACKER_N_RDF_FORMATS);
	g_return_if_fail (query != NULL);
	g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (callback != NULL);

	TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->serialize_async (connection,
	                                                                   flags,
	                                                                   format,
	                                                                   query,
	                                                                   cancellable,
	                                                                   callback,
	                                                                   user_data);
}

/**
 * tracker_sparql_connection_serialize_finish:
 * @connection: a #TrackerSparqlConnection
 * @result: the #GAsyncResult
 * @error: location for returned errors, or %NULL
 *
 * Finishes a tracker_sparql_connection_serialize_async() operation.
 * In case of error, %NULL will be returned and @error will be set.
 *
 * Returns: (transfer full): a #GInputStream to read RDF content.
 *
 * Since: 3.3
 **/
GInputStream *
tracker_sparql_connection_serialize_finish (TrackerSparqlConnection  *connection,
                                            GAsyncResult             *result,
                                            GError                  **error)
{
	g_return_val_if_fail (TRACKER_IS_SPARQL_CONNECTION (connection), NULL);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);
	g_return_val_if_fail (!error || !*error, NULL);

	return TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->serialize_finish (connection,
	                                                                           result,
	                                                                           error);
}

/**
 * tracker_sparql_connection_deserialize_async:
 * @connection: a #TrackerSparqlConnection
 * @flags: deserialization flags
 * @format: RDF format of data in stream
 * @default_graph: default graph that will receive the RDF data
 * @stream: input stream with RDF data
 * @cancellable: a #GCancellable
 * @callback: the #GAsyncReadyCallback called when the operation completes
 * @user_data: data passed to @callback
 *
 * Incorporates the contents of the RDF data contained in @stream into the
 * data stored by @connection. This is an asynchronous operation,
 * @callback will be invoked when the data has been fully inserted to
 * @connection.
 *
 * RDF data will be inserted in the given @default_graph if one is provided,
 * or the default graph if @default_graph is %NULL. Any RDF data that has a
 * graph specified (e.g. using the `GRAPH` clause in the Trig format) will
 * be inserted in the specified graph instead of @default_graph.
 *
 * The @flags argument is reserved for future expansions, currently
 * %TRACKER_DESERIALIZE_FLAGS_NONE must be passed.
 *
 * Since: 3.4
 **/
void
tracker_sparql_connection_deserialize_async (TrackerSparqlConnection *connection,
                                             TrackerDeserializeFlags  flags,
                                             TrackerRdfFormat         format,
                                             const gchar             *default_graph,
                                             GInputStream            *stream,
                                             GCancellable            *cancellable,
                                             GAsyncReadyCallback      callback,
                                             gpointer                 user_data)
{
	g_return_if_fail (TRACKER_IS_SPARQL_CONNECTION (connection));
	g_return_if_fail (flags == TRACKER_DESERIALIZE_FLAGS_NONE);
	g_return_if_fail (format < TRACKER_N_RDF_FORMATS);
	g_return_if_fail (G_IS_INPUT_STREAM (stream));
	g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (callback != NULL);

	TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->deserialize_async (connection,
	                                                                     flags,
	                                                                     format,
	                                                                     default_graph,
	                                                                     stream,
	                                                                     cancellable,
	                                                                     callback,
	                                                                     user_data);
}

/**
 * tracker_sparql_connection_deserialize_finish:
 * @connection: a #TrackerSparqlConnection
 * @result: the #GAsyncResult
 * @error: location for returned errors, or %NULL
 *
 * Finishes a tracker_sparql_connection_deserialize_async() operation.
 * In case of error, %NULL will be returned and @error will be set.
 *
 * Returns: %TRUE if all data was inserted successfully.
 *
 * Since: 3.4
 **/
gboolean
tracker_sparql_connection_deserialize_finish (TrackerSparqlConnection  *connection,
                                              GAsyncResult             *result,
                                              GError                  **error)
{
	g_return_val_if_fail (TRACKER_IS_SPARQL_CONNECTION (connection), FALSE);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);
	g_return_val_if_fail (!error || !*error, FALSE);

	return TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->deserialize_finish (connection,
	                                                                             result,
	                                                                             error);
}

/**
 * tracker_sparql_connection_map_connection:
 * @connection: a #TrackerSparqlConnection
 * @handle_name: handle name for @service_connection
 * @service_connection: a #TrackerSparqlConnection to use from @connection
 *
 * Maps @service_connection so it is available as a "private:@handle_name" URI
 * in @connection. This can be accessed via the SERVICE SPARQL syntax in
 * queries from @connection. E.g.:
 *
 * ```sparql
 * SELECT ?u {
 *   SERVICE <private:other-connection> {
 *     ?u a rdfs:Resource
 *   }
 * }
 * ```
 *
 * This is useful to interrelate data from multiple
 * #TrackerSparqlConnection instances maintained by the same process,
 * without creating a public endpoint for @service_connection.
 *
 * @connection may only be a #TrackerSparqlConnection created via
 * tracker_sparql_connection_new() and tracker_sparql_connection_new_async().
 *
 * Since: 3.3
 **/
void
tracker_sparql_connection_map_connection (TrackerSparqlConnection *connection,
					  const gchar             *handle_name,
					  TrackerSparqlConnection *service_connection)
{
	g_return_if_fail (TRACKER_IS_SPARQL_CONNECTION (connection));
	g_return_if_fail (TRACKER_IS_SPARQL_CONNECTION (service_connection));
	g_return_if_fail (handle_name && *handle_name);

	if (!TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->map_connection)
		return;

	TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->map_connection (connection,
	                                                                  handle_name,
	                                                                  service_connection);
}

/**
 * tracker_sparql_connection_remote_new:
 * @uri_base: Base URI of the remote connection
 *
 * Connects to a remote SPARQL endpoint. The connection is made using the libsoup
 * HTTP library. The connection will normally use the http:// or https:// protocol.
 *
 * Returns: (transfer full): a new remote #TrackerSparqlConnection. Call
 * g_object_unref() on the object when no longer used.
 */
TrackerSparqlConnection *
tracker_sparql_connection_remote_new (const gchar *uri_base)
{
	return TRACKER_SPARQL_CONNECTION (tracker_remote_connection_new (uri_base));
}

/**
 * tracker_sparql_connection_new:
 * @flags: values from #TrackerSparqlConnectionFlags
 * @store: (nullable): the directory that contains the database as a #GFile, or %NULL
 * @ontology: (nullable): the directory that contains the database schemas as a #GFile, or %NULL
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @error: pointer to a #GError
 *
 * Creates or opens a database.
 *
 * This method should only be used for databases owned by the current process.
 * To connect to databases managed by other processes, use
 * tracker_sparql_connection_bus_new().
 *
 * If @store is %NULL, the database will be created in memory.
 *
 * The @ontologies parameter must point to a location containing suitable
 * `.ontology` files in Turtle format. These control the database schema that
 * is used. You can use the default Nepomuk ontologies by calling
 * tracker_sparql_get_ontology_nepomuk ().
 *
 * If you open an existing database using a different @ontology to the one it
 * was created with, Tracker will attempt to migrate the existing data to the
 * new schema. This may raise an error. In particular, not all migrations are
 * possible without causing data loss and Tracker will refuse to delete data
 * during a migration.
 *
 * You can also pass %NULL for @ontologies to mean "use the ontologies that the
 * database was created with". This will fail if the database doesn't already
 * exist.
 *
 * Returns: (transfer full): a new #TrackerSparqlConnection. Call
 * g_object_unref() on the object when no longer used.
 *
 * Since: 3.0
 */
TrackerSparqlConnection *
tracker_sparql_connection_new (TrackerSparqlConnectionFlags   flags,
                               GFile                         *store,
                               GFile                         *ontology,
                               GCancellable                  *cancellable,
                               GError                       **error)
{
	g_return_val_if_fail (!store || G_IS_FILE (store), NULL);
	g_return_val_if_fail (!ontology || G_IS_FILE (ontology), NULL);
	g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), NULL);
	g_return_val_if_fail (!error || !*error, NULL);

	return tracker_direct_connection_new (flags, store, ontology, error);
}

static void
new_async_cb (GObject      *source,
              GAsyncResult *res,
              gpointer      user_data)
{
	TrackerSparqlConnection *conn;
	GTask *task = user_data;
	GError *error = NULL;

	conn = tracker_direct_connection_new_finish (res, &error);

	if (conn)
		g_task_return_pointer (task, conn, g_object_unref);
	else
		g_task_return_error (task, error);
}

/**
 * tracker_sparql_connection_new_async:
 * @flags: values from #TrackerSparqlConnectionFlags
 * @store: (nullable): the directory that contains the database as a #GFile, or %NULL
 * @ontology: (nullable): the directory that contains the database schemas as a #GFile, or %NULL
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @callback: the #GAsyncReadyCallback called when the operation completes
 * @user_data: data passed to @callback
 *
 * Asynchronous version of tracker_sparql_connection_new().
 *
 * Since: 3.0
 */

void
tracker_sparql_connection_new_async (TrackerSparqlConnectionFlags  flags,
                                     GFile                        *store,
                                     GFile                        *ontology,
                                     GCancellable                 *cancellable,
                                     GAsyncReadyCallback           callback,
                                     gpointer                      user_data)
{
	GTask *task;

	g_return_if_fail (!store || G_IS_FILE (store));
	g_return_if_fail (!ontology || G_IS_FILE (ontology));
	g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

	task = g_task_new (NULL, cancellable, callback, user_data);
	g_task_set_source_tag (task, tracker_sparql_connection_new_async);

	tracker_direct_connection_new_async (flags, store, ontology, cancellable,
	                                     new_async_cb, task);
}

/**
 * tracker_sparql_connection_new_finish:
 * @result: the #GAsyncResult
 * @error: pointer to a #GError
 *
 * Completion function for tracker_sparql_connection_new_async().
 *
 * Since: 3.0
 */
TrackerSparqlConnection *
tracker_sparql_connection_new_finish (GAsyncResult  *res,
                                      GError       **error)
{
	g_return_val_if_fail (G_IS_TASK (res), NULL);
	g_return_val_if_fail (g_task_get_source_tag (G_TASK (res)) ==
	                      tracker_sparql_connection_new_async,
	                      NULL);

	return g_task_propagate_pointer (G_TASK (res), error);
}

/**
 * tracker_sparql_connection_bus_new:
 * @service_name: The name of the D-Bus service to connect to.
 * @object_path: (nullable): The path to the object, or %NULL to use the default.
 * @dbus_connection: (nullable): The #GDBusConnection to use, or %NULL to use the session bus
 * @error: pointer to a #GError
 *
 * Connects to a database owned by another process on the
 * local machine.
 *
 * Returns: (transfer full): a new #TrackerSparqlConnection. Call g_object_unref() on the
 * object when no longer used.
 *
 * Since: 3.0
 */
TrackerSparqlConnection *
tracker_sparql_connection_bus_new (const gchar      *service,
                                   const gchar      *object_path,
                                   GDBusConnection  *conn,
                                   GError          **error)
{
	g_return_val_if_fail (service != NULL, NULL);
	g_return_val_if_fail (!conn || G_IS_DBUS_CONNECTION (conn), NULL);
	g_return_val_if_fail (!error || !*error, NULL);

	if (!object_path)
		object_path = "/org/freedesktop/Tracker3/Endpoint";

	return tracker_bus_connection_new (service, object_path, conn, error);
}

static void
bus_new_cb (GObject      *source,
            GAsyncResult *res,
            gpointer      user_data)
{
	TrackerSparqlConnection *conn;
	GTask *task = user_data;
	GError *error = NULL;

	conn = tracker_bus_connection_new_finish (res, &error);

	if (conn)
		g_task_return_pointer (task, conn, g_object_unref);
	else
		g_task_return_error (task, error);

	g_object_unref (task);
}

/**
 * tracker_sparql_connection_bus_new_async:
 * @service_name: The name of the D-Bus service to connect to.
 * @object_path: (nullable): The path to the object, or %NULL to use the default.
 * @dbus_connection: (nullable): The #GDBusConnection to use, or %NULL to use the session bus
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @callback: the #GAsyncReadyCallback called when the operation completes
 * @user_data: data passed to @callback
 *
 * Connects to a database owned by another process on the
 * local machine. This is an asynchronous operation.
 *
 * Since: 3.1
 */
void
tracker_sparql_connection_bus_new_async (const gchar         *service,
                                         const gchar         *object_path,
                                         GDBusConnection     *conn,
                                         GCancellable        *cancellable,
                                         GAsyncReadyCallback  callback,
                                         gpointer             user_data)
{
	GTask *task;

	g_return_if_fail (service != NULL);
	g_return_if_fail (!conn || G_IS_DBUS_CONNECTION (conn));
	g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

	task = g_task_new (NULL, cancellable, callback, user_data);
	g_task_set_source_tag (task, tracker_sparql_connection_bus_new_async);

	if (!object_path)
		object_path = "/org/freedesktop/Tracker3/Endpoint";

	tracker_bus_connection_new_async (service, object_path, conn,
	                                  cancellable, bus_new_cb,
	                                  task);
}

/**
 * tracker_sparql_connection_bus_new_finish:
 * @result: the #GAsyncResult
 * @error: pointer to a #GError
 *
 * Completion function for tracker_sparql_connection_bus_new_async().
 *
 * Returns: (transfer full): a new #TrackerSparqlConnection. Call g_object_unref() on the
 * object when no longer used.
 *
 * Since: 3.1
 */
TrackerSparqlConnection *
tracker_sparql_connection_bus_new_finish (GAsyncResult  *result,
                                          GError       **error)
{
	g_return_val_if_fail (G_IS_TASK (result), NULL);
	g_return_val_if_fail (!error || !*error, NULL);
	g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) ==
	                      tracker_sparql_connection_bus_new_async,
	                      NULL);

	return g_task_propagate_pointer (G_TASK (result), error);
}
