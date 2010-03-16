/*
 * Copyright (C) 2008, Nokia (urho.konttori@nokia.com)
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

#include <time.h>
#include <string.h>

#include <glib-object.h>

#include <libtracker-common/tracker-date-time.h>
#include <libtracker-common/tracker-type-utils.h>

#include <tracker-test-helpers.h>


static void
test_string_to_date (void)
{
	GDate     *expected;
	GDate     *result;
	time_t     result_time_t;
	const gchar  *input = "2008-06-16T11:10:10+0600";
	gchar  *timezone = g_strdup (g_getenv ("TZ"));
	GError *error = NULL;

	if (! g_setenv ("TZ", "UTC", TRUE)) {
		g_test_message ("unable to set timezone, test results are invalid, skipping\n");
		if (timezone) {
			g_free (timezone);
		}
		return;
	}

	expected = g_date_new_dmy (16, G_DATE_JUNE, 2008);

	result_time_t = tracker_string_to_date (input, NULL, &error);
	g_assert_no_error (error);

	result = g_date_new ();
	g_date_set_time_t (result, result_time_t);

	g_setenv ("TZ", timezone, TRUE);
	if (timezone) {
		g_free (timezone);
	}

	g_assert_cmpint (g_date_get_year (expected), ==, g_date_get_year (result));
	g_assert_cmpint (g_date_get_day (expected), ==, g_date_get_day (result));
	g_assert_cmpint (g_date_get_month (expected), ==, g_date_get_month (result));

	if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR)) {
		result_time_t = tracker_string_to_date (NULL, NULL, NULL);
	}
	g_test_trap_assert_failed ();

	result_time_t = tracker_string_to_date ("", NULL, &error);
	g_assert_cmpint (result_time_t, ==, -1);
	g_assert_error (error, TRACKER_DATE_ERROR, TRACKER_DATE_ERROR_INVALID_ISO8601);
	g_error_free (error);
	error = NULL;

	result_time_t = tracker_string_to_date ("i am not a date", NULL, &error);
	g_assert_cmpint (result_time_t, ==, -1);
	g_assert_error (error, TRACKER_DATE_ERROR, TRACKER_DATE_ERROR_INVALID_ISO8601);
	g_error_free (error);
	error = NULL;

	/* Fails! Check the code
	   result_time_t = tracker_string_to_date ("2008-06-32T04:23:10+0000", NULL);
	   g_assert_cmpint (result_time_t, ==, -1);
	*/
}

static void
test_date_to_string (void)
{
	struct tm *original;
	time_t     input;
	gchar     *result;

	original = g_new0 (struct tm, 1);
	original->tm_sec = 10;
	original->tm_min = 53;
	original->tm_hour = 23;
	original->tm_mday = 16;
	original->tm_mon = 5;
	original->tm_year = 108;
	original->tm_isdst = 0;

#if !(defined(__FreeBSD__) || defined(__OpenBSD__))
	input = mktime (original) - timezone;
#else
	input = timegm (original);
#endif

	result = tracker_date_to_string (input);

	g_assert (result != NULL && strncmp (result, "2008-06-16T23:53:10Z", 19) == 0);
}

static void
test_long_to_string (void)
{
	glong n;
	gchar *result;

	n = 10050;
	result = tracker_glong_to_string (n);
	g_assert (tracker_test_helpers_cmpstr_equal (result, "10050"));
	g_free (result);

	n = -9950;
	result = tracker_glong_to_string (n);
	g_assert (tracker_test_helpers_cmpstr_equal (result, "-9950"));
	g_free (result);
}

static void
test_int_to_string (void)
{
	gint n;
	gchar *result;

	n = 654;
	result = tracker_gint_to_string (n);
	g_assert (tracker_test_helpers_cmpstr_equal (result, "654"));
	g_free (result);

	n = -963;
	result = tracker_gint_to_string (n);
	g_assert (tracker_test_helpers_cmpstr_equal (result, "-963"));
	g_free (result);

}

