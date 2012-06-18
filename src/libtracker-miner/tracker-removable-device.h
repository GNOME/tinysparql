/*
 * Copyright (C) 2012, Nokia <ivan.frade@nokia.com>
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

#ifndef __TRACKER_REMOVABLE_DEVICE_H__
#define __TRACKER_REMOVABLE_DEVICE_H__

#if !defined (__LIBTRACKER_MINER_H_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "Only <libtracker-miner/tracker-miner.h> can be included directly."
#endif

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_REMOVABLE_DEVICE         (tracker_removable_device_get_type ())
#define TRACKER_REMOVABLE_DEVICE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_REMOVABLE_DEVICE, TrackerRemovableDevice))
#define TRACKER_REMOVABLE_DEVICE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), TRACKER_TYPE_REMOVABLE_DEVICE, TrackerRemovableDeviceClass))
#define TRACKER_IS_REMOVABLE_DEVICE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_REMOVABLE_DEVICE))
#define TRACKER_IS_REMOVABLE_DEVICE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), TRACKER_TYPE_REMOVABLE_DEVICE))
#define TRACKER_REMOVABLE_DEVICE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TRACKER_TYPE_REMOVABLE_DEVICE, TrackerRemovableDeviceClass))

typedef struct _TrackerRemovableDevice TrackerRemovableDevice;
typedef struct _TrackerRemovableDeviceClass TrackerRemovableDeviceClass;
typedef struct _TrackerRemovableDevicePrivate TrackerRemovableDevicePrivate;

/**
 * TrackerRemovableDevice:
 * @parent: parent object
 *
 * Represents a mounted removable or optical volume.
 **/
struct _TrackerRemovableDevice {
	GObject parent;
	TrackerRemovableDevicePrivate *priv;
};

/**
 * TrackerRemovableDeviceClass:
 * @parent_class: parent object class
 *
 * A storage class for #TrackerRemovableDevice.
 **/
struct _TrackerRemovableDeviceClass {
	GObjectClass parent_class;
};

GType                   tracker_removable_device_get_type                 (void) G_GNUC_CONST;

TrackerRemovableDevice *tracker_removable_device_new                      (GMount *mount);

GMount                 *tracker_removable_device_get_mount                (TrackerRemovableDevice *device);
GFile                  *tracker_removable_device_get_mount_point          (TrackerRemovableDevice *device);

G_END_DECLS

#endif /* __TRACKER_REMOVABLE_DEVICE_H__ */
