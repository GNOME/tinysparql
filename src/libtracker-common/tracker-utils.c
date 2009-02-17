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

/* Removes a substring modifing haystack in place */
gchar *
tracker_string_remove (gchar	   *haystack,
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
	gint	 pos, needle_len;

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
tracker_escape_string (const gchar *in)
{
	gchar **array, *out;

	if (!in) {
		return NULL;
	}

	if (!strchr (in, '\'')) {
		return g_strdup (in);
	}

	/* double single quotes */
	array = g_strsplit (in, "'", -1);
	out = g_strjoinv ("''", array);
	g_strfreev (array);

	return out;
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
	gchar	*str;
	gdouble  total;
	gint	 days, hours, minutes, seconds;

	g_return_val_if_fail (seconds_elapsed >= 0.0, g_strdup (_("less than one second")));

	total	 = seconds_elapsed;

	seconds  = (gint) total % 60;
	total	/= 60;
	minutes  = (gint) total % 60;
	total	/= 60;
	hours	 = (gint) total % 24;
	days	 = (gint) total / 24;

	s = g_string_new ("");

	if (short_string) {
		if (days) {
			g_string_append_printf (s, " %dd", days);
		}

		if (hours) {
			g_string_append_printf (s, " %2.2dh", hours);
		}

		if (minutes) {
			g_string_append_printf (s, " %2.2dm", minutes);
		}

		if (seconds) {
			g_string_append_printf (s, " %2.2ds", seconds);
		}
	} else {
		if (days) {
			g_string_append_printf (s, " %d day%s",
						days,
						days == 1 ? "" : "s");
		}

		if (hours) {
			g_string_append_printf (s, " %2.2d hour%s",
						hours,
						hours == 1 ? "" : "s");
		}

		if (minutes) {
			g_string_append_printf (s, " %2.2d minute%s",
						minutes,
						minutes == 1 ? "" : "s");
		}

		if (seconds) {
			g_string_append_printf (s, " %2.2d second%s",
						seconds,
						seconds == 1 ? "" : "s");
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

void
tracker_throttle (TrackerConfig *config,
		  gint		 multiplier)
{
	gint throttle;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	/* Get the throttle, add 5 (minimum value) so we don't do
	 * nothing and then multiply it by the factor given
	 */
	throttle  = tracker_config_get_throttle (config);
	throttle += 5;
	throttle *= multiplier;

	if (throttle > 0) {
		g_usleep (throttle);
	}
}

/* Temporary: Just here until we upgrade to GLib 2.18. */
static gboolean
tracker_dgettext_should_translate (void)
{
  static gsize translate = 0;
  enum {
    SHOULD_TRANSLATE = 1,
    SHOULD_NOT_TRANSLATE = 2
  };

  if (G_UNLIKELY (g_once_init_enter (&translate)))
    {
      gboolean should_translate = TRUE;

      const char *default_domain     = textdomain (NULL);
      const char *translator_comment = gettext ("");
#ifndef G_OS_WIN32
      const char *translate_locale   = setlocale (LC_MESSAGES, NULL);
#else
      const char *translate_locale   = g_win32_getlocale ();
#endif
      /* We should NOT translate only if all the following hold:
       *   - user has called textdomain() and set textdomain to non-default
       *   - default domain has no translations
       *   - locale does not start with "en_" and is not "C"
       *
       * Rationale:
       *   - If text domain is still the default domain, maybe user calls
       *     it later. Continue with old behavior of translating.
       *   - If locale starts with "en_", we can continue using the
       *     translations even if the app doesn't have translations for
       *     this locale.  That is, en_UK and en_CA for example.
       *   - If locale is "C", maybe user calls setlocale(LC_ALL,"") later.
       *     Continue with old behavior of translating.
       */
      if (0 != strcmp (default_domain, "messages") &&
          '\0' == *translator_comment &&
          0 != strncmp (translate_locale, "en_", 3) &&
          0 != strcmp (translate_locale, "C"))
        should_translate = FALSE;

      g_once_init_leave (&translate,
                         should_translate ?
                         SHOULD_TRANSLATE :
                         SHOULD_NOT_TRANSLATE);
    }

  return translate == SHOULD_TRANSLATE;
}

G_CONST_RETURN gchar *
tracker_dngettext (const gchar *domain,
		   const gchar *msgid,
		   const gchar *msgid_plural,
		   gulong       n)
{
  if (domain && G_UNLIKELY (!tracker_dgettext_should_translate ()))
    return n == 1 ? msgid : msgid_plural;

  return dngettext (domain, msgid, msgid_plural, n);
}
