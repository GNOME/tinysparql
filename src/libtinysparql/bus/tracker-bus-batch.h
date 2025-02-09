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

#include "bus/tracker-bus.h"

#include "tracker-private.h"

#define TRACKER_TYPE_BUS_BATCH (tracker_bus_batch_get_type ())
G_DECLARE_FINAL_TYPE (TrackerBusBatch,
                      tracker_bus_batch,
                      TRACKER, BUS_BATCH,
                      TrackerBatch)

TrackerBatch * tracker_bus_batch_new (TrackerBusConnection *connection);
