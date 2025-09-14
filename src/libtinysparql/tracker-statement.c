/*
 * Copyright (C) 2018, Red Hat Ltd.
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
 * TrackerSparqlStatement:
 *
 * `TrackerSparqlStatement` represents a prepared statement for a SPARQL query.
 *
 * The SPARQL query will be internally compiled into the format that is most
 * optimal to execute the query many times. For connections created
 * through [ctor@SparqlConnection.new] that will be a
 * SQLite compiled statement.
 *
 * The SPARQL query may contain parameterized variables expressed via the
 * `~` prefix in the SPARQL syntax (e.g. `~var`), these may happen anywhere
 * in the SPARQL where a literal or variable would typically happen. These
 * parameterized variables may be mapped to arbitrary values prior to
 * execution. The `TrackerSparqlStatement` may be reused for future
 * queries with different values.
 *
 * The argument bindings may be changed through the [method@SparqlStatement.bind_int],
 * [method@SparqlStatement.bind_int], etc... family of functions. Those functions
 * receive a @name argument corresponding for the variable name in the SPARQL query
 * (eg. `"var"` for `~var`) and a value to map the variable to.
 *
 * Once all arguments have a value, the query may be executed through
 * [method@SparqlStatement.execute_async] or [method@SparqlStatement.execute].
 *
 * It is possible to use any `TrackerSparqlStatement` from other threads than
 * the one it was created from. However, binding values and executing the
 * statement must only happen from one thread at a time. It is possible to reuse
 * the `TrackerSparqlStatement` right after [method@SparqlStatement.execute_async]
 * was called, there is no need to wait for [method@SparqlStatement.execute_finish].
 *
 * In some circumstances, it is possible that the query needs to be recompiled
 * from the SPARQL source. This will happen transparently.
 */
#include "config.h"

#include "tracker-statement.h"
#include "tracker-private.h"

enum {
	PROP_0,
	PROP_CONNECTION,
	PROP_SPARQL,
	N_PROPS
};

static GParamSpec *props[N_PROPS];

typedef struct {
	TrackerSparqlConnection *connection;
	gchar *sparql;
} TrackerSparqlStatementPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (TrackerSparqlStatement,
                                     tracker_sparql_statement,
                                     G_TYPE_OBJECT)

static void
tracker_sparql_statement_init (TrackerSparqlStatement *stmt)
{
}

static void
tracker_sparql_statement_finalize (GObject *object)
{
	TrackerSparqlStatement *stmt = TRACKER_SPARQL_STATEMENT (object);
	TrackerSparqlStatementPrivate *priv = tracker_sparql_statement_get_instance_private (stmt);

	g_clear_object (&priv->connection);
	g_free (priv->sparql);
	G_OBJECT_CLASS (tracker_sparql_statement_parent_class)->finalize (object);
}

