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

#include "tracker-extract-test-utils.h"
#include "tracker-extract-testsuite-generic.h"

void
test_tracker_extract_check_extract_data (gconstpointer extract)
{
	const TrackerExtractData *data = extract;
	guint extractors = 0;

	while (data->mime) {
		if (data->extract == NULL) {
			g_error ("Extractor for mime '%s' declared NULL", data->mime);
		}

		extractors++;
		data++;
	}

	g_assert (extractors > 0);
}
