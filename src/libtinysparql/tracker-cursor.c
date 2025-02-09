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
 * TrackerSparqlCursor:
 *
 * `TrackerSparqlCursor` provides the methods to iterate the results of a SPARQL query.
 *
 * Cursors are obtained through e.g. [method@SparqlStatement.execute]
 * or [method@SparqlConnection.query] after the SPARQL query has been
 * executed.
 *
 * When created, a cursor does not point to any element, [method@SparqlCursor.next]
 * is necessary to iterate one by one to the first (and following) results.
 * When the cursor iterated across all rows in the result set, [method@SparqlCursor.next]
 * will return %FALSE with no error set.
 *
 * On each row, it is possible to extract the result values through the
 * [method@SparqlCursor.get_integer], [method@SparqlCursor.get_string], etc... family
 * of methods. The column index of those functions starts at 0. The number of columns is
 * dependent on the SPARQL query issued, but may be checked at runtime through the
 * [method@SparqlCursor.get_n_columns] method.
 *
 * After a cursor is iterated, it is recommended to call [method@SparqlCursor.close]
 * explicitly to free up resources for other users of the same [class@SparqlConnection],
 * this is especially important in garbage collected languages. These resources
 * will be also implicitly freed on cursor object finalization.
 *
 * It is possible to use a given `TrackerSparqlCursor` in other threads than
 * the one it was created from. It must be however used from just one thread
 * at any given time.
 */
#include "config.h"

#include "tracker-cursor.h"
#include "tracker-private.h"

enum {
	PROP_0,
	PROP_CONNECTION,
	PROP_N_COLUMNS,
	N_PROPS
};

static GParamSpec *props[N_PROPS];

typedef struct {
	TrackerSparqlConnection *connection;
} TrackerSparqlCursorPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (TrackerSparqlCursor, tracker_sparql_cursor,
                                     G_TYPE_OBJECT)

static void
tracker_sparql_cursor_init (TrackerSparqlCursor *cursor)
{
}

static gboolean
tracker_sparql_cursor_real_is_bound (TrackerSparqlCursor *cursor,
                                     gint                 column)
{
	return tracker_sparql_cursor_get_value_type (cursor, column) != TRACKER_SPARQL_VALUE_TYPE_UNBOUND;
}

static gint64
tracker_sparql_cursor_real_get_integer (TrackerSparqlCursor *cursor,
                                        gint                 column)
{
	const gchar *text;

	g_return_val_if_fail (tracker_sparql_cursor_real_is_bound (cursor, column), 0);

	text = tracker_sparql_cursor_get_string (cursor, column, NULL);
	return g_ascii_strtoll (text, NULL, 10);
}

static gdouble
tracker_sparql_cursor_real_get_double (TrackerSparqlCursor *cursor,
                                       gint                 column)
{
	const gchar *text;

	g_return_val_if_fail (tracker_sparql_cursor_real_is_bound (cursor, column), 0);

	text = tracker_sparql_cursor_get_string (cursor, column, NULL);

	return g_ascii_strtod (text, NULL);
}

static gboolean
tracker_sparql_cursor_real_get_boolean (TrackerSparqlCursor *cursor,
                                        gint                 column)
{
	const gchar *text;

	g_return_val_if_fail (tracker_sparql_cursor_real_is_bound (cursor, column), FALSE);

	text = tracker_sparql_cursor_get_string (cursor, column, NULL);

	return g_ascii_strcasecmp (text, "true") == 0;
}

static GDateTime *
tracker_sparql_cursor_real_get_datetime (TrackerSparqlCursor *cursor,
                                         gint                 column)
{
	const gchar *text;
	GDateTime *date_time;

	g_return_val_if_fail (tracker_sparql_cursor_real_is_bound (cursor, column), NULL);

	text = tracker_sparql_cursor_get_string (cursor, column, NULL);
	date_time = g_date_time_new_from_iso8601 (text, NULL);

	return date_time;
}

static void
tracker_sparql_cursor_finalize (GObject *object)
{
	TrackerSparqlCursor *cursor = TRACKER_SPARQL_CURSOR (object);
	TrackerSparqlCursorPrivate *priv = tracker_sparql_cursor_get_instance_private (cursor);

	g_clear_object (&priv->connection);
	G_OBJECT_CLASS (tracker_sparql_cursor_parent_class)->finalize (object);
}

