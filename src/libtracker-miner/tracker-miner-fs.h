/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008, Nokia
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

#ifndef __TRACKERD_MINER_FS_H__
#define __TRACKERD_MINER_FS_H__

#include <glib-object.h>
#include <gio/gio.h>

#include <libtracker-common/tracker-sparql-builder.h>

#include "tracker-miner.h"

G_BEGIN_DECLS

#define TRACKER_TYPE_MINER_FS         (tracker_miner_fs_get_type())
#define TRACKER_MINER_FS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_MINER_FS, TrackerMinerFS))
#define TRACKER_MINER_FS_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), TRACKER_TYPE_MINER_FS, TrackerMinerFSClass))
#define TRACKER_IS_MINER_FS(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_MINER_FS))
#define TRACKER_IS_MINER_FS_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),  TRACKER_TYPE_MINER_FS))
#define TRACKER_MINER_FS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TRACKER_TYPE_MINER_FS, TrackerMinerFSClass))

typedef struct TrackerMinerFS        TrackerMinerFS;
typedef struct TrackerMinerFSClass   TrackerMinerFSClass;
typedef struct TrackerMinerFSPrivate TrackerMinerFSPrivate;

struct TrackerMinerFS {
	TrackerMiner parent;
	TrackerMinerFSPrivate *private;
};

struct TrackerMinerFSClass {
	TrackerMinerClass parent;

	gboolean (* check_file)            (TrackerMinerFS       *fs,
					    GFile                *file);
	gboolean (* check_directory)       (TrackerMinerFS       *fs,
					    GFile                *file);
	gboolean (* process_file)          (TrackerMinerFS       *fs,
					    GFile                *file,
					    TrackerSparqlBuilder *builder);
	gboolean (* monitor_directory)     (TrackerMinerFS       *fs,
					    GFile                *file);
	void     (* finished)              (TrackerMinerFS       *fs);
};

GType    tracker_miner_fs_get_type         (void) G_GNUC_CONST;

void     tracker_miner_fs_add_directory    (TrackerMinerFS *fs,
					    GFile          *file,
					    gboolean        recurse);
gboolean tracker_miner_fs_remove_directory (TrackerMinerFS *fs,
					    GFile          *file);


G_END_DECLS

#endif /* __TRACKERD_MINER_FS_H__ */
