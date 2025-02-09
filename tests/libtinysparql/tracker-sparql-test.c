/*
 * Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
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

#include <locale.h>

#include <glib-object.h>

#include <tinysparql.h>

typedef struct {
	const gchar *input ;
	const gchar *output;
} ESCAPE_TEST_DATA;

ESCAPE_TEST_DATA test_data []  = {
	{ "SELECT \"a\"", "SELECT \\\"a\\\"" },
	{ "SELECT \'a\'", "SELECT \\\'a\\\'" },
	{ "SELECT ?u \t \n \r \b \f", "SELECT ?u \\t \\n \\r \\b \\f" },
	{ NULL, NULL }
};

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

static void
test_tracker_sparql_escape_string (void)
{
	gint i;
	gchar *result;

	for (i = 0; test_data[i].input != NULL; i++) {
		result = tracker_sparql_escape_string (test_data[i].input);
		g_assert_cmpstr (result, ==, test_data[i].output);
		g_free (result);
	}
}

static void
test_tracker_sparql_escape_uri (void)
{
	gchar *result;

	result = tracker_sparql_escape_uri ("test:uri:🍌");
	g_assert_cmpstr (result, ==, "test:uri:%F0%9F%8D%8C");
	g_free (result);
}

static void
test_tracker_sparql_escape_uri_vprintf (void)
{
	gchar *result;

	result = tracker_sparql_escape_uri_printf ("test:uri:%s-%d-%0.3f-%%-%hd-%1.*f", "🍌", 14, 1.23403, (short) 8, 2, 1.23403);
	g_assert_cmpstr (result, ==, "test:uri:%F0%9F%8D%8C-14-1.234-%25-8-1.23");
	g_free (result);

	/* Test positional arguments and flags */
	result = tracker_sparql_escape_uri_printf ("test:uri:%2$0*1$d-%3$+d-%5$-*4$s-%7$1.*6$f", 3, 1, 42, 4, "+42", 2, 1.23403);
	g_assert_cmpstr (result, ==, "test:uri:001-+42-+42%20-1.23");
	g_free (result);

	result = tracker_sparql_escape_uri_printf ("test:uri:%");
	g_assert_cmpstr (result, ==, "test:uri:%");
	g_free (result);
}

/* Test that we return an error if no ontology is passed. */
static void
test_tracker_sparql_connection_no_ontology (void)
{
	GError *error = NULL;

	TrackerSparqlConnection *connection;

	connection = tracker_sparql_connection_new (0, NULL, NULL, NULL, &error);

	g_assert_null (connection);
	g_assert_error (error, TRACKER_SPARQL_ERROR, TRACKER_SPARQL_ERROR_ONTOLOGY_NOT_FOUND);

	g_error_free (error);
}

static void
test_tracker_sparql_connection_interleaved (void)
{
	GError *error = NULL;

	TrackerSparqlCursor *cursor1;
	TrackerSparqlCursor *cursor2;
	TrackerSparqlConnection *connection;

	const gchar* query = "select ?u {?u a rdfs:Resource .}";

	connection = create_local_connection (&error);
	g_assert_no_error (error);

	cursor1 = tracker_sparql_connection_query (connection, query, 0, &error);
	g_assert_no_error (error);

	/* intentionally not freeing cursor1 here */

	cursor2 = tracker_sparql_connection_query (connection, query, 0, &error);
	g_assert_no_error (error);

	g_object_unref(connection);

	g_object_unref(cursor2);
	g_object_unref(cursor1);
}

static void
close_cb (GObject      *source,
          GAsyncResult *res,
          gpointer      user_data)
{
	GError *error = NULL;

	tracker_sparql_connection_close_finish (TRACKER_SPARQL_CONNECTION (source),
	                                        res, &error);
	g_assert_no_error (error);

	g_main_loop_quit (user_data);

	g_object_unref (source);
}

static void
new_async_cb (GObject      *source,
              GAsyncResult *res,
              gpointer      user_data)
{
	TrackerSparqlConnection *conn;
	TrackerSparqlCursor *cursor;
	GError *error = NULL;

	conn = tracker_sparql_connection_new_finish (res, &error);
	g_assert_no_error (error);

	cursor = tracker_sparql_connection_query (conn,
	                                          "SELECT ((2 + 2) AS ?four) {}",
	                                          NULL,
	                                          &error);
	g_assert_no_error (error);

	g_assert_true (tracker_sparql_cursor_next (cursor, NULL, &error));
	g_assert_no_error (error);

	g_assert_cmpint (tracker_sparql_cursor_get_integer (cursor, 0), ==, 4);
	tracker_sparql_cursor_close (cursor);
	g_object_unref (cursor);

	tracker_sparql_connection_close_async (conn, NULL, close_cb, user_data);
}

