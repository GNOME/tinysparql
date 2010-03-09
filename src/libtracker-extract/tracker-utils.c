/*
 * Copyright (C) 2009, Nokia
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
#include <string.h>

#include <libtracker-common/tracker-utils.h>
#include <libtracker-common/tracker-date-time.h>

#include "tracker-utils.h"

#define DATE_FORMAT_ISO8601 "%Y-%m-%dT%H:%M:%S%z"

static const char *months[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static const char imonths[] = {
	'1', '2', '3', '4', '5',
	'6', '7', '8', '9', '0', '1', '2'
};

gchar *
tracker_coalesce (gint n_values,
                  ...)
{
	va_list args;
	gint    i;
	gchar *result = NULL;

	va_start (args, n_values);

	for (i = 0; i < n_values; i++) {
		gchar *value;

		value = va_arg (args, gchar *);
		if (!result && !tracker_is_blank_string (value)) {
			result = g_strstrip (value);
		} else {
			g_free (value);
		}
	}

	va_end (args);

	return result;
}

gchar *
tracker_merge (const gchar *delim, 
               gint         n_values,
               ...)
{
	va_list args;
	gint    i;
	GString *str = NULL;

	va_start (args, n_values);

	for (i = 0; i < n_values; i++) {
		gchar *value;

		value = va_arg (args, gchar *);
		if (value) {
			if (!str) {
				str = g_string_new (value);
			} else {
				if (delim) {
					g_string_append (str, delim);
				}
				g_string_append (str, value);
			}
			g_free (value);
		}
	}

	va_end (args);

	if (!str) {
		return NULL;
	}

	return g_string_free (str, FALSE);
}

gchar *
tracker_text_normalize (const gchar *text,
                        guint        max_words,
                        guint       *n_words)
{
	GString *string;
	gboolean in_break = TRUE;
	gunichar ch;
	gint words = 0;

	string = g_string_new (NULL);

	while ((ch = g_utf8_get_char_validated (text, -1)) > 0) {
		GUnicodeType type;

		type = g_unichar_type (ch);

		if (type == G_UNICODE_LOWERCASE_LETTER ||
		    type == G_UNICODE_MODIFIER_LETTER ||
		    type == G_UNICODE_OTHER_LETTER ||
		    type == G_UNICODE_TITLECASE_LETTER ||
		    type == G_UNICODE_UPPERCASE_LETTER) {
			/* Append regular chars */
			g_string_append_unichar (string, ch);
			in_break = FALSE;
		} else if (!in_break) {
			/* Non-regular char found, treat as word break */
			g_string_append_c (string, ' ');
			in_break = TRUE;
			words++;

			if (words > max_words) {
				break;
			}
		}

		text = g_utf8_find_next_char (text, NULL);
	}

	if (n_words) {
		*n_words = words;
	}

	return g_string_free (string, FALSE);
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

	result = g_malloc (sizeof (char) * 25);

	strftime (result, 25, DATE_FORMAT_ISO8601 , &date_tm);

	return result;
}

static gboolean
is_int (const gchar *str)
{
	gint i, len;

	if (!str || str[0] == '\0') {
		return FALSE;
	}

	len = strlen (str);

	for (i = 0; i < len; i++) {
		if (!g_ascii_isdigit (str[i])) {
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
tracker_date_guess (const gchar *date_string)
{
	gchar buf[30];
	gint  len;
	GError *error = NULL;

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

			tracker_string_to_date (buf, NULL, &error);

			if (error != NULL) {
				g_error_free (error);
				return NULL;
			}

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

		tracker_string_to_date (buf, NULL, &error);

		if (error != NULL) {
			g_error_free (error);
			return NULL;
		}

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

		tracker_string_to_date (buf, NULL, &error);

		if (error != NULL) {
			g_error_free (error);
			return NULL;
		}

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

		tracker_string_to_date (buf, NULL, &error);

		if (error != NULL) {
			g_error_free (error);
			return NULL;
		}

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

		tracker_string_to_date (buf, NULL, &error);

		if (error != NULL) {
			g_error_free (error);
			return NULL;
		}

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

		tracker_string_to_date (buf, NULL, &error);

		if (error != NULL) {
			g_error_free (error);
			return NULL;
		}

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

		tracker_string_to_date (buf, NULL, &error);

		if (error != NULL) {
			g_error_free (error);
			return NULL;
		}

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

		tracker_string_to_date (buf, NULL, &error);

		if (error != NULL) {
			g_error_free (error);
			return NULL;
		}

		return g_strdup (buf);
	}

	tracker_string_to_date (date_string, NULL, &error);

	if (error != NULL) {
		g_error_free (error);
		return NULL;
	}

	return g_strdup (date_string);
}
