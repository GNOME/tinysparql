/*
 * Copyright (C) 2021, Red Hat Ltd.
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

#include "tracker-private.h"

#define TRACKER_TYPE_REMOTE_STATEMENT (tracker_remote_statement_get_type ())
G_DECLARE_FINAL_TYPE (TrackerRemoteStatement,
                      tracker_remote_statement,
                      TRACKER, REMOTE_STATEMENT,
                      TrackerSparqlStatement)

TrackerSparqlStatement * tracker_remote_statement_new (TrackerSparqlConnection  *conn,
                                                       const gchar              *query,
                                                       GError                  **error);
