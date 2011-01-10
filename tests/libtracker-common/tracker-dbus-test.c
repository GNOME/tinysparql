/*
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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

#include <stdlib.h>

#include <glib.h>
#include <gio/gio.h>

#include <libtracker-common/tracker-dbus-glib.h>

#include <tracker-test-helpers.h>

static void
slist_to_strv (gboolean utf8)
{
	GSList *input = NULL;
	gint    i;
	gchar **input_as_strv;
	gint    strings = 5;

	for (i = 0; i < strings; i++) {
		if (utf8) {
			input = g_slist_prepend (input, g_strdup_printf ("%d", i));
		} else {
			input = g_slist_prepend (input, g_strdup (tracker_test_helpers_get_nonutf8 ()));
		}
	}
	g_assert_cmpint (g_slist_length (input), ==, strings);

	if (utf8) {
		input_as_strv = tracker_dbus_slist_to_strv (input);

		g_assert_cmpint (g_strv_length (input_as_strv), ==, (utf8 ? strings : 0));
		g_strfreev (input_as_strv);
	} else {
		if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR)) {
			input_as_strv = tracker_dbus_slist_to_strv (input);
			g_strfreev (input_as_strv);
		}
		/* Error message:
		 *   Could not add string:'/invalid/file/\xe4\xf6\xe590808.' to GStrv, invalid UTF-8
		 */
		g_test_trap_assert_stderr ("*Could not add string:*");
	}

	g_slist_foreach (input, (GFunc) g_free, NULL);
	g_slist_free (input);
}

static void
test_slist_to_strv (void)
{
	slist_to_strv (TRUE);
}

#if 0

static void
test_slist_to_strv_nonutf8 (void)
{
	slist_to_strv (FALSE);
}

#endif

#if 0

static void
test_async_queue_to_strv_nonutf8 (void)
{
	async_queue_to_strv (FALSE);
}

#endif

static void
test_dbus_request_failed (void)
{
	TrackerDBusRequest *request;
	GError *error = NULL;

	/* For some (unknown) reason, this calls don't appear in the
	 * coverage evaluation. */

	request = tracker_dbus_request_begin ("tracker-dbus-test.c",
	                                      "%s()",
	                                      __PRETTY_FUNCTION__);

	/* We have already the error and want only the log line */
	error = g_error_new (1000, -1, "The indexer founded an error");
	if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR)) {
		tracker_dbus_request_end (request, error);
	}

	g_test_trap_assert_stderr ("*The indexer founded an error*");
	g_error_free (error);
}

static void
test_dbus_request ()
{
	TrackerDBusRequest *request;

	tracker_dbus_enable_client_lookup (FALSE);

	/* Checking the logging output */
	if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDOUT)) {
		request = tracker_dbus_request_begin ("tracker-dbus-test.c",
		                                      "Test request (%s))",
		                                      "--TestNewOK--");
		exit (0);
	}

	g_test_trap_assert_passed ();
	g_test_trap_assert_stdout ("*TestNewOK*");

	request = tracker_dbus_request_begin ("tracker-dbus-test.c",
	                                      "Test request (%s))",
	                                      "--TestNewOK--");

	if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR)) {
		tracker_dbus_request_comment (request,
		                              "Well (%s)",
		                              "--TestCommentOK--");
		exit (0);
	}

	g_test_trap_assert_passed ();
	g_test_trap_assert_stderr ("*TestCommentOK*");

	if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDOUT)) {
		tracker_dbus_request_end (request, NULL);
		exit (0);
	}

	g_test_trap_assert_passed ();
	g_test_trap_assert_stdout ("*Success*");
}

