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

#include <tinysparql.h>

static int http_port;

static TrackerSparqlConnection *direct;
static TrackerSparqlConnection *dbus;
static TrackerSparqlConnection *http;
static TrackerEndpointDBus *endpoint_bus;
static TrackerEndpointHttp *endpoint_http;
gboolean started = FALSE;

static GMainLoop *main_loop = NULL;
static GThread *thread = NULL;

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
	g_object_unref (store);
	g_object_unref (ontology);

	return conn;
}

static gpointer
thread_func (gpointer user_data)
{
	GDBusConnection *dbus_conn = user_data;
	GMainContext *context;
	GError *error = NULL;

	context = g_main_context_new ();
	g_main_context_push_thread_default (context);

	main_loop = g_main_loop_new (context, FALSE);

	direct = create_local_connection (&error);
	g_assert_no_error (error);

	endpoint_bus = tracker_endpoint_dbus_new (direct, dbus_conn, NULL, NULL, &error);
	g_assert_no_error (error);

	endpoint_http = tracker_endpoint_http_new (direct, http_port, NULL, NULL, &error);
	g_assert_no_error (error);

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
	g_free (host);

	dbus = tracker_sparql_connection_bus_new (g_dbus_connection_get_unique_name (dbus_conn),
	                                          NULL, dbus_conn, &error);
	g_assert_no_error (error);
}

static void
setup (gpointer      *fixture,
       gconstpointer  userdata)
{
	create_connections ();
}

static void
teardown (gpointer      *fixture,
          gconstpointer  userdata)
{
	g_main_loop_quit (main_loop);
	g_thread_join (thread);

	g_clear_object (&endpoint_bus);
	g_clear_object (&endpoint_http);
	g_clear_object (&dbus);
	g_clear_object (&http);
	g_clear_object (&direct);
	started = FALSE;
}

static void
query_closed_cb (GObject      *object,
                 GAsyncResult *res,
                 gpointer      user_data)
{
	GError *error = NULL;
	TrackerSparqlCursor *cursor;

	cursor = tracker_sparql_connection_query_finish (TRACKER_SPARQL_CONNECTION (object),
	                                                 res, &error);
	g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CONNECTION_CLOSED);
	g_assert_null (cursor);
	g_clear_error (&error);

	g_main_loop_quit (user_data);
}

static void
update_closed_cb (GObject      *object,
                  GAsyncResult *res,
                  gpointer      user_data)
{
	GError *error = NULL;

	tracker_sparql_connection_update_finish (TRACKER_SPARQL_CONNECTION (object),
	                                         res, &error);
	g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CONNECTION_CLOSED);
	g_clear_error (&error);

	g_main_loop_quit (user_data);
}

static void
assert_operations_after_close (TrackerSparqlConnection *conn)
{
	TrackerResource *resource;
	GError *error = NULL;
	GMainLoop *main_loop;

	resource = tracker_resource_new ("file:///");
	tracker_resource_set_uri (resource, "rdf:type", "rdfs:Resource");

	main_loop = g_main_loop_new (NULL, FALSE);

	/* Test queries */
	tracker_sparql_connection_query (conn, "", NULL, &error);
	g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CONNECTION_CLOSED);
	g_clear_error (&error);

	tracker_sparql_connection_query_async (conn, "", NULL, query_closed_cb, main_loop);
	g_main_loop_run (main_loop);

	if (g_strcmp0 (G_OBJECT_TYPE_NAME (conn), "TrackerRemoteConnection") != 0) {
		/* Test updates */
		tracker_sparql_connection_update (conn, "", NULL, &error);
		g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CONNECTION_CLOSED);
		g_clear_error (&error);

		G_GNUC_BEGIN_IGNORE_DEPRECATIONS
			tracker_sparql_connection_update_blank (conn, "", NULL, &error);
		g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CONNECTION_CLOSED);
		g_clear_error (&error);
		G_GNUC_END_IGNORE_DEPRECATIONS

			tracker_sparql_connection_update_resource (conn, NULL, resource, NULL, &error);
		g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CONNECTION_CLOSED);
		g_clear_error (&error);

		tracker_sparql_connection_update_async (conn, "", NULL, update_closed_cb, main_loop);
		g_main_loop_run (main_loop);
	}

	g_clear_object (&resource);
	g_clear_pointer (&main_loop, g_main_loop_unref);
}

