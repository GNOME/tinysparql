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
 * SECTION: tracker-sparql-cursor
 * @short_description: Iteration of the query results
 * @title: TrackerSparqlCursor
 * @stability: Stable
 * @include: tracker-sparql.h
 *
 * <para>
 * #TrackerSparqlCursor is an object which provides methods to iterate the
 * results of a query to the Tracker Store.
 * </para>
 */

/**
 * TrackerSparqlValueType:
 * @TRACKER_SPARQL_VALUE_TYPE_UNBOUND: Unbound value type
 * @TRACKER_SPARQL_VALUE_TYPE_URI: Uri value type, rdfs:Resource
 * @TRACKER_SPARQL_VALUE_TYPE_STRING: String value type, xsd:string
 * @TRACKER_SPARQL_VALUE_TYPE_INTEGER: Integer value type, xsd:integer
 * @TRACKER_SPARQL_VALUE_TYPE_DOUBLE: Double value type, xsd:double
 * @TRACKER_SPARQL_VALUE_TYPE_DATETIME: Datetime value type, xsd:dateTime
 * @TRACKER_SPARQL_VALUE_TYPE_BLANK_NODE: Blank node value type
 * @TRACKER_SPARQL_VALUE_TYPE_BOOLEAN: Boolean value type, xsd:boolean
 *
 * Enumeration with the possible types of the cursor's cells
 *
 * Since: 0.10
 */
public enum Tracker.Sparql.ValueType {
	UNBOUND,
	URI,
	STRING,
	INTEGER,
	DOUBLE,
	DATETIME,
	BLANK_NODE,
	BOOLEAN
}

/**
 * TrackerSparqlCursor:
 *
 * The <structname>TrackerSparqlCursor</structname> object represents an
 * iterator of results.
 */
public abstract class Tracker.Sparql.Cursor : Object {

	/**
	 * TrackerSparqlCursor:connection:
	 *
	 * The #TrackerSparqlConnection used to retrieve the results.
	 */
	public Connection connection {
		/**
		 * tracker_sparql_cursor_get_connection:
		 * @self: a #TrackerSparqlCursor
		 *
		 * Returns: the #TrackerSparqlConnection associated with this
		 * #TrackerSparqlCursor. The returned object must not be unreferenced
		 * by the caller.
		 *
		 * Since: 0.10
		 */
		get;
		// Note: set method hidden in the documentation as the user of the
		// library should never use it.
		set;
	}

	/**
	 * TrackerSparqlCursor:n_columns:
	 *
	 * Number of columns available in the results to iterate.
	 */
	public abstract int n_columns {
		/**
		 * tracker_sparql_cursor_get_n_columns:
		 * @self: a #TrackerSparqlCursor
		 *
		 * This method should only be called after a successful
		 * tracker_sparql_cursor_next(); otherwise its return value
		 * will be undefined.
		 *
		 * Returns: a #gint representing the number of columns available in the
		 * results to iterate.
		 *
		 * Since: 0.10
		 */
		get;
	}

	/**
	 * tracker_sparql_cursor_get_value_type:
	 * @self: a #TrackerSparqlCursor
	 * @column: column number to retrieve (first one is 0)
	 *
	 * The data type bound to the current row in @column is returned.
	 *
	 * Returns: a #TrackerSparqlValueType.
	 *
	 * Since: 0.10
	 */
	public abstract ValueType get_value_type (int column);

	/**
	 * tracker_sparql_cursor_get_variable_name:
	 * @self: a #TrackerSparqlCursor
	 * @column: column number to retrieve (first one is 0)
	 *
	 * Retrieves the variable name for the current row in @column.
	 *
	 * Returns: a string which must not be freed.
	 *
	 * Since: 0.10
	 */
	public abstract unowned string? get_variable_name (int column);

	/**
	 * tracker_sparql_cursor_get_string:
	 * @self: a #TrackerSparqlCursor
	 * @column: column number to retrieve (first one is 0)
	 * @length: length of the returned string
	 *
	 * Retrieves a string representation of the data in the current
	 * row in @column.
	 *
	 * Returns: a string which must not be freed. %NULL is returned if
	 * the column is not in the [0,#n_columns] range.
	 *
	 * Since: 0.10
	 */
	public abstract unowned string? get_string (int column, out long length = null);

