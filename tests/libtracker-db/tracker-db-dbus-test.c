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
#include <glib.h>
#include <glib/gtestutils.h>

#include <libtracker-db/tracker-db-dbus.h>
#include <libtracker-db/tracker-db-interface.h>


static TrackerDBResultSet *
get_mock_tracker_db_result (gint results, gint columns, gboolean set_null) {

	TrackerDBResultSet *mock;
	gint i, j;

	mock = _tracker_db_result_set_new (columns);

	for (i = 0; i < results; i++) {
		_tracker_db_result_set_append (mock);

		for (j = 0; j < columns; j++) {

			GValue value = {0,};
			gchar * text = g_strdup_printf ("value %d", i);

			g_value_init (&value, G_TYPE_STRING);
			g_value_set_string (&value, (set_null ? NULL : text));
			_tracker_db_result_set_set_value (mock, j, &value);

			g_value_unset (&value);
			g_free (text);
		}
	}

	tracker_db_result_set_rewind (mock);

	return mock;

}



static void
test_dbus_query_result_to_strv ()
{

	TrackerDBResultSet *result_set = NULL;
	gchar **result;
	gint	count;

	/* NULL */
	result = tracker_dbus_query_result_to_strv (result_set, 0, &count);
	g_assert (result == NULL);

	/* 5 results, 1 column */
	result_set = get_mock_tracker_db_result (5, 1, FALSE);
	result = tracker_dbus_query_result_to_strv (result_set, 0, &count);

	g_assert_cmpint (count, ==, 5);
	g_assert_cmpint (g_strv_length (result), ==, 5);

	g_strfreev (result);
	g_object_unref (result_set);

	/* 0 results, 1 columns */
	result_set = get_mock_tracker_db_result (0, 1, FALSE);
	if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR)) {
		result = tracker_dbus_query_result_to_strv (result_set, 0, &count);
	}
	g_test_trap_assert_failed ();
	/* Should raise g_critical (priv->array...); */

	g_object_unref (result_set);


	/* 1 result ... NULL */
	result_set = get_mock_tracker_db_result (1, 1, TRUE);
	result = tracker_dbus_query_result_to_strv (result_set, 0, &count);

	g_assert_cmpint (count, ==, 0);

	g_strfreev (result);
	g_object_unref (result_set);

}

static void
test_dbus_query_result_to_hash_table ()
{
	/* TODO: Implement */
	g_print ("- Unimplemented -\n");
}

static void
free_string_ptr_array (GPtrArray *array)
{
	g_ptr_array_foreach (array, (GFunc)g_strfreev, NULL);
	g_ptr_array_free (array, TRUE);
}

static void
test_dbus_query_result_to_ptr_array ()
{
	TrackerDBResultSet *result_set = NULL;
	GPtrArray *result = NULL;

	/* NULL */
	result = tracker_dbus_query_result_to_ptr_array (result_set);
	g_assert_cmpint (result->len, ==, 0);
	free_string_ptr_array (result);

	/* 5 results, 1 column */
	result_set = get_mock_tracker_db_result (5, 1, FALSE);
	result = tracker_dbus_query_result_to_ptr_array (result_set);

	g_assert_cmpint (result->len, ==, 5);
	free_string_ptr_array (result);

	g_object_unref (result_set);

	/* 0 results, 1 columns */
	result_set = get_mock_tracker_db_result (0, 1, FALSE);
	if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR)) {
		result = tracker_dbus_query_result_to_ptr_array (result_set);
		free_string_ptr_array (result);
	}
	g_test_trap_assert_failed ();
	/* Should raise g_critical (priv->array...); */

	g_object_unref (result_set);

	/*  1 result ... NULL */
	result_set = get_mock_tracker_db_result (1, 1, TRUE);
	result = tracker_dbus_query_result_to_ptr_array (result_set);
	g_assert_cmpint (result->len, ==, 1);
	free_string_ptr_array (result);

	g_object_unref (result_set);
}

gint
main (gint argc, gchar **argv)
{
	int result;

	g_type_init ();
	g_thread_init (NULL);
	g_test_init (&argc, &argv, NULL);

	g_test_add_func ("/libtracker-db/tracker-db-dbus/query_result_to_strv",
			 test_dbus_query_result_to_strv);
	g_test_add_func ("/libtracker-db/tracker-db-dbus/query_result_to_hash_table",
			 test_dbus_query_result_to_hash_table);
	g_test_add_func ("/libtracker-db/tracker-db-dbus/query_result_to_ptr_array",
			 test_dbus_query_result_to_ptr_array);


	result = g_test_run ();

	/* End */

	return result;
}
