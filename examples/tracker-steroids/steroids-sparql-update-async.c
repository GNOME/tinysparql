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

#include <libtracker-client/tracker-client.h>

#include <glib.h>

static TrackerClient *client;
static GMainLoop *main_loop;

static void
query_cb (GPtrArray *results,
          GError    *error,
          gpointer   user_data)
{
	gint i, j;

	if (error) {
		g_critical ("Update failed: %s", error->message);
		g_error_free (error);
		g_object_unref (client);
		g_main_loop_quit (main_loop);
		return;
	}

	for (i = 0; i < results->len; i++) {
		GPtrArray *inner_array;

		inner_array = g_ptr_array_index (results, i);

		for (j = 0; j < inner_array->len; j++) {
			GHashTable *hash;
			GHashTableIter iter;
			gpointer key, value;

			hash = g_ptr_array_index (inner_array, j);

			g_hash_table_iter_init (&iter, hash);

			while (g_hash_table_iter_next (&iter, &key, &value)) {
				g_print ("%s -> %s\n", (gchar*) key, (gchar*) value);
			}

			g_hash_table_unref (hash);
		}
	}

	g_ptr_array_free (results, TRUE);

	g_object_unref (client);

	g_main_loop_quit (main_loop);
}

int
main (int argc, char **argv) 
{
	const gchar *query;
	
	if (argc != 2) {
		g_printerr ("Usage: %s query\n", argv[0]);
		return EXIT_FAILURE;
	}

	query = argv[1];

	main_loop = g_main_loop_new (NULL, FALSE);

	client = tracker_client_new (0, 0);

	if (tracker_resources_sparql_update_blank_async (client, query, query_cb, NULL) == 0) {
		g_critical ("error running update");
		return EXIT_FAILURE;
	}

	g_main_loop_run (main_loop);

	return EXIT_SUCCESS;
}
