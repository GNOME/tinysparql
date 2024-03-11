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
 *
 * Author:
 * Philip Van Hoof <philip@codeminded.be>
 */

#include "config.h"

#include <string.h>

#include <glib.h>
#include <gio/gio.h>
#include <glib/gstdio.h>

#include <libtracker-sparql/tracker-sparql.h>

typedef struct _Query Query;
typedef struct _Update Update;
typedef struct _ChangeInfo ChangeInfo;
typedef struct _ChangeTest ChangeTest;

struct _Query {
	const gchar *query;
	gboolean expect_error;
};

struct _Update {
	const gchar *update;
	gboolean expect_error;
};

struct _ChangeInfo {
	const gchar *ontology;
	const Update *updates;
	const Query *checks;
	gboolean expect_error;
};

struct _ChangeTest {
	const gchar *test_name;
	const ChangeInfo *changes;
};

const ChangeTest tests[] = {
	{
		.test_name = "/core/ontology-change/add-classes",
		.changes = (const ChangeInfo *) &(ChangeInfo[]) {
			{
				"add-classes-1.ontology.v1"
			},
			{
				"add-classes-1.ontology.v2",
				(const Update *) &(Update[]) {
					{ "updates/add-classes-1.rq" },
					{ NULL },
				}
			},
			{ NULL },
		},
	},
	{
		.test_name = "/core/ontology-change/remove-classes",
		.changes = (const ChangeInfo *) &(ChangeInfo[]) {
			{
				"remove-classes-1.ontology.v1",
				(const Update *) &(Update[]) {
					{ "updates/remove-classes-1-pre.rq" },
					{ NULL },
				}
			},
			{
				"remove-classes-1.ontology.v2",
			},
			{ NULL },
		},
	},
	{
		.test_name = "/core/ontology-change/add-properties-1",
		.changes = (const ChangeInfo *) &(ChangeInfo[]) {
			{
				"add-properties-1.ontology.v1"
			},
			{
				"add-properties-1.ontology.v2",
				(const Update *) &(Update[]) {
					{ "updates/add-properties-1.rq" },
					{ NULL },
				},
			},
			{ NULL }
		},
	},
	{
		.test_name = "/core/ontology-change/add-properties-2",
		.changes = (const ChangeInfo *) &(ChangeInfo[]) {
			{
				"add-properties-2.ontology.v1",
			},
			{
				"add-properties-2.ontology.v2",
				(const Update *) &(Update[]) {
					{ "updates/add-properties-2.rq" },
					{ NULL },
				},
			},
			{ NULL }
		},
	},
	{
		.test_name = "/core/ontology-change/remove-properties-1",
		.changes = (const ChangeInfo *) &(ChangeInfo[]) {
			{
				"remove-properties-1.ontology.v1",
				(const Update *) &(Update[]) {
					{ "updates/remove-properties-1-pre.rq" },
					{ NULL },
				},
			},
			{
				"remove-properties-1.ontology.v2",
			},
			{ NULL }
		},
	},
	{
		.test_name = "/core/ontology-change/add-superclass-1",
		.changes = (const ChangeInfo *) &(ChangeInfo[]) {
			{
				"add-superclass-1.ontology.v1",
			},
			{
				"add-superclass-1.ontology.v2",
				.expect_error = TRUE,
			},
			{ NULL }
		},
	},
	{
		.test_name = "/core/ontology-change/remove-superclass-1",
		.changes = (const ChangeInfo *) &(ChangeInfo[]) {
			{
				"remove-superclass-1.ontology.v1",
				(const Update *) &(Update[]) {
					{ "updates/remove-superclass-1-pre.rq" },
					{ NULL },
				},
			},
			{
				"remove-superclass-1.ontology.v2",
			},
			{ NULL }
		},
	},
	{
		.test_name = "/core/ontology-change/add-subclass-1",
		.changes = (const ChangeInfo *) &(ChangeInfo[]) {
			{
				"add-subclass-1.ontology.v1",
				(const Update *) &(Update[]) {
					{ "updates/add-subclass-1-pre.rq" },
					{ NULL },
				},
			},
			{
				"add-subclass-1.ontology.v2",
				.updates = (const Update *) &(Update[]) {
					{ "updates/add-subclass-1-post.rq" },
					{ NULL },
				},
				.checks = (const Query *) &(Query[]) {
					{ "queries/add-subclass-1" },
					{ NULL },
				},
			},
			{ NULL }
		},
	},
	{
		.test_name = "/core/ontology-change/remove-subclass-1",
		.changes = (const ChangeInfo *) &(ChangeInfo[]) {
			{
				"remove-subclass-1.ontology.v1",
				(const Update *) &(Update[]) {
					{ "updates/remove-subclass-1-pre.rq" },
					{ NULL },
				},
			},
			{
				"remove-subclass-1.ontology.v2",
			},
			{ NULL }
		},
	},
	{
		.test_name = "/core/ontology-change/add-superproperty-1",
		.changes = (const ChangeInfo *) &(ChangeInfo[]) {
			{
				"add-superproperty-1.ontology.v1",
			},
			{
				"add-superproperty-1.ontology.v2",
				.expect_error = TRUE,
			},
			{ NULL }
		},
	},
	{
		.test_name = "/core/ontology-change/remove-superproperty-1",
		.changes = (const ChangeInfo *) &(ChangeInfo[]) {
			{
				"remove-superproperty-1.ontology.v1",
				(const Update *) &(Update[]) {
					{ "updates/remove-superproperty-1-pre.rq" },
					{ NULL },
				},
			},
			{
				"remove-superproperty-1.ontology.v2",
			},
			{ NULL }
		},
	},
	{
		.test_name = "/core/ontology-change/add-subproperty-1",
		.changes = (const ChangeInfo *) &(ChangeInfo[]) {
			{
				"add-subproperty-1.ontology.v1",
				(const Update *) &(Update[]) {
					{ "updates/add-subproperty-1-pre.rq" },
					{ NULL },
				},
			},
			{
				"add-subproperty-1.ontology.v2",
			},
			{ NULL }
		},
	},
	{
		.test_name = "/core/ontology-change/remove-subproperty-1",
		.changes = (const ChangeInfo *) &(ChangeInfo[]) {
			{
				"remove-subproperty-1.ontology.v1",
				(const Update *) &(Update[]) {
					{ "updates/remove-subproperty-1-pre.rq" },
					{ NULL },
				},
			},
			{
				"remove-subproperty-1.ontology.v2",
			},
			{ NULL }
		},
	},
	{
		.test_name = "/core/ontology-change/change-cardinality-1",
		.changes = (const ChangeInfo *) &(ChangeInfo[]) {
			{
				"change-cardinality-1.ontology.v1",
				.updates = (const Update *) &(Update[]) {
					{ "updates/change-cardinality-1-pre.rq" },
					/* This should fail before the ontology change */
					{ "updates/change-cardinality-1-post.rq", TRUE },
					{ NULL },
				},
			},
			{
				"change-cardinality-1.ontology.v2",
				.updates = (const Update *) &(Update[]) {
					{ "updates/change-cardinality-1-post.rq" },
					{ NULL },
				},
				.checks = (const Query *) &(Query[]) {
					{ "queries/change-cardinality-1" },
					{ NULL },
				},
			},
			{ NULL }
		},
	},
	{
		.test_name = "/core/ontology-change/change-cardinality-invalid",
		.changes = (const ChangeInfo *) &(ChangeInfo[]) {
			{
				"change-cardinality-invalid-1.ontology.v1",
			},
			{
				"change-cardinality-invalid-1.ontology.v2",
				/* Limiting cardinality on a previously unlimited
				 * property is expected to fail
				 */
				.expect_error = TRUE,
			},
			{ NULL }
		},
	},
	{
		.test_name = "/core/ontology-change/change-range",
		.changes = (const ChangeInfo *) &(ChangeInfo[]) {
			{
				"change-range-1.ontology.v1",
				.updates = (const Update *) &(Update[]) {
					{ "updates/change-range-1-pre.rq" },
					{ NULL },
				},
				.checks = (const Query *) &(Query[]) {
					{ "queries/change-range-1-pre" },
					{ NULL },
				},
			},
			{
				"change-range-1.ontology.v2",
				.checks = (const Query *) &(Query[]) {
					{ "queries/change-range-1-post" },
					{ NULL },
				},
			},
			{ NULL }
		},
	},
	{
		.test_name = "/core/ontology-change/change-range-invalid",
		.changes = (const ChangeInfo *) &(ChangeInfo[]) {
			{
				"change-range-invalid-1.ontology.v1",
			},
			{
				"change-range-invalid-1.ontology.v2",
				/* An invalid conversion is requested, this is expected to fail */
				.expect_error = TRUE,
			},
			{ NULL }
		},
	},
	{
		.test_name = "/core/ontology-change/add-inverse-functional-property",
		.changes = (const ChangeInfo *) &(ChangeInfo[]) {
			{
				"add-inverse-functional-property-1.ontology.v1",
			},
			{
				"add-inverse-functional-property-1.ontology.v2",
				/* This is ATM expected to fail */
				.expect_error = TRUE,
			},
			{ NULL }
		},
	},
	{
		.test_name = "/core/ontology-change/make-inverse-functional-property",
		.changes = (const ChangeInfo *) &(ChangeInfo[]) {
			{
				"make-inverse-functional-property-1.ontology.v1",
			},
			{
				"make-inverse-functional-property-1.ontology.v2",
				/* This is expected to fail */
				.expect_error = TRUE,
			},
			{ NULL }
		},
	},
	{
		.test_name = "/core/ontology-change/unmake-inverse-functional-property",
		.changes = (const ChangeInfo *) &(ChangeInfo[]) {
			{
				"unmake-inverse-functional-property-1.ontology.v1",
			},
			{
				"unmake-inverse-functional-property-1.ontology.v2",
				/* This ATM is expected to fail */
				.expect_error = TRUE,
			},
			{ NULL }
		},
	},
	{
		.test_name = "/core/ontology-change/remove-inverse-functional-property",
		.changes = (const ChangeInfo *) &(ChangeInfo[]) {
			{
				"remove-inverse-functional-property-1.ontology.v1",
			},
			{
				"remove-inverse-functional-property-1.ontology.v2",
			},
			{ NULL }
		},
	},
	{
		.test_name = "/core/ontology-change/add-index",
		.changes = (const ChangeInfo *) &(ChangeInfo[]) {
			{
				"add-index-1.ontology.v1",
				(const Update *) &(Update[]) {
					{ "updates/add-index-1-pre.rq" },
					{ NULL },
				},
			},
			{
				"add-index-1.ontology.v2",
			},
			{ NULL }
		},
	},
	{
		.test_name = "/core/ontology-change/remove-index",
		.changes = (const ChangeInfo *) &(ChangeInfo[]) {
			{
				"remove-index-1.ontology.v1",
				(const Update *) &(Update[]) {
					{ "updates/remove-index-1-pre.rq" },
					{ NULL },
				},
			},
			{
				"remove-index-1.ontology.v2",
				(const Update *) &(Update[]) {
					{ "updates/remove-index-1-post.rq" },
					{ NULL },
				},
			},
			{ NULL }
		},
	},
	{
		.test_name = "/core/ontology-change/add-domain-index",
		.changes = (const ChangeInfo *) &(ChangeInfo[]) {
			{
				"add-domain-index-1.ontology.v1",
				(const Update *) &(Update[]) {
					{ "updates/add-domain-index-1-pre.rq" },
					{ NULL },
				},
			},
			{
				"add-domain-index-1.ontology.v2",
				.updates = (const Update *) &(Update[]) {
					{ "updates/add-domain-index-1-post.rq" },
					{ NULL },
				},
				.checks = (const Query *) &(Query[]) {
					{ "queries/add-domain-index-1" },
					{ NULL },
				},
			},
			{ NULL }
		},
	},
	{
		.test_name = "/core/ontology-change/remove-domain-index",
		.changes = (const ChangeInfo *) &(ChangeInfo[]) {
			{
				"remove-domain-index-1.ontology.v1",
				(const Update *) &(Update[]) {
					{ "updates/remove-domain-index-1-pre.rq" },
					{ NULL },
				},
			},
			{
				"remove-domain-index-1.ontology.v2",
			},
			{
				"remove-domain-index-1.ontology.v3",
				.updates = (const Update *) &(Update[]) {
					{ "updates/remove-domain-index-1-post.rq" },
					{ NULL },
				},
				.checks = (const Query *) &(Query[]) {
					{ "queries/remove-domain-index-1" },
					{ NULL },
				},
			},
			{ NULL }
		},
	},
	{
		.test_name = "/core/ontology-change/add-secondary-index",
		.changes = (const ChangeInfo *) &(ChangeInfo[]) {
			{
				"add-secondary-index-1.ontology.v1",
				(const Update *) &(Update[]) {
					{ "updates/add-secondary-index-1-pre.rq" },
					{ NULL },
				},
			},
			{
				"add-secondary-index-1.ontology.v2",
			},
			{ NULL }
		},
	},
	{
		.test_name = "/core/ontology-change/remove-secondary-index",
		.changes = (const ChangeInfo *) &(ChangeInfo[]) {
			{
				"remove-secondary-index-1.ontology.v1",
				(const Update *) &(Update[]) {
					{ "updates/remove-secondary-index-1-pre.rq" },
					{ NULL },
				},
			},
			{
				"remove-secondary-index-1.ontology.v2",
			},
			{ NULL }
		},
	},
	{
		.test_name = "/core/ontology-change/add-fts-property",
		.changes = (const ChangeInfo *) &(ChangeInfo[]) {
			{
				"add-fts-property-1.ontology.v1",
			},
			{
				"add-fts-property-1.ontology.v2",
				.updates = (const Update *) &(Update[]) {
					{ "updates/add-fts-property-1.rq" },
					{ NULL },
				},
				.checks = (const Query *) &(Query[]) {
					{ "queries/add-fts-property-1-post" },
					{ NULL },
				},
			},
			{ NULL }
		},
	},
	{
		.test_name = "/core/ontology-change/fts-cardinality-change",
		.changes = (const ChangeInfo *) &(ChangeInfo[]) {
			{
				"fts-cardinality-change-1.ontology.v1",
				(const Update *) &(Update[]) {
					{ "updates/fts-cardinality-change-1-pre.rq" },
					{ NULL },
				},
			},
			{
				"fts-cardinality-change-1.ontology.v2",
				(const Update *) &(Update[]) {
					{ "updates/fts-cardinality-change-1-post.rq" },
					{ NULL },
				},
			},
			{ NULL }
		},
	},
	{
		.test_name = "/core/ontology-change/make-non-fts",
		.changes = (const ChangeInfo *) &(ChangeInfo[]) {
			{
				"make-non-fts-1.ontology.v1",
				(const Update *) &(Update[]) {
					{ "updates/make-non-fts-1-pre.rq" },
					{ NULL },
				},
			},
			{
				"make-non-fts-1.ontology.v2",
				.updates = (const Update *) &(Update[]) {
					{ "updates/make-non-fts-1-post.rq" },
					{ NULL },
				},
				.checks = (const Query *) &(Query[]) {
					{ "queries/make-non-fts-1" },
					{ NULL },
				},
			},
			{ NULL }
		},
	},
	{
		.test_name = "/core/ontology-change/make-non-fts-2",
		.changes = (const ChangeInfo *) &(ChangeInfo[]) {
			{
				"make-non-fts-2.ontology.v1",
				(const Update *) &(Update[]) {
					{ "updates/make-non-fts-2-pre.rq" },
					{ NULL },
				},
			},
			{
				"make-non-fts-2.ontology.v2",
				.checks = (const Query *) &(Query[]) {
					{ "queries/make-non-fts-2", TRUE },
					{ NULL },
				},
			},
			{ NULL }
		},
	},
	{
		.test_name = "/core/ontology-change/add-documented-property",
		.changes = (const ChangeInfo *) &(ChangeInfo[]) {
			{
				"add-documented-property-1.ontology.v1",
			},
			{
				"add-documented-property-1.ontology.v2",
				.checks = (const Query *) &(Query[]) {
					{ "queries/add-documented-property-1" },
					{ NULL },
				},
			},
			{ NULL }
		},
	},
	{
		.test_name = "/core/ontology-change/change-property-documentation",
		.changes = (const ChangeInfo *) &(ChangeInfo[]) {
			{
				"change-property-documentation-1.ontology.v1",
			},
			{
				"change-property-documentation-1.ontology.v2",
				.checks = (const Query *) &(Query[]) {
					{ "queries/change-property-documentation-1" },
					{ NULL },
				},
			},
			{ NULL }
		},
	},
	{
		.test_name = "/core/ontology-change/graphs",
		.changes = (const ChangeInfo *) &(ChangeInfo[]) {
			{
				"graphs-1.ontology.v1",
				.updates = (const Update *) &(Update[]) {
					{ "updates/graphs-1-pre.rq" },
					{ NULL },
				},
			},
			{
				"graphs-1.ontology.v2",
				.updates = (const Update *) &(Update[]) {
					{ "updates/graphs-1-post.rq" },
					{ NULL },
				},
				.checks = (const Query *) &(Query[]) {
					{ "queries/graphs-1" },
					{ NULL },
				},
			},
			{ NULL }
		},
	},
	{
		.test_name = "/core/ontology-change/no-changes",
		.changes = (const ChangeInfo *) &(ChangeInfo[]) {
			{
				"no-changes-1.ontology.v1",
			},
			{
				"no-changes-1.ontology.v2",
			},
			{ NULL }
		},
	},
};

