/*
 * Copyright (C) 2022, Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#include <tinysparql.h>
#include <locale.h>
#include <glib.h>
#include <stdio.h>

static gchar *database_path = NULL;
static gint batch_size = 5000;
static gint duration = 30;

enum {
	BATCH_RESOURCE,
	BATCH_SPARQL,
	BATCH_SPARQL_STMT,
};

static GOptionEntry entries[] = {
	{ "database", 'p', 0, G_OPTION_ARG_FILENAME, &database_path,
	  "Location of the database",
	  "FILE"
	},
	{ "batch-size", 'b', 0, G_OPTION_ARG_INT, &batch_size,
	  "Update batch size",
	  "SIZE"
	},
	{ "duration", 'd', 0, G_OPTION_ARG_INT, &duration,
	  "Duration of individual benchmarks",
	  "DURATION"
	},
	{ NULL }
};

typedef gpointer (*DataCreateFunc) (void);

typedef void (*BenchmarkFunc) (TrackerSparqlConnection *conn,
                               DataCreateFunc           data_func,
                               double                  *elapsed,
                               int                     *elems,
                               double                  *min,
                               double                  *max);

enum {
	UNIT_SEC,
	UNIT_MSEC,
	UNIT_USEC,
};

static inline int
get_unit (gdouble value)
{
	/* Below msec, report in usecs */
	if (value < 0.001)
		return UNIT_USEC;
	else if (value < 1)
		return UNIT_MSEC;

	return UNIT_SEC;
}

static gdouble
transform_unit (gdouble value)
{
	int unit = get_unit (value);

	switch (unit) {
	case UNIT_USEC:
		return value * G_USEC_PER_SEC;
	case UNIT_MSEC:
		return value * 1000;
	case UNIT_SEC:
		return value;
	default:
		g_assert_not_reached ();
	}
}

static const gchar *
unit_string (gdouble value)
{
	int unit = get_unit (value);

	switch (unit) {
	case UNIT_USEC:
		return "usec";
	case UNIT_MSEC:
		return "msec";
	case UNIT_SEC:
		return "sec";
	default:
		g_assert_not_reached ();
	}
}

static inline gpointer
create_resource (void)
{
	TrackerResource *resource;

	resource = tracker_resource_new (NULL);
	tracker_resource_set_uri (resource, "rdf:type", "rdfs:Resource");

	return resource;
}

static inline gpointer
create_changing_resource (void)
{
	TrackerResource *resource;
	gchar buf[2] = { 0, 0 };
	static gint res = 0;
	static gint counter = 0;
	gchar *uri;

	/* In order to keep large batches, and guaranteeing modifications
	 * do not get coalesced, create one different URI per slot in the batch.
	 */
	uri = g_strdup_printf ("http://example.com/%d", res);
	resource = tracker_resource_new (uri);
	tracker_resource_set_uri (resource, "rdf:type", "rdfs:Resource");
	buf[0] = '0' + counter;
	tracker_resource_set_string (resource, "rdfs:label", (const gchar *) buf);
	counter = (counter + 1) % 10;
	res = (res + 1) % batch_size;
	g_free (uri);

	return resource;
}

static inline gpointer
create_query (void)
{
	return g_strdup ("SELECT ?u { ?u a rdfs:Resource } limit 1");
}

static inline TrackerBatch *
create_batch (TrackerSparqlConnection *conn,
              DataCreateFunc           data_func,
              guint                    type)
{
	TrackerBatch *batch;
	TrackerResource *resource = NULL;
	TrackerSparqlStatement *stmt = NULL;
	int i;

	batch = tracker_sparql_connection_create_batch (conn);

	for (i = 0; i < batch_size; i++) {
		if (type == BATCH_SPARQL) {
			gchar *sparql;

			resource = data_func ();
			sparql = tracker_resource_print_sparql_update (resource,
			                                               tracker_sparql_connection_get_namespace_manager (conn),
			                                               NULL);
			tracker_batch_add_sparql (batch, sparql);
			g_free (sparql);
		} else if (type == BATCH_SPARQL_STMT) {
			if (!stmt) {
				stmt = tracker_sparql_connection_update_statement (conn,
				                                                   "INSERT { [] a rdfs:Resource }",
				                                                   NULL, NULL);
			}

			tracker_batch_add_statement (batch, stmt, NULL);
		} else if (type == BATCH_RESOURCE) {
			resource = data_func ();
			tracker_batch_add_resource (batch, NULL, resource);
		}

		g_clear_object (&resource);
	}

	return batch;
}

