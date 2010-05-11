/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
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

#include <time.h>
#include <string.h>

#include <glib-object.h>

#include <libtracker-common/tracker-date-time.h>
#include <libtracker-extract/tracker-utils.h>

#include <tracker-test-helpers.h>

static void
test_guess_date (void)
{
	gchar *result;

	result = tracker_date_guess ("");
	g_assert (result == NULL);

	result = tracker_date_guess ("2008-06-14");
	g_assert (tracker_test_helpers_cmpstr_equal (result, "2008-06-14T00:00:00"));
	g_free (result);

	result = tracker_date_guess ("20080614000000");
	g_assert (tracker_test_helpers_cmpstr_equal (result, "2008-06-14T00:00:00"));
	g_free (result);

	result = tracker_date_guess ("20080614000000Z");
	g_assert (tracker_test_helpers_cmpstr_equal (result, "2008-06-14T00:00:00Z"));
	g_free (result);

	result = tracker_date_guess ("Mon Jun 14 04:20:20 2008"); /* MS Office */
	g_assert (tracker_test_helpers_cmpstr_equal (result, "2008-06-14T04:20:20"));
	g_free (result);

	result = tracker_date_guess ("2008:06:14 04:20:20"); /* Exif style */
	g_assert (tracker_test_helpers_cmpstr_equal (result, "2008-06-14T04:20:20"));
	g_free (result);

        result = tracker_date_guess ("2010");
        g_assert (tracker_test_helpers_cmpstr_equal (result, "2010-01-01T00:00:00Z"));
        g_free (result);

        result = tracker_date_guess ("A2010");
        g_assert (!result);

        /* Guessing from the code */
        result = tracker_date_guess ("20100318010203-00:03Z");
        g_assert (tracker_test_helpers_cmpstr_equal (result, "2010-03-18T01:02:03-00:03"));
        g_free (result);

        result = tracker_date_guess ("20100318010203+00:03Z");
        g_assert (tracker_test_helpers_cmpstr_equal (result, "2010-03-18T01:02:03+00:03"));
        g_free (result);

        /* "YYYY-MM-DDThh:mm:ss.ff+zz:zz" */
        result = tracker_date_guess ("2010-03-18T01:02:03.10-00:03");
        g_assert (tracker_test_helpers_cmpstr_equal (result, "2010-03-18T01:02:03-00:03"));
        g_free (result);


	if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR)) {
		result = tracker_date_guess (NULL);
		g_free (result);
	}

	g_test_trap_assert_failed ();
}

