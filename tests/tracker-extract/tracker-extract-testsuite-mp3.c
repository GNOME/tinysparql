/*
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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

#include <glib.h>

#include "tracker-extract-testsuite-mp3.h"
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

static const ExtractData data_id3v23_tcon[] = {
	{ "/mp3/id3v23_tcon_1.mp3", "/mp3/id3v23_tcon_1.data" },
	{ "/mp3/id3v23_tcon_2.mp3", "/mp3/id3v23_tcon_2.data" },
	{ "/mp3/id3v23_tcon_3.mp3", "/mp3/id3v23_tcon_3.data" },
	{ "/mp3/id3v23_tcon_4.mp3", "/mp3/id3v23_tcon_4.data" },
	{ "/mp3/id3v23_tcon_5.mp3", "/mp3/id3v23_tcon_5.data" },
	{ "/mp3/id3v23_tcon_6.mp3", "/mp3/id3v23_tcon_6.data" },
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
test_tracker_extract_mp3_access (gconstpointer data)
{
	tracker_test_extract_file_access (data, "/mp3/access_%d.mp3", 4);
}

void
test_tracker_extract_mp3_id3v1_basic (gconstpointer data)
{
	guint i;

	for (i = 0; data_id3v1_basic[i].filename; i++) {
		tracker_test_extract_file (data,
		                           data_id3v1_basic[i].filename,
		                           data_id3v1_basic[i].testdata);
	}
}

void
test_tracker_extract_mp3_id3v23_basic (gconstpointer data)
{
	guint i;

	for (i = 0; data_id3v23_basic[i].filename; i++) {
		tracker_test_extract_file (data,
		                           data_id3v23_basic[i].filename,
		                           data_id3v23_basic[i].testdata);
	}
}

void
test_tracker_extract_mp3_id3v23_tags (gconstpointer data)
{
	guint i;

	for (i = 0; data_id3v23_tags[i].filename; i++) {
		tracker_test_extract_file (data,
		                           data_id3v23_tags[i].filename,
		                           data_id3v23_tags[i].testdata);
	}
}

void
test_tracker_extract_mp3_id3v23_tcon(gconstpointer data)
{
	guint i;

	for (i = 0; data_id3v23_tcon[i].filename; i++) {
		tracker_test_extract_file (data,
		                           data_id3v23_tcon[i].filename,
		                           data_id3v23_tcon[i].testdata);
	}
}

void
test_tracker_extract_mp3_header_bitrate (gconstpointer data)
{
	guint i;

	for (i = 0; data_header_bitrate[i].filename; i++) {
		tracker_test_extract_file (data,
		                           data_header_bitrate[i].filename,
		                           data_header_bitrate[i].testdata);
	}
}

void
test_tracker_extract_mp3_header_bitrate_vbr (gconstpointer data)
{
	guint i;

	for (i = 0; data_header_bitrate_vbr[i].filename; i++) {
		tracker_test_extract_file (data,
		                           data_header_bitrate_vbr[i].filename,
		                           data_header_bitrate_vbr[i].testdata);
	}
}

void
test_tracker_extract_mp3_header_sampling (gconstpointer data)
{
	guint i;

	for (i = 0; data_header_sampling[i].filename; i++) {
		tracker_test_extract_file (data,
		                           data_header_sampling[i].filename,
		                           data_header_sampling[i].testdata);
	}
}

void
test_tracker_extract_mp3_performance (gconstpointer data)
{
	tracker_test_extract_file_performance (data, "/mp3/perf_cbr_id3v1_%d.mp3", 1000);
}

