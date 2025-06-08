/*
 * Copyright (C) 2023, Red Hat Inc.
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
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#include <tinysparql.h>
#include <locale.h>

typedef void (*InitTestFunc) (const gchar *data_dir);

typedef struct
{
	const gchar *name;
	const gchar *ontology;
	InitTestFunc pre;
	InitTestFunc post;
} InitTest;

typedef struct
{
	const gchar *name;
	const gchar *ontology;
	const gchar *original_db;
} VersionTest;

static void
reset_locale (const gchar *data_dir)
{
	setlocale (LC_ALL, "C");
}

static void
change_locale (const gchar *data_dir)
{
	setlocale (LC_ALL, "C.UTF-8");
}

static void
fool_integrity_check (const gchar *data_dir)
{
	gchar *path = g_build_filename (data_dir, ".meta.db.corrupted", NULL);
	gboolean retval;

	/* Add the "corrupted" file, to fool next DB open */
	retval = g_file_set_contents (path, "", -1, NULL);
	g_assert_true (retval);
	g_free (path);
}

static void
init_test (gconstpointer context)
{
	gchar *prefix, *data_dir, *ontology_dir;
	GError *error = NULL;
	GFile *data_location, *test_schemas;
	TrackerSparqlConnection *conn;
	const InitTest *test = context;

	prefix = g_build_filename (TOP_SRCDIR, "tests", "core", NULL);

	ontology_dir = g_build_filename (prefix, test->ontology, NULL);
	test_schemas = g_file_new_for_path (ontology_dir);
	g_free (ontology_dir);

	data_dir = g_build_filename (g_get_tmp_dir (), "tracker-initialization-test-XXXXXX", NULL);
	data_dir = g_mkdtemp_full (data_dir, 0700);
	data_location = g_file_new_for_path (data_dir);

	if (test->pre)
		test->pre (data_dir);

	conn = tracker_sparql_connection_new (TRACKER_SPARQL_CONNECTION_FLAGS_NONE,
	                                      data_location,
	                                      test_schemas,
	                                      NULL, &error);
	g_assert_no_error (error);

	/* Add a graph for the fun of it */
	tracker_sparql_connection_update (conn, "CREATE GRAPH <http://example/A>", NULL, &error);
	g_assert_no_error (error);

	g_clear_object (&conn);

	if (test->post)
		test->post (data_dir);

	/* Test opening a second time */
	conn = tracker_sparql_connection_new (TRACKER_SPARQL_CONNECTION_FLAGS_NONE,
	                                      data_location,
	                                      NULL, NULL, &error);
	g_assert_no_error (error);

	g_object_unref (conn);

	g_object_unref (test_schemas);
	g_object_unref (data_location);
	g_free (data_dir);
	g_free (prefix);
}

static void
version_test (gconstpointer context)
{
	gchar *prefix, *data_dir, *ontology_dir, *tar_command, *command;
	GError *error = NULL;
	GFile *data_location, *test_schemas;
	TrackerSparqlConnection *conn;
	const VersionTest *test = context;

	tar_command = g_find_program_in_path ("tar");
	if (!tar_command) {
		g_test_skip ("tar not found in path");
		return;
	}

	prefix = g_build_filename (TOP_SRCDIR, "tests", "core", NULL);

	ontology_dir = g_build_filename (prefix, test->ontology, NULL);
	test_schemas = g_file_new_for_path (ontology_dir);
	g_free (ontology_dir);

	data_dir = g_build_filename (g_get_tmp_dir (), "tracker-initialization-test-XXXXXX", NULL);
	data_dir = g_mkdtemp_full (data_dir, 0700);
	data_location = g_file_new_for_path (data_dir);

	command = g_strdup_printf ("%s -C %s -Jxf %s/%s",
	                           tar_command, data_dir, prefix, test->original_db);
	g_spawn_command_line_sync (command, NULL, NULL, NULL, &error);
	g_assert_no_error (error);

	conn = tracker_sparql_connection_new (TRACKER_SPARQL_CONNECTION_FLAGS_NONE,
	                                      data_location,
	                                      test_schemas, NULL, &error);
	g_assert_no_error (error);

	g_object_unref (conn);

	g_object_unref (data_location);
	g_object_unref (test_schemas);
	g_free (data_dir);
	g_free (prefix);
	g_free (command);
	g_free (tar_command);
}

const InitTest init_tests[] = {
	{ "/core/initialization-test/integrity-check", "initialization/fts", NULL, fool_integrity_check },
	{ "/core/initialization-test/locale-change-fts", "initialization/fts", reset_locale, change_locale },
	{ "/core/initialization-test/locale-change", "initialization/non-fts", reset_locale, change_locale },
};

const VersionTest version_tests[] = {
	{ "/core/version/3.0/fts", "initialization/fts", "initialization/versions/fts.25.tar.xz" },
	{ "/core/version/3.3/fts", "initialization/fts", "initialization/versions/fts.26.tar.xz" },
	{ "/core/version/3.4/fts", "initialization/fts", "initialization/versions/fts.27.tar.xz" },
	{ "/core/version/3.8/fts", "initialization/fts", "initialization/versions/fts.30.tar.xz" },
	{ "/core/version/3.0/non-fts", "initialization/non-fts", "initialization/versions/non-fts.25.tar.xz" },
	{ "/core/version/3.3/non-fts", "initialization/non-fts", "initialization/versions/non-fts.26.tar.xz" },
	{ "/core/version/3.4/non-fts", "initialization/non-fts", "initialization/versions/non-fts.27.tar.xz" },
	{ "/core/version/3.8/non-fts", "initialization/non-fts", "initialization/versions/non-fts.30.tar.xz" },
	{ "/core/version/3.0/nepomuk", "../../src/ontologies/nepomuk", "initialization/versions/nepomuk.25.tar.xz" },
	{ "/core/version/3.3/nepomuk", "../../src/ontologies/nepomuk", "initialization/versions/nepomuk.26.tar.xz" },
	{ "/core/version/3.4/nepomuk", "../../src/ontologies/nepomuk", "initialization/versions/nepomuk.27.tar.xz" },
	{ "/core/version/3.8/nepomuk", "../../src/ontologies/nepomuk", "initialization/versions/nepomuk.30.tar.xz" },
	{ "/core/version/3.8/graphs", "../../src/ontologies/nepomuk", "initialization/versions/graphs.30.tar.xz" },
};

int
main (int argc, char *argv[])
{
	gint result;
	guint i;

	g_test_init (&argc, &argv, NULL);

	for (i = 0; i < G_N_ELEMENTS (init_tests); i++)
		g_test_add_data_func (init_tests[i].name, &init_tests[i], init_test);

	for (i = 0; i < G_N_ELEMENTS (version_tests); i++)
		g_test_add_data_func (version_tests[i].name, &version_tests[i], version_test);

	result = g_test_run ();

	return result;

}
