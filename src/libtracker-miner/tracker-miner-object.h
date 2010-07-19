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

#ifndef __LIBTRACKER_MINER_OBJECT_H__
#define __LIBTRACKER_MINER_OBJECT_H__

#if !defined (__LIBTRACKER_MINER_H_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "Only <libtracker-miner/tracker-miner.h> can be included directly."
#endif

#include <glib-object.h>
#include <gio/gio.h>

#include <libtracker-sparql/tracker-sparql.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_MINER         (tracker_miner_get_type())
#define TRACKER_MINER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_MINER, TrackerMiner))
#define TRACKER_MINER_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    TRACKER_TYPE_MINER, TrackerMinerClass))
#define TRACKER_IS_MINER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_MINER))
#define TRACKER_IS_MINER_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    TRACKER_TYPE_MINER))
#define TRACKER_MINER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  TRACKER_TYPE_MINER, TrackerMinerClass))

#define TRACKER_MINER_ERROR_DOMAIN "TrackerMiner"
#define TRACKER_MINER_ERROR        tracker_miner_error_quark()

typedef struct TrackerMiner TrackerMiner;
typedef struct TrackerMinerPrivate TrackerMinerPrivate;

/**
 * TrackerMiner:
 *
 * Abstract miner object.
 **/
struct TrackerMiner {
	GObject parent_instance;
	TrackerMinerPrivate *private;
};

/**
 * TrackerMinerClass:
 * @parent_class: parent object class.
 * @started: Called when the miner is told to start collecting data.
 * @stopped: Called when the miner is told to stop collecting data.
 * @paused: Called when the miner is told to pause.
 * @resumed: Called when the miner is told to resume activity.
 * @progress: progress.
 * @error: error.
 * @ignore_next_update: Called after ignore on next update event happens.
 *
 * Virtual methods left to implement.
 **/
typedef struct {
	GObjectClass parent_class;

	/* signals */
	void (* started)            (TrackerMiner *miner);
	void (* stopped)            (TrackerMiner *miner);

	void (* paused)             (TrackerMiner *miner);
	void (* resumed)            (TrackerMiner *miner);

	void (* progress)           (TrackerMiner *miner,
	                             const gchar  *status,
	                             gdouble       progress);

	void (* ignore_next_update) (TrackerMiner *miner,
	                             const GStrv   urls);
} TrackerMinerClass;

GType            tracker_miner_get_type                    (void) G_GNUC_CONST;
GQuark           tracker_miner_error_quark                 (void);

void             tracker_miner_start                       (TrackerMiner         *miner);
void             tracker_miner_stop                        (TrackerMiner         *miner);
void             tracker_miner_ignore_next_update          (TrackerMiner         *miner,
                                                            const GStrv           urls);
gboolean         tracker_miner_is_started                  (TrackerMiner         *miner);
gint             tracker_miner_pause                       (TrackerMiner         *miner,
                                                            const gchar          *reason,
                                                            GError              **error);
gboolean         tracker_miner_resume                      (TrackerMiner         *miner,
                                                            gint                  cookie,
                                                            GError              **error);

TrackerSparqlConnection *
                 tracker_miner_get_connection              (TrackerMiner         *miner);

G_END_DECLS

#endif /* __LIBTRACKER_MINER_OBJECT_H__ */
