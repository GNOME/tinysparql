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

#ifndef __LIBTRACKER_MINER_DBUS_H__
#define __LIBTRACKER_MINER_DBUS_H__

#if !defined (__LIBTRACKER_MINER_H_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "Only <libtracker-miner/tracker-miner.h> can be included directly."
#endif

#include <libtracker-common/tracker-dbus.h>

#include "tracker-miner-object.h"

typedef void (* TrackerMinerDBusNameFunc)   (TrackerMiner             *miner,
                                             const gchar              *name,
                                             gboolean                  available);

void _tracker_miner_dbus_init               (TrackerMiner             *miner,
                                             const DBusGObjectInfo    *info);
void _tracker_miner_dbus_shutdown           (TrackerMiner             *miner);


#endif /* __LIBTRACKER_MINER_DBUS_H__ */
