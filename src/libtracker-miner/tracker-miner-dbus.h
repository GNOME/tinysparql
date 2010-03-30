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

#ifndef __LIBTRACKER_MINER_MINER_DBUS_H__
#define __LIBTRACKER_MINER_MINER_DBUS_H__

#if !defined (__LIBTRACKER_MINER_H_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "Only <libtracker-miner/tracker-miner.h> can be included directly."
#endif

#include <glib-object.h>
#include <dbus/dbus-glib-bindings.h>

#include "tracker-miner-object.h"

G_BEGIN_DECLS

#define TRACKER_MINER_DBUS_INTERFACE   "org.freedesktop.Tracker1.Miner"
#define TRACKER_MINER_DBUS_NAME_PREFIX "org.freedesktop.Tracker1.Miner."
#define TRACKER_MINER_DBUS_PATH_PREFIX "/org/freedesktop/Tracker1/Miner/"

void _tracker_miner_dbus_get_name           (TrackerMiner           *miner,
                                             DBusGMethodInvocation  *context,
                                             GError                **error);
void _tracker_miner_dbus_get_description    (TrackerMiner           *miner,
                                             DBusGMethodInvocation  *context,
                                             GError                **error);
void _tracker_miner_dbus_get_status         (TrackerMiner           *miner,
                                             DBusGMethodInvocation  *context,
                                             GError                **error);
void _tracker_miner_dbus_get_progress       (TrackerMiner           *miner,
                                             DBusGMethodInvocation  *context,
                                             GError                **error);
void _tracker_miner_dbus_get_pause_details  (TrackerMiner           *miner,
                                             DBusGMethodInvocation  *context,
                                             GError                **error);
void _tracker_miner_dbus_pause              (TrackerMiner           *miner,
                                             const gchar            *application,
                                             const gchar            *name,
                                             DBusGMethodInvocation  *context,
                                             GError                **error);
void _tracker_miner_dbus_resume             (TrackerMiner           *miner,
                                             gint                    cookie,
                                             DBusGMethodInvocation  *context,
                                             GError                **error);
void _tracker_miner_dbus_ignore_next_update (TrackerMiner           *miner,
                                             const GStrv             subjects,
                                             DBusGMethodInvocation  *context,
                                             GError                **error);

G_END_DECLS

#endif /* __LIBTRACKER_MINER_MINER_DBUS_H__ */
