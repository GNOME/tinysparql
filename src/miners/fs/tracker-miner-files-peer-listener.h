/*
 * Copyright (C) 2015, Carlos Garnacho
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
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#ifndef __TRACKER_MINER_FILES_PEER_LISTENER_H__
#define __TRACKER_MINER_FILES_PEER_LISTENER_H__

#include <glib-object.h>
#include <gio/gio.h>

#define TRACKER_TYPE_MINER_FILES_PEER_LISTENER            (tracker_miner_files_peer_listener_get_type ())
#define TRACKER_MINER_FILES_PEER_LISTENER(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), TRACKER_TYPE_MINER_FILES_PEER_LISTENER, TrackerMinerFilesPeerListener))
#define TRACKER_MINER_FILES_PEER_LISTENER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TRACKER_TYPE_DBUS_MINER_FILES_PEER_LISTENER, TrackerMinerFilesPeerListenerClass))
#define TRACKER_IS_MINER_FILES_PEER_LISTENER(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), TRACKER_TYPE_MINER_FILES_PEER_LISTENER))
#define TRACKER_IS_MINER_FILES_PEER_LISTENER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TRACKER_TYPE_MINER_FILES_PEER_LISTENER))
#define TRACKER_MINER_FILES_PEER_LISTENER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TRACKER_TYPE_MINER_FILES_PEER_LISTENER, TrackerMinerFilesPeerListenerClass))

typedef struct _TrackerMinerFilesPeerListener TrackerMinerFilesPeerListener;
typedef struct _TrackerMinerFilesPeerListenerClass TrackerMinerFilesPeerListenerClass;

struct _TrackerMinerFilesPeerListener {
	GObject parent_instance;
};

struct _TrackerMinerFilesPeerListenerClass {
	GObjectClass parent_class;
};

GType tracker_miner_files_peer_listener_get_type     (void) G_GNUC_CONST;

TrackerMinerFilesPeerListener *
         tracker_miner_files_peer_listener_new          (GDBusConnection               *connection);

void     tracker_miner_files_peer_listener_add_watch    (TrackerMinerFilesPeerListener *listener,
                                                         const gchar                   *dbus_name,
                                                         GFile                         *file);
void     tracker_miner_files_peer_listener_remove_watch (TrackerMinerFilesPeerListener *listener,
                                                         const gchar                   *dbus_name,
                                                         GFile                         *file);

void     tracker_miner_files_peer_listener_remove_dbus_name (TrackerMinerFilesPeerListener *listener,
                                                             const gchar                   *dbus_name);
void     tracker_miner_files_peer_listener_remove_file      (TrackerMinerFilesPeerListener *listener,
                                                             GFile                         *file);
gboolean tracker_miner_files_peer_listener_is_file_watched  (TrackerMinerFilesPeerListener *listener,
                                                             GFile                         *file);

#endif /* __TRACKER_MINER_FILES_PEER_LISTENER_H__ */
