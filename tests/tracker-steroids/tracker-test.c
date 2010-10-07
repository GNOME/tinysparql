/*
 * Copyright (C) 2010, Codeminded BVBA <abustany@gnome.org>
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

#include <stdlib.h>
#include <string.h>

#include <libtracker-sparql/tracker-sparql.h>
#include <libtracker-common/tracker-common.h>

/* This MUST be larger than TRACKER_STEROIDS_BUFFER_SIZE */
#define LONG_NAME_SIZE 128 * 1024 * sizeof(char)

typedef struct {
	GMainLoop *main_loop;
	const gchar *query;
} AsyncData;

static TrackerSparqlConnection *connection;

static void
insert_test_data ()
{
	GError *error = NULL;
	const char *delete_query = "DELETE { "
                               "<urn:testdata1> a rdfs:Resource ."
                               "<urn:testdata2> a rdfs:Resource ."
                               "<urn:testdata3> a rdfs:Resource ."
                               "<urn:testdata4> a rdfs:Resource ."
                               "}";
	char *longName = g_malloc (LONG_NAME_SIZE);
	char *filled_query;

	memset (longName, 'a', LONG_NAME_SIZE);

	longName[LONG_NAME_SIZE - 1] = '\0';

	filled_query = g_strdup_printf ("INSERT {"
	                                "    <urn:testdata1> a nfo:FileDataObject ; nie:url \"/foo/bar\" ."
	                                "    <urn:testdata2> a nfo:FileDataObject ; nie:url \"/plop/coin\" ."
	                                "    <urn:testdata3> a nmm:Artist ; nmm:artistName \"testArtist\" ."
	                                "    <urn:testdata4> a nmm:Photo ; nao:identifier \"%s\" ."
	                                "}", longName);

	tracker_sparql_connection_update (connection, delete_query, 0, NULL, &error);
	g_assert_no_error (error);

	tracker_sparql_connection_update (connection, filled_query, 0, NULL, &error);
	g_assert_no_error (error);

	g_free (filled_query);
	g_free (longName);
}

static void
set_force_dbus_glib (gboolean force)
{
	if (force) {
		g_setenv ("TRACKER_BUS_BACKEND", "dbus-glib", TRUE);
	} else {
		g_unsetenv ("TRACKER_BUS_BACKEND");
	}
}

/*
 * I comment that part out because I don't know how anonymous node hashing
 * works, but if we know two SparqlUpdate calls are going to return the same
 * urns we could use those functions to compare the results between the normal
 * and fast method. I wrote them before realizing I wouldn't know how to use
 * them.
 */
/*
static gboolean
compare_hash_tables (GHashTable *h1, GHashTable *h2)
{
	GHashTableIter i1, i2;
	gpointer k1, v1, k2, v2;

	if (g_hash_table_size (h1) != g_hash_table_size (h2)) {
		return FALSE;
	}

	g_hash_table_iter_init (&i1, h1);
	g_hash_table_iter_init (&i2, h2);

	while (g_hash_table_iter_next (&i1, &k1, &v1)) {
		g_hash_table_iter_next (&i2, &k2, &v2);

		if (g_strcmp0 (k1, k2)) {
			return FALSE;
		}

		if (g_strcmp0 (v1, v2)) {
			return FALSE;
		}
	}

	return TRUE;
}

static gboolean
compare_results (GPtrArray *r1, GPtrArray *r2)
{
	int i, j;

	if (!r1 || !r2) {
		return FALSE;
	}

	if (r1->len != r2->len) {
		return FALSE;
	}

	for (i = 0; i < r1->len; i++) {
		GPtrArray *inner1, *inner2;

		inner1 = g_ptr_array_index (r1, i);
		inner2 = g_ptr_array_index (r2, i);

		if (inner1->len != inner2->len) {
			return FALSE;
		}

		for (j = 0; j < inner1->len; j++) {
			GHashTable *h1, *h2;

			h1 = g_ptr_array_index (inner1, j);
			h2 = g_ptr_array_index (inner2, j);

			if (!compare_hash_tables (h1, h2)) {
				return FALSE;
			}
		}
	}

	return TRUE;
}
*/

