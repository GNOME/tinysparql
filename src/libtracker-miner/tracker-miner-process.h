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

#ifndef __TRACKERD_MINER_PROCESS_H__
#define __TRACKERD_MINER_PROCESS_H__

#include <glib-object.h>
#include <gio/gio.h>

#include "tracker-miner.h"

G_BEGIN_DECLS

#define TRACKER_TYPE_MINER_PROCESS         (tracker_miner_process_get_type())
#define TRACKER_MINER_PROCESS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_MINER_PROCESS, TrackerMinerProcess))
#define TRACKER_MINER_PROCESS_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), TRACKER_TYPE_MINER_PROCESS, TrackerMinerProcessClass))
#define TRACKER_IS_PROCESS(o)              (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_MINER_PROCESS))
#define TRACKER_IS_PROCESS_CLASS(c)        (G_TYPE_CHECK_CLASS_TYPE ((c),  TRACKER_TYPE_MINER_PROCESS))
#define TRACKER_MINER_PROCESS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TRACKER_TYPE_MINER_PROCESS, TrackerMinerProcessClass))
typedef struct TrackerMinerProcess        TrackerMinerProcess;
typedef struct TrackerMinerProcessClass   TrackerMinerProcessClass;
typedef struct TrackerMinerProcessPrivate TrackerMinerProcessPrivate;

struct TrackerMinerProcess {
	TrackerMiner parent;
	TrackerMinerProcessPrivate *private;
};

struct TrackerMinerProcessClass {
	TrackerMinerClass parent;

	gboolean (* check_file)           (TrackerMinerProcess *process,
					   GFile            *file);
	gboolean (* check_directory)      (TrackerMinerProcess *process,
					   GFile            *file);
	void     (* process_file)         (TrackerMinerProcess *process,
					   GFile            *file);
	gboolean (* monitor_directory)    (TrackerMinerProcess *process,
					   GFile            *file);
	
	void     (* finished)             (TrackerMinerProcess *process);
};

GType tracker_miner_process_get_type      (void) G_GNUC_CONST;

void  tracker_miner_process_add_directory (TrackerMinerProcess *process,
					   const gchar         *path,
					   gboolean             recurse);

G_END_DECLS

#endif /* __TRACKERD_MINER_PROCESS_H__ */
