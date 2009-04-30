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
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <libtracker-db/tracker-db-index-manager.h>
#include <libtracker-common/tracker-ontology.h>

static const gchar *old_xdg_cache = NULL;
static gchar *test_xdg_cache = NULL;

static void
set_cache_directory ()
{
        GFile *f;

        f = g_file_new_for_path ("./xdg-cache-home");
        test_xdg_cache = g_file_get_path (f);

        old_xdg_cache = g_getenv ("XDG_CACHE_HOME");
        g_setenv ("XDG_CACHE_HOME", test_xdg_cache, TRUE);
}

static void
restore_cache_directory ()
{
        g_setenv ("XDG_CACHE_HOME", old_xdg_cache, TRUE);
        g_free (test_xdg_cache);
}

static void
clean_tmp_test_directory ()
{
        if (g_file_test (test_xdg_cache, G_FILE_TEST_EXISTS)) {
                g_remove (test_xdg_cache);
        }
}

static gchar *
get_expected_file_index () {
        return g_build_filename (g_get_user_cache_dir (),
                                 "tracker",
                                 "file-index.db", NULL);
}

static gchar *
get_expected_email_index () {
        return g_build_filename (g_get_user_cache_dir (),
                                 "tracker",
                                 "email-index.db", NULL);
}

static void
test_init_shutdown ()
{
        gchar *expected_file_index;
        gchar *expected_email_index;

        expected_file_index = get_expected_file_index ();
        expected_email_index = get_expected_email_index ();

        if (g_file_test (test_xdg_cache, G_FILE_TEST_EXISTS)) {
                g_remove (test_xdg_cache);
        }

        /* _shutdown without initializing */
        tracker_db_index_manager_shutdown ();

        /* init and check the indexes are there */
        g_assert (tracker_db_index_manager_init (0, 1, 10000));
        
        g_assert (g_file_test (expected_file_index, G_FILE_TEST_EXISTS));
        g_assert (g_file_test (expected_email_index, G_FILE_TEST_EXISTS));
        tracker_db_index_manager_shutdown ();

        clean_tmp_test_directory ();
        g_free (expected_file_index);
        g_free (expected_email_index);
}

static void
test_remove_all ()
{
        gchar *expected_file_index;
        gchar *expected_email_index;

        expected_file_index = g_build_filename (g_get_user_cache_dir (),
                                                "tracker",
                                                "file-index.db", NULL);
        expected_email_index = g_build_filename (g_get_user_cache_dir (),
                                                 "tracker",
                                                 "email-index.db", NULL);
        tracker_db_index_manager_init (0,1,10000);
        g_assert (g_file_test (expected_file_index, G_FILE_TEST_EXISTS));
        g_assert (g_file_test (expected_email_index, G_FILE_TEST_EXISTS));

        tracker_db_index_manager_remove_all ();
        g_assert (!g_file_test (expected_file_index, G_FILE_TEST_EXISTS));
        g_assert (!g_file_test (expected_email_index, G_FILE_TEST_EXISTS));

        tracker_db_index_manager_shutdown ();

        if (g_file_test (test_xdg_cache, G_FILE_TEST_EXISTS)) {
                g_remove (test_xdg_cache);
        }
        
        g_free (expected_file_index);
        g_free (expected_email_index);
}

/* This function overrides the original tracker_file_get_size
 * to test the index_too_big functionality 
 */
goffset
tracker_file_get_size (const gchar *uri)
{
        gchar *expected_file_index;

        expected_file_index = g_build_filename (g_get_user_cache_dir (),
                                                "tracker",
                                                "file-index.db", NULL);

        if (g_strcmp0 (uri, expected_file_index)) {
                return 2000000001;
        }
        return 1000;
}

static void
test_index_too_big ()
{
        gchar *expected_file_index;

        expected_file_index = get_expected_file_index ();

        tracker_db_index_manager_init (0,1,10000);
        g_assert (g_file_test (expected_file_index, G_FILE_TEST_EXISTS));

        if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR)) {
                tracker_db_index_manager_are_indexes_too_big ();
        }
        g_test_trap_assert_failed ();
        g_test_trap_assert_stderr ("*One or more index files are too big*");
        tracker_db_index_manager_shutdown ();
}

static void
test_get_indexes ()
{
        TrackerDBIndex *files = NULL;
        TrackerDBIndex *emails = NULL;
        TrackerDBIndex *unknown = NULL;
        TrackerDBIndex *result = NULL;

        /* No need to add services in the ontology */
        tracker_ontology_init ();
        tracker_db_index_manager_init (0, 1, 1000);

        files = tracker_db_index_manager_get_index (TRACKER_DB_INDEX_FILE);
        emails = tracker_db_index_manager_get_index (TRACKER_DB_INDEX_EMAIL);
        unknown = tracker_db_index_manager_get_index (TRACKER_DB_INDEX_UNKNOWN);

        result = tracker_db_index_manager_get_index_by_service ("files");
        g_assert (result);
        g_assert (result == files);

        result = tracker_db_index_manager_get_index_by_service ("audio");
        g_assert (result);
        g_assert (result == files);

        result = tracker_db_index_manager_get_index_by_service ("AUDIO");
        g_assert (result);
        g_assert (result == files);

        result = tracker_db_index_manager_get_index_by_service ("emails");
        g_assert (result);
        g_assert (result == emails);

        tracker_db_index_manager_shutdown ();
        tracker_ontology_shutdown ();
}

static void
test_get_filename ()
{
        gchar *expected_file_index = get_expected_file_index ();
        gchar *expected_email_index = get_expected_email_index ();

        tracker_db_index_manager_init (0, 1, 1000);

        g_assert_cmpstr (tracker_db_index_manager_get_filename (TRACKER_DB_INDEX_FILE), 
                         ==, expected_file_index);

        g_assert_cmpstr (tracker_db_index_manager_get_filename (TRACKER_DB_INDEX_EMAIL), 
                         ==, expected_email_index);

        tracker_db_index_manager_shutdown ();
        
        g_free (expected_file_index);
        g_free (expected_email_index);
}

int
main (int argc, char **argv)
{
	int result;

	g_type_init ();
	g_test_init (&argc, &argv, NULL);
        set_cache_directory ();

        g_test_add_func ("/libtracker-db/tracker-db-index-manager/init_shutdown",
                         test_init_shutdown);

        g_test_add_func ("/libtracker-db/tracker-db-index-manager/remove_all",
                         test_remove_all);

        g_test_add_func ("/libtracker-db/tracker-db-index-manager/index_too_big",
                         test_index_too_big);

        g_test_add_func ("/libtracker-db/tracker-db-index-manager/get_indexes",
                         test_get_indexes);

        g_test_add_func ("/libtracker-db/tracker-db-index-manager/get_filename",
                         test_get_filename);
	result = g_test_run ();

        restore_cache_directory ();

	return result;
}
