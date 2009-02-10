/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia
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

#include "config.h"

#define _XOPEN_SOURCE
#include <time.h>

#include <strings.h>
#include <string.h>
#include <stdlib.h>

#include <glib.h>

#include "tracker-log.h"
#include "tracker-utils.h"
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
	gint	 i, len;

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
			buf[19] = '\0';

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

		t = tracker_string_to_date (str);

		g_free (str);

		if (t != -1) {
			return tracker_gint_to_string (t);
		}
	}

	return NULL;
}

static gboolean
is_valid_8601_datetime (const gchar *date_string)
{
	gint len;

	len = strlen (date_string);

	if (len < 19) {
		return FALSE;
	}

	if (!g_ascii_isdigit (date_string[0]) ||
	    !g_ascii_isdigit (date_string[1]) ||
	    !g_ascii_isdigit (date_string[2]) ||
	    !g_ascii_isdigit (date_string[3])) {
		return FALSE;
	}

	if (date_string[4] != '-') {
		return FALSE;
	}

	if (!g_ascii_isdigit (date_string[5]) ||
	    !g_ascii_isdigit (date_string[6])) {
		return FALSE;
	}

	if (date_string[7] != '-') {
		return FALSE;
	}

	if (!g_ascii_isdigit (date_string[8]) ||
	    !g_ascii_isdigit (date_string[9])) {
		return FALSE;
	}

	if ((date_string[10] != 'T')) {
		return FALSE;
	}

	if (!g_ascii_isdigit (date_string[11]) ||
	    !g_ascii_isdigit (date_string[12])) {
		return FALSE;
	}

	if (date_string[13] != ':') {
		return FALSE;
	}

	if (!g_ascii_isdigit (date_string[14]) ||
	    !g_ascii_isdigit (date_string[15])) {
		return FALSE;
	}

	if (date_string[16] != ':'){
		return FALSE;
	}

	if (!g_ascii_isdigit (date_string[17]) ||
	    !g_ascii_isdigit (date_string[18])) {
		return FALSE;
	}

	if (len == 20) {
		if (date_string[19] != 'Z') {
			return FALSE;
		}
	} else {
		if (len > 20) {
			/* Format must be YYYY-MM-DDThh:mm:ss+xx  or
			 * YYYY-MM-DDThh:mm:ss+xx:yy or
			 * YYYY-MM-DDThh:mm:ss+xxyy
			 */
			if (len < 22 || len > 25) {
				return FALSE;
			}

			if (date_string[19] != '+' &&
			    date_string[19] != '-') {
				return FALSE;
			}

			if (!g_ascii_isdigit (date_string[20]) ||
			    !g_ascii_isdigit (date_string[21])) {
				return FALSE;
			}
		}
	}

	return TRUE;
}

time_t
tracker_string_to_date (const gchar *date_string)
{
	struct tm tm;
	long	  val;
	time_t	  t;

	g_return_val_if_fail (date_string, -1);

	/* We should have a valid iso 8601 date in format
	 * YYYY-MM-DDThh:mm:ss with optional TZ
	 */
	if (!is_valid_8601_datetime (date_string)) {
		return -1;
	}

	memset (&tm, 0, sizeof (struct tm));
	val = strtoul (date_string, (gchar**) &date_string, 10);

	if (*date_string == '-') {
		/* YYYY-MM-DD */
		tm.tm_year = val - 1900;
		date_string++;
		tm.tm_mon = strtoul (date_string, (gchar **) &date_string, 10) - 1;

		if (*date_string++ != '-') {
			return -1;
		}

		tm.tm_mday = strtoul (date_string, (gchar **) &date_string, 10);
	}

	if (*date_string++ != 'T') {
		g_critical ("Date validation failed for '%s' st '%c'",
			    date_string,
			    *date_string);
		return -1;
	}

	val = strtoul (date_string, (gchar**) &date_string, 10);

	if (*date_string == ':') {
		/* hh:mm:ss */
		tm.tm_hour = val;
		date_string++;
		tm.tm_min = strtoul (date_string, (gchar**) &date_string, 10);

		if (*date_string++ != ':') {
			return -1;
		}

		tm.tm_sec = strtoul (date_string, (gchar**) &date_string, 10);
	}

	/* mktime() always assumes that "tm" is in locale time but we
	 * want to keep control on time, so we go to UTC
	 */
	t  = mktime (&tm);
	t -= timezone;

	if (*date_string == '+' ||
	    *date_string == '-') {
		gint sign;

		sign = *date_string++ == '+' ? -1 : 1;

		/* We have format hh:mm or hhmm */
		/* Now, we are reading hours */
		if (date_string[0] &&
		    date_string[1]) {
			if (g_ascii_isdigit (date_string[0]) &&
			    g_ascii_isdigit (date_string[1])) {
				gchar buff[3];

				buff[0] = date_string[0];
				buff[1] = date_string[1];
				buff[2] = '\0';

				val = strtoul (buff, NULL, 10);
				t += sign * (3600 * val);
				date_string += 2;
			}

			if (*date_string == ':' || *date_string == '\'') {
				date_string++;
			}
		}

		/* Now, we are reading minutes */
		if (date_string[0] &&
		    date_string[1]) {
			if (g_ascii_isdigit (date_string[0]) &&
			    g_ascii_isdigit (date_string[1])) {
				gchar buff[3];

				buff[0] = date_string[0];
				buff[1] = date_string[1];
				buff[2] = '\0';

				val = strtoul (buff, NULL, 10);
				t += sign * (60 * val);
				date_string += 2;
			}
		}
	}

	return t;
}

