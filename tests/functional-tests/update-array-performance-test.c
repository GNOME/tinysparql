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
 *
 * Copied from ../tracker-steroids/tracker-test.c
 */

#include <stdlib.h>
#include <string.h>

#include <tracker-bus.h>
#include <tracker-sparql.h>

typedef struct {
	GMainLoop *main_loop;
	const gchar *query;
	guint len, cur;
} AsyncData;

static TrackerSparqlConnection *connection;
#define MSIZE 90
#define TEST_STR "Brrr0092323"

static const gchar *queries[90] = {
	    "INSERT { _:a0 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:a9 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:a11 a nmo:Message; nie:title '" TEST_STR "' }", 
	    "INSERT { _:b0 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:b9 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:b11 a nmo:Message; nie:title '" TEST_STR "' }",
	    "INSERT { _:c0 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:c9 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:c12 a nmo:Message; nie:title '" TEST_STR "' }",
	    "INSERT { _:d0 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:d9 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:d12 a nmo:Message; nie:title '" TEST_STR "' }",
	    "INSERT { _:e0 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:e9 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:e11 a nmo:Message; nie:title '" TEST_STR "' }",
	    "INSERT { _:f0 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:f9 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:f11 a nmo:Message; nie:title '" TEST_STR "' }",
	    "INSERT { _:b1 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:b8 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:b13 a nmo:Message; nie:title '" TEST_STR "' }",
	    "INSERT { _:c1 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:c8 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:c13 a nmo:Message; nie:title '" TEST_STR "' }",
	    "INSERT { _:d1 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:d8 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:d14 a nmo:Message; nie:title '" TEST_STR "' }",
	    "INSERT { _:e1 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:e8 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:e14 a nmo:Message; nie:title '" TEST_STR "' }",
	    "INSERT { _:f1 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:f8 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:f15 a nmo:Message; nie:title '" TEST_STR "' }",
	    "INSERT { _:b2 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:b7 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:b15 a nmo:Message; nie:title '" TEST_STR "' }",
	    "INSERT { _:c2 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:c7 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:c15 a nmo:Message; nie:title '" TEST_STR "' }",
	    "INSERT { _:d2 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:d7 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:d16 a nmo:Message; nie:title '" TEST_STR "' }",
	    "INSERT { _:e2 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:e7 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:e16 a nmo:Message; nie:title '" TEST_STR "' }",
	    "INSERT { _:f2 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:f7 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:f17 a nmo:Message; nie:title '" TEST_STR "' }",
	    "INSERT { _:b3 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:b6 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:b16 a nmo:Message; nie:title '" TEST_STR "'}",
	    "INSERT { _:c3 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:c6 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:c18 a nmo:Message; nie:title '" TEST_STR "' }",
	    "INSERT { _:d3 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:d6 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:d19 a nmo:Message; nie:title '" TEST_STR "' }",
	    "INSERT { _:e3 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:e6 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:e20 a nmo:Message; nie:title '" TEST_STR "' }",
	    "INSERT { _:f3 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:f6 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:f21 a nmo:Message; nie:title '" TEST_STR "' }",
	    "INSERT { _:b4 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:b5 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:b22 a nmo:Message; nie:title '" TEST_STR "' }",
	    "INSERT { _:c4 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:c5 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:c23 a nmo:Message; nie:title '" TEST_STR "' }",
	    "INSERT { _:d4 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:d5 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:d24 a nmo:Message; nie:title '" TEST_STR "' }",
	    "INSERT { _:e4 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:e5 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:e24 a nmo:Message; nie:title '" TEST_STR "' }",
	    "INSERT { _:f4 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:f5 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:f25 a nmo:Message; nie:title '" TEST_STR "' }",
	    "INSERT { _:c5 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:c2 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:c26 a nmo:Message; nie:title '" TEST_STR "' }",
	    "INSERT { _:d5 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:d2 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:d28 a nmo:Message; nie:title '" TEST_STR "' }",
	    "INSERT { _:e5 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:e2 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:e29 a nmo:Message; nie:title '" TEST_STR "' }",
	    "INSERT { _:f5 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:f2 a nmo:Message; nie:title '" TEST_STR "' }", "INSERT { _:f33 a nmo:Message; nie:title '" TEST_STR "' }"};

