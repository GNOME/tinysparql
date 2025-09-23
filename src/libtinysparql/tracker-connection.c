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
 * TrackerSparqlConnection:
 *
 * `TrackerSparqlConnection` holds a connection to a RDF triple store.
 *
 * This triple store may be of three types:
 *
 *  - Local to the process, created through [ctor@SparqlConnection.new].
 *  - A HTTP SPARQL endpoint over the network, created through
 *    [ctor@SparqlConnection.remote_new]
 *  - A DBus SPARQL endpoint owned by another process in the same machine, created
 *    through [ctor@SparqlConnection.bus_new]
 *
 * When creating a local triple store, it is required to give details about its
 * structure. This is done by passing a location to an ontology, see more
 * on how are [ontologies defined](ontologies.html). A local database may be
 * stored in a filesystem location, or it may reside in memory.
 *
 * A `TrackerSparqlConnection` is private to the calling process, it can be
 * exposed to other hosts/processes via a [class@Endpoint], see
 * [ctor@EndpointDBus.new] and [ctor@EndpointHttp.new].
 *
 * When issuing SPARQL queries and updates, it is recommended that these are
 * created through [class@SparqlStatement] to avoid the SPARQL
 * injection class of bugs, see [method@SparqlConnection.query_statement]
 * and [method@SparqlConnection.update_statement]. For SPARQL updates
 * it is also possible to use a "builder" approach to generate RDF data, see
 * [class@Resource]. It is also possible to create [class@SparqlStatement]
 * objects for SPARQL queries and updates from SPARQL strings embedded in a
 * [struct@Gio.Resource], see [method@SparqlConnection.load_statement_from_gresource].
 *
 * To get the best performance, it is recommended that SPARQL updates are clustered
 * through [class@Batch].
 *
 * `TrackerSparqlConnection` also offers a number of methods for the simple cases,
 * [method@SparqlConnection.query] may be used when there is a SPARQL
 * query string directly available, and the [method@SparqlConnection.update]
 * family of functions may be used for one-off updates. All functions have asynchronous
 * variants.
 *
 * When a SPARQL query is executed, a [class@SparqlCursor] will be obtained
 * to iterate over the query results.
 *
 * Depending on the ontology definition, `TrackerSparqlConnection` may emit
 * notifications whenever resources of certain types get insert, modified or
 * deleted from the triple store (see [nrl:notify](nrl-ontology.html#nrl:notify).
 * These notifications can be handled via a [class@Notifier] obtained with
 * [method@SparqlConnection.create_notifier].
 *
 * After done with a connection, it is recommended to call [method@SparqlConnection.close]
 * or [method@SparqlConnection.close_async] explicitly to cleanly close the
 * connection and prevent consistency checks on future runs. The triple store
 * connection will be implicitly closed when the `TrackerSparqlConnection` object
 * is disposed.
 *
 * A `TrackerSparqlConnection` may be used from multiple threads, asynchronous
 * updates are executed sequentially on arrival order, asynchronous
 * queries are dispatched in a thread pool.
 *
 * If you ever have the need to procedurally compose SPARQL query strings, consider
 * the use of [func@sparql_escape_string] for literal strings and
 * the [func@sparql_escape_uri] family of functions for URIs.
 */
#include "config.h"

#include "tracker-connection.h"
#include "tracker-private.h"
#include "tracker-debug.h"

#include "bus/tracker-bus.h"
#include "direct/tracker-direct.h"
#include "remote/tracker-remote.h"

typedef struct
{
	gboolean closing;
} TrackerSparqlConnectionPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (TrackerSparqlConnection, tracker_sparql_connection,
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

	/* Initialize GResources */
	tracker_ensure_resources ();
}

gboolean
tracker_sparql_connection_lookup_dbus_service (TrackerSparqlConnection  *connection,
                                               const gchar              *dbus_name,
                                               const gchar              *dbus_path,
                                               gchar                   **name,
                                               gchar                   **path)
{
	TrackerSparqlConnectionClass *connection_class;

	connection_class = TRACKER_SPARQL_CONNECTION_GET_CLASS (connection);
	if (!connection_class->lookup_dbus_service)
		return FALSE;

	return TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->lookup_dbus_service (connection,
	                                                                              dbus_name,
	                                                                              dbus_path,
	                                                                              name,
	                                                                              path);
}

gboolean
tracker_sparql_connection_set_error_on_closed (TrackerSparqlConnection  *connection,
                                               GError                  **error)
{
	TrackerSparqlConnectionPrivate *priv =
		tracker_sparql_connection_get_instance_private (connection);

	if (priv->closing) {
		g_set_error (error,
		             G_IO_ERROR,
		             G_IO_ERROR_CONNECTION_CLOSED,
		             "Connection is closed");
		return TRUE;
	}

	return FALSE;
}

gboolean
tracker_sparql_connection_report_async_error_on_closed (TrackerSparqlConnection *connection,
                                                        GAsyncReadyCallback      callback,
                                                        gpointer                 user_data)
{
	TrackerSparqlConnectionPrivate *priv =
		tracker_sparql_connection_get_instance_private (connection);

	if (priv->closing) {
		g_task_report_new_error (connection, callback, user_data, NULL,
		                         G_IO_ERROR,
		                         G_IO_ERROR_CONNECTION_CLOSED,
		                         "Connection is closed");
		return TRUE;
	}

	return FALSE;
}

/**
 * tracker_sparql_connection_query:
 * @connection: A `TrackerSparqlConnection`
 * @sparql: String containing the SPARQL query
 * @cancellable: (nullable): Optional [type@Gio.Cancellable]
 * @error: Error location
 *
 * Executes a SPARQL query on @connection.
 *
 * This method is synchronous and will block until the query
 * is executed. See [method@SparqlConnection.query_async]
 * for an asynchronous variant.
 *
 * If the query is partially built from user input or other
 * untrusted sources, special care is required about possible
 * SPARQL injection. In order to avoid it entirely, it is recommended
 * to use [class@SparqlStatement]. The function
 * [func@sparql_escape_string] exists as a last resort,
 * but its use is not recommended.
 *
 * Returns: (transfer full): a [class@SparqlCursor] with the results.
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

	if (tracker_sparql_connection_set_error_on_closed (connection, error))
		return NULL;

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
 * @connection: A `TrackerSparqlConnection`
 * @sparql: String containing the SPARQL query
 * @cancellable: (nullable): Optional [type@Gio.Cancellable]
 * @callback: User-defined [type@Gio.AsyncReadyCallback] to be called when
 *            the asynchronous operation is finished.
 * @user_data: User-defined data to be passed to @callback
 *
 * Executes asynchronously a SPARQL query on @connection
 *
 * If the query is partially built from user input or other
 * untrusted sources, special care is required about possible
 * SPARQL injection. In order to avoid it entirely, it is recommended
 * to use [class@SparqlStatement]. The function
 * [func@sparql_escape_string] exists as a last resort,
 * but its use is not recommended.
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

	if (tracker_sparql_connection_report_async_error_on_closed (connection,
	                                                            callback,
	                                                            user_data))
		return;

	TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->query_async (connection,
	                                                               sparql,
	                                                               cancellable,
	                                                               callback,
	                                                               user_data);
}

/**
 * tracker_sparql_connection_query_finish:
 * @connection: A `TrackerSparqlConnection`
 * @res: A [type@Gio.AsyncResult] with the result of the operation
 * @error: Error location
 *
 * Finishes the operation started with [method@SparqlConnection.query_async].
 *
 * Returns: (transfer full): a [class@SparqlCursor] with the results.
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
 * @connection: A `TrackerSparqlConnection`
 * @sparql: String containing the SPARQL update query
 * @cancellable: (nullable): Optional [type@Gio.Cancellable]
 * @error: Error location
 *
 * Executes a SPARQL update on @connection.
 *
 * This method is synchronous and will block until the update
 * is finished. See [method@SparqlConnection.update_async]
 * for an asynchronous variant.
 *
 * It is recommented to consider the usage of [class@Batch]
 * to cluster database updates. Frequent isolated SPARQL updates
 * through this method will have a degraded performance in comparison.
 *
 * If the query is partially built from user input or other
 * untrusted sources, special care is required about possible
 * SPARQL injection. In order to avoid it entirely, it is recommended
 * to use [class@SparqlStatement], or to build the SPARQL
 * input through [class@Resource]. The function
 * [func@sparql_escape_string] exists as a last resort,
 * but its use is not recommended.
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

	if (tracker_sparql_connection_set_error_on_closed (connection, error))
		return;

	TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->update (connection,
	                                                          sparql,
	                                                          cancellable,
	                                                          error);
}

/**
 * tracker_sparql_connection_update_async:
 * @connection: A `TrackerSparqlConnection`
 * @sparql: String containing the SPARQL update query
 * @cancellable: (nullable): Optional [type@Gio.Cancellable]
 * @callback: User-defined [type@Gio.AsyncReadyCallback] to be called when
 *            the asynchronous operation is finished.
 * @user_data: User-defined data to be passed to @callback
 *
 * Executes asynchronously a SPARQL update.
 *
 * It is recommented to consider the usage of [class@Batch]
 * to cluster database updates. Frequent isolated SPARQL updates
 * through this method will have a degraded performance in comparison.
 *
 * If the query is partially built from user input or other
 * untrusted sources, special care is required about possible
 * SPARQL injection. In order to avoid it entirely, it is recommended
 * to use [class@SparqlStatement], or to build the SPARQL
 * input through [class@Resource]. The function
 * [func@sparql_escape_string] exists as a last resort,
 * but its use is not recommended.
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

	if (tracker_sparql_connection_report_async_error_on_closed (connection,
	                                                            callback,
	                                                            user_data))
		return;

	TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->update_async (connection,
	                                                                sparql,
	                                                                cancellable,
	                                                                callback,
	                                                                user_data);
}

/**
 * tracker_sparql_connection_update_finish:
 * @connection: A `TrackerSparqlConnection`
 * @res: A [type@Gio.AsyncResult] with the result of the operation
 * @error: Error location
 *
 * Finishes the operation started with [method@SparqlConnection.update_async].
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
 * @connection: A `TrackerSparqlConnection`
 * @sparql: An array of strings containing the SPARQL update queries
 * @sparql_length: The amount of strings you pass as @sparql
 * @cancellable: (nullable): Optional [type@Gio.Cancellable]
 * @callback: User-defined [type@Gio.AsyncReadyCallback] to be called when
 *            the asynchronous operation is finished.
 * @user_data: User-defined data to be passed to @callback
 *
 * Executes asynchronously an array of SPARQL updates. All updates in the
 * array are handled within a single transaction.
 *
 * If the query is partially built from user input or other
 * untrusted sources, special care is required about possible
 * SPARQL injection. In order to avoid it entirely, it is recommended
 * to use [class@SparqlStatement], or to build the SPARQL
 * input through [class@Resource]. The function
 * [func@sparql_escape_string] exists as a last resort,
 * but its use is not recommended.
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

	if (tracker_sparql_connection_report_async_error_on_closed (connection,
	                                                            callback,
	                                                            user_data))
		return;

	if (!TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->update_array_async) {
		g_task_report_new_error (G_OBJECT (connection), callback, user_data,
		                         connection,
		                         TRACKER_SPARQL_ERROR,
		                         TRACKER_SPARQL_ERROR_UNSUPPORTED,
		                         "Updates unsupported by this connection");
		return;
	}

	TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->update_array_async (connection,
	                                                                      sparql,
	                                                                      sparql_length,
	                                                                      cancellable,
	                                                                      callback,
	                                                                      user_data);
}

/**
 * tracker_sparql_connection_update_array_finish:
 * @connection: A `TrackerSparqlConnection`
 * @res: A [type@Gio.AsyncResult] with the result of the operation
 * @error: Error location
 *
 * Finishes the operation started with [method@SparqlConnection.update_array_async].
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

	if (!TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->update_array_finish)
		return g_task_propagate_boolean (G_TASK (res), error);

	return TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->update_array_finish (connection,
	                                                                              res,
	                                                                              error);

}

/**
 * tracker_sparql_connection_update_blank:
 * @connection: A `TrackerSparqlConnection`
 * @sparql: String containing the SPARQL update query
 * @cancellable: (nullable): Optional [type@Gio.Cancellable]
 * @error: Error location
 *
 * Executes a SPARQL update and returns the names of the generated blank nodes.
 *
 * This method is synchronous and will block until the update
 * is finished. See [method@SparqlConnection.update_blank_async]
 * for an asynchronous variant.
 *
 * The @sparql query should be built with [class@Resource], or
 * its parts correctly escaped using [func@sparql_escape_string],
 * otherwise SPARQL injection is possible.
 *
 * The format string of the `GVariant` is `aaa{ss}` (an array of an array
 * of dictionaries). The first array represents each INSERT that may exist in
 * the SPARQL string. The second array represents each new node for a given
 * WHERE clause. The last array holds a string pair with the blank node name
 * (e.g. `foo` for the blank node `_:foo`) and the URN that was generated for
 * it. For most updates the first two outer arrays will only contain one item.
 *
 * Returns: a [type@GLib.Variant] with the generated URNs.
 *
 * Deprecated: 3.5: This function makes the expectation that blank nodes have
 * a durable name that persist. The SPARQL and RDF specs define a much more
 * reduced scope for blank node labels. This function advises a behavior that
 * goes against that reduced scope, and will directly make the returned values
 * meaningless if the #TRACKER_SPARQL_CONNECTION_FLAGS_ANONYMOUS_BNODES flag
 * is defined in the connection.
 *
 * Users that want names generated for them, should look for other methods
 * (e.g. IRIs containing UUIDv4 strings).
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

	if (tracker_sparql_connection_set_error_on_closed (connection, error))
		return NULL;

	return TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->update_blank (connection,
	                                                                       sparql,
	                                                                       cancellable,
	                                                                       error);
}

/**
 * tracker_sparql_connection_update_blank_async:
 * @connection: A `TrackerSparqlConnection`
 * @sparql: String containing the SPARQL update query
 * @cancellable: (nullable): Optional [type@Gio.Cancellable]
 * @callback: User-defined [type@Gio.AsyncReadyCallback] to be called when
 *            the asynchronous operation is finished.
 * @user_data: User-defined data to be passed to @callback
 *
 *
 * Executes asynchronously a SPARQL update and returns the names of the generated blank nodes.
 *
 * See the [method@SparqlConnection.update_blank] documentation to
 * learn the differences with [method@SparqlConnection.update].
 *
 * Deprecated: 3.5: See [method@SparqlConnection.update_blank].
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

	if (tracker_sparql_connection_report_async_error_on_closed (connection,
	                                                            callback,
	                                                            user_data))
		return;

	TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->update_blank_async (connection,
	                                                                      sparql,
	                                                                      cancellable,
	                                                                      callback,
	                                                                      user_data);
}

/**
 * tracker_sparql_connection_update_blank_finish:
 * @connection: A `TrackerSparqlConnection`
 * @res: A [type@Gio.AsyncResult] with the result of the operation
 * @error: Error location
 *
 * Finishes the operation started with [method@SparqlConnection.update_blank_async].
 *
 * This method returns the URNs of the generated nodes, if any. See the
 * [method@SparqlConnection.update_blank] documentation for the interpretation
 * of the returned [type@GLib.Variant].
 *
 * Returns: a [type@GLib.Variant] with the generated URNs.
 *
 * Deprecated: 3.5: See [method@SparqlConnection.update_blank].
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
 * @connection: A `TrackerSparqlConnection`
 * @graph: (nullable): RDF graph where the resource should be inserted/updated, or %NULL for the default graph
 * @resource: A [class@Resource]
 * @cancellable: (nullable): Optional [type@Gio.Cancellable]
 * @error: Error location
 *
 * Inserts a resource as described by @resource on the given @graph.
 *
 * This method is synchronous and will block until the update
 * is finished. See [method@SparqlConnection.update_resource_async]
 * for an asynchronous variant.
 *
 * It is recommented to consider the usage of [class@Batch]
 * to cluster database updates. Frequent isolated SPARQL updates
 * through this method will have a degraded performance in comparison.
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

	if (tracker_sparql_connection_set_error_on_closed (connection, error))
		return FALSE;

	if (!TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->update_resource) {
		g_set_error (error,
		             TRACKER_SPARQL_ERROR,
		             TRACKER_SPARQL_ERROR_UNSUPPORTED,
		             "Updates unsupported by this connection");
		return FALSE;
	}

	return TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->update_resource (connection,
	                                                                          graph,
	                                                                          resource,
	                                                                          cancellable,
	                                                                          error);
}

/**
 * tracker_sparql_connection_update_resource_async:
 * @connection: A `TrackerSparqlConnection`
 * @graph: (nullable): RDF graph where the resource should be inserted/updated, or %NULL for the default graph
 * @resource: A [class@Resource]
 * @cancellable: (nullable): Optional [type@Gio.Cancellable]
 * @callback: User-defined [type@Gio.AsyncReadyCallback] to be called when
 *            the asynchronous operation is finished.
 * @user_data: User-defined data to be passed to @callback
 *
 * Inserts asynchronously a resource as described by @resource on the given @graph.
 *
 * It is recommented to consider the usage of [class@Batch]
 * to cluster database updates. Frequent isolated SPARQL updates
 * through this method will have a degraded performance in comparison.
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

	if (tracker_sparql_connection_report_async_error_on_closed (connection,
	                                                            callback,
	                                                            user_data))
		return;

	if (!TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->update_resource_async) {
		g_task_report_new_error (G_OBJECT (connection), callback, user_data,
		                         connection,
		                         TRACKER_SPARQL_ERROR,
		                         TRACKER_SPARQL_ERROR_UNSUPPORTED,
		                         "Updates unsupported by this connection");
		return;
	}

	TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->update_resource_async (connection,
	                                                                         graph,
	                                                                         resource,
	                                                                         cancellable,
	                                                                         callback,
	                                                                         user_data);
}

/**
 * tracker_sparql_connection_update_resource_finish:
 * @connection: A `TrackerSparqlConnection`
 * @res: A [type@Gio.AsyncResult] with the result of the operation
 * @error: Error location
 *
 * Finishes the operation started with [method@SparqlConnection.update_resource_async].
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

	if (!TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->deserialize_finish)
		return g_task_propagate_boolean (G_TASK (res), error);

	return TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->update_resource_finish (connection,
	                                                                                 res,
	                                                                                 error);
}

/**
 * tracker_sparql_connection_get_namespace_manager:
 * @connection: A `TrackerSparqlConnection`
 *
 * Returns a [class@NamespaceManager] that contains all
 * prefixes in the ontology of @connection.
 *
 * Returns: (transfer none): a [class@NamespaceManager] with the prefixes of @connection.
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
 * @connection: A `TrackerSparqlConnection`
 * @sparql: The SPARQL query
 * @cancellable: (nullable): Optional [type@Gio.Cancellable]
 * @error: Error location
 *
 * Prepares the given `SELECT`/`ASK`/`DESCRIBE`/`CONSTRUCT` SPARQL query as a
 * [class@SparqlStatement].
 *
 * This prepared statement can be executed through [method@SparqlStatement.execute]
 * or [method@SparqlStatement.serialize_async] families of functions.
 *
 * Returns: (transfer full): A prepared statement
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

	if (tracker_sparql_connection_set_error_on_closed (connection, error))
		return NULL;

	return TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->query_statement (connection,
	                                                                          sparql,
	                                                                          cancellable,
	                                                                          error);
}

/**
 * tracker_sparql_connection_update_statement:
 * @connection: A `TrackerSparqlConnection`
 * @sparql: The SPARQL update
 * @cancellable: (nullable): Optional [type@Gio.Cancellable]
 * @error: Error location
 *
 * Prepares the given `INSERT`/`DELETE` SPARQL as a [class@SparqlStatement].
 *
 * This prepared statement can be executed through
 * the [method@SparqlStatement.update] family of functions.
 *
 * Returns: (transfer full): A prepared statement
 *
 * Since: 3.5
 */
TrackerSparqlStatement *
tracker_sparql_connection_update_statement (TrackerSparqlConnection  *connection,
                                            const gchar              *sparql,
                                            GCancellable             *cancellable,
                                            GError                  **error)
{
	g_return_val_if_fail (TRACKER_IS_SPARQL_CONNECTION (connection), NULL);
	g_return_val_if_fail (sparql != NULL, NULL);
	g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), NULL);
	g_return_val_if_fail (!error || !*error, NULL);

	if (tracker_sparql_connection_set_error_on_closed (connection, error))
		return NULL;

	if (!TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->update_statement) {
		g_set_error (error,
		             TRACKER_SPARQL_ERROR,
		             TRACKER_SPARQL_ERROR_UNSUPPORTED,
		             "Updates unsupported by this connection");
		return NULL;
	}

	return TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->update_statement (connection,
	                                                                           sparql,
	                                                                           cancellable,
	                                                                           error);
}

