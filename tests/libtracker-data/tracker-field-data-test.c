/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008, Nokia (urho.konttori@nokia.com)
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

#include "config.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <libtracker-data/tracker-field-data.h>
#include <libtracker-common/tracker-common.h>

static void
test_get_set () 
{
        TrackerFieldData *fd;

        fd = tracker_field_data_new ();

        tracker_field_data_set_alias (fd, "aliasFD");
        tracker_field_data_set_table_name (fd, "nameFD");
        tracker_field_data_set_field_name (fd, "fieldNameFD");
        tracker_field_data_set_select_field (fd, "selectFieldFD");
        tracker_field_data_set_where_field (fd, "whereFieldFD");
        tracker_field_data_set_order_field (fd, "orderFieldFD");
        tracker_field_data_set_id_field (fd, "idFieldFD");
        tracker_field_data_set_data_type (fd, TRACKER_FIELD_TYPE_BLOB);
        tracker_field_data_set_multiple_values (fd, TRUE);
        tracker_field_data_set_is_select (fd, TRUE);
        tracker_field_data_set_is_condition (fd, TRUE);
        tracker_field_data_set_is_order (fd, TRUE);
        tracker_field_data_set_needs_join (fd, TRUE);
        tracker_field_data_set_needs_collate (fd, TRUE);
        tracker_field_data_set_needs_null (fd, TRUE);

        g_assert (g_strcmp0 (tracker_field_data_get_alias (fd), "aliasFD") == 0);
        g_assert (g_strcmp0 (tracker_field_data_get_table_name (fd), "nameFD") == 0);
        g_assert (g_strcmp0 (tracker_field_data_get_field_name (fd), "fieldNameFD") == 0);
        g_assert (g_strcmp0 (tracker_field_data_get_select_field (fd), "selectFieldFD") == 0);
        g_assert (g_strcmp0 (tracker_field_data_get_where_field (fd), "whereFieldFD") == 0);
        g_assert (g_strcmp0 (tracker_field_data_get_order_field (fd), "orderFieldFD") == 0);
        g_assert (g_strcmp0 (tracker_field_data_get_id_field (fd), "idFieldFD") == 0);
        g_assert (tracker_field_data_get_data_type (fd) == TRACKER_FIELD_TYPE_BLOB);
        g_assert (tracker_field_data_get_multiple_values (fd));
        g_assert (tracker_field_data_get_is_select (fd));
        g_assert (tracker_field_data_get_is_condition (fd));
        g_assert (tracker_field_data_get_is_order (fd));
        g_assert (tracker_field_data_get_needs_join (fd));
        g_assert (tracker_field_data_get_needs_collate (fd));
        g_assert (tracker_field_data_get_needs_null (fd));

        g_object_unref (fd);
}

static void
test_props ()
{
        TrackerFieldData *obj;

        obj = g_object_new (TRACKER_TYPE_FIELD_DATA, 
                            "alias", "alias value", 
                            "table-name", "table name value",
                            "field-name", "field name value",
                            "select-field", "select field value",
                            "where-field", "where field value",
                            "order-field", "order field value",
                            "id-field", "id field value",
                            "data-type", TRACKER_FIELD_TYPE_BLOB,
                            "multiple-values", TRUE,
                            "is-select", TRUE,
                            "is-condition", TRUE,
                            "is-order", TRUE,
                            "needs-join", FALSE,
                            "needs-collate", FALSE,
                            "needs-null", TRUE,
                            NULL);
        
        const gchar *value = NULL;
        g_object_get (obj, "alias", &value, NULL);
        g_assert (g_strcmp0 (value, "alias value") == 0);

        g_object_get (obj, "table-name", &value, NULL);
        g_assert (g_strcmp0 (value, "table name value") == 0);

        g_object_get (obj, "field-name", &value, NULL);
        g_assert (g_strcmp0 (value, "field name value") == 0);

        g_object_get (obj, "select-field", &value, NULL);
        g_assert (g_strcmp0 (value, "select field value") == 0);

        g_object_get (obj, "where-field", &value, NULL);
        g_assert (g_strcmp0 (value, "where field value") == 0);

        g_object_get (obj, "order-field", &value, NULL);
        g_assert (g_strcmp0 (value, "order field value") == 0);

        g_object_get (obj, "id-field", &value, NULL);
        g_assert (g_strcmp0 (value, "id field value") == 0);

        TrackerFieldType type;
        g_object_get (obj, "data-type", &type, NULL);
        g_assert (type == TRACKER_FIELD_TYPE_BLOB);

        gboolean bool_value;
        g_object_get (obj, "multiple-values", &bool_value, NULL);
        g_assert (bool_value == TRUE);
        g_object_get (obj, "is-select", &bool_value, NULL);
        g_assert (bool_value == TRUE);
        g_object_get (obj, "is-condition", &bool_value, NULL); 
        g_assert (bool_value == TRUE);
        g_object_get (obj, "is-order", &bool_value, NULL); 
        g_assert (bool_value == TRUE);
        g_object_get (obj, "needs-join", &bool_value, NULL);
        g_assert (bool_value == FALSE);
        g_object_get (obj, "needs-collate", &bool_value, NULL);
        g_assert (bool_value == FALSE);
        g_object_get (obj, "needs-null", &bool_value, NULL);
        g_assert (bool_value == TRUE);
                                                                       
}

gint
main (gint argc, gchar **argv) 
{

	g_type_init ();
	g_test_init (&argc, &argv, NULL);

        g_test_add_func ("/libtracker-data/tracker-field-data/get_set",
                         test_get_set);

        g_test_add_func ("/libtracker-data/tracker-field-data/props",
                         test_props);

	return g_test_run ();
}