static void
query_and_compare_results (const char *query)
{
	TrackerSparqlCursor *cursor_glib;
	TrackerSparqlCursor *cursor_fd;
	GError *error = NULL;

	set_force_dbus_glib (TRUE);
	cursor_glib = tracker_sparql_connection_query (connection, query, NULL, &error);

	g_assert_no_error (error);

	set_force_dbus_glib (FALSE);
	cursor_fd = tracker_sparql_connection_query (connection, query, NULL, &error);

	g_assert_no_error (error);

	while (tracker_sparql_cursor_next (cursor_glib, NULL, NULL) && tracker_sparql_cursor_next (cursor_fd, NULL, NULL)) {
		g_assert_cmpstr (tracker_sparql_cursor_get_string (cursor_glib, 0, NULL),
				 ==,
				 tracker_sparql_cursor_get_string (cursor_fd, 0, NULL));
	}

	/* Check that both cursors are at the end (same number of rows) */
	g_assert (!tracker_sparql_cursor_next (cursor_glib, NULL, NULL));
	g_assert (!tracker_sparql_cursor_next (cursor_fd, NULL, NULL));

	g_object_unref (cursor_glib);
	g_object_unref (cursor_fd);
}

static void
test_tracker_sparql_query_iterate () {
	query_and_compare_results ("SELECT ?r nie:url(?r) WHERE {?r a nfo:FileDataObject}");
}

static void
test_tracker_sparql_query_iterate_largerow ()
{
	query_and_compare_results ("SELECT nao:identifier(?r) WHERE {?r a nmm:Photo}");
}

/* Runs an invalid query */
static void
test_tracker_sparql_query_iterate_error ()
{
	TrackerSparqlCursor *cursor;
	GError *error = NULL;
	const gchar *query = "bork bork bork";

	cursor = tracker_sparql_connection_query (connection, query, NULL, &error);

	/* tracker_sparql_query_iterate should return null on error */
	g_assert (!cursor);

	/* error should be set, along with its message, note: we don't
	 * use g_assert_error() because the code does not match the
	 * enum values for TRACKER_SPARQL_ERROR_*, this is due to
	 * dbus/error matching between client/server. This should be
	 * fixed in gdbus.
	 */
	g_assert (error != NULL && error->domain == TRACKER_SPARQL_ERROR);

	g_error_free (error);
}

/* Runs a query returning an empty set */
static void
test_tracker_sparql_query_iterate_empty ()
{
	TrackerSparqlCursor *cursor;
	GError *error = NULL;
	const gchar *query = "SELECT ?r WHERE {?r a nfo:FileDataObject; nao:identifier \"thisannotationdoesnotexist\"}";

	cursor = tracker_sparql_connection_query (connection, query, NULL, &error);

	g_assert (cursor);
	g_assert_no_error (error);

	g_assert (!tracker_sparql_cursor_next (cursor, NULL, NULL));
	/* This should be 1, the original test had it wrong: there's one column,
	 * no matter if there are no results*/
	g_assert (tracker_sparql_cursor_get_n_columns (cursor) == 1);
	if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDOUT | G_TEST_TRAP_SILENCE_STDERR)) {
		tracker_sparql_cursor_get_string (cursor, 0, NULL);
		exit (0);
	}
	g_test_trap_assert_failed ();

	g_object_unref (cursor);
}

/* Closes the cursor before all results are read */
static void
test_tracker_sparql_query_iterate_sigpipe ()
{
	TrackerSparqlCursor *cursor;
	GError *error = NULL;
	const gchar *query = "SELECT ?r WHERE {?r a nfo:FileDataObject}";

	cursor = tracker_sparql_connection_query (connection, query, NULL, &error);

	g_assert (cursor);
	g_assert_no_error (error);

	tracker_sparql_cursor_next (cursor, NULL, NULL);

	g_object_unref (cursor);
}

