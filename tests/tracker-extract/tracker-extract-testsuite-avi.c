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

#include "tracker-extract-testsuite-avi.h"
#include "tracker-extract-test-utils.h"

typedef struct {
	gchar *filename;
	gchar *testdata;
} ExtractData;

static const ExtractData data_basic_tags[] = {
	{ "/avi/basic_tags_1.avi", "/avi/basic_tags_1.data" },
	{ NULL, NULL }
};

void
test_tracker_extract_avi_basic_tags (gconstpointer data)
{
	guint i;

	for (i = 0; data_basic_tags[i].filename; i++) {
		tracker_test_extract_file (data,
		                           data_basic_tags[i].filename,
		                           data_basic_tags[i].testdata);
	}
}

