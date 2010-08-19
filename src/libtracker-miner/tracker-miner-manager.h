/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
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

#ifndef __LIBTRACKER_MINER_MANAGER_H__
#define __LIBTRACKER_MINER_MANAGER_H__

#if !defined (__LIBTRACKER_MINER_H_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "Only <libtracker-miner/tracker-miner.h> can be included directly."
#endif

#include <glib-object.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_MINER_MANAGER         (tracker_miner_manager_get_type())
#define TRACKER_MINER_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_MINER_MANAGER, TrackerMinerManager))
#define TRACKER_MINER_MANAGER_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    TRACKER_TYPE_MINER_MANAGER, TrackerMinerManagerClass))
#define TRACKER_IS_MINER_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_MINER_MANAGER))
#define TRACKER_IS_MINER_MANAGER_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    TRACKER_TYPE_MINER_MANAGER))
#define TRACKER_MINER_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  TRACKER_TYPE_MINER_MANAGER, TrackerMinerManagerClass))

typedef struct TrackerMinerManager TrackerMinerManager;
typedef struct TrackerMinerManagerClass TrackerMinerManagerClass;

/**
 * TrackerMinerManager:
 *
 * Object to query and control miners.
 **/
struct TrackerMinerManager {
	GObject parent_instance;
};

/**
 * TrackerMinerManagerClass:
 *
 * #TrackerMinerManager class.
 **/
struct TrackerMinerManagerClass {
	GObjectClass parent_class;

	void (* miner_progress)    (TrackerMinerManager *manager,
	                            const gchar         *miner_name,
	                            const gchar         *status,
	                            gdouble              progress);
	void (* miner_paused)      (TrackerMinerManager *manager,
	                            const gchar         *miner_name);
	void (* miner_resumed)     (TrackerMinerManager *manager,
	                            const gchar         *miner_name);
	void (* miner_activated)   (TrackerMinerManager *manager,
	                            const gchar         *miner_name);
	void (* miner_deactivated) (TrackerMinerManager *manager,
	                            const gchar         *miner_name);
};

GType                tracker_miner_manager_get_type           (void) G_GNUC_CONST;

TrackerMinerManager *tracker_miner_manager_new                (void);
GSList *             tracker_miner_manager_get_running        (TrackerMinerManager  *manager);
GSList *             tracker_miner_manager_get_available      (TrackerMinerManager  *manager);
gboolean             tracker_miner_manager_pause              (TrackerMinerManager  *manager,
                                                               const gchar          *miner,
                                                               const gchar          *reason,
                                                               guint32              *cookie);
gboolean             tracker_miner_manager_resume             (TrackerMinerManager  *manager,
                                                               const gchar          *miner,
                                                               guint32               cookie);
gboolean             tracker_miner_manager_is_active          (TrackerMinerManager  *manager,
                                                               const gchar          *miner);
gboolean             tracker_miner_manager_is_paused          (TrackerMinerManager  *manager,
                                                               const gchar          *miner,
                                                               GStrv                *applications,
                                                               GStrv                *reasons);
gboolean             tracker_miner_manager_get_status         (TrackerMinerManager  *manager,
                                                               const gchar          *miner,
                                                               gchar               **status,
                                                               gdouble              *progress);
gboolean             tracker_miner_manager_ignore_next_update (TrackerMinerManager  *manager,
                                                               const gchar          *miner,
                                                               const gchar         **urls);
const gchar *        tracker_miner_manager_get_display_name   (TrackerMinerManager  *manager,
                                                               const gchar          *miner);
const gchar *        tracker_miner_manager_get_description    (TrackerMinerManager  *manager,
                                                               const gchar          *miner);

void                 tracker_miner_manager_index_file         (TrackerMinerManager  *manager,
                                                               GFile                *file,
                                                               GError              **error);

G_END_DECLS

#endif /* __LIBTRACKER_MINER_MANAGER_H__ */
