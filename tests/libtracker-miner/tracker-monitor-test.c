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

#include <sys/types.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gstdio.h>

/* Special case, the monitor header is not normally exported */
#include <libtracker-miner/tracker-monitor.h>

static TrackerMonitor *monitor;
static GFile *file;
static GFile *file_for_tmp;
static gchar *basename;
static gchar *path;

static void
test_monitor_new (void)
{
	gboolean success;

	monitor = tracker_monitor_new ();
	g_assert (monitor != NULL);

	basename = g_strdup_printf ("monitor-test-%d", getpid ());
	path = g_build_path (G_DIR_SEPARATOR_S, g_get_tmp_dir (), basename, NULL);

	success = g_mkdir_with_parents (path, 00755) == 0;
	g_assert_cmpint (success, ==, TRUE);

	file = g_file_new_for_path (path);
	g_assert (G_IS_FILE (file));

	file_for_tmp = g_file_new_for_path (g_get_tmp_dir ());
	g_assert (G_IS_FILE (file_for_tmp));
}

static void
test_monitor_enabled (void)
{
	/* Test general API with monitors enabled first */
	tracker_monitor_set_enabled (monitor, TRUE);
	g_assert_cmpint (tracker_monitor_get_enabled (monitor), ==, TRUE);

	g_assert_cmpint (tracker_monitor_get_count (monitor), ==, 0);
	g_assert_cmpint (tracker_monitor_add (monitor, file), ==, TRUE);
	g_assert_cmpint (tracker_monitor_add (monitor, file), ==, TRUE); /* Test double add on purpose */
	g_assert_cmpint (tracker_monitor_get_count (monitor), ==, 1);
	g_assert_cmpint (tracker_monitor_is_watched (monitor, file), ==, TRUE);
	g_assert_cmpint (tracker_monitor_is_watched_by_string (monitor, path), ==, TRUE);
	g_assert_cmpint (tracker_monitor_remove (monitor, file), ==, TRUE);
	g_assert_cmpint (tracker_monitor_is_watched (monitor, file), ==, FALSE);
	g_assert_cmpint (tracker_monitor_is_watched_by_string (monitor, path), ==, FALSE);
	g_assert_cmpint (tracker_monitor_get_count (monitor), ==, 0);

	tracker_monitor_add (monitor, file);
	tracker_monitor_add (monitor, file_for_tmp);
	g_assert_cmpint (tracker_monitor_get_count (monitor), ==, 2);
	g_assert_cmpint (tracker_monitor_remove_recursively (monitor, file_for_tmp), ==, TRUE);
	g_assert_cmpint (tracker_monitor_get_count (monitor), ==, 0);

	/* Test general API with monitors disabled first */
	tracker_monitor_set_enabled (monitor, FALSE);
	g_assert_cmpint (tracker_monitor_get_enabled (monitor), ==, FALSE);

	g_assert_cmpint (tracker_monitor_get_count (monitor), ==, 0);
	g_assert_cmpint (tracker_monitor_add (monitor, file), ==, TRUE);
	g_assert_cmpint (tracker_monitor_get_count (monitor), ==, 1);
	g_assert_cmpint (tracker_monitor_is_watched (monitor, file), ==, FALSE);
	g_assert_cmpint (tracker_monitor_is_watched_by_string (monitor, path), ==, FALSE);
	g_assert_cmpint (tracker_monitor_remove (monitor, file), ==, TRUE);
	g_assert_cmpint (tracker_monitor_is_watched (monitor, file), ==, FALSE);
	g_assert_cmpint (tracker_monitor_is_watched_by_string (monitor, path), ==, FALSE);
	g_assert_cmpint (tracker_monitor_get_count (monitor), ==, 0);

	tracker_monitor_add (monitor, file);
	tracker_monitor_add (monitor, file_for_tmp);
	g_assert_cmpint (tracker_monitor_get_count (monitor), ==, 2);
	g_assert_cmpint (tracker_monitor_remove_recursively (monitor, file_for_tmp), ==, TRUE);
	g_assert_cmpint (tracker_monitor_get_count (monitor), ==, 0);
}

static void
test_monitor_events (void)
{
	/* TODO */
}

static void
test_monitor_free (void)
{
	g_assert_cmpint (g_rmdir (path), ==, 0);

	g_assert (file_for_tmp != NULL);
	g_object_unref (file_for_tmp);

	g_assert (file != NULL);
	g_object_unref (file);

	g_assert (path != NULL);
	g_free (path);

	g_assert (basename != NULL);
	g_free (basename);

	g_assert (monitor != NULL);
	g_object_unref (monitor);
	monitor = NULL;
}

int
main (int    argc,
      char **argv)
{
	g_type_init ();
	g_thread_init (NULL);
	g_test_init (&argc, &argv, NULL);

	g_test_message ("Testing filesystem monitor");

	g_test_add_func ("/libtracker-miner/tracker-monitor/monitor-new",
	                 test_monitor_new);
	g_test_add_func ("/libtracker-miner/tracker-monitor/monitor-enabled",
	                 test_monitor_enabled);
	g_test_add_func ("/libtracker-miner/tracker-monitor/monitor-events",
	                 test_monitor_events);
	g_test_add_func ("/libtracker-miner/tracker-monitor/monitor-free",
	                 test_monitor_free);

	return g_test_run ();
}
