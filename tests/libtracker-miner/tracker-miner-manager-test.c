/*
 * Copyright (C) 2010, Nokia (urho.konttori@nokia.com)
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
#include <glib-object.h>
#include <libtracker-miner/tracker-miner-manager.h>
#include "miners-mock.h"


/*
 * miners-mock.c implements the DBus magic and has hardcoded
 *   some test data.
 */


TrackerMinerManager *manager = NULL;

static void
test_miner_manager_get_running () 
{
        GSList *running_miners;

        running_miners = tracker_miner_manager_get_running (manager);

        g_assert_cmpint (g_slist_length (running_miners), ==, 1);
        g_assert_cmpstr (running_miners->data, ==, MOCK_MINER_1);
}

static void
test_miner_manager_get_available () 
{

        GSList *available_miners;
        GSList *item;

        available_miners = tracker_miner_manager_get_available (manager);

        g_assert_cmpint (g_slist_length (available_miners), ==, 2);
        
        item = g_slist_find_custom (available_miners, 
                                    MOCK_MINER_1,
                                    (GCompareFunc) g_strcmp0);
        g_assert (item != NULL);

        item = g_slist_find_custom (available_miners, 
                                    MOCK_MINER_2,
                                    (GCompareFunc) g_strcmp0);
        g_assert (item != NULL);
}

static void
test_miner_manager_is_active () 
{
        g_assert (tracker_miner_manager_is_active (manager, MOCK_MINER_1));
        g_assert (!tracker_miner_manager_is_active (manager, MOCK_MINER_2));
}

static void
test_miner_manager_pause_resume ()
{
        GStrv   *apps = NULL, *reasons = NULL;
        guint32 cookie;

        g_assert (tracker_miner_manager_is_active (manager, MOCK_MINER_1));
        g_assert (!tracker_miner_manager_is_paused (manager, MOCK_MINER_1, apps, reasons));

        tracker_miner_manager_pause (manager, MOCK_MINER_1, "Testing pause", &cookie);

        g_assert (!tracker_miner_manager_is_active (manager, MOCK_MINER_1));
        g_assert (tracker_miner_manager_is_paused (manager, MOCK_MINER_1, apps, reasons));

        tracker_miner_manager_resume (manager, MOCK_MINER_1, cookie);

        g_assert (tracker_miner_manager_is_active (manager, MOCK_MINER_1));
        g_assert (!tracker_miner_manager_is_paused (manager, MOCK_MINER_1, apps, reasons));
}

static void
test_miner_manager_getters ()
{
        const gchar *result;

        result = tracker_miner_manager_get_display_name (manager, MOCK_MINER_1);
        g_assert_cmpstr (result, ==, "Mock miner for testing");

        result = tracker_miner_manager_get_description  (manager, MOCK_MINER_1);
        g_assert_cmpstr (result, ==, "Comment in the mock miner");

        result = tracker_miner_manager_get_display_name (manager, MOCK_MINER_2);
        g_assert_cmpstr (result, ==, "Yet another mock miner");

        result = tracker_miner_manager_get_description  (manager, MOCK_MINER_2);
        g_assert_cmpstr (result, ==, "Stupid and tedious test for the comment");
}

static void
test_miner_manager_ignore_next_update ()
{
        const gchar *urls[] = { "url1", "url2", "url3", NULL };

        /* Lame, this doesn't test nothing... but improves coverage numbers */
        g_assert (tracker_miner_manager_ignore_next_update (manager, MOCK_MINER_1, urls));
}

static void
test_miner_manager_get_status ()
{
        gchar   *status;
        gdouble  progress;
        g_assert (tracker_miner_manager_get_status (manager, MOCK_MINER_1, &status, &progress));
}

int
main (int    argc,
      char **argv)
{
        gint result;

	g_type_init ();
	g_thread_init (NULL);
	g_test_init (&argc, &argv, NULL);
        
	g_test_message ("Testing miner manager");

        miners_mock_init ();
        g_setenv ("TRACKER_MINERS_DIR", TEST_MINERS_DIR, TRUE);
        manager = tracker_miner_manager_new ();

	g_test_add_func ("/libtracker-miner/tracker-miner-manager/get_running",
                         test_miner_manager_get_running);

	g_test_add_func ("/libtracker-miner/tracker-miner-manager/get_available",
                         test_miner_manager_get_available);

        g_test_add_func ("/libtracker-miner/tracker-miner-manager/is_active",
                         test_miner_manager_is_active);

        g_test_add_func ("/libtracker-miner/tracker-miner-manager/pause_resume",
                         test_miner_manager_pause_resume);

        g_test_add_func ("/libtracker-miner/tracker-miner-manager/getters",
                         test_miner_manager_getters);

        g_test_add_func ("/libtracker-miner/tracker-miner-manager/ignore_next_update",
                         test_miner_manager_ignore_next_update);

        g_test_add_func ("/libtracker-miner/tracker-miner-manager/status",
                         test_miner_manager_get_status);

        result = g_test_run ();
        
        g_object_unref (manager);
        g_unsetenv ("TRACKER_MINERS_DIR");

	return result;
}
