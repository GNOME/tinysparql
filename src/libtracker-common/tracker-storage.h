/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia (urho.konttori@nokia.com)
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

#ifndef __LIBTRACKER_STORAGE_H__
#define __LIBTRACKER_STORAGE_H__

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#if !defined (__LIBTRACKER_COMMON_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "only <libtracker-common/tracker-common.h> must be included directly."
#endif

#define TRACKER_TYPE_STORAGE	     (tracker_storage_get_type ())
#define TRACKER_STORAGE(o)	     (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_STORAGE, TrackerStorage))
#define TRACKER_STORAGE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), TRACKER_TYPE_STORAGE, TrackerStorageClass))
#define TRACKER_IS_STORAGE(o)	     (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_STORAGE))
#define TRACKER_IS_STORAGE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), TRACKER_TYPE_STORAGE))
#define TRACKER_STORAGE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TRACKER_TYPE_STORAGE, TrackerStorageClass))

typedef struct _TrackerStorage	TrackerStorage;
typedef struct _TrackerStorageClass TrackerStorageClass;

struct _TrackerStorage {
	GObject      parent;
};

struct _TrackerStorageClass {
	GObjectClass parent_class;
};

#ifdef HAVE_HAL

GType	     tracker_storage_get_type		             (void) G_GNUC_CONST;

TrackerStorage * tracker_storage_new                         (void);

GList *      tracker_storage_get_mounted_directory_roots     (TrackerStorage *storage);
GList *      tracker_storage_get_removable_device_roots      (TrackerStorage *storage);
GList *      tracker_storage_get_removable_device_udis       (TrackerStorage *storage);

const gchar *tracker_storage_udi_get_mount_point             (TrackerStorage *storage,
							      const gchar    *udi);
gboolean     tracker_storage_udi_get_is_mounted              (TrackerStorage *storage,
							      const gchar    *udi);
gboolean     tracker_storage_uri_is_on_removable_device      (TrackerStorage *storage,
							      const gchar    *uri,
							      gchar         **mount_point,
							      gboolean       *available);

const gchar* tracker_storage_get_volume_udi_for_file	     (TrackerStorage *storage,
							      GFile          *file);

#endif /* HAVE_HAL */

G_END_DECLS

#endif /* __LIBTRACKER_STORAGE_H__ */
