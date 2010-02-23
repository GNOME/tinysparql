/*
 * Copyright (C) 2008, Nokia (urho.konttori@nokia.com)
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

#ifndef __LIBTRACKER_MINER_STORAGE_H__
#define __LIBTRACKER_MINER_STORAGE_H__

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_STORAGE         (tracker_storage_get_type ())
#define TRACKER_STORAGE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_STORAGE, TrackerStorage))
#define TRACKER_STORAGE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), TRACKER_TYPE_STORAGE, TrackerStorageClass))
#define TRACKER_IS_STORAGE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_STORAGE))
#define TRACKER_IS_STORAGE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), TRACKER_TYPE_STORAGE))
#define TRACKER_STORAGE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TRACKER_TYPE_STORAGE, TrackerStorageClass))

typedef struct _TrackerStorage TrackerStorage;
typedef struct _TrackerStorageClass TrackerStorageClass;

struct _TrackerStorage {
	GObject parent;
};

struct _TrackerStorageClass {
	GObjectClass parent_class;
};

GType           tracker_storage_get_type                    (void) G_GNUC_CONST;
TrackerStorage *tracker_storage_new                         (void);


/* Needed */
GSList *        tracker_storage_get_removable_device_roots  (TrackerStorage  *storage);
GSList *        tracker_storage_get_removable_device_udis   (TrackerStorage  *storage);
const gchar *   tracker_storage_udi_get_mount_point         (TrackerStorage  *storage,
                                                             const gchar     *udi);
const gchar*    tracker_storage_get_volume_udi_for_file     (TrackerStorage  *storage,
                                                             GFile           *file);



G_END_DECLS

#endif /* __LIBTRACKER_MINER_STORAGE_H__ */
