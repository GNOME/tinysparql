/*
 * Copyright (C) 2009, Nokia
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

#ifndef __TRACKER_WRITEBACK_WRITEBACK_H__
#define __TRACKER_WRITEBACK_WRITEBACK_H__

#include <glib-object.h>

#include <libtracker-miner/tracker-miner.h>
#include <libtracker-client/tracker.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_WRITEBACK         (tracker_writeback_get_type())
#define TRACKER_WRITEBACK(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_WRITEBACK, TrackerWriteback))
#define TRACKER_WRITEBACK_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), TRACKER_TYPE_WRITEBACK, TrackerWritebackClass))
#define TRACKER_IS_WRITEBACK(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_WRITEBACK))
#define TRACKER_IS_WRITEBACK_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),  TRACKER_TYPE_WRITEBACK))
#define TRACKER_WRITEBACK_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TRACKER_TYPE_WRITEBACK, TrackerWritebackClass))

typedef struct TrackerWriteback TrackerWriteback;
typedef struct TrackerWritebackClass TrackerWritebackClass;

struct TrackerWriteback {
	GObject parent_instance;
};

struct TrackerWritebackClass {
	GObjectClass parent_class;

	gboolean (* update_metadata) (TrackerWriteback *writeback,
	                              GPtrArray        *values,
	                              TrackerClient    *client);
};

GType                tracker_writeback_get_type          (void) G_GNUC_CONST;
gboolean             tracker_writeback_update_metadata   (TrackerWriteback *writeback,
                                                          GPtrArray        *values,
                                                          TrackerClient    *client);
TrackerMinerManager* tracker_writeback_get_miner_manager (void);

/* Entry functions to be defined by modules */
TrackerWriteback *   writeback_module_create             (GTypeModule      *module);
const gchar * const *writeback_module_get_rdf_types      (void);

G_END_DECLS

#endif /* __TRACKER_WRITEBACK_WRITEBACK_H__ */