static void
test_tracker_sparql_update_fast_small ()
{
	GError *error = NULL;
	const gchar *query = "INSERT { _:x a nmo:Message }";

	tracker_sparql_connection_update (connection, query, 0, NULL, &error);

	g_assert_no_error (error);
}

static void
test_tracker_sparql_update_fast_large ()
{
	GError *error = NULL;
	gchar *lots;
	gchar *query;

	lots = g_malloc (LONG_NAME_SIZE);
	memset (lots, 'a', LONG_NAME_SIZE);
	lots[LONG_NAME_SIZE-1] = '\0';

	query = g_strdup_printf ("INSERT { _:x a nmo:Message; nao:identifier \"%s\" }", lots);

	tracker_sparql_connection_update (connection, query, 0, NULL, &error);

	g_free (lots);
	g_free (query);

	g_assert_no_error (error);
}

static void
async_update_array_callback (GObject      *source_object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
	AsyncData *data = user_data;
	GPtrArray *errors;

	errors = tracker_sparql_connection_update_array_finish (connection, result);

	g_assert (errors->len == 6);

	g_assert (g_ptr_array_index (errors, 0) == NULL);
	g_assert (g_ptr_array_index (errors, 1) == NULL);
	g_assert (g_ptr_array_index (errors, 2) == NULL);
	g_assert (g_ptr_array_index (errors, 3) != NULL);
	g_assert (g_ptr_array_index (errors, 4) == NULL);
	g_assert (g_ptr_array_index (errors, 5) == NULL);

	g_ptr_array_unref (errors);

	g_main_loop_quit (data->main_loop);
}


static void
test_tracker_sparql_update_array_async ()
{
	const gchar *queries[6] = { "INSERT { _:a a nmo:Message }",
	                            "INSERT { _:b a nmo:Message }",
	                            "INSERT { _:c a nmo:Message }",
	                            "INSERT { _:d syntax error a nmo:Message }",
	                            "INSERT { _:e a nmo:Message }",
	                            "INSERT { _:f a nmo:Message }" };

	GMainLoop *main_loop;
	AsyncData *data;

	main_loop = g_main_loop_new (NULL, FALSE);

	data = g_slice_new (AsyncData);
	data->main_loop = main_loop;

	tracker_sparql_connection_update_array_async (connection,
	                                              queries, 6,
	                                              0, NULL,
	                                              async_update_array_callback,
	                                              data);

	g_main_loop_run (main_loop);

	g_slice_free (AsyncData, data);
	g_main_loop_unref (main_loop);

}

static void
test_tracker_sparql_update_fast_error ()
{
	GError *error = NULL;
	const gchar *query = "blork blork blork";

	tracker_sparql_connection_update (connection, query, 0, NULL, &error);

	g_assert (error != NULL && error->domain == TRACKER_SPARQL_ERROR);
	g_error_free (error);
}

static void
test_tracker_sparql_update_blank_fast_small ()
{
	GError *error = NULL;
	const gchar *query = "INSERT { _:x a nmo:Message }";
	GVariant *results;

	results = tracker_sparql_connection_update_blank (connection, query, 0, NULL, &error);

	g_assert_no_error (error);
	g_assert (results);

	/* FIXME: Properly test once we get update_blank implemented */
}

static void
test_tracker_sparql_update_blank_fast_large ()
{
	GError *error = NULL;
	gchar *lots;
	gchar *query;
	GVariant *results;

	lots = g_malloc (LONG_NAME_SIZE);
	memset (lots, 'a', LONG_NAME_SIZE);
	lots[LONG_NAME_SIZE-1] = '\0';

	query = g_strdup_printf ("INSERT { _:x a nmo:Message; nao:identifier \"%s\" }", lots);

	results = tracker_sparql_connection_update_blank (connection, query, 0, NULL, &error);

	g_free (lots);
	g_free (query);

	g_assert_no_error (error);
	g_assert (results);

	/* FIXME: Properly test once we get update_blank implemented */
}