	/**
	 * tracker_sparql_cursor_next:
	 * @self: a #TrackerSparqlCursor
	 * @cancellable: a #GCancellable used to cancel the operation
	 * @error: #GError for error reporting.
	 *
	 * Iterates to the next result. This is completely synchronous and
	 * it may block.
	 *
	 * Returns: %FALSE if no more results found, otherwise %TRUE.
	 *
	 * Since: 0.10
	 */
	public abstract bool next (Cancellable? cancellable = null) throws GLib.Error;

	/**
	 * tracker_sparql_cursor_next_finish:
	 * @self: a #TrackerSparqlCursor
	 * @_res_: a #GAsyncResult with the result of the operation
	 * @error: #GError for error reporting.
	 *
	 * Finishes the asynchronous iteration to the next result.
	 *
	 * Returns: %FALSE if no more results found, otherwise %TRUE.
	 *
	 * Since: 0.10
	 */

	/**
	 * tracker_sparql_cursor_next_async:
	 * @self: a #TrackerSparqlCursor
	 * @cancellable: a #GCancellable used to cancel the operation
	 * @_callback_: user-defined #GAsyncReadyCallback to be called when
	 *              asynchronous operation is finished.
	 * @_user_data_: user-defined data to be passed to @_callback_
	 *
	 * Iterates, asynchronously, to the next result.
	 *
	 * Since: 0.10
	 */
	public async abstract bool next_async (Cancellable? cancellable = null) throws GLib.Error;

	/**
	 * tracker_sparql_cursor_rewind:
	 * @self: a #TrackerSparqlCursor
	 *
	 * Resets the iterator to point back to the first result.
	 *
	 * Since: 0.10
	 */
	public abstract void rewind ();

	/**
	 * tracker_sparql_cursor_get_integer:
	 * @self: a #TrackerSparqlCursor
	 * @column: column number to retrieve (first one is 0)
	 *
	 * Retrieve an integer for the current row in @column.
	 *
	 * Returns: a #gint64.
	 *
	 * Since: 0.10
	 */
	public virtual int64 get_integer (int column) {
		return_val_if_fail (get_value_type (column) == ValueType.INTEGER, 0);
		unowned string as_str = get_string (column);
		return as_str.to_int64();
	}

	/**
	 * tracker_sparql_cursor_get_double:
	 * @self: a #TrackerSparqlCursor
	 * @column: column number to retrieve (first one is 0)
	 *
	 * Retrieve a double for the current row in @column.
	 *
	 * Returns: a double.
	 *
	 * Since: 0.10
	 */
	public virtual double get_double (int column) {
		return_val_if_fail (get_value_type (column) == ValueType.DOUBLE, 0);
		unowned string as_str = get_string (column);
		return as_str.to_double();
	}

	/**
	 * tracker_sparql_cursor_get_boolean:
	 * @self: a #TrackerSparqlCursor
	 * @column: column number to retrieve (first one is 0)
	 *
	 * Retrieve a boolean for the current row in @column.
	 *
	 * Returns: a #gboolean.
	 *
	 * Since: 0.10
	 */
	public virtual bool get_boolean (int column) {
		ValueType type = get_value_type (column);
		return_val_if_fail (type == ValueType.BOOLEAN, 0);
		unowned string as_str = get_string (column);
		return (strcmp (as_str, "true") == 0);
	}

	/**
	 * tracker_sparql_cursor_is_bound:
	 * @self: a #TrackerSparqlCursor
	 * @column: column number to retrieve (first one is 0)
	 *
	 * If the current row and @column are bound to a value, %TRUE is returned.
	 *
	 * Returns: a %TRUE or %FALSE.
	 *
	 * Since: 0.10
	 */
	public virtual bool is_bound (int column) {
		if (get_value_type (column) != ValueType.UNBOUND) {
			return true;
		}
		return false;
	}
}
