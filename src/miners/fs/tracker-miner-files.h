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

#ifndef __TRACKER_MINER_FS_FILES_H__
#define __TRACKER_MINER_FS_FILES_H__

#include <libtracker-miner/tracker-miner.h>

#include "tracker-config.h"

G_BEGIN_DECLS

#define TRACKER_TYPE_MINER_FILES         (tracker_miner_files_get_type())
#define TRACKER_MINER_FILES(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_MINER_FILES, TrackerMinerFiles))
#define TRACKER_MINER_FILES_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), TRACKER_TYPE_MINER_FILES, TrackerMinerFilesClass))
#define TRACKER_IS_MINER_FILES(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_MINER_FILES))
#define TRACKER_IS_MINER_FILES_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),  TRACKER_TYPE_MINER_FILES))
#define TRACKER_MINER_FILES_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TRACKER_TYPE_MINER_FILES, TrackerMinerFilesClass))

typedef struct TrackerMinerFiles TrackerMinerFiles;
typedef struct TrackerMinerFilesClass TrackerMinerFilesClass;
typedef struct TrackerMinerFilesPrivate TrackerMinerFilesPrivate;

struct TrackerMinerFiles {
	TrackerMinerFS parent_instance;
	TrackerMinerFilesPrivate *private;
};

struct TrackerMinerFilesClass {
	TrackerMinerFSClass parent_class;
};

GType         tracker_miner_files_get_type                 (void) G_GNUC_CONST;

TrackerMiner *tracker_miner_files_new                      (TrackerConfig  *config,
                                                            GError        **error);

/* Convenience functions for --eligible tracker-miner-fs cmdline */
gboolean      tracker_miner_files_check_file               (GFile          *file,
                                                            GSList         *ignored_file_paths,
                                                            GSList         *ignored_file_patterns);
gboolean      tracker_miner_files_check_directory          (GFile          *file,
                                                            GSList         *index_recursive_directories,
                                                            GSList         *index_single_directories,
                                                            GSList         *ignored_directory_paths,
                                                            GSList         *ignored_directory_patterns);
gboolean      tracker_miner_files_check_directory_contents (GFile          *parent,
                                                            GList          *children,
                                                            GSList         *ignored_content);
gboolean      tracker_miner_files_monitor_directory        (GFile          *file,
                                                            gboolean        enable_monitors,
                                                            GSList         *directories_to_check);

G_END_DECLS

#endif /* __TRACKER_MINER_FS_FILES_H__ */
