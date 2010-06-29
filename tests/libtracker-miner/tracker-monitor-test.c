/*
 * Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gstdio.h>

/* Special case, the monitor header is not normally exported */
#include <libtracker-miner/tracker-monitor.h>

#define ALARM_TIMEOUT 10 /* seconds */

typedef enum {
	MONITOR_SIGNAL_EXPECTED_NONE = 0,
	MONITOR_SIGNAL_EXPECTED_ITEM_CREATED = 1 << 0,
	MONITOR_SIGNAL_EXPECTED_ITEM_UPDATED = 1 << 1,
	MONITOR_SIGNAL_EXPECTED_ITEM_DELETED = 1 << 2,
	MONITOR_SIGNAL_EXPECTED_ITEM_MOVED   = 1 << 3,
} MonitorSignalExpected;

static TrackerMonitor *monitor;
static GFile *file_for_monitor;
static GFile *file_for_tmp;
static GFile *file_for_events;
static GFile *file_for_move_in;
static GFile *file_for_move_out;
static gchar *path_for_monitor;
static gchar *path_for_events;
static gchar *path_for_move_in;
static gchar *path_for_move_out;
static GHashTable *events;
static GMainLoop *main_loop;
static guint8 signals_received;

static void
signal_handler (int signo)
{
	if (g_strsignal (signo)) {
		g_print ("\n");
		g_print ("Received signal:%d->'%s'\n",
		         signo,
		         g_strsignal (signo));
	}

	g_print ("If we got this alarm, we likely didn't get the event we expected in time\n");
	g_assert_cmpint (signo, !=, SIGALRM);
}

static void
initialize_signal_handler (void)
{
#ifndef G_OS_WIN32
	struct sigaction act;
	sigset_t         empty_mask;

	sigemptyset (&empty_mask);
	act.sa_handler = signal_handler;
	act.sa_mask    = empty_mask;
	act.sa_flags   = 0;

	sigaction (SIGALRM, &act, NULL);
#endif /* G_OS_WIN32 */
}

static void
events_wait_and_check (MonitorSignalExpected expected_signal,
                       MonitorSignalExpected unexpected_signal)
{
	/* Set alarm in case we don't get the event */
	alarm (ALARM_TIMEOUT);
	g_debug ("***** Setting ALARM");

	g_assert (main_loop == NULL);

	main_loop = g_main_loop_new (NULL, FALSE);
	g_assert (main_loop != NULL);

	g_main_loop_run (main_loop);
	g_main_loop_unref (main_loop);
	main_loop = NULL;

	g_debug ("Compilation of signals received (CREATED: %s, UPDATED: %s, DELETED: %s, MOVED: %s)",
	         signals_received & MONITOR_SIGNAL_EXPECTED_ITEM_CREATED ? "yes" : "no",
	         signals_received & MONITOR_SIGNAL_EXPECTED_ITEM_UPDATED ? "yes" : "no",
	         signals_received & MONITOR_SIGNAL_EXPECTED_ITEM_DELETED ? "yes" : "no",
	         signals_received & MONITOR_SIGNAL_EXPECTED_ITEM_MOVED ? "yes" : "no");

	/* Fail the test if we didn't get the signal we expected */
	g_assert_cmpuint ((signals_received & expected_signal), >, 0);

	/* Fail the test if we got an unexpected signal */
	g_assert_cmpuint ((signals_received & unexpected_signal), ==, 0);

	/* Clear the received signals */
	signals_received = MONITOR_SIGNAL_EXPECTED_NONE;
}

static void
events_received (void)
{
	/* Cancel alarm */
	alarm (0);
	g_debug ("***** Cancelled ALARM");

	g_assert (main_loop != NULL);
	g_main_loop_quit (main_loop);
}

static void
create_file (const gchar *filename,
             const gchar *contents)
{
	FILE *file;
	size_t length;

	g_assert (filename != NULL);
	g_assert (contents != NULL);

	file = g_fopen (filename, "wb");
	g_assert (file != NULL);

	length = strlen (contents);
	g_assert_cmpint (fwrite (contents, 1, length, file), >=, length);
	g_assert_cmpint (fflush (file), ==, 0);
	g_assert_cmpint (fclose (file), !=, EOF);
}