static void
tracker_sparql_statement_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
	TrackerSparqlStatement *stmt = TRACKER_SPARQL_STATEMENT (object);
	TrackerSparqlStatementPrivate *priv = tracker_sparql_statement_get_instance_private (stmt);

	switch (prop_id) {
	case PROP_CONNECTION:
		priv->connection = g_value_dup_object (value);
		break;
	case PROP_SPARQL:
		priv->sparql = g_value_dup_string (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
tracker_sparql_statement_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
	TrackerSparqlStatement *stmt = TRACKER_SPARQL_STATEMENT (object);
	TrackerSparqlStatementPrivate *priv = tracker_sparql_statement_get_instance_private (stmt);

	switch (prop_id) {
	case PROP_CONNECTION:
		g_value_set_object (value, priv->connection);
		break;
	case PROP_SPARQL:
		g_value_set_string (value, priv->sparql);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
tracker_sparql_statement_class_init (TrackerSparqlStatementClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_sparql_statement_finalize;
	object_class->set_property = tracker_sparql_statement_set_property;
	object_class->get_property = tracker_sparql_statement_get_property;

	/**
	 * TrackerSparqlStatement:connection:
	 *
	 * The [class@SparqlConnection] the statement was created for.
	 */
	props[PROP_CONNECTION] =
		g_param_spec_object ("connection",
		                     "connection",
		                     "connection",
		                     TRACKER_TYPE_SPARQL_CONNECTION,
				     G_PARAM_CONSTRUCT_ONLY |
		                     G_PARAM_STATIC_STRINGS |
		                     G_PARAM_READABLE |
		                     G_PARAM_WRITABLE);
	/**
	 * TrackerSparqlStatement:sparql:
	 *
	 * SPARQL query stored in this statement.
	 */
	props[PROP_SPARQL] =
		g_param_spec_string ("sparql",
		                     "sparql",
		                     "sparql",
		                     NULL,
				     G_PARAM_CONSTRUCT_ONLY |
		                     G_PARAM_STATIC_STRINGS |
		                     G_PARAM_READABLE |
				     G_PARAM_WRITABLE);

	g_object_class_install_properties (object_class, N_PROPS, props);
}

/**
 * tracker_sparql_statement_get_connection:
 * @stmt: a `TrackerSparqlStatement`
 *
 * Returns the [class@SparqlConnection] that this statement was created for.
 *
 * Returns: (transfer none): The SPARQL connection of this statement.
 **/
TrackerSparqlConnection *
tracker_sparql_statement_get_connection (TrackerSparqlStatement *stmt)
{
	TrackerSparqlStatementPrivate *priv = tracker_sparql_statement_get_instance_private (stmt);

	g_return_val_if_fail (TRACKER_IS_SPARQL_STATEMENT (stmt), NULL);

	return priv->connection;
}

/**
 * tracker_sparql_statement_get_sparql:
 * @stmt: a `TrackerSparqlStatement`
 *
 * Returns the SPARQL string that this prepared statement holds.
 *
 * Returns: The contained SPARQL query
 **/
const gchar *
tracker_sparql_statement_get_sparql (TrackerSparqlStatement *stmt)
{
	TrackerSparqlStatementPrivate *priv = tracker_sparql_statement_get_instance_private (stmt);

	g_return_val_if_fail (TRACKER_IS_SPARQL_STATEMENT (stmt), NULL);

	return priv->sparql;
}

/**
 * tracker_sparql_statement_bind_boolean:
 * @stmt: a `TrackerSparqlStatement`
 * @name: variable name
 * @value: value
 *
 * Binds the boolean @value to the parameterized variable given by @name.
 */
void
tracker_sparql_statement_bind_boolean (TrackerSparqlStatement *stmt,
                                       const gchar            *name,
                                       gboolean                value)
{
	g_return_if_fail (TRACKER_IS_SPARQL_STATEMENT (stmt));
	g_return_if_fail (name != NULL);

	TRACKER_SPARQL_STATEMENT_GET_CLASS (stmt)->bind_boolean (stmt,
	                                                         name,
	                                                         value);
}

/**
 * tracker_sparql_statement_bind_int:
 * @stmt: a `TrackerSparqlStatement`
 * @name: variable name
 * @value: value
 *
 * Binds the integer @value to the parameterized variable given by @name.
 */
void
tracker_sparql_statement_bind_int (TrackerSparqlStatement *stmt,
                                   const gchar            *name,
                                   gint64                  value)
{
	g_return_if_fail (TRACKER_IS_SPARQL_STATEMENT (stmt));
	g_return_if_fail (name != NULL);

	TRACKER_SPARQL_STATEMENT_GET_CLASS (stmt)->bind_int (stmt,
	                                                     name,
	                                                     value);
}

/**
 * tracker_sparql_statement_bind_double:
 * @stmt: a `TrackerSparqlStatement`
 * @name: variable name
 * @value: value
 *
 * Binds the double @value to the parameterized variable given by @name.
 */
void
tracker_sparql_statement_bind_double (TrackerSparqlStatement *stmt,
                                      const gchar            *name,
                                      gdouble                 value)
{
	g_return_if_fail (TRACKER_IS_SPARQL_STATEMENT (stmt));
	g_return_if_fail (name != NULL);

	TRACKER_SPARQL_STATEMENT_GET_CLASS (stmt)->bind_double (stmt,
	                                                        name,
	                                                        value);
}

/**
 * tracker_sparql_statement_bind_string:
 * @stmt: a `TrackerSparqlStatement`
 * @name: variable name
 * @value: value
 *
 * Binds the string @value to the parameterized variable given by @name.
 */
void
tracker_sparql_statement_bind_string (TrackerSparqlStatement *stmt,
                                      const gchar            *name,
                                      const gchar            *value)
{
	g_return_if_fail (TRACKER_IS_SPARQL_STATEMENT (stmt));
	g_return_if_fail (name != NULL);
	g_return_if_fail (value != NULL);

	TRACKER_SPARQL_STATEMENT_GET_CLASS (stmt)->bind_string (stmt,
	                                                        name,
	                                                        value);
}

/**
 * tracker_sparql_statement_bind_datetime:
 * @stmt: a `TrackerSparqlStatement`
 * @name: variable name
 * @value: value
 *
 * Binds the [type@GLib.DateTime] @value to the parameterized variable given by @name.
 *
 * Since: 3.2
 */

void
tracker_sparql_statement_bind_datetime (TrackerSparqlStatement *stmt,
                                        const gchar            *name,
                                        GDateTime              *value)
{
	g_return_if_fail (TRACKER_IS_SPARQL_STATEMENT (stmt));
	g_return_if_fail (name != NULL);
	g_return_if_fail (value != NULL);

	TRACKER_SPARQL_STATEMENT_GET_CLASS (stmt)->bind_datetime (stmt,
	                                                          name,
	                                                          value);
}

/**
 * tracker_sparql_statement_bind_langstring:
 * @stmt: a `TrackerSparqlStatement`
 * @name: variable name
 * @value: value
 * @langtag: language tag
 *
 * Binds the @value to the parameterized variable given by @name, tagged
 * with the language defined by @langtag. The language tag should follow
 * [RFC 5646](https://www.rfc-editor.org/rfc/rfc5646.html). The parameter
 * will be represented as a [`rdf:langString`](rdf-ontology.html#rdf:langString).
 *
 * Since: 3.7
 **/
void
tracker_sparql_statement_bind_langstring (TrackerSparqlStatement *stmt,
                                          const gchar            *name,
                                          const gchar            *value,
                                          const gchar            *langtag)
{
	g_return_if_fail (TRACKER_IS_SPARQL_STATEMENT (stmt));
	g_return_if_fail (name != NULL);
	g_return_if_fail (value != NULL);
	g_return_if_fail (langtag != NULL);

	TRACKER_SPARQL_STATEMENT_GET_CLASS (stmt)->bind_langstring (stmt,
	                                                            name,
	                                                            value,
	                                                            langtag);
}

/**
 * tracker_sparql_statement_execute:
 * @stmt: a `TrackerSparqlStatement`
 * @cancellable: (nullable): Optional [type@Gio.Cancellable]
 * @error: Error location
 *
 * Executes the `SELECT` or `ASK` SPARQL query with the currently bound values.
 *
 * This function also works for `DESCRIBE` and `CONSTRUCT` queries that
 * retrieve data from the triple store. These query forms that return
 * RDF data are however more useful together with [method@SparqlStatement.serialize_async].
 *
 * This function should only be called on `TrackerSparqlStatement` objects
 * obtained through [method@SparqlConnection.query_statement] or
 * SELECT/CONSTRUCT/DESCRIBE statements loaded through
 * [method@SparqlConnection.load_statement_from_gresource].
 * An error will be raised if this method is called on a `INSERT` or `DELETE`
 * SPARQL query.
 *
 * Returns: (transfer full): A `TrackerSparqlCursor` with the query results.
 */
TrackerSparqlCursor *
tracker_sparql_statement_execute (TrackerSparqlStatement  *stmt,
                                  GCancellable            *cancellable,
                                  GError                 **error)
{
	TrackerSparqlStatementPrivate *priv =
		tracker_sparql_statement_get_instance_private (stmt);
	TrackerSparqlCursor *cursor;

	g_return_val_if_fail (TRACKER_IS_SPARQL_STATEMENT (stmt), NULL);
	g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), NULL);
	g_return_val_if_fail (!error || !*error, NULL);

	if (tracker_sparql_connection_set_error_on_closed (priv->connection, error))
		return NULL;

	cursor = TRACKER_SPARQL_STATEMENT_GET_CLASS (stmt)->execute (stmt,
	                                                             cancellable,
	                                                             error);
	if (cursor)
		tracker_sparql_cursor_set_connection (cursor, priv->connection);

	return cursor;
}

/**
 * tracker_sparql_statement_execute_async:
 * @stmt: a `TrackerSparqlStatement`
 * @cancellable: (nullable): Optional [type@Gio.Cancellable]
 * @callback: user-defined [type@Gio.AsyncReadyCallback] to be called when
 *            the asynchronous operation is finished.
 * @user_data: user-defined data to be passed to @callback
 *
 * Executes asynchronously the `SELECT` or `ASK` SPARQL query with the currently bound values.
 *
 * This function also works for `DESCRIBE` and `CONSTRUCT` queries that
 * retrieve data from the triple store. These query forms that return
 * RDF data are however more useful together with [method@SparqlStatement.serialize_async].
 *
 * This function should only be called on `TrackerSparqlStatement` objects
 * obtained through [method@SparqlConnection.query_statement] or
 * SELECT/CONSTRUCT/DESCRIBE statements loaded through
 * [method@SparqlConnection.load_statement_from_gresource].
 * An error will be raised if this method is called on a `INSERT` or `DELETE`
 * SPARQL query.
 */
void
tracker_sparql_statement_execute_async (TrackerSparqlStatement *stmt,
                                        GCancellable           *cancellable,
                                        GAsyncReadyCallback     callback,
                                        gpointer                user_data)
{
	TrackerSparqlStatementPrivate *priv =
		tracker_sparql_statement_get_instance_private (stmt);

	g_return_if_fail (TRACKER_IS_SPARQL_STATEMENT (stmt));
	g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

	if (tracker_sparql_connection_report_async_error_on_closed (priv->connection,
	                                                            callback,
	                                                            user_data))
		return;

	TRACKER_SPARQL_STATEMENT_GET_CLASS (stmt)->execute_async (stmt,
	                                                          cancellable,
	                                                          callback,
	                                                          user_data);
}

/**
 * tracker_sparql_statement_execute_finish:
 * @stmt: a `TrackerSparqlStatement`
 * @res: a [type@Gio.AsyncResult] with the result of the operation
 * @error: Error location
 *
 * Finishes the asynchronous operation started through
 * [method@SparqlStatement.execute_async].
 *
 * Returns: (transfer full): A `TrackerSparqlCursor` with the query results.
 */
TrackerSparqlCursor *
tracker_sparql_statement_execute_finish (TrackerSparqlStatement  *stmt,
                                         GAsyncResult            *res,
                                         GError                 **error)
{
	TrackerSparqlStatementPrivate *priv =
		tracker_sparql_statement_get_instance_private (stmt);
	TrackerSparqlCursor *cursor;

	g_return_val_if_fail (TRACKER_IS_SPARQL_STATEMENT (stmt), NULL);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (res), NULL);
	g_return_val_if_fail (!error || !*error, NULL);

	cursor = TRACKER_SPARQL_STATEMENT_GET_CLASS (stmt)->execute_finish (stmt,
	                                                                    res,
	                                                                    error);
	if (cursor)
		tracker_sparql_cursor_set_connection (cursor, priv->connection);

	return cursor;
}