static void
query_helper (TrackerSparqlConnection *conn,
              const gchar             *query_filename,
              const gchar             *results_filename,
              gboolean                 expect_error)
{
	GError *error = NULL;
	gchar *query = NULL;
	gchar *results = NULL;
	GString *test_results = NULL;
	TrackerSparqlCursor *cursor;
	gboolean retval;

	retval = g_file_get_contents (query_filename, &query, NULL, &error);
	g_assert_true (retval);
	g_assert_no_error (error);

	retval = g_file_get_contents (results_filename, &results, NULL, &error);
	g_assert_true (retval);
	g_assert_no_error (error);

	cursor = tracker_sparql_connection_query (conn, query, NULL, &error);

	if (expect_error) {
		g_assert_nonnull (error);
		return;
	}

	g_assert_no_error (error);

	/* compare results with reference output */
	test_results = g_string_new ("");

	if (cursor) {
		gint col;

		while (tracker_sparql_cursor_next (cursor, NULL, &error)) {
			for (col = 0; col < tracker_sparql_cursor_get_n_columns (cursor); col++) {
				const gchar *str;

				if (col > 0) {
					g_string_append (test_results, "\t");
				}

				str = tracker_sparql_cursor_get_string (cursor, col, NULL);
				if (str != NULL) {
					/* bound variable */
					g_string_append_printf (test_results, "\"%s\"", str);
				}
			}

			g_string_append (test_results, "\n");
		}

		g_object_unref (cursor);
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

	g_string_free (test_results, TRUE);
	g_free (results);
	g_free (query);
}

static void
handle_queries (TrackerSparqlConnection *conn,
                const gchar             *prefix,
                const Query             *queries)
{
	while (queries->query) {
		gchar *test_prefix, *query_filename, *results_filename;

		test_prefix = g_build_filename (prefix, "change", queries->query, NULL);
		query_filename = g_strconcat (test_prefix, ".rq", NULL);
		results_filename = g_strconcat (test_prefix, ".out", NULL);
		query_helper (conn, query_filename, results_filename, queries->expect_error);
		queries++;
	}
}

static void
handle_updates (TrackerSparqlConnection *conn,
                const gchar             *prefix,
                const Update            *updates)
{
	while (updates->update) {
		gchar *file = NULL, *queries;
		GError *error = NULL;

		file = g_build_filename (prefix, "change", updates->update, NULL);

		if (g_file_get_contents (file, &queries, NULL, NULL)) {
			tracker_sparql_connection_update (conn,
			                                  queries,
			                                  NULL,
			                                  &error);

			if (updates->expect_error)
				g_assert_nonnull (error);
			else
				g_assert_no_error (error);

			g_free (queries);
		}

		g_free (file);
		updates++;
	}
}

static void
test_ontology_change (gconstpointer context)
{
	gchar *ontology_file;
	GFile *file2;
	gchar *prefix, *build_prefix, *ontologies;
	gchar *data_dir, *ontology_dir;
	guint i;
	GError *error = NULL;
	GFile *data_location, *test_schemas;
	TrackerSparqlConnection *conn;
	const ChangeTest *test = context;
	gint retval;

	prefix = g_build_path (G_DIR_SEPARATOR_S, TOP_SRCDIR, "tests", "core", NULL);
	build_prefix = g_build_path (G_DIR_SEPARATOR_S, TOP_BUILDDIR, "tests", "core", NULL);
	ontologies = g_build_filename (prefix, "ontologies", NULL);

	ontology_file = g_build_path (G_DIR_SEPARATOR_S, build_prefix, "change", "ontologies", "99-example.ontology", NULL);

	file2 = g_file_new_for_path (ontology_file);

	g_file_delete (file2, NULL, NULL);

	ontology_dir = g_build_path (G_DIR_SEPARATOR_S, build_prefix, "change", "ontologies", NULL);
	retval = g_mkdir_with_parents (ontology_dir, 0777);
	g_assert_cmpint (retval , ==, 0);
	test_schemas = g_file_new_for_path (ontology_dir);
	g_free (ontology_dir);

	data_dir = g_build_filename (g_get_tmp_dir (), "tracker-ontology-change-test-XXXXXX", NULL);
	data_dir = g_mkdtemp_full (data_dir, 0700);
	data_location = g_file_new_for_path (data_dir);
	g_free (data_dir);

	for (i = 0; test->changes[i].ontology; i++) {
		GFile *file1;
		gchar *source;
		gchar *from, *to;

		source = g_build_filename (prefix, "change", "source", test->changes[i].ontology, NULL);
		file1 = g_file_new_for_path (source);
		g_free (source);

		from = g_file_get_path (file1);
		to = g_file_get_path (file2);
		g_debug ("copy %s to %s", from, to);
		g_free (from);
		g_free (to);

		g_file_copy (file1, file2, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, &error);
		g_object_unref (file1);

		g_assert_no_error (error);
		g_assert_cmpint (g_chmod (ontology_file, 0666), ==, 0);

		conn = tracker_sparql_connection_new (TRACKER_SPARQL_CONNECTION_FLAGS_NONE,
		                                      data_location,
		                                      test_schemas,
		                                      NULL, &error);
		if (test->changes[i].expect_error)
			g_assert_nonnull (error);
		else
			g_assert_no_error (error);

		if (test->changes[i].updates)
			handle_updates (conn, prefix, test->changes[i].updates);

		if (test->changes[i].checks)
			handle_queries (conn, prefix, test->changes[i].checks);

		g_clear_object (&conn);
		g_clear_error (&error);
	}

	/* Test opening for a last time */
	conn = tracker_sparql_connection_new (TRACKER_SPARQL_CONNECTION_FLAGS_NONE,
	                                      data_location,
	                                      NULL, NULL, &error);
	g_assert_no_error (error);

	g_object_unref (conn);

	g_file_delete (file2, NULL, NULL);

	g_object_unref (file2);
	g_object_unref (test_schemas);
	g_object_unref (data_location);
	g_free (ontologies);
	g_free (build_prefix);
	g_free (prefix);
	g_free (ontology_file);
}

int
main (int argc, char **argv)
{
	gint result;
	guint i;

	g_test_init (&argc, &argv, NULL);

	for (i = 0; i < G_N_ELEMENTS (tests); i++)
		g_test_add_data_func (tests[i].test_name, &tests[i], test_ontology_change);

	result = g_test_run ();

	return result;
}
