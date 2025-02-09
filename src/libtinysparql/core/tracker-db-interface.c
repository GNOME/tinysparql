/*
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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

#include <string.h>

#include <gobject/gvaluecollector.h>

#include "tracker-db-interface.h"

GQuark
tracker_db_interface_error_quark (void)
{
	return g_quark_from_static_string ("tracker-db-interface-error-quark");
}

gboolean
tracker_db_interface_execute_query (TrackerDBInterface  *interface,
                                    GError             **error,
                                    const gchar         *query,
                                    ...)
{
	gboolean retval;
	va_list args;

	va_start (args, query);
	retval = tracker_db_interface_execute_vquery (interface,
	                                              error,
	                                              query,
	                                              args);
	va_end (args);

	return retval;
}

gboolean
tracker_db_interface_start_transaction (TrackerDBInterface *interface)
{
	GError *error = NULL;

	tracker_db_interface_execute_query (interface,
	                                    &error,
	                                    "BEGIN TRANSACTION");

	if (error) {
		g_warning ("%s", error->message);
		g_error_free (error);
		return FALSE;
	}

	return TRUE;
}

gboolean
tracker_db_interface_end_db_transaction (TrackerDBInterface  *interface,
                                         GError             **error)
{
	GError *internal_error = NULL;

	tracker_db_interface_execute_query (interface, &internal_error, "COMMIT");

	if (internal_error) {
		g_propagate_error (error, internal_error);
		/* We propagate the error, ROLLBACK happens later */
		return FALSE;
	}

	return TRUE;
}
