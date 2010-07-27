/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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
 */

#ifndef __TRACKER_STORE_DBUS_H__
#define __TRACKER_STORE_DBUS_H__

#include <glib.h>

#include <dbus/dbus-glib-bindings.h>

#include "tracker-status.h"

G_BEGIN_DECLS

gboolean        tracker_dbus_init                    (void);
void            tracker_dbus_shutdown                (void);
gboolean        tracker_dbus_register_objects        (void);
GObject        *tracker_dbus_get_object              (GType type);
TrackerStatus  *tracker_dbus_register_notifier       (void);

G_END_DECLS

#endif /* __TRACKER_STORE_DBUS_H__ */
