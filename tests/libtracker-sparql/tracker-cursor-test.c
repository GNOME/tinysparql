/*
 * Copyright (C) 2010, Codeminded BVBA <abustany@gnome.org>
 * Copyright (C) 2022, Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include <libtracker-sparql/tracker-sparql.h>

/* This MUST be larger than TRACKER_STEROIDS_BUFFER_SIZE */
#define LONG_NAME_SIZE 128 * 1024 * sizeof(char)

typedef struct {
	GMainLoop *main_loop;
	const gchar *query;
} AsyncData;

static TrackerSparqlConnection *connection;
gboolean started = FALSE;

static GMainLoop *main_loop;

#define N_QUERIES 3

static const gchar *queries[N_QUERIES] = {
	/* #1 */
	"SELECT ?p WHERE { ?p nrl:indexed true }",
	/* #2 */
	"SELECT ?prefix ?ns WHERE { ?ns a nrl:Namespace ; nrl:prefix ?prefix }",
	/* #3 */
	"SELECT ?p WHERE { ?p nrl:fulltextIndexed true }",
};

static void
delete_test_data (gpointer      *fixture,
                  gconstpointer  user_data)
{
	GError *error = NULL;
	const char *delete_query = "DELETE { "
	                           "<urn:testdata1> a rdfs:Resource ."
	                           "<urn:testdata2> a rdfs:Resource ."
	                           "<urn:testdata3> a rdfs:Resource ."
	                           "<urn:testdata4> a rdfs:Resource ."
	                           "}";

	tracker_sparql_connection_update (connection, delete_query, NULL, &error);
	g_assert_no_error (error);
}

static void
insert_test_data (gpointer      *fixture,
                  gconstpointer  user_data)
{
	GError *error = NULL;
	char *longName = g_malloc (LONG_NAME_SIZE);
	char *filled_query;

	/* Ensure data is deleted */
	delete_test_data (fixture, user_data);

	memset (longName, 'a', LONG_NAME_SIZE);

	longName[LONG_NAME_SIZE - 1] = '\0';

	filled_query = g_strdup_printf ("INSERT {"
	                                "    <urn:testdata1> a nfo:FileDataObject ; nie:url \"/foo/bar\" ."
	                                "    <urn:testdata2> a nfo:FileDataObject ; nie:url \"/plop/coin\" ."
	                                "    <urn:testdata3> a nmm:Artist ; nmm:artistName \"testArtist\" ."
	                                "    <urn:testdata4> a nmm:Photo ; nao:identifier \"%s\" ."
	                                "}", longName);

	tracker_sparql_connection_update (connection, filled_query, NULL, &error);
	g_assert_no_error (error);

	g_free (filled_query);
	g_free (longName);
}

static void
compare_cursors (TrackerSparqlCursor *cursor_a,
                 TrackerSparqlCursor *cursor_b)
{
	while (tracker_sparql_cursor_next (cursor_a, NULL, NULL) && tracker_sparql_cursor_next (cursor_b, NULL, NULL)) {
		g_assert_cmpstr (tracker_sparql_cursor_get_string (cursor_a, 0, NULL),
				 ==,
				 tracker_sparql_cursor_get_string (cursor_b, 0, NULL));
	}

	/* Check that both cursors are at the end (same number of rows) */
	g_assert_true (!tracker_sparql_cursor_next (cursor_a, NULL, NULL));
	g_assert_true (!tracker_sparql_cursor_next (cursor_b, NULL, NULL));
}

static void
query_and_compare_results (const char *query)
{
	TrackerSparqlCursor *cursor_glib;
	TrackerSparqlCursor *cursor_fd;
	GError *error = NULL;

	cursor_glib = tracker_sparql_connection_query (connection, query, NULL, &error);

	g_assert_no_error (error);

	cursor_fd = tracker_sparql_connection_query (connection, query, NULL, &error);

	g_assert_no_error (error);

	compare_cursors (cursor_glib, cursor_fd);

	g_object_unref (cursor_glib);
	g_object_unref (cursor_fd);
}

