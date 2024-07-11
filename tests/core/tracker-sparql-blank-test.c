/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
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

#include <string.h>
#include <locale.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

#include <tinysparql.h>

static gchar *tests_data_dir = NULL;

typedef struct {
	void *user_data;
	gchar *data_location;
} TestInfo;

static void
test_blank (TestInfo      *info,
            gconstpointer  context)
{
	GError *error;
	GVariant *updates;
	GVariantIter iter;
	GVariant *rows;
	guint len = 0;
	gchar *solutions[3][3];
	GFile *data_location;
	TrackerSparqlConnection *conn;

	error = NULL;

	data_location = g_file_new_for_path (info->data_location);

	/* initialization */
	conn = tracker_sparql_connection_new (TRACKER_SPARQL_CONNECTION_FLAGS_NONE,
	                                      data_location, data_location,
	                                      NULL, &error);
	g_assert_no_error (error);

	/* perform update in transaction */
	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	updates = tracker_sparql_connection_update_blank (conn,
	                                                  "INSERT { _:foo a rdfs:Resource } "
	                                                  "INSERT { _:foo a rdfs:Resource . _:bar a rdfs:Resource } ",
	                                                  NULL, &error);
	G_GNUC_END_IGNORE_DEPRECATIONS
	g_assert_no_error (error);
	g_assert_true (updates != NULL);

	g_variant_iter_init (&iter, updates);
	while ((rows = g_variant_iter_next_value (&iter))) {
		GVariantIter sub_iter;
		GVariant *sub_value;

		g_variant_iter_init (&sub_iter, rows);

		while ((sub_value = g_variant_iter_next_value (&sub_iter))) {
			gchar *a = NULL, *b = NULL;
			GVariantIter sub_sub_iter;
			GVariant *sub_sub_value;

			g_variant_iter_init (&sub_sub_iter, sub_value);

			while ((sub_sub_value = g_variant_iter_next_value (&sub_sub_iter))) {
				g_variant_get (sub_sub_value, "{ss}", &a, &b);
				solutions[len][0] = a;
				solutions[len][1] = b;
				len++;
				g_assert_cmpint (len, <=, 3);
				g_variant_unref (sub_sub_value);
			}
			g_variant_unref (sub_value);
		}
		g_variant_unref (rows);
	}

	g_assert_cmpint (len, ==, 3);

	g_assert_cmpstr (solutions[0][0], ==, "foo");
	g_assert_true (solutions[0][1] != NULL);

	g_assert_cmpstr (solutions[1][0], ==, "foo");
	g_assert_true (solutions[1][1] != NULL);

	g_assert_cmpstr (solutions[0][1], ==, solutions[1][1]);

	g_assert_cmpstr (solutions[2][0], ==, "bar");
	g_assert_true (solutions[2][1] != NULL);

	g_assert_cmpstr (solutions[2][1], !=, solutions[1][1]);

	/* cleanup */

	g_free (solutions[0][0]);
	g_free (solutions[0][1]);
	g_free (solutions[1][0]);
	g_free (solutions[1][1]);
	g_free (solutions[2][0]);
	g_free (solutions[2][1]);

	g_variant_unref (updates);
	g_object_unref (data_location);
	g_object_unref (conn);
}

static void
check_bnode_default (TrackerSparqlConnection *conn)
{
	GError *error = NULL;
	TrackerSparqlCursor *cursor;
	gchar *bnode_name, *query;
	gboolean next;

	/* Query blank node name */
	cursor = tracker_sparql_connection_query (conn,
						  "SELECT ?u { ?u a rdfs:Resource. FILTER (isBlank (?u)) }",
						  NULL, &error);
	g_assert_no_error (error);
	g_assert_nonnull (cursor);

	next = tracker_sparql_cursor_next (cursor, NULL, &error);
	g_assert_no_error (error);
	g_assert_true (next);

	bnode_name = g_strdup (tracker_sparql_cursor_get_string (cursor, 0, NULL));
	g_assert_nonnull (bnode_name);

	next = tracker_sparql_cursor_next (cursor, NULL, &error);
	g_assert_no_error (error);
	g_assert_false (next);

	g_object_unref (cursor);

	/* Query blank node by name */
	query = g_strdup_printf ("SELECT (<%s> AS ?u) { }", bnode_name);
	cursor = tracker_sparql_connection_query (conn, query, NULL, &error);
	g_free (query);
	g_assert_no_error (error);
	g_assert_nonnull (cursor);
	next = tracker_sparql_cursor_next (cursor, NULL, &error);
	g_assert_no_error (error);
	g_assert_true (next);
	g_assert_cmpstr (bnode_name, ==, tracker_sparql_cursor_get_string (cursor, 0, NULL));
	g_object_unref (cursor);

	/* Query data around blank node */
	query = g_strdup_printf ("SELECT ?t { <%s> a ?t } ORDER BY ?t", bnode_name);
	cursor = tracker_sparql_connection_query (conn, query, NULL, &error);
	g_free (query);
	g_assert_no_error (error);
	g_assert_nonnull (cursor);

	next = tracker_sparql_cursor_next (cursor, NULL, &error);
	g_assert_no_error (error);
	g_assert_true (next);

	g_assert_cmpstr (TRACKER_PREFIX_RDFS "Resource",
			 ==,
			 tracker_sparql_cursor_get_string (cursor, 0, NULL));

	next = tracker_sparql_cursor_next (cursor, NULL, &error);
	g_assert_no_error (error);
	g_assert_false (next);

	g_object_unref (cursor);
	g_free (bnode_name);
}

