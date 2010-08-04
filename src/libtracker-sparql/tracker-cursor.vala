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
		 * Returns the connection used to retrieve the results.
		 *
		 * Returns: a #TrackerSparqlConnection. The returned object must not
		 * be freed by the caller.
		 */
		get;
		// Note: set method hidden in the documentation as the user of the
		//  library should never use it.
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
		 * Returns the number of columns available in the results to iterate.
		 *
		 * Returns: a #gint with the number of columns.
		 */
		get;
	}

	/**
	 * tracker_sparql_cursor_get_string:
	 * @self: a #TrackerSparqlCursor
	 * @column: column number to retrieve (first one is 0)
	 * @length: length of the returned string
	 *
	 * Returns the string at @column in the current row being iterated.
	 *
	 * Returns: a string, which should not be freed by the caller. #NULL
	 * is returned if the column number is in the [0,#n_columns] range.
	 */
	public abstract unowned string? get_string (int column, out long length = null);

	/**
	 * tracker_sparql_cursor_next:
	 * @self: a #TrackerSparqlCursor
	 * @cancellable: a #GCancellable used to cancel the operation
	 * @error: #GError for error reporting.
	 *
	 * Iterates to the next result. The API call is completely synchronous, so
	 * it may block.
	 *
	 * Returns: #FALSE if no more results found, #TRUE otherwise.
	 */
	public abstract bool next (Cancellable? cancellable = null) throws GLib.Error;

	/**
	 * tracker_sparql_cursor_next_async:
	 * @self: a #TrackerSparqlCursor
	 * @_callback_: user-defined #GAsyncReadyCallback to be called when
	 *              asynchronous operation is finished.
	 * @_user_data_: user-defined data to be passed to @_callback_
	 * @cancellable: a #GCancellable used to cancel the operation
	 *
	 * Iterates, asynchronously, to the next result.
	 */

	/**
	 * tracker_sparql_cursor_next_finish:
	 * @self: a #TrackerSparqlCursor
	 * @_res_: a #GAsyncResult with the result of the operation
	 * @error: #GError for error reporting.
	 *
	 * Finishes the asynchronous iteration to the next result.
	 *
	 * Returns: #FALSE if no more results found, #TRUE otherwise.
	 */
	public async abstract bool next_async (Cancellable? cancellable = null) throws GLib.Error;

	/**
	 * tracker_sparql_cursor_rewind:
	 * @self: a #TrackerSparqlCursor
	 *
	 * Resets the iterator to point back to the first result.
	 */
	public abstract void rewind ();
}
