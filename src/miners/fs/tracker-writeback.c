/*
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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

#include <gio/gio.h>

#include "tracker-writeback.h"
#include "tracker-writeback-listener.h"
#include "tracker-writeback-dispatcher.h"

/* Listener listens for the Writeback signal coming from the store, it performs
 * a few queries to get a set of changed values, and pushes the writeback task
 * to miner-fs's queues. */
static TrackerWritebackListener *listener = NULL;

/* That task in miner-fs's queue callsback to the dispatcher. The dispatcher
 * calls the external tracker-writeback process which does the actual write */
static TrackerWritebackDispatcher *dispatcher = NULL;

void
tracker_writeback_init (TrackerMinerFiles  *miner_files,
                        GError            **error)
{
	GError *internal_error = NULL;

	listener = tracker_writeback_listener_new (miner_files, &internal_error);

	if (!internal_error) {
		dispatcher = tracker_writeback_dispatcher_new (miner_files, &internal_error);
	}

	if (internal_error) {
		if (listener) {
			g_object_unref (listener);
			listener = NULL;
		}
		g_propagate_error (error, internal_error);
	}
}

void
tracker_writeback_shutdown (void)
{
	if (listener) {
		g_object_unref (listener);
		listener = NULL;
	}

	if (dispatcher) {
		g_object_unref (dispatcher);
		dispatcher = NULL;
	}
}
