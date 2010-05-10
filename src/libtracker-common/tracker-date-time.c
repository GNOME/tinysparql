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

/* For timegm usage on __GLIBC__ */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdlib.h>

#include <glib.h>

#include "tracker-date-time.h"
#include "tracker-type-utils.h"

GQuark tracker_date_error_quark (void) {
	return g_quark_from_static_string ("tracker_date_error-quark");
}

time_t
tracker_string_to_date (const gchar *date_string,
                        gint        *offset_p,
                        GError      **error)
{
	/* TODO Add more checks and use GError to report invalid input
	 * as this is potential user input.
	 */

	static GRegex *regex = NULL;

	GMatchInfo *match_info;
	gchar      *match;
	struct tm tm;
	time_t    t;
	gint offset;

	g_return_val_if_fail (date_string, -1);

	/* We should have a valid iso 8601 date in format
	 * YYYY-MM-DDThh:mm:ss with optional TZ
	 */

	if (!regex) {
		GError *e = NULL;
		regex = g_regex_new ("^(-?[0-9][0-9][0-9][0-9])-([0-9][0-9])-([0-9][0-9])T([0-9][0-9]):([0-9][0-9]):([0-9][0-9])(\\.[0-9]+)?(Z|(\\+|-)([0-9][0-9]):?([0-9][0-9]))?$", 0, 0, &e);
		if (e) {
			g_error ("%s", e->message);
		}
	}

	if (!g_regex_match (regex, date_string, 0, &match_info)) {
		g_match_info_free (match_info);
		g_set_error (error, TRACKER_DATE_ERROR, TRACKER_DATE_ERROR_INVALID_ISO8601,
		             "Not a ISO 8601 date string. Allowed form is [-]CCYY-MM-DDThh:mm:ss[Z|(+|-)hh:mm]");
		return -1;
	}

	memset (&tm, 0, sizeof (struct tm));

	/* year */
	match = g_match_info_fetch (match_info, 1);
	tm.tm_year = atoi (match) - 1900;
	g_free (match);

	/* month */
	match = g_match_info_fetch (match_info, 2);
	tm.tm_mon = atoi (match) - 1;
	g_free (match);

	/* day of month */
	match = g_match_info_fetch (match_info, 3);
	tm.tm_mday = atoi (match);
	g_free (match);

	/* hour */
	match = g_match_info_fetch (match_info, 4);
	tm.tm_hour = atoi (match);
	g_free (match);

	/* minute */
	match = g_match_info_fetch (match_info, 5);
	tm.tm_min = atoi (match);
	g_free (match);

	/* second */
	match = g_match_info_fetch (match_info, 6);
	tm.tm_sec = atoi (match);
	g_free (match);

	match = g_match_info_fetch (match_info, 8);
	if (match) {
		/* timezoned */
		g_free (match);

		/* mktime() always assumes that "tm" is in locale time but we
		 * want to keep control on time, so we go to UTC
		 */
#if !(defined(__FreeBSD__) || defined(__OpenBSD__) || defined (__GLIBC__))
		t  = mktime (&tm);
		t -= timezone;
#else
		t = timegm (&tm);
#endif

		offset = 0;

		match = g_match_info_fetch (match_info, 9);
		if (match) {
			/* non-UTC timezone */

			gboolean positive_offset;

			positive_offset = (match[0] == '+');
			g_free (match);

			match = g_match_info_fetch (match_info, 10);
			offset = atoi (match) * 3600;
			g_free (match);

			match = g_match_info_fetch (match_info, 11);
			offset += atoi (match) * 60;
			g_free (match);

			if (!positive_offset) {
				offset = -offset;
			}

			if (offset < -14 * 3600 || offset > 14 * 3600) {
				g_set_error (error, TRACKER_DATE_ERROR, TRACKER_DATE_ERROR_OFFSET,
				             "UTC offset too large: %d seconds", offset);
				g_match_info_free (match_info);
				return -1;
			}

			t -= offset;
		}
	} else {
		/* local time */
		tm.tm_isdst = -1;

		t = mktime (&tm);

		offset = -timezone + (tm.tm_isdst > 0 ? 3600 : 0);
	}

	g_match_info_free (match_info);

	if (offset_p) {
		*offset_p = offset;
	}

	return t;
}

