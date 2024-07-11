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

static TrackerSparqlConnection *connection;
gboolean started = FALSE;

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
test_tracker_sparql_update_fast_small (gpointer      *fixture,
                                       gconstpointer  user_data)
{
	GError *error = NULL;
	const gchar *query = "INSERT { _:x a nfo:Image }";

	tracker_sparql_connection_update (connection, query, NULL, &error);

	g_assert_no_error (error);
}

static void
test_tracker_sparql_update_fast_large (gpointer      *fixture,
                                       gconstpointer  user_data)
{
	GError *error = NULL;
	gchar *lots;
	gchar *query;

	lots = g_malloc (LONG_NAME_SIZE);
	memset (lots, 'a', LONG_NAME_SIZE);
	lots[LONG_NAME_SIZE-1] = '\0';

	query = g_strdup_printf ("INSERT { _:x a nfo:Image; nao:identifier \"%s\" }", lots);

	tracker_sparql_connection_update (connection, query, NULL, &error);

	g_free (lots);
	g_free (query);

	g_assert_no_error (error);
}

static void
async_update_array_callback (GObject      *source_object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
	GError *error = NULL;
	AsyncData *data = user_data;

	tracker_sparql_connection_update_array_finish (connection, result, &error);

	/* main error is only set on fatal (D-Bus) errors that apply to the whole update */
	g_assert_true (error != NULL);

	g_main_loop_quit (data->main_loop);
}


static void
test_tracker_sparql_update_array_async (gpointer      *fixture,
                                        gconstpointer  user_data)
{
	const gchar *queries[6] = { "INSERT { _:a a nfo:Image }",
	                            "INSERT { _:b a nfo:Image }",
	                            "INSERT { _:c a nfo:Image }",
	                            "INSERT { _:d syntax error a nfo:Image }",
	                            "INSERT { _:e a nfo:Image }",
	                            "INSERT { _:f a nfo:Image }" };

	GMainLoop *main_loop;
	AsyncData *data;

	main_loop = g_main_loop_new (NULL, FALSE);

	data = g_slice_new (AsyncData);
	data->main_loop = main_loop;

	/* Cast here is because vala doesn't make const-char-** possible :( */
	tracker_sparql_connection_update_array_async (connection,
	                                              (char**) queries,
	                                              6,
	                                              NULL,
	                                              async_update_array_callback,
	                                              data);

	g_main_loop_run (main_loop);

	g_slice_free (AsyncData, data);
	g_main_loop_unref (main_loop);

}

static void
async_update_array_empty_callback (GObject      *source_object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
	GError *error = NULL;
	AsyncData *data = user_data;

	tracker_sparql_connection_update_array_finish (connection, result, &error);

	/* main error is only set on fatal (D-Bus) errors that apply to the whole update */
	g_assert_no_error (error);

	g_main_loop_quit (data->main_loop);
}

static void
test_tracker_sparql_update_array_async_empty (gpointer      *fixture,
                                              gconstpointer  user_data)
{
	const gchar **queries = NULL;
	GMainLoop *main_loop;
	AsyncData *data;

	main_loop = g_main_loop_new (NULL, FALSE);

	data = g_slice_new (AsyncData);
	data->main_loop = main_loop;

	/* Cast here is because vala doesn't make const-char-** possible :( */
	tracker_sparql_connection_update_array_async (connection,
	                                              (char**) queries,
	                                              0,
	                                              NULL,
	                                              async_update_array_empty_callback,
	                                              data);

	g_main_loop_run (main_loop);

	g_slice_free (AsyncData, data);
	g_main_loop_unref (main_loop);

}

static void
test_tracker_sparql_update_fast_error (gpointer      *fixture,
                                       gconstpointer  user_data)
{
	GError *error = NULL;
	const gchar *query = "blork blork blork";

	tracker_sparql_connection_update (connection, query, NULL, &error);

	g_assert_true (error != NULL && error->domain == TRACKER_SPARQL_ERROR);
	g_error_free (error);
}

static void
test_tracker_sparql_update_blank_fast_small (gpointer      *fixture,
                                             gconstpointer  user_data)
{
	GError *error = NULL;
	const gchar *query = "INSERT { _:x a nfo:Image }";
	GVariant *results;

	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	results = tracker_sparql_connection_update_blank (connection, query, NULL, &error);
	G_GNUC_END_IGNORE_DEPRECATIONS

	g_assert_no_error (error);
	g_assert_true (results);

	/* FIXME: Properly test once we get update_blank implemented */
}

static void
test_tracker_sparql_update_blank_fast_large (gpointer      *fixture,
                                             gconstpointer  user_data)
{
	GError *error = NULL;
	gchar *lots;
	gchar *query;
	GVariant *results;

	lots = g_malloc (LONG_NAME_SIZE);
	memset (lots, 'a', LONG_NAME_SIZE);
	lots[LONG_NAME_SIZE-1] = '\0';

	query = g_strdup_printf ("INSERT { _:x a nfo:Image; nao:identifier \"%s\" }", lots);

	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	results = tracker_sparql_connection_update_blank (connection, query, NULL, &error);
	G_GNUC_END_IGNORE_DEPRECATIONS

	g_free (lots);
	g_free (query);

	g_assert_no_error (error);
	g_assert_true (results);

	/* FIXME: Properly test once we get update_blank implemented */
}

static void
test_tracker_sparql_update_blank_fast_error (gpointer      *fixture,
                                             gconstpointer  user_data)
{
	GError *error = NULL;
	const gchar *query = "blork blork blork";
	GVariant *results;

	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	results = tracker_sparql_connection_update_blank (connection, query, NULL, &error);
	G_GNUC_END_IGNORE_DEPRECATIONS

	g_assert_true (error != NULL && error->domain == TRACKER_SPARQL_ERROR);
	g_assert_true (!results);

	g_error_free (error);
}