/**
 * tracker_sparql_connection_create_notifier:
 * @connection: A `TrackerSparqlConnection`
 *
 * Creates a new [class@Notifier] to receive notifications about changes in @connection.
 *
 * See [class@Notifier] documentation for information about how to use this
 * object.
 *
 * Connections to HTTP endpoints will return %NULL.
 *
 * Returns: (transfer full) (nullable): A newly created notifier.
 **/
TrackerNotifier *
tracker_sparql_connection_create_notifier (TrackerSparqlConnection *connection)
{
	g_return_val_if_fail (TRACKER_IS_SPARQL_CONNECTION (connection), NULL);

	if (!TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->create_notifier)
		return NULL;

	return TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->create_notifier (connection);
}

/**
 * tracker_sparql_connection_close:
 * @connection: A `TrackerSparqlConnection`
 *
 * Closes a SPARQL connection.
 *
 * No other API calls than g_object_unref() should happen after this call.
 *
 * This call is blocking. All pending updates will be flushed, and the
 * store databases will be closed orderly. All ongoing SELECT queries
 * will be cancelled. Notifiers will no longer emit events.
 */
void
tracker_sparql_connection_close (TrackerSparqlConnection *connection)
{
	TrackerSparqlConnectionPrivate *priv =
		tracker_sparql_connection_get_instance_private (connection);

	g_return_if_fail (TRACKER_IS_SPARQL_CONNECTION (connection));

	priv->closing = TRUE;

	TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->close (connection);
}

