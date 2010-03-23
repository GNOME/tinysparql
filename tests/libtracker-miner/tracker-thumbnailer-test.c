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
#include <libtracker-miner/tracker-miner.h>
#include "thumbnailer-mock.h"


static void
test_thumbnailer_init ()
{
        g_assert (tracker_thumbnailer_init ());

        tracker_thumbnailer_shutdown ();
}

static void
test_thumbnailer_send_empty () 
{
        GList *dbus_calls = NULL;

        dbus_mock_call_log_reset ();

        tracker_thumbnailer_init ();
        tracker_thumbnailer_send ();

        dbus_calls = dbus_mock_call_log_get ();
        g_assert (dbus_calls == NULL);

        tracker_thumbnailer_shutdown ();
}

static void
test_thumbnailer_send_moves ()
{
        GList *dbus_calls = NULL;
        
        dbus_mock_call_log_reset ();

        tracker_thumbnailer_init ();

        /* Returns TRUE, but there is no dbus call */
        g_assert (tracker_thumbnailer_move_add ("file://a.jpeg", "mock/one", "file://b.jpeg"));
        g_assert (dbus_mock_call_log_get () == NULL);

        /* Returns FALSE, unsupported mime */
        g_assert (!tracker_thumbnailer_move_add ("file://a.jpeg", "unsupported", "file://b.jpeg"));
        g_assert (dbus_mock_call_log_get () == NULL);

        tracker_thumbnailer_send ();

        /* One call to "move" method */
        dbus_calls = dbus_mock_call_log_get ();
        g_assert_cmpint (g_list_length (dbus_calls), ==, 1);
        g_assert_cmpstr (dbus_calls->data, ==, "Move");

        tracker_thumbnailer_shutdown ();
        dbus_mock_call_log_reset ();
}

static void
test_thumbnailer_send_removes ()
{
        GList *dbus_calls = NULL;
        
        dbus_mock_call_log_reset ();

        tracker_thumbnailer_init ();

        /* Returns TRUE, but there is no dbus call */
        g_assert (tracker_thumbnailer_remove_add ("file://a.jpeg", "mock/one"));
        g_assert (dbus_mock_call_log_get () == NULL);

        /* Returns FALSE, unsupported mime */
        g_assert (!tracker_thumbnailer_remove_add ("file://a.jpeg", "unsupported"));
        g_assert (dbus_mock_call_log_get () == NULL);

        tracker_thumbnailer_send ();

        /* One call to "Delete" method */
        dbus_calls = dbus_mock_call_log_get ();
        g_assert_cmpint (g_list_length (dbus_calls), ==, 1);
        g_assert_cmpstr (dbus_calls->data, ==, "Delete");

        tracker_thumbnailer_shutdown ();
        dbus_mock_call_log_reset ();
}

static void
test_thumbnailer_send_cleanup ()
{
        GList *dbus_calls = NULL;
        
        dbus_mock_call_log_reset ();

        tracker_thumbnailer_init ();

        /* Returns TRUE, and there is a dbus call */
        g_assert (tracker_thumbnailer_cleanup ("file://tri/lu/ri"));

        /* One call to "Clean" method */
        dbus_calls = dbus_mock_call_log_get ();
        g_assert_cmpint (g_list_length (dbus_calls), ==, 1);
        g_assert_cmpstr (dbus_calls->data, ==, "Cleanup");

        tracker_thumbnailer_shutdown ();
        dbus_mock_call_log_reset ();
}

int
main (int    argc,
      char **argv)
{
	g_type_init ();
	g_thread_init (NULL);
	g_test_init (&argc, &argv, NULL);

	g_test_message ("Testing filesystem crawler");

	g_test_add_func ("/libtracker-miner/tracker-thumbnailer/init",
	                 test_thumbnailer_init);
	g_test_add_func ("/libtracker-miner/tracker-thumbnailer/send_empty",
	                 test_thumbnailer_send_empty);
        g_test_add_func ("/libtracker-minter/tracker-thumbnailer/send_moves",
                         test_thumbnailer_send_moves);
        g_test_add_func ("/libtracker-minter/tracker-thumbnailer/send_removes",
                         test_thumbnailer_send_removes);
        g_test_add_func ("/libtracker-minter/tracker-thumbnailer/send_cleanup",
                         test_thumbnailer_send_cleanup);

	return g_test_run ();
}
