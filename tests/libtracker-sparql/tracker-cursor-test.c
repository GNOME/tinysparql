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

#include <tinysparql.h>

/* This MUST be larger than TRACKER_STEROIDS_BUFFER_SIZE */
#define LONG_NAME_SIZE 128 * 1024 * sizeof(char)

typedef struct {
	GMainLoop *main_loop;
	const gchar *query;
} AsyncData;

static int http_port;

static TrackerSparqlConnection *direct;
static TrackerSparqlConnection *dbus;
static TrackerSparqlConnection *http;
static TrackerEndpointDBus *endpoint_bus;
static TrackerEndpointHttp *endpoint_http;
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

	tracker_sparql_connection_update (direct, delete_query, NULL, &error);
	g_assert_no_error (error);
}

static void
insert_test_data (gpointer      *fixture,
                  gconstpointer  user_data)
{
	GError *error = NULL;
	char *long_name = g_malloc (LONG_NAME_SIZE);
	char *filled_query;

	memset (long_name, 'a', LONG_NAME_SIZE);
	long_name[LONG_NAME_SIZE - 1] = '\0';

	filled_query = g_strdup_printf ("INSERT {"
	                                "    <urn:testdata1> a nfo:FileDataObject ; nie:url \"/foo/bar\" ."
	                                "    <urn:testdata2> a nfo:FileDataObject ; nie:url \"/plop/coin\" ."
	                                "    <urn:testdata3> a nmm:Artist ; nmm:artistName \"testArtist\" ."
	                                "    <urn:testdata4> a nmm:Photo ; nao:identifier \"%s\" ."
	                                "}", long_name);

	tracker_sparql_connection_update (direct, filled_query, NULL, &error);
	g_assert_no_error (error);

	g_free (filled_query);
	g_free (long_name);
}

static void
compare_cursors (TrackerSparqlCursor *cursor_a,
                 TrackerSparqlCursor *cursor_b)
{
	gint col, n_cols;

	g_assert_cmpint (tracker_sparql_cursor_get_n_columns (cursor_a),
	                 ==,
	                 tracker_sparql_cursor_get_n_columns (cursor_b));

	n_cols = tracker_sparql_cursor_get_n_columns (cursor_a);

	while (tracker_sparql_cursor_next (cursor_a, NULL, NULL) && tracker_sparql_cursor_next (cursor_b, NULL, NULL)) {
		for (col = 0; col < n_cols; col++) {
			g_assert_cmpstr (tracker_sparql_cursor_get_variable_name (cursor_a, col),
			                 ==,
			                 tracker_sparql_cursor_get_variable_name (cursor_b, col));

			g_assert_cmpstr (tracker_sparql_cursor_get_string (cursor_a, col, NULL),
			                 ==,
			                 tracker_sparql_cursor_get_string (cursor_b, col, NULL));

			g_assert_cmpint (tracker_sparql_cursor_is_bound (cursor_a, col),
			                 ==,
			                 tracker_sparql_cursor_is_bound (cursor_b, col));

			g_assert_cmpint (tracker_sparql_cursor_get_value_type (cursor_a, col),
			                 ==,
			                 tracker_sparql_cursor_get_value_type (cursor_b, col));
		}
	}

	/* Check that both cursors are at the end (same number of rows) */
	g_assert_true (!tracker_sparql_cursor_next (cursor_a, NULL, NULL));
	g_assert_true (!tracker_sparql_cursor_next (cursor_b, NULL, NULL));
}

static void
query_and_compare_results (TrackerSparqlConnection *conn,
                           const char              *query)
{
	TrackerSparqlCursor *cursor_check, *cursor;
	GError *error = NULL;

	cursor_check = tracker_sparql_connection_query (direct, query, NULL, &error);
	g_assert_no_error (error);
	g_assert_true (tracker_sparql_cursor_get_connection (cursor_check) == direct);

	cursor = tracker_sparql_connection_query (conn, query, NULL, &error);
	g_assert_no_error (error);
	g_assert_true (tracker_sparql_cursor_get_connection (cursor) == conn);

	compare_cursors (cursor_check, cursor);

	g_object_unref (cursor_check);
	g_object_unref (cursor);
}