static void
test_dbus_request_client_lookup ()
{
	TrackerDBusRequest *request;

	tracker_dbus_enable_client_lookup (TRUE);


	/* Checking the logging output */
	if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDOUT)) {
		request = tracker_dbus_request_begin ("tracker-dbus-test.c",
		                                      "Test request (%s))",
		                                      "--TestNewOK--");
		exit (0);
	}

	g_test_trap_assert_passed ();
	g_test_trap_assert_stdout ("*TestNewOK*");
	g_test_trap_assert_stdout ("*lt-tracker-dbus*");

	request = tracker_dbus_request_begin ("tracker-dbus-test.c",
	                                      "Test request (%s))",
	                                      "--TestNewOK--");

	if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR)) {
		tracker_dbus_request_comment (request,
		                              "Well (%s)",
		                              "--TestCommentOK--");
		exit (0);
	}

	g_test_trap_assert_passed ();
	g_test_trap_assert_stderr ("*TestCommentOK*");

	if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDOUT)) {
		tracker_dbus_request_info (request,
		                           "Test info %s",
		                           "--TestInfoOK--");
		exit (0);
	}

	g_test_trap_assert_passed ();
	g_test_trap_assert_stdout ("*TestInfoOK*");

	if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDOUT)) {
		tracker_dbus_request_debug (request,
		                            "Test debug %s",
		                            "--TestDebugOK--");
		exit (0);
	}

	g_test_trap_assert_passed ();
	g_test_trap_assert_stdout ("*TestDebugOK*");

	if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDOUT)) {
		tracker_dbus_request_end (request, NULL);
		exit (0);
	}

	g_test_trap_assert_passed ();
	g_test_trap_assert_stdout ("*Success*");

	/* Force client shutdown */
	tracker_dbus_enable_client_lookup (FALSE);
}

static void
test_dbus_request_client_lookup_monothread ()
{
	/* Run everything in the same fork to check the clients_shutdown code */
	if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDOUT | G_TEST_TRAP_SILENCE_STDERR)) {
		TrackerDBusRequest *request;

		tracker_dbus_enable_client_lookup (TRUE);
		request = tracker_dbus_request_begin ("tracker-dbus-test.c",
		                                      "Test request (%s))",
		                                      "--TestNewOK--");
		tracker_dbus_request_comment (request,
		                              "Well (%s)",
		                              "--TestCommentOK--");
/*
		tracker_dbus_request_end (request, NULL,
		                          "--TestFailedOK--");
		tracker_quark = tracker_dbus_error_quark ();
		error = g_error_new (tracker_quark, -1, "test_using_g_error");
		tracker_dbus_request_end (tracker_quark, error);
*/

		tracker_dbus_request_end (request, NULL);
		/* Force client shutdown */
		tracker_dbus_enable_client_lookup (FALSE);
		exit (0);
	}

	g_test_trap_assert_passed ();
}

static void
test_dbus_request_failed_coverage ()
{
	/* Repeat the failed test case in one thread to get coverage */
	if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDOUT | G_TEST_TRAP_SILENCE_STDERR)) {
		GQuark tracker_quark;
		GError *error = NULL;
		TrackerDBusRequest *request;

		tracker_dbus_enable_client_lookup (TRUE);

		/* Using GError */
		request = tracker_dbus_request_begin ("tracker-dbus-test.c",
		                                      "Test request (%s))",
		                                      "--TestNewOK--");
		tracker_quark = tracker_dbus_error_quark ();
		error = g_error_new (tracker_quark, -1, "test_using_g_error");
		tracker_dbus_request_end (request, error);

		request = tracker_dbus_request_begin ("tracker-dbus-test.c",
		                                      "Test request (%s))",
		                                      "--TestNewOK--");
		tracker_dbus_request_end (request, NULL);

		/* Force client shutdown */
		tracker_dbus_enable_client_lookup (FALSE);
		exit (0);
	}

	g_test_trap_assert_passed ();
}

int
main (int argc, char **argv) {

	gint result;

	g_type_init ();
	g_thread_init (NULL);
	g_test_init (&argc, &argv, NULL);

/*
	Disabled non-UTF-8 tests to not break test report generation

	g_test_add_func ("/libtracker-common/tracker-dbus/slist_to_strv_nonutf8",
	                 test_slist_to_strv_nonutf8);
	g_test_add_func ("/libtracker-common/tracker-dbus/async_queue_to_strv_nonutf8",
	                 test_async_queue_to_strv_nonutf8);
*/

	g_test_add_func ("/libtracker-common/tracker-dbus/slist_to_strv_ok", 
	                 test_slist_to_strv);
	g_test_add_func ("/libtracker-common/tracker-dbus/request",
	                 test_dbus_request);
	g_test_add_func ("/libtracker-common/tracker-dbus/request-client-lookup",
	                 test_dbus_request_client_lookup);
	g_test_add_func ("/libtracker-common/tracker-dbus/request-client-lookup",
	                 test_dbus_request_client_lookup_monothread);
	g_test_add_func ("/libtracker-common/tracker-dbus/request_failed",
	                 test_dbus_request_failed);
	g_test_add_func ("/libtracker-common/tracker-dbus/request_failed_coverage",
	                 test_dbus_request_failed_coverage);

	result = g_test_run ();

	tracker_test_helpers_free_nonutf8 ();

	return result;
}