gchar *
tracker_date_to_string (time_t date_time)
{
	gchar     buffer[30];
	struct tm utc_time;
	size_t    count;

	memset (buffer, '\0', sizeof (buffer));
	memset (&utc_time, 0, sizeof (struct tm));

	gmtime_r (&date_time, &utc_time);

	/* Output is ISO 8160 format : "YYYY-MM-DDThh:mm:ssZ" */
	count = strftime (buffer, sizeof (buffer), "%FT%TZ", &utc_time);

	return count > 0 ? g_strdup (buffer) : NULL;
}

static void
date_time_value_init (GValue *value)
{
	value->data[0].v_int64 = 0;
	value->data[1].v_int = 0;
}

static void
date_time_value_copy (const GValue *src_value,
                      GValue       *dest_value)
{
	dest_value->data[0].v_int64 = src_value->data[0].v_int64;
	dest_value->data[1].v_int = src_value->data[1].v_int;
}

GType
tracker_date_time_get_type (void)
{
	static GType tracker_date_time_type_id = 0;
	if (G_UNLIKELY (tracker_date_time_type_id == 0)) {
		static const GTypeValueTable value_table = {
			date_time_value_init,
			NULL,
			date_time_value_copy
		};
		static const GTypeInfo type_info = {
			0,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			0,
			0,
			NULL,
			&value_table
		};
		static const GTypeFundamentalInfo fundamental_info = {
			0
		};
		tracker_date_time_type_id = g_type_register_fundamental (
			g_type_fundamental_next (),
			"TrackerDateTime",
			&type_info,
			&fundamental_info,
			0);
	}
	return tracker_date_time_type_id;
}

void
tracker_date_time_set (GValue  *value,
                       gint64   time,
                       gint     offset)
{
	g_return_if_fail (G_VALUE_HOLDS (value, TRACKER_TYPE_DATE_TIME));
	g_return_if_fail (offset >= -14 * 3600 && offset <= 14 * 3600);

	value->data[0].v_int64 = time;
	value->data[1].v_int = offset;
}

void
tracker_date_time_set_from_string (GValue      *value,
                                   const gchar *date_time_string,
                                   GError     **error)
{
	gint64 time;
	gint offset;
	GError *new_error = NULL;

	g_return_if_fail (G_VALUE_HOLDS (value, TRACKER_TYPE_DATE_TIME));
	g_return_if_fail (date_time_string != NULL);

	time = tracker_string_to_date (date_time_string, &offset, &new_error);

	if (new_error != NULL) {
		g_propagate_error (error, new_error);
		return;
	}

	tracker_date_time_set (value, time, offset);
}

gint64
tracker_date_time_get_time (const GValue *value)
{
	g_return_val_if_fail (G_VALUE_HOLDS (value, TRACKER_TYPE_DATE_TIME), 0);

	/* UTC timestamp */
	return value->data[0].v_int64;
}

gint
tracker_date_time_get_offset (const GValue *value)
{
	g_return_val_if_fail (G_VALUE_HOLDS (value, TRACKER_TYPE_DATE_TIME), 0);

	/* UTC offset */
	return value->data[1].v_int;
}

gint
tracker_date_time_get_local_date (const GValue *value)
{
	gint64 local_timestamp;

	g_return_val_if_fail (G_VALUE_HOLDS (value, TRACKER_TYPE_DATE_TIME), 0);

	/* return number of days since epoch */
	local_timestamp = tracker_date_time_get_time (value) + tracker_date_time_get_offset (value);
	return local_timestamp / 3600 / 24;
}

gint
tracker_date_time_get_local_time (const GValue *value)
{
	gint64 local_timestamp;

	g_return_val_if_fail (G_VALUE_HOLDS (value, TRACKER_TYPE_DATE_TIME), 0);

	/* return local time of day */
	local_timestamp = tracker_date_time_get_time (value) + tracker_date_time_get_offset (value);
	return local_timestamp % (24 * 3600);
}