static inline TrackerBatch *
create_batch_insert_delete (TrackerSparqlConnection *conn,
                            guint                    type)
{
	TrackerBatch *batch;
	TrackerSparqlStatement *stmt = NULL;
	int i;

	batch = tracker_sparql_connection_create_batch (conn);

	for (i = 0; i < batch_size; i++) {
		gchar *uri;

		uri = g_strdup_printf ("http://example.com/%d", i);

		if (type == BATCH_RESOURCE) {
			TrackerResource *resource;

			/* Always used for inserts */
			resource = tracker_resource_new (uri);
			tracker_resource_set_uri (resource, "rdf:type", "rdfs:Resource");
			tracker_batch_add_resource (batch, NULL, resource);
			g_object_unref (resource);
		} else if (type == BATCH_SPARQL) {
			gchar *query;

			query = g_strdup_printf ("DELETE DATA { <%s> a rdfs:Resource }", uri);
			tracker_batch_add_sparql (batch, query);
			g_free (query);
		} else {
			if (!stmt) {
				stmt = tracker_sparql_connection_update_statement (conn,
				                                                   "DELETE DATA { ~uri a rdfs:Resource }",
				                                                   NULL,
				                                                   NULL);
			}

			tracker_batch_add_statement (batch,
			                             stmt,
			                             "uri", G_TYPE_STRING, uri,
			                             NULL);
		}

		g_free (uri);
	}

	return batch;
}

static int
consume_cursor (TrackerSparqlCursor *cursor)
{
	GError *error = NULL;
	int magic = 0;

	while (tracker_sparql_cursor_next (cursor, NULL, &error)) {
		const gchar *str;

		/* Some bit fiddling so the loop is not optimized out */
		str = tracker_sparql_cursor_get_string (cursor, 0, NULL);
		magic ^= str[0] == 'h';
	}

	tracker_sparql_cursor_close (cursor);

	return magic;
}

static void
benchmark_update_batch (TrackerSparqlConnection *conn,
                        DataCreateFunc           data_func,
                        double                  *elapsed,
                        int                     *elems,
                        double                  *min,
                        double                  *max)
{
	GTimer *timer;
	GError *error = NULL;

	timer = g_timer_new ();

	while (*elapsed < duration) {
		TrackerBatch *batch;
		double batch_elapsed;

		g_timer_reset (timer);
		batch = create_batch (conn, data_func, BATCH_RESOURCE);
		tracker_batch_execute (batch, NULL, &error);
		g_assert_no_error (error);
		g_object_unref (batch);

		batch_elapsed = g_timer_elapsed (timer, NULL);
		*min = MIN (*min, batch_elapsed);
		*max = MAX (*max, batch_elapsed);
		*elapsed += batch_elapsed;
		*elems += 1;
	}

	/* We count things by resources, not batches */
	*min /= batch_size;
	*max /= batch_size;
	*elems *= batch_size;

	g_timer_destroy (timer);
}

static void
benchmark_update_batch_stmt (TrackerSparqlConnection *conn,
                             DataCreateFunc           data_func,
                             double                  *elapsed,
                             int                     *elems,
                             double                  *min,
                             double                  *max)
{
	GTimer *timer;
	GError *error = NULL;

	timer = g_timer_new ();

	while (*elapsed < duration) {
		TrackerBatch *batch;
		double batch_elapsed;

		batch = create_batch (conn, data_func, BATCH_SPARQL_STMT);
		tracker_batch_execute (batch, NULL, &error);
		g_assert_no_error (error);
		g_object_unref (batch);

		batch_elapsed = g_timer_elapsed (timer, NULL);
		*min = MIN (*min, batch_elapsed);
		*max = MAX (*max, batch_elapsed);
		*elapsed += batch_elapsed;
		*elems += 1;
		g_timer_reset (timer);
	}

	/* We count things by resources, not batches */
	*min /= batch_size;
	*max /= batch_size;
	*elems *= batch_size;

	g_timer_destroy (timer);
}