/**
 * tracker_sparql_connection_close_async:
 * @connection: A `TrackerSparqlConnection`
 * @cancellable: (nullable): Optional [type@Gio.Cancellable]
 * @callback: User-defined [type@Gio.AsyncReadyCallback] to be called when
 *            the asynchronous operation is finished.
 * @user_data: User-defined data to be passed to @callback
 *
 * Closes a SPARQL connection asynchronously.
 *
 * No other API calls than g_object_unref() should happen after this call.
 **/
void
tracker_sparql_connection_close_async (TrackerSparqlConnection *connection,
                                       GCancellable            *cancellable,
                                       GAsyncReadyCallback      callback,
                                       gpointer                 user_data)
{
	TrackerSparqlConnectionPrivate *priv =
		tracker_sparql_connection_get_instance_private (connection);

	g_return_if_fail (TRACKER_IS_SPARQL_CONNECTION (connection));

	priv->closing = TRUE;

	TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->close_async (connection,
	                                                               cancellable,
	                                                               callback,
	                                                               user_data);
}

/**
 * tracker_sparql_connection_close_finish:
 * @connection: A `TrackerSparqlConnection`
 * @res: A [type@Gio.AsyncResult] with the result of the operation
 * @error: Error location
 *
 * Finishes the operation started with [method@SparqlConnection.close_async].
 *
 * Returns: %FALSE if some error occurred, %TRUE otherwise
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
 * @connection: a `TrackerSparqlConnection`
 *
 * Creates a new [class@Batch] to store and execute SPARQL updates.
 *
 * If the connection is readonly or cannot issue SPARQL updates, %NULL will be returned.
 *
 * Returns: (transfer full): (nullable): A new [class@Batch]
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
 * @connection: A `TrackerSparqlConnection`
 * @resource_path: The resource path of the file to parse.
 * @cancellable: (nullable): Optional [type@Gio.Cancellable]
 * @error: Error location
 *
 * Prepares a [class@SparqlStatement] for the SPARQL contained as a [struct@Gio.Resource]
 * file at @resource_path.
 *
 * SPARQL Query files typically have the .rq extension. This will use
 * [method@SparqlConnection.query_statement] or [method@SparqlConnection.update_statement]
 * underneath to indistinctly return SPARQL query or update statements.
 *
 * Returns: (transfer full): A prepared statement
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
	GError *inner_error1 = NULL, *inner_error2 = NULL;

	g_return_val_if_fail (TRACKER_IS_SPARQL_CONNECTION (connection), NULL);
	g_return_val_if_fail (resource_path && *resource_path, NULL);
	g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), NULL);
	g_return_val_if_fail (!error || !*error, NULL);

	if (tracker_sparql_connection_set_error_on_closed (connection, error))
		return NULL;

	query = g_resources_lookup_data (resource_path,
	                                 G_RESOURCE_LOOKUP_FLAGS_NONE,
	                                 error);
	if (!query)
		return NULL;

	stmt = TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->query_statement (connection,
	                                                                          g_bytes_get_data (query,
	                                                                                            NULL),
	                                                                          cancellable,
	                                                                          &inner_error1);

	if (inner_error1 && TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->update_statement) {
		stmt = TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->update_statement (connection,
		                                                                           g_bytes_get_data (query,
		                                                                                             NULL),
		                                                                           cancellable,
		                                                                           &inner_error2);
		if (inner_error1 && inner_error2) {
			/* Pick one */
			g_propagate_error (error, inner_error1);
			g_clear_error (&inner_error2);
		} else {
			g_clear_error (&inner_error1);
		}
	}

	g_bytes_unref (query);

	return stmt;
}

