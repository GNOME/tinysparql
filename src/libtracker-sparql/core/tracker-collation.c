/*
 * Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
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

#include <glib.h>
#include <glib/gi18n.h>

#include "tracker-collation.h"

static gboolean
skip_non_alphanumeric (const gchar **str,
                       gint         *len)
{
	const gchar *remaining = *str, *end = &remaining[*len];
	gboolean found = FALSE, is_alnum;
	gunichar unichar;

	while (remaining < end) {
		unichar = g_utf8_get_char (remaining);
		is_alnum = g_unichar_isalnum (unichar);
		if (is_alnum)
			break;

		found = TRUE;
		remaining = g_utf8_next_char (remaining);
	}

	/* The string must not be left empty */
	if (remaining == end)
		return FALSE;

	if (found) {
		*len = end - remaining;
		*str = remaining;
	}

	return found;
}

static gboolean
check_remove_prefix (const gchar  *str,
                     gint          len,
                     const gchar  *prefix,
                     gint          prefix_len,
                     const gchar **str_out,
                     gint         *len_out)
{
	const gchar *remaining;
	gchar *strstart;
	gint remaining_len;

	if (len <= prefix_len)
		return FALSE;

	/* Check that the prefix matches */
	strstart = g_utf8_casefold (str, prefix_len);
	if (strcmp (strstart, prefix) != 0) {
		g_free (strstart);
		return FALSE;
	}

	/* Check that the following letter is a break
	 * character.
	 */
	g_free (strstart);
	remaining = &str[prefix_len];
	remaining_len = len - prefix_len;

	if (!skip_non_alphanumeric (&remaining, &remaining_len))
		return FALSE;

	*len_out = remaining_len;
	*str_out = remaining;
	return TRUE;
}

/* Helper function valid for all implementations */
gint
tracker_collation_utf8_title (gpointer      collator,
                              gint          len1,
                              gconstpointer str1,
                              gint          len2,
                              gconstpointer str2)
{
	const gchar *title_beginnings_str;
	static gchar **title_beginnings = NULL;
	const gchar *res1 = NULL, *res2 = NULL;
	gint i;

	skip_non_alphanumeric ((const gchar **) &str1, &len1);
	skip_non_alphanumeric ((const gchar **) &str2, &len2);

	/* Translators: this is a '|' (U+007C) separated list of common
	 * title beginnings. Meant to be skipped for sorting purposes,
	 * case doesn't matter. Given English media is quite common, it is
	 * advised to leave the untranslated articles in addition to
	 * the translated ones.
	 */
	title_beginnings_str = N_("the|a|an");

	if (!title_beginnings)
		title_beginnings = g_strsplit (_(title_beginnings_str), "|", -1);

	for (i = 0; title_beginnings[i]; i++) {
		gchar *prefix;
		gint prefix_len;

		prefix = g_utf8_casefold (title_beginnings[i], -1);
		prefix_len = strlen (prefix);

		if (!res1)
			check_remove_prefix (str1, len1, prefix, prefix_len,
			                     &res1, &len1);
		if (!res2)
			check_remove_prefix (str2, len2, prefix, prefix_len,
			                     &res2, &len2);
		g_free (prefix);
	}

	if (!res1)
		res1 = str1;
	if (!res2)
		res2 = str2;

	return tracker_collation_utf8 (collator, len1, res1, len2, res2);
}
