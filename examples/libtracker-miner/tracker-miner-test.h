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

#ifndef __TRACKER_MINER_TEST_H__
#define __TRACKER_MINER_TEST_H__

#include <glib-object.h>

#include <libtracker-miner/tracker-miner.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_MINER_TEST         (tracker_miner_test_get_type())
#define TRACKER_MINER_TEST(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_MINER_TEST, TrackerMinerTest))
#define TRACKER_MINER_TEST_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), TRACKER_TYPE_MINER_TEST, TrackerMinerTestClass))
#define TRACKER_IS_MINER_TEST(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_MINER_TEST))
#define TRACKER_IS_MINER_TEST_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), TRACKER_TYPE_MINER_TEST))
#define TRACKER_MINER_TEST_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  TRACKER_TYPE_MINER_TEST, TrackerMinerTestClass))

typedef struct TrackerMinerTest      TrackerMinerTest;
typedef struct TrackerMinerTestClass TrackerMinerTestClass;

struct TrackerMinerTest {
	TrackerMinerFS parent_instance;
};

struct TrackerMinerTestClass {
	TrackerMinerFSClass parent_class;
};

GType          tracker_miner_test_get_type (void) G_GNUC_CONST;
TrackerMiner * tracker_miner_test_new      (const gchar *name);

G_END_DECLS

#endif /* __TRACKER_MINER_TEST_H__ */