static void
test_monitor_enabled (void)
{
	/* Test general API with monitors enabled first */
	tracker_monitor_set_enabled (monitor, TRUE);
	g_assert_cmpint (tracker_monitor_get_enabled (monitor), ==, TRUE);

	g_assert_cmpint (tracker_monitor_get_count (monitor), ==, 0);
	g_assert_cmpint (tracker_monitor_add (monitor, file_for_monitor), ==, TRUE);
	g_assert_cmpint (tracker_monitor_add (monitor, file_for_monitor), ==, TRUE); /* Test double add on purpose */
	g_assert_cmpint (tracker_monitor_get_count (monitor), ==, 1);
	g_assert_cmpint (tracker_monitor_is_watched (monitor, file_for_monitor), ==, TRUE);
	g_assert_cmpint (tracker_monitor_is_watched_by_string (monitor, path_for_monitor), ==, TRUE);
	g_assert_cmpint (tracker_monitor_remove (monitor, file_for_monitor), ==, TRUE);
	g_assert_cmpint (tracker_monitor_is_watched (monitor, file_for_monitor), ==, FALSE);
	g_assert_cmpint (tracker_monitor_is_watched_by_string (monitor, path_for_monitor), ==, FALSE);
	g_assert_cmpint (tracker_monitor_get_count (monitor), ==, 0);

	tracker_monitor_add (monitor, file_for_monitor);
	tracker_monitor_add (monitor, file_for_tmp);
	g_assert_cmpint (tracker_monitor_get_count (monitor), ==, 2);
	g_assert_cmpint (tracker_monitor_remove_recursively (monitor, file_for_tmp), ==, TRUE);
	g_assert_cmpint (tracker_monitor_get_count (monitor), ==, 0);

	/* Test general API with monitors disabled first */
	tracker_monitor_set_enabled (monitor, FALSE);
	g_assert_cmpint (tracker_monitor_get_enabled (monitor), ==, FALSE);

	g_assert_cmpint (tracker_monitor_get_count (monitor), ==, 0);
	g_assert_cmpint (tracker_monitor_add (monitor, file_for_monitor), ==, TRUE);
	g_assert_cmpint (tracker_monitor_get_count (monitor), ==, 1);
	g_assert_cmpint (tracker_monitor_is_watched (monitor, file_for_monitor), ==, FALSE);
	g_assert_cmpint (tracker_monitor_is_watched_by_string (monitor, path_for_monitor), ==, FALSE);
	g_assert_cmpint (tracker_monitor_remove (monitor, file_for_monitor), ==, TRUE);
	g_assert_cmpint (tracker_monitor_is_watched (monitor, file_for_monitor), ==, FALSE);
	g_assert_cmpint (tracker_monitor_is_watched_by_string (monitor, path_for_monitor), ==, FALSE);
	g_assert_cmpint (tracker_monitor_get_count (monitor), ==, 0);

	tracker_monitor_add (monitor, file_for_monitor);
	tracker_monitor_add (monitor, file_for_tmp);
	g_assert_cmpint (tracker_monitor_get_count (monitor), ==, 2);
	g_assert_cmpint (tracker_monitor_remove_recursively (monitor, file_for_tmp), ==, TRUE);
	g_assert_cmpint (tracker_monitor_get_count (monitor), ==, 0);
}