/**
 * tracker_sparql_connection_serialize_async:
 * @connection: A `TrackerSparqlConnection`
 * @flags: Serialization flags
 * @format: Output RDF format
 * @query: SPARQL query
 * @cancellable: (nullable): Optional [type@Gio.Cancellable]
 * @callback: User-defined [type@Gio.AsyncReadyCallback] to be called when
 *            the asynchronous operation is finished.
 * @user_data: User-defined data to be passed to @callback
 *
 * Serializes a `DESCRIBE` or `CONSTRUCT` query into the specified RDF format.
 *
 * This is an asynchronous operation, @callback will be invoked when
 * the data is available for reading.
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

	if (tracker_sparql_connection_report_async_error_on_closed (connection,
	                                                            callback,
	                                                            user_data))
		return;

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
 * @connection: A `TrackerSparqlConnection`
 * @result: A [type@Gio.AsyncResult] with the result of the operation
 * @error: Error location
 *
 * Finishes the operation started with [method@SparqlConnection.serialize_async].
 *
 * Returns: (transfer full): A [class@Gio.InputStream] to read RDF content.
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
 * @connection: A `TrackerSparqlConnection`
 * @flags: Deserialization flags
 * @format: RDF format of data in stream
 * @default_graph: Default graph that will receive the RDF data
 * @stream: Input stream with RDF data
 * @cancellable: (nullable): Optional [type@Gio.Cancellable]
 * @callback: User-defined [type@Gio.AsyncReadyCallback] to be called when
 *            the asynchronous operation is finished.
 * @user_data: User-defined data to be passed to @callback
 *
 * Loads the RDF data contained in @stream into the given @connection.

 * This is an asynchronous operation, @callback will be invoked when the
 * data has been fully inserted to @connection.
 *
 * The RDF data will be inserted in the given @default_graph if one is provided,
 * or the anonymous graph if @default_graph is %NULL. Any RDF data that has a
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

	if (tracker_sparql_connection_report_async_error_on_closed (connection,
	                                                            callback,
	                                                            user_data))
		return;

	if (!TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->deserialize_async) {
		g_task_report_new_error (G_OBJECT (connection), callback, user_data,
		                         connection,
		                         TRACKER_SPARQL_ERROR,
		                         TRACKER_SPARQL_ERROR_UNSUPPORTED,
		                         "Updates unsupported by this connection");
		return;
	}

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
 * @connection: A `TrackerSparqlConnection`
 * @result: A [type@Gio.AsyncResult] with the result of the operation
 * @error: Error location
 *
 * Finishes the operation started with [method@SparqlConnection.deserialize_async].
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

	if (!TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->deserialize_finish)
		return g_task_propagate_boolean (G_TASK (result), error);

	return TRACKER_SPARQL_CONNECTION_GET_CLASS (connection)->deserialize_finish (connection,
	                                                                             result,
	                                                                             error);
}

/**
 * tracker_sparql_connection_map_connection:
 * @connection: A `TrackerSparqlConnection`
 * @handle_name: Handle name for @service_connection
 * @service_connection: a `TrackerSparqlConnection` to use from @connection
 *
 * Maps a `TrackerSparqlConnection` onto another through a `private:@handle_name` URI.
 *
 * This can be accessed via the SERVICE SPARQL syntax in
 * queries from @connection. E.g.:
 *
 * ```c
 * tracker_sparql_connection_map_connection (connection,
 *                                           "other-connection",
 *                                           other_connection);
 * ```
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
 * `TrackerSparqlConnection` instances maintained by the same process,
 * without creating a public endpoint for @service_connection.
 *
 * @connection may only be a `TrackerSparqlConnection` created via
 * [ctor@SparqlConnection.new] and [func@SparqlConnection.new_async].
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
 * Creates a connection to a remote HTTP SPARQL endpoint.
 *
 * The connection is made using the libsoup HTTP library. The connection will
 * normally use the `https://` or `http://` protocols.
 *
 * Returns: (transfer full): a new remote `TrackerSparqlConnection`.
 */
