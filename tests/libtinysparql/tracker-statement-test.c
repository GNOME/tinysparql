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
	const gchar *arg1;
	const gchar *arg2;
	const gchar *arg3;
	const gchar *arg4;
	gboolean service;
} TestInfo;

TestInfo tests[] = {
	{ "simple", "statement/simple.rq", "statement/simple.out", "hello" },
	{ "simple-2", "/test/sparql/statement/simple.rq", "statement/simple.out", "hello" },
	{ "simple-error", "statement/simple-error.rq" },
	{ "simple-error-2", "/test/sparql/statement/simple-error.rq" },
	{ "object", "statement/object.rq", "statement/object.out", "Music album" },
	{ "object-2", "/test/sparql/statement/object.rq", "statement/object.out", "Music album" },
	{ "object-iri", "statement/object-iri.rq", "statement/object-iri.out", "http://tracker.api.gnome.org/ontology/v3/nfo#MediaList" },
	{ "object-iri-2", "/test/sparql/statement/object-iri.rq", "statement/object-iri.out", "http://tracker.api.gnome.org/ontology/v3/nfo#MediaList" },
	{ "subject", "statement/subject.rq", "statement/subject.out", "http://tracker.api.gnome.org/ontology/v3/nmm#MusicAlbum" },
	{ "subject-2", "statement/subject.rq", "statement/subject-2.out", "urn:nonexistent" },
	{ "subject-3", "/test/sparql/statement/subject.rq", "statement/subject.out", "http://tracker.api.gnome.org/ontology/v3/nmm#MusicAlbum" },
	{ "subject-4", "/test/sparql/statement/subject.rq", "statement/subject-2.out", "urn:nonexistent" },
	{ "filter", "statement/filter.rq", "statement/filter.out", "http://tracker.api.gnome.org/ontology/v3/nmm#MusicAlbum", "Music album" },
	{ "filter-2", "/test/sparql/statement/filter.rq", "statement/filter.out", "http://tracker.api.gnome.org/ontology/v3/nmm#MusicAlbum", "Music album" },
	{ "service", "statement/service.rq", "statement/service.out", "Music album", NULL, NULL, NULL, TRUE },
	{ "service-2", "statement/service-2.rq", "statement/service-2.out", NULL, NULL, NULL, "1.1", TRUE },
	{ "limit", "statement/limit.rq", "statement/limit.out", "1" },
	{ "limit-2", "statement/limit.rq", "statement/limit-2.out", "2" },
	{ "limit-3", "/test/sparql/statement/limit.rq", "statement/limit.out", "1" },
	{ "limit-4", "/test/sparql/statement/limit.rq", "statement/limit-2.out", "2" },
	{ "offset", "statement/offset.rq", "statement/offset.out", "0" },
	{ "offset-2", "statement/offset.rq", "statement/offset-2.out", "1" },
	{ "offset-3", "/test/sparql/statement/offset.rq", "statement/offset.out", "0" },
	{ "offset-4", "/test/sparql/statement/offset.rq", "statement/offset-2.out", "1" },
	{ "datetime", "statement/datetime.rq", "statement/datetime.out", NULL, NULL, "2020-12-04T04:10:03Z" },
	{ "datetime-2", "/test/sparql/statement/datetime.rq", "statement/datetime.out", NULL, NULL, "2020-12-04T04:10:03Z" },
	{ "cast", "statement/cast.rq", "statement/cast.out", "2021-02-24T22:01:02Z" },
	{ "cast-2", "/test/sparql/statement/cast.rq", "statement/cast.out", "2021-02-24T22:01:02Z" },
};

