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

#ifndef __TRACKER_MINER_DBUS_HELPER_H__
#define __TRACKER_MINER_DBUS_HELPER_H__

#include <libtracker-common/tracker-dbus.h>
#include "tracker-miner.h"

typedef void (* TrackerMinerDBusNameFunc) (TrackerMiner *miner,
                                           const gchar  *name,
                                           gboolean      available);

void tracker_miner_dbus_init           (TrackerMiner             *miner,
                                        const DBusGObjectInfo    *info);
void tracker_miner_dbus_shutdown       (TrackerMiner             *miner);
void tracker_miner_dbus_add_name_watch (TrackerMiner             *miner,
                                        const gchar              *name,
                                        TrackerMinerDBusNameFunc  func);

#endif /* __TRACKER_MINER_DBUS_HELPER_H__ */