/**
 * tracker_sparql_statement_update:
 * @stmt: a `TrackerSparqlStatement`
 * @cancellable: (nullable): Optional [type@Gio.Cancellable]
 * @error: Error location
 *
 * Executes the `INSERT`/`DELETE` SPARQL query series with the currently bound values.
 *
 * This function should only be called on `TrackerSparqlStatement` objects
 * obtained through [method@SparqlConnection.update_statement] or
 * `INSERT`/`DELETE` statements loaded through
 * [method@SparqlConnection.load_statement_from_gresource].
 * An error will be raised if this method is called on
 * `SELECT`/`ASK`/`DESCRIBE`/`CONSTRUCT` SPARQL queries.
 *
 * Returns: %TRUE if the update finished with no errors, %FALSE otherwise
 *
 * Since: 3.5
 */
gboolean
tracker_sparql_statement_update (TrackerSparqlStatement  *stmt,
                                 GCancellable            *cancellable,
                                 GError                 **error)
{
	TrackerSparqlStatementPrivate *priv =
		tracker_sparql_statement_get_instance_private (stmt);

	g_return_val_if_fail (TRACKER_IS_SPARQL_STATEMENT (stmt), FALSE);
	g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);
	g_return_val_if_fail (!error || !*error, FALSE);

	if (tracker_sparql_connection_set_error_on_closed (priv->connection, error))
		return FALSE;

	return TRACKER_SPARQL_STATEMENT_GET_CLASS (stmt)->update (stmt,
	                                                          cancellable,
	                                                          error);
}