typedef struct {
	TestInfo *test;
	TrackerSparqlConnection *conn;
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
	GError *nerror = NULL;
	GError *error = NULL;
	gint col;
	gboolean retval;

	retval = g_file_get_contents (results_filename, &results, NULL, &nerror);
	g_assert_true (retval);
	g_assert_no_error (nerror);
	g_clear_error (&nerror);

	/* compare results with reference output */

	test_results = g_string_new ("");

	while (tracker_sparql_cursor_next (cursor, NULL, &error)) {
		GString *row_str = g_string_new (NULL);

		for (col = 0; col < tracker_sparql_cursor_get_n_columns (cursor); col++) {
			const gchar *str;
			GDateTime *date_time;

			if (col > 0) {
				g_string_append (row_str, "\t");
			}
			if (g_strcmp0 (g_path_get_basename (results_filename), "datetime.out") == 0) {

				date_time = tracker_sparql_cursor_get_datetime (cursor, col);
				str = g_date_time_format_iso8601 (date_time);
				g_date_time_unref (date_time);

			} else
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

	g_assert_no_error (error);

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
query_statement (TestFixture   *test_fixture,
                 gconstpointer  context)
{
	TrackerSparqlStatement *stmt;
	TrackerSparqlCursor *cursor;
	GError *error = NULL;
	gchar *path, *query;
	GDateTime *date_time;
	TestInfo *test_info = test_fixture->test;
	gboolean retval;

	if (test_info->query_file[0] == '/') {
		/* Absolute paths refer to GResource paths here */
		stmt = tracker_sparql_connection_load_statement_from_gresource (test_fixture->conn,
		                                                                test_info->query_file,
		                                                                NULL, &error);
		g_assert_true (tracker_sparql_statement_get_connection (stmt) == test_fixture->conn);
		g_assert_nonnull (tracker_sparql_statement_get_sparql (stmt));
	} else {
		path = g_build_filename (TEST_SRCDIR, test_info->query_file, NULL);
		retval = g_file_get_contents (path, &query, NULL, &error);
		g_assert_true (retval);
		g_assert_no_error (error);
		g_free (path);

		if (test_info->service) {
			gchar *service_query;

			service_query = g_strdup_printf (query, bus_name);
			g_free (query);
			query = service_query;
		}

		stmt = tracker_sparql_connection_query_statement (test_fixture->conn, query,
		                                                  NULL, &error);
		g_assert_true (tracker_sparql_statement_get_connection (stmt) == test_fixture->conn);
		g_assert_cmpstr (tracker_sparql_statement_get_sparql (stmt), ==, query);
		g_free (query);
	}

	g_assert_no_error (error);

	if (test_info->arg1)
		tracker_sparql_statement_bind_string (stmt, "arg1", test_info->arg1);
	if (test_info->arg2)
		tracker_sparql_statement_bind_string (stmt, "arg2", test_info->arg2);
	if (test_info->arg3) {
		date_time = g_date_time_new_from_iso8601 (test_info->arg3, NULL);
		tracker_sparql_statement_bind_datetime (stmt, "arg3", date_time);
		g_date_time_unref (date_time);
	}
	if (test_info->arg4) {
		double d = g_strtod (test_info->arg4, NULL);
		tracker_sparql_statement_bind_double (stmt, "arg4", d);
	}

	cursor = tracker_sparql_statement_execute (stmt, NULL, &error);

	if (test_info->output_file) {
		g_assert_no_error (error);
		g_assert_true (tracker_sparql_cursor_get_connection (cursor) ==
		               tracker_sparql_statement_get_connection (stmt));

		path = g_build_filename (TEST_SRCDIR, test_info->output_file, NULL);
		check_result (cursor, path);
		g_free (path);
		g_object_unref (cursor);
	} else {
		g_assert_nonnull (error);
	}

	g_object_unref (stmt);
}

static void
rdf_types (TestFixture   *test_fixture,
           gconstpointer  context)
{
	TrackerSparqlStatement *stmt;
	TrackerSparqlCursor *cursor;
	TrackerSparqlConnection *c;
	GError *error = NULL;
	GDateTime *datetime;
	const gchar *langtag, *query;
	gboolean retval;
	glong len;
	gchar *s;

	datetime = g_date_time_new_utc (2020, 01, 01, 01, 01, 01);

	query = "SELECT "
	        "  (~string AS ?string)"
	        "  (~int AS ?int)"
	        "  (~double AS ?double)"
	        "  (~bool AS ?bool)"
	        "  (~datetime AS ?datetime)"
	        "  (~langString AS ?langString)"
	        "{ }";
	stmt = tracker_sparql_connection_query_statement (test_fixture->conn,
	                                                  query,
	                                                  NULL,
	                                                  &error);
	g_assert_no_error (error);

	g_object_get (stmt, "connection", &c, NULL);
	g_assert_true (c == test_fixture->conn);
	g_object_unref (c);

	g_object_get (stmt, "sparql", &s, NULL);
	g_assert_cmpstr (s, ==, query);
	g_free (s);

	tracker_sparql_statement_bind_string (stmt, "string", "Hello");
	tracker_sparql_statement_bind_int (stmt, "int", 42);
	tracker_sparql_statement_bind_double (stmt, "double", 42.2);
	tracker_sparql_statement_bind_boolean (stmt, "bool", TRUE);
	tracker_sparql_statement_bind_datetime (stmt, "datetime", datetime);
	tracker_sparql_statement_bind_langstring (stmt, "langString", "Hola", "es");

	cursor = tracker_sparql_statement_execute (stmt, NULL, &error);
	g_assert_no_error (error);

	retval = tracker_sparql_cursor_next (cursor, NULL, &error);
	g_assert_no_error (error);
	g_assert_true (retval);

	g_assert_cmpstr (tracker_sparql_cursor_get_string (cursor, 0, &len), ==, "Hello");
	g_assert_cmpint (len, ==, strlen ("Hello"));
	g_assert_cmpint (tracker_sparql_cursor_get_integer (cursor, 1), ==, 42);
	g_assert_cmpfloat_with_epsilon (tracker_sparql_cursor_get_double (cursor, 2), 42.2, DBL_EPSILON);
	g_assert_cmpint (tracker_sparql_cursor_get_boolean (cursor, 3), ==, TRUE);
	g_assert_true (g_date_time_equal (tracker_sparql_cursor_get_datetime (cursor, 4), datetime));
	g_assert_cmpstr (tracker_sparql_cursor_get_langstring (cursor, 5, &langtag, &len), ==, "Hola");
	g_assert_cmpstr (langtag, ==, "es");
	g_assert_cmpint (len, ==, strlen ("Hola"));

	/* Check strings and langstrings the other way around too */
	g_assert_cmpstr (tracker_sparql_cursor_get_langstring (cursor, 0, &langtag, &len), ==, "Hello");
	g_assert_cmpstr (langtag, ==, NULL);
	g_assert_cmpint (len, ==, strlen ("Hello"));
	g_assert_cmpstr (tracker_sparql_cursor_get_string (cursor, 5, &len), ==, "Hola");
	g_assert_cmpint (len, ==, strlen ("Hola"));

	g_date_time_unref (datetime);
	g_object_unref (stmt);
	g_object_unref (cursor);
}

static void
execute_async_cb (GObject      *source,
                  GAsyncResult *res,
                  gpointer      user_data)
{
	TrackerSparqlCursor *cursor;
	GError *error = NULL;

	cursor = tracker_sparql_statement_execute_finish (TRACKER_SPARQL_STATEMENT (source),
	                                                  res, &error);
	g_assert_no_error (error);

	g_assert_true (tracker_sparql_cursor_next (cursor, NULL, &error));
	g_assert_no_error (error);
	g_assert_cmpstr (tracker_sparql_cursor_get_string (cursor, 0, NULL), ==, "Hello");

	g_main_loop_quit (user_data);
}

static void
execute_async (TestFixture   *test_fixture,
               gconstpointer  context)
{
	TrackerSparqlStatement *stmt;
	GError *error = NULL;
	GMainLoop *loop;

	stmt = tracker_sparql_connection_query_statement (test_fixture->conn,
	                                                  "SELECT "
	                                                  "  (~string AS ?string)"
	                                                  "{ }",
	                                                  NULL,
	                                                  &error);
	g_assert_no_error (error);

	tracker_sparql_statement_bind_string (stmt, "string", "Hello");

	loop = g_main_loop_new (NULL, FALSE);

	tracker_sparql_statement_execute_async (stmt, NULL, execute_async_cb, loop);

	g_main_loop_run (loop);
	g_main_loop_unref (loop);
}

static void
stmt_update (TestFixture   *test_fixture,
             gconstpointer  context)
{
	TrackerSparqlStatement *stmt;
	TrackerSparqlCursor *cursor;
	GError *error = NULL;
	const char *query;

	query = "INSERT DATA { ~res a rdfs:Resource ; rdfs:label ~label }";
	stmt = tracker_sparql_connection_update_statement (test_fixture->conn,
	                                                   query,
	                                                   NULL,
	                                                   &error);

	if (strstr (G_OBJECT_TYPE_NAME (test_fixture->conn), "Remote") != NULL) {
		/* HTTP connections do not implement updates on purpose */
		g_assert_null (stmt);
		return;
	}

	g_assert_no_error (error);
	g_assert_nonnull (stmt);
	g_assert_true (tracker_sparql_statement_get_connection (stmt) == test_fixture->conn);
	g_assert_cmpstr (tracker_sparql_statement_get_sparql (stmt), ==, query);

	tracker_sparql_statement_bind_string (stmt, "res", "http://example.com/a");
	tracker_sparql_statement_bind_string (stmt, "label", "Label");
	tracker_sparql_statement_update (stmt, NULL, &error);
	g_assert_no_error (error);

	tracker_sparql_statement_bind_string (stmt, "res", "http://example.com/b");
	tracker_sparql_statement_bind_langstring (stmt, "label", "Etiqueta", "es");
	tracker_sparql_statement_update (stmt, NULL, &error);
	g_assert_no_error (error);

	/* Check that data was inserted */
	cursor = tracker_sparql_connection_query (test_fixture->conn,
	                                          "ASK { <http://example.com/a> a rdfs:Resource ; rdfs:label 'Label' ."
	                                          "      <http://example.com/b> a rdfs:Resource ; rdfs:label 'Etiqueta'@es ."
	                                          "}",
	                                          NULL,
	                                          &error);
	g_assert_no_error (error);

	g_assert_true (tracker_sparql_cursor_next (cursor, NULL, &error));
	g_assert_no_error (error);

	g_assert_true (tracker_sparql_cursor_get_boolean (cursor, 0));
	g_object_unref (cursor);
	g_object_unref (stmt);
}

static void
update_async_cb (GObject      *source,
                 GAsyncResult *res,
                 gpointer      user_data)
{
	TrackerSparqlConnection *conn;
	TrackerSparqlCursor *cursor;
	gboolean retval;
	GError *error = FALSE;

	retval = tracker_sparql_statement_update_finish (TRACKER_SPARQL_STATEMENT (source),
	                                                 res, &error);
	g_assert_no_error (error);
	g_assert_true (retval);

	conn = tracker_sparql_statement_get_connection (TRACKER_SPARQL_STATEMENT (source));

	/* Check that data was inserted */
	cursor = tracker_sparql_connection_query (conn,
	                                          "ASK { <http://example.com/z> a rdfs:Resource }",
	                                          NULL,
	                                          &error);
	g_assert_no_error (error);

	g_assert_true (tracker_sparql_cursor_next (cursor, NULL, &error));
	g_assert_no_error (error);

	g_assert_true (tracker_sparql_cursor_get_boolean (cursor, 0));
	g_object_unref (cursor);

	g_main_loop_quit (user_data);
}

static void
stmt_update_async (TestFixture   *test_fixture,
                   gconstpointer  context)
{
	TrackerSparqlStatement *stmt;
	GError *error = NULL;
	GMainLoop *loop;

	stmt = tracker_sparql_connection_update_statement (test_fixture->conn,
	                                                   "INSERT DATA { ~res a rdfs:Resource }",
	                                                   NULL,
	                                                   &error);

	if (strstr (G_OBJECT_TYPE_NAME (test_fixture->conn), "Remote") != NULL) {
		/* HTTP connections do not implement updates on purpose */
		g_assert_null (stmt);
		return;
	}

	g_assert_no_error (error);
	g_assert_nonnull (stmt);

	loop = g_main_loop_new (NULL, FALSE);

	tracker_sparql_statement_bind_string (stmt, "res", "http://example.com/z");
	tracker_sparql_statement_update_async (stmt, NULL, update_async_cb, loop);

	g_main_loop_run (loop);
	g_main_loop_unref (loop);
	g_object_unref (stmt);
}

static void
stmt_fts (TestFixture   *test_fixture,
          gconstpointer  context)
{
	TrackerSparqlStatement *stmt;
	TrackerSparqlCursor *cursor;
	GError *error = NULL;
	gboolean retval;

	/* FIXME: We are relying on this insertion to happen in prior tests for the HTTP connection test */
	if (strstr (G_OBJECT_TYPE_NAME (test_fixture->conn), "Remote") == NULL) {
		tracker_sparql_connection_update (test_fixture->conn,
		                                  "INSERT DATA { "
		                                  "  <http://example.com/fts1> a nmm:MusicPiece ; nie:title '''abc''' ."
		                                  "  <http://example.com/fts2> a nmm:MusicPiece ; nie:title '''def''' ."
		                                  "}",
		                                  NULL,
		                                  &error);
		g_assert_no_error (error);
	}

	stmt = tracker_sparql_connection_query_statement (test_fixture->conn,
	                                                  "SELECT ?u { ?u fts:match ~arg }",
	                                                  NULL,
	                                                  &error);
	g_assert_no_error (error);

	/* Search one element */
	tracker_sparql_statement_bind_string (stmt, "arg", "abc");
	cursor = tracker_sparql_statement_execute (stmt, NULL, &error);
	g_assert_no_error (error);
	retval = tracker_sparql_cursor_next (cursor, NULL, &error);
	g_assert_no_error (error);
	g_assert_true (retval);
	g_assert_cmpstr (tracker_sparql_cursor_get_string (cursor, 0, NULL), ==, "http://example.com/fts1");
	g_clear_object (&cursor);

	/* Search another element */
	tracker_sparql_statement_bind_string (stmt, "arg", "def");
	cursor = tracker_sparql_statement_execute (stmt, NULL, &error);
	g_assert_no_error (error);
	retval = tracker_sparql_cursor_next (cursor, NULL, &error);
	g_assert_no_error (error);
	g_assert_true (retval);
	g_assert_cmpstr (tracker_sparql_cursor_get_string (cursor, 0, NULL), ==, "http://example.com/fts2");
	g_clear_object (&cursor);

	/* Search with mo matches */
	tracker_sparql_statement_bind_string (stmt, "arg", "xyz");
	cursor = tracker_sparql_statement_execute (stmt, NULL, &error);
	g_assert_no_error (error);
	retval = tracker_sparql_cursor_next (cursor, NULL, &error);
	g_assert_no_error (error);
	g_assert_false (retval);
	g_clear_object (&cursor);

	g_clear_object (&stmt);
}

TrackerSparqlConnection *
create_local_connection (GError **error)
{
	TrackerSparqlConnection *conn;
	GFile *ontology;

	ontology = tracker_sparql_get_ontology_nepomuk ();

	conn = tracker_sparql_connection_new (0, NULL, ontology, NULL, error);
	g_object_unref (ontology);

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

	endpoint_http = tracker_endpoint_http_new (data->direct, 54321, NULL, NULL, NULL);
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
	*remote = tracker_sparql_connection_remote_new ("http://127.0.0.1:54321/sparql");
	g_thread_unref (thread);

	return TRUE;
}

typedef void (*TestFunc) (TestFixture   *test_fixture,
                          gconstpointer  context);

typedef struct {
	const gchar *name;
	TestFunc func;
} TestFuncData;

TestFuncData test_funcs[] = {
	{ "rdf_types", rdf_types },
	{ "execute_async", execute_async },
	{ "update", stmt_update },
	{ "update_async", stmt_update_async },
	{ "fts", stmt_fts },
};

static void
add_tests (TrackerSparqlConnection *conn,
           const gchar             *name,
           gboolean                 run_service_tests)
{
	TestFixture *fixture;
	gchar *testpath;
	guint i;

	for (i = 0; i < G_N_ELEMENTS (tests); i++) {

		if (tests[i].service && !run_service_tests)
			continue;

		fixture = g_new0 (TestFixture, 1);
		fixture->conn = conn;
		fixture->test = &tests[i];
		testpath = g_strconcat ("/libtracker-sparql/statement/", name, "/", tests[i].test_name, NULL);
		g_test_add (testpath, TestFixture, fixture, setup, query_statement, NULL);
		g_free (testpath);
	}

	for (i = 0; i < G_N_ELEMENTS (test_funcs); i++) {
		fixture = g_new0 (TestFixture, 1);
		fixture->conn = conn;
		testpath = g_strconcat ("/libtracker-sparql/statement/", name, "/", test_funcs[i].name, NULL);
		g_test_add (testpath, TestFixture, fixture, setup, test_funcs[i].func, NULL);
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
