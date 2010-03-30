/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
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

#include "tracker-writeback.h"

G_DEFINE_ABSTRACT_TYPE (TrackerWriteback, tracker_writeback, G_TYPE_OBJECT)

static TrackerMinerManager *manager = NULL;

static void
tracker_writeback_class_init (TrackerWritebackClass *klass)
{
}

static void
tracker_writeback_init (TrackerWriteback *writeback)
{
}

gboolean
tracker_writeback_update_metadata (TrackerWriteback *writeback,
                                   GPtrArray        *values,
                                   TrackerClient    *client)
{
	g_return_val_if_fail (TRACKER_IS_WRITEBACK (writeback), FALSE);
	g_return_val_if_fail (values != NULL, FALSE);

	if (TRACKER_WRITEBACK_GET_CLASS (writeback)->update_metadata) {
		return TRACKER_WRITEBACK_GET_CLASS (writeback)->update_metadata (writeback, values, client);
	}

	return FALSE;
}

TrackerMinerManager*
tracker_writeback_get_miner_manager (void)
{
	if (!manager) {
		manager = tracker_miner_manager_new ();
	}

	return manager;
}
