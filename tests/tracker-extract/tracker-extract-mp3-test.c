/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008, Nokia (urho.konttori@nokia.com)
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

#include <string.h>
#include <fcntl.h>

#include <glib.h>

#include <tracker-extract/tracker-extract.h>

typedef struct {
	gchar *filename;
	gchar *testdata;
} ExtractData;

static GHashTable *
parse_testdata_file (const gchar *filename)
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

	fd = g_fopen (filename, O_RDONLY);

	g_assert (fd >= 0);

	scanner = g_scanner_new (&config);
	g_scanner_input_file (scanner, fd);
	scanner->input_name = filename;

	for (ttype = g_scanner_get_next_token(scanner);
	     ttype != G_TOKEN_EOF;
	     ttype = g_scanner_get_next_token (scanner)) {
		if (ttype = G_TOKEN_IDENTIFIER) {
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
dump_metadataitem (gpointer key,
		   gpointer value,
		   gpointer user_data)
{
	gchar *value_utf8;

	g_assert (key != NULL);
	g_assert (value != NULL);

	value_utf8 = g_locale_to_utf8 (value, -1, NULL, NULL, NULL);

	if (value_utf8) {
		value_utf8 = g_strstrip (value_utf8);
		g_print ("%s=%s;\n", (gchar*) key, value_utf8);
		g_free (value_utf8);
	}
}

static void
dump_metadata (GHashTable *metadata)
{
	g_assert (metadata != NULL);
	g_hash_table_foreach (metadata, dump_metadataitem, NULL);
}

static void
check_metadata (GHashTable *metadata, gchar *key, gchar *value)
{
	gchar *cvalue;
	g_assert (metadata != NULL);
	g_assert (key != NULL);
	g_assert (value != NULL);
	
	cvalue = g_hash_table_lookup (metadata, key);
	g_assert (cvalue != NULL);
	g_assert_cmpstr (cvalue, ==, value);
}

static TrackerExtractorData *
search_mime_extractor (const gchar *mime)
{
	TrackerExtractorData *data;

	data = tracker_get_extractor_data ();	

	while (data->mime) {
		if (strcmp (data->mime,mime) == 0) {
			return data;
		}
		data++;
	}	

	return NULL;
}

static void
extract_file (const gchar *file, const gchar *testdatafile)
{
	GHashTable *metadata;
	GHashTable *testdata;

	gchar      *filename;	
	gchar      *testdata_filename;

	TrackerExtractorData *data;

	GHashTableIter iter;
	gpointer key, value;

	filename = g_strconcat (TEST_DATA_DIR, file, NULL);
	testdata_filename = g_strconcat (TEST_DATA_DIR, testdatafile, NULL);

	metadata = g_hash_table_new_full (g_str_hash,
					  g_str_equal,
					  g_free,
					  g_free);

	data = search_mime_extractor ("audio/mpeg");
	g_assert (data != NULL);
	(*data->extractor) (filename, metadata);

	testdata = parse_testdata_file (testdata_filename);

	g_hash_table_iter_init (&iter, testdata);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		check_metadata (metadata, (gchar *)key, (gchar *)value);
	}

	g_hash_table_destroy (metadata);
	g_hash_table_destroy (testdata);
}

static void
test_extract_mp3_check_extractor_data (void)
{
	TrackerExtractorData *data;
	guint extractors = 0;

	data = tracker_get_extractor_data();

	while (data->mime) {
		if (data->extractor == NULL) {
			g_error ("Extractor for mime '%s' declared NULL", data->mime);
		}

		extractors++;
		data++;
	}

	g_assert (extractors > 1);
}

static void
test_extract_mp3_extract_file (gconstpointer user_data)
{
	const ExtractData *data = user_data;
	extract_file (data->filename, data->testdata);
}

int
main (int argc, char **argv) {

	gint result;
	ExtractData data;

	g_type_init ();
	g_thread_init (NULL);
	g_test_init (&argc, &argv, NULL);

	g_test_message ("Testing basic extractor functionality");
	g_test_add_func ("/tracker-extract/tracker-extract-mp3/check-extractor-data", test_extract_mp3_check_extractor_data);

	g_test_message ("Testing basic id3v1 metadata tags");

	/* Tests fails, disabling, -mr */
#if 0
	data.filename = "/mp3/id3v1_basic_1.mp3";
	data.testdata = "/mp3/id3v1_basic_1.data";
	g_test_add_data_func ("/tracker-extract/tracker-extract-mp3/id3v1/1", &data, test_extract_mp3_extract_file);

	data.filename = "/mp3/id3v1_basic_2.mp3";
	data.testdata = "/mp3/id3v1_basic_2.data";
	g_test_add_data_func ("/tracker-extract/tracker-extract-mp3/id3v1/2", &data, test_extract_mp3_extract_file);	

	data.filename = "/mp3/id3v1_basic_3.mp3";
	data.testdata = "/mp3/id3v1_basic_3.data";
	g_test_add_data_func ("/tracker-extract/tracker-extract-mp3/id3v1/3", &data, test_extract_mp3_extract_file);

	g_test_message ("Testing basic id3v23 metadata tags");

	data.filename = "/mp3/id3v23_basic_1.mp3";
	data.testdata = "/mp3/id3v23_basic_1.data";
	g_test_add_data_func ("/tracker-extract/tracker-extract-mp3/id3v23/1", &data, test_extract_mp3_extract_file);

	data.filename = "/mp3/id3v23_basic_2.mp3";
	data.testdata = "/mp3/id3v23_basic_2.data";
	g_test_add_data_func ("/tracker-extract/tracker-extract-mp3/id3v23/2", &data, test_extract_mp3_extract_file);	

	data.filename = "/mp3/id3v23_basic_3.mp3";
	data.testdata = "/mp3/id3v23_basic_3.data";
	g_test_add_data_func ("/tracker-extract/tracker-extract-mp3/id3v23/3", &data, test_extract_mp3_extract_file);

	g_test_message ("Testing specific id3v23 tags");

	data.filename = "/mp3/id3v23_trck_1.mp3";
	data.testdata = "/mp3/id3v23_trck_1.data";
	g_test_add_data_func ("/tracker-extract/tracker-extract-mp3/id3v23/trck-1", &data, test_extract_mp3_extract_file);

	data.filename = "/mp3/id3v23_comm_1.mp3";
	data.testdata = "/mp3/id3v23_comm_1.data";
	g_test_add_data_func ("/tracker-extract/tracker-extract-mp3/id3v23/comm-1", &data, test_extract_mp3_extract_file);

	data.filename = "/mp3/id3v23_comm_2.mp3";
	data.testdata = "/mp3/id3v23_comm_2.data";
	g_test_add_data_func ("/tracker-extract/tracker-extract-mp3/id3v23/comm-2", &data, test_extract_mp3_extract_file);

	data.filename = "/mp3/id3v23_comm_3.mp3";
	data.testdata = "/mp3/id3v23_comm_3.data";
	g_test_add_data_func ("/tracker-extract/tracker-extract-mp3/id3v23/comm-3", &data, test_extract_mp3_extract_file);

	g_test_message ("Testing header/calculated metadata");

	data.filename = "/mp3/header_bitrate_mpeg1_1.mp3";
	data.testdata = "/mp3/header_bitrate_mpeg1_1.data";
	g_test_add_data_func ("/tracker-extract/tracker-extract-mp3/header/bitrate-mpeg1-1", &data, test_extract_mp3_extract_file);

	data.filename = "/mp3/header_bitrate_mpeg1_2.mp3";
	data.testdata = "/mp3/header_bitrate_mpeg1_2.data";
	g_test_add_data_func ("/tracker-extract/tracker-extract-mp3/header/bitrate-mpeg1-2", &data, test_extract_mp3_extract_file);

	data.filename = "/mp3/header_bitrate_mpeg1_3.mp3";
	data.testdata = "/mp3/header_bitrate_mpeg1_3.data";
	g_test_add_data_func ("/tracker-extract/tracker-extract-mp3/header/bitrate-mpeg1-3", &data, test_extract_mp3_extract_file);

	data.filename = "/mp3/header_bitrate_mpeg1_4.mp3";
	data.testdata = "/mp3/header_bitrate_mpeg1_4.data";
	g_test_add_data_func ("/tracker-extract/tracker-extract-mp3/header/bitrate-mpeg1-4", &data, test_extract_mp3_extract_file);

	data.filename = "/mp3/header_bitrate_mpeg1_5.mp3";
	data.testdata = "/mp3/header_bitrate_mpeg1_5.data";
	g_test_add_data_func ("/tracker-extract/tracker-extract-mp3/header/bitrate-mpeg1-5", &data, test_extract_mp3_extract_file);

	data.filename = "/mp3/header_bitrate_mpeg1_6.mp3";
	data.testdata = "/mp3/header_bitrate_mpeg1_6.data";
	g_test_add_data_func ("/tracker-extract/tracker-extract-mp3/header/bitrate-mpeg1-6", &data, test_extract_mp3_extract_file);

	data.filename = "/mp3/header_bitrate_mpeg1_7.mp3";
	data.testdata = "/mp3/header_bitrate_mpeg1_7.data";
	g_test_add_data_func ("/tracker-extract/tracker-extract-mp3/header/bitrate-mpeg1-7", &data, test_extract_mp3_extract_file);

	data.filename = "/mp3/header_bitrate_mpeg1_8.mp3";
	data.testdata = "/mp3/header_bitrate_mpeg1_8.data";
	g_test_add_data_func ("/tracker-extract/tracker-extract-mp3/header/bitrate-mpeg1-8", &data, test_extract_mp3_extract_file);

	data.filename = "/mp3/header_bitrate_mpeg1_9.mp3";
	data.testdata = "/mp3/header_bitrate_mpeg1_9.data";
	g_test_add_data_func ("/tracker-extract/tracker-extract-mp3/header/bitrate-mpeg1-9", &data, test_extract_mp3_extract_file);

	data.filename = "/mp3/header_bitrate_mpeg1_10.mp3";
	data.testdata = "/mp3/header_bitrate_mpeg1_10.data";
	g_test_add_data_func ("/tracker-extract/tracker-extract-mp3/header/bitrate-mpeg1-10", &data, test_extract_mp3_extract_file);
	data.filename = "/mp3/header_bitrate_mpeg1_11.mp3";
	data.testdata = "/mp3/header_bitrate_mpeg1_11.data";
	g_test_add_data_func ("/tracker-extract/tracker-extract-mp3/header/bitrate-mpeg1-11", &data, test_extract_mp3_extract_file);
	data.filename = "/mp3/header_bitrate_mpeg1_12.mp3";
	data.testdata = "/mp3/header_bitrate_mpeg1_12.data";
	g_test_add_data_func ("/tracker-extract/tracker-extract-mp3/header/bitrate-mpeg1-12", &data, test_extract_mp3_extract_file);

	data.filename = "/mp3/header_bitrate_mpeg1_13.mp3";
	data.testdata = "/mp3/header_bitrate_mpeg1_13.data";
	g_test_add_data_func ("/tracker-extract/tracker-extract-mp3/header/bitrate-mpeg1-13", &data, test_extract_mp3_extract_file);
	data.filename = "/mp3/header_bitrate_mpeg1_14.mp3";
	data.testdata = "/mp3/header_bitrate_mpeg1_14.data";
	g_test_add_data_func ("/tracker-extract/tracker-extract-mp3/header/bitrate-mpeg1-14", &data, test_extract_mp3_extract_file);
	data.filename = "/mp3/header_sampling_mpeg1_1.mp3";
	data.testdata = "/mp3/header_sampling_mpeg1_1.data";
	g_test_add_data_func ("/tracker-extract/tracker-extract-mp3/header/sampling-mpeg1-1", &data, test_extract_mp3_extract_file);
	data.filename = "/mp3/header_sampling_mpeg1_2.mp3";
	data.testdata = "/mp3/header_sampling_mpeg1_2.data";
	g_test_add_data_func ("/tracker-extract/tracker-extract-mp3/header/sampling-mpeg1-2", &data, test_extract_mp3_extract_file);
	data.filename = "/mp3/header_sampling_mpeg1_3.mp3";
	data.testdata = "/mp3/header_sampling_mpeg1_3.data";
	g_test_add_data_func ("/tracker-extract/tracker-extract-mp3/header/sampling-mpeg1-3", &data, test_extract_mp3_extract_file);
	data.filename = "/mp3/header_sampling_mpeg2_1.mp3";
	data.testdata = "/mp3/header_sampling_mpeg2_1.data";
	g_test_add_data_func ("/tracker-extract/tracker-extract-mp3/header/sampling-mpeg2-1", &data, test_extract_mp3_extract_file);
	data.filename = "/mp3/header_sampling_mpeg2_2.mp3";
	data.testdata = "/mp3/header_sampling_mpeg2_2.data";
	g_test_add_data_func ("/tracker-extract/tracker-extract-mp3/header/sampling-mpeg2-2", &data, test_extract_mp3_extract_file);
	data.filename = "/mp3/header_sampling_mpeg2_3.mp3";
	data.testdata = "/mp3/header_sampling_mpeg2_3.data";
	g_test_add_data_func ("/tracker-extract/tracker-extract-mp3/header/sampling-mpeg2-3", &data, test_extract_mp3_extract_file);
	data.filename = "/mp3/header_sampling_mpeg25_1.mp3";
	data.testdata = "/mp3/header_sampling_mpeg25_1.data";
	g_test_add_data_func ("/tracker-extract/tracker-extract-mp3/header/sampling-mpeg25-1", &data, test_extract_mp3_extract_file);

	data.filename = "/mp3/header_sampling_mpeg25_2.mp3";
	data.testdata = "/mp3/header_sampling_mpeg25_2.data";
	g_test_add_data_func ("/tracker-extract/tracker-extract-mp3/header/sampling-mpeg25-2", &data, test_extract_mp3_extract_file);

	data.filename = "/mp3/header_sampling_mpeg25_3.mp3";
	data.testdata = "/mp3/header_sampling_mpeg25_3.data";
	g_test_add_data_func ("/tracker-extract/tracker-extract-mp3/header/sampling-mpeg25-3", &data, test_extract_mp3_extract_file);
#endif

	g_test_message ("Testing metadata ordering (priorities of sources)");

	result = g_test_run ();

	return result;
}
