/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <time.h>
#include <string.h>
#include <stdio.h>

#include <libtracker-common/tracker-utils.h>
#include <libtracker-common/tracker-date-time.h>

#include "tracker-utils.h"

#ifndef HAVE_GETLINE

#include <stddef.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>

#undef getdelim
#undef getline

#define GROW_BY 80

#endif /* HAVE_GETLINE */

#define DATE_FORMAT_ISO8601 "%Y-%m-%dT%H:%M:%S%z"

/**
 * SECTION:tracker-utils
 * @title: Data utilities
 * @short_description: Functions for coalescing, merging, date
 * handling and normalizing
 * @stability: Stable
 * @include: libtracker-extract/tracker-extract.h
 *
 * This API is provided to facilitate common more general functions
 * which extractors may find useful. These functions are also used by
 * the in-house extractors quite frequently.
 **/

static const char *months[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static const char imonths[] = {
	'1', '2', '3', '4', '5',
	'6', '7', '8', '9', '0', '1', '2'
};


/**
 * tracker_coalesce_strip:
 * @n_values: the number of @Varargs supplied
 * @Varargs: the string pointers to coalesce
 *
 * This function iterates through a series of string pointers passed
 * using @Varargs and returns the first which is not %NULL, not empty
 * (i.e. "") and not comprised of one or more spaces (i.e. " ").
 *
 * The returned value is stripped using g_strstrip(). It is MOST
 * important NOT to pass constant string pointers to this function!
 *
 * Returns: the first string pointer from those provided which
 * matches, otherwise %NULL.
 *
 * Since: 0.9
 **/
const gchar *
tracker_coalesce_strip (gint n_values,
                        ...)
{
	va_list args;
	gint    i;
	const gchar *result = NULL;

	va_start (args, n_values);

	for (i = 0; i < n_values; i++) {
		gchar *value;

		value = va_arg (args, gchar *);
		if (!result && !tracker_is_blank_string (value)) {
			result = (const gchar *) g_strstrip (value);
			break;
		}
	}

	va_end (args);

	return result;
}

/**
 * tracker_coalesce:
 * @n_values: the number of @Varargs supplied
 * @Varargs: the string pointers to coalesce
 *
 * This function iterates through a series of string pointers passed
 * using @Varargs and returns the first which is not %NULL, not empty
 * (i.e. "") and not comprised of one or more spaces (i.e. " ").
 *
 * The returned value is stripped using g_strstrip(). All other values
 * supplied are freed. It is MOST important NOT to pass constant
 * string pointers to this function!
 *
 * Returns: the first string pointer from those provided which
 * matches, otherwise %NULL.
 *
 * Since: 0.8
 *
 * Deprecated: 1.0: Use tracker_coalesce_strip() instead.
 *
 **/
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


/**
 * tracker_merge_const:
 * @delimiter: the delimiter to use when merging
 * @n_values: the number of @Varargs supplied
 * @Varargs: the string pointers to merge
 *
 * This function iterates through a series of string pointers passed
 * using @Varargs and returns a newly allocated string of the merged
 * strings.
 *
 * The @delimiter can be %NULL. If specified, it will be used in
 * between each merged string in the result.
 *
 * Returns: a newly-allocated string holding the result which should
 * be freed with g_free() when finished with, otherwise %NULL.
 *
 * Since: 0.9
 **/
gchar *
tracker_merge_const (const gchar *delimiter,
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
				if (delimiter) {
					g_string_append (str, delimiter);
				}
				g_string_append (str, value);
			}
		}
	}

	va_end (args);

	if (!str) {
		return NULL;
	}

	return g_string_free (str, FALSE);
}

/**
 * tracker_merge:
 * @delimiter: the delimiter to use when merging
 * @n_values: the number of @Varargs supplied
 * @Varargs: the string pointers to merge
 *
 * This function iterates through a series of string pointers passed
 * using @Varargs and returns a newly allocated string of the merged
 * strings. All passed strings are freed (don't pass const values)/
 *
 * The @delimiter can be %NULL. If specified, it will be used in
 * between each merged string in the result.
 *
 * Returns: a newly-allocated string holding the result which should
 * be freed with g_free() when finished with, otherwise %NULL.
 *
 * Since: 0.8
 *
 * Deprecated: 1.0: Use tracker_merge_const() instead.
 **/