static void
tracker_sparql_cursor_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
	TrackerSparqlCursor *cursor = TRACKER_SPARQL_CURSOR (object);
	TrackerSparqlCursorPrivate *priv = tracker_sparql_cursor_get_instance_private (cursor);

	switch (prop_id) {
	case PROP_CONNECTION:
		priv->connection = g_value_dup_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
tracker_sparql_cursor_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
	TrackerSparqlCursor *cursor = TRACKER_SPARQL_CURSOR (object);
	TrackerSparqlCursorPrivate *priv = tracker_sparql_cursor_get_instance_private (cursor);

	switch (prop_id) {
	case PROP_CONNECTION:
		g_value_set_object (value, priv->connection);
		break;
	case PROP_N_COLUMNS:
		g_value_set_int (value, tracker_sparql_cursor_get_n_columns (cursor));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
tracker_sparql_cursor_class_init (TrackerSparqlCursorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_sparql_cursor_finalize;
	object_class->set_property = tracker_sparql_cursor_set_property;
	object_class->get_property = tracker_sparql_cursor_get_property;

	klass->get_integer = tracker_sparql_cursor_real_get_integer;
	klass->get_double = tracker_sparql_cursor_real_get_double;
	klass->get_boolean = tracker_sparql_cursor_real_get_boolean;
	klass->get_datetime = tracker_sparql_cursor_real_get_datetime;
	klass->is_bound = tracker_sparql_cursor_real_is_bound;

	/**
	 * TrackerSparqlCursor:connection:
	 *
	 * The [class@SparqlConnection] used to retrieve the results.
	 */
	props[PROP_CONNECTION] =
		g_param_spec_object ("connection",
		                     "connection",
		                     "connection",
		                     TRACKER_SPARQL_TYPE_CONNECTION,
		                     G_PARAM_CONSTRUCT_ONLY |
		                     G_PARAM_STATIC_STRINGS |
		                     G_PARAM_READABLE |
		                     G_PARAM_WRITABLE);
	/**
	 * TrackerSparqlCursor:n-columns:
	 *
	 * Number of columns available in the result set.
	 */
	props[PROP_N_COLUMNS] =
		g_param_spec_int ("n-columns",
		                  "n-columns",
		                  "n-columns",
		                  G_MININT, G_MAXINT, 0,
		                  G_PARAM_STATIC_STRINGS |
		                  G_PARAM_READABLE);

	g_object_class_install_properties (object_class, N_PROPS, props);
}

/**
 * tracker_sparql_cursor_get_connection:
 * @cursor: a `TrackerSparqlCursor`
 *
 * Returns the [class@SparqlConnection] associated with this
 * `TrackerSparqlCursor`.
 *
 * Returns: (transfer none): the cursor [class@SparqlConnection]. The
 * returned object must not be unreferenced by the caller.
 */
TrackerSparqlConnection *
tracker_sparql_cursor_get_connection (TrackerSparqlCursor *cursor)
{
	TrackerSparqlCursorPrivate *priv = tracker_sparql_cursor_get_instance_private (cursor);

	g_return_val_if_fail (TRACKER_IS_SPARQL_CURSOR (cursor), NULL);

	return priv->connection;
}

void
tracker_sparql_cursor_set_connection (TrackerSparqlCursor     *cursor,
                                      TrackerSparqlConnection *connection)
{
	TrackerSparqlCursorPrivate *priv = tracker_sparql_cursor_get_instance_private (cursor);

	g_return_if_fail (TRACKER_IS_SPARQL_CURSOR (cursor));
	g_return_if_fail (TRACKER_IS_SPARQL_CONNECTION (connection));

	g_set_object (&priv->connection, connection);
}

/**
 * tracker_sparql_cursor_get_n_columns:
 * @cursor: a `TrackerSparqlCursor`
 *
 * Retrieves the number of columns available in the result set.
 *
 * This method should only be called after a successful
 * [method@SparqlCursor.next], otherwise its return value
 * will be undefined.
 *
 * Returns: The number of columns returned in the result set.
 */
gint
tracker_sparql_cursor_get_n_columns (TrackerSparqlCursor *cursor)
{
	g_return_val_if_fail (TRACKER_IS_SPARQL_CURSOR (cursor), 0);

	return TRACKER_SPARQL_CURSOR_GET_CLASS (cursor)->get_n_columns (cursor);
}

/**
 * tracker_sparql_cursor_get_string:
 * @cursor: a `TrackerSparqlCursor`
 * @column: column number to retrieve (first one is 0)
 * @length: (out) (nullable): length of the returned string, or %NULL
 *
 * Retrieves a string representation of the data in the current
 * row in @column.
 *
 * Any type may be converted to a string. If the value is not bound
 * (See [method@SparqlCursor.is_bound]) this method will return %NULL.
 *
 * Returns: (nullable): a string which must not be freed. %NULL is returned if
 * the column is not in the `[0, n_columns]` range, or if the row/column
 * refer to a nullable optional value in the result set.
 */
const gchar *
tracker_sparql_cursor_get_string (TrackerSparqlCursor *cursor,
                                  gint                 column,
                                  glong               *length)
{
	g_return_val_if_fail (TRACKER_IS_SPARQL_CURSOR (cursor), NULL);

	return TRACKER_SPARQL_CURSOR_GET_CLASS (cursor)->get_string (cursor,
	                                                             column,
	                                                             NULL,
	                                                             length);
}

/**
 * tracker_sparql_cursor_get_langstring:
 * @cursor: a `TrackerSparqlCursor`
 * @column: column number to retrieve
 * @langtag: (out): language tag of the returned string, or %NULL if the
 *   string has no language tag
 * @length: (out) (nullable): length of the returned string
 *
 * Retrieves a string representation of the data in the current
 * row in @column. If the string has language information (i.e. it is
 * a `rdf:langString`](rdf-ontology.html#rdf:langString)), the language
 * tag will be returned in the location provided through @langtag. This
 * language tag will typically be in a format conforming
 * [RFC 5646](https://www.rfc-editor.org/rfc/rfc5646.html).
 *
 * Returns: (nullable): a string which must not be freed. %NULL is returned if
 * the column is not in the `[0, n_columns]` range, or if the row/column
 * refer to a nullable optional value in the result set.
 *
 * Since: 3.7
 **/
const gchar *
tracker_sparql_cursor_get_langstring (TrackerSparqlCursor  *cursor,
                                      gint                  column,
                                      const gchar         **langtag,
                                      glong                *length)
{
	g_return_val_if_fail (TRACKER_IS_SPARQL_CURSOR (cursor), NULL);
	g_return_val_if_fail (langtag != NULL, NULL);

	return TRACKER_SPARQL_CURSOR_GET_CLASS (cursor)->get_string (cursor,
	                                                             column,
	                                                             langtag,
	                                                             length);
}

/**
 * tracker_sparql_cursor_get_boolean:
 * @cursor: a `TrackerSparqlCursor`
 * @column: column number to retrieve (first one is 0)
 *
 * Retrieve a boolean for the current row in @column.
 *
 * If the row/column do not have a boolean value, the result is
 * undefined, see [method@SparqlCursor.get_value_type].
 *
 * Returns: a boolean value.
 */
gboolean
tracker_sparql_cursor_get_boolean (TrackerSparqlCursor *cursor,
                                   gint                 column)
{
	g_return_val_if_fail (TRACKER_IS_SPARQL_CURSOR (cursor), FALSE);

	return TRACKER_SPARQL_CURSOR_GET_CLASS (cursor)->get_boolean (cursor,
	                                                              column);
}

/**
 * tracker_sparql_cursor_get_double:
 * @cursor: a `TrackerSparqlCursor`
 * @column: column number to retrieve (first one is 0)
 *
 * Retrieve a double for the current row in @column.
 *
 * If the row/column do not have a integer or double value, the result is
 * undefined, see [method@SparqlCursor.get_value_type].
 *
 * Returns: a double value.
 */
gdouble
tracker_sparql_cursor_get_double (TrackerSparqlCursor *cursor,
                                  gint                 column)
{
	g_return_val_if_fail (TRACKER_IS_SPARQL_CURSOR (cursor), -1);

	return TRACKER_SPARQL_CURSOR_GET_CLASS (cursor)->get_double (cursor,
	                                                             column);
}

/**
 * tracker_sparql_cursor_get_integer:
 * @cursor: a `TrackerSparqlCursor`
 * @column: column number to retrieve (first one is 0)
 *
 * Retrieve an integer for the current row in @column.
 *
 * If the row/column do not have an integer value, the result is
 * undefined, see [method@SparqlCursor.get_value_type].
 *
 * Returns: a 64-bit integer value.
 */
gint64
tracker_sparql_cursor_get_integer (TrackerSparqlCursor *cursor,
                                   gint                 column)
{
	g_return_val_if_fail (TRACKER_IS_SPARQL_CURSOR (cursor), -1);

	return TRACKER_SPARQL_CURSOR_GET_CLASS (cursor)->get_integer (cursor,
	                                                              column);
}

/**
 * tracker_sparql_cursor_get_value_type:
 * @cursor: a `TrackerSparqlCursor`
 * @column: column number to retrieve (first one is 0)
 *
 * Returns the data type bound to the current row and the given @column.
 *
 * If the column is unbound, the value will be %TRACKER_SPARQL_VALUE_TYPE_UNBOUND.
 * See also [method@SparqlCursor.is_bound].
 *
 * Values of type #TRACKER_SPARQL_VALUE_TYPE_RESOURCE and
 * #TRACKER_SPARQL_VALUE_TYPE_BLANK_NODE can be considered equivalent, the
 * difference is the resource being referenced as a named IRI or a blank
 * node.
 *
 * All other [enum@SparqlValueType] value types refer to literal values.
 *
 * Returns: a [enum@SparqlValueType] expressing the content type of
 *   the given column for the current row.
 */
TrackerSparqlValueType
tracker_sparql_cursor_get_value_type (TrackerSparqlCursor *cursor,
                                      gint                 column)
{
	g_return_val_if_fail (TRACKER_IS_SPARQL_CURSOR (cursor),
	                      TRACKER_SPARQL_VALUE_TYPE_UNBOUND);

	return TRACKER_SPARQL_CURSOR_GET_CLASS (cursor)->get_value_type (cursor,
	                                                                 column);
}

/**
 * tracker_sparql_cursor_get_variable_name:
 * @cursor: a `TrackerSparqlCursor`
 * @column: column number to retrieve (first one is 0)
 *
 * Retrieves the name of the given @column.
 *
 * This name will be defined at the SPARQL query, either
 * implicitly from the names of the variables returned in
 * the resultset, or explicitly through the `AS ?var` SPARQL
 * syntax.
 *
 * Returns: (nullable): The name of the given column.
 */
const gchar *
tracker_sparql_cursor_get_variable_name (TrackerSparqlCursor *cursor,
                                         gint                 column)
{
	g_return_val_if_fail (TRACKER_IS_SPARQL_CURSOR (cursor), NULL);

	return TRACKER_SPARQL_CURSOR_GET_CLASS (cursor)->get_variable_name (cursor,
	                                                                    column);
}

/**
 * tracker_sparql_cursor_get_datetime:
 * @cursor: a `TrackerSparqlCursor`
 * @column: Column number to retrieve (first one is 0)
 *
 * Retrieves a [type@GLib.DateTime] pointer for the current row in @column.
 *
 * Returns: (transfer full) (nullable): [type@GLib.DateTime] object, or %NULL if the given column does not
 *   contain a [xsd:date](xsd-ontology.html#xsd:date) or [xsd:dateTime](xsd-ontology.html#xsd:dateTime).
 *
 * Since: 3.2
 */
GDateTime *
tracker_sparql_cursor_get_datetime (TrackerSparqlCursor *cursor,
                                    gint                 column)
{
	g_return_val_if_fail (TRACKER_IS_SPARQL_CURSOR (cursor), NULL);

	return TRACKER_SPARQL_CURSOR_GET_CLASS (cursor)->get_datetime (cursor,
	                                                               column);
}

/**
 * tracker_sparql_cursor_close:
 * @cursor: a `TrackerSparqlCursor`
 *
 * Closes the cursor. The object can only be freed after this call.
 */
void
tracker_sparql_cursor_close (TrackerSparqlCursor *cursor)
{
	g_return_if_fail (TRACKER_IS_SPARQL_CURSOR (cursor));

	TRACKER_SPARQL_CURSOR_GET_CLASS (cursor)->close (cursor);
}

/**
 * tracker_sparql_cursor_is_bound:
 * @cursor: a `TrackerSparqlCursor`
 * @column: column number to retrieve (first one is 0)
 *
 * Returns whether the given @column has a bound value in the current row.
 *
 * This may not be the case through e.g. the `OPTIONAL { }` SPARQL syntax.
 *
 * Returns: a %TRUE or %FALSE.
 */
gboolean
tracker_sparql_cursor_is_bound (TrackerSparqlCursor *cursor,
                                gint                 column)
{
	g_return_val_if_fail (TRACKER_IS_SPARQL_CURSOR (cursor), FALSE);

	return TRACKER_SPARQL_CURSOR_GET_CLASS (cursor)->is_bound (cursor, column);
}

/**
 * tracker_sparql_cursor_next:
 * @cursor: a `TrackerSparqlCursor`
 * @cancellable: (nullable): Optional [type@Gio.Cancellable]
 * @error: Error location
 *
 * Iterates the cursor to the next result.
 *
 * If the cursor was not started, it will point to the first result after
 * this call. This operation is completely synchronous and it may block,
 * see [method@SparqlCursor.next_async] for an asynchronous variant.
 *
 * Returns: %FALSE if there are no more results or if an error is found, otherwise %TRUE.
 */
gboolean
tracker_sparql_cursor_next (TrackerSparqlCursor  *cursor,
                            GCancellable         *cancellable,
                            GError              **error)
{
	GError *inner_error = NULL;
	gboolean success;

	g_return_val_if_fail (TRACKER_IS_SPARQL_CURSOR (cursor), FALSE);
	g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);
	g_return_val_if_fail (!error || !*error, FALSE);

	success = TRACKER_SPARQL_CURSOR_GET_CLASS (cursor)->next (cursor,
	                                                          cancellable,
	                                                          &inner_error);

	if (inner_error)
		g_propagate_error (error, _translate_internal_error (inner_error));

	return success;
}

/**
 * tracker_sparql_cursor_next_async:
 * @cursor: a `TrackerSparqlCursor`
 * @cancellable: (nullable): Optional [type@Gio.Cancellable]
 * @callback: user-defined [type@Gio.AsyncReadyCallback] to be called when
 *            asynchronous operation is finished.
 * @user_data: user-defined data to be passed to @callback
 *
 * Iterates the cursor asyncronously to the next result.
 *
 * If the cursor was not started, it will point to the first result after
 * this operation completes.
 *
 * In the period between this call and the corresponding
 * [method@SparqlCursor.next_finish] call, the other cursor methods
 * should not be used, nor their results trusted. The cursor should only
 * be iterated once at a time.
 */
void
tracker_sparql_cursor_next_async (TrackerSparqlCursor  *cursor,
                                  GCancellable         *cancellable,
                                  GAsyncReadyCallback   callback,
                                  gpointer              user_data)
{
	g_return_if_fail (TRACKER_IS_SPARQL_CURSOR (cursor));
	g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

	TRACKER_SPARQL_CURSOR_GET_CLASS (cursor)->next_async (cursor,
	                                                      cancellable,
	                                                      callback,
	                                                      user_data);
}

/**
 * tracker_sparql_cursor_next_finish:
 * @cursor: a `TrackerSparqlCursor`
 * @res: a [type@Gio.AsyncResult] with the result of the operation
 * @error: Error location
 *
 * Finishes the asynchronous iteration to the next result started with
 * [method@SparqlCursor.next_async].
 *
 * Returns: %FALSE if there are no more results or if an error is found, otherwise %TRUE.
 */
gboolean
tracker_sparql_cursor_next_finish (TrackerSparqlCursor  *cursor,
                                   GAsyncResult         *res,
                                   GError              **error)
{
	GError *inner_error = NULL;
	gboolean success;

	g_return_val_if_fail (TRACKER_IS_SPARQL_CURSOR (cursor), FALSE);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (res), FALSE);
	g_return_val_if_fail (!error || !*error, FALSE);

	success = TRACKER_SPARQL_CURSOR_GET_CLASS (cursor)->next_finish (cursor,
	                                                                 res,
	                                                                 &inner_error);

	if (inner_error)
		g_propagate_error (error, _translate_internal_error (inner_error));

	return success;
}

/**
 * tracker_sparql_cursor_rewind:
 * @cursor: a `TrackerSparqlCursor`
 *
 * Resets the iterator to point back to the first result.
 *
 * Deprecated: 3.5: This function only works on cursors
 * from direct [class@SparqlConnection] objects and cannot work
 * reliably across all cursor types. Issue a different query to
 * obtain a new cursor.
 */
void
tracker_sparql_cursor_rewind (TrackerSparqlCursor *cursor)
{
	g_return_if_fail (TRACKER_IS_SPARQL_CURSOR (cursor));

	if (TRACKER_SPARQL_CURSOR_GET_CLASS (cursor)->rewind)
		TRACKER_SPARQL_CURSOR_GET_CLASS (cursor)->rewind (cursor);
	else
		g_warning ("Rewind not implemented for cursor type %s", G_OBJECT_TYPE_NAME (cursor));
}
