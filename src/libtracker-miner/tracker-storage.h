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

#ifndef __LIBTRACKER_MINER_STORAGE_H__
#define __LIBTRACKER_MINER_STORAGE_H__

#if !defined (__LIBTRACKER_MINER_H_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "Only <libtracker-miner/tracker-miner.h> can be included directly."
#endif

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

/**
 * TrackerStorageType:
 * @TRACKER_STORAGE_REMOVABLE: Storage is a removable media
 * @TRACKER_STORAGE_OPTICAL: Storage is an optical disc
 *
 * Flags specifying properties of the type of storage.
 */
typedef enum {
	TRACKER_STORAGE_REMOVABLE = 1 << 0,
	TRACKER_STORAGE_OPTICAL   = 1 << 1
} TrackerStorageType;

/**
 * TRACKER_STORAGE_TYPE_IS_REMOVABLE:
 * @type: Mask of TrackerStorageType flags
 *
 * Check if the given storage type is marked as being removable media.
 *
 * Returns: %TRUE if the storage is marked as removable media, %FALSE otherwise
 */
#define TRACKER_STORAGE_TYPE_IS_REMOVABLE(type) ((type & TRACKER_STORAGE_REMOVABLE) ? TRUE : FALSE)

/**
 * TRACKER_STORAGE_TYPE_IS_OPTICAL:
 * @type: Mask of TrackerStorageType flags
 *
 * Check if the given storage type is marked as being optical disc
 *
 * Returns: %TRUE if the storage is marked as optical disc, %FALSE otherwise
 */
#define TRACKER_STORAGE_TYPE_IS_OPTICAL(type) ((type & TRACKER_STORAGE_OPTICAL) ? TRUE : FALSE)


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

GType              tracker_storage_get_type                 (void) G_GNUC_CONST;
TrackerStorage *   tracker_storage_new                      (void);
GSList *           tracker_storage_get_device_roots         (TrackerStorage     *storage,
                                                             TrackerStorageType  type,
                                                             gboolean            exact_match);
GSList *           tracker_storage_get_device_uuids         (TrackerStorage     *storage,
                                                             TrackerStorageType  type,
                                                             gboolean            exact_match);
const gchar *      tracker_storage_get_mount_point_for_uuid (TrackerStorage     *storage,
                                                             const gchar        *uuid);
TrackerStorageType tracker_storage_get_type_for_uuid        (TrackerStorage     *storage,
                                                             const gchar        *uuid);
const gchar *      tracker_storage_get_uuid_for_file        (TrackerStorage     *storage,
                                                             GFile              *file);

G_END_DECLS

#endif /* __LIBTRACKER_MINER_STORAGE_H__ */