/**
 * tracker_sparql_statement_update_async:
 * @stmt: a `TrackerSparqlStatement`
 * @cancellable: (nullable): Optional [type@Gio.Cancellable]
 * @callback: user-defined [type@Gio.AsyncReadyCallback] to be called when
 *            the asynchronous operation is finished.
 * @user_data: user-defined data to be passed to @callback
 *
 * Executes asynchronously the `INSERT`/`DELETE` SPARQL query series with the currently bound values.
 *
 * This function should only be called on `TrackerSparqlStatement` objects
 * obtained through [method@SparqlConnection.update_statement] or
 * `INSERT`/`DELETE` statements loaded through
 * [method@SparqlConnection.load_statement_from_gresource].
 * An error will be raised if this method is called on
 * `SELECT`/`ASK`/`DESCRIBE`/`CONSTRUCT` SPARQL queries.
 *
 * Since: 3.5
 */
void
tracker_sparql_statement_update_async (TrackerSparqlStatement *stmt,
                                       GCancellable           *cancellable,
                                       GAsyncReadyCallback     callback,
                                       gpointer                user_data)
{
	TrackerSparqlStatementPrivate *priv =
		tracker_sparql_statement_get_instance_private (stmt);

	g_return_if_fail (TRACKER_IS_SPARQL_STATEMENT (stmt));
	g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

	if (tracker_sparql_connection_report_async_error_on_closed (priv->connection,
	                                                            callback,
	                                                            user_data))
		return;

	TRACKER_SPARQL_STATEMENT_GET_CLASS (stmt)->update_async (stmt,
	                                                         cancellable,
	                                                         callback,
	                                                         user_data);
}

