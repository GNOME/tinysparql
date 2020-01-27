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

#include <glib/gstdio.h>
#include <gio/gio.h>

#include <libtracker-common/tracker-common.h>

#include <libtracker-data/tracker-data.h>

static gchar *tests_data_dir = NULL;

typedef struct _TestInfo TestInfo;

struct _TestInfo {
	const gchar *test_name;
	const gchar *data;
	gboolean expect_query_error;
	gboolean expect_update_error;
	gchar *data_location;
};

const TestInfo tests[] = {
	/* Update tests */
	{ "update/insert-data-query-1", "update/insert-data-1", FALSE, FALSE },
	{ "update/insert-data-query-2", "update/insert-data-2", FALSE, TRUE },
	{ "update/delete-data-query-1", "update/delete-data-1", FALSE, FALSE },
	{ "update/delete-data-query-2", "update/delete-data-2", FALSE, TRUE },
	{ "update/delete-where-query-1", "update/delete-where-1", FALSE, FALSE },
	{ "update/delete-where-query-2", "update/delete-where-2", FALSE, FALSE },
	{ "update/delete-where-query-3", "update/delete-where-3", FALSE, FALSE },
	{ "update/invalid-insert-where-query-1", "update/invalid-insert-where-1", FALSE, TRUE },
	{ "update/delete-insert-where-query-1", "update/delete-insert-where-1", FALSE, FALSE },
	{ "update/delete-insert-where-query-2", "update/delete-insert-where-2", FALSE, FALSE },
	{ "update/delete-insert-where-query-3", "update/delete-insert-where-3", FALSE, FALSE },
	{ "update/delete-insert-where-query-4", "update/delete-insert-where-4", FALSE, FALSE },
	{ "update/delete-insert-where-query-5", "update/delete-insert-where-5", FALSE, FALSE },
	{ "update/delete-insert-where-query-6", "update/delete-insert-where-6", FALSE, FALSE },
	{ NULL }
};