static void
test_tracker_sparql_query_iterate (gpointer      fixture,
                                   gconstpointer user_data)
{
	TrackerSparqlConnection *conn = (TrackerSparqlConnection *) user_data;

	query_and_compare_results (conn, "SELECT ?r nie:url(?r) WHERE {?r a nfo:FileDataObject}");
}

static void
test_tracker_sparql_query_iterate_largerow (gpointer      fixture,
                                            gconstpointer user_data)
{
	TrackerSparqlConnection *conn = (TrackerSparqlConnection *) user_data;

	query_and_compare_results (conn, "SELECT nao:identifier(?r) WHERE {?r a nmm:Photo}");
}

/* Runs an invalid query */
static void
test_tracker_sparql_query_iterate_error (gpointer      fixture,
                                         gconstpointer user_data)
{
	TrackerSparqlConnection *conn = (TrackerSparqlConnection *) user_data;
	TrackerSparqlCursor *cursor;
	GError *error = NULL;
	const gchar *query = "bork bork bork";

	cursor = tracker_sparql_connection_query (conn, query, NULL, &error);
	/* tracker_sparql_query_iterate should return null on error */
	g_assert_null (cursor);

	/* error should be set, along with its message, note: we don't
	 * use g_assert_error() because the code does not match the
	 * enum values for TRACKER_SPARQL_ERROR_*, this is due to
	 * dbus/error matching between client/server. This should be
	 * fixed in gdbus.
	 */
	g_assert_nonnull (error);
	//g_assert_true (error->domain == TRACKER_SPARQL_ERROR);

	g_error_free (error);
}

static void
test_tracker_sparql_query_iterate_empty (gpointer      fixture,
                                         gconstpointer user_data)
{
	TrackerSparqlConnection *conn = (TrackerSparqlConnection *) user_data, *prop_conn;
	TrackerSparqlCursor *cursor;
	GError *error = NULL;
	const gchar *query = "SELECT ?r WHERE {?r a nfo:FileDataObject; nao:identifier \"thisannotationdoesnotexist\"}";
	gint prop_cols;

	cursor = tracker_sparql_connection_query (conn, query, NULL, &error);
	g_assert_true (cursor);
	g_assert_no_error (error);
	g_assert_true (tracker_sparql_cursor_get_connection (cursor) == conn);

	g_assert_false (tracker_sparql_cursor_next (cursor, NULL, NULL));
	g_assert_true (tracker_sparql_cursor_get_n_columns (cursor) == 1);

	g_object_get (G_OBJECT (cursor),
	              "n-columns", &prop_cols,
	              "connection", &prop_conn,
	              NULL);

	g_assert_cmpint (prop_cols, ==, tracker_sparql_cursor_get_n_columns (cursor));
	g_assert_true (prop_conn == conn);

	g_object_unref (cursor);
}

/* Closes the cursor before all results are read */
static void
test_tracker_sparql_query_iterate_close_early (gpointer      fixture,
                                               gconstpointer user_data)
{
	TrackerSparqlConnection *conn = (TrackerSparqlConnection *) user_data;
	TrackerSparqlCursor *cursor;
	GError *error = NULL;
	const gchar *query = "SELECT ?r WHERE {?r a nfo:FileDataObject}";

	cursor = tracker_sparql_connection_query (conn, query, NULL, &error);
	g_assert_true (cursor);
	g_assert_no_error (error);
	g_assert_true (tracker_sparql_cursor_get_connection (cursor) == conn);

	g_assert_true (tracker_sparql_cursor_next (cursor, NULL, NULL));

	g_object_unref (cursor);
}