static void
test_monitor_file_events (void)
{
	/* Set up environment */
	g_assert_cmpint (g_hash_table_size (events), ==, 0);
	tracker_monitor_set_enabled (monitor, TRUE);
	g_assert_cmpint (tracker_monitor_get_enabled (monitor), ==, TRUE);
	g_assert_cmpint (tracker_monitor_add (monitor, file_for_monitor), ==, TRUE);
	g_assert_cmpint (tracker_monitor_get_count (monitor), ==, 1);

	/* Test CREATED.
	 *  Actually, UPDATED will also probably arrive here. */
	g_debug (">> Testing CREATE");
	create_file (path_for_events, "foo");
	events_wait_and_check (MONITOR_SIGNAL_EXPECTED_ITEM_CREATED,
	                       MONITOR_SIGNAL_EXPECTED_NONE);

	/* Test UPDATE */
	g_debug (">> Testing UPDATE");
	create_file (path_for_events, "bar");
	events_wait_and_check (MONITOR_SIGNAL_EXPECTED_ITEM_UPDATED,
	                       MONITOR_SIGNAL_EXPECTED_NONE);

	/* Test MOVE to (monitored dir).
	 * When moving to a directory also monitored, a MOVE event
	 * must arrive, and no DELETED event. */
	g_debug (">> Testing MOVE to monitored dir");
	g_assert_cmpint (g_rename (path_for_events, path_for_move_in), ==, 0);
	events_wait_and_check (MONITOR_SIGNAL_EXPECTED_ITEM_MOVED,
	                       MONITOR_SIGNAL_EXPECTED_ITEM_DELETED);

	/* Test MOVE back (monitored dir)
	 * When moving from a directory also monitored, a MOVE event
	 * must arrive, and no DELETED event. */
	g_debug (">> Testing MOVE from monitored dir");
	g_assert_cmpint (g_rename (path_for_move_in, path_for_events), ==, 0);
	events_wait_and_check (MONITOR_SIGNAL_EXPECTED_ITEM_MOVED,
	                       MONITOR_SIGNAL_EXPECTED_ITEM_DELETED);

	/* Test MOVE to (not monitored dir)
	 * When moving to a directory NOT monitored, a DELETE event
	 * must arrive, and no MOVE event. */
	g_debug (">> Testing MOVE to NOT monitored dir");
	g_assert_cmpint (g_rename (path_for_events, path_for_move_out), ==, 0);
	events_wait_and_check (MONITOR_SIGNAL_EXPECTED_ITEM_DELETED,
	                       MONITOR_SIGNAL_EXPECTED_ITEM_MOVED);

	/* Test MOVE back (not monitored dir)
	 * When moving from a directory NOT monitored, a CREATE event
	 * must arrive, and no MOVE event.*/
	g_debug (">> Testing MOVE from NOT monitored dir");
	g_assert_cmpint (g_rename (path_for_move_out, path_for_events), ==, 0);
	events_wait_and_check (MONITOR_SIGNAL_EXPECTED_ITEM_CREATED,
	                       MONITOR_SIGNAL_EXPECTED_ITEM_MOVED);

	/* TODO: Add more complex MOVE operations */

	/* Test DELETED */
	g_assert_cmpint (g_unlink (path_for_events), ==, 0);
	events_wait_and_check (MONITOR_SIGNAL_EXPECTED_ITEM_DELETED,
	                       MONITOR_SIGNAL_EXPECTED_NONE);

	/* Clean up */
	g_assert_cmpint (tracker_monitor_remove (monitor, file_for_monitor), ==, TRUE);
}

static void
test_monitor_events_created_cb (TrackerMonitor *monitor,
                                GFile          *file,
                                gboolean        is_directory,
                                gpointer        user_data)
{
	gchar *path;

	g_assert (file != NULL);

	path = g_file_get_path (file);
	g_assert (path != NULL);

	g_debug ("***** '%s' (%s) (CREATED)",
	         path,
	         is_directory ? "DIR" : "FILE");

	signals_received |= MONITOR_SIGNAL_EXPECTED_ITEM_CREATED;

	/* More tests? */

	g_free (path);

	/* g_assert_cmpstr (g_file_get_path (file), ==, g_file_get_path (file_for_events)); */
	if (!is_directory && g_file_equal (file, file_for_events)) {
		events_received ();
	}
}

static void
test_monitor_events_updated_cb (TrackerMonitor *monitor,
                                GFile          *file,
                                gboolean        is_directory,
                                gpointer        user_data)
{
	gchar *path;

	g_assert (file != NULL);

	path = g_file_get_path (file);
	g_assert (path != NULL);

	g_debug ("***** '%s' (%s) (UPDATED)",
	         path,
	         is_directory ? "DIR" : "FILE");

	signals_received |= MONITOR_SIGNAL_EXPECTED_ITEM_UPDATED;

	/* More tests? */

	g_free (path);

	if (!is_directory && g_file_equal (file, file_for_events)) {
		events_received ();
	}
}

static void
test_monitor_events_deleted_cb (TrackerMonitor *monitor,
                                GFile          *file,
                                gboolean        is_directory,
                                gpointer        user_data)
{
	gchar *path;

	g_assert (file != NULL);

	path = g_file_get_path (file);
	g_assert (path != NULL);

	g_debug ("***** '%s' (%s) (DELETED)",
	         path,
	         is_directory ? "DIR" : "FILE");

	signals_received |= MONITOR_SIGNAL_EXPECTED_ITEM_DELETED;

	/* More tests? */

	g_free (path);

	if (!is_directory && g_file_equal (file, file_for_events)) {
		events_received ();
	}
}

static void
test_monitor_events_moved_cb (TrackerMonitor *monitor,
                              GFile          *file,
                              GFile          *other_file,
                              gboolean        is_directory,
                              gboolean        is_source_monitored,
                              gpointer        user_data)
{
	g_assert (file != NULL);

	signals_received |= MONITOR_SIGNAL_EXPECTED_ITEM_MOVED;

	if (!is_source_monitored) {
		if (is_directory) {
			gchar *path;

			path = g_file_get_path (other_file);

			g_debug ("***** Not in store:'?'->'%s' (DIR) (MOVED, source unknown)",
			         path);

			g_free (path);
		}
	} else {
		gchar *path;
		gchar *other_path;

		path = g_file_get_path (file);
		other_path = g_file_get_path (other_file);

		g_debug ("***** '%s'->'%s' (%s) (MOVED)",
		         path,
		         other_path,
		         is_directory ? "DIR" : "FILE");

		/* FIXME: Guessing this soon the queue the event should pertain
		 *        to could introduce race conditions if events from other
		 *        queues for the same files are processed before items_moved,
		 *        Most of these decisions should be taken when the event is
		 *        actually being processed.
		 */

		g_free (other_path);
		g_free (path);
	}

	if (!is_directory &&
	    (g_file_equal (file, file_for_events) ||
	     g_file_equal (other_file, file_for_events))) {
		events_received ();
	}
}

