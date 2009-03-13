/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2009, Nokia (urho.konttori@nokia.com)
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

#include <glib/gstdio.h>

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
dump_metadata_item (gpointer key,
		    gpointer value,
		    gpointer user_data)
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
dump_metadata (GHashTable *metadata)
{
	g_assert (metadata != NULL);
	g_hash_table_foreach (metadata, dump_metadata_item, NULL);
}

static void
check_metadata (GHashTable  *metadata, 
		const gchar *key, 
		const gchar *value)
{
	gchar *cvalue;

	g_assert (metadata != NULL);
	g_assert (key != NULL);
	g_assert (value != NULL);
	
	cvalue = g_hash_table_lookup (metadata, key);
	g_assert (cvalue != NULL);
	g_assert_cmpstr (cvalue, ==, value);
}

TrackerExtractData *
tracker_test_extract_get_extract (const gchar *mime)
{
	TrackerExtractData *data;
	
	/* Search for exact match first */
	data = tracker_get_extract_data ();	
	while (data->mime) {
		if (strcmp (data->mime, mime) == 0) {
			return data;
		}
		data++;
	}

	/* Search for generic */
	data = tracker_get_extract_data ();
	while (data->mime) {
		if (g_pattern_match_simple (data->mime, mime)) {
			return data;
		}
		data++;
	}

	return NULL;
}

void
tracker_test_extract_file (const TrackerExtractData *data, 
			   const gchar              *file, 
			   const gchar              *test_data_file)
{
	GHashTable *metadata;
	GHashTable *test_data;
	gchar *filename;	
	gchar *test_data_filename;
	GHashTableIter iter;
	gpointer key, value;

	g_assert (data != NULL);
	g_assert (file != NULL);
	g_assert (test_data_file != NULL);

	filename = g_strconcat (TEST_DATA_DIR, file, NULL);
	test_data_filename = g_strconcat (TEST_DATA_DIR, test_data_file, NULL);

	metadata = g_hash_table_new_full (g_str_hash,
					  g_str_equal,
					  g_free,
					  g_free);

	(*data->extract) (filename, metadata);

	test_data = parse_file (test_data_filename);

	g_hash_table_iter_init (&iter, test_data);

	if (0) {
		dump_metadata (metadata);
	}

	while (g_hash_table_iter_next (&iter, &key, &value)) {
		check_metadata (metadata, key, value);
	}

	g_hash_table_destroy (metadata);
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
		GHashTable *metadata;
		gchar filename[256];
		gchar tmp[256];

		metadata = g_hash_table_new_full (g_str_hash,
						  g_str_equal,
						  g_free,
						  g_free);

		

		if (sprintf (tmp, "%s%s",TEST_DATA_DIR, file_match) < 0) {
			g_assert_not_reached();
		}

		if (sprintf (filename, tmp, i) < 0) {
			g_assert_not_reached();
		}

		(*data->extract) (filename, metadata);

		g_assert (g_hash_table_size (metadata) > 0);

		g_hash_table_destroy (metadata);
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
		GHashTable *metadata;
		gchar filename[256];
		gchar tmp[256];

		metadata = g_hash_table_new_full (g_str_hash,
						  g_str_equal,
						  g_free,
						  g_free);

		if (sprintf (tmp, "%s%s", TEST_DATA_DIR, file_match) < 0) {
			g_assert_not_reached ();
		}

		if (sprintf (filename, tmp, i) < 0) {
			g_assert_not_reached ();
		}

		(*data->extract) (filename, metadata);

		g_assert (g_hash_table_size (metadata) > 0);

		g_hash_table_destroy (metadata);
	}		
}
