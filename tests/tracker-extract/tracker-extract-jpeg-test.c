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
#include <stdio.h>
#include <glib.h>

#include <tracker-extract/tracker-extract.h>

#include "tracker-extract-test-utils.h"
#include "tracker-extract-testsuite-generic.h"
#include "tracker-extract-testsuite-jpeg.h"

int
main (int argc, char **argv) {

	TrackerExtractData *data;
	gint result;

	g_type_init ();
	g_thread_init (NULL);
	g_test_init (&argc, &argv, NULL);
 
	g_test_message ("Testing extract functionality");
	g_test_add_func ("/tracker-extract/tracker-extract-jpeg/check-extract-data",
			 test_tracker_extract_check_extract_data);

#if 0
	data = tracker_test_extract_get_extract ("image/jpeg");

	g_test_add_data_func ("/tracker-extract/tracker-extract-jpeg/basic_size",
			      data, test_tracker_extract_jpeg_basic_size);

	g_test_add_data_func ("/tracker-extract/tracker-extract-jpeg/exif_size",
			      data, test_tracker_extract_jpeg_exif_size);

	g_test_add_data_func ("/tracker-extract/tracker-extract-jpeg/exif_orientation",
			      data, test_tracker_extract_jpeg_exif_orientation);

	g_test_add_data_func ("/tracker-extract/tracker-extract-jpeg/exif_flash",
			      data, test_tracker_extract_jpeg_exif_flash);

	g_test_add_data_func ("/tracker-extract/tracker-extract-jpeg/exif_tags",
			      data, test_tracker_extract_jpeg_exif_tags);

	if (g_test_perf()) {
		g_test_add_data_func ("/tracker-extract/tracker-extract-jpeg/performance",
				      data, performance_tracker_extract_jpeg);
	}

#endif


	result = g_test_run ();

	return result;
}
