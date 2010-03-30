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

#include "tracker-miner-test.h"

G_DEFINE_TYPE (TrackerMinerTest, tracker_miner_test, TRACKER_TYPE_MINER_FS)

static void
tracker_miner_test_class_init (TrackerMinerTestClass *klass)
{
}

static void
tracker_miner_test_init (TrackerMinerTest *miner)
{
}

TrackerMiner *
tracker_miner_test_new (const gchar *name)
{
	return g_object_new (TRACKER_TYPE_MINER_TEST,
	                     "name", name,
	                     NULL);
}
