/*
 * Copyright (C) 2021, Red Hat Inc.
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

#define TRACKER_TYPE_REMOTE_NAMESPACE_MANAGER (tracker_remote_namespace_manager_get_type ())
G_DECLARE_FINAL_TYPE (TrackerRemoteNamespaceManager,
                      tracker_remote_namespace_manager,
                      TRACKER, REMOTE_NAMESPACE_MANAGER,
                      TrackerNamespaceManager)

TrackerNamespaceManager * tracker_remote_namespace_manager_new (TrackerSparqlConnection *conn);

void tracker_remote_namespace_manager_new_async (TrackerSparqlConnection *conn,
                                                 GAsyncReadyCallback      callback,
                                                 gpointer                 user_data);

TrackerNamespaceManager * tracker_remote_namespace_manager_new_finish (GAsyncResult  *res,
                                                                       GError       **error);
