/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2009, Nokia (urho.konttori@nokia.com)
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

#ifndef __LIBTRACKERMINER_MINER_H__
#define __LIBTRACKERMINER_MINER_H__

#include <glib-object.h>
#include <libtracker-client/tracker.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_MINER	   (tracker_miner_get_type())
#define TRACKER_MINER(o)	   (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_MINER, TrackerMiner))
#define TRACKER_MINER_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    TRACKER_TYPE_MINER, TrackerMinerClass))
#define TRACKER_IS_MINER(o)	   (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_MINER))
#define TRACKER_IS_MINER_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    TRACKER_TYPE_MINER))
#define TRACKER_MINER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  TRACKER_TYPE_MINER, TrackerMinerClass))

#define TRACKER_MINER_DBUS_INTERFACE   "org.freedesktop.Tracker1.Miner"
#define TRACKER_MINER_DBUS_NAME_PREFIX "org.freedesktop.Tracker1.Miner."
#define TRACKER_MINER_DBUS_PATH_PREFIX "/org/freedesktop/Tracker1/Miner/"

#define TRACKER_MINER_ERROR_DOMAIN     "TrackerMiner"
#define TRACKER_MINER_ERROR	       tracker_miner_error_quark()

typedef struct TrackerMiner TrackerMiner;
typedef struct TrackerMinerClass TrackerMinerClass;
typedef struct TrackerMinerPrivate TrackerMinerPrivate;

struct TrackerMiner {
        GObject parent_instance;
        TrackerMinerPrivate *private;
};

struct TrackerMinerClass {
        GObjectClass parent_class;

        /* signals */
        void (* started)    (TrackerMiner *miner);
        void (* stopped)    (TrackerMiner *miner);

        void (* paused)     (TrackerMiner *miner);
        void (* resumed)    (TrackerMiner *miner);

	void (* terminated) (TrackerMiner *miner);

	void (* progress)   (TrackerMiner *miner,
			     const gchar  *status,
			     gdouble       progress);

	void (* error)      (TrackerMiner *miner,
			     GError       *error);
};

GType          tracker_miner_get_type       (void) G_GNUC_CONST;
GQuark	       tracker_miner_error_quark    (void);

void           tracker_miner_start          (TrackerMiner  *miner);
void           tracker_miner_stop           (TrackerMiner  *miner);

gboolean       tracker_miner_is_started     (TrackerMiner  *miner);

TrackerClient *tracker_miner_get_client     (TrackerMiner  *miner);
gboolean       tracker_miner_execute_sparql (TrackerMiner  *miner,
					     const gchar   *sparql,
					     GError       **error);
gboolean       tracker_miner_commit         (TrackerMiner  *miner);

gint           tracker_miner_pause          (TrackerMiner  *miner,
					     const gchar   *application,
					     const gchar   *reason,
					     GError       **error);
gboolean       tracker_miner_resume         (TrackerMiner  *miner,
					     gint           cookie,
					     GError       **error);


G_END_DECLS

#endif /* __LIBTRACKERMINER_MINER_H__ */
