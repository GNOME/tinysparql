/*
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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

#ifndef __TRACKER_MINER_FS_POWER_H__
#define __TRACKER_MINER_FS_POWER_H__

#include <glib-object.h>

G_BEGIN_DECLS

#if !defined (__LIBTRACKER_COMMON_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "only <libtracker-common/tracker-common.h> must be included directly."
#endif

#define TRACKER_TYPE_POWER         (tracker_power_get_type ())
#define TRACKER_POWER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_POWER, TrackerPower))
#define TRACKER_POWER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), TRACKER_TYPE_POWER, TrackerPowerClass))
#define TRACKER_IS_POWER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_POWER))
#define TRACKER_IS_POWER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), TRACKER_TYPE_POWER))
#define TRACKER_POWER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TRACKER_TYPE_POWER, TrackerPowerClass))

typedef struct _TrackerPower TrackerPower;
typedef struct _TrackerPowerClass TrackerPowerClass;

struct _TrackerPower {
	GObject parent;
};

struct _TrackerPowerClass {
	GObjectClass parent_class;
};

GType         tracker_power_get_type               (void) G_GNUC_CONST;

TrackerPower *tracker_power_new                    (void);

gboolean      tracker_power_get_on_battery         (TrackerPower *power);
gboolean      tracker_power_get_on_low_battery     (TrackerPower *power);

G_END_DECLS

#endif /* __TRACKER_MINER_FS_POWER_H__ */
