/*
 * Copyright (C) 2010, Nokia
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

#ifndef __TRACKER_STORE_MINER_FILES_REINDEX_H__
#define __TRACKER_STORE_MINER_FILES_REINDEX_H__

#include <glib-object.h>

#define TRACKER_MINER_FILES_REINDEX_SERVICE         "org.freedesktop.Tracker1.Miner.Files.Reindex"
#define TRACKER_MINER_FILES_REINDEX_PATH            "/org/freedesktop/Tracker1/Miner/Files/Reindex"
#define TRACKER_MINER_FILES_REINDEX_INTERFACE       "org.freedesktop.Tracker1.Miner.Files.Reindex"

#include "tracker-miner-files.h"

G_BEGIN_DECLS

#define TRACKER_TYPE_MINER_FILES_REINDEX            (tracker_miner_files_reindex_get_type ())
#define TRACKER_MINER_FILES_REINDEX(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), TRACKER_TYPE_MINER_FILES_REINDEX, TrackerMinerFilesReindex))
#define TRACKER_MINER_FILES_REINDEX_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TRACKER_TYPE_DBUS_MINER_FILES_REINDEX, TrackerMinerFilesReindexClass))
#define TRACKER_IS_MINER_FILES_REINDEX(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), TRACKER_TYPE_MINER_FILES_REINDEX))
#define TRACKER_IS_MINER_FILES_REINDEX_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TRACKER_TYPE_MINER_FILES_REINDEX))
#define TRACKER_MINER_FILES_REINDEX_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TRACKER_TYPE_MINER_FILES_REINDEX, TrackerMinerFilesReindexClass))

typedef struct TrackerMinerFilesReindex TrackerMinerFilesReindex;
typedef struct TrackerMinerFilesReindexClass TrackerMinerFilesReindexClass;

struct TrackerMinerFilesReindex {
	GObject parent;
};

struct TrackerMinerFilesReindexClass {
	GObjectClass parent;
};

GType                     tracker_miner_files_reindex_get_type   (void);
TrackerMinerFilesReindex *tracker_miner_files_reindex_new        (TrackerMinerFiles         *miner_files);
void                      tracker_miner_files_reindex_mime_types (TrackerMinerFilesReindex  *object,
                                                                  gchar                    **mime_types,
                                                                  DBusGMethodInvocation     *context,
                                                                  GError                   **error);

G_END_DECLS

#endif /* __TRACKER_STORE_MINER_FILES_REINDEX_H__ */
