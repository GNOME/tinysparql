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
#include "tracker-db-manager-common.h"

gboolean
test_assert_query_run (TrackerDB db, const gchar *query)
{
	TrackerDBInterface *iface;

	iface = tracker_db_manager_get_db_interface (db);

	return test_assert_query_run_on_iface (iface, query);

}

gboolean
test_assert_query_run_on_iface (TrackerDBInterface *iface, const gchar *query)
{
	TrackerDBResultSet *result_set;
	GError *error = NULL;

	result_set = tracker_db_interface_execute_query (iface,
	                                                 &error,
	                                                 query);

	if (error && error->message) {
		g_warning ("Error loading query:'%s' - %s", query, error->message);
		g_error_free (error);
		return FALSE;
	}

	return TRUE;
}
