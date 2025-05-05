/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
 * Copyright (C) 2008-2010, Nokia <ivan.frade@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include <glib.h>

#include "tracker-date-time.h"

GQuark tracker_date_error_quark (void) {
	return g_quark_from_static_string ("tracker_date_error-quark");
}

GDateTime *
tracker_date_new_from_iso8601 (GTimeZone    *tz,
                               const gchar  *string,
                               GError      **error)
{
	GDateTime *datetime;

	datetime = g_date_time_new_from_iso8601 (string, tz);

	if (!datetime) {
		g_set_error (error,
			     TRACKER_DATE_ERROR,
			     TRACKER_DATE_ERROR_INVALID_ISO8601,
		             "'%s' is not a ISO 8601 date string. "
			     "Allowed form is CCYY-MM-DDThh:mm:ss[.ssssss][Z|(+|-)hh:mm]",
			     string);
	}

	return datetime;
}

gchar *
tracker_date_format_iso8601 (GDateTime *datetime)
{
	gboolean has_offset, has_subsecond;

	has_offset = g_date_time_get_utc_offset (datetime) != 0;
	has_subsecond = g_date_time_get_microsecond (datetime) != 0;

	if (has_offset && has_subsecond)
		return g_date_time_format (datetime, "%C%y-%m-%dT%H:%M:%S.%f%:z");
	else if (has_offset)
		return g_date_time_format (datetime, "%C%y-%m-%dT%T%:z");
	else if (has_subsecond)
		return g_date_time_format (datetime, "%C%y-%m-%dT%H:%M:%S.%fZ");
	else
		return g_date_time_format (datetime, "%C%y-%m-%dT%TZ");
}
