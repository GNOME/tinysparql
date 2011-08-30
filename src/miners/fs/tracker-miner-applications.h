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

#ifndef __TRACKER_MINER_FS_APPLICATIONS_H__
#define __TRACKER_MINER_FS_APPLICATIONS_H__

#include <libtracker-miner/tracker-miner.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_MINER_APPLICATIONS         (tracker_miner_applications_get_type())
#define TRACKER_MINER_APPLICATIONS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_MINER_APPLICATIONS, TrackerMinerApplications))
#define TRACKER_MINER_APPLICATIONS_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), TRACKER_TYPE_MINER_APPLICATIONS, TrackerMinerApplicationsClass))
#define TRACKER_IS_MINER_APPLICATIONS(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_MINER_APPLICATIONS))
#define TRACKER_IS_MINER_APPLICATIONS_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),  TRACKER_TYPE_MINER_APPLICATIONS))
#define TRACKER_MINER_APPLICATIONS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TRACKER_TYPE_MINER_APPLICATIONS, TrackerMinerApplicationsClass))

typedef struct _TrackerMinerApplications TrackerMinerApplications;
typedef struct _TrackerMinerApplicationsClass TrackerMinerApplicationsClass;

struct _TrackerMinerApplications {
	TrackerMinerFS parent_instance;
	gpointer locale_notification_id;
};

struct _TrackerMinerApplicationsClass {
	TrackerMinerFSClass parent_class;
};

GType         tracker_miner_applications_get_type              (void) G_GNUC_CONST;
TrackerMiner *tracker_miner_applications_new                   (GError       **error);
gboolean      tracker_miner_applications_detect_locale_changed (TrackerMiner  *miner);

G_END_DECLS

#endif /* __TRACKER_MINER_FS_APPLICATIONS_H__ */