static void
test_connection_close (gpointer      fixture,
                       gconstpointer user_data)
{
	TrackerSparqlConnection *conn = *((TrackerSparqlConnection **) user_data);

	tracker_sparql_connection_close (conn);

	/* Test that it may close multiple times */
	tracker_sparql_connection_close (conn);

	/* Test queries and updates */
	assert_operations_after_close (conn);
}

static void
close_async_cb (GObject      *source,
                GAsyncResult *res,
                gpointer      user_data)
{
	GError *error = NULL;
	gboolean retval;

	retval = tracker_sparql_connection_close_finish (TRACKER_SPARQL_CONNECTION (source),
	                                                 res, &error);

	g_assert_no_error (error);
	g_assert_true (retval);
	g_main_loop_quit (user_data);
}

static void
test_connection_close_async (gpointer      fixture,
                             gconstpointer user_data)
{
	TrackerSparqlConnection *conn = *((TrackerSparqlConnection **) user_data);
	GMainLoop *loop;

	loop = g_main_loop_new (NULL, FALSE);
	tracker_sparql_connection_close_async (conn, NULL, close_async_cb, loop);
	g_main_loop_run (loop);

	/* Test that it may close multiple times */
	tracker_sparql_connection_close_async (conn, NULL, close_async_cb, loop);
	g_main_loop_run (loop);

	g_main_loop_unref (loop);

	/* Test queries and updates */
	assert_operations_after_close (conn);
}

static TrackerResource *
create_test_resource (void)
{
	TrackerResource *resource;

	resource = tracker_resource_new ("urn:testdata1");
	tracker_resource_add_uri (resource, "rdf:type", "nie:DataObject");
	tracker_resource_set_string (resource, "nie:url", "file:///");

	return resource;
}

static TrackerResource *
create_test_resource_invalid (void)
{
	TrackerResource *resource;

	resource = tracker_resource_new ("urn:testdata2");
	tracker_resource_set_string (resource, "nie:asdfasdf", "file:///");

	return resource;
}

static void
test_connection_update_resource (gpointer      fixture,
                                 gconstpointer user_data)
{
	TrackerSparqlConnection *conn = *((TrackerSparqlConnection **) user_data);
	TrackerResource *resource;
	TrackerSparqlCursor *cursor;
	GError *error = NULL;
	gboolean retval;

	resource = create_test_resource ();

	retval = tracker_sparql_connection_update_resource (conn,
	                                                    NULL,
	                                                    resource,
	                                                    NULL,
	                                                    &error);
	if (g_strcmp0 (G_OBJECT_TYPE_NAME (conn), "TrackerRemoteConnection") == 0) {
		/* HTTP connections cannot perform udpates */
		g_assert_error (error, TRACKER_SPARQL_ERROR, TRACKER_SPARQL_ERROR_UNSUPPORTED);
		g_assert_false (retval);
		return;
	} else {
		g_assert_no_error (error);
		g_assert_true (retval);
	}

	retval = tracker_sparql_connection_update_resource (conn,
	                                                    "nrl:TestGraph",
	                                                    resource,
	                                                    NULL,
	                                                    &error);
	g_assert_no_error (error);
	g_assert_true (retval);

	cursor = tracker_sparql_connection_query (conn,
	                                          "ASK {"
	                                          "  <urn:testdata1> a rdfs:Resource ."
	                                          "}",
	                                          NULL, &error);
	g_assert_no_error (error);
	g_assert_nonnull (cursor);

	g_assert_true (tracker_sparql_cursor_next (cursor, NULL, &error));
	g_assert_no_error (error);
	g_assert_true (tracker_sparql_cursor_get_boolean (cursor, 0));

	cursor = tracker_sparql_connection_query (conn,
	                                          "ASK {"
	                                          "  GRAPH nrl:TestGraph {"
	                                          "    <urn:testdata1> a rdfs:Resource ."
	                                          "  }"
	                                          "}",
	                                          NULL, &error);
	g_assert_no_error (error);
	g_assert_nonnull (cursor);

	g_assert_true (tracker_sparql_cursor_next (cursor, NULL, &error));
	g_assert_no_error (error);
	g_assert_true (tracker_sparql_cursor_get_boolean (cursor, 0));

	/* Check that update errors work */
	g_clear_object (&resource);
	resource = create_test_resource_invalid ();
	retval = tracker_sparql_connection_update_resource (conn,
	                                                    NULL,
	                                                    resource,
	                                                    NULL,
	                                                    &error);
	g_assert_error (error, TRACKER_SPARQL_ERROR, TRACKER_SPARQL_ERROR_UNKNOWN_PROPERTY);
	g_assert_false (retval);
}