static void
test_tracker_sparql_update_blank_fast_error ()
{
	GError *error = NULL;
	const gchar *query = "blork blork blork";
	GVariant *results;

	results = tracker_sparql_connection_update_blank (connection, query, 0, NULL, &error);

	g_assert (error != NULL && error->domain == TRACKER_SPARQL_ERROR);
	g_assert (!results);

	g_error_free (error);
}

static void
test_tracker_sparql_update_blank_fast_no_blanks ()
{
	GError *error = NULL;
	const gchar *query = "INSERT { <urn:not_blank> a nmo:Message }";
	GVariant *results;

	results = tracker_sparql_connection_update_blank (connection, query, 0, NULL, &error);

	/* FIXME: Properly test once we get update_blank implemented */

	g_assert_no_error (error);
	g_assert (results);
}

static void
test_tracker_batch_sparql_update_fast ()
{
	/* GError *error = NULL; */
	/* const gchar *query = "INSERT { _:x a nmo:Message }"; */

	/* FIXME: batch update is missing so far
	 * tracker_sparql_connection_batch_update (connection, query, NULL, &error); */

	/* g_assert (!error); */
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
	g_assert (cursor_fd != NULL);

	set_force_dbus_glib (TRUE);
	cursor_glib = tracker_sparql_connection_query (connection, data->query, NULL, &error);
	set_force_dbus_glib (FALSE);

	g_assert_no_error (error);
	g_assert (cursor_glib != NULL);

	while (tracker_sparql_cursor_next (cursor_fd, NULL, NULL) && tracker_sparql_cursor_next (cursor_glib, NULL, NULL)) {
		g_assert_cmpstr (tracker_sparql_cursor_get_string (cursor_fd, 0, NULL),
				 ==,
				 tracker_sparql_cursor_get_string (cursor_glib, 0, NULL));
	}

	g_assert (!tracker_sparql_cursor_next (cursor_fd, NULL, NULL));
	g_assert (!tracker_sparql_cursor_next (cursor_glib, NULL, NULL));

	g_object_unref (cursor_fd);
	g_object_unref (cursor_glib);
}

static void
test_tracker_sparql_query_iterate_async ()
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
	g_assert (error);
}

static void
test_tracker_sparql_query_iterate_async_cancel ()
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

static void
async_update_callback (GObject      *source_object,
                       GAsyncResult *result,
                       gpointer      user_data)
{
	AsyncData *data = user_data;
	GError *error = NULL;

	tracker_sparql_connection_update_finish (connection, result, &error);

	g_assert_no_error (error);

	g_main_loop_quit (data->main_loop);
}

static void
test_tracker_sparql_update_async ()
{
	const gchar *query = "INSERT { _:x a nmo:Message }";
	GMainLoop *main_loop;
	AsyncData *data;

	main_loop = g_main_loop_new (NULL, FALSE);

	data = g_slice_new (AsyncData);
	data->main_loop = main_loop;

	tracker_sparql_connection_update_async (connection,
	                                        query,
	                                        0,
	                                        NULL,
	                                        async_update_callback,
	                                        data);

	g_main_loop_run (main_loop);

	g_slice_free (AsyncData, data);
	g_main_loop_unref (main_loop);
}

static void
cancel_update_cb (GObject      *source_object,
                  GAsyncResult *result,
                  gpointer      user_data)
{
	GMainLoop *main_loop = user_data;
	GError *error = NULL;

	g_main_loop_quit (main_loop);

	tracker_sparql_connection_update_finish (connection, result, &error);

	/* An error should be returned (cancelled!) */
	g_assert (error);
}

static void
test_tracker_sparql_update_async_cancel ()
{
	GCancellable *cancellable = g_cancellable_new ();
	const gchar *query = "INSERT { _:x a nmo:Message }";
	GMainLoop *main_loop;

	main_loop = g_main_loop_new (NULL, FALSE);

	tracker_sparql_connection_update_async (connection,
	                                        query,
	                                        0,
	                                        cancellable,
	                                        cancel_update_cb,
	                                        main_loop);
	g_cancellable_cancel (cancellable);

	g_main_loop_run (main_loop);

	g_main_loop_unref (main_loop);
}