gchar *
tracker_merge (const gchar *delimiter,
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
				if (delimiter) {
					g_string_append (str, delimiter);
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

/**
 * tracker_text_normalize:
 * @text: the text to normalize
 * @max_words: the maximum words of @text to normalize
 * @n_words: the number of words actually normalized
 *
 * This function iterates through @text checking for UTF-8 validity
 * using g_utf8_get_char_validated(). For each character found, the
 * %GUnicodeType is checked to make sure it is one fo the following
 * values:
 * <itemizedlist>
 *  <listitem><para>%G_UNICODE_LOWERCASE_LETTER</para></listitem>
 *  <listitem><para>%G_UNICODE_MODIFIER_LETTER</para></listitem>
 *  <listitem><para>%G_UNICODE_OTHER_LETTER</para></listitem>
 *  <listitem><para>%G_UNICODE_TITLECASE_LETTER</para></listitem>
 *  <listitem><para>%G_UNICODE_UPPERCASE_LETTER</para></listitem>
 * </itemizedlist>
 *
 * All other symbols, punctuation, marks, numbers and separators are
 * stripped. A regular space (i.e. " ") is used to separate the words
 * in the returned string.
 *
 * The @n_words can be %NULL. If specified, it will be populated with
 * the number of words that were normalized in the result.
 *
 * Returns: a newly-allocated string holding the result which should
 * be freed with g_free() when finished with, otherwise %NULL.
 *
 * Since: 0.8
 *
 * Deprecated: 1.0: Use tracker_text_validate_utf8() instead.
 **/
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
		if (!in_break) {
			/* Count the last word */
			words += 1;
		}
		*n_words = words;
	}

	return g_string_free (string, FALSE);
}

/**
 * tracker_text_validate_utf8:
 * @text: the text to validate
 * @text_len: length of @text, or -1 if NUL-terminated
 * @str: the string where to place the validated UTF-8 characters, or %NULL if
 *  not needed.
 * @valid_len: Output number of valid UTF-8 bytes found, or %NULL if not needed
 *
 * This function iterates through @text checking for UTF-8 validity
 * using g_utf8_validate(), appends the first chunk of valid characters
 * to @str, and gives the number of valid UTF-8 bytes in @valid_len.
 *
 * Returns: %TRUE if some bytes were found to be valid, %FALSE otherwise.
 *
 * Since: 0.9
 **/
gboolean
tracker_text_validate_utf8 (const gchar  *text,
                            gssize        text_len,
                            GString     **str,
                            gsize        *valid_len)
{
	gsize len_to_validate;

	g_return_val_if_fail (text, FALSE);

	len_to_validate = text_len >= 0 ? text_len : strlen (text);

	if (len_to_validate > 0) {
		const gchar *end = text;

		/* Validate string, getting the pointer to first non-valid character
		 *  (if any) or to the end of the string. */
		g_utf8_validate (text, len_to_validate, &end);
		if (end > text) {
			/* If str output required... */
			if (str) {
				/* Create string to output if not already as input */
				*str = (*str == NULL ?
				        g_string_new_len (text, end - text) :
				        g_string_append_len (*str, text, end - text));
			}

			/* If utf8 len output required... */
			if (valid_len) {
				*valid_len = end - text;
			}

			return TRUE;
		}
	}

	return FALSE;
}

/**
 * tracker_date_format_to_iso8601:
 * @date_string: the date in a string pointer
 * @format: the format of the @date_string
 *
 * This function uses strptime() to create a time tm structure using
 * @date_string and @format.
 *
 * Returns: a newly-allocated string with the time represented in
 * ISO8601 date format which should be freed with g_free() when
 * finished with, otherwise %NULL.
 *
 * Since: 0.8
 **/
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

/**
 * tracker_date_guess:
 * @date_string: the date in a string pointer
 *
 * This function uses a number of methods to try and guess the date
 * held in @date_string. The @date_string must be at least 5
 * characters in length or longer for any guessing to be attempted.
 * Some of the string formats guessed include:
 *
 * <itemizedlist>
 *  <listitem><para>"YYYY-MM-DD" (Simple format)</para></listitem>
 *  <listitem><para>"20050315113224-08'00'" (PDF format)</para></listitem>
 *  <listitem><para>"20050216111533Z" (PDF format)</para></listitem>
 *  <listitem><para>"Mon Feb  9 10:10:00 2004" (Microsoft Office format)</para></listitem>
 *  <listitem><para>"2005:04:29 14:56:54" (Exif format)</para></listitem>
 *  <listitem><para>"YYYY-MM-DDThh:mm:ss.ff+zz:zz</para></listitem>
 * </itemizedlist>
 *
 * Returns: a newly-allocated string with the time represented in
 * ISO8601 date format which should be freed with g_free() when
 * finished with, otherwise %NULL.
 *
 * Since: 0.8
 **/
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
		/* Check for date part only YYYY-MM-DD */
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

		if (num_month < 0) {
			return NULL;
		}

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