static void
test_uint_to_string (void)
{
	guint n;
	gchar *result;

	n = 100;
	result = tracker_guint_to_string (n);
	g_assert (tracker_test_helpers_cmpstr_equal (result, "100"));
	g_free (result);
}

static void
test_gint32_to_string (void)
{
	gint32 n;
	gchar *result;

	n = 100;
	result = tracker_gint32_to_string (n);
	g_assert (tracker_test_helpers_cmpstr_equal (result, "100"));
	g_free (result);

	n = -96;
	result = tracker_gint32_to_string (n);
	g_assert (tracker_test_helpers_cmpstr_equal (result, "-96"));
	g_free (result);

}

static void
test_guint32_to_string (void)
{
	guint32 n;
	gchar *result;

	n = 100;
	result = tracker_guint32_to_string (n);
	g_assert (tracker_test_helpers_cmpstr_equal (result, "100"));
	g_free (result);

}

static void
test_string_to_uint (void)
{
	guint num_result, rc;

	rc = tracker_string_to_uint ("10", &num_result);

	g_assert (rc);
	g_assert_cmpint (num_result, ==, 10);


	if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR)) {
		rc = tracker_string_to_uint (NULL, &num_result);
	}
	g_test_trap_assert_failed ();

	/* ???? FIXME */
	rc = tracker_string_to_uint ("-20", &num_result);

	if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR)) {
		tracker_string_to_uint (NULL, &num_result);
	}
	g_test_trap_assert_failed ();

	if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR)) {
		tracker_string_to_uint ("199", NULL);
	}
	g_test_trap_assert_failed ();

	rc = tracker_string_to_uint ("i am not a number", &num_result);
	g_assert (!rc);
	g_assert_cmpint (rc, ==, 0);
}

static void
test_string_in_string_list (void)
{
	const gchar *complete = "This is an extract of text with different terms an props like Audio:Title ...";
	gchar **pieces;

	pieces = g_strsplit (complete, " ", -1);

	g_assert_cmpint (tracker_string_in_string_list ("is", pieces), ==, 1);
	g_assert_cmpint (tracker_string_in_string_list ("Audio:Title", pieces), ==, 12);

	if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR)) {
		g_assert_cmpint (tracker_string_in_string_list (NULL, pieces), ==, -1);
	}
	g_test_trap_assert_failed ();

	if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR)) {
		g_assert_cmpint (tracker_string_in_string_list ("terms", NULL), ==, -1);
	}
	g_test_trap_assert_failed ();
}

static void
test_gslist_to_string_list (void)
{
	GSList *input = NULL;
	gchar **result;

	input = g_slist_prepend (input, (gpointer) "four");
	input = g_slist_prepend (input, (gpointer) "three");
	input = g_slist_prepend (input, (gpointer) "two");
	input = g_slist_prepend (input, (gpointer) "one");

	result = tracker_gslist_to_string_list (input);

	g_assert (tracker_test_helpers_cmpstr_equal (result[0], "one") &&
	          tracker_test_helpers_cmpstr_equal (result[1], "two") &&
	          tracker_test_helpers_cmpstr_equal (result[2], "three") &&
	          tracker_test_helpers_cmpstr_equal (result[3], "four"));

	g_strfreev (result);

	if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR)) {
		result = tracker_gslist_to_string_list (NULL);
	}

	g_test_trap_assert_failed ();
}

static void
test_string_list_to_string (void)
{
	const gchar *input = "one two three four";
	gchar **pieces;
	gchar *result;

	pieces = g_strsplit (input, " ", 4);

	result = tracker_string_list_to_string (pieces, 4, ' ');
	g_assert (tracker_test_helpers_cmpstr_equal (input, result));
	g_free (result);

	result = tracker_string_list_to_string (pieces, 3, '_');
	g_assert (tracker_test_helpers_cmpstr_equal ("one_two_three", result));
	g_free (result);


	if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR)) {
		result = tracker_string_list_to_string (NULL, 6, 'x');
	}
	g_test_trap_assert_failed ();

	result = tracker_string_list_to_string (pieces, -1, ' ');
	g_assert (tracker_test_helpers_cmpstr_equal (input, result));
	g_free (result);

	result = tracker_string_list_to_string (pieces, 6, ' ');
	g_assert (tracker_test_helpers_cmpstr_equal (input, result));
	g_free (result);

	g_strfreev (pieces);
}