static void
async_update_blank_callback (GObject      *source_object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
	AsyncData *data = user_data;
	GVariant *results;
	GError *error = NULL;

	g_main_loop_quit (data->main_loop);

	results = tracker_sparql_connection_update_blank_finish (connection, result, &error);

	g_assert_no_error (error);
	g_assert (results != NULL);
}

static void
test_tracker_sparql_update_blank_async ()
{
	const gchar *query = "INSERT { _:x a nmo:Message }";
	GMainLoop *main_loop;
	AsyncData *data;

	main_loop = g_main_loop_new (NULL, FALSE);

	data = g_slice_new (AsyncData);
	data->main_loop = main_loop;

	tracker_sparql_connection_update_blank_async (connection,
	                                              query,
	                                              0,
	                                              NULL,
	                                              async_update_blank_callback,
	                                              data);

	g_main_loop_run (main_loop);

	g_slice_free (AsyncData, data);
	g_main_loop_unref (main_loop);
}

gint
main (gint argc, gchar **argv)
{
	g_type_init ();
	g_test_init (&argc, &argv, NULL);

	/* do not require prior installation */
	g_setenv ("TRACKER_SPARQL_MODULE_PATH", "../../src/libtracker-bus/.libs", TRUE);

	connection = tracker_sparql_connection_get (NULL, NULL);

	insert_test_data ();

	g_test_add_func ("/steroids/tracker/tracker_sparql_query_iterate", test_tracker_sparql_query_iterate);
	g_test_add_func ("/steroids/tracker/tracker_sparql_query_iterate_largerow", test_tracker_sparql_query_iterate_largerow);
	g_test_add_func ("/steroids/tracker/tracker_sparql_query_iterate_error", test_tracker_sparql_query_iterate_error);
	g_test_add_func ("/steroids/tracker/tracker_sparql_query_iterate_empty", test_tracker_sparql_query_iterate_empty);
	g_test_add_func ("/steroids/tracker/tracker_sparql_query_iterate_sigpipe", test_tracker_sparql_query_iterate_sigpipe);
	g_test_add_func ("/steroids/tracker/tracker_sparql_update_fast_small", test_tracker_sparql_update_fast_small);
	g_test_add_func ("/steroids/tracker/tracker_sparql_update_fast_large", test_tracker_sparql_update_fast_large);
	g_test_add_func ("/steroids/tracker/tracker_sparql_update_fast_error", test_tracker_sparql_update_fast_error);
	g_test_add_func ("/steroids/tracker/tracker_sparql_update_blank_fast_small", test_tracker_sparql_update_blank_fast_small);
	g_test_add_func ("/steroids/tracker/tracker_sparql_update_blank_fast_large", test_tracker_sparql_update_blank_fast_large);
	g_test_add_func ("/steroids/tracker/tracker_sparql_update_blank_fast_error", test_tracker_sparql_update_blank_fast_error);
	g_test_add_func ("/steroids/tracker/tracker_sparql_update_blank_fast_no_blanks", test_tracker_sparql_update_blank_fast_no_blanks);
	g_test_add_func ("/steroids/tracker/tracker_batch_sparql_update_fast", test_tracker_batch_sparql_update_fast);
	g_test_add_func ("/steroids/tracker/tracker_sparql_query_iterate_async", test_tracker_sparql_query_iterate_async);
	g_test_add_func ("/steroids/tracker/tracker_sparql_query_iterate_async_cancel", test_tracker_sparql_query_iterate_async_cancel);
	g_test_add_func ("/steroids/tracker/tracker_sparql_update_async", test_tracker_sparql_update_async);
	g_test_add_func ("/steroids/tracker/tracker_sparql_update_async_cancel", test_tracker_sparql_update_async_cancel);
	g_test_add_func ("/steroids/tracker/tracker_sparql_update_blank_async", test_tracker_sparql_update_blank_async);
	g_test_add_func ("/steroids/tracker/tracker_sparql_update_array_async", test_tracker_sparql_update_array_async);

	return g_test_run ();
}
