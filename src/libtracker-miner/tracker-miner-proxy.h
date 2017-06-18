/*
 * Copyright (C) 2017, Red Hat, Inc.
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
 *
 * Authors: Carlos Garnacho <carlosg@gnome.org>
 */

#ifndef __TRACKER_MINER_PROXY_H__
#define __TRACKER_MINER_PROXY_H__

#if !defined (__LIBTRACKER_MINER_H_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "Only <libtracker-miner/tracker-miner.h> can be included directly."
#endif

#include <glib-object.h>
#include <gio/gio.h>
#include "tracker-miner-object.h"

#define TRACKER_TYPE_MINER_PROXY         (tracker_miner_proxy_get_type())
#define TRACKER_MINER_PROXY(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_MINER_PROXY, TrackerMinerProxy))
#define TRACKER_MINER_PROXY_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    TRACKER_TYPE_MINER_PROXY, TrackerMinerProxyClass))
#define TRACKER_IS_MINER_PROXY(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_MINER_PROXY))
#define TRACKER_IS_MINER_PROXY_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    TRACKER_TYPE_MINER_PROXY))
#define TRACKER_MINER_PROXY_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  TRACKER_TYPE_MINER_PROXY, TrackerMinerProxyClass))

typedef struct _TrackerMinerProxy TrackerMinerProxy;
typedef struct _TrackerMinerProxyClass TrackerMinerProxyClass;

struct _TrackerMinerProxy {
	GObject parent_instance;
};

struct _TrackerMinerProxyClass {
	GObjectClass parent_class;
	/*<private>*/
	gpointer padding[10];
};

GType               tracker_miner_proxy_get_type (void) G_GNUC_CONST;

TrackerMinerProxy * tracker_miner_proxy_new      (TrackerMiner     *miner,
                                                  GDBusConnection  *connection,
                                                  const gchar      *dbus_path,
                                                  GCancellable     *cancellable,
                                                  GError          **error);

#endif /* __TRACKER_MINER_PROXY_H__ */
