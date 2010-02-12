/*
 * Copyright (C) 2009, Nokia
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 * Authors: Philip Van Hoof <philip@codeminded.be>
 */

#include "config.h"

#include <locale.h>
#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>

#include <libtracker-extract/tracker-utils.h>
#include <libtracker-common/tracker-utils.h>

gchar *
tracker_extract_coalesce (gint n_values,
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
tracker_extract_merge (const gchar *delim, gint n_values,
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
tracker_extract_text_normalize (const gchar *text,
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
