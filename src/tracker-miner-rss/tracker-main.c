/*
 * Copyright (C) 2009/2010, Roberto Guido <madbob@users.barberaware.org>
 *                          Michele Tameni <michele@amdplanet.it>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <stdlib.h>
#include "tracker-miner-rss.h"

int
main () {
	TrackerMinerRSS *miner;

	g_type_init ();
	g_thread_init (NULL);
	miner = g_object_new (TRACKER_TYPE_MINER_RSS, "name", "RSS", NULL);
	tracker_miner_start (TRACKER_MINER (miner));
	g_main_loop_run (g_main_loop_new (NULL, FALSE));
	exit (0);
}
