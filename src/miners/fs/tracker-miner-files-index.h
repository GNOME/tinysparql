/*
 * Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
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

#ifndef __TRACKER_MINER_FILES_INDEX_H__
#define __TRACKER_MINER_FILES_INDEX_H__

#include <glib-object.h>

#define TRACKER_MINER_FILES_INDEX_SERVICE         "org.freedesktop.Tracker1.Miner.Files.Index"
#define TRACKER_MINER_FILES_INDEX_PATH            "/org/freedesktop/Tracker1/Miner/Files/Index"
#define TRACKER_MINER_FILES_INDEX_INTERFACE       "org.freedesktop.Tracker1.Miner.Files.Index"

#include "tracker-miner-files.h"

G_BEGIN_DECLS

#define TRACKER_TYPE_MINER_FILES_INDEX            (tracker_miner_files_index_get_type ())
#define TRACKER_MINER_FILES_INDEX(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), TRACKER_TYPE_MINER_FILES_INDEX, TrackerMinerFilesIndex))
#define TRACKER_MINER_FILES_INDEX_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TRACKER_TYPE_DBUS_MINER_FILES_INDEX, TrackerMinerFilesIndexClass))
#define TRACKER_IS_MINER_FILES_INDEX(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), TRACKER_TYPE_MINER_FILES_INDEX))
#define TRACKER_IS_MINER_FILES_INDEX_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TRACKER_TYPE_MINER_FILES_INDEX))
#define TRACKER_MINER_FILES_INDEX_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TRACKER_TYPE_MINER_FILES_INDEX, TrackerMinerFilesIndexClass))

typedef struct TrackerMinerFilesIndex TrackerMinerFilesIndex;
typedef struct TrackerMinerFilesIndexClass TrackerMinerFilesIndexClass;

struct TrackerMinerFilesIndex {
	GObject parent;
};

struct TrackerMinerFilesIndexClass {
	GObjectClass parent;
};

GType                     tracker_miner_files_index_get_type           (void);
TrackerMinerFilesIndex   *tracker_miner_files_index_new                (TrackerMinerFiles         *miner_files);
void                      tracker_miner_files_index_reindex_mime_types (TrackerMinerFilesIndex    *object,
	                                                                gchar                    **mime_types,
	                                                                DBusGMethodInvocation     *context,
                                                                        GError                   **error);

void                      tracker_miner_files_index_index_file         (TrackerMinerFilesIndex    *object,
	                                                                gchar                     *file_uri,
	                                                                DBusGMethodInvocation     *context,
	                                                                GError                   **error);

G_END_DECLS

#endif /* __TRACKER_MINER_FILES_INDEX_H__ */
