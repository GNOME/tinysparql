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

#include <unistd.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <glib-object.h>
#include <gmodule.h>

#include "tracker-extract-test-utils.h"

static GHashTable *
parse_file (const gchar *filename)
{
	GHashTable *testdata;
	GScanner *scanner;
	GTokenType ttype;
	GScannerConfig config = {
		" \t\r\n",                     /* characters to skip */
		G_CSET_a_2_z "_" G_CSET_A_2_Z, /* identifier start */
		G_CSET_a_2_z "_.:" G_CSET_A_2_Z G_CSET_DIGITS,/* identifier cont. */
		"#\n",                         /* single line comment */
		TRUE,                          /* case_sensitive */
		TRUE,                          /* skip multi-line comments */
		TRUE,                          /* skip single line comments */
		FALSE,                         /* scan multi-line comments */
		TRUE,                          /* scan identifiers */
		TRUE,                          /* scan 1-char identifiers */
		FALSE,                         /* scan NULL identifiers */
		FALSE,                         /* scan symbols */
		FALSE,                         /* scan binary */
		FALSE,                         /* scan octal */
		TRUE,                          /* scan float */
		TRUE,                          /* scan hex */
		FALSE,                         /* scan hex dollar */
		TRUE,                          /* scan single quote strings */
		TRUE,                          /* scan double quite strings */
		TRUE,                          /* numbers to int */
		FALSE,                         /* int to float */
		TRUE,                          /* identifier to string */
		TRUE,                          /* char to token */
		FALSE,                         /* symbol to token */
		FALSE,                         /* scope 0 fallback */
		FALSE                          /* store int64 */
	};
	gint fd;

	testdata = g_hash_table_new_full (g_str_hash,
	                                  g_str_equal,
	                                  g_free,
	                                  g_free);

	fd = g_open (filename, O_RDONLY);

	g_assert (fd >= 0);

	scanner = g_scanner_new (&config);
	g_scanner_input_file (scanner, fd);
	scanner->input_name = filename;

	for (ttype = g_scanner_get_next_token(scanner);
	     ttype != G_TOKEN_EOF;
	     ttype = g_scanner_get_next_token (scanner)) {
		if (ttype == G_TOKEN_IDENTIFIER) {
			gchar key[256];

			strcpy (key, scanner->value.v_identifier);

			ttype = g_scanner_get_next_token(scanner);
			g_assert (ttype == G_TOKEN_EQUAL_SIGN);

			ttype = g_scanner_get_next_token(scanner);

			switch (ttype) {
			case G_TOKEN_STRING:
				g_hash_table_insert (testdata,
				                     g_strdup(key),
				                     g_strdup(scanner->value.v_string));
				break;

			default:
				g_assert_not_reached();

			}
		}
	}

	g_scanner_destroy (scanner);
	close (fd);

	return testdata;
}

static void
dump_metadata_item (const gchar *key,
                    const gchar *value)
{
	gchar *value_utf8;

	g_assert (key != NULL);
	g_assert (value != NULL);

	value_utf8 = g_locale_to_utf8 (value, -1, NULL, NULL, NULL);

	if (value_utf8) {
		g_print ("%s=%s;\n", (gchar*) key, value_utf8);
		g_free (value_utf8);
	}
}

static void
dump_metadata (GPtrArray *metadata)
{
	guint i;

	g_assert (metadata != NULL);

	for (i = 0; i < metadata->len; i++) {
		GValueArray *statement;
		const gchar *subject;
		const gchar *predicate;
		const gchar *object;

		statement = metadata->pdata[i];
		subject = g_value_get_string (&statement->values[0]);
		predicate = g_value_get_string (&statement->values[1]);
		object = g_value_get_string (&statement->values[2]);

		dump_metadata_item (predicate, object);
	}
}

static void
check_metadata (GPtrArray   *metadata,
                const gchar *key,
                const gchar *value)
{
	GValueArray *statement;
	const gchar *subject, *predicate, *object;
	gboolean found = FALSE;
	guint i;

	g_assert (metadata != NULL);
	g_assert (key != NULL);
	g_assert (value != NULL);

	for (i = 0; i < metadata->len; i++) {
		statement = metadata->pdata[i];
		subject = g_value_get_string (&statement->values[0]);
		predicate = g_value_get_string (&statement->values[1]);
		object = g_value_get_string (&statement->values[2]);

		if (g_strcmp0 (key, predicate) == 0) {
			found = TRUE;
			break;
		}
	}

	g_assert (found);
	g_assert (object != NULL);
	g_assert_cmpstr (value, ==, object);
}