static void
benchmark_update_sparql (TrackerSparqlConnection *conn,
                         DataCreateFunc           data_func,
                         double                  *elapsed,
                         int                     *elems,
                         double                  *min,
                         double                  *max)
{
	GTimer *timer;
	GError *error = NULL;

	timer = g_timer_new ();

	while (*elapsed < duration) {
		TrackerBatch *batch;
		double batch_elapsed;

		batch = create_batch (conn, data_func, BATCH_SPARQL);
		tracker_batch_execute (batch, NULL, &error);
		g_assert_no_error (error);
		g_object_unref (batch);

		batch_elapsed = g_timer_elapsed (timer, NULL);
		*min = MIN (*min, batch_elapsed);
		*max = MAX (*max, batch_elapsed);
		*elapsed += batch_elapsed;
		*elems += 1;
		g_timer_reset (timer);
	}

	/* We count things by resources, not batches */
	*min /= batch_size;
	*max /= batch_size;
	*elems *= batch_size;

	g_timer_destroy (timer);
}

static void
benchmark_update_insert_delete (TrackerSparqlConnection *conn,
                                DataCreateFunc           data_func,
                                double                  *elapsed,
                                int                     *elems,
                                double                  *min,
                                double                  *max)
{
	GTimer *timer;
	GError *error = NULL;
	int i = 0;

	timer = g_timer_new ();

	while (*elapsed < duration) {
		TrackerBatch *batch;
		double batch_elapsed;

		g_timer_reset (timer);
		batch = create_batch_insert_delete (conn,
		                                    (i % 2) == 0 ?
		                                    BATCH_RESOURCE : BATCH_SPARQL);
		tracker_batch_execute (batch, NULL, &error);
		g_assert_no_error (error);
		g_object_unref (batch);

		batch_elapsed = g_timer_elapsed (timer, NULL);
		*min = MIN (*min, batch_elapsed);
		*max = MAX (*max, batch_elapsed);
		*elapsed += batch_elapsed;
		*elems += 1;
		i++;
	}

	/* We count things by resources, not batches */
	*min /= batch_size;
	*max /= batch_size;
	*elems *= batch_size;

	g_timer_destroy (timer);
}

static void
benchmark_update_insert_delete_stmt (TrackerSparqlConnection *conn,
                                     DataCreateFunc           data_func,
                                     double                  *elapsed,
                                     int                     *elems,
                                     double                  *min,
                                     double                  *max)
{
	GTimer *timer;
	GError *error = NULL;
	int i = 0;

	timer = g_timer_new ();

	while (*elapsed < duration) {
		TrackerBatch *batch;
		double batch_elapsed;

		g_timer_reset (timer);
		batch = create_batch_insert_delete (conn,
		                                    (i % 2) == 0 ?
		                                    BATCH_RESOURCE : BATCH_SPARQL_STMT);
		tracker_batch_execute (batch, NULL, &error);
		g_assert_no_error (error);
		g_object_unref (batch);

		batch_elapsed = g_timer_elapsed (timer, NULL);
		*min = MIN (*min, batch_elapsed);
		*max = MAX (*max, batch_elapsed);
		*elapsed += batch_elapsed;
		*elems += 1;
		i++;
	}

	/* We count things by resources, not batches */
	*min /= batch_size;
	*max /= batch_size;
	*elems *= batch_size;

	g_timer_destroy (timer);
}

static void
benchmark_query_statement (TrackerSparqlConnection *conn,
                           DataCreateFunc           data_func,
                           double                  *elapsed,
                           int                     *elems,
                           double                  *min,
                           double                  *max)
{
	TrackerSparqlStatement *stmt;
	GTimer *timer;
	GError *error = NULL;
	gchar *query;

	timer = g_timer_new ();
	query = data_func ();
	stmt = tracker_sparql_connection_query_statement (conn, query,
	                                                  NULL, &error);
	g_assert_no_error (error);
	g_free (query);

	while (*elapsed < duration) {
		TrackerSparqlCursor *cursor;
		double query_elapsed;

		cursor = tracker_sparql_statement_execute (stmt, NULL, &error);
		g_assert_no_error (error);
		consume_cursor (cursor);
		g_object_unref (cursor);

		query_elapsed = g_timer_elapsed (timer, NULL);
		*min = MIN (*min, query_elapsed);
		*max = MAX (*max, query_elapsed);
		*elapsed += query_elapsed;
		*elems += 1;
		g_timer_reset (timer);
	}

	g_object_unref (stmt);
	g_timer_destroy (timer);
}