static void
test_tracker_sparql_query_iterate (gpointer      fixture,
                                   gconstpointer user_data)
{
	query_and_compare_results ("SELECT ?r nie:url(?r) WHERE {?r a nfo:FileDataObject}");
}

static void
test_tracker_sparql_query_iterate_largerow (gpointer      fixture,
                                            gconstpointer user_data)
{
	query_and_compare_results ("SELECT nao:identifier(?r) WHERE {?r a nmm:Photo}");
}

/* Runs an invalid query */
static void
test_tracker_sparql_query_iterate_error (gpointer      fixture,
                                         gconstpointer user_data)
{
	TrackerSparqlCursor *cursor;
	GError *error = NULL;
	const gchar *query = "bork bork bork";

	cursor = tracker_sparql_connection_query (connection, query, NULL, &error);

	/* tracker_sparql_query_iterate should return null on error */
	g_assert_true (!cursor);

	/* error should be set, along with its message, note: we don't
	 * use g_assert_error() because the code does not match the
	 * enum values for TRACKER_SPARQL_ERROR_*, this is due to
	 * dbus/error matching between client/server. This should be
	 * fixed in gdbus.
	 */
	g_assert_true (error != NULL && error->domain == TRACKER_SPARQL_ERROR);

	g_error_free (error);
}

static void
test_tracker_sparql_query_iterate_empty (gpointer      fixture,
                                         gconstpointer user_data)
{
	TrackerSparqlCursor *cursor;
	GError *error = NULL;
	const gchar *query = "SELECT ?r WHERE {?r a nfo:FileDataObject; nao:identifier \"thisannotationdoesnotexist\"}";

	cursor = tracker_sparql_connection_query (connection, query, NULL, &error);

	g_assert_true (cursor);
	g_assert_no_error (error);

	g_assert_false (tracker_sparql_cursor_next (cursor, NULL, NULL));
	g_assert_true (tracker_sparql_cursor_get_n_columns (cursor) == 1);

	/* FIXME: test behavior of cursor getters after last value */

	g_object_unref (cursor);
}

/* Closes the cursor before all results are read */
static void
test_tracker_sparql_query_iterate_sigpipe (gpointer      fixture,
                                           gconstpointer user_data)
{
	TrackerSparqlCursor *cursor;
	GError *error = NULL;
	const gchar *query = "SELECT ?r WHERE {?r a nfo:FileDataObject}";

	cursor = tracker_sparql_connection_query (connection, query, NULL, &error);

	g_assert_true (cursor);
	g_assert_no_error (error);

	g_assert_true (tracker_sparql_cursor_next (cursor, NULL, NULL));

	g_object_unref (cursor);
}

static void
async_query_cb (GObject      *source_object,
                GAsyncResult *result,
                gpointer      user_data)
{
	TrackerSparqlCursor *cursor_fd;
	TrackerSparqlCursor *cursor_glib;
	AsyncData *data = user_data;
	GError *error = NULL;

	g_main_loop_quit (data->main_loop);

	cursor_fd = tracker_sparql_connection_query_finish (connection, result, &error);

	g_assert_no_error (error);
	g_assert_true (cursor_fd != NULL);

	cursor_glib = tracker_sparql_connection_query (connection, data->query, NULL, &error);

	g_assert_no_error (error);
	g_assert_true (cursor_glib != NULL);

	compare_cursors (cursor_glib, cursor_fd);

	g_object_unref (cursor_fd);
	g_object_unref (cursor_glib);
}

