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
	GPtrArray *updates, *solutions;
	GHashTable *blank_nodes[2];

	error = NULL;

	tracker_db_journal_set_rotating (FALSE, G_MAXSIZE, NULL);

	/* initialization */
	tracker_data_manager_init (TRACKER_DB_MANAGER_FORCE_REINDEX,
	                           NULL,
	                           NULL,
	                           FALSE,
	                           NULL,
	                           NULL,
	                           NULL);

	/* perform update in transaction */

	updates = tracker_data_update_sparql_blank (
	                                            "INSERT { _:foo a rdfs:Resource } "
	                                            "INSERT { _:foo a rdfs:Resource . _:bar a rdfs:Resource } ",
	                                            &error);
	g_assert_no_error (error);

	g_assert_cmpint (updates->len, ==, 2);

	solutions = updates->pdata[0];
	g_assert_cmpint (solutions->len, ==, 1);
	blank_nodes[0] = solutions->pdata[0];

	solutions = updates->pdata[1];
	g_assert_cmpint (solutions->len, ==, 1);
	blank_nodes[1] = solutions->pdata[0];

	g_assert (g_hash_table_lookup (blank_nodes[0], "foo") != NULL);
	g_assert (g_hash_table_lookup (blank_nodes[1], "foo") != NULL);
	g_assert (g_hash_table_lookup (blank_nodes[1], "bar") != NULL);

	g_assert_cmpstr (g_hash_table_lookup (blank_nodes[0], "foo"), !=, g_hash_table_lookup (blank_nodes[1], "foo"));

	/* cleanup */

	g_hash_table_unref (blank_nodes[0]);
	g_hash_table_unref (blank_nodes[1]);
	g_ptr_array_free (updates->pdata[0], TRUE);
	g_ptr_array_free (updates->pdata[1], TRUE);
	g_ptr_array_free (updates, TRUE);

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
