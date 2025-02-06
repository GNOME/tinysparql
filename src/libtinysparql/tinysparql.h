/*
 * Copyright (C) 2024, Red Hat Inc.
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

#define __LIBTRACKER_SPARQL_INSIDE__

#include "tracker-version-generated.h"
#include "tracker-version.h"
#include "tracker-enums.h"
#include "tracker-error.h"
#include "tracker-connection.h"
#include "tracker-batch.h"
#include "tracker-cursor.h"
#include "tracker-endpoint.h"
#include "tracker-endpoint-dbus.h"
#include "tracker-endpoint-http.h"
#include "tracker-version.h"
#include "tracker-ontologies.h"
#include "tracker-resource.h"
#include "tracker-statement.h"
#include "tracker-notifier.h"
#include "tracker-sparql-enum-types.h"
#include "tracker-utils.h"

#undef __LIBTRACKER_SPARQL_INSIDE__
