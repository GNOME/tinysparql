/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
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

#ifndef __TRACKER_STORE_STATISTICS_H__
#define __TRACKER_STORE_STATISTICS_H__

#include <glib-object.h>

#define TRACKER_STATISTICS_SERVICE         "org.freedesktop.Tracker1"
#define TRACKER_STATISTICS_PATH                    "/org/freedesktop/Tracker1/Statistics"
#define TRACKER_STATISTICS_INTERFACE       "org.freedesktop.Tracker1.Statistics"

G_BEGIN_DECLS

#define TRACKER_TYPE_STATISTICS                    (tracker_statistics_get_type ())
#define TRACKER_STATISTICS(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), TRACKER_TYPE_STATISTICS, TrackerStatistics))
#define TRACKER_STATISTICS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TRACKER_TYPE_DBUS_STATISTICS, TrackerStatisticsClass))
#define TRACKER_IS_STATISTICS(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), TRACKER_TYPE_STATISTICS))
#define TRACKER_IS_STATISTICS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TRACKER_TYPE_STATISTICS))
#define TRACKER_STATISTICS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TRACKER_TYPE_STATISTICS, TrackerStatisticsClass))

typedef struct TrackerStatistics TrackerStatistics;
typedef struct TrackerStatisticsClass TrackerStatisticsClass;

struct TrackerStatistics {
	GObject parent;
};

struct TrackerStatisticsClass {
	GObjectClass parent;
};

GType              tracker_statistics_get_type (void);
TrackerStatistics *tracker_statistics_new      (void);
void               tracker_statistics_get      (TrackerStatistics      *object,
                                                DBusGMethodInvocation  *context,
                                                GError                **error);

G_END_DECLS

#endif /* __TRACKER_STORE_STATISTICS_H__ */
