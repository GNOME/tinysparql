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

int
main (int argc, char **argv)
{
	const gchar *query;
	TrackerClient *client;
	GError *error = NULL;
	GPtrArray *results;
	TrackerResultIterator *iterator;
	gchar buffer[1024 * 1024];
	GTimer *timer;
	gint i, j;
	gdouble time_normal, time_steroids;

	if (argc != 2) {
		fprintf (stderr, "Usage: %s query\n", argv[0]);
		return EXIT_FAILURE;
	}

	query = argv[1];

	client = tracker_client_new (0, 0);

	timer = g_timer_new ();

	results = tracker_resources_sparql_query (client, query, &error);

	if (error) {
		g_critical ("Query error: %s", error->message);
		g_error_free (error);
		g_timer_destroy (timer);
		return EXIT_FAILURE;
	}

	for (i = 0; i < results->len; i++) {
		GStrv row = g_ptr_array_index (results, i);

		for (j = 0; row[j]; j++) {
			memcpy (buffer, row[j], g_utf8_strlen (row[j], -1));
		}
	}

	time_normal = g_timer_elapsed (timer, NULL);

	g_ptr_array_free (results, TRUE);

	iterator = tracker_resources_sparql_query_iterate (client, query, &error);

	while (tracker_result_iterator_next (iterator)) {
		for (i = 0; i < tracker_result_iterator_n_columns (iterator); i++) {
			const char *data;

			data = tracker_result_iterator_value (iterator, i);
			memcpy (buffer, data, g_utf8_strlen (data, -1));
		}
	}

	time_steroids = g_timer_elapsed (timer, NULL);

	tracker_result_iterator_free (iterator);

	g_print ("Normal:   %f seconds\n", time_normal);
	g_print ("Steroids: %f seconds\n", time_steroids);
	g_print ("Speedup:  %f %%\n", 100 * (time_normal / time_steroids - 1));

	return EXIT_SUCCESS;
}
