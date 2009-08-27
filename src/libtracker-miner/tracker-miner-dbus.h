/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008, Nokia

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

#ifndef __TRACKER_MINER_DBUS_H__
#define __TRACKER_MINER_DBUS_H__

#include <glib-object.h>

#include "tracker-miner.h"

G_BEGIN_DECLS

void tracker_miner_dbus_get_name        (TrackerMiner           *miner,
					 DBusGMethodInvocation  *context,
					 GError                **error);
void tracker_miner_dbus_get_description (TrackerMiner           *miner,
					 DBusGMethodInvocation  *context,
					 GError                **error);
void tracker_miner_dbus_get_status      (TrackerMiner           *miner,
					 DBusGMethodInvocation  *context,
					 GError                **error);
void tracker_miner_dbus_get_progress    (TrackerMiner           *miner,
					 DBusGMethodInvocation  *context,
					 GError                **error);
void tracker_miner_dbus_get_is_paused   (TrackerMiner           *miner,
					 DBusGMethodInvocation  *context,
					 GError                **error);
void tracker_miner_dbus_pause           (TrackerMiner           *miner,
					 const gchar            *application,
					 const gchar            *name,
					 DBusGMethodInvocation  *context,
					 GError                **error);
void tracker_miner_dbus_resume          (TrackerMiner           *miner,
					 gint                    cookie,
					 DBusGMethodInvocation  *context,
					 GError                **error);

G_END_DECLS

#endif /* __TRACKER_MINER_DBUS_H__ */