static void
test_tracker_sparql_update_blank_fast_no_blanks (gpointer      *fixture,
                                                 gconstpointer  user_data)
{
	GError *error = NULL;
	const gchar *query = "INSERT { <urn:not_blank> a nfo:Image }";
	GVariant *results;

	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	results = tracker_sparql_connection_update_blank (connection, query, NULL, &error);
	G_GNUC_END_IGNORE_DEPRECATIONS

	/* FIXME: Properly test once we get update_blank implemented */

	g_assert_no_error (error);
	g_assert_true (results);
}

static void
test_tracker_batch_sparql_update_fast (gpointer      *fixture,
                                       gconstpointer  user_data)
{
	/* GError *error = NULL; */
	/* const gchar *query = "INSERT { _:x a nfo:Image }"; */

	/* FIXME: batch update is missing so far
	 * tracker_sparql_connection_batch_update (connection, query, NULL, &error); */

	/* g_assert_true (!error); */
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
test_tracker_sparql_update_async (gpointer      *fixture,
                                  gconstpointer  user_data)
{
	const gchar *query = "INSERT { _:x a nfo:Image }";
	GMainLoop *main_loop;
	AsyncData *data;

	main_loop = g_main_loop_new (NULL, FALSE);

	data = g_slice_new (AsyncData);
	data->main_loop = main_loop;

	tracker_sparql_connection_update_async (connection,
	                                        query,
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
	g_assert_true (error);
}

static void
test_tracker_sparql_update_async_cancel (gpointer      *fixture,
                                         gconstpointer  user_data)
{
	GCancellable *cancellable = g_cancellable_new ();
	const gchar *query = "INSERT { _:x a nfo:Image }";
	GMainLoop *main_loop;

	main_loop = g_main_loop_new (NULL, FALSE);

	tracker_sparql_connection_update_async (connection,
	                                        query,
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

	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	results = tracker_sparql_connection_update_blank_finish (connection, result, &error);
	G_GNUC_END_IGNORE_DEPRECATIONS

	g_assert_no_error (error);
	g_assert_true (results != NULL);
}

static void
test_tracker_sparql_update_blank_async (gpointer      *fixture,
                                        gconstpointer  user_data)
{
	const gchar *query = "INSERT { _:x a nfo:Image }";
	GMainLoop *main_loop;
	AsyncData *data;

	main_loop = g_main_loop_new (NULL, FALSE);

	data = g_slice_new (AsyncData);
	data->main_loop = main_loop;

	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	tracker_sparql_connection_update_blank_async (connection,
	                                              query,
	                                              NULL,
	                                              async_update_blank_callback,
	                                              data);
	G_GNUC_END_IGNORE_DEPRECATIONS

	g_main_loop_run (main_loop);

	g_slice_free (AsyncData, data);
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

	g_main_loop_unref (main_loop);
	g_main_context_pop_thread_default (context);
	g_main_context_unref (context);

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

gint
main (gint argc, gchar **argv)
{
	g_test_init (&argc, &argv, NULL);

	connection = create_dbus_connection (NULL);

	g_test_add ("/steroids/tracker/tracker_sparql_update_fast_small", gpointer, NULL, insert_test_data,
			test_tracker_sparql_update_fast_small, delete_test_data);
	g_test_add ("/steroids/tracker/tracker_sparql_update_fast_large", gpointer, NULL, insert_test_data,
			test_tracker_sparql_update_fast_large, delete_test_data);
	g_test_add ("/steroids/tracker/tracker_sparql_update_fast_error", gpointer, NULL, insert_test_data,
			test_tracker_sparql_update_fast_error, delete_test_data);
	g_test_add ("/steroids/tracker/tracker_sparql_update_blank_fast_small", gpointer, NULL, insert_test_data,
			test_tracker_sparql_update_blank_fast_small, delete_test_data);
	g_test_add ("/steroids/tracker/tracker_sparql_update_blank_fast_large", gpointer, NULL, insert_test_data,
			test_tracker_sparql_update_blank_fast_large, delete_test_data);
	g_test_add ("/steroids/tracker/tracker_sparql_update_blank_fast_error", gpointer, NULL, insert_test_data,
			test_tracker_sparql_update_blank_fast_error, delete_test_data);
	g_test_add ("/steroids/tracker/tracker_sparql_update_blank_fast_no_blanks", gpointer, NULL, insert_test_data,
			test_tracker_sparql_update_blank_fast_no_blanks, delete_test_data);
	g_test_add ("/steroids/tracker/tracker_batch_sparql_update_fast", gpointer, NULL, insert_test_data,
			test_tracker_batch_sparql_update_fast, delete_test_data);
	g_test_add ("/steroids/tracker/tracker_sparql_update_async", gpointer, NULL, insert_test_data,
			test_tracker_sparql_update_async, delete_test_data);
	g_test_add ("/steroids/tracker/tracker_sparql_update_async_cancel", gpointer, NULL, insert_test_data,
			test_tracker_sparql_update_async_cancel, delete_test_data);
	g_test_add ("/steroids/tracker/tracker_sparql_update_blank_async", gpointer, NULL, insert_test_data,
			test_tracker_sparql_update_blank_async, delete_test_data);
	g_test_add ("/steroids/tracker/tracker_sparql_update_array_async", gpointer, NULL, insert_test_data,
			test_tracker_sparql_update_array_async, delete_test_data);
	g_test_add ("/steroids/tracker/tracker_sparql_update_array_async_empty", gpointer, NULL, insert_test_data,
			test_tracker_sparql_update_array_async_empty, delete_test_data);

	return g_test_run ();
}