TrackerSparqlConnection *
tracker_sparql_connection_remote_new (const gchar *uri_base)
{
	return TRACKER_SPARQL_CONNECTION (tracker_remote_connection_new (uri_base));
}

/**
 * tracker_sparql_connection_new:
 * @flags: Connection flags to define the SPARQL connection behavior
 * @store: (nullable): The database location as a [iface@Gio.File], or %NULL
 * @ontology: (nullable): The directory that contains the database schemas as a [iface@Gio.File], or %NULL
 * @cancellable: (nullable): Optional [type@Gio.Cancellable]
 * @error: Error location
 *
 * Creates or opens a process-local database.
 *
 * This method should only be used for databases owned by the current process.
 * To connect to databases managed by other processes, use
 * [ctor@SparqlConnection.bus_new].
 *
 * The @store argument determines where in the local filesystem the database
 * will be stored. If @store is %NULL, the database will be created in memory.
 * Traditionally, @store describes a directory where a file named `meta.db`
 * contains the data. Starting with version 3.10, if the basename of @store
 * contains a `.` extension separator, it will be considered as the database
 * file itself.
 *
 * If defined, the @ontology argument must point to a location containing
 * suitable `.ontology` files in Turtle format. These define the structure of
 * the triple store. You can learn more about [ontologies](ontologies.html),
 * or you can use the stock Nepomuk ontologies by calling
 * [func@sparql_get_ontology_nepomuk].
 *
 * If opening an existing database, it is possible to pass %NULL as the
 * @ontology location, the ontology will be introspected from the database.
 * Passing a %NULL @ontology will raise an error if the database does not exist.
 *
 * If a database is opened without the `READONLY` [flags@SparqlConnectionFlags]
 * flag enabled, and the given @ontology holds differences with the current
 * data layout, migration to the new structure will be attempted. This operation
 * may raise an error. In particular, not all migrations are possible without
 * causing data loss and Tracker will refuse to delete data during a migration.
 * The database is always left in a consistent state, either prior or posterior
 * to migration.
 *
 * Operations on a [class@SparqlConnection] resulting on a
 * `CORRUPT` [error@SparqlError] will have the event recorded
 * persistently through a `.$DB_BASENAME.corrupted` file alongside the database file.
 * If the database is opened without the `READONLY` [flags@SparqlConnectionFlags]
 * flag enabled and the file is found, this constructor will attempt to repair the
 * database. In that situation, this constructor will either return a valid
 * [class@SparqlConnection] if the database was repaired successfully, or
 * raise a `CORRUPT` [error@SparqlError] error if the database remains
 * corrupted.
 *
 * It is considered a developer error to ship ontologies that contain format
 * errors, or that fail at migrations.
 *
 * It is encouraged to use `resource:///` URI locations for @ontology wherever
 * possible, so the triple store structure is tied to the executable binary,
 * and in order to minimize disk seeks during `TrackerSparqlConnection`
 * initialization.
 *
 * Returns: (transfer full): a new `TrackerSparqlConnection`.
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
 * @flags: Connection flags to define the SPARQL connection behavior
 * @store: (nullable): The database location as a [iface@Gio.File], or %NULL
 * @ontology: (nullable): The directory that contains the database schemas as a [iface@Gio.File], or %NULL
 * @cancellable: (nullable): Optional [type@Gio.Cancellable]
 * @callback: User-defined [type@Gio.AsyncReadyCallback] to be called when
 *            the asynchronous operation is finished.
 * @user_data: User-defined data to be passed to @callback
 *
 * Creates or opens a process-local database asynchronously.
 *
 * See [ctor@SparqlConnection.new] for more information.
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
 * @result: A [type@Gio.AsyncResult] with the result of the operation
 * @error: Error location
 *
 * Finishes the operation started with [func@SparqlConnection.new_async].
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
 * @service_name (nullable): The name of the D-Bus service to connect to, or %NULL if not using a message bus.
 * @object_path: (nullable): The path to the object, or %NULL to use the default.
 * @dbus_connection: (nullable): The [type@Gio.DBusConnection] to use, or %NULL to use the session bus
 * @error: Error location
 *
 * Connects to a database owned by another process on the
 * local machine via DBus.
 *
 * When using a message bus (session/system), the @service_name argument will
 * be used to describe the remote endpoint, either by unique or well-known D-Bus
 * names. If not using a message bus (e.g. peer-to-peer D-Bus connections) the
 * @service_name may be %NULL.
 *
 * The D-Bus object path of the remote endpoint will be given through
 * @object_path, %NULL may be used to use the default
 * `/org/freedesktop/Tracker3/Endpoint` path.
 *
 * The D-Bus connection used to set up the connection may be given through
 * the @dbus_connection argument. Using %NULL will resort to the default session
 * bus.
 *
 * Returns: (transfer full): a new `TrackerSparqlConnection`.
 */
