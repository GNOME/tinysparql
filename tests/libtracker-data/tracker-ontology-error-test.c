/*
 * Copyright (C) 2021, Abanoub Ghadban <abanoub.gdb@gmail.com>
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

typedef struct _TestInfo TestInfo;

struct _TestInfo {
	const gchar *test_name;
};

const TestInfo tests[] = {
	{ "ontology-error/unknown-prefix-001" },
	{ "ontology-error/unknown-prefix-002" },
	{ "ontology-error/unknown-prefix-003" },
	{ NULL }
};

static void
ontology_error_helper (GFile *ontology_location, char *error_path)
{
	TrackerDataManager *manager;
	gchar *error_msg = NULL;
	GError* error = NULL;
	GError* ontology_error = NULL;
	GMatchInfo *matchInfo;
	GRegex *regex;

	manager = tracker_data_manager_new (TRACKER_DB_MANAGER_IN_MEMORY,
	                                    NULL, ontology_location,
	                                    100, 100);
	g_initable_init (G_INITABLE (manager), NULL, &ontology_error);
	g_assert_true (ontology_error != NULL);

	g_file_get_contents (error_path, &error_msg, NULL, &error);
	g_assert_no_error (error);

	regex = g_regex_new (error_msg, 0, 0, &error);
	g_regex_match (regex, ontology_error->message, 0, &matchInfo);

	if (!g_match_info_matches (matchInfo))
		g_error ("Error Message: %s doesn't match the regular expression: %s",
		         ontology_error->message, error_msg);

	g_regex_unref (regex);
	g_match_info_unref (matchInfo);
	g_error_free (ontology_error);
	g_free (error_msg);
	g_object_unref (manager);
}

static void
test_ontology_error (void)
{
	gchar *test_ontology_path;
	gchar *test_ontology_dir;
	gchar *prefix, *build_prefix;
	guint i;
	GError *error = NULL;
	GFile *test_ontology_file;
	GFile *test_schemas;

	prefix = g_build_path (G_DIR_SEPARATOR_S, TOP_SRCDIR, "tests", "libtracker-data", NULL);
	build_prefix = g_build_path (G_DIR_SEPARATOR_S, TOP_BUILDDIR, "tests", "libtracker-data", NULL);

	// Create a temporary directory inside the build directory to store in it the ontology that will be tested
	test_ontology_dir = g_build_path (G_DIR_SEPARATOR_S, build_prefix, "ontology-error", "ontologies", NULL);
	g_mkdir_with_parents (test_ontology_dir, 0777);
	test_schemas = g_file_new_for_path (test_ontology_dir);

	test_ontology_path = g_build_path (G_DIR_SEPARATOR_S, test_ontology_dir, "test.ontology", NULL);
	test_ontology_file = g_file_new_for_path (test_ontology_path);
	g_file_delete (test_ontology_file, NULL, NULL);
	g_free (test_ontology_dir);

	for (i = 0; tests[i].test_name; i++) {
		GFile *source_ontology_file;
		gchar *source_ontology_filename = g_strconcat (tests[i].test_name, ".ontology", NULL);
		gchar *source_ontology_path = g_build_path (G_DIR_SEPARATOR_S, prefix, source_ontology_filename, NULL);
		gchar *error_filename = g_strconcat (tests[i].test_name, ".out", NULL);
		gchar *error_path = g_build_path (G_DIR_SEPARATOR_S, prefix, error_filename, NULL);
		gchar *from, *to;

		source_ontology_file = g_file_new_for_path (source_ontology_path);

		from = g_file_get_path (source_ontology_file);
		to = g_file_get_path (test_ontology_file);
		g_debug ("copy %s to %s", from, to);
		g_free (from);
		g_free (to);

		// Copy the ontology to the temporary ontologies directory
		g_file_copy (source_ontology_file, test_ontology_file, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, &error);

		g_assert_no_error (error);
		g_assert_cmpint (g_chmod (test_ontology_path, 0666), ==, 0);

		ontology_error_helper (test_schemas, error_path);

		g_free (source_ontology_filename);
		g_free (source_ontology_path);
		g_free (error_filename);
		g_free (error_path);
		g_object_unref (source_ontology_file);
	}

	g_file_delete (test_ontology_file, NULL, NULL);
	g_object_unref (test_ontology_file);
	g_object_unref (test_schemas);
	g_free (build_prefix);
	g_free (prefix);
	g_free (test_ontology_path);
}

int
main (int argc, char **argv)
{
	gint result;

	g_test_init (&argc, &argv, NULL);

	g_test_add_func ("/libtracker-data/ontology-error", test_ontology_error);
	result = g_test_run ();

	return result;
}
