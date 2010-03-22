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
test_string_in_gslist (void)
{
        GSList *input = NULL;

        input = g_slist_prepend (input, g_strdup ("one"));
        input = g_slist_prepend (input, g_strdup ("two"));
        input = g_slist_prepend (input, g_strdup ("three"));
        input = g_slist_prepend (input, g_strdup ("four"));

        g_assert (tracker_string_in_gslist ("one", input));
        g_assert (tracker_string_in_gslist ("two", input));
        g_assert (tracker_string_in_gslist ("three", input));
        g_assert (tracker_string_in_gslist ("four", input));
        g_assert (!tracker_string_in_gslist ("five", input));

        g_slist_foreach (input, (GFunc)g_free, NULL);
        g_slist_free (input);
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
test_string_list_to_gslist (void)
{
        const gchar  *input [] = {"one", "two", "three", "four", NULL};
        GSList *result = NULL;

        result = tracker_string_list_to_gslist ((gchar **)input, -1);
        g_assert (result);
        g_assert_cmpint (g_slist_length (result), ==, 4);

        /* This function is tested in other test, so it should work or fail there also */
        g_assert (tracker_string_in_gslist ("one", result));
        g_assert (tracker_string_in_gslist ("two", result));
        g_assert (tracker_string_in_gslist ("three", result));
        g_assert (tracker_string_in_gslist ("four", result));

        g_slist_foreach (result, (GFunc)g_free, NULL);
        g_slist_free (result);

        result = tracker_string_list_to_gslist ((gchar **)input, 2);
        g_assert (result);
        g_assert_cmpint (g_slist_length (result), ==, 2);

        g_assert (tracker_string_in_gslist ("one", result));
        g_assert (tracker_string_in_gslist ("two", result));
        g_assert (!tracker_string_in_gslist ("three", result));
        g_assert (!tracker_string_in_gslist ("four", result));

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
test_string_to_string_list (void)
{
        const gchar *input = "first line";
        gchar **result;

        result = tracker_string_to_string_list (input);
        g_assert_cmpint (g_strv_length (result), ==, 1);
        g_assert_cmpstr (result [0], ==, "first line");
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

static void
test_gslist_with_string_data_equal (void)
{
        GSList *list1 = NULL;
        GSList *list2 = NULL;
        GSList *shorty = NULL;
        GSList *list3 = NULL;

        list1 = g_slist_prepend (list1, g_strdup ("one"));
        list1 = g_slist_prepend (list1, g_strdup ("two"));
        list1 = g_slist_prepend (list1, g_strdup ("three"));

        g_assert (tracker_gslist_with_string_data_equal (list1, list1));

        shorty = g_slist_prepend (shorty, g_strdup ("one"));
        g_assert (!tracker_gslist_with_string_data_equal (list1, shorty));
        g_assert (!tracker_gslist_with_string_data_equal (shorty, list1));

        list2 = g_slist_prepend (list2, g_strdup ("one"));
        list2 = g_slist_prepend (list2, g_strdup ("two"));
        list2 = g_slist_prepend (list2, g_strdup ("three"));
        g_assert (tracker_gslist_with_string_data_equal (list1, list2));
        g_assert (tracker_gslist_with_string_data_equal (list2, list1));

        list3 = g_slist_prepend (list3, g_strdup ("one"));
        list3 = g_slist_prepend (list3, g_strdup ("something different"));
        list3 = g_slist_prepend (list3, g_strdup ("three"));
        g_assert (!tracker_gslist_with_string_data_equal (list1, list3));
        g_assert (!tracker_gslist_with_string_data_equal (list3, list1));

        g_slist_foreach (list1, (GFunc)g_free, NULL);
        g_slist_foreach (list2, (GFunc)g_free, NULL);
        g_slist_foreach (shorty, (GFunc)g_free, NULL);
        g_slist_foreach (list3, (GFunc)g_free, NULL);

        g_slist_free (list1);
        g_slist_free (list2);
        g_slist_free (shorty);
        g_slist_free (list3);
}

static void
test_glist_copy_with_string_data (void)
{
        GList *input = NULL; 
        GList *result = NULL;

        input = g_list_prepend (input, g_strdup ("one"));
        input = g_list_prepend (input, g_strdup ("two"));

        result = tracker_glist_copy_with_string_data (input);
        g_assert (result != input);
        g_assert_cmpint (g_list_length (result), ==, 2);

        /* Further checks... that the contents are actually the same */
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
	g_test_add_func ("/libtracker-common/tracker-type-utils/string_list_as_list",
	                 test_string_to_string_list);
	g_test_add_func ("/libtracker-common/tracker-type-utils/gslist_to_string_list",
	                 test_gslist_to_string_list);
	g_test_add_func ("/libtracker-common/tracker-type-utils/string_in_string_list",
	                 test_string_in_string_list);
        g_test_add_func ("/libtracker-common/tracker-type-utils/string_in_gslist",
                         test_string_in_gslist);
        g_test_add_func ("/libtracker-common/tracker-type-utils/string_list_to_gslist",
                         test_string_list_to_gslist);
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
	g_test_add_func ("/libtracker-common/tracker-type-utils/gslist_with_string_data_equal",
	                 test_gslist_with_string_data_equal);
	g_test_add_func ("/libtracker-common/tracker-type-utils/glist_copy_with_string_data",
	                 test_glist_copy_with_string_data);
	result = g_test_run ();

	return result;
}
