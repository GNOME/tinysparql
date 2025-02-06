/*
 * Copyright (C) 2018, Red Hat, Inc.
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

#pragma once

#include "tracker-direct.h"

#include <tinysparql.h>

#include "tracker-private.h"

#define TRACKER_TYPE_DIRECT_BATCH (tracker_direct_batch_get_type ())

G_DECLARE_FINAL_TYPE (TrackerDirectBatch,
                      tracker_direct_batch,
                      TRACKER, DIRECT_BATCH,
                      TrackerBatch)

struct _TrackerDirectBatch
{
	TrackerBatch parent_instance;
};

TrackerBatch * tracker_direct_batch_new (TrackerSparqlConnection *conn);

gboolean tracker_direct_batch_update (TrackerDirectBatch  *batch,
                                      TrackerDataManager  *data_manager,
                                      GError             **error);
