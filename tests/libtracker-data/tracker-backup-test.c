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

#include <libtracker-common/tracker-common.h>

#include <libtracker-data/tracker-data-backup.h>
#include <libtracker-data/tracker-data-manager.h>
#include <libtracker-data/tracker-data-query.h>
#include <libtracker-data/tracker-data-update.h>
#include <libtracker-data/tracker-data.h>
#include <libtracker-data/tracker-sparql-query.h>

static gint backup_calls = 0;
static GMainLoop *loop = NULL;

static void
backup_finished_cb (GError *error, gpointer user_data)
{
	g_assert (TRUE);
	backup_calls += 1;

	if (loop != NULL) {
		/* backup callback, quit main loop */
		g_main_loop_quit (loop);
	}
}

static gboolean
check_content_in_db (gint expected_instances, gint expected_relations)
{
	GError *error = NULL;
	const gchar  *query_instances_1 = "SELECT ?u WHERE { ?u a foo:class1. }";
	const gchar  *query_relation = "SELECT ?a ?b WHERE { ?a foo:propertyX ?b }";
	TrackerDBResultSet *result_set;

	result_set = tracker_data_query_sparql (query_instances_1, &error);
	g_assert_no_error (error);
	if (expected_instances == 0) {
		g_assert (result_set == NULL);
	} else {
		g_assert_cmpint (tracker_db_result_set_get_n_rows (result_set), ==, expected_instances);
		g_object_unref (result_set);
	}

	result_set = tracker_data_query_sparql (query_relation, &error);
	g_assert_no_error (error);
	if (expected_relations == 0) {
		g_assert (result_set == NULL);
	} else {
		g_assert_cmpint (tracker_db_result_set_get_n_rows (result_set), ==, expected_relations);
		g_object_unref (result_set);
	}

	return TRUE;
}
/*
 * Load ontology a few instances
 * Run a couple of queries to check it is ok
 * Back-up. 
 * Remove the DB.
 * Restore
 * Run again the queries
 */
static void
test_backup_and_restore_helper (gboolean journal)
{
	gchar  *data_prefix, *data_filename, *backup_filename, *db_location, *meta_db;
	GError *error = NULL;
	GFile  *backup_file;
	gchar *test_schemas[5] = { NULL, NULL, NULL, NULL, NULL };

	db_location = g_build_path (G_DIR_SEPARATOR_S, g_get_current_dir (), "tracker", NULL);
	data_prefix = g_build_path (G_DIR_SEPARATOR_S, 
	                            TOP_SRCDIR, "tests", "libtracker-data", "backup", "backup",
	                            NULL);

	/*
	 * This function uses $(data_prefix).ontology
	 */ 
	test_schemas[0] = g_build_path (G_DIR_SEPARATOR_S, TOP_SRCDIR, "tests", "libtracker-data", "ontologies", "20-dc", NULL);
	test_schemas[1] = g_build_path (G_DIR_SEPARATOR_S, TOP_SRCDIR, "tests", "libtracker-data", "ontologies", "31-nao", NULL);
	test_schemas[2] = g_build_path (G_DIR_SEPARATOR_S, TOP_SRCDIR, "tests", "libtracker-data", "ontologies", "90-tracker", NULL);
	test_schemas[3] = data_prefix;

	tracker_db_journal_set_rotating (FALSE, G_MAXSIZE, NULL);

	tracker_data_manager_init (TRACKER_DB_MANAGER_FORCE_REINDEX,
	                           (const gchar **) test_schemas,
	                           NULL, FALSE, NULL, NULL, NULL);

	/* load data set */
	data_filename = g_strconcat (data_prefix, ".data", NULL);
	if (g_file_test (data_filename, G_FILE_TEST_IS_REGULAR)) {
		tracker_turtle_reader_load (data_filename, &error);
		g_assert_no_error (error);
	} else {
		g_assert_not_reached ();
	}
	g_free (data_filename);


	/* Check everything is correct */
	check_content_in_db (3, 1);

	backup_filename = g_build_path (G_DIR_SEPARATOR_S, 
	                                db_location, "tracker.dump",
	                                NULL);
	backup_file = g_file_new_for_path (backup_filename);
	g_free (backup_filename);
	tracker_data_backup_save (backup_file,
	                          backup_finished_cb,
	                          NULL,
	                          NULL);

	/* Backup is asynchronous, wait until it is finished */
	loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (loop);
	g_main_loop_unref (loop);
	loop = NULL;

	tracker_data_manager_shutdown ();

	meta_db = g_build_path (G_DIR_SEPARATOR_S, db_location, "meta.db", NULL);
	g_unlink (meta_db);
	g_free (meta_db);

	if (!journal) {
		meta_db = g_build_path (G_DIR_SEPARATOR_S, db_location, "data", "tracker-store.journal", NULL);
		g_unlink (meta_db);
		g_free (meta_db);
	}

	meta_db = g_build_path (G_DIR_SEPARATOR_S, db_location, "data", ".meta.isrunning", NULL);
	g_unlink (meta_db);
	g_free (meta_db);

	tracker_db_journal_set_rotating (FALSE, G_MAXSIZE, NULL);

	tracker_data_manager_init (TRACKER_DB_MANAGER_FORCE_REINDEX,
	                           (const gchar **) test_schemas,
	                           NULL, FALSE, NULL, NULL, NULL);
	check_content_in_db (0, 0);

	test_schemas[0] = data_prefix;
	tracker_data_backup_restore (backup_file, backup_finished_cb, NULL, NULL, (const gchar **) test_schemas, NULL, NULL);
	check_content_in_db (3, 1);

	g_free (test_schemas[0]);
	g_free (test_schemas[1]);

	g_assert_cmpint (backup_calls, ==, 2);

	tracker_data_manager_shutdown ();
}

static void
test_backup_and_restore (void)
{
	test_backup_and_restore_helper (FALSE);
	backup_calls = 0;
}

static void
test_journal_then_backup_and_restore (void)
{
	test_backup_and_restore_helper (TRUE);
	backup_calls = 0;
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

	g_free (current_dir);

	g_test_add_func ("/tracker/libtracker-data/backup/journal_then_save_and_restore",
	                 test_journal_then_backup_and_restore);

	g_test_add_func ("/tracker/libtracker-data/backup/save_and_restore",
	                 test_backup_and_restore);

	/* run tests */
	result = g_test_run ();

	/* clean up */
	g_print ("Removing temporary data\n");
	g_spawn_command_line_sync ("rm -R tracker/", NULL, NULL, NULL, NULL);

	return result;
}
