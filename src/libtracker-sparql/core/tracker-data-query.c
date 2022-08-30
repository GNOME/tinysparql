/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
 * Copyright (C) 2007, Jason Kivlighn <jkivlighn@gmail.com>
 * Copyright (C) 2007, Creative Commons <http://creativecommons.org>
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

#include "config.h"

#include <string.h>
#include <stdlib.h>

#include "tracker-class.h"
#include "tracker-data-manager.h"
#include "tracker-data-query.h"
#include "tracker-db-interface-sqlite.h"
#include "tracker-db-manager.h"
#include "tracker-ontologies.h"
#include "tracker-sparql.h"

gchar *
tracker_data_query_resource_urn (TrackerDataManager  *manager,
                                 TrackerDBInterface  *iface,
                                 TrackerRowid         id)
{
	TrackerDBStatement *stmt;
	gchar *uri = NULL;
	GArray *res = NULL;

	g_return_val_if_fail (id != 0, NULL);

	stmt = tracker_db_interface_create_statement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_SELECT, NULL,
	                                              "SELECT Uri FROM Resource WHERE ID = ?");
	if (!stmt)
		return NULL;

	tracker_db_statement_bind_int (stmt, 0, id);
	res = tracker_db_statement_get_values (stmt,
	                                       TRACKER_PROPERTY_TYPE_STRING,
	                                       NULL);
	g_object_unref (stmt);

	if (res && res->len == 1)
		uri = g_value_dup_string (&g_array_index (res, GValue, 0));

	g_clear_pointer (&res, g_array_unref);

	return uri;
}

TrackerRowid
tracker_data_query_resource_id (TrackerDataManager  *manager,
                                TrackerDBInterface  *iface,
                                const gchar         *uri,
                                GError             **error)
{
	TrackerDBStatement *stmt;
	GError *inner_error = NULL;
	TrackerRowid id = 0;
	GArray *res = NULL;

	g_return_val_if_fail (uri != NULL, 0);

	stmt = tracker_db_interface_create_statement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_SELECT, &inner_error,
	                                              "SELECT ID FROM Resource WHERE Uri = ?");

	if (stmt) {
		tracker_db_statement_bind_text (stmt, 0, uri);
		res = tracker_db_statement_get_values (stmt,
		                                       TRACKER_PROPERTY_TYPE_INTEGER,
		                                       &inner_error);
		g_object_unref (stmt);
	}

	if (res && res->len == 1)
		id = g_value_get_int64 (&g_array_index (res, GValue, 0));

	g_clear_pointer (&res, g_array_unref);

	if (G_UNLIKELY (inner_error)) {
		g_propagate_prefixed_error (error,
		                            inner_error,
		                            "Querying resource ID:");
		return 0;
	}

	return id;
}

TrackerDBCursor *
tracker_data_query_sparql_cursor (TrackerDataManager  *manager,
                                  const gchar         *query,
                                  GError             **error)
{
	TrackerSparql *sparql_query;
	TrackerSparqlCursor *cursor;

	g_return_val_if_fail (query != NULL, NULL);

	sparql_query = tracker_sparql_new (manager, query);

	cursor = tracker_sparql_execute_cursor (sparql_query, NULL, error);

	g_object_unref (sparql_query);

	return TRACKER_DB_CURSOR (cursor);
}

gboolean
tracker_data_query_string_to_value (TrackerDataManager   *manager,
                                    const gchar          *value,
                                    const gchar          *langtag,
                                    TrackerPropertyType   type,
                                    GValue               *gvalue,
                                    GError              **error)
{
	TrackerData *data;
	TrackerRowid object_id;
	gchar *datetime_str;
	GDateTime *datetime;

	switch (type) {
	case TRACKER_PROPERTY_TYPE_STRING:
		g_value_init (gvalue, G_TYPE_STRING);
		g_value_set_string (gvalue, value);
		break;
	case TRACKER_PROPERTY_TYPE_LANGSTRING:
		g_value_init (gvalue, G_TYPE_BYTES);
		g_value_take_boxed (gvalue, tracker_sparql_make_langstring (value, langtag));
		break;
	case TRACKER_PROPERTY_TYPE_INTEGER:
		g_value_init (gvalue, G_TYPE_INT64);
		g_value_set_int64 (gvalue, atoll (value));
		break;
	case TRACKER_PROPERTY_TYPE_BOOLEAN:
		/* use G_TYPE_INT64 to be compatible with value stored in DB
		   (important for value_equal function) */
		g_value_init (gvalue, G_TYPE_INT64);
		g_value_set_int64 (gvalue, g_ascii_strncasecmp (value, "true", 4) == 0);
		break;
	case TRACKER_PROPERTY_TYPE_DOUBLE:
		g_value_init (gvalue, G_TYPE_DOUBLE);
		g_value_set_double (gvalue, g_ascii_strtod (value, NULL));
		break;
	case TRACKER_PROPERTY_TYPE_DATE:
		g_value_init (gvalue, G_TYPE_INT64);
		datetime_str = g_strdup_printf ("%sT00:00:00Z", value);
		datetime = tracker_date_new_from_iso8601 (datetime_str, error);
		g_free (datetime_str);

		if (!datetime)
			return FALSE;

		g_value_set_int64 (gvalue, g_date_time_to_unix (datetime));
		g_date_time_unref (datetime);
		break;
	case TRACKER_PROPERTY_TYPE_DATETIME:
		g_value_init (gvalue, G_TYPE_DATE_TIME);
		datetime = tracker_date_new_from_iso8601 (value, error);

		if (!datetime)
			return FALSE;

		g_value_take_boxed (gvalue, datetime);
		break;
	case TRACKER_PROPERTY_TYPE_RESOURCE:
		data = tracker_data_manager_get_data (manager);
		object_id = tracker_data_update_ensure_resource (data, value, error);
		if (object_id == 0)
			return FALSE;

		g_value_init (gvalue, G_TYPE_INT64);
		g_value_set_int64 (gvalue, object_id);
		break;
	case TRACKER_PROPERTY_TYPE_UNKNOWN:
		g_warn_if_reached ();
		return FALSE;
	}

	return TRUE;
}
