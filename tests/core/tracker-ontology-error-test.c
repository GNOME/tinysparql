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

#include <tinysparql.h>

typedef struct _TestInfo TestInfo;

struct _TestInfo {
	const gchar *test_name;
};

const TestInfo tests[] = {
	{ "ontology-error/unknown-prefix-001" },
	{ "ontology-error/unknown-prefix-002" },
	{ "ontology-error/unknown-prefix-003" },
	{ "ontology-error/incomplete-property-001" },
	{ "ontology-error/parsing-errors-001" },
	{ NULL }
};

GString *recorded_error_msgs;
GPrintFunc old_printerr_handler;

static void
printerr_handler (const gchar *string)
{
	gchar *error_msg = g_strdup (string);
	g_strstrip (error_msg);

	if (recorded_error_msgs->len)
		g_string_append (recorded_error_msgs, "~\n");

	g_string_append_printf (recorded_error_msgs, "%s\n", error_msg);
	g_free (error_msg);
}

static void
record_printed_errors ()
{
	recorded_error_msgs = g_string_new ("");
	old_printerr_handler = g_set_printerr_handler (printerr_handler);
}

static void
stop_error_recording (gchar **printed_error_msgs)
{
	g_set_printerr_handler (old_printerr_handler);

	*printed_error_msgs = g_string_free (recorded_error_msgs, FALSE);
}

static gchar*
load_error_msgs (gchar *errors_path, gchar *ontology_path)
{
	GError *error = NULL;
	gchar *raw_errors = NULL, *error_msg;
	GString *prefixed_errors = NULL;
	gchar *ret;
	GFile *ontology_file = g_file_new_for_path (ontology_path);
	gchar *ontology_uri = g_file_get_uri (ontology_file);
	gboolean retval;

	retval = g_file_get_contents (errors_path, &raw_errors, NULL, &error);
	g_assert_true (retval);
	g_assert_no_error (error);

	error_msg = strtok (raw_errors, "~");
	while (error_msg) {
		if (!prefixed_errors) {
			prefixed_errors = g_string_new ("");
		} else {
			g_string_append (prefixed_errors, "~\n");
		}

		g_strstrip (error_msg);
		g_string_append_printf (prefixed_errors, "%s:%s\n",
		                        ontology_uri, error_msg);
		error_msg = strtok (NULL, "~");
	}

	ret = prefixed_errors ? g_string_free (prefixed_errors, FALSE) : NULL;

	g_free (raw_errors);
	g_object_unref (ontology_file);
	g_free (ontology_uri);
	return ret;
}

static void
assert_same_output (gchar *output1, gchar* output2)
{
	GError *error = NULL;
	gchar *quoted_output1, *quoted_output2;
	gchar *command_line;
	gchar *quoted_command_line;
	gchar *shell;
	gchar *diff;

	if (strcmp (output1, output2) == 0)
		return;

	/* print result difference */
	quoted_output1 = g_shell_quote (output1);
	quoted_output2 = g_shell_quote (output2);

	command_line = g_strdup_printf ("diff -u <(echo %s) <(echo %s)", quoted_output1, quoted_output2);
	quoted_command_line = g_shell_quote (command_line);
	shell = g_strdup_printf ("bash -c %s", quoted_command_line);
	g_spawn_command_line_sync (shell, &diff, NULL, NULL, &error);
	g_assert_no_error (error);

	g_error ("%s", diff);

	g_free (quoted_output1);
	g_free (quoted_output2);
	g_free (command_line);
	g_free (quoted_command_line);
	g_free (shell);
	g_free (diff);
}

static void
ontology_error_helper (GFile *ontology_location, char *error_path)
{
	TrackerSparqlConnection *conn;
	gchar *error_msg = NULL;
	GError* error = NULL;
	GError* ontology_error = NULL;
	GMatchInfo *matchInfo;
	GRegex *regex;
	gboolean retval;

	conn = tracker_sparql_connection_new (TRACKER_SPARQL_CONNECTION_FLAGS_NONE,
	                                      NULL, ontology_location,
	                                      NULL, &ontology_error);
	g_assert_true (ontology_error != NULL);

	retval = g_file_get_contents (error_path, &error_msg, NULL, &error);
	g_assert_true (retval);
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
	g_clear_object (&conn);
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
	gint retval;

	prefix = g_build_path (G_DIR_SEPARATOR_S, TOP_SRCDIR, "tests", "core", NULL);
	build_prefix = g_build_path (G_DIR_SEPARATOR_S, TOP_BUILDDIR, "tests", "core", NULL);

	// Create a temporary directory inside the build directory to store in it the ontology that will be tested
	test_ontology_dir = g_build_path (G_DIR_SEPARATOR_S, build_prefix, "ontology-error", "ontologies", NULL);
	retval = g_mkdir_with_parents (test_ontology_dir, 0777);
	g_assert_cmpint (retval, ==, 0);
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
		gchar *errors_filename = g_strconcat (tests[i].test_name, ".errors.out", NULL);
		gchar *error_path = g_build_path (G_DIR_SEPARATOR_S, prefix, error_filename, NULL);
		gchar *errors_path = g_build_path (G_DIR_SEPARATOR_S, prefix, errors_filename, NULL);
		gchar *from, *to;
		gchar *expected_error_msgs = "", *printed_error_msgs;
		gboolean copy_result;

		source_ontology_file = g_file_new_for_path (source_ontology_path);

		from = g_file_get_path (source_ontology_file);
		to = g_file_get_path (test_ontology_file);
		g_debug ("copy %s to %s", from, to);
		g_free (from);

		/* Copy the ontology to the temporary ontologies directory */
		copy_result = g_file_copy (source_ontology_file, test_ontology_file, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, &error);
		g_assert_no_error (error);
		g_assert_true (copy_result);
		g_assert_cmpint (g_chmod (test_ontology_path, 0666), ==, 0);

		/* The error messages are prefixed with the ontology path which contain the error
		 * So, it needs the ontology path to prefix the expected error messages with it */
		if (g_file_test (errors_path, G_FILE_TEST_EXISTS))
			expected_error_msgs = load_error_msgs (errors_path, to);

		record_printed_errors ();
		ontology_error_helper (test_schemas, error_path);
		stop_error_recording (&printed_error_msgs);

		assert_same_output (expected_error_msgs, printed_error_msgs);

		g_free (to);
		g_free (source_ontology_filename);
		g_free (source_ontology_path);
		g_free (error_filename);
		g_free (errors_filename);
		g_free (error_path);
		g_free (errors_path);
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

	g_test_add_func ("/core/ontology-error", test_ontology_error);
	result = g_test_run ();

	return result;
}
