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

#include "tracker-extract-testsuite-png.h"
#include "tracker-extract-test-utils.h"

typedef struct {
	gchar *filename;
	gchar *testdata;
} ExtractData;

static const ExtractData data_basic_size[] = {
	{ "/png/basic_size_1.png", "/png/basic_size_1.data" },
	{ "/png/basic_size_2.png", "/png/basic_size_2.data" },
	{ "/png/basic_size_3.png", "/png/basic_size_3.data" },
	{ "/png/basic_size_4.png", "/png/basic_size_4.data" },
	{ "/png/basic_size_5.png", "/png/basic_size_5.data" },
	{ NULL, NULL }
};

static const ExtractData data_xmp_exif_orientation[] = {
	{ "/png/xmp_exif_orientation_1.png", "/png/xmp_exif_orientation_1.data" },
	{ "/png/xmp_exif_orientation_2.png", "/png/xmp_exif_orientation_2.data" },
	{ "/png/xmp_exif_orientation_3.png", "/png/xmp_exif_orientation_3.data" },
	{ "/png/xmp_exif_orientation_4.png", "/png/xmp_exif_orientation_4.data" },
	{ "/png/xmp_exif_orientation_5.png", "/png/xmp_exif_orientation_5.data" },
	{ "/png/xmp_exif_orientation_6.png", "/png/xmp_exif_orientation_6.data" },
	{ "/png/xmp_exif_orientation_7.png", "/png/xmp_exif_orientation_7.data" },
	{ "/png/xmp_exif_orientation_8.png", "/png/xmp_exif_orientation_8.data" },
	{ NULL, NULL }
};

void
test_tracker_extract_png_basic_size (gconstpointer data)
{
	guint i;

	for (i = 0; data_basic_size[i].filename; i++) {
		tracker_test_extract_file (data,
		                           data_basic_size[i].filename,
		                           data_basic_size[i].testdata);
	}
}

void
test_tracker_extract_png_xmp_exif_orientation (gconstpointer data)
{
	guint i;

	for (i = 0; data_xmp_exif_orientation[i].filename; i++) {
		tracker_test_extract_file (data,
		                           data_xmp_exif_orientation[i].filename,
		                           data_xmp_exif_orientation[i].testdata);
	}
}

void
test_tracker_extract_png_performance (gconstpointer data)
{
	tracker_test_extract_file_performance (data, "/png/perf_png_%d.png", 1000);
}