static void
test_boolean_as_text_to_number (void)
{
	gchar *result;

	/* Correct true values */
	result = tracker_string_boolean_to_string_gint ("True");
	g_assert (tracker_test_helpers_cmpstr_equal (result, "1"));
	g_free (result);


	result = tracker_string_boolean_to_string_gint ("TRUE");
	g_assert (tracker_test_helpers_cmpstr_equal (result, "1"));
	g_free (result);

	result = tracker_string_boolean_to_string_gint ("true");
	g_assert (tracker_test_helpers_cmpstr_equal (result, "1"));
	g_free (result);

	/* Correct false values */
	result = tracker_string_boolean_to_string_gint ("False");
	g_assert (tracker_test_helpers_cmpstr_equal (result, "0"));
	g_free (result);

	result = tracker_string_boolean_to_string_gint ("FALSE");
	g_assert (tracker_test_helpers_cmpstr_equal (result, "0"));
	g_free (result);

	result = tracker_string_boolean_to_string_gint ("false");
	g_assert (tracker_test_helpers_cmpstr_equal (result, "0"));
	g_free (result);

	/* Invalid values */
	result = tracker_string_boolean_to_string_gint ("Thrue");
	g_assert (tracker_test_helpers_cmpstr_equal (result, "Thrue"));
	g_free (result);

	result = tracker_string_boolean_to_string_gint ("Falsez");
	g_assert (tracker_test_helpers_cmpstr_equal (result, "Falsez"));
	g_free (result);

	result = tracker_string_boolean_to_string_gint ("Other invalid value");
	g_assert (tracker_test_helpers_cmpstr_equal (result, "Other invalid value"));
	g_free (result);


	if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR)) {
		result = tracker_string_boolean_to_string_gint (NULL);
	}
	g_test_trap_assert_failed ();
}

int
main (int argc, char **argv)
{
	gint result;

	g_type_init ();
	g_test_init (&argc, &argv, NULL);

	g_test_add_func ("/libtracker-common/tracker-type-utils/boolean_as_text_to_number",
	                 test_boolean_as_text_to_number);
	g_test_add_func ("/libtracker-common/tracker-type-utils/string_list_as_list",
	                 test_string_list_to_string);
	g_test_add_func ("/libtracker-common/tracker-type-utils/gslist_to_string_list",
	                 test_gslist_to_string_list);
	g_test_add_func ("/libtracker-common/tracker-type-utils/string_in_string_list",
	                 test_string_in_string_list);
	g_test_add_func ("/libtracker-common/tracker-type-utils/string_to_uint",
	                 test_string_to_uint);
	g_test_add_func ("/libtracker-common/tracker-type-utils/guint32_to_string",
	                 test_guint32_to_string);
	g_test_add_func ("/libtracker-common/tracker-type-utils/gint32_to_string",
	                 test_gint32_to_string);
	g_test_add_func ("/libtracker-common/tracker-type-utils/uint_to_string",
	                 test_uint_to_string);
	g_test_add_func ("/libtracker-common/tracker-type-utils/int_to_string",
	                 test_int_to_string);
	g_test_add_func ("/libtracker-common/tracker-type-utils/long_to_string",
	                 test_long_to_string);
	g_test_add_func ("/libtracker-common/tracker-type-utils/date_to_string",
	                 test_date_to_string);
	g_test_add_func ("/libtracker-common/tracker-type-utils/string_to_date",
	                 test_string_to_date);
	result = g_test_run ();

	return result;
}