TrackerExtractData *
tracker_test_extract_get_extract (const gchar *path, const gchar *mime)
{
	TrackerExtractData *data;
	TrackerExtractData *data_iter;
	GModule *module;
	TrackerExtractDataFunc func;
	GError *error;

	if (!g_module_supported ()) {
		g_error ("Modules are not supported for this platform");
		return NULL;
	}

	error = NULL;

	module = g_module_open (path, G_MODULE_BIND_LOCAL);
	if (!module) {
		g_error ("Could not load module '%s': %s", path, g_module_error ());
		return NULL;
	}

	g_module_make_resident (module);

	if (g_module_symbol (module, "tracker_get_extract_data", (gpointer *) &func)) {
		data = (func) ();
	} else {
		g_error ("Could not get accesspoint to the module");
		return;
	}

	/* Search for exact match first */
	data_iter = data;
	while (data_iter->mime) {
		if (strcmp (data_iter->mime, mime) == 0) {
			return data_iter;
		}
		data_iter++;
	}

	/* Search for generic */
	data_iter = data;
	while (data_iter->mime) {
		if (g_pattern_match_simple (data_iter->mime, mime)) {
			return data_iter;
		}
		data_iter++;
	}

	return NULL;
}

static void
free_statements (GPtrArray *metadata)
{
	guint i;

	for (i = 0; i < metadata->len; i++) {
		GValueArray *statement;
		statement = metadata->pdata[i];
		g_value_array_free (statement);
	}

	g_ptr_array_free (metadata, TRUE);
}

void
tracker_test_extract_file (const TrackerExtractData *data,
                           const gchar              *file,
                           const gchar              *test_data_file)
{
	GPtrArray  *metadata;
	GHashTable *test_data;
	gchar      *filename, *uri;
	gchar      *test_data_filename;

	GHashTableIter iter;
	gpointer key, value;

	g_assert (data != NULL);
	g_assert (file != NULL);
	g_assert (test_data_file != NULL);

	filename = g_strconcat (TEST_DATA_DIR, file, NULL);
	test_data_filename = g_strconcat (TEST_DATA_DIR, test_data_file, NULL);

	metadata = g_ptr_array_new ();

	uri = g_filename_to_uri (filename, NULL, NULL);
	(*data->extract) (uri, metadata);
	g_free (uri);

	test_data = parse_file (test_data_filename);

	g_hash_table_iter_init (&iter, test_data);

	if (0) {
		dump_metadata (metadata);
	}

	while (g_hash_table_iter_next (&iter, &key, &value)) {
		check_metadata (metadata, key, value);
	}

	free_statements (metadata);

	g_hash_table_destroy (test_data);
}

void
tracker_test_extract_file_performance (const TrackerExtractData *data,
                                       const gchar              *file_match,
                                       guint                     file_count)
{
	double perftime;
	guint i;

	g_assert (data != NULL);
	g_assert (file_match != NULL);
	g_assert (file_count > 0);

	g_test_timer_start ();

	for (i = 1; i <= file_count; i++) {
		GPtrArray *metadata;
		gchar filename[256], *uri;
		gchar tmp[256];

		metadata = g_ptr_array_new ();

		if (sprintf (tmp, "%s%s",TEST_DATA_DIR, file_match) < 0) {
			g_assert_not_reached();
		}

		if (sprintf (filename, tmp, i) < 0) {
			g_assert_not_reached();
		}

		uri = g_filename_to_uri (filename, NULL, NULL);
		(*data->extract) (uri, metadata);
		g_free (uri);

		g_assert (metadata->len > 0);

		free_statements (metadata);

	}

	perftime = g_test_timer_elapsed();

	g_debug ("Time was: %f", perftime);

	g_test_minimized_result (perftime, "Time of the performance tests");
}

void
tracker_test_extract_file_access (const TrackerExtractData *data,
                                  const gchar              *file_match,
                                  guint                     file_count)
{
	guint i;

	g_assert (data != NULL);
	g_assert (file_match != NULL);
	g_assert (file_count > 0);

	for (i = 1; i <= file_count; i++) {
		gchar *uri;
		GPtrArray *metadata;
		gchar filename[256];
		gchar tmp[256];

		metadata = g_ptr_array_new ();

		if (sprintf (tmp, "%s%s",TEST_DATA_DIR, file_match) < 0) {
			g_assert_not_reached();
		}

		if (sprintf (filename, tmp, i) < 0) {
			g_assert_not_reached ();
		}

		uri = g_filename_to_uri (filename, NULL, NULL);
		(*data->extract) (uri, metadata);
		g_free (uri);

		g_assert (metadata->len > 0);

		free_statements (metadata);
	}
}

/* This is added because tracker-main.c includes this file and so
 * should we otherwise it is missing when we try to build the tests.
 */
TrackerHal *
tracker_main_get_hal (void)
{
	return NULL;
}


