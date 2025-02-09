/*
 * Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
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

#include <tinysparql.h>

#define TRACKER_TYPE_BUS_CURSOR (tracker_bus_cursor_get_type ())
G_DECLARE_FINAL_TYPE (TrackerBusCursor,
                      tracker_bus_cursor,
                      TRACKER, BUS_CURSOR,
                      TrackerDeserializer);

TrackerSparqlCursor *tracker_bus_cursor_new (GInputStream *stream,
					     GVariant     *variables);