gchar *
tracker_date_to_string (time_t date_time)
{
	gchar	  buffer[30];
	struct tm local_time;
	size_t	  count;

	memset (buffer, '\0', sizeof (buffer));
	memset (&local_time, 0, sizeof (struct tm));

	localtime_r (&date_time, &local_time);

	/* Output is ISO 8160 format : "YYYY-MM-DDThh:mm:ss+zz:zz" */
	count = strftime (buffer, sizeof (buffer), "%FT%T%z", &local_time);

	return count > 0 ? g_strdup (buffer) : NULL;
}

gchar *
tracker_glong_to_string (glong i)
{
	return g_strdup_printf ("%ld", i);
}

gchar *
tracker_gint_to_string (gint i)
{
	return g_strdup_printf ("%d", i);
}

gchar *
tracker_guint_to_string (guint i)
{
	return g_strdup_printf ("%u", i);
}

gchar *
tracker_gint32_to_string (gint32 i)
{
	return g_strdup_printf ("%" G_GINT32_FORMAT, i);
}

gchar *
tracker_guint32_to_string (guint32 i)
{
	return g_strdup_printf ("%" G_GUINT32_FORMAT, i);
}

gboolean
tracker_string_to_uint (const gchar *s,
			guint	    *value)
{
	unsigned long int  n;
	gchar		  *end;

	g_return_val_if_fail (s != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);

	n = (guint) strtoul (s, &end, 10);

	if (end == s) {
		*value = 0;
		return FALSE;
	}

	if (n > G_MAXUINT) {
		*value = 0;
		return FALSE;

	} else {
		*value = (guint) n;
		return TRUE;
	}
}

gint
tracker_string_in_string_list (const gchar  *str,
			       gchar	   **strv)
{
	gchar **p;
	gint	i;

	g_return_val_if_fail (str != NULL, -1);

	if (!strv) {
		return -1;
	}

	for (p = strv, i = 0; *p; p++, i++) {
		if (strcasecmp (*p, str) == 0) {
			return i;
		}
	}

	return -1;
}

gboolean
tracker_string_in_gslist (const gchar *str,
			  GSList      *list)
{
	GSList *l;

	g_return_val_if_fail (str != NULL, FALSE);

	for (l = list; l; l = l->next) {
		if (g_strcmp0 (l->data, str) == 0) {
			return TRUE;
		}
	}

	return FALSE;
}

GSList *
tracker_string_list_to_gslist (gchar **strv,
			       gsize   size)
{
	GSList *list;
	gsize	i;
	gsize	size_used;

	g_return_val_if_fail (strv != NULL, NULL);

	if (size < 1) {
		size_used = g_strv_length (strv);
	} else {
		size_used = size;
	}

	list = NULL;

	for (i = 0; i < size; i++) {
		if (strv[i]) {
			list = g_slist_prepend (list, g_strdup (strv[i]));
		} else {
			break;
		}
	}

	return g_slist_reverse (list);
}

gchar *
tracker_string_list_to_string (gchar **strv,
			       gsize   size,
			       gchar   sep)
{
	GString *string;
	gsize	 i;
	gsize	 size_used;

	g_return_val_if_fail (strv != NULL, NULL);

	if (size < 1) {
		size_used = g_strv_length (strv);
	} else {
		size_used = size;
	}

	string = g_string_new ("");

	for (i = 0; i < size; i++) {
		if (strv[i]) {
			if (i > 0) {
				g_string_append_c (string, sep);
			}

			string = g_string_append (string, strv[i]);
		} else {
			break;
		}
	}

	return g_string_free (string, FALSE);
}

gchar **
tracker_string_to_string_list (const gchar *str)
{
	gchar **result;

	result = g_new0 (gchar *, 2);

	result [0] = g_strdup (str);
	result [1] = NULL;

	return result;
}

gchar **
tracker_gslist_to_string_list (GSList *list)
{
	GSList	*l;
	gchar  **strv;
	gint	 i;

	strv = g_new0 (gchar*, g_slist_length (list) + 1);

	for (l = list, i = 0; l; l = l->next) {
		if (!l->data) {
			continue;
		}

		strv[i++] = g_strdup (l->data);
	}

	strv[i] = NULL;

	return strv;
}

GSList *
tracker_gslist_copy_with_string_data (GSList *list)
{
	GSList *l;
	GSList *new_list;

	if (!list) {
		return NULL;
	}

	new_list = NULL;

	for (l = list; l; l = l->next) {
		new_list = g_slist_prepend (new_list, g_strdup (l->data));
	}

	new_list = g_slist_reverse (new_list);

	return new_list;
}

GList *
tracker_glist_copy_with_string_data (GList *list)
{
	GList *l;
	GList *new_list;

	if (!list) {
		return NULL;
	}

	new_list = NULL;

	for (l = list; l; l = l->next) {
		new_list = g_list_prepend (new_list, g_strdup (l->data));
	}

	new_list = g_list_reverse (new_list);

	return new_list;
}

gchar *
tracker_string_boolean_to_string_gint (const gchar *value)
{
	g_return_val_if_fail (value != NULL, NULL);

	if (g_ascii_strcasecmp (value, "true") == 0) {
		return g_strdup ("1");
	} else if (g_ascii_strcasecmp (value, "false") == 0) {
		return g_strdup ("0");
	} else {
		return g_strdup (value);
	}
}
