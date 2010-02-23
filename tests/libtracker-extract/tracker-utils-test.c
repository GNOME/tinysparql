/*
 * Copyright (C) 2009, Nokia
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

	result = tracker_extract_guess_date ("");
	g_assert (result == NULL);

	result = tracker_extract_guess_date ("2008-06-14");
	g_assert (tracker_test_helpers_cmpstr_equal (result, "2008-06-14T00:00:00"));
	g_free (result);

	result = tracker_extract_guess_date ("20080614000000");
	g_assert (tracker_test_helpers_cmpstr_equal (result, "2008-06-14T00:00:00"));
	g_free (result);

	result = tracker_extract_guess_date ("20080614000000Z");
	g_assert (tracker_test_helpers_cmpstr_equal (result, "2008-06-14T00:00:00Z"));
	g_free (result);

	result = tracker_extract_guess_date ("Mon Jun 14 04:20:20 2008"); /* MS Office */
	g_assert (tracker_test_helpers_cmpstr_equal (result, "2008-06-14T04:20:20"));
	g_free (result);

	result = tracker_extract_guess_date ("2008:06:14 04:20:20"); /* Exif style */
	g_assert (tracker_test_helpers_cmpstr_equal (result, "2008-06-14T04:20:20"));
	g_free (result);

	if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR)) {
		result = tracker_extract_guess_date (NULL);
	}

	g_test_trap_assert_failed ();
}

int
main (int argc, char **argv)
{
	gint result;

	g_type_init ();
	g_test_init (&argc, &argv, NULL);

	g_test_add_func ("/libtracker-extract/tracker-utils/guess_date",
	                 test_guess_date);

	result = g_test_run ();

	return result;
}
