/*
 * Copyright (C) 2016, Sam Thursfield <sam@afuera.me.uk>
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
 * SECTION: tracker-sparql-private-store
 * @short_description: Creating a private Store.
 * @title: TrackerSparqlPrivateStore
 * @stability: Stable
 * @include: tracker-sparql.h
 *
 * <para>
 * Use a private Tracker Store if your application wants to keep data
 * separate from the user's session-wide Tracker Store.
 *
 * In general, we encourage storing data in the session-wide Tracker Store,
 * so that other applications can make use of it. Use
 * tracker_sparql_connection_get() to access the user's session-wide Store.
 * There are some reasons why you might want to avoid the user's session-wide
 * store, and use a private store instead.
 *
 * If you are writing automated tests, use a private store to ensure the tests
 * can't interfere with the user's real data.
 *
 * If you want to use a different set of ontologies to those that ship
 * by default with Tracker, you'll need to use a private store. It's very hard
 * for the session-wide store to deal with changes to the ontologies, so we
 * discourage installing new or changed ontologies for the system-wide Tracker
 * instance. Using a private store works around this limitation.
 *
 * With a private Tracker store, all access to the database happens in-process.
 * If multiple processes are writing to the same private Tracker store, you
 * might performance issues. This is because SQLite needs to lock the entire
 * database in order to do a write.
 */

/**
 * TrackerSparqlPrivateStore:
 *
 * The <structname>TrackerSparqlPrivateStore</structname> object represents a
 * connection to a private Tracker store.
 */
public class Tracker.Sparql.PrivateStore : Tracker.Sparql.Connection {
	/**
	 * tracker_sparql_private_store_open:
	 * @store_path: Location of a new or existing Tracker database.
	 * @ontologies: An array of paths to ontologies to load, or %NULL to load all
	 *    system-provided ontologies.
	 * @error: #GError for error reporting.
	 *
	 * Opens a private Tracker store. See above for more information on private
	 * Tracker stores.
	 *
	 * All access to a private Tracker store happens in-process. No D-Bus
	 * communication is involved. For that reason there is no async variant of
	 * this function.
	 *
	 * Returns: a new #TrackerSparqlConnection. Call g_object_unref() on the
	 * object when no longer used.
	 *
	 * Since: 0.10
	 */
	/* Implementation of this is in libtracker-sparql-backend/tracker-backend.vala */
	public extern static new Connection open (String store_path, String? ontologies_path) throws Sparql.Error, IOError;
}
