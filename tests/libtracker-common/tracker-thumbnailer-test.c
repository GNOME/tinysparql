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
#include "config.h"
#include <glib.h>
#include <stdlib.h>

#include <libtracker-common/tracker-thumbnailer.h>

static const gchar *old_xdg_config = NULL;
static TrackerConfig *cosmic_config = NULL;

static void
set_config_directory ()
{
        old_xdg_config = g_getenv ("XDG_CONFIG_HOME");
        g_setenv ("XDG_CONFIG_HOME", ".", TRUE);
}

static void
restore_config_directory ()
{
        g_setenv ("XDG_CONFIG_HOME", old_xdg_config, TRUE);
}

static void
test_init_shutdown ()
{

        tracker_thumbnailer_shutdown ();

        tracker_thumbnailer_init (cosmic_config);
        tracker_thumbnailer_shutdown ();

        tracker_thumbnailer_init (cosmic_config);
        tracker_thumbnailer_shutdown ();

        tracker_thumbnailer_shutdown ();
        tracker_thumbnailer_init (cosmic_config);
        tracker_thumbnailer_init (cosmic_config);
        tracker_thumbnailer_shutdown ();
}

static void
test_queue_file ()
{
        tracker_thumbnailer_init (cosmic_config);

        /* URI with supported mimetype */
        if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR)) {
                tracker_thumbnailer_queue_file ("file:///a/b/c.jpeg", "image/jpeg");
        }
        g_test_trap_assert_stderr ("*Thumbnailer queue appended with uri:'file:///a/b/c.jpeg', mime type:'image/jpeg', request_id:1...*");


        /* URI with unsupported mimetype */
        if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDOUT)) {
                tracker_thumbnailer_queue_file ("file:///a/b/c.jpeg", "unsupported");
                exit (0);
        }
        g_test_trap_assert_passed ();
        g_test_trap_assert_stdout ("*Thumbnailer ignoring uri:'file:///a/b/c.jpeg', mime type:'unsupported'*");


        /* Path with supported mimetype */
        if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR)) {
                tracker_thumbnailer_queue_file ("/a/b/c.jpeg", "image/jpeg");
        }
        g_test_trap_assert_stderr ("*Thumbnailer queue appended with uri:'file:///a/b/c.jpeg', mime type:'image/jpeg', request_id:1...*");


        tracker_thumbnailer_shutdown ();
}

static void
test_queue_send ()
{
        gint i;
        
        tracker_thumbnailer_init (cosmic_config);
        
        for (i = 0; i < 10; i++) {
                gchar *filename = g_strdup_printf ("file:///a/b/c%d.jpeg", i);
                tracker_thumbnailer_queue_file (filename, "image/jpeg");
        }

        if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR)) {
                tracker_thumbnailer_queue_send ();
        }
        g_test_trap_assert_stderr ("*DBUS-CALL: Queue*");
        g_test_trap_assert_stderr ("*Thumbnailer queue sent with 10 items to thumbnailer daemon, request ID:10...*");

        tracker_thumbnailer_shutdown ();
}

static void
test_move ()
{
        tracker_thumbnailer_init (cosmic_config);
        
        if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR)) {
                tracker_thumbnailer_move ("file:///a/b/c1.jpeg", "image/jpeg",
                                          "file:///a/b/d1.jpeg");
        }
        g_test_trap_assert_stderr ("*DBUS-CALL: Move*");
        g_test_trap_assert_stderr ("*Thumbnailer request to move uri from:'file:///a/b/c1.jpeg' to:'file:///a/b/d1.jpeg', request_id:1...*");
        tracker_thumbnailer_shutdown ();
}

static void
test_remove ()
{
        tracker_thumbnailer_init (cosmic_config);
        
        if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR)) {
                tracker_thumbnailer_remove ("file:///a/b/c1.jpeg", "image/jpeg");
        }
        g_test_trap_assert_stderr ("*DBUS-CALL: Delete*");
        g_test_trap_assert_stderr ("*Thumbnailer request to remove uri:'file:///a/b/c1.jpeg', request_id:1...*");

        tracker_thumbnailer_shutdown ();
}

static void
test_cleanup ()
{
        tracker_thumbnailer_init (cosmic_config);
        
        if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR)) {
                tracker_thumbnailer_cleanup ("file:///a/b/c1.jpeg");
        }
        g_test_trap_assert_stderr ("*DBUS-CALL: Cleanup*");
        g_test_trap_assert_stderr ("*Thumbnailer cleaning up uri:'file:///a/b/c1.jpeg', request_id:1...*");

        tracker_thumbnailer_shutdown ();
}

gint
main (gint argc, gchar **argv)
{
	gint result;

	g_type_init ();
	g_test_init (&argc, &argv, NULL);

        set_config_directory ();
        cosmic_config = tracker_config_new ();
        /* True is the default value, but makes sure! */
        tracker_config_set_enable_thumbnails (cosmic_config, TRUE);
        
        
        g_test_add_func ("/libtracker-common/tracker-thumbnailer/init_shutdown",
                         test_init_shutdown);
        g_test_add_func ("/libtracker-common/tracker-thumbnailer/queue_file",
                         test_queue_file);
        g_test_add_func ("/libtracker-common/tracker-thumbnailer/queue_send",
                         test_queue_send);
        g_test_add_func ("/libtracker-common/tracker-thumbnailer/move",
                         test_move);
        g_test_add_func ("/libtracker-common/tracker-thumbnailer/remove",
                         test_remove);
        g_test_add_func ("/libtracker-common/tracker-thumbnailer/cleanup",
                         test_cleanup);

	result = g_test_run ();

        restore_config_directory ();

	return result;
}
