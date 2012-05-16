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

#include "config.h"

#include <string.h>

#include <gio/gio.h>
#include <gio/gunixmounts.h>

#include <libtracker-common/tracker-log.h>

#include "tracker-removable-device.h"
#include "tracker-marshal.h"
#include "tracker-storage.h"
#include "tracker-utils.h"

/**
 * SECTION:tracker-removable-device
 * @short_description: Removable storage and mount point convenience API
 * @include: libtracker-miner/tracker-miner.h
 *
 * A #TrackerRemovableDevice represents a mounted removable volume or optical
 * disc.
 **/

#define TRACKER_REMOVABLE_DEVICE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_REMOVABLE_DEVICE, TrackerRemovableDevicePrivate))

struct _TrackerRemovableDevicePrivate {
	GMount *mount;
	gchar *mount_point;
	gchar *uuid;
	guint unmount_timer_id;
	guint removable : 1;
	guint optical : 1;
};

enum {
	MINING_COMPLETE,
	LAST_SIGNAL
};

static void tracker_removable_device_finalize (GObject *object);

static guint signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (TrackerRemovableDevice, tracker_removable_device, G_TYPE_OBJECT);

static void
tracker_removable_device_class_init (TrackerRemovableDeviceClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_removable_device_finalize;

	/**
	 * TrackerRemovableDevice::crawling-complete:
	 * @device: the #TrackerRemovableDevice
	 *
	 * The ::crawling-complete signal is emitted when each file on the
	 * removable device has been processed by the FS miner.
	 *
	 * Since: 0.14.2
	 **/
	signals[MINING_COMPLETE] =
		g_signal_new ("mining-complete",
		              G_OBJECT_CLASS_TYPE (object_class),
		              G_SIGNAL_RUN_LAST,
		              0,
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE,
		              0);

	g_type_class_add_private (object_class, sizeof (TrackerRemovableDevicePrivate));
}

static void
tracker_removable_device_init (TrackerRemovableDevice *device)
{
	device->priv = TRACKER_REMOVABLE_DEVICE_GET_PRIVATE (device);
}

static void
tracker_removable_device_finalize (GObject *object)
{
	TrackerRemovableDevicePrivate *priv;

	priv = TRACKER_REMOVABLE_DEVICE_GET_PRIVATE (object);

	g_object_unref (priv->mount);

	(G_OBJECT_CLASS (tracker_removable_device_parent_class)->finalize) (object);
}

TrackerRemovableDevice *
tracker_removable_device_new (GMount *mount)
{
	TrackerRemovableDevice *device;

	device = g_object_new (TRACKER_TYPE_REMOVABLE_DEVICE, NULL);

	device->priv->mount = g_object_ref (mount);

	return device;
}

/**
 * tracker_removable_device_get_mount:
 * @device: a #TrackerRemovableDevice
 *
 * Returns the #GMount object representing @device. The caller does not need
 * to unreference the return value.
 *
 * Returns: (transfer none): the #GMount object representing the device.
 **/
GMount *
tracker_removable_device_get_mount (TrackerRemovableDevice *device)
{
	g_return_val_if_fail (TRACKER_IS_REMOVABLE_DEVICE (device), NULL);

	return device->priv->mount;
}

/**
 * tracker_removable_device_get_mount_point:
 * @device: a #TrackerRemovableDevice
 *
 * Returns a #GFile object representing the mount point of @device. The
 * caller should unreference this object when no longer needed.
 *
 * Returns: (transfer full): a #GFile object presenting the mount point of
 *          @device.
 **/
GFile *
tracker_removable_device_get_mount_point (TrackerRemovableDevice *device)
{
	g_return_val_if_fail (TRACKER_IS_REMOVABLE_DEVICE( device), NULL);

	return g_mount_get_root (device->priv->mount);
}
/**
 * tracker_removable_device_file_notify:
 * @device: a #TrackerRemovableDevice
 * @file: a #GFile
 *
 * Increases the files completed count of @device. If all files have been
 * processed, this function will cause the ::mining-complete signal to be
 * emitted.
 */
void
tracker_removable_device_file_notify (TrackerRemovableDevice *device,
                                      GFile                  *file)
{
	gint files_found;
	gint files_processed;

	files_found = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (device), "tracker-files-found"));
	files_processed = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (device), "tracker-files-processed"));

	g_object_set_data (G_OBJECT (device), "tracker-files-processed", GINT_TO_POINTER (++ files_processed));

	g_warn_if_fail (files_found >= files_processed);

	if (files_found == files_processed) {
		g_signal_emit (device, signals[MINING_COMPLETE], 0);

		g_object_set_data (G_OBJECT (device), "tracker-files-found", NULL);
		g_object_set_data (G_OBJECT (device), "tracker-files-processed", NULL);
	}
}
