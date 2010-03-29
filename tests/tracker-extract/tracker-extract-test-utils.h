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

#ifndef __TRACKER_EXTRACT_TEST_UTILS_H__
#define __TRACKER_EXTRACT_TEST_UTILS_H__

#include <string.h>
#include <fcntl.h>
#include <stdio.h>

#include <glib.h>

#include <tracker-extract/tracker-main.h>

TrackerExtractData *tracker_test_extract_get_extract      (const gchar              *path,
                                                           const gchar              *mime);
void                tracker_test_extract_file             (const TrackerExtractData *data,
                                                           const gchar              *file,
                                                           const gchar              *testdatafile);
void                tracker_test_extract_file_performance (const TrackerExtractData *data,
                                                           const gchar              *file_match,
                                                           guint                     file_count);
void                tracker_test_extract_file_access      (const TrackerExtractData *data,
                                                           const gchar              *file_match,
                                                           guint                     file_count);

#endif
