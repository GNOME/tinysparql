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

#include <libtracker-common/tracker-dbus.h>

#include <tracker-test-helpers.h>

/* Log handler to use in the trap fork tests; where we make sure to dump to
 * stdout/stderr, regardless of G_MESSAGES_DEBUG being set or not */
static void
log_handler (const gchar    *domain,
             GLogLevelFlags  log_level,
             const gchar    *message,
             gpointer        user_data)
{
	if (log_level & G_LOG_LEVEL_ERROR ||
	    log_level & G_LOG_LEVEL_CRITICAL ||
	    log_level & G_LOG_LEVEL_WARNING ||
	    log_level & G_LOG_LEVEL_MESSAGE)
		g_printerr ("%s\n", message);
	else
		g_print ("%s\n", message);
}

static inline GSList *
slist_to_strv_get_source (gint     strings,
                          gboolean utf8)
{
	GSList *input = NULL;
	gint i;

	for (i = 0; i < strings; i++) {
		if (utf8) {
			input = g_slist_prepend (input, g_strdup_printf ("%d", i));
		} else {
			input = g_slist_prepend (input, g_strdup (tracker_test_helpers_get_nonutf8 ()));
		}
	}

	return input;
}

static void
test_slist_to_strv_failures_subprocess (void)
{
	GSList *input;
	gchar **input_as_strv;

	input = slist_to_strv_get_source (5, FALSE);

	g_log_set_default_handler (log_handler, NULL);
	input_as_strv = tracker_dbus_slist_to_strv (input);
	g_assert_cmpint (g_strv_length (input_as_strv), ==, 0);
	g_strfreev (input_as_strv);

	g_slist_foreach (input, (GFunc) g_free, NULL);
	g_slist_free (input);
}

static void
test_slist_to_strv_failures (void)
{
	g_test_trap_subprocess ("/libtracker-common/tracker-dbus/slist_to_strv_failures/subprocess", 0, 0);

	/* Error message:
	 *   Could not add string:'/invalid/file/\xe4\xf6\xe590808.' to GStrv, invalid UTF-8
	 */
	g_test_trap_assert_passed ();
	g_test_trap_assert_stderr ("*Could not add string:*");
}

static void
test_slist_to_strv (void)
{
	GSList *input;
	gchar **input_as_strv;
	gint strings = 5;

	input = slist_to_strv_get_source (strings, TRUE);
	g_assert_cmpint (g_slist_length (input), ==, strings);

	input_as_strv = tracker_dbus_slist_to_strv (input);

	g_assert_cmpint (g_strv_length (input_as_strv), ==, strings);
	g_strfreev (input_as_strv);

	g_slist_foreach (input, (GFunc) g_free, NULL);
	g_slist_free (input);
}

static void
test_dbus_request_subprocess (void)
{
	TrackerDBusRequest *request;
	GError *error = NULL;

	tracker_dbus_enable_client_lookup (FALSE);

	g_log_set_default_handler (log_handler, NULL);

	/* New request case */
	request = tracker_dbus_request_begin ("tracker-dbus-test.c",
	                                      "Test request (%s))",
	                                      "--TestNewOK--");
	tracker_dbus_request_end (request, NULL);

	/* Comment and success case */
	request = tracker_dbus_request_begin ("tracker-dbus-test.c",
	                                      "Test request (%s))",
	                                      "--TestNewOK--");
	tracker_dbus_request_comment (request,
	                              "Well (%s)",
	                              "--TestCommentOK--");
	tracker_dbus_request_end (request, NULL);

	/* Error case */
	request = tracker_dbus_request_begin ("tracker-dbus-test.c",
	                                      "%s()",
	                                      __PRETTY_FUNCTION__);

	error = g_error_new (1000, -1, "The indexer founded an error");
	tracker_dbus_request_end (request, error);
	g_error_free (error);
}

static void
test_dbus_request (void)
{
	/* Checking the logging output */
	g_test_trap_subprocess ("/libtracker-common/tracker-dbus/request/subprocess", 0, 0);

	g_test_trap_assert_passed ();
	g_test_trap_assert_stdout ("*TestNewOK*");
	g_test_trap_assert_stderr ("*TestCommentOK*");
	g_test_trap_assert_stdout ("*Success*");
	g_test_trap_assert_stderr ("*The indexer founded an error*");
}

int
main (int argc, char **argv) {

	gint result;

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
	g_test_add_func ("/libtracker-common/tracker-dbus/slist_to_strv_failures",
	                 test_slist_to_strv_failures);
	g_test_add_func ("/libtracker-common/tracker-dbus/slist_to_strv_failures/subprocess",
	                 test_slist_to_strv_failures_subprocess);
	g_test_add_func ("/libtracker-common/tracker-dbus/request",
	                 test_dbus_request);
	g_test_add_func ("/libtracker-common/tracker-dbus/request/subprocess",
	                 test_dbus_request_subprocess);
/* port to gdbus first
	 g_test_add_func ("/libtracker-common/tracker-dbus/request-client-lookup",
	                 test_dbus_request_client_lookup);

	g_test_add_func ("/libtracker-common/tracker-dbus/request-client-lookup",
	                 test_dbus_request_client_lookup_monothread);
	g_test_add_func ("/libtracker-common/tracker-dbus/request_failed_coverage",
	                 test_dbus_request_failed_coverage);
*/
	result = g_test_run ();

	tracker_test_helpers_free_nonutf8 ();

	return result;
}
