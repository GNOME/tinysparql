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

#include "tracker-extract-testsuite-jpeg.h"
#include "tracker-extract-test-utils.h"

typedef struct {
	gchar *filename;
	gchar *testdata;
} ExtractData;

static const ExtractData data_basic_size[] = {
	{ "/jpeg/basic_size_1.jpg", "/jpeg/basic_size_1.data" },
	{ "/jpeg/basic_size_2.jpg", "/jpeg/basic_size_2.data" },
	{ "/jpeg/basic_size_3.jpg", "/jpeg/basic_size_3.data" },
	{ "/jpeg/basic_size_4.jpg", "/jpeg/basic_size_4.data" },
	{ "/jpeg/basic_size_5.jpg", "/jpeg/basic_size_5.data" },
	{ NULL, NULL }
};

static const ExtractData data_exif_size[] = {
	{ "/jpeg/exif_size_1.jpg", "/jpeg/exif_size_1.data" },
	{ NULL, NULL }
};

static const ExtractData data_exif_orientation[] = {
	{ "/jpeg/exif_orientation_1.jpg", "/jpeg/exif_orientation_1.data" },
	{ "/jpeg/exif_orientation_2.jpg", "/jpeg/exif_orientation_2.data" },
	{ "/jpeg/exif_orientation_3.jpg", "/jpeg/exif_orientation_3.data" },
	{ "/jpeg/exif_orientation_4.jpg", "/jpeg/exif_orientation_4.data" },
	{ "/jpeg/exif_orientation_5.jpg", "/jpeg/exif_orientation_5.data" },
	{ "/jpeg/exif_orientation_6.jpg", "/jpeg/exif_orientation_6.data" },
	{ "/jpeg/exif_orientation_7.jpg", "/jpeg/exif_orientation_7.data" },
	{ "/jpeg/exif_orientation_8.jpg", "/jpeg/exif_orientation_8.data" },
	{ NULL, NULL }
};

static const ExtractData data_exif_flash[] = {
	{ "/jpeg/exif_flash_1.jpg", "/jpeg/exif_flash_1.data" },
	{ "/jpeg/exif_flash_2.jpg", "/jpeg/exif_flash_2.data" },
	{ "/jpeg/exif_flash_3.jpg", "/jpeg/exif_flash_3.data" },
	{ "/jpeg/exif_flash_4.jpg", "/jpeg/exif_flash_4.data" },
	{ "/jpeg/exif_flash_5.jpg", "/jpeg/exif_flash_5.data" },
	{ "/jpeg/exif_flash_6.jpg", "/jpeg/exif_flash_6.data" },
	{ "/jpeg/exif_flash_7.jpg", "/jpeg/exif_flash_7.data" },
	{ "/jpeg/exif_flash_8.jpg", "/jpeg/exif_flash_8.data" },
	{ "/jpeg/exif_flash_9.jpg", "/jpeg/exif_flash_9.data" },
	{ "/jpeg/exif_flash_10.jpg", "/jpeg/exif_flash_10.data" },
	{ "/jpeg/exif_flash_11.jpg", "/jpeg/exif_flash_11.data" },
	{ "/jpeg/exif_flash_12.jpg", "/jpeg/exif_flash_12.data" },
	{ "/jpeg/exif_flash_13.jpg", "/jpeg/exif_flash_13.data" },
	{ "/jpeg/exif_flash_14.jpg", "/jpeg/exif_flash_14.data" },
	{ "/jpeg/exif_flash_15.jpg", "/jpeg/exif_flash_15.data" },
	{ "/jpeg/exif_flash_16.jpg", "/jpeg/exif_flash_16.data" },
	{ "/jpeg/exif_flash_17.jpg", "/jpeg/exif_flash_17.data" },
	{ "/jpeg/exif_flash_18.jpg", "/jpeg/exif_flash_18.data" },
	{ "/jpeg/exif_flash_19.jpg", "/jpeg/exif_flash_19.data" },
	{ "/jpeg/exif_flash_20.jpg", "/jpeg/exif_flash_20.data" },
	{ "/jpeg/exif_flash_21.jpg", "/jpeg/exif_flash_21.data" },
	{ "/jpeg/exif_flash_22.jpg", "/jpeg/exif_flash_22.data" },
	{ "/jpeg/exif_flash_23.jpg", "/jpeg/exif_flash_23.data" },
	{ "/jpeg/exif_flash_24.jpg", "/jpeg/exif_flash_24.data" },
	{ "/jpeg/exif_flash_25.jpg", "/jpeg/exif_flash_25.data" },
	{ "/jpeg/exif_flash_26.jpg", "/jpeg/exif_flash_26.data" },
	{ "/jpeg/exif_flash_27.jpg", "/jpeg/exif_flash_27.data" },
	{ NULL, NULL },
};

