/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
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

#ifndef __LIBTRACKER_DB_H__
#define __LIBTRACKER_DB_H__

G_BEGIN_DECLS

#if !defined (TRACKER_ENABLE_INTERNALS) && !defined (TRACKER_COMPILATION)
#error "TRACKER_ENABLE_INTERNALS not defined, this must be defined to use tracker's internal functions"
#endif

#define __LIBTRACKER_DB_INSIDE__

#include "tracker-db-dbus.h"
#include "tracker-db-interface.h"
#include "tracker-db-interface-sqlite.h"
#include "tracker-db-journal.h"
#include "tracker-db-manager.h"
#include "tracker-db-config.h"

#undef __LIBTRACKER_DB_INSIDE__

G_END_DECLS

#endif /* __LIBTRACKER_DB_H__ */
