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

#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <locale.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

#include <tracker-common.h>

#define TEST_FILENAME "./file-utils-test.txt"
#define TEST_HIDDEN_FILENAME "./.hidden-file.txt"

static void
ensure_file_exists (const gchar *filename)
{
	gboolean retval;

        if (!g_file_test (filename, G_FILE_TEST_EXISTS)) {
	        retval = g_file_set_contents (filename, "Just some stuff", -1, NULL);
	        g_assert_true (retval);
        }
}

static void
remove_file (const gchar *filename)
{
        g_assert_true (g_file_test (filename, G_FILE_TEST_EXISTS));
        g_assert_cmpint (g_remove (filename), ==, 0);
}

static void
test_file_utils_get_size ()
{
        goffset size;
        struct stat st;

        size = tracker_file_get_size (TEST_FILENAME);
        g_assert_cmpint (size, >, 0);

        g_assert_cmpint (stat (TEST_FILENAME, &st), ==, 0);
        g_assert_cmpint (size, ==, st.st_size);

        /* File doesn't exist */
        size = tracker_file_get_size ("./file-does-NOT-exist");
        g_assert_cmpint (size, ==, 0);
}

static void
test_file_system_has_enough_space ()
{
        /* Hopefully we will always have 1 byte free... */
        g_assert_true (tracker_file_system_has_enough_space ("/tmp", 1, FALSE));
        g_assert_true (tracker_file_system_has_enough_space ("/tmp", 1, TRUE));

        /* gulong goes only up to 4Gb. Cannot ask for unreasonable amount of space */
        //g_assert_true (!tracker_file_system_has_enough_space ("/home", G_MAXULONG, FALSE));
}

int
main (int argc, char **argv)
{
	int result;

	g_test_init (&argc, &argv, NULL);

	setlocale (LC_ALL, "");

        ensure_file_exists (TEST_FILENAME);
        ensure_file_exists (TEST_HIDDEN_FILENAME);

        g_test_add_func ("/common/file-utils/get_size",
                         test_file_utils_get_size);
        g_test_add_func ("/common/file-utils/has_enough_space",
                         test_file_system_has_enough_space);

	result = g_test_run ();

        remove_file (TEST_FILENAME);
        remove_file (TEST_HIDDEN_FILENAME);

	return result;
}