static const ExtractData data_exif_tags[] = {
	{ "/jpeg/exif_artist_1.jpg", "/jpeg/exif_artist_1.data" },
	{ "/jpeg/exif_flash_1.jpg", "/jpeg/exif_flash_1.data" },
	{ "/jpeg/exif_focal_1.jpg", "/jpeg/exif_focal_1.data" },
	{ "/jpeg/exif_fnumber_1.jpg", "/jpeg/exif_fnumber_1.data" },
	{ "/jpeg/exif_name_1.jpg", "/jpeg/exif_name_1.data" },
	{ "/jpeg/exif_white_1.jpg", "/jpeg/exif_white_1.data" },
	{ "/jpeg/exif_comment_1.jpg", "/jpeg/exif_comment_1.data" },
	{ "/jpeg/exif_description_1.jpg", "/jpeg/exif_description_1.data" },
	{ "/jpeg/exif_iso_1.jpg", "/jpeg/exif_iso_1.data" },
	{ "/jpeg/exif_software_1.jpg", "/jpeg/exif_software_1.data" },
	{ "/jpeg/exif_metering_1.jpg", "/jpeg/exif_metering_1.data" },
	{ NULL, NULL }
};

static const ExtractData data_xmp_dc[] = {
	{ "/jpeg/exif_metering_1.jpg", "/jpeg/exif_metering_1.data" },
	{ NULL, NULL }
};

void
test_tracker_extract_jpeg_basic_size (gconstpointer data)
{
	guint i;

	for (i = 0; data_basic_size[i].filename; i++) {
		tracker_test_extract_file (data,
		                           data_basic_size[i].filename,
		                           data_basic_size[i].testdata);
	}
}

void
test_tracker_extract_jpeg_exif_size (gconstpointer data)
{
	guint i;

	for (i = 0; data_exif_size[i].filename; i++) {
		tracker_test_extract_file (data,
		                           data_exif_size[i].filename,
		                           data_exif_size[i].testdata);
	}
}

void
test_tracker_extract_jpeg_exif_orientation (gconstpointer data)
{
	guint i;

	for (i = 0; data_exif_orientation[i].filename; i++) {
		tracker_test_extract_file (data,
		                           data_exif_orientation[i].filename,
		                           data_exif_orientation[i].testdata);
	}
}

void
test_tracker_extract_jpeg_exif_flash (gconstpointer data)
{
	guint i;

	for (i = 0; data_exif_flash[i].filename; i++) {
		tracker_test_extract_file (data,
		                           data_exif_flash[i].filename,
		                           data_exif_flash[i].testdata);
	}
}

void
test_tracker_extract_jpeg_exif_tags (gconstpointer data)
{
	guint i;

	for (i = 0; data_exif_tags[i].filename; i++) {
		tracker_test_extract_file (data,
		                           data_exif_tags[i].filename,
		                           data_exif_tags[i].testdata);
	}
}

void
test_tracker_extract_jpeg_performance (gconstpointer data)
{
	tracker_test_extract_file_performance (data, "/jpeg/perf_jpeg_%d.jpg", 1000);
}