typedef struct {
	GMainLoop *loop;
	gboolean retval;
	GError *error;
} UpdateAsyncData;

static void
update_resource_cb (GObject      *source,
                    GAsyncResult *res,
                    gpointer      user_data)
{
	UpdateAsyncData *data = user_data;

	data->retval =
		tracker_sparql_connection_update_resource_finish (TRACKER_SPARQL_CONNECTION (source),
		                                                  res, &data->error);
	g_main_loop_quit (data->loop);
}

static void
test_connection_update_resource_async (gpointer      fixture,
                                       gconstpointer user_data)
{
	TrackerSparqlConnection *conn = *((TrackerSparqlConnection **) user_data);
	TrackerResource *resource;
	TrackerSparqlCursor *cursor;
	GError *error = NULL;
	UpdateAsyncData data = { 0 };

	data.loop = g_main_loop_new (NULL, FALSE);

	resource = create_test_resource ();

	tracker_sparql_connection_update_resource_async (conn,
	                                                 NULL,
	                                                 resource,
	                                                 NULL,
	                                                 update_resource_cb,
	                                                 &data);
	g_main_loop_run (data.loop);

	if (g_strcmp0 (G_OBJECT_TYPE_NAME (conn), "TrackerRemoteConnection") == 0) {
		/* HTTP connections cannot perform updates */
		g_assert_error (data.error, TRACKER_SPARQL_ERROR, TRACKER_SPARQL_ERROR_UNSUPPORTED);
		g_assert_false (data.retval);
		return;
	} else {
		g_assert_no_error (data.error);
		g_assert_true (data.retval);
	}

	tracker_sparql_connection_update_resource_async (conn,
	                                                 "nrl:TestGraph",
	                                                 resource,
	                                                 NULL,
	                                                 update_resource_cb,
	                                                 &data);
	g_main_loop_run (data.loop);

	g_assert_no_error (data.error);
	g_assert_true (data.retval);

	cursor = tracker_sparql_connection_query (conn,
	                                          "ASK {"
	                                          "  <urn:testdata1> a rdfs:Resource ."
	                                          "}",
	                                          NULL, &error);
	g_assert_no_error (error);
	g_assert_nonnull (cursor);

	g_assert_true (tracker_sparql_cursor_next (cursor, NULL, &error));
	g_assert_no_error (error);
	g_assert_true (tracker_sparql_cursor_get_boolean (cursor, 0));

	cursor = tracker_sparql_connection_query (conn,
	                                          "ASK {"
	                                          "  GRAPH nrl:TestGraph {"
	                                          "    <urn:testdata1> a rdfs:Resource ."
	                                          "  }"
	                                          "}",
	                                          NULL, &error);
	g_assert_no_error (error);
	g_assert_nonnull (cursor);

	g_assert_true (tracker_sparql_cursor_next (cursor, NULL, &error));
	g_assert_no_error (error);
	g_assert_true (tracker_sparql_cursor_get_boolean (cursor, 0));

	/* Check that update errors work */
	g_clear_object (&resource);
	resource = create_test_resource_invalid ();
	tracker_sparql_connection_update_resource_async (conn,
	                                                 NULL,
	                                                 resource,
	                                                 NULL,
	                                                 update_resource_cb,
	                                                 &data);
	g_main_loop_run (data.loop);

	g_assert_error (data.error, TRACKER_SPARQL_ERROR, TRACKER_SPARQL_ERROR_UNKNOWN_PROPERTY);
	g_assert_false (data.retval);
}