static void
test_tracker_sparql_query_iterate_async (gpointer      fixture,
                                         gconstpointer user_data)
{
	const gchar *query = "SELECT ?r nie:url(?r) WHERE {?r a nfo:FileDataObject}";
	GMainLoop *main_loop;
	AsyncData *data;

	main_loop = g_main_loop_new (NULL, FALSE);

	data = g_slice_new (AsyncData);
	data->main_loop = main_loop;
	data->query = query;

	tracker_sparql_connection_query_async (connection,
	                                       query,
	                                       NULL,
	                                       async_query_cb,
	                                       data);

	g_main_loop_run (main_loop);

	g_slice_free (AsyncData, data);
	g_main_loop_unref (main_loop);
}

static void
cancel_query_cb (GObject      *source_object,
                 GAsyncResult *result,
                 gpointer      user_data)
{
	GMainLoop *main_loop = user_data;
	GError *error = NULL;

	g_main_loop_quit (main_loop);

	tracker_sparql_connection_query_finish (connection, result, &error);

	/* An error should be returned (cancelled!) */
	g_assert_true (error);
}

static void
test_tracker_sparql_query_iterate_async_cancel (gpointer      fixture,
                                                gconstpointer user_data)
{
	const gchar *query = "SELECT ?r nie:url(?r) WHERE {?r a nfo:FileDataObject}";
	GMainLoop *main_loop;
	GCancellable *cancellable = g_cancellable_new ();

	main_loop = g_main_loop_new (NULL, FALSE);

	tracker_sparql_connection_query_async (connection,
	                                       query,
	                                       cancellable,
	                                       cancel_query_cb,
	                                       main_loop);

	g_cancellable_cancel (cancellable);

	g_main_loop_run (main_loop);

	g_main_loop_unref (main_loop);
}

TrackerSparqlConnection *
create_local_connection (GError **error)
{
        TrackerSparqlConnection *conn;
        GFile *store, *ontology;
        gchar *path;

        path = g_build_filename (g_get_tmp_dir (), "libtracker-sparql-test-XXXXXX", NULL);
        g_mkdtemp_full (path, 0700);
        store = g_file_new_for_path (path);
        g_free (path);

        ontology = g_file_new_for_path (TEST_ONTOLOGIES_DIR);

        conn = tracker_sparql_connection_new (0, store, ontology, NULL, error);
        g_object_unref (store);
        g_object_unref (ontology);

        return conn;
}

static gpointer
thread_func (gpointer user_data)
{
	GDBusConnection *dbus_conn = user_data;
	TrackerSparqlConnection *direct;
	TrackerEndpointDBus *endpoint;
	GMainContext *context;
	GMainLoop *main_loop;

	context = g_main_context_new ();
	g_main_context_push_thread_default (context);

	main_loop = g_main_loop_new (context, FALSE);

	direct = create_local_connection (NULL);
	if (!direct)
		return NULL;

	endpoint = tracker_endpoint_dbus_new (direct, dbus_conn, NULL, NULL, NULL);
	if (!endpoint)
		return NULL;

	started = TRUE;
	g_main_loop_run (main_loop);

	return NULL;
}

static TrackerSparqlConnection *
create_dbus_connection (GError **error)
{
	TrackerSparqlConnection *dbus;
	GDBusConnection *dbus_conn;
	GThread *thread;

	dbus_conn = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, error);
	if (!dbus_conn)
		return NULL;

	thread = g_thread_new (NULL, thread_func, dbus_conn);

	while (!started)
		g_usleep (100);

	dbus = tracker_sparql_connection_bus_new (g_dbus_connection_get_unique_name (dbus_conn),
						  NULL, dbus_conn, error);
	g_thread_unref (thread);

	return dbus;
}

static void test_tracker_sparql_cursor_next_async_query (TrackerSparqlConnection *connection,
                                                         guint                    query);