static void
test_tracker_sparql_connection_new_async (void)
{
	GMainLoop *loop;
	GFile *ontology;

	loop = g_main_loop_new (NULL, FALSE);

	ontology = tracker_sparql_get_ontology_nepomuk ();

	tracker_sparql_connection_new_async (0, NULL,
	                                     ontology,
	                                     NULL,
	                                     new_async_cb,
	                                     loop);
	g_main_loop_run (loop);

	g_object_unref (ontology);
}

static void
test_tracker_sparql_connection_bus_new_unknown (void)
{
	TrackerSparqlConnection *conn;
	GError *error = NULL;

	conn = tracker_sparql_connection_bus_new ("com.example.Unknown",
	                                          NULL,
	                                          NULL,
	                                          &error);
	g_assert_error (error, G_DBUS_ERROR, G_DBUS_ERROR_SERVICE_UNKNOWN);
	g_assert_null (conn);
}

static void
bus_new_unknown_cb (GObject      *source,
                    GAsyncResult *res,
                    gpointer      user_data)
{
	TrackerSparqlConnection *conn;
	GError *error = NULL;

	conn = tracker_sparql_connection_bus_new_finish (res, &error);
	g_assert_error (error, G_DBUS_ERROR, G_DBUS_ERROR_SERVICE_UNKNOWN);
	g_assert_null (conn);

	g_main_loop_quit (user_data);
}

static void
test_tracker_sparql_connection_bus_new_async_unknown (void)
{
	GMainLoop *loop;

	loop = g_main_loop_new (NULL, FALSE);

	tracker_sparql_connection_bus_new_async ("com.example.Unknown",
	                                         NULL,
	                                         NULL,
	                                         NULL,
	                                         bus_new_unknown_cb,
	                                         loop);

	g_main_loop_run (loop);
}

static void
test_tracker_check_version (void)
{
	g_assert_true(tracker_check_version(TRACKER_MAJOR_VERSION,
	                                    TRACKER_MINOR_VERSION,
	                                    TRACKER_MICRO_VERSION) == NULL);
	g_assert_true(strstr (tracker_check_version(TRACKER_MAJOR_VERSION + 1, 0, 0),
	                      "old") != NULL);
	g_assert_true(strstr (tracker_check_version(TRACKER_MAJOR_VERSION - 1, 0, 0),
	                      "new") != NULL);
	g_assert_true(strstr (tracker_check_version(TRACKER_MAJOR_VERSION, -1, 0),
	                      "new") != NULL);
	g_assert_true(strstr (tracker_check_version(TRACKER_MAJOR_VERSION, 99, 0),
	                      "old") != NULL);
}

gint
main (gint argc, gchar **argv)
{
	int result;

	setlocale (LC_ALL, "");

	g_test_init (&argc, &argv, NULL);

	/* g_test_init() enables verbose logging by default, but Tracker is too
	 * verbose. To make the logs managable, we hide DEBUG and INFO messages
	 * unless TRACKER_TESTS_VERBOSE is set.
	 */
	if (! g_getenv ("TRACKER_TESTS_VERBOSE")) {
		g_log_set_handler ("Tracker", G_LOG_LEVEL_DEBUG | G_LOG_LEVEL_INFO, g_log_default_handler, NULL);
	}

	g_test_add_func ("/libtracker-sparql/tracker-sparql/tracker_sparql_escape_string",
	                 test_tracker_sparql_escape_string);
	g_test_add_func ("/libtracker-sparql/tracker-sparql/tracker_sparql_escape_uri",
	                 test_tracker_sparql_escape_uri);
	g_test_add_func ("/libtracker-sparql/tracker-sparql/tracker_sparql_escape_uri_vprintf",
	                 test_tracker_sparql_escape_uri_vprintf);
	g_test_add_func ("/libtracker-sparql/tracker-sparql/tracker_sparql_connection_no_ontology",
	                 test_tracker_sparql_connection_no_ontology);
	g_test_add_func ("/libtracker-sparql/tracker-sparql/tracker_sparql_connection_interleaved",
	                 test_tracker_sparql_connection_interleaved);
	g_test_add_func ("/libtracker-sparql/tracker-sparql/tracker_sparql_connection_new_async",
	                 test_tracker_sparql_connection_new_async);
	g_test_add_func ("/libtracker-sparql/tracker-sparql/tracker_sparql_connection_bus_new_unknown",
	                 test_tracker_sparql_connection_bus_new_unknown);
	g_test_add_func ("/libtracker-sparql/tracker-sparql/tracker_sparql_connection_bus_new_async_unknown",
	                 test_tracker_sparql_connection_bus_new_async_unknown);
	g_test_add_func ("/libtracker-sparql/tracker-sparql/tracker_check_version",
	                 test_tracker_check_version);

	result = g_test_run ();

	return result;
}
