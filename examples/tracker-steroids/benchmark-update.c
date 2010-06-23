/*
 * Copyright (C) 2010, Codeminded BVBA <abustany@gnome.org>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libtracker-client/tracker-client.h>

#define N_TRIES 500

int
main (int argc, char **argv)
{
	const gchar *query;
	TrackerClient *client;
	GError *error = NULL;
	GTimer *timer;
	gint i;
	gdouble time_normal, time_steroids;

	if (argc != 2) {
		g_printerr ("Usage: %s query\n", argv[0]);
		return EXIT_FAILURE;
	}

	query = argv[1];

	client = tracker_client_new (0, 0);

	/* Run a first time to warm cache */
	for (i = 0; i < N_TRIES; i++) {
		tracker_resources_sparql_update (client, query, &error);

		if (error) {
			g_critical ("Query error: %s", error->message);
			g_error_free (error);
			return EXIT_FAILURE;
		}
	}

	timer = g_timer_new ();

	for (i = 0; i < N_TRIES; i++) {
		/* FIXME disable FD passing */
		tracker_resources_sparql_update (client, query, &error);

		if (error) {
			g_critical ("Query error: %s", error->message);
			g_error_free (error);
			g_timer_destroy (timer);
			return EXIT_FAILURE;
		}
	}

	time_normal = g_timer_elapsed (timer, NULL);

	for (i = 0; i < N_TRIES; i++) {
		tracker_resources_sparql_update (client, query, &error);

		if (error) {
			g_critical ("Query error: %s", error->message);
			g_error_free (error);
			g_timer_destroy (timer);
			return EXIT_FAILURE;
		}
	}

	time_steroids = g_timer_elapsed (timer, NULL);

	g_print ("Normal:   %f seconds\n", time_normal/N_TRIES);
	g_print ("Steroids: %f seconds\n", time_steroids/N_TRIES);
	g_print ("Speedup:  %f %%\n", 100 * (time_normal / time_steroids - 1));

	return EXIT_SUCCESS;
}
