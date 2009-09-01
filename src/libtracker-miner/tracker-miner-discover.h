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

#ifndef __LIBTRACKERMINER_DISCOVER_H__
#define __LIBTRACKERMINER_DISCOVER_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_MINER_DISCOVER         (tracker_miner_discover_get_type())
#define TRACKER_MINER_DISCOVER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_MINER_DISCOVER, TrackerMinerDiscover))
#define TRACKER_MINER_DISCOVER_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    TRACKER_TYPE_MINER_DISCOVER, TrackerMinerDiscoverClass))
#define TRACKER_IS_MINER_DISCOVER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_MINER_DISCOVER))
#define TRACKER_IS_MINER_DISCOVER_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    TRACKER_TYPE_MINER_DISCOVER))
#define TRACKER_MINER_DISCOVER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  TRACKER_TYPE_MINER_DISCOVER, TrackerMinerDiscoverClass))

typedef struct TrackerMinerDiscover TrackerMinerDiscover;
typedef struct TrackerMinerDiscoverClass TrackerMinerDiscoverClass;

struct TrackerMinerDiscover {
	GObject parent_instance;
};

struct TrackerMinerDiscoverClass {
	GObjectClass parent_class;
};

GType   tracker_miner_discover_get_type (void) G_GNUC_CONST;

TrackerMinerDiscover * tracker_miner_discover_new (void);

GSList *tracker_miner_discover_get_running   (TrackerMinerDiscover *discover);
GSList *tracker_miner_discover_get_available (TrackerMinerDiscover *discover);

G_END_DECLS

#endif /* __LIBTRACKERMINER_DISCOVER_H__ */
