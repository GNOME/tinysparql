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
#include <libtracker-common/tracker-file-utils.h>
#include <libtracker-common/tracker-utils.h>
#include <libtracker-common/tracker-locale.h>

static void
test_seconds_to_string ()
{
	gchar *result;

	result = tracker_seconds_to_string (0, TRUE);
	g_assert_cmpstr (result, ==, "less than one second");
	g_free (result);

	result = tracker_seconds_to_string (0.1, TRUE);
	g_assert_cmpstr (result, ==, "less than one second");
	g_free (result);

	result = tracker_seconds_to_string (59.9, TRUE);
	g_assert_cmpstr (result, ==, "59s");
	g_free (result);

	result = tracker_seconds_to_string (60, TRUE);
	g_assert_cmpstr (result, ==, "01m");
	g_free (result);

	result = tracker_seconds_to_string (100.12, TRUE);
	g_assert_cmpstr (result, ==, "01m 40s");
	g_free (result);

	result = tracker_seconds_to_string (100, FALSE);
	g_assert_cmpstr (result, ==, "01 minute 40 seconds");
	g_free (result);

	result = tracker_seconds_to_string (1000000, TRUE);
	g_assert_cmpstr (result, ==, "11d 13h 46m 40s");
	g_free (result);

	result = tracker_seconds_to_string (1000000000, TRUE);
	g_assert_cmpstr (result, ==, "11574d 01h 46m 40s");
	g_free (result);

}

static void
test_seconds_estimate_to_string ()
{
	gchar *result;

	result = tracker_seconds_estimate_to_string (60, TRUE, 60, 120);
	g_assert_cmpstr (result, ==, "02m");
	g_free (result);
}

static void
test_is_empty_string ()
{
        g_assert (tracker_is_empty_string (NULL));
        g_assert (tracker_is_empty_string (""));
        g_assert (!tracker_is_empty_string ("Eeeeepa not empty"));
}

static void
test_is_blank_string ()
{
        g_assert (tracker_is_blank_string (NULL));
        g_assert (tracker_is_blank_string (""));
        g_assert (tracker_is_blank_string (" "));
        g_assert (tracker_is_blank_string ("       "));
        g_assert (!tracker_is_blank_string ("   -    "));
        g_assert (!tracker_is_blank_string ("   -"));
        g_assert (!tracker_is_blank_string ("-   "));
        g_assert (!tracker_is_blank_string ("nonono"));

}

static void
test_seconds_estimate (void)
{
        g_assert_cmpint (tracker_seconds_estimate (10, 10, 20), ==, 20);
        g_assert_cmpint (tracker_seconds_estimate (10, 9, 20), ==, 22);

        g_assert_cmpint (tracker_seconds_estimate (0, 2, 2), ==, 0);
        g_assert_cmpint (tracker_seconds_estimate (-1, 2, 2), ==, 0);
        g_assert_cmpint (tracker_seconds_estimate (1, 0, 2), ==, 0);
        g_assert_cmpint (tracker_seconds_estimate (1, -1, 2), ==, 0);
        g_assert_cmpint (tracker_seconds_estimate (1, 1, 0), ==, 0);
}

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

int
main (int argc, char **argv)
{
	gboolean ret;

	g_test_init (&argc, &argv, NULL);

	tracker_locale_init ();

	g_test_add_func ("/libtracker-common/tracker-utils/seconds_to_string",
	                 test_seconds_to_string);

	g_test_add_func ("/libtracker-common/tracker-utils/seconds_estimate_to_string",
	                 test_seconds_estimate_to_string);
        
        g_test_add_func ("/libtracker-common/tracker-utils/seconds_estimate",
                         test_seconds_estimate);

        g_test_add_func ("/libtracker-common/tracker-utils/empty_string",
                         test_is_empty_string);

        g_test_add_func ("/libtracker-common/tracker-utils/blank_string",
                         test_is_blank_string);

        g_test_add_func ("/libtracker-common/tracker-utils/strhex",
                         test_strhex);

	ret = g_test_run ();

	tracker_locale_shutdown ();

	return ret;
}
