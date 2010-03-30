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

#ifndef __TRACKER_EXTRACT_TESTSUITE_MP3_H__
#define __TRACKER_EXTRACT_TESTSUITE_MP3_H__

#include <tracker-extract/tracker-extract.h>

void test_tracker_extract_mp3_access             (gconstpointer data);
void test_tracker_extract_mp3_id3v1_basic        (gconstpointer data);
void test_tracker_extract_mp3_id3v23_basic       (gconstpointer data);
void test_tracker_extract_mp3_id3v23_tcon        (gconstpointer data);
void test_tracker_extract_mp3_id3v23_tags        (gconstpointer data);
void test_tracker_extract_mp3_header_bitrate     (gconstpointer data);
void test_tracker_extract_mp3_header_bitrate_vbr (gconstpointer data);
void test_tracker_extract_mp3_header_sampling    (gconstpointer data);
void test_tracker_extract_mp3_performance        (gconstpointer data);

#endif
