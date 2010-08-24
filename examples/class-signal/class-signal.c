/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.          See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <dbus/dbus-glib-bindings.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <libtracker-sparql/tracker-sparql.h>

#define TRACKER_SERVICE                 "org.freedesktop.Tracker1"
#define TRACKER_RESOURCES_OBJECT        "/org/freedesktop/Tracker1/Resources"
#define TRACKER_INTERFACE_RESOURCES     "org.freedesktop.Tracker1.Resources"

#define DBUS_MATCH_STR	"type='signal', " \
	"sender='" TRACKER_SERVICE "', " \
	"path='" TRACKER_RESOURCES_OBJECT "', " \
	"interface='" TRACKER_INTERFACE_RESOURCES "'"

static TrackerSparqlConnection *con;

static void
handle_statement (gint subject, gint predicate)
{
	gchar *query, *pred;
	TrackerSparqlCursor *cursor;

	query = g_strdup_printf ("SELECT tracker:subject (%d) tracker:subject(%d) {}",
	                         subject, predicate);
	cursor = tracker_sparql_connection_query (con, query, NULL, NULL);
	g_free (query);
	tracker_sparql_cursor_next (cursor, NULL, NULL);
	pred = g_strdup (tracker_sparql_cursor_get_string (cursor, 1, NULL));
	query = g_strdup_printf ("SELECT ?t { <%s> <%s> ?t }",
	                         tracker_sparql_cursor_get_string (cursor, 0, NULL),
	                         pred);
	g_object_unref (cursor);
	cursor = tracker_sparql_connection_query (con, query, NULL, NULL);
	g_free (query);
	while (tracker_sparql_cursor_next (cursor, NULL, NULL))
		g_print ("\t%s = %s\n", pred, tracker_sparql_cursor_get_string (cursor, 0, NULL));
	g_print ("\n");
	g_free (pred);
	g_object_unref (cursor);
}

static void
class_signal_cb (DBusMessage *message)
{
	DBusMessageIter iter, arr;
	gchar *class_name;
	gint arg_type, i;

	dbus_message_iter_init (message, &iter);
	dbus_message_iter_get_basic (&iter, &class_name);
	g_print ("%s:\n", class_name);

	for (i = 0; i < 2; i++) {
		dbus_message_iter_next (&iter);
		dbus_message_iter_recurse (&iter, &arr);

		while ((arg_type = dbus_message_iter_get_arg_type (&arr)) != DBUS_TYPE_INVALID) {
			DBusMessageIter strct;
			gint subject = 0, predicate = 0, object = 0;

			dbus_message_iter_recurse (&arr, &strct);
			dbus_message_iter_get_basic (&strct, &subject);
			dbus_message_iter_next (&strct);
			dbus_message_iter_get_basic (&strct, &predicate);
			dbus_message_iter_next (&strct);
			dbus_message_iter_get_basic (&strct, &object);
			handle_statement (subject, predicate);
			dbus_message_iter_next (&arr);
		}
	}
}

static DBusHandlerResult
message_filter (DBusConnection *connection, DBusMessage *message, gpointer ud)
{
	if (dbus_message_is_signal (message, TRACKER_INTERFACE_RESOURCES, "ClassSignal")) {
		class_signal_cb (message);
		return DBUS_HANDLER_RESULT_HANDLED;
	}
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}


gint
main (gint argc, gchar *argv[])
{
	GMainLoop *loop;
	GError *error = NULL;
	DBusConnection *connection;

	g_type_init ();
	loop = g_main_loop_new (NULL, FALSE);
	con = tracker_sparql_connection_get (&error);
	connection = dbus_bus_get_private (DBUS_BUS_SESSION, NULL);
	dbus_bus_request_name (connection, TRACKER_SERVICE, 0, NULL);
	dbus_connection_add_filter (connection, message_filter, NULL, NULL);
	dbus_bus_add_match (connection, DBUS_MATCH_STR, NULL);
	dbus_connection_setup_with_g_main (connection, NULL);
	g_main_loop_run (loop);
	g_main_loop_unref (loop);
	g_object_unref (con);
	dbus_connection_unref (connection);

	return 0;
}
