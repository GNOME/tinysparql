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

#include "tracker-extract-testsuite-tiff.h"
#include "tracker-extract-test-utils.h"

typedef struct {
	gchar *filename;
	gchar *testdata;
} ExtractData;

static const ExtractData data_basic_size[] = {
	{ "/tiff/basic_size_1.tif", "/tiff/basic_size_1.data" },
	{ "/tiff/basic_size_2.tif", "/tiff/basic_size_2.data" },
	{ "/tiff/basic_size_3.tif", "/tiff/basic_size_3.data" },
	{ "/tiff/basic_size_4.tif", "/tiff/basic_size_4.data" },
	{ "/tiff/basic_size_5.tif", "/tiff/basic_size_5.data" },
	{ NULL, NULL }
};

static const ExtractData data_exif_size[] = {
	{ "/tiff/exif_size_1.tif", "/tiff/exif_size_1.data" },
	{ NULL, NULL }
};

static const ExtractData data_exif_orientation[] = {
	{ "/tiff/exif_orientation_1.tif", "/tiff/exif_orientation_1.data" },
	{ "/tiff/exif_orientation_2.tif", "/tiff/exif_orientation_2.data" },
	{ "/tiff/exif_orientation_3.tif", "/tiff/exif_orientation_3.data" },
	{ "/tiff/exif_orientation_4.tif", "/tiff/exif_orientation_4.data" },
	{ "/tiff/exif_orientation_5.tif", "/tiff/exif_orientation_5.data" },
	{ "/tiff/exif_orientation_6.tif", "/tiff/exif_orientation_6.data" },
	{ "/tiff/exif_orientation_7.tif", "/tiff/exif_orientation_7.data" },
	{ "/tiff/exif_orientation_8.tif", "/tiff/exif_orientation_8.data" },
	{ NULL, NULL }
};

void
test_tracker_extract_tiff_basic_size (gconstpointer data)
{
	guint i;

	for (i = 0; data_basic_size[i].filename; i++) {
		tracker_test_extract_file (data,
		                           data_basic_size[i].filename,
		                           data_basic_size[i].testdata);
	}
}

void
test_tracker_extract_tiff_exif_size (gconstpointer data)
{
	guint i;

	for (i = 0; data_exif_size[i].filename; i++) {
		g_debug ("Filename: %s", data_basic_size[i].filename);

		tracker_test_extract_file (data,
		                           data_exif_size[i].filename,
		                           data_exif_size[i].testdata);
	}
}

void
test_tracker_extract_tiff_exif_orientation (gconstpointer data)
{
	guint i;

	for (i = 0; data_exif_orientation[i].filename; i++) {
		tracker_test_extract_file (data,
		                           data_exif_orientation[i].filename,
		                           data_exif_orientation[i].testdata);
	}
}

void
test_tracker_extract_tiff_performance (gconstpointer data)
{
	tracker_test_extract_file_performance (data, "/tiff/perf_tiff_%d.tif", 1000);
}