#ifndef HAVE_GETLINE

static gint
my_igetdelim (gchar  **linebuf,
              guint   *linebufsz,
              gint     delimiter,
              FILE    *file)
{
	gint ch;
	gint idx;

	if ((file == NULL || linebuf == NULL || *linebuf == NULL || *linebufsz == 0) &&
	    !(*linebuf == NULL && *linebufsz == 0)) {
		errno = EINVAL;
		return -1;
	}

	if (*linebuf == NULL && *linebufsz == 0) {
		*linebuf = g_malloc (GROW_BY);

		if (!*linebuf) {
			errno = ENOMEM;
			return -1;
		}

		*linebufsz += GROW_BY;
	}

	idx = 0;

	while ((ch = fgetc (file)) != EOF) {
		/* Grow the line buffer as necessary */
		while (idx > *linebufsz - 2) {
			*linebuf = g_realloc (*linebuf, *linebufsz += GROW_BY);

			if (!*linebuf) {
				errno = ENOMEM;
				return -1;
			}
		}
		(*linebuf)[idx++] = (gchar) ch;

		if ((gchar) ch == delimiter) {
			break;
		}
	}

	if (idx != 0) {
		(*linebuf)[idx] = 0;
	} else if ( ch == EOF ) {
		return -1;
	}

	return idx;
}

/**
 * tracker_getline:
 * @lineptr: Buffer to write into
 * @n: Max bytes of linebuf
 * @stream: Filestream to read from
 *
 * Reads an entire line from stream, storing the address of the buffer
 * containing  the  text into *lineptr.  The buffer is null-terminated
 * and includes the newline character, if one was found.
 *
 * Read GNU getline()'s manpage for more information
 *
 * Returns: the number of characters read, including the delimiter
 * character, but not including the terminating %NULL byte. This value
 * can be used to handle embedded %NULL bytes in the line read. Upon
 * failure, -1 is returned.
 *
 * Since: 0.9
 **/
gssize
tracker_getline (gchar **lineptr,
                 gsize  *n,
                 FILE   *stream)
{
	return my_igetdelim (lineptr, n, '\n', stream);
}

#else

/**
 * tracker_getline:
 * @lineptr: Buffer to write into
 * @n: Max bytes of linebuf
 * @stream: Filestream to read from
 *
 * Reads an entire line from stream, storing the address of the buffer
 * containing  the  text into *lineptr.  The buffer is null-terminated
 * and includes the newline character, if one was found.
 *
 * Read GNU getline()'s manpage for more information
 *
 * Returns: the number of characters read, including the delimiter
 * character, but not including the terminating %NULL byte. This value
 * can be used to handle embedded %NULL bytes in the line read. Upon
 * failure, -1 is returned.
 *
 * Since: 0.9
 **/
gssize
tracker_getline (gchar **lineptr,
                 gsize  *n,
                 FILE *stream)
{
	return getline (lineptr, n, stream);
}

#endif /* HAVE_GETLINE */


/**
 * tracker_keywords_parse:
 * @store: Array where to store the keywords
 * @keywords: Keywords line to parse
 *
 * Parses a keywords line into store, avoiding duplicates and stripping leading
 * and trailing spaces from keywords. Allowed delimiters are , and ;
 *
 * Since: 0.9
 **/
void
tracker_keywords_parse (GPtrArray   *store,
                        const gchar *keywords)
{
	gchar *keywords_d = g_strdup (keywords);
	char *saveptr, *p;
	size_t len;

	p = keywords_d;
	keywords_d = strchr (keywords_d, '"');

	if (keywords_d) {
		keywords_d++;
	} else {
		keywords_d = p;
	}

	len = strlen (keywords_d);
	if (keywords_d[len - 1] == '"') {
		keywords_d[len - 1] = '\0';
	}

	for (p = strtok_r (keywords_d, ",;", &saveptr); p;
	     p = strtok_r (NULL, ",;", &saveptr)) {
		guint i;
		gboolean found = FALSE;
		gchar *p_do = g_strdup (p);
		gchar *p_dup = p_do;
		guint len = strlen (p_dup);

		if (*p_dup == ' ')
			p_dup++;

		if (p_dup[len-1] == ' ')
			p_dup[len-1] = '\0';

		for (i = 0; i < store->len; i++) {
			const gchar *earlier = g_ptr_array_index (store, i);
			if (g_strcmp0 (earlier, p_dup) == 0) {
				found = TRUE;
				break;
			}
		}

		if (!found) {
			g_ptr_array_add (store, g_strdup (p_dup));
		}

		g_free (p_do);
	}

	g_free (keywords_d);
}
