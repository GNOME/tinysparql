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

#include <glib.h>
#include <gio/gio.h>

#include <libtracker-common/tracker-file-utils.h>
#include <libtracker-common/tracker-locale.h>

#include <tracker-test-helpers.h>

static GSList *
array_as_list (const gchar **array)
{
	gint i;
	GSList *result = NULL;

	for (i = 0; array[i] != NULL; i++) {
		result = g_slist_prepend (result, g_strdup(array[i]));

	}

	return result;
}

static gboolean
string_in_list (GSList *list, const gchar *string)
{
	GSList *it;
	for ( it = list; it != NULL; it = it->next) {
		if (strcmp (it->data, string) == 0) {
			return TRUE;
		}
	}
	return FALSE;
}

static void
test_path_list_filter_duplicates (void)
{
	const gchar *input_roots [] = {"/home", "/home/ivan", "/tmp", "/usr/", "/usr/share/local", NULL};

	GSList *input_as_list = NULL;
	GSList *result;

	input_as_list = array_as_list (input_roots);

	result = tracker_path_list_filter_duplicates (input_as_list, ".", TRUE);
	g_assert_cmpint (3, ==, g_slist_length (result));

	g_assert (string_in_list (result, "/home"));
	g_assert (string_in_list (result, "/tmp"));
	g_assert (string_in_list (result, "/usr"));

	g_slist_foreach (input_as_list, (GFunc) g_free, NULL);
}

static void
test_path_evaluate_name (void)
{
	gchar *result, *expected;

	const gchar *home = g_getenv ("HOME");
	const gchar *pwd = g_getenv ("PWD");

	const gchar *test = "/one/two";
	gchar *parent_dir;

	g_setenv ("TEST_TRACKER_DIR", test, TRUE);


	result = tracker_path_evaluate_name ("/home/user/all/ok");
	tracker_test_helpers_cmpstr_equal (result, "/home/user/all/ok");
	g_free (result);

	/* The result of this test and the next one are not consistent!
	 * Must it remove the end '/' or not?
	 */
	result = tracker_path_evaluate_name ("/home/user/all/dir/");
	tracker_test_helpers_cmpstr_equal (result, "/home/user/all/dir");
	g_free (result);


	/*
	 * TODO: In valgrind this test shows a memory leak
	 */
	result = tracker_path_evaluate_name ("~/all/dir/");
	expected = g_build_path (G_DIR_SEPARATOR_S, home, "/all/dir/", NULL);
	tracker_test_helpers_cmpstr_equal (result, expected);

	g_free (result);
	g_free (expected);

	result = tracker_path_evaluate_name ("$HOME/all/dir/");
	expected = g_build_path (G_DIR_SEPARATOR_S, home, "/all/dir", NULL);
	tracker_test_helpers_cmpstr_equal (result, expected);

	g_free (result);
	g_free (expected);

	result = tracker_path_evaluate_name ("${HOME}/all/dir/");
	expected = g_build_path (G_DIR_SEPARATOR_S, home, "/all/dir", NULL);
	tracker_test_helpers_cmpstr_equal (result, expected);

	g_free (result);
	g_free (expected);

	result = tracker_path_evaluate_name ("./test/current/dir");
	expected = g_build_path (G_DIR_SEPARATOR_S, pwd, "/test/current/dir", NULL);
	tracker_test_helpers_cmpstr_equal (result, expected);

	g_free (result);
	g_free (expected);

	result = tracker_path_evaluate_name ("$TEST_TRACKER_DIR/test/dir");
	expected = g_build_path (G_DIR_SEPARATOR_S, test, "/test/dir", NULL);
	tracker_test_helpers_cmpstr_equal (result, expected);

	g_free (result);
	g_free (expected);

	result = tracker_path_evaluate_name ("../test/dir");
	parent_dir = g_path_get_dirname (pwd);
	expected = g_build_path (G_DIR_SEPARATOR_S, parent_dir, "/test/dir", NULL);
	tracker_test_helpers_cmpstr_equal (result, expected);

	g_free (result);
	g_free (parent_dir);
	g_free (expected);

	result = tracker_path_evaluate_name ("");
	g_assert (!result);

	result = tracker_path_evaluate_name (NULL);
	g_assert (!result);

	result = tracker_path_evaluate_name (tracker_test_helpers_get_nonutf8 ());
	tracker_test_helpers_cmpstr_equal (result,
	                                   tracker_test_helpers_get_nonutf8 ());

	g_unsetenv ("TEST_TRACKER_DIR");
}


static void
test_file_get_mime_type (void)
{
	gchar *dir_name, *result;
	GFile *dir;

	/* Create test directory */
	dir_name = g_build_filename (g_get_tmp_dir (), "tracker-test", NULL);
	dir = g_file_new_for_path (dir_name);
	g_file_make_directory (dir, NULL, NULL);

	result = tracker_file_get_mime_type (dir);

	g_assert (tracker_test_helpers_cmpstr_equal (result, "inode/directory"));

	/* Remove test directory */
	g_file_delete (dir, NULL, NULL);
	g_object_unref (dir);
	g_free (dir_name);
}

int
main (int argc, char **argv)
{
	int result;

	g_type_init ();
	g_test_init (&argc, &argv, NULL);

	tracker_locale_init ();

	g_test_add_func ("/tracker/libtracker-common/tracker-file-utils/path_evaluate_name",
	                 test_path_evaluate_name);

	g_test_add_func ("/tracker/libtracker-common/tracker-file-utils/path_list_filter_duplicates",
	                 test_path_list_filter_duplicates);

	g_test_add_func ("/tracker/libtracker-common/tracker-file-utils/file_get_mime_type",
	                 test_file_get_mime_type);

	result = g_test_run ();

	tracker_locale_shutdown ();

	return result;
}
