/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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
#include <glib.h>
#include <gio/gio.h>

#include <libtracker-db/tracker-db-file-info.h>
#include <libtracker-db/tracker-db-action.h>
#include "tracker-test-helpers.h"

static void
test_ref_counting ()
{
        TrackerDBFileInfo *info;
        TrackerDBFileInfo *other;
        TrackerDBFileInfo *third;
        GFile             *f;
        gchar             *abs_path;

        f = g_file_new_for_path ("./data/plain-file.txt");
        abs_path = g_file_get_path (f);

        /* Create one file-info */
        info = tracker_db_file_info_new (abs_path,
                                         TRACKER_DB_ACTION_FILE_CHECK,
                                         1,
                                         TRACKER_DB_WATCH_OTHER);
        g_assert_cmpint (info->ref_count, ==, 1);

        /* A reference to it */
        other = tracker_db_file_info_ref (info);
        g_assert_cmpint (info->ref_count, ==, 2);
        g_assert_cmpint (info->ref_count, ==, 2);

        /* Unref the second ref */
        third = tracker_db_file_info_unref (other);
        g_assert (third == info);
        g_assert_cmpint (info->ref_count, ==, 1);

        /* Unref the original one */
        third = tracker_db_file_info_unref (info);
        g_assert (!third);

        /* Try double unref */
        tracker_db_file_info_unref (info);

        g_free (abs_path);
        g_object_unref (f);
}

static void
test_get ()
{
        TrackerDBFileInfo *info;
        TrackerDBFileInfo *populated;
        GFile             *f;
        gchar             *abs_path, *link_path, *dir_path;
        gchar             *realfile_basename;

        /* Regular file */
        f = g_file_new_for_path ("./data/plain-file.txt");
        abs_path = g_file_get_path (f);
        realfile_basename = g_file_get_basename (f);

        info = tracker_db_file_info_new (abs_path,
                                         TRACKER_DB_ACTION_FILE_CHECK,
                                         1,
                                         TRACKER_DB_WATCH_OTHER);
        
        populated = tracker_db_file_info_get (info);
        g_assert (!populated->is_directory);
        g_assert (!populated->is_link);
        g_assert_cmpint (populated->file_size, ==, 0);

        tracker_db_file_info_unref (populated);
        tracker_db_file_info_unref (info);
        g_object_unref (f);
        g_free (abs_path);
        /* realfile_basename is needed to test links */
        

        /* Link */
        f = g_file_new_for_path ("./data/link-to-file");
        link_path = g_file_get_path (f);

        info = tracker_db_file_info_new (link_path,
                                         TRACKER_DB_ACTION_FILE_CHECK,
                                         1,
                                         TRACKER_DB_WATCH_OTHER);
        
        populated = tracker_db_file_info_get (info);
        g_assert (!populated->is_directory);
        g_assert (populated->is_link);
        g_assert_cmpstr (populated->link_name, ==, realfile_basename);
        /* The link_path is the relative path to the link end */
        g_assert_cmpstr (populated->link_path, ==, ".");

        tracker_db_file_info_unref (info);
        tracker_db_file_info_unref (populated);
        g_object_unref (f);
        g_free (link_path);
        g_free (realfile_basename);



        /* Directory */
        f = g_file_new_for_path ("./data");
        dir_path = g_file_get_path (f);
        info = tracker_db_file_info_new (dir_path,
                                         TRACKER_DB_ACTION_DIRECTORY_CHECK,
                                         1,
                                         TRACKER_DB_WATCH_OTHER);
        populated = tracker_db_file_info_get (info);
        g_assert (populated->is_directory);
        g_assert (!populated->is_link);

        tracker_db_file_info_unref (info);
        tracker_db_file_info_unref (populated);
        g_free (dir_path);
        g_object_unref (f);

        /* Unexistent file */
        f = g_file_new_for_path ("./data/dont-create-a-file-with-this-name");
        abs_path = g_file_get_path (f);
        info = tracker_db_file_info_new (abs_path,
                                         TRACKER_DB_ACTION_FILE_CHECK,
                                         1,
                                         TRACKER_DB_WATCH_OTHER);
        
        populated = tracker_db_file_info_get (info);
        g_assert (!populated->is_directory);
        g_assert (!populated->is_link);
        g_assert_cmpint (populated->file_size, ==, 0);

        tracker_db_file_info_unref (info);
        tracker_db_file_info_unref (populated);
        g_free (abs_path);
        g_object_unref (f);

        /* Non UTF8 string */
        f = g_file_new_for_path (tracker_test_helpers_get_nonutf8 ());
        abs_path = g_file_get_path (f);
        g_print ("%s", abs_path);
        info = tracker_db_file_info_new (abs_path,
                                         TRACKER_DB_ACTION_FILE_CHECK,
                                         1,
                                         TRACKER_DB_WATCH_OTHER);

}

static void
test_watchtype ()
{
        TrackerDBWatch iter;
        
        for (iter = TRACKER_DB_WATCH_ROOT; iter != TRACKER_DB_WATCH_OTHER; iter++) {
                g_assert(tracker_db_watch_to_string (iter));
        }
}

static void
test_free ()
{
        TrackerDBFileInfo *info = NULL;

        tracker_db_file_info_free (info);
        info = tracker_db_file_info_new ("a", 
                                         TRACKER_DB_ACTION_FILE_CHECK,
                                         1, 
                                         TRACKER_DB_WATCH_OTHER);

        tracker_db_file_info_free (info);
}

int
main (int argc, char **argv)
{
	int result;

	g_type_init ();
	g_test_init (&argc, &argv, NULL);

        g_test_add_func ("/libtracker-db/tracker-db-file-info/ref_counting",
                         test_ref_counting);

        g_test_add_func ("/libtracker-db/tracker-db-file-info/get",
                         test_get);

        g_test_add_func ("/libtracker-db/tracker-db-file-info/watchtype",
                         test_watchtype);

        g_test_add_func ("/libtracker-db/tracker-db-file-info/free",
                         test_free);

	result = g_test_run ();

	return result;
}
