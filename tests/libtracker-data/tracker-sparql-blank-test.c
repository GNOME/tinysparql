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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include <string.h>

#include <glib.h>
#include <gio/gio.h>

#include <libtracker-data/tracker-data-manager.h>
#include <libtracker-data/tracker-data-query.h>
#include <libtracker-data/tracker-data-update.h>
#include <libtracker-data/tracker-data.h>
#include <libtracker-data/tracker-sparql-query.h>

static void
test_blank (void)
{
	GError *error;
	GVariant *updates;
	GVariantIter iter;
	GVariant *rows;
	guint len = 0;
	gchar *solutions[3][3];

	error = NULL;

	tracker_db_journal_set_rotating (FALSE, G_MAXSIZE, NULL);

	/* initialization */
	tracker_data_manager_init (TRACKER_DB_MANAGER_FORCE_REINDEX,
	                           NULL,
	                           NULL,
	                           FALSE,
	                           100,
	                           100,
	                           NULL,
	                           NULL,
	                           NULL,
	                           &error);

	g_assert_no_error (error);

	/* perform update in transaction */

	updates = tracker_data_update_sparql_blank ("INSERT { _:foo a rdfs:Resource } "
	                                            "INSERT { _:foo a rdfs:Resource . _:bar a rdfs:Resource } ",
	                                            &error);
	g_assert_no_error (error);
	g_assert (updates != NULL);

	g_variant_iter_init (&iter, updates);
	while ((rows = g_variant_iter_next_value (&iter))) {
		GVariantIter sub_iter;
		GVariant *sub_value;

		g_variant_iter_init (&sub_iter, rows);

		while ((sub_value = g_variant_iter_next_value (&sub_iter))) {
			gchar *a = NULL, *b = NULL;
			GVariantIter sub_sub_iter;
			GVariant *sub_sub_value;

			g_variant_iter_init (&sub_sub_iter, sub_value);

			while ((sub_sub_value = g_variant_iter_next_value (&sub_sub_iter))) {
				g_variant_get (sub_sub_value, "{ss}", &a, &b);
				solutions[len][0] = a;
				solutions[len][1] = b;
				len++;
				g_assert_cmpint (len, <=, 3);
				g_variant_unref (sub_sub_value);
			}
			g_variant_unref (sub_value);
		}
		g_variant_unref (rows);
	}

	g_assert_cmpint (len, ==, 3);

	g_assert_cmpstr (solutions[0][0], ==, "foo");
	g_assert (solutions[0][1] != NULL);

	g_assert_cmpstr (solutions[1][0], ==, "foo");
	g_assert (solutions[1][1] != NULL);

	g_assert_cmpstr (solutions[2][0], ==, "bar");
	g_assert (solutions[2][1] != NULL);

	/* cleanup */

	g_free (solutions[0][0]);
	g_free (solutions[0][1]);
	g_free (solutions[1][0]);
	g_free (solutions[1][1]);
	g_free (solutions[2][0]);
	g_free (solutions[2][1]);

	g_variant_unref (updates);

	tracker_data_manager_shutdown ();
}

int
main (int argc, char **argv)
{
	gint result;
	gchar *current_dir;

	g_type_init ();

	if (!g_thread_supported ()) {
		g_thread_init (NULL);
	}

	g_test_init (&argc, &argv, NULL);

	current_dir = g_get_current_dir ();

	g_setenv ("XDG_DATA_HOME", current_dir, TRUE);
	g_setenv ("XDG_CACHE_HOME", current_dir, TRUE);
	g_setenv ("TRACKER_DB_SQL_DIR", TOP_SRCDIR "/data/db/", TRUE);
	g_setenv ("TRACKER_DB_ONTOLOGIES_DIR", TOP_SRCDIR "/data/ontologies/", TRUE);

	g_test_add_func ("/libtracker-data/sparql-blank", test_blank);

	/* run tests */

	result = g_test_run ();

	/* clean up */
	g_print ("Removing temporary data\n");
	g_spawn_command_line_sync ("rm -R tracker/", NULL, NULL, NULL, NULL);

	return result;
}
