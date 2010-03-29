/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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



