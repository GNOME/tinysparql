/*
 * Copyright (C) 2016 Carlos Garnacho <carlosg@gnome.org>
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
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#pragma once

#include <tinysparql.h>

#define TRACKER_TYPE_REMOTE_CONNECTION (tracker_remote_connection_get_type())
G_DECLARE_FINAL_TYPE (TrackerRemoteConnection,
		      tracker_remote_connection,
		      TRACKER, REMOTE_CONNECTION,
		      TrackerSparqlConnection)

TrackerRemoteConnection *tracker_remote_connection_new (const gchar *base_uri);