static void
check_bnode_anonymous (TrackerSparqlConnection *conn)
{
	GError *error = NULL;
	TrackerSparqlCursor *cursor;
	gchar *bnode_name, *query;
	gboolean next;

	/* Query blank node name */
	cursor = tracker_sparql_connection_query (conn,
						  "SELECT ?u { ?u a rdfs:Resource. FILTER (isBlank (?u)) }",
						  NULL, &error);
	g_assert_no_error (error);
	g_assert_nonnull (cursor);

	next = tracker_sparql_cursor_next (cursor, NULL, &error);
	g_assert_no_error (error);
	g_assert_true (next);

	bnode_name = g_strdup (tracker_sparql_cursor_get_string (cursor, 0, NULL));
	g_assert_nonnull (bnode_name);

	next = tracker_sparql_cursor_next (cursor, NULL, &error);
	g_assert_no_error (error);
	g_assert_false (next);

	g_object_unref (cursor);

	/* Query blank node by name, this should return NULL */
	query = g_strdup_printf ("SELECT (<%s> AS ?u) { }", bnode_name);
	cursor = tracker_sparql_connection_query (conn, query, NULL, &error);
	g_free (query);
	g_assert_no_error (error);
	g_assert_nonnull (cursor);
	next = tracker_sparql_cursor_next (cursor, NULL, &error);
	g_assert_no_error (error);
	g_assert_true (next);
	g_assert_null (tracker_sparql_cursor_get_string (cursor, 0, NULL));
	g_object_unref (cursor);

	/* Query data around blank node, this should return nothing */
	query = g_strdup_printf ("SELECT ?t { <%s> a ?t } ORDER BY ?t", bnode_name);
	cursor = tracker_sparql_connection_query (conn, query, NULL, &error);
	g_free (query);
	g_assert_no_error (error);
	g_assert_nonnull (cursor);

	next = tracker_sparql_cursor_next (cursor, NULL, &error);
	g_assert_no_error (error);
	g_assert_false (next);

	g_object_unref (cursor);
	g_free (bnode_name);
}

static void
test_bnode_query_default (TestInfo      *info,
			  gconstpointer  context)
{
	GError *error = NULL;
	GFile *data_location;
	TrackerSparqlConnection *conn;

	data_location = g_file_new_for_path (info->data_location);

	/* Test blank node behavior with default connection flags,
         * concretely TRACKER_SPARQL_CONNECTION_FLAGS_ANONYMOUS_BNODES
	 * being disabled.
	 */
	conn = tracker_sparql_connection_new (TRACKER_SPARQL_CONNECTION_FLAGS_NONE,
	                                      data_location, data_location,
	                                      NULL, &error);
	g_assert_no_error (error);

	/* Insert blank node */
	tracker_sparql_connection_update (conn,
					  "INSERT { _:foo a rdfs:Resource } ",
					  NULL, &error);
	g_assert_no_error (error);

	check_bnode_default (conn);

	g_object_unref (data_location);
	g_object_unref (conn);
}

static void
test_bnode_query_anonymous (TestInfo      *info,
			    gconstpointer  context)
{
	GError *error = NULL;
	GFile *data_location;
	TrackerSparqlConnection *conn;

	data_location = g_file_new_for_path (info->data_location);

	/* Test blank node behavior with
	 * TRACKER_SPARQL_CONNECTION_FLAGS_ANONYMOUS_BNODES set.
	 */
	conn = tracker_sparql_connection_new (TRACKER_SPARQL_CONNECTION_FLAGS_ANONYMOUS_BNODES,
	                                      data_location, data_location,
	                                      NULL, &error);
	g_assert_no_error (error);

	/* Insert blank node */
	tracker_sparql_connection_update (conn,
					  "INSERT { _:foo a rdfs:Resource } ",
					  NULL, &error);
	g_assert_no_error (error);

	check_bnode_anonymous (conn);

	g_object_unref (data_location);
	g_object_unref (conn);
}

