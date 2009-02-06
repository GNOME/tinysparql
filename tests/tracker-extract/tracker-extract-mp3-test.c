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
#include "tracker-extract-testsuite-mp3.h"

int
main (int argc, char **argv) {

	TrackerExtractData *data;
	gint result;

	g_type_init ();
	g_thread_init (NULL);
	g_test_init (&argc, &argv, NULL);

	g_test_message ("Testing extract functionality");
	g_test_add_func ("/tracker-extract/tracker-extract-mp3/check-extract-data",
			 test_tracker_extract_check_extract_data);

	data = tracker_test_extract_get_extract ("audio/mpeg");

	g_test_add_data_func ("/tracker-extract/tracker-extract-mp3/access",
			      data, access_tracker_extract_mp3);

#if 0

	g_test_add_data_func ("/tracker-extract/tracker-extract-mp3/id3v1_basic",
			      data, test_tracker_extract_mp3_id3v1_basic);
	g_test_add_data_func ("/tracker-extract/tracker-extract-mp3/id3v23_basic",
			      data, test_tracker_extract_mp3_id3v23_basic);
	g_test_add_data_func ("/tracker-extract/tracker-extract-mp3/id3v23_tcon",
			      data, test_tracker_extract_mp3_id3v23_tcon);
	g_test_add_data_func ("/tracker-extract/tracker-extract-mp3/id3v23_tags",
			      data, test_tracker_extract_mp3_id3v23_tags);
	g_test_add_data_func ("/tracker-extract/tracker-extract-mp3/header_bitrate",
			      data, test_tracker_extract_mp3_header_bitrate);
	g_test_add_data_func ("/tracker-extract/tracker-extract-mp3/header_sampling",
			      data, test_tracker_extract_mp3_header_sampling);


	if (g_test_perf()) {
		g_test_add_data_func ("/tracker-extract/tracker-extract-mp3/performance_cbr",
				      data, performance_tracker_extract_mp3);	
	}

#endif

	result = g_test_run ();

	return result;
}