TrackerSparqlConnection *
tracker_sparql_connection_bus_new (const gchar      *service,
                                   const gchar      *object_path,
                                   GDBusConnection  *conn,
                                   GError          **error)
{
	g_return_val_if_fail (!conn || G_IS_DBUS_CONNECTION (conn), NULL);
	g_return_val_if_fail (!error || !*error, NULL);
	g_return_val_if_fail ((service == NULL && conn &&
	                       (g_dbus_connection_get_flags (conn) & G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION) == 0) ||
	                      (service != NULL && g_dbus_is_name (service)), NULL);

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
 * @dbus_connection: (nullable): The [class@Gio.DBusConnection] to use, or %NULL to use the session bus
 * @cancellable: (nullable): Optional [type@Gio.Cancellable]
 * @callback: User-defined [type@Gio.AsyncReadyCallback] to be called when
 *            the asynchronous operation is finished.
 * @user_data: User-defined data to be passed to @callback
 *
 * Connects asynchronously to a database owned by another process on the
 * local machine via DBus.
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
 * @result: A [type@Gio.AsyncResult] with the result of the operation
 * @error: Error location
 *
 * Finishes the operation started with [func@SparqlConnection.bus_new_async].
 *
 * Returns: (transfer full): a new `TrackerSparqlConnection`.
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
