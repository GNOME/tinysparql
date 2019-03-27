/*
 * Copyright (C) 2009, Adrien Bustany <abustany@gnome.org>
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

#ifndef __LIBTRACKER_MINER_ONLINE_H__
#define __LIBTRACKER_MINER_ONLINE_H__

#if !defined (__LIBTRACKER_MINER_H_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "Only <libtracker-miner/tracker-miner.h> can be included directly."
#endif

#include <libtracker-miner/tracker-miner-object.h>
#include <libtracker-miner/tracker-miner-enums.h>

#define TRACKER_TYPE_MINER_ONLINE         (tracker_miner_online_get_type())
#define TRACKER_MINER_ONLINE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_MINER_ONLINE, TrackerMinerOnline))
#define TRACKER_MINER_ONLINE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    TRACKER_TYPE_MINER_ONLINE, TrackerMinerOnlineClass))
#define TRACKER_IS_MINER_ONLINE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_MINER_ONLINE))
#define TRACKER_IS_MINER_ONLINE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    TRACKER_TYPE_MINER_ONLINE))
#define TRACKER_MINER_ONLINE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  TRACKER_TYPE_MINER_ONLINE, TrackerMinerOnlineClass))

G_BEGIN_DECLS

typedef struct _TrackerMinerOnline TrackerMinerOnline;
typedef struct _TrackerMinerOnlineClass TrackerMinerOnlineClass;

/**
 * TrackerMinerOnline:
 *
 * Abstract miner object for data requiring connectivity.
 **/
struct _TrackerMinerOnline {
	TrackerMiner parent_instance;
};

/**
 * TrackerMinerOnlineClass:
 * @parent_class: a #TrackerMinerClass
 * @connected: called when there is a network connection, or a new
 *   default route, returning #TRUE starts/resumes indexing.
 * @disconnected: called when there is no network connection.
 * @padding: Reserved for future API improvements.
 *
 * Virtual methods that can be overridden.
 *
 * Since: 0.18
 **/
struct _TrackerMinerOnlineClass {
	TrackerMinerClass parent_class;

	/* vmethods */
	gboolean (* connected)    (TrackerMinerOnline *miner,
	                           TrackerNetworkType  network);
	void     (* disconnected) (TrackerMinerOnline *miner);

	/* <Private> */
	gpointer padding[10];
};

GType               tracker_miner_online_get_type         (void) G_GNUC_CONST;

TrackerNetworkType  tracker_miner_online_get_network_type (TrackerMinerOnline *miner);

G_END_DECLS

#endif /* __LIBTRACKER_MINER_ONLINE_H__ */