static void
test_tracker_sparql_cursor_next_async_cb (GObject      *source,
                                          GAsyncResult *result,
                                          gpointer      user_data)
{
	TrackerSparqlConnection *connection;
	TrackerSparqlCursor *cursor;
	GCancellable *cancellable;
	GError *error = NULL;
	gboolean success;
	static gint next = 0;
	gint next_to_cancel = 1;
	gint query;

	query = GPOINTER_TO_INT(user_data);
	cancellable = g_task_get_cancellable (G_TASK (result));

	g_assert_true (result != NULL);
	success = tracker_sparql_cursor_next_finish (TRACKER_SPARQL_CURSOR (source),
	                                             result,
	                                             &error);

	if (query == 1 && next == next_to_cancel) {
		g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
		g_print ("# Got Cancellation GError\n");
	} else {
		g_assert_no_error (error);
	}

	cursor = TRACKER_SPARQL_CURSOR (source);
	g_assert_true (cursor != NULL);
	connection = tracker_sparql_cursor_get_connection (cursor);

	if (!success) {
		query++;
		next = 0;

		if (query == 1 || query == 2) {
			test_tracker_sparql_cursor_next_async_query (connection,
			                                             query);
		} else if (query == 3) {
			g_main_loop_quit (main_loop);
		}
	} else {
		next++;

		/* Random number here for next_count_to_cancel is "2",
		 * just want to do this mid-cursor iteration
		 */
		if (next == next_to_cancel && query == 1) {
			/* Cancel */
			g_print ("# Cancelling cancellable: at count:%d\n", next);
			g_cancellable_cancel (cancellable);
		}

		tracker_sparql_cursor_next_async (cursor,
		                                  cancellable,
		                                  test_tracker_sparql_cursor_next_async_cb,
		                                  GINT_TO_POINTER (query));
	}
}

static void
test_tracker_sparql_cursor_next_async_query (TrackerSparqlConnection *connection,
                                             guint                    query)
{
	TrackerSparqlCursor *cursor;
	GError *error = NULL;
	GCancellable *cancellable;

	g_assert_true (query < G_N_ELEMENTS (queries));
	g_print ("# ASYNC query %d starting:\n", query);

	cancellable = g_cancellable_new ();
	g_assert_true (cancellable != NULL);

	cursor = tracker_sparql_connection_query (connection,
	                                          queries[query],
	                                          NULL,
	                                          &error);
	g_assert_no_error (error);
	g_assert_true (cursor != NULL);

	tracker_sparql_cursor_next_async (cursor,
	                                  cancellable,
	                                  test_tracker_sparql_cursor_next_async_cb,
	                                  GINT_TO_POINTER(query));
}

static void
test_tracker_sparql_cursor_next_async (gpointer      fixture,
                                       gconstpointer user_data)
{
	TrackerSparqlConnection *connection;
	GError *error = NULL;

	main_loop = g_main_loop_new (NULL, TRUE);

	/* So, the idea here:
	 * 1. Test async cursor_next() call.
	 * 2. Make sure we can cancel a cursor_next() call and start a new query (was failing)
	 * 3. Handle multiple async queries + async cursor_next() calls.
	 */

	connection = create_local_connection (&error);
	g_assert_no_error (error);
	g_assert_true (connection != NULL);

	test_tracker_sparql_cursor_next_async_query (connection, 0);
	g_main_loop_run (main_loop);
}

static void
test_tracker_sparql_cursor_get_variable_name (gpointer      fixture,
                                              gconstpointer user_data)
{
	TrackerSparqlConnection *connection;
	TrackerSparqlCursor *cursor;
	GError *error = NULL;

	connection = create_local_connection (&error);
	g_assert_no_error (error);
	g_assert_true (connection != NULL);

	cursor = tracker_sparql_connection_query (connection,
						  "SELECT ?urn ?added ?label ?unbound { "
						  "  ?urn nrl:added ?added ; "
						  "       rdfs:label ?label . "
						  "} LIMIT 1",
						  NULL, &error);
	g_assert_no_error (error);
	g_assert_true (cursor != NULL);

	tracker_sparql_cursor_next (cursor, NULL, &error);
	g_assert_no_error (error);

	g_assert_cmpstr (tracker_sparql_cursor_get_variable_name (cursor, 0),
			 ==,
			 "urn");
	g_assert_cmpstr (tracker_sparql_cursor_get_variable_name (cursor, 1),
			 ==,
			 "added");
	g_assert_cmpstr (tracker_sparql_cursor_get_variable_name (cursor, 2),
			 ==,
			 "label");
	g_assert_cmpstr (tracker_sparql_cursor_get_variable_name (cursor, 3),
			 ==,
			 "unbound");

	tracker_sparql_cursor_close (cursor);
	tracker_sparql_connection_close (connection);
}

