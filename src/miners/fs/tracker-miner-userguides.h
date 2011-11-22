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
 *
 * Author: Martyn Russell <martyn@lanedo.com>
 */

#ifndef __TRACKER_MINER_FS_USERGUIDES_H__
#define __TRACKER_MINER_FS_USERGUIDES_H__

#include <libtracker-miner/tracker-miner.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_MINER_USERGUIDES         (tracker_miner_userguides_get_type())
#define TRACKER_MINER_USERGUIDES(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_MINER_USERGUIDES, TrackerMinerUserguides))
#define TRACKER_MINER_USERGUIDES_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), TRACKER_TYPE_MINER_USERGUIDES, TrackerMinerUserguidesClass))
#define TRACKER_IS_MINER_USERGUIDES(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_MINER_USERGUIDES))
#define TRACKER_IS_MINER_USERGUIDES_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),  TRACKER_TYPE_MINER_USERGUIDES))
#define TRACKER_MINER_USERGUIDES_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TRACKER_TYPE_MINER_USERGUIDES, TrackerMinerUserguidesClass))

typedef struct _TrackerMinerUserguides TrackerMinerUserguides;
typedef struct _TrackerMinerUserguidesClass TrackerMinerUserguidesClass;

struct _TrackerMinerUserguides {
	TrackerMinerFS parent_instance;
	gpointer locale_notification_id;
};

struct _TrackerMinerUserguidesClass {
	TrackerMinerFSClass parent_class;
};

GType         tracker_miner_userguides_get_type              (void) G_GNUC_CONST;
TrackerMiner *tracker_miner_userguides_new                   (GError       **error);
gboolean      tracker_miner_userguides_detect_locale_changed (TrackerMiner  *miner);

G_END_DECLS

#endif /* __TRACKER_MINER_FS_USERGUIDES_H__ */
