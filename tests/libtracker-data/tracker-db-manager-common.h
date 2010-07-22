/*
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
 *
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

#ifndef __TRACKER_DB_MANAGER_TEST_COMMON__
#define __TRACKER_DB_MANAGER_TEST_COMMON__

#include <glib.h>

#include <libtracker-data/tracker-data.h>

gboolean test_assert_query_run (TrackerDB db, const gchar *query);
gboolean test_assert_query_run_on_iface (TrackerDBInterface *iface, const gchar *query);

#endif