static void
update_array_cb (GObject      *source,
                 GAsyncResult *res,
                 gpointer      user_data)
{
	UpdateAsyncData *data = user_data;

	data->retval =
		tracker_sparql_connection_update_array_finish (TRACKER_SPARQL_CONNECTION (source),
		                                               res, &data->error);
	g_main_loop_quit (data->loop);
}

static void
test_connection_update_array_async (gpointer      fixture,
                                    gconstpointer user_data)
{
	TrackerSparqlConnection *conn = *((TrackerSparqlConnection **) user_data);
	TrackerSparqlCursor *cursor;
	GError *error = NULL;
	UpdateAsyncData data = { 0 };
	gchar *updates[] = {
		"INSERT DATA { <urn:testdata1> a rdfs:Resource }",
		"INSERT DATA { <urn:testdata2> a rdfs:Resource }",
	};

	data.loop = g_main_loop_new (NULL, FALSE);

	tracker_sparql_connection_update_array_async (conn,
	                                              updates,
	                                              G_N_ELEMENTS (updates),
	                                              NULL,
	                                              update_array_cb,
	                                              &data);
	g_main_loop_run (data.loop);

	if (g_strcmp0 (G_OBJECT_TYPE_NAME (conn), "TrackerRemoteConnection") == 0) {
		/* HTTP connections cannot perform updates */
		g_assert_error (data.error, TRACKER_SPARQL_ERROR, TRACKER_SPARQL_ERROR_UNSUPPORTED);
		g_assert_false (data.retval);
		return;
	} else {
		g_assert_no_error (data.error);
		g_assert_true (data.retval);
	}

	cursor = tracker_sparql_connection_query (conn,
	                                          "ASK {"
	                                          "  <urn:testdata1> a rdfs:Resource ."
	                                          "}",
	                                          NULL, &error);
	g_assert_no_error (error);
	g_assert_nonnull (cursor);

	g_assert_true (tracker_sparql_cursor_next (cursor, NULL, &error));
	g_assert_no_error (error);
	g_assert_true (tracker_sparql_cursor_get_boolean (cursor, 0));
}

typedef struct {
	const gchar *name;
	GTestFixtureFunc func;
} TestInfo;

TestInfo tests[] = {
	{ "connection_close", test_connection_close },
	{ "connection_close_async", test_connection_close_async },
	{ "update_resource", test_connection_update_resource },
	{ "update_resource_async", test_connection_update_resource_async },
	{ "update_array_async", test_connection_update_array_async },
};

static void
add_tests (const gchar              *conn_name,
           TrackerSparqlConnection **conn)
{
	guint i;

	for (i = 0; i < G_N_ELEMENTS (tests); i++) {
		gchar *test_name;

		test_name = g_strdup_printf ("/libtracker-sparql/connection/%s/%s",
		                             conn_name, tests[i].name);
		g_test_add (test_name,
		            gpointer, conn,
		            setup,
		            (void (*) (gpointer *, gconstpointer)) tests[i].func,
		            teardown);
		g_free (test_name);
	}
}

gint
main (gint argc, gchar **argv)
{
	g_test_init (&argc, &argv, NULL);

	add_tests ("direct", &direct);
	add_tests ("dbus", &dbus);
	add_tests ("http", &http);

	return g_test_run ();
}
