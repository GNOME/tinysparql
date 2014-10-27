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
#include <locale.h>

#include <glib/gstdio.h>
#include <gio/gio.h>

#include <libtracker-common/tracker-common.h>
#include <libtracker-data/tracker-data.h>

static gchar *tests_data_dir = NULL;
static gchar *xdg_location = NULL;
static gint backup_calls = 0;
static GMainLoop *loop = NULL;

typedef struct _TestInfo TestInfo;

struct _TestInfo {
	const gchar *test_name;
	gboolean use_journal;
};

const TestInfo tests[] = {
	{ "journal_then_save_and_restore", TRUE },
	{ "save_and_resource", FALSE },
	{ NULL }
};

static void
backup_finished_cb (GError   *error,
                    gpointer  user_data)
{
	g_assert (TRUE);
	backup_calls += 1;

	if (loop != NULL) {
		/* backup callback, quit main loop */
		g_main_loop_quit (loop);
	}
}

static gboolean
check_content_in_db (gint expected_instances,
                     gint expected_relations)
{
	GError *error = NULL;
	const gchar  *query_instances_1 = "SELECT ?u WHERE { ?u a foo:class1. }";
	const gchar  *query_relation = "SELECT ?a ?b WHERE { ?a foo:propertyX ?b }";
	TrackerDBCursor *cursor;
	gint n_rows;

	cursor = tracker_data_query_sparql_cursor (query_instances_1, &error);
	g_assert_no_error (error);
	n_rows = 0;
	while (tracker_db_cursor_iter_next (cursor, NULL, &error)) {
		n_rows++;
	}
	g_assert_no_error (error);
	g_assert_cmpint (n_rows, ==, expected_instances);
	g_object_unref (cursor);

	cursor = tracker_data_query_sparql_cursor (query_relation, &error);
	g_assert_no_error (error);
	n_rows = 0;
	while (tracker_db_cursor_iter_next (cursor, NULL, &error)) {
		n_rows++;
	}
	g_assert_no_error (error);
	g_assert_cmpint (n_rows, ==, expected_relations);
	g_object_unref (cursor);

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
	gchar  *data_prefix, *data_filename, *backup_location, *backup_filename, *db_location, *meta_db;
	GError *error = NULL;
	GFile  *backup_file;
	gchar *test_schemas[5] = { NULL, NULL, NULL, NULL, NULL };

	db_location = g_build_path (G_DIR_SEPARATOR_S, xdg_location, "tracker", NULL);
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
	                           NULL, FALSE, FALSE,
	                           100, 100, NULL, NULL, NULL, &error);

	g_assert_no_error (error);

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

	backup_location = g_build_filename (db_location, "backup", NULL);
	g_mkdir (backup_location, 0777);
	backup_filename = g_build_filename (backup_location, "tracker.dump", NULL);
	backup_file = g_file_new_for_path (backup_filename);
	g_free (backup_filename);
	g_free (backup_location);
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

#ifndef DISABLE_JOURNAL
	if (!journal) {
		meta_db = g_build_path (G_DIR_SEPARATOR_S, db_location, "data", "tracker-store.journal", NULL);
		g_unlink (meta_db);
		g_free (meta_db);

		meta_db = g_build_path (G_DIR_SEPARATOR_S, db_location, "data", "tracker-store.ontology.journal", NULL);
		g_unlink (meta_db);
		g_free (meta_db);
	}
#endif /* DISABLE_JOURNAL */

	meta_db = g_build_path (G_DIR_SEPARATOR_S, db_location, "data", ".meta.isrunning", NULL);
	g_unlink (meta_db);
	g_free (meta_db);

#ifndef DISABLE_JOURNAL
	tracker_db_journal_set_rotating (FALSE, G_MAXSIZE, NULL);
#endif /* DISABLE_JOURNAL */

	tracker_data_manager_init (TRACKER_DB_MANAGER_FORCE_REINDEX,
	                           (const gchar **) test_schemas,
	                           NULL, FALSE, FALSE,
	                           100, 100, NULL, NULL, NULL, &error);

	g_assert_no_error (error);

	check_content_in_db (0, 0);

	tracker_data_backup_restore (backup_file, (const gchar **) test_schemas, NULL, NULL, &error);
	g_assert_no_error (error);
	check_content_in_db (3, 1);

	g_free (test_schemas[0]);
	g_free (test_schemas[1]);
	g_free (test_schemas[2]);
	g_free (test_schemas[3]);

	g_assert_cmpint (backup_calls, ==, 1);

	tracker_data_manager_shutdown ();
}

static void
test_backup_and_restore (TestInfo      *info,
                         gconstpointer  context)
{
	gint index;

	index = GPOINTER_TO_INT (context);
	*info = tests[index];

	test_backup_and_restore_helper (info->use_journal);
	backup_calls = 0;
}

static void
setup (TestInfo      *info,
       gconstpointer  context)
{
	gint i;

	i = GPOINTER_TO_INT (context);
	*info = tests[i];

	/* Sadly, we can't use ONE location per test because GLib
	 * caches XDG env vars, so g_get_*dir() will not change if we
	 * update the environment, this sucks majorly.
	 */
	if (!xdg_location) {
		gchar *basename;

		/* NOTE: g_test_build_filename() doesn't work env vars G_TEST_* are not defined?? */
		basename = g_strdup_printf ("%d", g_test_rand_int_range (0, G_MAXINT));
		xdg_location = g_build_path (G_DIR_SEPARATOR_S, tests_data_dir, basename, NULL);
		g_free (basename);

		g_assert_true (g_setenv ("XDG_DATA_HOME", xdg_location, TRUE));
		g_assert_true (g_setenv ("XDG_CACHE_HOME", xdg_location, TRUE));
		g_assert_true (g_setenv ("TRACKER_DB_ONTOLOGIES_DIR", TOP_SRCDIR "/src/ontologies/", TRUE));
	}
}

static void
teardown (TestInfo      *info,
          gconstpointer  context)
{
	gchar *cleanup_command;

	/* clean up */
	g_print ("Removing temporary data (%s)\n", xdg_location);

	cleanup_command = g_strdup_printf ("rm -Rf %s/", xdg_location);
	g_spawn_command_line_sync (cleanup_command, NULL, NULL, NULL, NULL);
	g_free (cleanup_command);

	g_free (xdg_location);
	xdg_location = NULL;
}

int
main (int argc, char **argv)
{
	gint result;
	gchar *current_dir;

	setlocale (LC_COLLATE, "en_US.utf8");

	current_dir = g_get_current_dir ();
	tests_data_dir = g_build_path (G_DIR_SEPARATOR_S, current_dir, "test-data", NULL);
	g_free (current_dir);

	g_test_init (&argc, &argv, NULL);

	g_test_add ("/libtracker-data/backup/journal_then_save_and_restore", TestInfo, GINT_TO_POINTER(0), setup, test_backup_and_restore, teardown);
	g_test_add ("/libtracker-data/backup/save_and_restore", TestInfo, GINT_TO_POINTER(1), setup, test_backup_and_restore, teardown);

	result = g_test_run ();

	return result;
}