static void
test_text_validate_utf8 ()
{
	GString *s = NULL;
	gsize    utf8_len = 0;
	gint i;
	gboolean result;
	const gchar *valid_utf8[] = {
		"GNU's not Unix",
		"abcdefghijklmnopqrstuvwxyz0123456789?!,.-_",
		"\xF0\x90\x80\x80",
		"\x41" "\xCE\xA9" "\xE8\xAA\x9E" "\xF0\x90\x8E\x84",
		NULL
	};

	/* If text is empty, FALSE should be returned */
	result = tracker_text_validate_utf8 ("", 0, NULL, NULL);
	g_assert_cmpuint (result, ==, 0);

	/* If a fully valid UTF-8 string is given as input, output
	 *  length should be equal to the input length, and the
	 *  output GString should contain exactly the same contents
	 *  as the input */
	for (i = 0; valid_utf8[i] != NULL; i++) {
		s = NULL;
		utf8_len = 0;
		result = tracker_text_validate_utf8 (valid_utf8[i],
		                                     strlen (valid_utf8[i]),
		                                     &s,
		                                     &utf8_len);
		g_assert_cmpuint (result, ==, 1);
		g_assert_cmpuint (utf8_len, ==, strlen (valid_utf8[i]));
		g_assert (s);
		g_assert_cmpuint (s->len, ==, strlen (valid_utf8[i]));
		g_assert_cmpstr (s->str, ==, valid_utf8[i]);
		g_string_free (s, TRUE);
	}

	/* Same as previous, passing -1 as input text length */
	for (i = 0; valid_utf8[i] != NULL; i++) {
		s = NULL;
		utf8_len = 0;
		result = tracker_text_validate_utf8 (valid_utf8[i],
		                                     -1,
		                                     &s,
		                                     &utf8_len);
		g_assert_cmpuint (result, ==, 1);
		g_assert_cmpuint (utf8_len, ==, strlen (valid_utf8[i]));
		g_assert (s);
		g_assert_cmpuint (s->len, ==, strlen (valid_utf8[i]));
		g_assert_cmpstr (s->str, ==, valid_utf8[i]);
		g_string_free (s, TRUE);
	}

	/* Same as previous, only wanting output text */
	for (i = 0; valid_utf8[i] != NULL; i++) {
		s = NULL;
		result = tracker_text_validate_utf8 (valid_utf8[i],
		                                     -1,
		                                     &s,
		                                     NULL);
		g_assert_cmpuint (result, ==, 1);
		g_assert (s);
		g_assert_cmpuint (s->len, ==, strlen (valid_utf8[i]));
		g_assert_cmpstr (s->str, ==, valid_utf8[i]);
		g_string_free (s, TRUE);
	}

	/* Same as previous, only wanting number of valid UTF-8 bytes */
	for (i = 0; valid_utf8[i] != NULL; i++) {
		utf8_len = 0;
		result = tracker_text_validate_utf8 (valid_utf8[i],
		                                     -1,
		                                     NULL,
		                                     &utf8_len);
		g_assert_cmpuint (result, ==, 1);
		g_assert_cmpuint (utf8_len, ==, strlen (valid_utf8[i]));
	}

	/* If the input string starts with non-valid UTF-8 already, FALSE
	 *  should be returned */
	result = tracker_text_validate_utf8 ("\xF0\x90\x80" "a", -1, NULL, NULL);
	g_assert_cmpuint (result, ==, 0);

	/* If the input string suddenly has some non-valid UTF-8 bytes,
	 *  TRUE should be returned, and the outputs should contain only info
	 *  about the valid first chunk of UTF-8 bytes */
	s = NULL;
	utf8_len = 0;
	result = tracker_text_validate_utf8 ("abcdefghijk" "\xF0\x90\x80" "a",
	                                     -1,
	                                     &s,
	                                     &utf8_len);
	g_assert_cmpuint (result, ==, 1);
	g_assert_cmpuint (utf8_len, ==, strlen ("abcdefghijk"));
	g_assert (s);
	g_assert_cmpuint (s->len, ==, strlen ("abcdefghijk"));
	g_assert_cmpstr (s->str, ==, "abcdefghijk");
	g_string_free (s, TRUE);
}

static void
test_date_to_iso8601 ()
{
        /* Not much to test here because it uses strptime/strftime */
        gchar *result;

        result = tracker_date_format_to_iso8601 ("2010:03:13 12:12:12", "%Y:%m:%d %H:%M:%S");
        g_assert (g_str_has_prefix (result, "2010-03-13T12:12:12"));
        g_assert_cmpint (strlen (result), <=, 25);

        /* Pattern and string don't match */
        result = tracker_date_format_to_iso8601 ("2010:03:13 12:12", "%Y:%m:%d %H:%M:%S");
        g_assert (result == NULL);
}

int
main (int argc, char **argv)
{
	gint result;

	g_type_init ();
	g_test_init (&argc, &argv, NULL);

	g_test_add_func ("/libtracker-extract/tracker-utils/guess_date",
	                 test_guess_date);

        g_test_add_func ("/libtracker-extract/tracker-utils/text-validate-utf8",
                         test_text_validate_utf8);

        g_test_add_func ("/libtracker-extract/tracker-utils/date_to_iso8601",
                         test_date_to_iso8601);

	result = g_test_run ();

	return result;
}
