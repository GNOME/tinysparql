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

#include <tracker-sparql.h>

typedef struct {
	const gchar *input ;
	const gchar *output;
} ESCAPE_TEST_DATA;

ESCAPE_TEST_DATA test_data []  = {
	{ "SELECT \"a\"", "SELECT \\\"a\\\"" },
	{ "SELECT ?u \t \n \r \b \f", "SELECT ?u \\t \\n \\r \\b \\f" },
	{ NULL, NULL }
};

static TrackerSparqlConnection *connection;
static GMainLoop *main_loop;

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
test_tracker_sparql_escape_uri_vprintf (void)
{
	gchar *result;

	result = tracker_sparql_escape_uri_printf ("test:uri:contact-%d", 14, NULL);
	g_assert_cmpstr (result, ==, "test:uri:contact-14");
	g_free (result);
}

static void
test_tracker_sparql_cursor_next_async_cb (GObject      *source,
                                          GAsyncResult *result,
                                          gpointer      user_data)
{
	TrackerSparqlCursor *cursor;
	GError *error = NULL;
	gboolean success;
	static gint successes = 0;

	g_assert (result != NULL);
	success = tracker_sparql_cursor_next_finish (TRACKER_SPARQL_CURSOR (source), result, &error);
	g_assert_no_error (error);

	cursor = TRACKER_SPARQL_CURSOR (source);
	g_assert (cursor != NULL);

	g_print ("  %p: %s\n", user_data, tracker_sparql_cursor_get_string (cursor, 0, NULL));

	if (!success) {
		successes++;
		if (successes > 1) {
			g_main_loop_quit (main_loop);
		}
	} else {
		tracker_sparql_cursor_next_async (cursor, NULL, test_tracker_sparql_cursor_next_async_cb, user_data);
	}
}

static void
test_tracker_sparql_cursor_next_async (void)
{
	TrackerSparqlCursor *cursor;
	GError *error = NULL;
	const gchar *query1 = "SELECT ?p WHERE { ?p tracker:indexed true }";
	const gchar *query2 = "SELECT"
			     "  ?song"
			     "  nie:url(?song)"
			     "  tracker:coalesce(nie:title(?song), nfo:fileName(?song), \"Unknown\")"
			     "  fn:string-join((?performer, ?album), \" - \")"
			     "  nfo:duration(?song)"
			     "  ?tooltip "
			     "WHERE {"
			     "  ?match fts:match \"love\""
			     "  {"
			     "    ?song nmm:musicAlbum ?match"
			     "  } UNION {"
			     "    ?song nmm:performer ?match"
			     "  } UNION {"
			     "    ?song a nfo:Audio ."
			     "    ?match a nfo:Audio"
			     "    FILTER (?song = ?match)"
			     "  }"
			     "  ?song nmm:performer [ nmm:artistName ?performer ] ;"
			     "        nmm:musicAlbum [ nie:title ?album ] ;"
			     "        nfo:belongsToContainer [ nie:url ?tooltip ]"
			     "} "
			     "ORDER BY DESC(fts:rank(?song)) DESC(nie:title(?song)) "
			     "OFFSET 0 LIMIT 100";

	
	g_print ("ASYNC query 1 starting:\n");
	cursor = tracker_sparql_connection_query (connection, query1, NULL, &error);
	g_assert_no_error (error);
	g_assert (cursor != NULL);
	tracker_sparql_cursor_next_async (cursor, NULL, test_tracker_sparql_cursor_next_async_cb, GINT_TO_POINTER(1));

	g_print ("ASYNC query 2 starting:\n");
	cursor = tracker_sparql_connection_query (connection, query2, NULL, &error);
	g_assert_no_error (error);
	g_assert (cursor != NULL);
	tracker_sparql_cursor_next_async (cursor, NULL, test_tracker_sparql_cursor_next_async_cb, GINT_TO_POINTER(2));
}

gint
main (gint argc, gchar **argv)
{
	int result;
	GError *error = NULL;

	g_thread_init (NULL);
	g_type_init ();
	g_test_init (&argc, &argv, NULL);

	main_loop = g_main_loop_new (NULL, FALSE);
	g_assert (main_loop != NULL);

	connection = tracker_sparql_connection_get (&error);

	g_assert_no_error (error);
	g_assert (connection != NULL);

	g_test_add_func ("/libtracker-sparql/tracker/tracker_sparql_escape_string", 
	                 test_tracker_sparql_escape_string);
	g_test_add_func ("/libtracker-sparql/tracker/tracker_sparql_escape_uri_vprintf",
	                 test_tracker_sparql_escape_uri_vprintf);
	g_test_add_func ("/libtracker-sparql/tracker/tracker_sparql_cursor_next_async",
	                 test_tracker_sparql_cursor_next_async);

	result = g_test_run ();

	g_main_loop_run (main_loop);

	g_object_unref (connection);

	return result;
}
