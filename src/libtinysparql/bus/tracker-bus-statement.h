/*
 * Copyright (C) 2020, Red Hat Ltd.
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

#include "bus/tracker-bus.h"

#include "tracker-private.h"

#define TRACKER_TYPE_BUS_STATEMENT (tracker_bus_statement_get_type ())
G_DECLARE_FINAL_TYPE (TrackerBusStatement,
                      tracker_bus_statement,
                      TRACKER, BUS_STATEMENT,
                      TrackerSparqlStatement)

TrackerSparqlStatement * tracker_bus_statement_new (TrackerBusConnection *conn,
						    const gchar          *sparql);

TrackerSparqlStatement * tracker_bus_statement_new_update (TrackerBusConnection *conn,
                                                           const gchar          *query);
