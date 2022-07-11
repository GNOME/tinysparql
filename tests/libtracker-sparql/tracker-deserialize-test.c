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
#include <libtracker-sparql/tracker-sparql.h>

typedef struct {
	const gchar *test_name;
	const gchar *data_file;
	const gchar *query_file;
	const gchar *output_file;
	TrackerRdfFormat format;
} TestInfo;

TestInfo tests[] = {
	{ "ttl/ttl-1", "deserialize/ttl-1.ttl", "deserialize/ttl-1.rq", "deserialize/ttl-1.out", TRACKER_RDF_FORMAT_TURTLE },
	{ "trig/trig-1", "deserialize/trig-1.trig", "deserialize/trig-1.rq", "deserialize/trig-1.out", TRACKER_RDF_FORMAT_TRIG },
};

typedef struct {
	TestInfo *test;
	TrackerSparqlConnection *direct;
	TrackerSparqlConnection *dbus;
	GMainLoop *loop;
} TestFixture;

typedef struct {
	TrackerSparqlConnection *direct;
	GDBusConnection *dbus_conn;
} StartupData;

static gboolean started = FALSE;
static const gchar *bus_name = NULL;

static void
check_result (TrackerSparqlCursor *cursor,
              const gchar         *results_filename)
{
	GString *test_results;
	gchar *results;
	GError *error = NULL;
	gint col;

	g_file_get_contents (results_filename, &results, NULL, &error);
	g_assert_no_error (error);

	/* compare results with reference output */

	test_results = g_string_new ("");

	while (tracker_sparql_cursor_next (cursor, NULL, &error)) {
		GString *row_str = g_string_new (NULL);

		for (col = 0; col < tracker_sparql_cursor_get_n_columns (cursor); col++) {
			const gchar *str;

			if (col > 0) {
				g_string_append (row_str, "\t");
			}

			str = tracker_sparql_cursor_get_string (cursor, col, NULL);

			/* Hack to avoid misc properties that might tamper with
			 * test reproduceability in DESCRIBE and other unrestricted
			 * queries.
			 */
			if (g_strcmp0 (str, TRACKER_PREFIX_NRL "modified") == 0 ||
			    g_strcmp0 (str, TRACKER_PREFIX_NRL "added") == 0) {
				g_string_free (row_str, TRUE);
				row_str = NULL;
				break;
			}

			if (str != NULL) {
				/* bound variable */
				g_string_append_printf (row_str, "\"%s\"", str);
			}
		}

		if (row_str) {
			g_string_append (test_results, row_str->str);
			g_string_free (row_str, TRUE);
			g_string_append (test_results, "\n");
		}
	}

	if (strcmp (results, test_results->str) != 0) {
		/* print result difference */
		gchar *quoted_results;
		gchar *command_line;
		gchar *quoted_command_line;
		gchar *shell;
		gchar *diff;

		quoted_results = g_shell_quote (test_results->str);
		command_line = g_strdup_printf ("echo -n %s | diff -u %s -", quoted_results, results_filename);
		quoted_command_line = g_shell_quote (command_line);
		shell = g_strdup_printf ("sh -c %s", quoted_command_line);
		g_spawn_command_line_sync (shell, &diff, NULL, NULL, &error);
		g_assert_no_error (error);

		g_error ("%s", diff);

		g_free (quoted_results);
		g_free (command_line);
		g_free (quoted_command_line);
		g_free (shell);
		g_free (diff);
	}

	g_string_free (test_results, TRUE);
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
deserialize_dbus_cb (GObject      *source,
                     GAsyncResult *res,
                     gpointer      user_data)
{
	TestFixture *test_fixture = user_data;
	gboolean retval;
	GError *error = NULL;

	retval = tracker_sparql_connection_deserialize_finish (TRACKER_SPARQL_CONNECTION (source),
	                                                       res, &error);
	g_assert_no_error (error);
	g_assert_true (retval);

	g_main_loop_quit (test_fixture->loop);
}

static void
serialize_direct_cb (GObject      *source,
                     GAsyncResult *res,
                     gpointer      user_data)
{
	TestFixture *test_fixture = user_data;
	GInputStream *istream;
	GError *error = NULL;

	istream = tracker_sparql_connection_serialize_finish (TRACKER_SPARQL_CONNECTION (source),
	                                                     res, &error);
	g_assert_no_error (error);
	g_assert_nonnull (istream);

	/* Load RDF data into other connection */
	tracker_sparql_connection_deserialize_async (test_fixture->dbus,
	                                             TRACKER_DESERIALIZE_FLAGS_NONE,
	                                             test_fixture->test->format,
	                                             NULL,
	                                             istream,
	                                             NULL,
	                                             deserialize_dbus_cb,
	                                             test_fixture);
}

static void
deserialize_direct_cb (GObject      *source,
                       GAsyncResult *res,
                       gpointer      user_data)
{
	TestFixture *test_fixture = user_data;
	gboolean retval;
	GError *error = NULL;

	retval = tracker_sparql_connection_deserialize_finish (TRACKER_SPARQL_CONNECTION (source),
	                                                       res, &error);
	g_assert_no_error (error);
	g_assert_true (retval);

	/* Read RDF data back */
	tracker_sparql_connection_serialize_async (test_fixture->direct,
	                                           TRACKER_SERIALIZE_FLAGS_NONE,
	                                           test_fixture->test->format,
	                                           "DESCRIBE ?u WHERE { ?u a rdfs:Resource }",
	                                           NULL,
	                                           serialize_direct_cb,
	                                           user_data);
}

static void
test (TestFixture   *test_fixture,
      gconstpointer  context)
{
	GError *error = NULL;
	gchar *path, *query_path, *query;
	TestInfo *test_info = test_fixture->test;
	TrackerSparqlCursor *cursor;
	GInputStream *istream;
	GFile *file;

	test_fixture->loop = g_main_loop_new (NULL, FALSE);

	path = g_build_filename (TOP_SRCDIR, "tests", "libtracker-sparql",
	                         test_info->data_file, NULL);
	file = g_file_new_for_path (path);
	istream = G_INPUT_STREAM (g_file_read (file, NULL, &error));
	g_object_unref (file);
	g_assert_no_error (error);
	g_free (path);

	/* Load RDF data */
	tracker_sparql_connection_deserialize_async (test_fixture->direct,
	                                             TRACKER_DESERIALIZE_FLAGS_NONE,
	                                             test_fixture->test->format,
	                                             NULL,
	                                             istream,
	                                             NULL,
	                                             deserialize_direct_cb,
	                                             test_fixture);

	g_main_loop_run (test_fixture->loop);

	query_path = g_build_filename (TOP_SRCDIR, "tests", "libtracker-sparql",
	                               test_info->query_file, NULL);
	path = g_build_filename (TOP_SRCDIR, "tests", "libtracker-sparql",
	                         test_info->output_file, NULL);

	g_file_get_contents (query_path, &query, NULL, &error);
	g_assert_no_error (error);
	g_free (query_path);

	cursor = tracker_sparql_connection_query (test_fixture->direct,
	                                          query,
	                                          NULL,
	                                          &error);
	g_assert_no_error (error);
	g_assert_nonnull (cursor);
	check_result (cursor, path);
	g_object_unref (cursor);

	cursor = tracker_sparql_connection_query (test_fixture->dbus,
	                                          query,
	                                          NULL,
	                                          &error);
	g_assert_no_error (error);
	g_assert_nonnull (cursor);
	check_result (cursor, path);
	g_object_unref (cursor);

	g_free (path);
	g_free (query);
}

TrackerSparqlConnection *
create_local_connection (GError **error)
{
        TrackerSparqlConnection *conn;
        GFile *ontology;

        ontology = g_file_new_for_path (TEST_ONTOLOGIES_DIR);

        conn = tracker_sparql_connection_new (0, NULL, ontology, NULL, error);
        g_object_unref (ontology);

        return conn;
}

static gpointer
thread_func (gpointer user_data)
{
	StartupData *data = user_data;
	TrackerEndpointDBus *endpoint;
	GMainContext *context;
	GMainLoop *main_loop;

	context = g_main_context_new ();
	g_main_context_push_thread_default (context);

	main_loop = g_main_loop_new (context, FALSE);

	endpoint = tracker_endpoint_dbus_new (data->direct, data->dbus_conn, NULL, NULL, NULL);
	if (!endpoint)
		return NULL;

	started = TRUE;
	g_main_loop_run (main_loop);

	return NULL;
}

static gboolean
create_connections (TrackerSparqlConnection **dbus,
                    TrackerSparqlConnection **direct,
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
	g_thread_unref (thread);

	return TRUE;
}

gint
main (gint argc, gchar **argv)
{
	TrackerSparqlConnection *dbus = NULL, *direct = NULL;
	GError *error = NULL;
	guint i;

	g_test_init (&argc, &argv, NULL);

	g_assert_true (create_connections (&dbus, &direct, &error));
	g_assert_no_error (error);

	for (i = 0; i < G_N_ELEMENTS (tests); i++) {
		TestFixture *fixture;
		gchar *testpath;

		fixture = g_new0 (TestFixture, 1);
		fixture->direct = direct;
		fixture->dbus = dbus;
		fixture->test = &tests[i];
		testpath = g_strconcat ("/libtracker-sparql/deserialize/", tests[i].test_name, NULL);
		g_test_add (testpath, TestFixture, fixture, setup, test, NULL);
		g_free (testpath);
	}

	return g_test_run ();
}
