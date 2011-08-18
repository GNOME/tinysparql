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
	g_print ("%s\n", result);
}

#define assert_filename_match(a, b) { \
	g_assert (tracker_case_match_filename_without_extension (a, b) == TRUE); \
	g_assert (tracker_case_match_filename_without_extension (b, a) == TRUE); }

#define assert_no_filename_match(a, b) { \
	g_assert (tracker_case_match_filename_without_extension (a, b) == FALSE); \
	g_assert (tracker_case_match_filename_without_extension (b, a) == FALSE); }

static void
test_case_match_filename_without_extension ()
{
	assert_filename_match ("test.mp3", "test.mp3");
	assert_filename_match ("test.mp3", "test.wav");
	assert_filename_match ("test.mp3", "test.mp");
	assert_filename_match ("test.mp3", "test.");
	assert_filename_match ("test.mp3", "test");
	assert_filename_match ("01 - Song 1 (Remix).wav", "01 - Song 1 (Remix).flac");

	assert_no_filename_match ("test.mp3", "bacon.mp3");

	/* Pathological cases, mainly testing that nothing crashes */
	assert_no_filename_match (".", "\n");
	assert_no_filename_match ("as", "as..");
	assert_no_filename_match ("...as", "...as..");
	assert_no_filename_match (".", "test.");
	assert_filename_match ("", ".");
}

int
main (int argc, char **argv)
{
	gboolean ret;

	g_type_init ();
	g_test_init (&argc, &argv, NULL);

	tracker_locale_init ();

	g_test_add_func ("/libtracker-common/tracker-utils/seconds_to_string",
	                 test_seconds_to_string);

	g_test_add_func ("/libtracker-common/tracker-utils/seconds_estimate_to_string",
	                 test_seconds_estimate_to_string);

	g_test_add_func ("/libtracker-common/tracker-utils/case_match_filename_without_extension",
	                 test_case_match_filename_without_extension);

	ret = g_test_run ();

	tracker_locale_shutdown ();

	return ret;
}
