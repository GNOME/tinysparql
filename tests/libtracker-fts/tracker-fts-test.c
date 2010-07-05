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

#include <glib.h>
#include <gio/gio.h>

#include <libtracker-common/tracker-ontologies.h>

#include <libtracker-data/tracker-data-manager.h>
#include <libtracker-data/tracker-data-query.h>
#include <libtracker-data/tracker-data-update.h>
#include <libtracker-data/tracker-data.h>
#include <libtracker-data/tracker-sparql-query.h>

typedef struct _TestInfo TestInfo;

struct _TestInfo {
	const gchar *test_name;
	gint number_of_queries;
};

const TestInfo tests[] = {
	{ "fts3aa", 2 },
	{ "fts3ae", 1 },
	{ "prefix/fts3prefix", 3 },
	{ "limits/fts3limits", 4 },
	{ NULL }
};

static void
test_sparql_query (gconstpointer test_data)
{
	TrackerDBResultSet *result_set;
	const TestInfo *test_info;
	GError *error;
	GString *test_results;
	gchar *update, *update_filename;
	gchar *query, *query_filename;
	gchar *results, *results_filename;
	gchar *prefix, *data_prefix, *test_prefix;
	gint i;
	const gchar *test_schemas[2] = { NULL, NULL };

	error = NULL;
	test_info = test_data;

	/* initialization */
	prefix = g_build_path (G_DIR_SEPARATOR_S, TOP_SRCDIR, "tests", "libtracker-fts", NULL);
	data_prefix = g_build_filename (prefix, "data", NULL);
	test_prefix = g_build_filename (prefix, test_info->test_name, NULL);
	g_free (prefix);

	test_schemas[0] = data_prefix;
	tracker_db_journal_set_rotating (FALSE, G_MAXSIZE, NULL);
	tracker_data_manager_init (TRACKER_DB_MANAGER_FORCE_REINDEX,
	                           test_schemas,
	                           NULL, FALSE, NULL, NULL, NULL);

	/* load data / perform updates */

	update_filename = g_strconcat (test_prefix, "-data.rq", NULL);
	g_file_get_contents (update_filename, &update, NULL, &error);
	g_assert_no_error (error);

	tracker_data_begin_db_transaction ();
	tracker_data_update_sparql (update, &error);
	tracker_data_commit_db_transaction ();
	g_assert_no_error (error);

	g_free (update_filename);
	g_free (update);

	/* perform queries */

	for (i = 1; i <= test_info->number_of_queries; i++) {
		query_filename = g_strdup_printf ("%s-%d.rq", test_prefix, i);
		g_file_get_contents (query_filename, &query, NULL, &error);
		g_assert_no_error (error);

		results_filename = g_strdup_printf ("%s-%d.out", test_prefix, i);
		g_file_get_contents (results_filename, &results, NULL, &error);
		g_assert_no_error (error);

		result_set = tracker_data_query_sparql (query, &error);
		g_assert_no_error (error);

		/* compare results with reference output */

		test_results = g_string_new ("");

		if (result_set) {
			gboolean valid = TRUE;
			guint col_count;
			gint col;

			col_count = tracker_db_result_set_get_n_columns (result_set);

			while (valid) {
				for (col = 0; col < col_count; col++) {
					GValue value = { 0 };

					_tracker_db_result_set_get_value (result_set, col, &value);

					switch (G_VALUE_TYPE (&value)) {
					case G_TYPE_INT:
						g_string_append_printf (test_results, "\"%d\"", g_value_get_int (&value));
						break;
					case G_TYPE_DOUBLE:
						g_string_append_printf (test_results, "\"%f\"", g_value_get_double (&value));
						break;
					case G_TYPE_STRING:
						g_string_append_printf (test_results, "\"%s\"", g_value_get_string (&value));
						break;
					default:
						/* unbound variable */
						break;
					}

					if (col < col_count - 1) {
						g_string_append (test_results, "\t");
					}
				}

				g_string_append (test_results, "\n");

				valid = tracker_db_result_set_iter_next (result_set);
			}

			g_object_unref (result_set);
		}

		if (strcmp (results, test_results->str)) {
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

		/* cleanup */

		g_free (query_filename);
		g_free (query);
		g_free (results_filename);
		g_free (results);
		g_string_free (test_results, TRUE);
	}

	g_free (data_prefix);
	g_free (test_prefix);

	tracker_data_manager_shutdown ();
}

int
main (int argc, char **argv)
{
	gint result;
	gint i;
	gchar *current_dir;

	g_type_init ();

	if (!g_thread_supported ()) {
		g_thread_init (NULL);
	}

	g_test_init (&argc, &argv, NULL);

	current_dir = g_get_current_dir ();

	g_setenv ("XDG_DATA_HOME", current_dir, TRUE);
	g_setenv ("XDG_CACHE_HOME", current_dir, TRUE);
	g_setenv ("TRACKER_DB_SQL_DIR", TOP_SRCDIR "/data/db/", TRUE);
	g_setenv ("TRACKER_DB_ONTOLOGIES_DIR", TOP_SRCDIR "/data/ontologies/", TRUE);
	g_setenv ("TRACKER_FTS_STOP_WORDS", "0", TRUE);

	g_free (current_dir);

	/* add test cases */
	for (i = 0; tests[i].test_name; i++) {
		gchar *testpath;

		testpath = g_strconcat ("/libtracker-fts/", tests[i].test_name, NULL);
		g_test_add_data_func (testpath, &tests[i], test_sparql_query);
		g_free (testpath);
	}

	/* run tests */
	result = g_test_run ();

	/* clean up */
	g_print ("Removing temporary data\n");
	g_spawn_command_line_sync ("rm -R tracker/", NULL, NULL, NULL, NULL);

	return result;
}
