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

#include <gio/gio.h>

#include <libtracker-sparql/tracker-sparql.h>

static TrackerSparqlConnection *con;

static void
handle_statement (gint subject, gint predicate)
{
	gchar *query, *pred;
	TrackerSparqlCursor *cursor;

	query = g_strdup_printf ("SELECT tracker:uri (%d) tracker:uri(%d) {}",
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
class_signal_cb (GDBusConnection *connection,
                 const gchar     *sender_name,
                 const gchar     *object_path,
                 const gchar     *interface_name,
                 const gchar     *signal_name,
                 GVariant        *parameters,
                 gpointer         user_data)

{
	GVariantIter *iter1, *iter2;
	gchar *class_name;
	gint graph = 0, subject = 0, predicate = 0, object = 0;

	g_variant_get (parameters, "(&sa(iiii)a(iiii))", &class_name, &iter1, &iter2);
	g_print ("%s:\n", class_name);

	while (g_variant_iter_loop (iter1, "(iiii)", &graph, &subject, &predicate, &object)) {
		handle_statement (subject, predicate);
	}

	while (g_variant_iter_loop (iter2, "(iiii)", &graph, &subject, &predicate, &object)) {
		handle_statement (subject, predicate);
	}

	g_variant_iter_free (iter1);
	g_variant_iter_free (iter2);
}

gint
main (gint argc, gchar *argv[])
{
	GMainLoop *loop;
	GError *error = NULL;
	GDBusConnection *connection;
	guint signal_id;

	g_type_init ();
	loop = g_main_loop_new (NULL, FALSE);
	con = tracker_sparql_connection_get (NULL, &error);
	connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);

	signal_id = g_dbus_connection_signal_subscribe (connection,
	                                                TRACKER_DBUS_SERVICE,
	                                                TRACKER_DBUS_INTERFACE_RESOURCES,
	                                                "GraphUpdated",
	                                                TRACKER_DBUS_OBJECT_RESOURCES,
	                                                NULL, /* Use class-name here */
	                                                G_DBUS_SIGNAL_FLAGS_NONE,
	                                                class_signal_cb,
	                                                NULL,
	                                                NULL);

	g_main_loop_run (loop);
	g_dbus_connection_signal_unsubscribe (connection, signal_id);
	g_main_loop_unref (loop);
	g_object_unref (con);
	g_object_unref (connection);

	return 0;
}
