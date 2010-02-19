/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008-2010, Nokia (urho.konttori@nokia.com)
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

#define _XOPEN_SOURCE
#include <time.h>

#include <strings.h>
#include <string.h>
#include <stdlib.h>

#include <glib.h>

#include "tracker-date-time.h"
#include "tracker-type-utils.h"

#define DATE_FORMAT_ISO8601 "%Y-%m-%dT%H:%M:%S%z"

static const char *months[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static const char imonths[] = {
	'1', '2', '3', '4', '5',
	'6', '7', '8', '9', '0', '1', '2'
};

static gboolean
is_int (const gchar *str)
{
	gint     i, len;

	if (!str || str[0] == '\0') {
		return FALSE;
	}

	len = strlen (str);

	for (i = 0; i < len; i++) {
		if (!g_ascii_isdigit(str[i])) {
			return FALSE;
		}
	}

	return TRUE ;
}

static gint
parse_month (const gchar *month)
{
	gint i;

	for (i = 0; i < 12; i++) {
		if (!strncmp (month, months[i], 3)) {
			return i;
		}
	}

	return -1;
}

/* Determine date format and convert to ISO 8601 format */
/* FIXME We should handle all the fractions here (see ISO 8601), as well as YYYY:DDD etc */
gchar *
tracker_date_format (const gchar *date_string)
{
	gchar buf[30];
	gint  len;

	if (!date_string) {
		return NULL;
	}

	len = strlen (date_string);

	/* We cannot format a date without at least a four digit
	 * year.
	 */
	if (len < 4) {
		return NULL;
	}

	/* Check for year only dates (EG ID3 music tags might have
	 * Audio.ReleaseDate as 4 digit year)
	 */
	if (len == 4) {
		if (is_int (date_string)) {
			buf[0] = date_string[0];
			buf[1] = date_string[1];
			buf[2] = date_string[2];
			buf[3] = date_string[3];
			buf[4] = '-';
			buf[5] = '0';
			buf[6] = '1';
			buf[7] = '-';
			buf[8] = '0';
			buf[9] = '1';
			buf[10] = 'T';
			buf[11] = '0';
			buf[12] = '0';
			buf[13] = ':';
			buf[14] = '0';
			buf[15] = '0';
			buf[16] = ':';
			buf[17] = '0';
			buf[18] = '0';
			buf[19] = 'Z';
			buf[20] = '\0';

			return g_strdup (buf);
		} else {
			return NULL;
		}
	} else if (len == 10)  {
		/* Check for date part only YYYY-MM-DD*/
		buf[0] = date_string[0];
		buf[1] = date_string[1];
		buf[2] = date_string[2];
		buf[3] = date_string[3];
		buf[4] = '-';
		buf[5] = date_string[5];
		buf[6] = date_string[6];
		buf[7] = '-';
		buf[8] = date_string[8];
		buf[9] = date_string[9];
		buf[10] = 'T';
		buf[11] = '0';
		buf[12] = '0';
		buf[13] = ':';
		buf[14] = '0';
		buf[15] = '0';
		buf[16] = ':';
		buf[17] = '0';
		buf[18] = '0';
		buf[19] = '\0';

		return g_strdup (buf);
	} else if (len == 14) {
		/* Check for pdf format EG 20050315113224-08'00' or
		 * 20050216111533Z
		 */
		buf[0] = date_string[0];
		buf[1] = date_string[1];
		buf[2] = date_string[2];
		buf[3] = date_string[3];
		buf[4] = '-';
		buf[5] = date_string[4];
		buf[6] = date_string[5];
		buf[7] = '-';
		buf[8] = date_string[6];
		buf[9] = date_string[7];
		buf[10] = 'T';
		buf[11] = date_string[8];
		buf[12] = date_string[9];
		buf[13] = ':';
		buf[14] = date_string[10];
		buf[15] = date_string[11];
		buf[16] = ':';
		buf[17] = date_string[12];
		buf[18] = date_string[13];
		buf[19] = '\0';

		return g_strdup (buf);
	} else if (len == 15 && date_string[14] == 'Z') {
		buf[0] = date_string[0];
		buf[1] = date_string[1];
		buf[2] = date_string[2];
		buf[3] = date_string[3];
		buf[4] = '-';
		buf[5] = date_string[4];
		buf[6] = date_string[5];
		buf[7] = '-';
		buf[8] = date_string[6];
		buf[9] = date_string[7];
		buf[10] = 'T';
		buf[11] = date_string[8];
		buf[12] = date_string[9];
		buf[13] = ':';
		buf[14] = date_string[10];
		buf[15] = date_string[11];
		buf[16] = ':';
		buf[17] = date_string[12];
		buf[18] = date_string[13];
		buf[19] = 'Z';
		buf[20] = '\0';

		return g_strdup (buf);
	} else if (len == 21 && (date_string[14] == '-' || date_string[14] == '+' )) {
		buf[0] = date_string[0];
		buf[1] = date_string[1];
		buf[2] = date_string[2];
		buf[3] = date_string[3];
		buf[4] = '-';
		buf[5] = date_string[4];
		buf[6] = date_string[5];
		buf[7] = '-';
		buf[8] = date_string[6];
		buf[9] = date_string[7];
		buf[10] = 'T';
		buf[11] = date_string[8];
		buf[12] = date_string[9];
		buf[13] = ':';
		buf[14] = date_string[10];
		buf[15] = date_string[11];
		buf[16] = ':';
		buf[17] = date_string[12];
		buf[18] = date_string[13];
		buf[19] = date_string[14];
		buf[20] = date_string[15];
		buf[21] = date_string[16];
		buf[22] =  ':';
		buf[23] = date_string[18];
		buf[24] = date_string[19];
		buf[25] = '\0';

		return g_strdup (buf);
	} else if ((len == 24) && (date_string[3] == ' ')) {
		/* Check for msoffice date format "Mon Feb  9 10:10:00 2004" */
		gint  num_month;
		gchar mon1;
		gchar day1;

		num_month = parse_month (date_string + 4);

		mon1 = imonths[num_month];

		if (date_string[8] == ' ') {
			day1 = '0';
		} else {
			day1 = date_string[8];
		}

		buf[0] = date_string[20];
		buf[1] = date_string[21];
		buf[2] = date_string[22];
		buf[3] = date_string[23];
		buf[4] = '-';

		if (num_month < 10) {
			buf[5] = '0';
			buf[6] = mon1;
		} else {
			buf[5] = '1';
			buf[6] = mon1;
		}

		buf[7] = '-';
		buf[8] = day1;
		buf[9] = date_string[9];
		buf[10] = 'T';
		buf[11] = date_string[11];
		buf[12] = date_string[12];
		buf[13] = ':';
		buf[14] = date_string[14];
		buf[15] = date_string[15];
		buf[16] = ':';
		buf[17] = date_string[17];
		buf[18] = date_string[18];
		buf[19] = '\0';

		return g_strdup (buf);
	} else if ((len == 19) && (date_string[4] == ':') && (date_string[7] == ':')) {
		/* Check for Exif date format "2005:04:29 14:56:54" */
		buf[0] = date_string[0];
		buf[1] = date_string[1];
		buf[2] = date_string[2];
		buf[3] = date_string[3];
		buf[4] = '-';
		buf[5] = date_string[5];
		buf[6] = date_string[6];
		buf[7] = '-';
		buf[8] = date_string[8];
		buf[9] = date_string[9];
		buf[10] = 'T';
		buf[11] = date_string[11];
		buf[12] = date_string[12];
		buf[13] = ':';
		buf[14] = date_string[14];
		buf[15] = date_string[15];
		buf[16] = ':';
		buf[17] = date_string[17];
		buf[18] = date_string[18];
		buf[19] = '\0';

		return g_strdup (buf);
	} else if ((len == 28) && (date_string[4] == '-') && (date_string[10] == 'T')
	           && (date_string[19] == '.') ) {
		/* The fraction of seconds ISO 8601 "YYYY-MM-DDThh:mm:ss.ff+zz:zz" */
		buf[0] = date_string[0];
		buf[1] = date_string[1];
		buf[2] = date_string[2];
		buf[3] = date_string[3];
		buf[4] = '-';
		buf[5] = date_string[5];
		buf[6] = date_string[6];
		buf[7] = '-';
		buf[8] = date_string[8];
		buf[9] = date_string[9];
		buf[10] = 'T';
		buf[11] = date_string[11];
		buf[12] = date_string[12];
		buf[13] = ':';
		buf[14] = date_string[14];
		buf[15] = date_string[15];
		buf[16] = ':';
		buf[17] = date_string[17];
		buf[18] = date_string[18];
		buf[19] = date_string[22];
		buf[20] = date_string[23];
		buf[21] = date_string[24];
		buf[22] = ':';
		buf[23] = date_string[26];
		buf[24] = date_string[27];
		buf[25] = '\0';

		return g_strdup (buf);
	}

	return g_strdup (date_string);
}

gchar *
tracker_date_format_to_iso8601 (const gchar *date_string,
                                const gchar *format)
{
	gchar *result;
	struct tm date_tm = { 0 };

	g_return_val_if_fail (date_string != NULL, NULL);
	g_return_val_if_fail (format != NULL, NULL);

	if (strptime (date_string, format, &date_tm) == 0) {
		return NULL;
	}

	result = g_malloc (sizeof (char)*25);

	strftime (result, 25, DATE_FORMAT_ISO8601 , &date_tm);

	return result;
}

gchar *
tracker_date_to_time_string (const gchar *date_string)
{
	gchar *str;

	str = tracker_date_format (date_string);

	if (str) {
		time_t t;

		t = tracker_string_to_date (str, NULL);

		g_free (str);

		if (t != -1) {
			return tracker_gint_to_string (t);
		}
	}

	return NULL;
}

time_t
tracker_string_to_date (const gchar *date_string,
                        gint        *offset_p)
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
		regex = g_regex_new ("^(-?[0-9][0-9][0-9][0-9])-([0-9][0-9])-([0-9][0-9])T([0-9][0-9]):([0-9][0-9]):([0-9][0-9])(\\.[0-9]+)?(Z|((\\+|-)[0-9][0-9]):?([0-9][0-9]))?$", 0, 0, &e);
		if (e) {
			g_error ("%s", e->message);
		}
	}

	if (!g_regex_match (regex, date_string, 0, &match_info)) {
		g_match_info_free (match_info);
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
		t  = mktime (&tm);
		t -= timezone;

		offset = 0;

		match = g_match_info_fetch (match_info, 9);
		if (match) {
			/* non-UTC timezone */
			offset = atoi (match) * 3600;
			g_free (match);

			match = g_match_info_fetch (match_info, 10);
			offset += atoi (match) * 60;
			g_free (match);

			if (offset < -14 * 3600 || offset > 14 * 3600) {
				g_warning ("UTC offset too large: %d seconds", offset);
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
tracker_date_time_set (GValue *value,
                       gint64  time,
                       gint    offset)
{
	g_return_if_fail (G_VALUE_HOLDS (value, TRACKER_TYPE_DATE_TIME));
	g_return_if_fail (offset >= -14 * 3600 && offset <= 14 * 3600);

	value->data[0].v_int64 = time;
	value->data[1].v_int = offset;
}

void
tracker_date_time_set_from_string (GValue      *value,
                                   const gchar *date_time_string)
{
	gint64 time;
	gint offset;

	g_return_if_fail (G_VALUE_HOLDS (value, TRACKER_TYPE_DATE_TIME));
	g_return_if_fail (date_time_string != NULL);

	time = tracker_string_to_date (date_time_string, &offset);
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