static void
async_query_cb (GObject      *source_object,
                GAsyncResult *result,
                gpointer      user_data)
{
	TrackerSparqlConnection *conn = (TrackerSparqlConnection *) source_object;
	TrackerSparqlCursor *cursor_check, *cursor;
	AsyncData *data = user_data;
	GError *error = NULL;

	g_main_loop_quit (data->main_loop);

	cursor = tracker_sparql_connection_query_finish (conn, result, &error);
	g_assert_no_error (error);
	g_assert_true (cursor != NULL);
	g_assert_true (tracker_sparql_cursor_get_connection (cursor) == conn);

	cursor_check = tracker_sparql_connection_query (direct, data->query, NULL, &error);
	g_assert_no_error (error);
	g_assert_true (cursor_check != NULL);
	g_assert_true (tracker_sparql_cursor_get_connection (cursor_check) == direct);

	compare_cursors (cursor_check, cursor);

	g_object_unref (cursor_check);
	g_object_unref (cursor);
}

static void
test_tracker_sparql_query_iterate_async (gpointer      fixture,
                                         gconstpointer user_data)
{
	TrackerSparqlConnection *conn = (TrackerSparqlConnection *) user_data;
	const gchar *query = "SELECT ?r nie:url(?r) WHERE {?r a nfo:FileDataObject}";
	GMainLoop *main_loop;
	AsyncData *data;

	main_loop = g_main_loop_new (NULL, FALSE);

	data = g_slice_new (AsyncData);
	data->main_loop = main_loop;
	data->query = query;

	tracker_sparql_connection_query_async (conn,
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
	TrackerSparqlConnection *conn = (TrackerSparqlConnection *) source_object;
	GMainLoop *main_loop = user_data;
	GError *error = NULL;

	g_main_loop_quit (main_loop);

	tracker_sparql_connection_query_finish (conn, result, &error);

	/* An error should be returned (cancelled!) */
	g_assert_true (error);
}

static void
test_tracker_sparql_query_iterate_async_cancel (gpointer      fixture,
                                                gconstpointer user_data)
{
	TrackerSparqlConnection *conn = (TrackerSparqlConnection *) user_data;
	const gchar *query = "SELECT ?r nie:url(?r) WHERE {?r a nfo:FileDataObject}";
	GMainLoop *main_loop;
	GCancellable *cancellable = g_cancellable_new ();

	main_loop = g_main_loop_new (NULL, FALSE);

	tracker_sparql_connection_query_async (conn,
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

	ontology = tracker_sparql_get_ontology_nepomuk ();

	conn = tracker_sparql_connection_new (0, store, ontology, NULL, error);

	{
		TrackerSparqlConnectionFlags object_flags;
		GFile *object_store, *object_ontology;

		g_object_get (conn,
		              "flags", &object_flags,
		              "store-location", &object_store,
		              "ontology-location", &object_ontology,
		              NULL);
		g_assert_cmpint (object_flags, ==, 0);
		g_assert_true (object_store == store);
		g_assert_true (object_ontology == ontology);
		g_clear_object (&object_store);
		g_clear_object (&object_ontology);
	}

	g_object_unref (store);
	g_object_unref (ontology);

        return conn;
}

static gpointer
thread_func (gpointer user_data)
{
	GDBusConnection *dbus_conn = user_data;
	GMainContext *context;
	GMainLoop *main_loop;
	GError *error = NULL;

	context = g_main_context_new ();
	g_main_context_push_thread_default (context);

	main_loop = g_main_loop_new (context, FALSE);

	direct = create_local_connection (&error);
	g_assert_no_error (error);

	endpoint_bus = tracker_endpoint_dbus_new (direct, dbus_conn, NULL, NULL, &error);
	g_assert_no_error (error);

	{
		GDBusConnection *object_dbus_conn;
		gchar *object_dbus_path;
		TrackerSparqlConnection *object_sparql_conn;
		gboolean object_readonly;
		gchar **object_allowed_services, **object_allowed_graphs;

		g_object_get (endpoint_bus,
		              "dbus-connection", &object_dbus_conn,
		              "object-path", &object_dbus_path,
		              "sparql-connection", &object_sparql_conn,
		              "readonly", &object_readonly,
		              "allowed-services", &object_allowed_services,
		              "allowed-graphs", &object_allowed_graphs,
		              NULL);
		g_assert_true (object_dbus_conn == dbus_conn);
		/* Check that NULL gets us the default object path */
		g_assert_cmpstr (object_dbus_path, ==, "/org/freedesktop/Tracker3/Endpoint");
		g_assert_true (object_sparql_conn == direct);
		g_assert_false (object_readonly);
		g_assert_null (object_allowed_services);
		g_assert_null (object_allowed_graphs);

		g_clear_object (&object_dbus_conn);
		g_clear_object (&object_sparql_conn);
		g_clear_pointer (&object_dbus_path, g_free);
		g_clear_pointer (&object_allowed_services, g_strfreev);
		g_clear_pointer (&object_allowed_graphs, g_strfreev);

		g_assert_false (tracker_endpoint_get_readonly (TRACKER_ENDPOINT (endpoint_bus)));
		g_assert_null (tracker_endpoint_get_allowed_services (TRACKER_ENDPOINT (endpoint_bus)));
		g_assert_null (tracker_endpoint_get_allowed_graphs (TRACKER_ENDPOINT (endpoint_bus)));
	}

	endpoint_http = tracker_endpoint_http_new (direct, http_port, NULL, NULL, &error);
	g_assert_no_error (error);

	{
		int object_http_port;
		GTlsCertificate *object_certificate;
		TrackerSparqlConnection *object_sparql_conn;
		gboolean object_readonly;
		gchar **object_allowed_services, **object_allowed_graphs;

		g_object_get (endpoint_http,
		              "http-port", &object_http_port,
		              "http-certificate", &object_certificate,
		              "sparql-connection", &object_sparql_conn,
		              "readonly", &object_readonly,
		              "allowed-services", &object_allowed_services,
		              "allowed-graphs", &object_allowed_graphs,
		              NULL);
		g_assert_true (object_http_port == http_port);
		g_assert_null (object_certificate);
		g_assert_true (object_sparql_conn == direct);
		/* Http endpoints are readonly */
		g_assert_true (object_readonly);
		g_assert_null (object_allowed_services);
		g_assert_null (object_allowed_graphs);

		g_clear_object (&object_certificate);
		g_clear_object (&object_sparql_conn);
		g_clear_pointer (&object_allowed_services, g_strfreev);
		g_clear_pointer (&object_allowed_graphs, g_strfreev);

		g_assert_true (tracker_endpoint_get_readonly (TRACKER_ENDPOINT (endpoint_http)));
		g_assert_null (tracker_endpoint_get_allowed_services (TRACKER_ENDPOINT (endpoint_http)));
		g_assert_null (tracker_endpoint_get_allowed_graphs (TRACKER_ENDPOINT (endpoint_http)));
	}

	started = TRUE;
	g_main_loop_run (main_loop);

	g_main_context_pop_thread_default (context);
	g_main_context_unref (context);

	return NULL;
}

static void
create_connections (void)
{
	GDBusConnection *dbus_conn;
	GThread *thread;
	GError *error = NULL;
	gchar *host;

	http_port = g_test_rand_int_range (30000, 60000);

	dbus_conn = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
	g_assert_no_error (error);

	thread = g_thread_new (NULL, thread_func, dbus_conn);

	while (!started)
		g_usleep (100);

	host = g_strdup_printf ("http://127.0.0.1:%d/sparql", http_port);
	http = tracker_sparql_connection_remote_new (host);

	{
		gchar *object_base_uri;

		g_object_get (http,
		              "base-uri", &object_base_uri,
		              NULL);
		g_assert_cmpstr (object_base_uri, ==, host);
		g_clear_pointer (&object_base_uri, g_free);
	}

	g_free (host);

	dbus = tracker_sparql_connection_bus_new (g_dbus_connection_get_unique_name (dbus_conn),
						  NULL, dbus_conn, &error);
	g_assert_no_error (error);

	{
		GDBusConnection *object_dbus_connection;
		gchar *object_bus_name, *object_dbus_path;

		g_object_get (dbus,
		              "bus-name", &object_bus_name,
		              "bus-object-path", &object_dbus_path,
		              "bus-connection", &object_dbus_connection,
		              NULL);

		g_assert_cmpstr (object_bus_name, ==, g_dbus_connection_get_unique_name (dbus_conn));
		/* Expect the default object path */
		g_assert_cmpstr ("/org/freedesktop/Tracker3/Endpoint", ==, object_dbus_path);
		g_assert_true (object_dbus_connection == dbus_conn);
		g_clear_object (&object_dbus_connection);
		g_clear_pointer (&object_bus_name, g_free);
		g_clear_pointer (&object_dbus_path, g_free);
	}

	g_thread_unref (thread);
}

static void test_tracker_sparql_cursor_next_async_query (TrackerSparqlConnection *conn,
                                                         guint                    query);

static void
test_tracker_sparql_cursor_next_async_cb (GObject      *source,
                                          GAsyncResult *result,
                                          gpointer      user_data)
{
	TrackerSparqlCursor *cursor;
	TrackerSparqlConnection *conn;
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
	conn = tracker_sparql_cursor_get_connection (cursor);
	g_assert_true (cursor != NULL);

	if (!success) {
		query++;
		next = 0;

		if (query == 1 || query == 2) {
			test_tracker_sparql_cursor_next_async_query (conn, query);
		} else if (query == 3) {
			g_main_loop_quit (main_loop);
		}

		g_object_unref (cursor);
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
test_tracker_sparql_cursor_next_async_query (TrackerSparqlConnection *conn,
                                             guint                    query)
{
	TrackerSparqlCursor *cursor;
	GError *error = NULL;
	GCancellable *cancellable;

	g_assert_true (query < G_N_ELEMENTS (queries));
	g_print ("# ASYNC query %d starting:\n", query);

	cancellable = g_cancellable_new ();
	g_assert_true (cancellable != NULL);

	cursor = tracker_sparql_connection_query (conn,
	                                          queries[query],
	                                          NULL,
	                                          &error);
	g_assert_no_error (error);
	g_assert_true (cursor != NULL);
	g_assert_true (tracker_sparql_cursor_get_connection (cursor) == conn);

	tracker_sparql_cursor_next_async (cursor,
	                                  cancellable,
	                                  test_tracker_sparql_cursor_next_async_cb,
	                                  GINT_TO_POINTER(query));
}

static void
test_tracker_sparql_cursor_next_async (gpointer      fixture,
                                       gconstpointer user_data)
{
	TrackerSparqlConnection *conn = (TrackerSparqlConnection *) user_data;

	main_loop = g_main_loop_new (NULL, TRUE);

	/* So, the idea here:
	 * 1. Test async cursor_next() call.
	 * 2. Make sure we can cancel a cursor_next() call and start a new query (was failing)
	 * 3. Handle multiple async queries + async cursor_next() calls.
	 */
	test_tracker_sparql_cursor_next_async_query (conn, 0);
	g_main_loop_run (main_loop);
}

static void
test_tracker_sparql_cursor_get_variable_name (gpointer      fixture,
                                              gconstpointer user_data)
{
	TrackerSparqlConnection *conn = (TrackerSparqlConnection *) user_data;
	TrackerSparqlCursor *cursor;
	GError *error = NULL;
	gboolean has_next;

	g_assert_no_error (error);

	cursor = tracker_sparql_connection_query (conn,
						  "SELECT ?urn ?added ?label ?unbound { "
						  "  ?urn nrl:added ?added ; "
						  "       rdfs:label ?label . "
						  "} LIMIT 1",
						  NULL, &error);
	g_assert_no_error (error);
	g_assert_true (cursor != NULL);
	g_assert_true (tracker_sparql_cursor_get_connection (cursor) == conn);

	has_next = tracker_sparql_cursor_next (cursor, NULL, &error);
	g_assert_true (has_next);
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
}

static void
test_tracker_sparql_cursor_get_value_type (gpointer      fixture,
                                           gconstpointer user_data)
{
	TrackerSparqlConnection *conn = (TrackerSparqlConnection *) user_data;
	TrackerSparqlCursor *cursor;
	GError *error = NULL;
	gboolean has_next;

	g_assert_no_error (error);

	cursor = tracker_sparql_connection_query (conn,
						  "SELECT ?urn ?added ?label ?unbound (42 AS ?int) (true AS ?bool) (42.2 AS ?double) (BNODE() AS ?bnode) { "
						  "  ?urn nrl:added ?added ; "
						  "       rdfs:label ?label . "
						  "} LIMIT 1",
						  NULL, &error);
	g_assert_no_error (error);
	g_assert_true (cursor != NULL);
	g_assert_true (tracker_sparql_cursor_get_connection (cursor) == conn);

	has_next = tracker_sparql_cursor_next (cursor, NULL, &error);
	g_assert_true (has_next);
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
	g_assert_cmpint (tracker_sparql_cursor_get_value_type (cursor, 4),
			 ==,
			 TRACKER_SPARQL_VALUE_TYPE_INTEGER);
	g_assert_cmpint (tracker_sparql_cursor_get_value_type (cursor, 5),
			 ==,
			 TRACKER_SPARQL_VALUE_TYPE_BOOLEAN);
	g_assert_cmpint (tracker_sparql_cursor_get_value_type (cursor, 6),
			 ==,
			 TRACKER_SPARQL_VALUE_TYPE_DOUBLE);
	g_assert_cmpint (tracker_sparql_cursor_get_value_type (cursor, 7),
			 ==,
			 TRACKER_SPARQL_VALUE_TYPE_BLANK_NODE);

	tracker_sparql_cursor_close (cursor);
}

static void
test_tracker_sparql_cursor_get_langstring (gpointer      fixture,
                                           gconstpointer user_data)
{
	TrackerSparqlConnection *conn = (TrackerSparqlConnection *) user_data;
	TrackerSparqlCursor *cursor;
	GError *error = NULL;
	const gchar *str, *langtag;
	glong len;
	gboolean has_next;

	cursor = tracker_sparql_connection_query (conn,
	                                          "SELECT ('Hola'@es AS ?greeting) { }",
						  NULL, &error);
	g_assert_no_error (error);
	g_assert_true (cursor != NULL);
	g_assert_true (tracker_sparql_cursor_get_connection (cursor) == conn);

	has_next = tracker_sparql_cursor_next (cursor, NULL, &error);
	g_assert_true (has_next);
	g_assert_no_error (error);

	g_assert_cmpint (tracker_sparql_cursor_get_value_type (cursor, 0),
			 ==, TRACKER_SPARQL_VALUE_TYPE_STRING);
	g_assert_cmpstr (tracker_sparql_cursor_get_variable_name (cursor, 0),
	                 ==, "greeting");

	str = tracker_sparql_cursor_get_langstring (cursor, 0, &langtag, &len);
	g_assert_cmpstr (str, ==, "Hola");
	g_assert_cmpstr (langtag, ==, "es");
	g_assert_cmpint (len, ==, 4);

	str = tracker_sparql_cursor_get_string (cursor, 0, &len);
	g_assert_cmpstr (str, ==, "Hola");
	g_assert_cmpint (len, ==, 4);

	tracker_sparql_cursor_close (cursor);
}

static void
test_tracker_sparql_cursor_after_last_row (gpointer      fixture,
                                           gconstpointer user_data)
{
	TrackerSparqlConnection *conn = (TrackerSparqlConnection *) user_data;
	TrackerSparqlCursor *cursor;
	GError *error = NULL;
	gboolean has_next;

	cursor = tracker_sparql_connection_query (conn,
	                                          "SELECT ('Hola' AS ?greeting) { }",
	                                          NULL, &error);
	g_assert_no_error (error);
	g_assert_nonnull (cursor);
	g_assert_true (tracker_sparql_cursor_get_connection (cursor) == conn);

	has_next = tracker_sparql_cursor_next (cursor, NULL, &error);
	g_assert_no_error (error);
	g_assert_true (has_next);

	/* Check that iterating multiple times after the last cursor does return FALSE, but no error */
	has_next = tracker_sparql_cursor_next (cursor, NULL, &error);
	g_assert_false (has_next);
	g_assert_no_error (error);

	has_next = tracker_sparql_cursor_next (cursor, NULL, &error);
	g_assert_false (has_next);
	g_assert_no_error (error);

	/* Check cursor API after last next() */
	g_assert_null (tracker_sparql_cursor_get_string (cursor, 0, NULL));
	g_assert_cmpstr (tracker_sparql_cursor_get_variable_name (cursor, 0), ==, "greeting");
	g_assert_cmpint (tracker_sparql_cursor_get_value_type (cursor, 0), ==, TRACKER_SPARQL_VALUE_TYPE_UNBOUND);
	g_assert_cmpint (tracker_sparql_cursor_is_bound (cursor, 0), ==, FALSE);
	g_assert_cmpint (tracker_sparql_cursor_get_n_columns (cursor), ==, 1);

	tracker_sparql_cursor_close (cursor);
	g_clear_object (&cursor);
}

static void
test_tracker_sparql_cursor_after_last_column (gpointer      fixture,
                                              gconstpointer user_data)
{
	TrackerSparqlConnection *conn = (TrackerSparqlConnection *) user_data;
	TrackerSparqlCursor *cursor;
	GError *error = NULL;
	gboolean has_next;

	cursor = tracker_sparql_connection_query (conn,
	                                          "SELECT ('Hola' AS ?greeting) { }",
	                                          NULL, &error);
	g_assert_no_error (error);
	g_assert_true (cursor != NULL);
	g_assert_true (tracker_sparql_cursor_get_connection (cursor) == conn);

	has_next = tracker_sparql_cursor_next (cursor, NULL, &error);
	g_assert_no_error (error);
	g_assert_true (has_next);

	/* Check cursor API after last column */
	g_assert_null (tracker_sparql_cursor_get_string (cursor, 1, NULL));
	g_assert_null (tracker_sparql_cursor_get_variable_name (cursor, 1));
	g_assert_cmpint (tracker_sparql_cursor_get_value_type (cursor, 1), ==, TRACKER_SPARQL_VALUE_TYPE_UNBOUND);
	g_assert_cmpint (tracker_sparql_cursor_is_bound (cursor, 1), ==, FALSE);

	tracker_sparql_cursor_close (cursor);
	g_clear_object (&cursor);
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
	{ "tracker_sparql_query_iterate_close_early", test_tracker_sparql_query_iterate_close_early },
	{ "tracker_sparql_query_iterate_async", test_tracker_sparql_query_iterate_async },
	{ "tracker_sparql_query_iterate_async_cancel", test_tracker_sparql_query_iterate_async_cancel },
	{ "tracker_sparql_cursor_next_async", test_tracker_sparql_cursor_next_async },
	{ "tracker_sparql_cursor_get_variable_name", test_tracker_sparql_cursor_get_variable_name },
	{ "tracker_sparql_cursor_get_value_type", test_tracker_sparql_cursor_get_value_type },
	{ "tracker_sparql_cursor_get_langstring", test_tracker_sparql_cursor_get_langstring },
	{ "tracker_sparql_cursor_after_last_row", test_tracker_sparql_cursor_after_last_row },
	{ "tracker_sparql_cursor_after_last_column", test_tracker_sparql_cursor_after_last_column },
};

static void
add_tests (const gchar             *conn_name,
           TrackerSparqlConnection *conn)
{
	guint i;

	for (i = 0; i < G_N_ELEMENTS (tests); i++) {
		gchar *test_name;

		test_name = g_strdup_printf ("/libtracker-sparql/cursor/%s/%s",
		                             conn_name, tests[i].name);
		g_test_add (test_name,
		            gpointer, conn,
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

	create_connections ();

	add_tests ("direct", direct);
	add_tests ("dbus", dbus);
	add_tests ("http", http);

	return g_test_run ();
}