static void
benchmark_query_sparql (TrackerSparqlConnection *conn,
                        DataCreateFunc           data_func,
                        double                  *elapsed,
                        int                     *elems,
                        double                  *min,
                        double                  *max)
{
	GTimer *timer;
	GError *error = NULL;
	gchar *query;

	timer = g_timer_new ();
	query = data_func ();

	while (*elapsed < duration) {
		TrackerSparqlCursor *cursor;
		double query_elapsed;

		cursor = tracker_sparql_connection_query (conn, query,
		                                          NULL, &error);
		g_assert_no_error (error);
		consume_cursor (cursor);
		g_object_unref (cursor);

		query_elapsed = g_timer_elapsed (timer, NULL);
		*min = MIN (*min, query_elapsed);
		*max = MAX (*max, query_elapsed);
		*elapsed += query_elapsed;
		*elems += 1;
		g_timer_reset (timer);
	}

	g_timer_destroy (timer);
	g_free (query);
}

struct {
	const gchar *desc;
	BenchmarkFunc func;
	DataCreateFunc data_func;
} benchmarks[] = {
	{ "Resource batch update (sync)", benchmark_update_batch, create_resource },
	{ "Statement batch update (sync)", benchmark_update_batch_stmt, create_resource },
	{ "SPARQL batch update (sync)", benchmark_update_sparql, create_resource },
	{ "Resource modification (sync)", benchmark_update_batch, create_changing_resource },
	{ "Resource insert + Statement delete (sync)", benchmark_update_insert_delete_stmt, NULL },
	{ "Resource insert + SPARQL delete (sync)", benchmark_update_insert_delete, NULL },
	{ "Prepared statement query (sync)", benchmark_query_statement, create_query },
	{ "SPARQL query (sync)", benchmark_query_sparql, create_query },
};

static void
run_benchmarks (TrackerSparqlConnection *conn)
{
	guint i;
	guint max_len = 0;

	for (i = 0; i < G_N_ELEMENTS (benchmarks); i++)
		max_len = MAX (max_len, strlen (benchmarks[i].desc));

	g_print ("%*s\t\tElements\tElems/sec\tMin         \tMax         \tAvg\n",
	         max_len, "Test");

	for (i = 0; i < G_N_ELEMENTS (benchmarks); i++) {
		double elapsed = 0, min = G_MAXDOUBLE, max = -G_MAXDOUBLE, adjusted, avg;
		int elems = 0;

		g_print ("%*s\t\t", max_len, benchmarks[i].desc);

		benchmarks[i].func (conn, benchmarks[i].data_func,
		                    &elapsed, &elems, &min, &max);

		if (elapsed > duration) {
			/* To avoid explaining how long did the benchmark
			 * actually take to run. Adjust the output to the
			 * specified time limit.
			 */
			adjusted = elems * ((double) duration / elapsed);
		} else {
			adjusted = elems;
		}

		avg = elapsed / elems;
		g_print ("%.3f\t%.3f\t%.3f %s\t%.3f %s\t%3.3f %s\n",
		         adjusted,
		         elems / elapsed,
		         transform_unit (min), unit_string (min),
		         transform_unit (max), unit_string (max),
		         transform_unit (avg), unit_string (avg));
	}
}

int
main (int argc, char *argv[])
{
	TrackerSparqlConnection *conn;
	GOptionContext *context;
	GError *error = NULL;
	GFile *db = NULL;

        setlocale (LC_ALL, "");

	context = g_option_context_new (NULL);
	g_option_context_add_main_entries (context, entries, NULL);

	if (!g_option_context_parse (context, &argc, (char***) &argv, &error)) {
		g_printerr ("%s, %s\n", "Unrecognized options", error->message);
		g_error_free (error);
		g_option_context_free (context);
		return EXIT_FAILURE;
	}

	g_option_context_free (context);

	g_print ("Batch size: %d, Individual test duration: %d sec\n",
	         batch_size, duration);

	if (database_path) {
		if (g_file_test (database_path, G_FILE_TEST_EXISTS)) {
			g_printerr ("Database path '%s' already exists", database_path);
			return EXIT_FAILURE;
		}

		g_print ("Opening file database at '%s'…\n",
		         database_path);
		db = g_file_new_for_commandline_arg (database_path);
	} else {
		g_print ("Opening in-memory database…\n");
	}

	conn = tracker_sparql_connection_new (0, db,
	                                      tracker_sparql_get_ontology_nepomuk(),
	                                      NULL, &error);
	g_assert_no_error (error);

	run_benchmarks (conn);

	g_object_unref (conn);
	g_clear_object (&db);

	return EXIT_SUCCESS;
}