static void
test_tracker_sparql_cursor_get_value_type (gpointer      fixture,
                                           gconstpointer user_data)
{
	TrackerSparqlConnection *connection;
	TrackerSparqlCursor *cursor;
	GError *error = NULL;

	connection = create_local_connection (&error);
	g_assert_no_error (error);
	g_assert_true (connection != NULL);

	cursor = tracker_sparql_connection_query (connection,
						  "SELECT ?urn ?added ?label ?unbound { "
						  "  ?urn nrl:added ?added ; "
						  "       rdfs:label ?label . "
						  "} LIMIT 1",
						  NULL, &error);
	g_assert_no_error (error);
	g_assert_true (cursor != NULL);

	tracker_sparql_cursor_next (cursor, NULL, &error);
	g_assert_no_error (error);

	g_assert_cmpint (tracker_sparql_cursor_get_value_type (cursor, 0),
			 ==,
			 TRACKER_SPARQL_VALUE_TYPE_URI);
	g_assert_cmpint (tracker_sparql_cursor_get_value_type (cursor, 1),
			 ==,
			 TRACKER_SPARQL_VALUE_TYPE_DATETIME);
	g_assert_cmpint (tracker_sparql_cursor_get_value_type (cursor, 2),
			 ==,
			 TRACKER_SPARQL_VALUE_TYPE_STRING);
	g_assert_cmpint (tracker_sparql_cursor_get_value_type (cursor, 3),
			 ==,
			 TRACKER_SPARQL_VALUE_TYPE_UNBOUND);

	tracker_sparql_cursor_close (cursor);
	tracker_sparql_connection_close (connection);
}

typedef struct {
	const gchar *name;
	GTestFixtureFunc func;
} TestInfo;

TestInfo tests[] = {
	{ "tracker_sparql_query_iterate", test_tracker_sparql_query_iterate },
	{ "tracker_sparql_query_iterate_largerow", test_tracker_sparql_query_iterate_largerow },
	{ "tracker_sparql_query_iterate_error", test_tracker_sparql_query_iterate_error },
	{ "tracker_sparql_query_iterate_empty", test_tracker_sparql_query_iterate_empty },
	{ "tracker_sparql_query_iterate_sigpipe", test_tracker_sparql_query_iterate_sigpipe },
	{ "tracker_sparql_query_iterate_async", test_tracker_sparql_query_iterate_async },
	{ "tracker_sparql_query_iterate_async_cancel", test_tracker_sparql_query_iterate_async_cancel },
	{ "tracker_sparql_cursor_next_async", test_tracker_sparql_cursor_next_async },
	{ "tracker_sparql_cursor_get_variable_name", test_tracker_sparql_cursor_get_variable_name },
	{ "tracker_sparql_cursor_get_value_type", test_tracker_sparql_cursor_get_value_type },
};

static void
add_tests (void)
{
	guint i;

	for (i = 0; i < G_N_ELEMENTS (tests); i++) {
		gchar *test_name;

		test_name = g_strdup_printf ("/libtracker-sparql/cursor/%s", tests[i].name);
		g_test_add (test_name,
		            gpointer, NULL,
		            insert_test_data,
		            (void (*) (gpointer *, gconstpointer)) tests[i].func,
		            delete_test_data);
		g_free (test_name);
	}
}

gint
main (gint argc, gchar **argv)
{
	g_test_init (&argc, &argv, NULL);

	connection = create_dbus_connection (NULL);

	add_tests ();

	return g_test_run ();
}
