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

#include "tracker-extract-testsuite-mp3.h"

#include <glib.h>

#include "tracker-extract-test-utils.h"

typedef struct {
	gchar *filename;
	gchar *testdata;
} ExtractData;

static const ExtractData data_id3v1_basic[] = { 
	{ "/mp3/id3v1_basic_1.mp3", "/mp3/id3v1_basic_1.data" },
	{ "/mp3/id3v1_basic_2.mp3", "/mp3/id3v1_basic_2.data" },
	{ "/mp3/id3v1_basic_3.mp3", "/mp3/id3v1_basic_3.data" },
	{ NULL, NULL }
};

static const ExtractData data_id3v23_basic[] = { 
	{ "/mp3/id3v23_basic_1.mp3", "/mp3/id3v23_basic_1.data" },
	{ "/mp3/id3v23_basic_2.mp3", "/mp3/id3v23_basic_2.data" },
	{ "/mp3/id3v23_basic_3.mp3", "/mp3/id3v23_basic_3.data" },
	{ NULL, NULL }
};

static const ExtractData data_id3v23_tags[] = { 
	{ "/mp3/id3v23_trck_1.mp3", "/mp3/id3v23_trck_1.data" },
	{ "/mp3/id3v23_comm_1.mp3", "/mp3/id3v23_comm_1.data" },
	{ "/mp3/id3v23_comm_2.mp3", "/mp3/id3v23_comm_2.data" },
	{ "/mp3/id3v23_comm_3.mp3", "/mp3/id3v23_comm_3.data" },
	{ "/mp3/id3v23_tags_1.mp3", "/mp3/id3v23_tags_1.data" },
	{ "/mp3/id3v23_tags_2.mp3", "/mp3/id3v23_tags_2.data" },
	{ NULL, NULL }
};

static const ExtractData data_header_bitrate[] = { 
	{ "/mp3/header_bitrate_mpeg1_1.mp3", "/mp3/header_bitrate_mpeg1_1.data" },
	{ "/mp3/header_bitrate_mpeg1_2.mp3", "/mp3/header_bitrate_mpeg1_2.data" },
	{ "/mp3/header_bitrate_mpeg1_3.mp3", "/mp3/header_bitrate_mpeg1_3.data" },
	{ "/mp3/header_bitrate_mpeg1_4.mp3", "/mp3/header_bitrate_mpeg1_4.data" },
	{ "/mp3/header_bitrate_mpeg1_5.mp3", "/mp3/header_bitrate_mpeg1_5.data" },
	{ "/mp3/header_bitrate_mpeg1_6.mp3", "/mp3/header_bitrate_mpeg1_6.data" },
	{ "/mp3/header_bitrate_mpeg1_7.mp3", "/mp3/header_bitrate_mpeg1_7.data" },
	{ "/mp3/header_bitrate_mpeg1_8.mp3", "/mp3/header_bitrate_mpeg1_8.data" },
	{ "/mp3/header_bitrate_mpeg1_9.mp3", "/mp3/header_bitrate_mpeg1_9.data" },
	{ "/mp3/header_bitrate_mpeg1_10.mp3","/mp3/header_bitrate_mpeg1_10.data"},
	{ "/mp3/header_bitrate_mpeg1_11.mp3","/mp3/header_bitrate_mpeg1_11.data"},
	{ "/mp3/header_bitrate_mpeg1_12.mp3","/mp3/header_bitrate_mpeg1_12.data"},
	{ "/mp3/header_bitrate_mpeg1_13.mp3","/mp3/header_bitrate_mpeg1_13.data"},
	{ "/mp3/header_bitrate_mpeg1_14.mp3","/mp3/header_bitrate_mpeg1_14.data"},
	{ NULL, NULL }
};

static const ExtractData data_header_bitrate_vbr[] = { 
	{ "/mp3/header_bitrate_vbr_mpeg1_1.mp3", "/mp3/header_bitrate_vbr_mpeg1_1.data" },
	{ "/mp3/header_bitrate_vbr_mpeg1_1.mp3", "/mp3/header_bitrate_vbr_mpeg1_1.data" },
	{ "/mp3/header_bitrate_vbr_mpeg1_1.mp3", "/mp3/header_bitrate_vbr_mpeg1_1.data" },
	{ NULL, NULL }
};


static const ExtractData data_header_sampling[] = { 
	{ "/mp3/header_sampling_mpeg1_1.mp3", "/mp3/header_sampling_mpeg1_1.data" },
	{ "/mp3/header_sampling_mpeg1_2.mp3", "/mp3/header_sampling_mpeg1_2.data" },
	{ "/mp3/header_sampling_mpeg1_3.mp3", "/mp3/header_sampling_mpeg1_3.data" },
	{ "/mp3/header_sampling_mpeg2_1.mp3", "/mp3/header_sampling_mpeg2_1.data" },
	{ "/mp3/header_sampling_mpeg2_2.mp3", "/mp3/header_sampling_mpeg2_2.data" },
	{ "/mp3/header_sampling_mpeg2_3.mp3", "/mp3/header_sampling_mpeg2_3.data" },
	{ "/mp3/header_sampling_mpeg25_1.mp3", "/mp3/header_sampling_mpeg25_1.data" },
	{ "/mp3/header_sampling_mpeg25_2.mp3", "/mp3/header_sampling_mpeg25_2.data" },
	{ "/mp3/header_sampling_mpeg25_3.mp3", "/mp3/header_sampling_mpeg25_3.data" },
	{ NULL, NULL }
};