static void
check_result (TrackerDBCursor *cursor,
              const TestInfo *test_info,
              const gchar *results_filename,
              GError *error)
{
	GString *test_results;
	gchar *results;
	GError *nerror = NULL;

	if (test_info->expect_query_error) {
		g_assert (error != NULL);
	} else {
		g_assert_no_error (error);
	}

	g_file_get_contents (results_filename, &results, NULL, &nerror);
	g_assert_no_error (nerror);
	g_clear_error (&nerror);

	/* compare results with reference output */

	test_results = g_string_new ("");

	if (cursor) {
		gint col;

		while (tracker_db_cursor_iter_next (cursor, NULL, &error)) {
			GString *row_str = g_string_new (NULL);

			for (col = 0; col < tracker_db_cursor_get_n_columns (cursor); col++) {
				const gchar *str;

				if (col > 0) {
					g_string_append (row_str, "\t");
				}

				str = tracker_db_cursor_get_string (cursor, col, NULL);

				/* Hack to avoid misc properties that might tamper with
				 * test reproduceability in DESCRIBE and other unrestricted
				 * queries.
				 */
				if (g_strcmp0 (str, TRACKER_PREFIX_TRACKER "modified") == 0 ||
				    g_strcmp0 (str, TRACKER_PREFIX_TRACKER "added") == 0) {
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
	} else if (test_info->expect_query_error) {
		g_assert (error != NULL && error->domain == TRACKER_SPARQL_ERROR);
		g_string_free (test_results, TRUE);
		g_free (results);
		return;
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
test_sparql_query (TestInfo      *test_info,
                   gconstpointer  context)
{
	TrackerDBCursor *cursor;
	GError *error = NULL;
	gchar *data_filename;
	gchar *query, *query_filename;
	gchar *results_filename;
	gchar *prefix, *data_prefix, *test_prefix;
	GFile *file, *test_schemas, *data_location;
	TrackerDataManager *manager;
	TrackerData *data_update;

	/* initialization */
	prefix = g_build_path (G_DIR_SEPARATOR_S, TOP_SRCDIR, "tests", "libtracker-data", NULL);
	data_prefix = g_build_filename (prefix, test_info->data, NULL);
	test_prefix = g_build_filename (prefix, test_info->test_name, NULL);
	g_free (prefix);

	file = g_file_new_for_path (data_prefix);
	test_schemas = g_file_get_parent (file);
	g_object_unref (file);

	data_location = g_file_new_for_path (test_info->data_location);

	manager = tracker_data_manager_new (TRACKER_DB_MANAGER_FORCE_REINDEX,
	                                    data_location, data_location, test_schemas, /* loc, domain and ontology_name */
	                                    FALSE, 100, 100);
	g_initable_init (G_INITABLE (manager), NULL, &error);
	g_assert_no_error (error);

	data_update = tracker_data_manager_get_data (manager);

	/* data_path = g_build_path (G_DIR_SEPARATOR_S, TOP_SRCDIR, "tests", "libtracker-data", NULL); */

	/* load data set */
	data_filename = g_strconcat (data_prefix, ".ttl", NULL);
	if (g_file_test (data_filename, G_FILE_TEST_IS_REGULAR)) {
		GFile *file = g_file_new_for_path (data_filename);
		tracker_data_load_turtle_file (data_update, file, NULL, &error);
		g_assert_no_error (error);
		g_object_unref (file);
	} else {
		/* no .ttl available, assume .rq with SPARQL Update */
		gchar *data;

		g_free (data_filename);

		data_filename = g_strconcat (data_prefix, ".rq", NULL);
		g_file_get_contents (data_filename, &data, NULL, &error);
		g_assert_no_error (error);

		tracker_data_update_sparql (data_update, data, &error);
		if (test_info->expect_update_error) {
			g_assert (error != NULL);
			g_clear_error (&error);
		} else {
			g_assert_no_error (error);
		}

		g_free (data);
	}

	query_filename = g_strconcat (test_prefix, ".rq", NULL);
	g_file_get_contents (query_filename, &query, NULL, &error);
	g_assert_no_error (error);

	results_filename = g_strconcat (test_prefix, ".out", NULL);

	/* perform actual query */

	cursor = tracker_data_query_sparql_cursor (manager, query, &error);

	check_result (cursor, test_info, results_filename, error);

	g_free (query_filename);
	g_free (query);

	query_filename = g_strconcat (test_prefix, ".extra.rq", NULL);
	if (g_file_get_contents (query_filename, &query, NULL, NULL)) {
		g_object_unref (cursor);
		cursor = tracker_data_query_sparql_cursor (manager, query, &error);
		g_assert_no_error (error);
		g_free (results_filename);
		results_filename = g_strconcat (test_prefix, ".extra.out", NULL);
		check_result (cursor, test_info, results_filename, error);
	}

	g_free (data_prefix);
	g_free (test_prefix);

	if (cursor) {
		g_object_unref (cursor);
	}

	/* cleanup */

	g_free (data_filename);
	g_free (query_filename);
	g_free (query);
	g_free (results_filename);
	g_object_unref (test_schemas);
	g_object_unref (data_location);

	tracker_data_manager_shutdown (manager);
	g_object_unref (manager);
}

static void
setup (TestInfo      *info,
       gconstpointer  context)
{
	const TestInfo *test = context;
	gchar *basename;

	*info = *test;

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
	gint i;

	setlocale (LC_COLLATE, "en_US.utf8");

	tracker_log_init (0, NULL);

	current_dir = g_get_current_dir ();
	tests_data_dir = g_build_filename (current_dir, "sparql-test-data-XXXXXX", NULL);
	g_free (current_dir);

	g_mkdtemp (tests_data_dir);

	g_test_init (&argc, &argv, NULL);

	/* add test cases */
	for (i = 0; tests[i].test_name; i++) {
		gchar *testpath;

#ifndef HAVE_LIBICU
		/* Skip tests which fail collation tests and are known
		 * to do so. For more details see:
		 *
		 * https://bugzilla.gnome.org/show_bug.cgi?id=636074
		 */
		if (strcmp (tests[i].test_name, "functions/functions-xpath-2") == 0) {
			continue;
		}
#endif

		testpath = g_strconcat ("/libtracker-data/sparql/", tests[i].test_name, NULL);
		g_test_add (testpath, TestInfo, &tests[i], setup, test_sparql_query, teardown);
		g_free (testpath);
	}

	/* run tests */
	result = g_test_run ();

	g_assert_cmpint (g_remove (tests_data_dir), ==, 0);
	g_free (tests_data_dir);

	return result;
}