static void
test_monitor_new (void)
{
	gchar *basename;
	gboolean success;

	monitor = tracker_monitor_new ();
	g_assert (monitor != NULL);

	basename = g_strdup_printf ("monitor-test-%d", getpid ());
	path_for_monitor = g_build_path (G_DIR_SEPARATOR_S, g_get_tmp_dir (), basename, NULL);
	g_free (basename);

	success = g_mkdir_with_parents (path_for_monitor, 00755) == 0;
	g_assert_cmpint (success, ==, TRUE);

	file_for_monitor = g_file_new_for_path (path_for_monitor);
	g_assert (G_IS_FILE (file_for_monitor));

	file_for_tmp = g_file_new_for_path (g_get_tmp_dir ());
	g_assert (G_IS_FILE (file_for_tmp));

	g_signal_connect (monitor, "item-created",
	                  G_CALLBACK (test_monitor_events_created_cb),
	                  NULL);
	g_signal_connect (monitor, "item-updated",
	                  G_CALLBACK (test_monitor_events_updated_cb),
	                  NULL);
	g_signal_connect (monitor, "item-deleted",
	                  G_CALLBACK (test_monitor_events_deleted_cb),
	                  NULL);
	g_signal_connect (monitor, "item-moved",
	                  G_CALLBACK (test_monitor_events_moved_cb),
	                  NULL);

	events = g_hash_table_new_full (g_file_hash,
	                                (GEqualFunc) g_file_equal,
	                                g_object_unref,
	                                NULL);

	path_for_events = g_build_filename (path_for_monitor, "events.txt", NULL);
	g_assert (path_for_events != NULL);
	file_for_events = g_file_new_for_path (path_for_events);
	g_assert (file_for_events != NULL);

	path_for_move_in = g_build_filename (path_for_monitor, "moved.txt", NULL);
	g_assert (path_for_move_in != NULL);
	file_for_move_in = g_file_new_for_path (path_for_move_in);
	g_assert (file_for_move_in != NULL);

	path_for_move_out = g_build_filename (g_get_tmp_dir (), "moved.txt", NULL);
	g_assert (path_for_move_out != NULL);
	file_for_move_out = g_file_new_for_path (path_for_move_out);
	g_assert (file_for_move_out != NULL);
}

static void
test_monitor_free (void)
{
	g_assert (events != NULL);
	g_hash_table_unref (events);

	g_assert_cmpint (g_rmdir (path_for_monitor), ==, 0);

	g_assert (path_for_move_out != NULL);
	g_free (path_for_move_out);
	g_assert (file_for_move_out != NULL);
	g_object_unref (file_for_move_out);

	g_assert (path_for_move_in != NULL);
	g_free (path_for_move_in);
	g_assert (file_for_move_in != NULL);
	g_object_unref (file_for_move_in);

	g_assert (path_for_events != NULL);
	g_free (path_for_events);
	g_assert (file_for_events != NULL);
	g_object_unref (file_for_events);

	g_assert (file_for_tmp != NULL);
	g_object_unref (file_for_tmp);

	g_assert (file_for_monitor != NULL);
	g_object_unref (file_for_monitor);
	g_assert (path_for_monitor != NULL);
	g_free (path_for_monitor);

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

	initialize_signal_handler ();

	g_test_message ("Testing filesystem monitor");

	g_test_add_func ("/libtracker-miner/tracker-monitor/monitor-new",
	                 test_monitor_new);
	g_test_add_func ("/libtracker-miner/tracker-monitor/monitor-file-events",
	                 test_monitor_file_events);

	/* TODO: Add directory events tests */

	/* FIXME: Bug found, if this test occurs before the events
	 * test the setting up of the monitor again doesn't produce
	 * any actual events.
	 */
	g_test_add_func ("/libtracker-miner/tracker-monitor/monitor-enabled",
	                 test_monitor_enabled);
	g_test_add_func ("/libtracker-miner/tracker-monitor/monitor-free",
	                 test_monitor_free);

	return g_test_run ();
}
