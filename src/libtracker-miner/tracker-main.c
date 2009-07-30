/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2009, Nokia

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

#include <stdlib.h>
#include "tracker-miner-test.h"

static void
on_miner_terminated (TrackerMiner *miner,
		     gpointer      user_data)
{
        GMainLoop *main_loop = user_data;

        g_main_loop_quit (main_loop);
}

static gboolean
on_miner_start (TrackerMiner *miner)
{
	g_message ("Starting miner");
	tracker_miner_start (miner);

	return FALSE;
}

int
main (int argc, char *argv[])
{
        TrackerMiner *miner;
        GMainLoop *main_loop;

        g_type_init ();

        main_loop = g_main_loop_new (NULL, FALSE);

        miner = tracker_miner_test_new ("test");
        g_signal_connect (miner, "terminated",
                          G_CALLBACK (on_miner_terminated), main_loop);

	g_timeout_add_seconds (1, (GSourceFunc) on_miner_start, miner);

        g_main_loop_run (main_loop);

        return EXIT_SUCCESS;
}