static void
async_update_array_callback (GObject      *source_object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
	AsyncData *data = user_data;
	GPtrArray *errors;

	errors = tracker_sparql_connection_update_array_finish (connection, result);
	g_ptr_array_unref (errors);
	g_main_loop_quit (data->main_loop);
}


static void
test_tracker_sparql_update_array_async ()
{
	GMainLoop *main_loop;
	AsyncData *data;

	main_loop = g_main_loop_new (NULL, FALSE);

	data = g_slice_new (AsyncData);
	data->main_loop = main_loop;

	/* Cast here is because vala doesn't make const-char-** possible :( */
	tracker_sparql_connection_update_array_async (connection,
	                                              (char**) queries, MSIZE,
	                                              0, NULL,
	                                              async_update_array_callback,
	                                              data);

	g_main_loop_run (main_loop);

	g_slice_free (AsyncData, data);
	g_main_loop_unref (main_loop);

}

static void
async_update_callback (GObject      *source_object,
                       GAsyncResult *result,
                       gpointer      user_data)
{
	AsyncData *data = user_data;
	GError *error = NULL;

	data->cur++;

	tracker_sparql_connection_update_finish (connection, result, &error);
	if (error)
		g_error_free (error);

	if (data->cur == data->len)
		g_main_loop_quit (data->main_loop);
}

static void
test_tracker_sparql_update_async ()
{
	guint i;
	GMainLoop *main_loop;
	AsyncData *data;

	main_loop = g_main_loop_new (NULL, FALSE);

	data = g_slice_new (AsyncData);
	data->len = MSIZE;
	data->main_loop = main_loop;
	data->cur = 0;

	for (i = 0; i < data->len; i++) {
		tracker_sparql_connection_update_async (connection,
		                                        queries[i],
		                                        0, NULL,
		                                        async_update_callback,
		                                        data);
	}

	g_main_loop_run (main_loop);

	g_slice_free (AsyncData, data);
	g_main_loop_unref (main_loop);

}


gint
main (gint argc, gchar **argv)
{
	GTimer *array_t, *update_t;

	g_type_init ();

	/* do not require prior installation */
	g_setenv ("TRACKER_SPARQL_MODULE_PATH", "../../src/libtracker-bus/.libs", TRUE);

	connection = tracker_sparql_connection_get (NULL, NULL);

	g_print ("First run (first update then array)\n");

	tracker_sparql_connection_update (connection,
	                                  "DELETE { ?r a rdfs:Resource } WHERE { ?r nie:title '" TEST_STR "' }",
	                                  0, NULL, NULL);

	update_t = g_timer_new ();
	test_tracker_sparql_update_async ();
	g_timer_stop (update_t);

	tracker_sparql_connection_update (connection,
	                                  "DELETE { ?r a rdfs:Resource } WHERE { ?r nie:title '" TEST_STR "' }",
	                                  0, NULL, NULL);

	array_t = g_timer_new ();
	test_tracker_sparql_update_array_async ();
	g_timer_stop (array_t);

	tracker_sparql_connection_update (connection,
	                                  "DELETE { ?r a rdfs:Resource } WHERE { ?r nie:title '" TEST_STR "' }",
	                                  0, NULL, NULL);

	g_print ("Array: %f, Update: %f\n", g_timer_elapsed (array_t, NULL), g_timer_elapsed (update_t, NULL));

	g_print ("Reversing run (first array then update)\n");

	g_timer_destroy (array_t);
	g_timer_destroy (update_t);

	array_t = g_timer_new ();
	test_tracker_sparql_update_array_async ();
	g_timer_stop (array_t);

	tracker_sparql_connection_update (connection,
	                                  "DELETE { ?r a rdfs:Resource } WHERE { ?r nie:title '" TEST_STR "' }",
	                                  0, NULL, NULL);

	update_t = g_timer_new ();
	test_tracker_sparql_update_async ();
	g_timer_stop (update_t);

	tracker_sparql_connection_update (connection,
	                                  "DELETE { ?r a rdfs:Resource } WHERE { ?r nie:title '" TEST_STR "' }",
	                                  0, NULL, NULL);

	g_print ("Array: %f, Update: %f\n", g_timer_elapsed (array_t, NULL), g_timer_elapsed (update_t, NULL));

	g_timer_destroy (array_t);
	g_timer_destroy (update_t);
	g_object_unref (connection);

	return 0;
}
