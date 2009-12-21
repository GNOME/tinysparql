/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia (urho.konttori@nokia.com)
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

#include <string.h>
#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>

#include "tracker-utils.h"

inline gboolean
tracker_is_empty_string (const char *str)
{
	return str == NULL || str[0] == '\0';
}

inline gboolean
tracker_is_blank_string (const char *str)
{
	register const gchar *p;

	if (str == NULL || str[0] == '\0') {
		return TRUE;
	}

	for (p = str; *p; p = g_utf8_next_char (p)) {
		register gunichar c;

		c = g_utf8_get_char (p);

		if (!g_unichar_isspace (c)) {
			return FALSE;
		}
	}

	return TRUE;
}

/* Removes a substring modifing haystack in place */
gchar *
tracker_string_remove (gchar       *haystack,
                       const gchar *needle)
{
	gchar *current, *pos, *next, *end;
	gint len;

	len = strlen (needle);
	end = haystack + strlen (haystack);
	current = pos = strstr (haystack, needle);

	if (!current) {
		return haystack;
	}

	while (*current != '\0') {
		pos = strstr (pos, needle) + len;
		next = strstr (pos, needle);

		if (!next) {
			next = end;
		}

		while (pos < next) {
			*current = *pos;
			current++;
			pos++;
		}

		if (*pos == '\0') {
			*current = *pos;
		}
	}

	return haystack;
}

gchar *
tracker_string_replace (const gchar *haystack,
                        const gchar *needle,
                        const gchar *replacement)
{
	GString *str;
	gint     pos, needle_len;

	g_return_val_if_fail (haystack != NULL, NULL);
	g_return_val_if_fail (needle != NULL, NULL);

	needle_len = strlen (needle);

	str = g_string_new ("");

	/* FIXME: should use strstr */
	for (pos = 0; haystack[pos]; pos++) {
		if (strncmp (&haystack[pos], needle, needle_len) == 0) {
			if (replacement) {
				str = g_string_append (str, replacement);
			}

			pos += needle_len - 1;
		} else {
			str = g_string_append_c (str, haystack[pos]);
		}
	}

	return g_string_free (str, FALSE);
}

gchar *
tracker_seconds_estimate_to_string (gdouble  seconds_elapsed,
                                    gboolean short_string,
                                    guint    items_done,
                                    guint    items_remaining)
{
	gdouble per_item;
	gdouble total;

	g_return_val_if_fail (seconds_elapsed >= 0.0, g_strdup (_("unknown time")));

	/* We don't want division by 0 or if total is 0 because items
	 * remaining is 0 then, equally pointless.
	 */
	if (items_done < 1 ||
	    items_remaining < 1) {
		return g_strdup (_("unknown time"));
	}

	per_item = seconds_elapsed / items_done;
	total = per_item * items_remaining;

	return tracker_seconds_to_string (total, short_string);
}

gchar *
tracker_seconds_to_string (gdouble  seconds_elapsed,
                           gboolean short_string)
{
	GString *s;
	gchar   *str;
	gdouble  total;
	gint     days, hours, minutes, seconds;

	g_return_val_if_fail (seconds_elapsed >= 0.0, g_strdup (_("less than one second")));

	total    = seconds_elapsed;

	seconds  = (gint) total % 60;
	total   /= 60;
	minutes  = (gint) total % 60;
	total   /= 60;
	hours    = (gint) total % 24;
	days     = (gint) total / 24;

	s = g_string_new ("");

	if (short_string) {
		if (days) { /* Translators: this is %d days */
			g_string_append_printf (s, _(" %dd"), days);
		}

		if (hours) { /* Translators: this is %2.2d hours */
			g_string_append_printf (s, _(" %2.2dh"), hours);
		}

		if (minutes) { /* Translators: this is %2.2d minutes */
			g_string_append_printf (s, _(" %2.2dm"), minutes);
		}

		if (seconds) { /* Translators: this is %2.2d seconds */
			g_string_append_printf (s, _(" %2.2ds"), seconds);
		}
	} else {
		if (days) {
			g_string_append_printf (s, ngettext (" %d day", " %d days", days), days);
		}

		if (hours) {
			g_string_append_printf (s, ngettext (" %2.2d hour", " %2.2d hours", hours), hours);
		}

		if (minutes) {
			g_string_append_printf (s, ngettext (" %2.2d minute", " %2.2d minutes", minutes), minutes);
		}

		if (seconds) {
			g_string_append_printf (s, ngettext (" %2.2d second", " %2.2d seconds", seconds), seconds);
		}
	}

	str = g_string_free (s, FALSE);

	if (str[0] == '\0') {
		g_free (str);
		str = g_strdup (_("less than one second"));
	} else {
		g_strchug (str);
	}

	return str;
}

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
tracker_merge (const gchar *delim, gint n_values,
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
