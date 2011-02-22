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

typedef struct {
	GMainLoop *loop;
	GTimer *timer;
} LoopTimer;

static void
on_connection (GObject *object, GAsyncResult *res, gpointer user_data)
{
	LoopTimer *lt = user_data;
	GError *error = NULL;
	gdouble ct;
	TrackerSparqlConnection *con = tracker_sparql_connection_get_finish (res, &error);

	ct = g_timer_elapsed (lt->timer, NULL);

	g_timer_start (lt->timer);

	if (!error) {
		TrackerSparqlCursor *cursor;
		cursor = tracker_sparql_connection_query (con, "SELECT ?r { ?r a rdfs:Resource }", NULL, NULL);
		while (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
			g_print ("%s\n", tracker_sparql_cursor_get_string (cursor, 0, NULL));
		}
		g_object_unref (cursor);
		g_object_unref (con);	
	} else {
		g_critical ("%s", error->message);
		g_error_free (error);
	}

	g_print ("Async construction took: %.6f\n", ct);
	g_print ("Query took: %.6f\n", g_timer_elapsed (lt->timer, NULL));

	g_main_loop_quit (lt->loop);
}

gint
main (gint argc, gchar *argv[])
{
	LoopTimer lt;

	g_type_init ();
	lt.loop = g_main_loop_new (NULL, FALSE);

	lt.timer = g_timer_new ();

	tracker_sparql_connection_get_async (NULL, on_connection, &lt);

	g_main_loop_run (lt.loop);

	g_timer_destroy (lt.timer);
	g_main_loop_unref (lt.loop);

	return 0;
}