/**
 * tracker_sparql_statement_update_finish:
 * @stmt: a `TrackerSparqlStatement`
 * @result: a [type@Gio.AsyncResult] with the result of the operation
 * @error: Error location
 *
 * Finishes the asynchronous update started through
 * [method@SparqlStatement.update_async].
 *
 * Returns: %TRUE if the update finished with no errors, %FALSE otherwise
 *
 * Since: 3.5
 */
gboolean
tracker_sparql_statement_update_finish (TrackerSparqlStatement  *stmt,
                                        GAsyncResult            *result,
                                        GError                 **error)
{
	g_return_val_if_fail (TRACKER_IS_SPARQL_STATEMENT (stmt), FALSE);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);
	g_return_val_if_fail (!error || !*error, FALSE);

	return TRACKER_SPARQL_STATEMENT_GET_CLASS (stmt)->update_finish (stmt,
	                                                                 result,
	                                                                 error);
}


/**
 * tracker_sparql_statement_clear_bindings:
 * @stmt: a `TrackerSparqlStatement`
 *
 * Clears all bindings.
 */
void
tracker_sparql_statement_clear_bindings (TrackerSparqlStatement *stmt)
{
	g_return_if_fail (TRACKER_IS_SPARQL_STATEMENT (stmt));

	TRACKER_SPARQL_STATEMENT_GET_CLASS (stmt)->clear_bindings (stmt);
}

