/*
 * Copyright (C) 2020, Red Hat Ltd.
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
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <tinysparql.h>

typedef struct {
	const gchar *test_name;
	const gchar *query_file;
	const gchar *output_file;
	TrackerRdfFormat format;
	const gchar *arg1;
} TestInfo;

TestInfo tests[] = {
	{ "ttl/var", "serialize/describe-var-ttl.rq", "serialize/describe-var-ttl.out", TRACKER_RDF_FORMAT_TURTLE, "Class" },
	{ "ttl/single", "serialize/describe-single-ttl.rq", "serialize/describe-single-ttl.out", TRACKER_RDF_FORMAT_TURTLE, NULL },
	{ "ttl/graph", "serialize/describe-graph-ttl.rq", "serialize/describe-graph-ttl.out", TRACKER_RDF_FORMAT_TURTLE, NULL },
	{ "ttl/construct", "serialize/construct-ttl.rq", "serialize/construct-ttl.out", TRACKER_RDF_FORMAT_TURTLE, NULL },
	{ "trig/var", "serialize/describe-var-trig.rq", "serialize/describe-var-trig.out", TRACKER_RDF_FORMAT_TRIG, "Class" },
	{ "trig/single", "serialize/describe-single-trig.rq", "serialize/describe-single-trig.out", TRACKER_RDF_FORMAT_TRIG, NULL },
	{ "trig/graph", "serialize/describe-graph-trig.rq", "serialize/describe-graph-trig.out", TRACKER_RDF_FORMAT_TRIG, NULL },
	{ "trig/construct", "serialize/construct-trig.rq", "serialize/construct-trig.out", TRACKER_RDF_FORMAT_TRIG, NULL },
};

typedef struct {
	TestInfo *test;
	TrackerSparqlConnection *conn;
	GMainLoop *loop;
	GInputStream *istream;
} TestFixture;

typedef struct {
	TrackerSparqlConnection *direct;
	GDBusConnection *dbus_conn;
} StartupData;

static gboolean started = FALSE;
static const gchar *bus_name = NULL;

static void
check_result (GInputStream *istream,
              const gchar  *results_filename)
{
	gchar *results;
	GError *nerror = NULL;
	GError *error = NULL;
	gchar *quoted_results;
	gchar *command_line;
	gchar *quoted_command_line;
	gchar *shell;
	gchar *diff;
	gchar output[8096] = { 0 };
	gboolean retval;

	retval = g_input_stream_read_all (istream,
	                                  output, sizeof (output),
	                                  NULL, NULL, &error);
	g_assert_no_error (error);
	g_assert_true (retval);

	retval = g_file_get_contents (results_filename, &results, NULL, &nerror);
	g_assert_true (retval);
	g_assert_no_error (nerror);
	g_clear_error (&nerror);

	/* compare results with reference output */
	quoted_results = g_shell_quote (output);
	command_line = g_strdup_printf ("printf %%s %s | grep -v -e nrl:modified -e nrl:added | diff -u %s -", quoted_results, results_filename);
	quoted_command_line = g_shell_quote (command_line);
	shell = g_strdup_printf ("sh -c %s", quoted_command_line);
	g_spawn_command_line_sync (shell, &diff, NULL, NULL, &error);
	g_assert_no_error (error);

	if (diff && *diff)
		g_error ("%s", diff);

	g_free (quoted_results);
	g_free (command_line);
	g_free (quoted_command_line);
	g_free (shell);
	g_free (diff);

	g_free (results);
}

static void
setup (TestFixture   *fixture,
       gconstpointer  context)
{
	const TestFixture *test = context;

	*fixture = *test;
}

static void
serialize_stmt_cb (GObject      *source,
                   GAsyncResult *res,
                   gpointer      user_data)
{
	TestFixture *fixture = user_data;
	GError *error = NULL;

	fixture->istream =
		tracker_sparql_statement_serialize_finish (TRACKER_SPARQL_STATEMENT (source),
		                                           res, &error);
	g_assert_no_error (error);
	g_main_loop_quit (fixture->loop);
}


static void
serialize_cb (GObject      *source,
              GAsyncResult *res,
              gpointer      user_data)
{
	TestFixture *fixture = user_data;
	GError *error = NULL;

	fixture->istream =
		tracker_sparql_connection_serialize_finish (TRACKER_SPARQL_CONNECTION (source),
		                                            res, &error);
	g_assert_no_error (error);
	g_main_loop_quit (fixture->loop);
}

static void
serialize (TestFixture   *test_fixture,
           gconstpointer  context)
{
	TrackerSparqlStatement *stmt;
	GError *error = NULL;
	gchar *path, *query;
	TestInfo *test_info = test_fixture->test;
	gboolean retval;

	test_fixture->loop = g_main_loop_new (NULL, FALSE);

	path = g_build_filename (TEST_SRCDIR, test_info->query_file, NULL);
	retval = g_file_get_contents (path, &query, NULL, &error);
	g_assert_true (retval);
	g_assert_no_error (error);
	g_free (path);

	if (test_info->arg1) {
		stmt = tracker_sparql_connection_query_statement (test_fixture->conn,
		                                                  query,
		                                                  NULL,
		                                                  &error);
		g_assert_no_error (error);

		tracker_sparql_statement_bind_string (stmt, "arg1", test_info->arg1);

		tracker_sparql_statement_serialize_async (stmt,
		                                          TRACKER_SERIALIZE_FLAGS_NONE,
		                                          test_info->format,
		                                          NULL,
		                                          serialize_stmt_cb,
		                                          test_fixture);
		g_object_unref (stmt);
	} else {
		tracker_sparql_connection_serialize_async (test_fixture->conn,
		                                           TRACKER_SERIALIZE_FLAGS_NONE,
		                                           test_info->format,
		                                           query,
		                                           NULL,
		                                           serialize_cb,
		                                           test_fixture);
	}

	g_main_loop_run (test_fixture->loop);

	g_assert_nonnull (test_fixture->istream);

	path = g_build_filename (TEST_SRCDIR, test_info->output_file, NULL);
	check_result (test_fixture->istream, path);
	g_input_stream_close (test_fixture->istream, NULL, NULL);
	g_object_unref (test_fixture->istream);
	g_main_loop_unref (test_fixture->loop);
	g_free (path);
	g_free (query);
}