void
extract_file (const TrackerExtractorData *data, const gchar *file, const gchar *testdatafile)
{
	GHashTable *metadata;
	GHashTable *testdata;

	gchar      *filename;	
	gchar      *testdata_filename;

	GHashTableIter iter;
	gpointer key, value;

	g_assert (data != NULL);
	g_assert (file != NULL);
	g_assert (testdatafile != NULL);

	filename = g_strconcat (TEST_DATA_DIR, file, NULL);
	testdata_filename = g_strconcat (TEST_DATA_DIR, testdatafile, NULL);

	metadata = g_hash_table_new_full (g_str_hash,
					  g_str_equal,
					  g_free,
					  g_free);

	(*data->extractor) (filename, metadata);

	testdata = parse_testdata_file (testdata_filename);

	g_hash_table_iter_init (&iter, testdata);

/*	dump_metadata(metadata); */

	while (g_hash_table_iter_next (&iter, &key, &value)) {
		check_metadata (metadata, (gchar *)key, (gchar *)value);
	}

	g_hash_table_destroy (metadata);
	g_hash_table_destroy (testdata);
}

void
performance_extract_files (const TrackerExtractorData *data, const gchar *filematch, guint filecount)
{
	double perftime;
	guint i;

	g_assert (data != NULL);
	g_assert (filematch != NULL);
	g_assert (filecount >0 );
	
	g_test_timer_start();

	for (i=1;i<=filecount;i++) {		
		char filename[256];
		GHashTable *metadata;

		metadata = g_hash_table_new_full (g_str_hash,
						  g_str_equal,
						  g_free,
						  g_free);

		if (sprintf (filename, "%s%s%d.mp3",TEST_DATA_DIR,filematch,i) < 0) {
			g_assert_not_reached();
		}

		(*data->extractor) (filename, metadata);

		g_assert (g_hash_table_size (metadata) > 0);

		g_hash_table_destroy (metadata);
	}		

	perftime = g_test_timer_elapsed();

	g_debug ("Time was: %f", perftime);

	g_test_minimized_result (perftime, "Time of the mp3 performance tests");
}


void test_tracker_extract_mp3_id3v1_basic(gconstpointer data)
{
	const TrackerExtractorData *extractor = data;
	guint i;

	for (i=0; data_id3v1_basic[i].filename; i++) {
		extract_file (data,
			      data_id3v1_basic[i].filename,
			      data_id3v1_basic[i].testdata);		
	}
}

void test_tracker_extract_mp3_id3v23_basic(gconstpointer data)
{
	const TrackerExtractorData *extractor = data;
	guint i;

	for (i=0; data_id3v23_basic[i].filename; i++) {
		extract_file (data,
			      data_id3v23_basic[i].filename,
			      data_id3v23_basic[i].testdata);		
	}
}

void test_tracker_extract_mp3_id3v23_tags(gconstpointer data)
{
	const TrackerExtractorData *extractor = data;
	guint i;

	for (i=0; data_id3v23_tags[i].filename; i++) {
		extract_file (data,
			      data_id3v23_tags[i].filename,
			      data_id3v23_tags[i].testdata);		
	}
}

void test_tracker_extract_mp3_header_bitrate(gconstpointer data)
{
	const TrackerExtractorData *extractor = data;
	guint i;

	for (i=0; data_header_bitrate[i].filename; i++) {
		extract_file (data,
			      data_header_bitrate[i].filename,
			      data_header_bitrate[i].testdata);		
	}
}

void test_tracker_extract_mp3_header_bitrate_vbr(gconstpointer data)
{
	const TrackerExtractorData *extractor = data;
	guint i;

	for (i=0; data_header_bitrate_vbr[i].filename; i++) {
		extract_file (data,
			      data_header_bitrate_vbr[i].filename,
			      data_header_bitrate_vbr[i].testdata);		
	}
}

void test_tracker_extract_mp3_header_sampling(gconstpointer data)
{
	const TrackerExtractorData *extractor = data;
	guint i;

	for (i=0; data_header_sampling[i].filename; i++) {
		extract_file (data,
			      data_header_sampling[i].filename,
			      data_header_sampling[i].testdata);		
	}
}



void performance_tracker_extract_mp3(gconstpointer data)
{
	const TrackerExtractorData *extractor = data;
	
	performance_extract_files (data, "/mp3/perf_cbr_id3v1_", 1000);
}