/**
 * tracker_sparql_statement_serialize_async:
 * @stmt: a `TrackerSparqlStatement`
 * @flags: serialization flags
 * @format: RDF format of the serialized data
 * @cancellable: (nullable): Optional [type@Gio.Cancellable]
 * @callback: user-defined [type@Gio.AsyncReadyCallback] to be called when
 *            the asynchronous operation is finished.
 * @user_data: user-defined data to be passed to @callback
 *
 * Serializes a `DESCRIBE` or `CONSTRUCT` query into the given RDF @format.
 *
 * The query @stmt was created from must be either a `DESCRIBE` or `CONSTRUCT`
 * query, an error will be raised otherwise.
 *
 * This is an asynchronous operation, @callback will be invoked when the
 * data is available for reading.
 *
 * The SPARQL endpoint may not support the specified format, in that case
 * an error will be raised.
 *
 * The @flags argument is reserved for future expansions, currently
 * #TRACKER_SERIALIZE_FLAGS_NONE must be passed.
 *
 * Since: 3.3
 **/
void
tracker_sparql_statement_serialize_async (TrackerSparqlStatement *stmt,
                                          TrackerSerializeFlags   flags,
                                          TrackerRdfFormat        format,
                                          GCancellable           *cancellable,
                                          GAsyncReadyCallback     callback,
                                          gpointer                user_data)
{
	TrackerSparqlStatementPrivate *priv =
		tracker_sparql_statement_get_instance_private (stmt);

	g_return_if_fail (TRACKER_IS_SPARQL_STATEMENT (stmt));
	g_return_if_fail (flags == TRACKER_SERIALIZE_FLAGS_NONE);
	g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (callback != NULL);

	if (tracker_sparql_connection_report_async_error_on_closed (priv->connection,
	                                                            callback,
	                                                            user_data))
		return;

	TRACKER_SPARQL_STATEMENT_GET_CLASS (stmt)->serialize_async (stmt,
	                                                            flags,
	                                                            format,
	                                                            cancellable,
	                                                            callback,
	                                                            user_data);
}

/**
 * tracker_sparql_statement_serialize_finish:
 * @stmt: a `TrackerSparqlStatement`
 * @result: a [type@Gio.AsyncResult] with the result of the operation
 * @error: Error location
 *
 * Finishes the asynchronous operation started through
 * [method@SparqlStatement.serialize_async].
 *
 * Returns: (transfer full): a [class@Gio.InputStream] to read RDF content.
 *
 * Since: 3.3
 **/
GInputStream *
tracker_sparql_statement_serialize_finish (TrackerSparqlStatement  *stmt,
                                           GAsyncResult            *result,
                                           GError                 **error)
{
	g_return_val_if_fail (TRACKER_IS_SPARQL_STATEMENT (stmt), NULL);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);
	g_return_val_if_fail (!error || !*error, NULL);

	return TRACKER_SPARQL_STATEMENT_GET_CLASS (stmt)->serialize_finish (stmt,
	                                                                    result,
	                                                                    error);
}