static void
populate_data (TrackerSparqlConnection *conn)
{
	TrackerResource *res;
	GError *error = NULL;

	/* Add some test data in different graphs */
	res = tracker_resource_new ("http://example/a");
	tracker_resource_set_uri (res, "rdf:type", "nmm:MusicPiece");
	tracker_resource_set_uri (res, "nie:title", "Aaa");
	tracker_sparql_connection_update_resource (conn, "http://example/A", res, NULL, &error);
	g_assert_no_error (error);
	g_object_unref (res);

	res = tracker_resource_new ("http://example/a");
	tracker_resource_set_uri (res, "rdf:type", "nmm:MusicPiece");
	tracker_resource_set_int (res, "nmm:trackNumber", 1);
	tracker_sparql_connection_update_resource (conn, "http://example/B", res, NULL, &error);
	g_assert_no_error (error);
	g_object_unref (res);

	res = tracker_resource_new ("http://example/b");
	tracker_resource_set_uri (res, "rdf:type", "nmm:MusicPiece");
	tracker_resource_set_int (res, "nmm:beatsPerMinute", 120);
	tracker_sparql_connection_update_resource (conn, "http://example/B", res, NULL, &error);
	g_assert_no_error (error);
	g_object_unref (res);
}

TrackerSparqlConnection *
create_local_connection (GError **error)
{
	TrackerSparqlConnection *conn;
	GFile *ontology;

	ontology = tracker_sparql_get_ontology_nepomuk ();

	conn = tracker_sparql_connection_new (0, NULL, ontology, NULL, error);
	g_object_unref (ontology);

	populate_data (conn);

	return conn;
}

static gpointer
thread_func (gpointer user_data)
{
	StartupData *data = user_data;
	TrackerEndpointDBus *endpoint;
	TrackerEndpointHttp *endpoint_http;
	GMainContext *context;
	GMainLoop *main_loop;

	context = g_main_context_new ();
	g_main_context_push_thread_default (context);

	main_loop = g_main_loop_new (context, FALSE);

	endpoint = tracker_endpoint_dbus_new (data->direct, data->dbus_conn, NULL, NULL, NULL);
	if (!endpoint)
		return NULL;

	endpoint_http = tracker_endpoint_http_new (data->direct, 54322, NULL, NULL, NULL);
	if (!endpoint_http)
		return NULL;

	started = TRUE;
	g_main_loop_run (main_loop);

	g_main_loop_unref (main_loop);
	g_main_context_pop_thread_default (context);
	g_main_context_unref (context);

	return NULL;
}

static gboolean
create_connections (TrackerSparqlConnection **dbus,
                    TrackerSparqlConnection **direct,
                    TrackerSparqlConnection **remote,
                    GError                  **error)
{
	StartupData data;
	GThread *thread;

	data.direct = create_local_connection (NULL);
	if (!data.direct)
		return FALSE;
	data.dbus_conn = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, error);
	if (!data.dbus_conn)
		return FALSE;

	thread = g_thread_new (NULL, thread_func, &data);

	while (!started)
		g_usleep (100);

	bus_name = g_dbus_connection_get_unique_name (data.dbus_conn);
	*dbus = tracker_sparql_connection_bus_new (bus_name,
	                                           NULL, data.dbus_conn, error);
	*direct = create_local_connection (error);
	*remote = tracker_sparql_connection_remote_new ("http://127.0.0.1:54322/sparql");
	g_thread_unref (thread);

	return TRUE;
}

static void
add_tests (TrackerSparqlConnection *conn,
           const gchar             *name,
           gboolean                 run_service_tests)
{
	guint i;

	for (i = 0; i < G_N_ELEMENTS (tests); i++) {
		TestFixture *fixture;
		gchar *testpath;

		fixture = g_new0 (TestFixture, 1);
		fixture->conn = conn;
		fixture->test = &tests[i];
		testpath = g_strconcat ("/libtracker-sparql/serialize/", name, "/", tests[i].test_name, NULL);
		g_test_add (testpath, TestFixture, fixture, setup, serialize, NULL);
		g_free (testpath);
	}
}

gint
main (gint argc, gchar **argv)
{
	TrackerSparqlConnection *dbus = NULL, *direct = NULL, *remote = NULL;
	GError *error = NULL;

	g_test_init (&argc, &argv, NULL);

	g_assert_true (create_connections (&dbus, &direct, &remote, &error));
	g_assert_no_error (error);

	add_tests (direct, "direct", TRUE);
	add_tests (dbus, "dbus", FALSE);
	add_tests (remote, "http", FALSE);

	return g_test_run ();
}