static void
test_bnode_default_to_anonymous (TestInfo      *info,
				 gconstpointer  context)
{
	GError *error = NULL;
	GFile *data_location;
	TrackerSparqlConnection *conn;

	data_location = g_file_new_for_path (info->data_location);

	/* Test that a database created with default blank node behavior
	 * behaves as expected if opened with
	 * TRACKER_SPARQL_CONNECTION_FLAGS_ANONYMOUS_BNODES.
	 */
	conn = tracker_sparql_connection_new (TRACKER_SPARQL_CONNECTION_FLAGS_NONE,
	                                      data_location, data_location,
	                                      NULL, &error);
	g_assert_no_error (error);

	/* Insert blank node */
	tracker_sparql_connection_update (conn,
					  "INSERT { _:foo a rdfs:Resource } ",
					  NULL, &error);
	g_assert_no_error (error);

	check_bnode_default (conn);

	g_object_unref (conn);

	/* Reopen database with anonymous blank nodes */
	conn = tracker_sparql_connection_new (TRACKER_SPARQL_CONNECTION_FLAGS_ANONYMOUS_BNODES,
	                                      data_location, data_location,
	                                      NULL, &error);
	g_assert_no_error (error);

	check_bnode_anonymous (conn);

	g_object_unref (conn);
	g_object_unref (data_location);
}

static void
test_bnode_anonymous_to_default (TestInfo      *info,
				 gconstpointer  context)
{
	GError *error = NULL;
	GFile *data_location;
	TrackerSparqlConnection *conn;

	data_location = g_file_new_for_path (info->data_location);

	/* Test that a database and blank node created with
	 * TRACKER_SPARQL_CONNECTION_FLAGS_ANONYMOUS_BNODES behaves
	 * correctly with default flags.
	 */
	conn = tracker_sparql_connection_new (TRACKER_SPARQL_CONNECTION_FLAGS_ANONYMOUS_BNODES,
	                                      data_location, data_location,
	                                      NULL, &error);
	g_assert_no_error (error);

	/* Insert blank node */
	tracker_sparql_connection_update (conn,
					  "INSERT { _:foo a rdfs:Resource } ",
					  NULL, &error);
	g_assert_no_error (error);

	check_bnode_anonymous (conn);

	g_object_unref (conn);

	/* Reopen database with default flags */
	conn = tracker_sparql_connection_new (TRACKER_SPARQL_CONNECTION_FLAGS_NONE,
	                                      data_location, data_location,
	                                      NULL, &error);
	g_assert_no_error (error);

	check_bnode_default (conn);

	g_object_unref (conn);
	g_object_unref (data_location);
}

static void
setup (TestInfo      *info,
       gconstpointer  context)
{
	gchar *basename;

	/* NOTE: g_test_build_filename() doesn't work env vars G_TEST_* are not defined?? */
	basename = g_strdup_printf ("%d", g_test_rand_int_range (0, G_MAXINT));
	info->data_location = g_build_path (G_DIR_SEPARATOR_S, tests_data_dir, basename, NULL);
	g_free (basename);
}

static void
teardown (TestInfo      *info,
          gconstpointer  context)
{
	gchar *cleanup_command;

	/* clean up */
	cleanup_command = g_strdup_printf ("rm -Rf %s/", info->data_location);
	g_spawn_command_line_sync (cleanup_command, NULL, NULL, NULL, NULL);
	g_free (cleanup_command);

	g_free (info->data_location);
}

int
main (int argc, char **argv)
{
	gchar *current_dir;
	gint result;

	setlocale (LC_COLLATE, "en_US.utf8");

	current_dir = g_get_current_dir ();
	tests_data_dir = g_build_path (G_DIR_SEPARATOR_S, current_dir, "sparql-blank-test-data-XXXXXX", NULL);
	g_free (current_dir);

	g_mkdtemp (tests_data_dir);

	g_test_init (&argc, &argv, NULL);

	g_test_add ("/core/sparql-blank", TestInfo, NULL, setup, test_blank, teardown);
	g_test_add ("/core/bnode-query-default", TestInfo, NULL, setup, test_bnode_query_default, teardown);
	g_test_add ("/core/bnode-query-anonymous", TestInfo, NULL, setup, test_bnode_query_anonymous, teardown);
	g_test_add ("/core/bnode-default-to-anonymous", TestInfo, NULL, setup, test_bnode_default_to_anonymous, teardown);
	g_test_add ("/core/bnode-anonymous-to-default", TestInfo, NULL, setup, test_bnode_anonymous_to_default, teardown);

	/* run tests */
	result = g_test_run ();

	g_assert_cmpint (g_remove (tests_data_dir), ==, 0);
	g_free (tests_data_dir);

	return result;
}
