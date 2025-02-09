/*
 * Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
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
 */

#include "config.h"

#include <glib-object.h>
#include <tracker-common.h>
#include <locale.h>

static void
test_strhex (void)
{
        gchar *result;

        result = tracker_strhex ((const guint8 *)"a", 1, '|');
        g_assert_cmpstr (result, ==, "61");
        g_free (result);

        result = tracker_strhex ((const guint8 *)"ab", 2, '@');
        g_assert_cmpstr (result, ==, "61@62");
        g_free (result);

        result = tracker_strhex ((const guint8 *)"a b", 3, '@');
        g_assert_cmpstr (result, ==, "61@20@62");
        g_free (result);

        result = tracker_strhex ((const guint8 *)"abc", 1, '@');
        g_assert_cmpstr (result, ==, "61");
        g_free (result);

}

static void
test_term_ellipsize (void)
{
	gchar *result;

	result = tracker_term_ellipsize ("ab", 3, TRACKER_ELLIPSIZE_START);
	g_assert_cmpstr (result, ==, "ab");
	g_free (result);

	result = tracker_term_ellipsize ("ab", 3, TRACKER_ELLIPSIZE_END);
	g_assert_cmpstr (result, ==, "ab");
	g_free (result);

	result = tracker_term_ellipsize ("abc", 3, TRACKER_ELLIPSIZE_START);
	g_assert_cmpstr (result, ==, "…bc");
	g_free (result);

	result = tracker_term_ellipsize ("abc", 3, TRACKER_ELLIPSIZE_END);
	g_assert_cmpstr (result, ==, "ab…");
	g_free (result);

	result = tracker_term_ellipsize ("😄😄😥😥", 3, TRACKER_ELLIPSIZE_START);
	g_assert_cmpstr (result, ==, "…😥😥");
	g_free (result);

	result = tracker_term_ellipsize ("😄😄😥😥", 3, TRACKER_ELLIPSIZE_END);
	g_assert_cmpstr (result, ==, "😄😄…");
	g_free (result);
}

int
main (int argc, char **argv)
{
	gboolean ret;

	g_test_init (&argc, &argv, NULL);

	setlocale (LC_ALL, "");

        g_test_add_func ("/libtracker-common/tracker-utils/strhex",
                         test_strhex);
        g_test_add_func ("/libtracker-common/tracker-utils/term_ellipsize",
                         test_term_ellipsize);

	ret = g_test_run ();

	return ret;
}
