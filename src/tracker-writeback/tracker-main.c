/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2009, Nokia
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

#include <stdlib.h>

#include "tracker-writeback-dispatcher.h"

int
main (int   argc,
      char *argv[])
{
        TrackerWritebackDispatcher *dispatcher;
        GMainLoop *loop;

        g_type_init ();

        loop = g_main_loop_new (NULL, FALSE);
        dispatcher = tracker_writeback_dispatcher_new ();

        g_main_loop_run (loop);

        g_object_unref (dispatcher);

        return EXIT_SUCCESS;
}
