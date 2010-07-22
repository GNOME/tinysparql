/*
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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

#include <glib/gstdio.h>

#include <libtracker-common/tracker-crc32.h>

#include <libtracker-data/tracker-db-journal.h>

static void
test_init_and_shutdown (void)
{
	gboolean result;

	/* check double init/shutdown */
	tracker_db_journal_set_rotating (FALSE, G_MAXSIZE, NULL);
	result = tracker_db_journal_init (NULL, FALSE);
	g_assert (result == TRUE);

	result = tracker_db_journal_shutdown ();
	g_assert (result == TRUE);

	tracker_db_journal_set_rotating (FALSE, G_MAXSIZE, NULL);
	result = tracker_db_journal_init (NULL, FALSE);
	g_assert (result == TRUE);

	result = tracker_db_journal_shutdown ();
	g_assert (result == TRUE);
}

static void
test_write_functions (void)
{
	gchar *path;
	const gchar *filename;
	gsize initial_size, actual_size;
	gboolean result;

	path = g_build_filename (TOP_BUILDDIR, "tests", "libtracker-db", "tracker-store.journal", NULL);
	g_unlink (path);

	tracker_db_journal_set_rotating (FALSE, G_MAXSIZE, NULL);
	tracker_db_journal_init (path, FALSE);

	filename = tracker_db_journal_get_filename ();
	g_assert (filename != NULL);
	g_assert_cmpstr (filename, ==, path);

	/* Size is 8 due to header */
	actual_size = tracker_db_journal_get_size ();
	g_assert_cmpint (actual_size, ==, 8);

	/* Check with rollback, nothing is added */
	initial_size = tracker_db_journal_get_size ();
	result = tracker_db_journal_start_transaction (time (NULL));
	g_assert_cmpint (result, ==, TRUE);
	result = tracker_db_journal_append_resource (10, "http://resource");
	g_assert_cmpint (result, ==, TRUE);
	result = tracker_db_journal_append_resource (11, "http://predicate");
	g_assert_cmpint (result, ==, TRUE);
	result = tracker_db_journal_append_delete_statement (0, 10, 11, "test");
	g_assert_cmpint (result, ==, TRUE);
	result = tracker_db_journal_rollback_transaction ();
	g_assert_cmpint (result, ==, TRUE);
	actual_size = tracker_db_journal_get_size ();
	g_assert_cmpint (initial_size, ==, actual_size);

	/* Check with commit, somethign is added */
	result = tracker_db_journal_start_transaction (time (NULL));
	g_assert_cmpint (result, ==, TRUE);
	result = tracker_db_journal_append_resource (12, "http://resource");
	g_assert_cmpint (result, ==, TRUE);
	result = tracker_db_journal_append_resource (13, "http://predicate");
	g_assert_cmpint (result, ==, TRUE);
	result = tracker_db_journal_append_resource (14, "http://resource");
	g_assert_cmpint (result, ==, TRUE);
	result = tracker_db_journal_append_delete_statement_id (0, 12, 13, 14);
	g_assert_cmpint (result, ==, TRUE);
	result = tracker_db_journal_commit_db_transaction ();
	g_assert_cmpint (result, ==, TRUE);
	actual_size = tracker_db_journal_get_size ();
	g_assert_cmpint (initial_size, !=, actual_size);

	/* Test insert statement */
	result = tracker_db_journal_start_transaction (time (NULL));
	g_assert_cmpint (result, ==, TRUE);
	result = tracker_db_journal_append_resource (15, "http://resource");
	g_assert_cmpint (result, ==, TRUE);
	result = tracker_db_journal_append_resource (16, "http://predicate");
	g_assert_cmpint (result, ==, TRUE);
	result = tracker_db_journal_append_insert_statement (0, 15, 16, "test");
	g_assert_cmpint (result, ==, TRUE);
	result = tracker_db_journal_commit_db_transaction ();
	g_assert_cmpint (result, ==, TRUE);

	/* Test insert id */
	result = tracker_db_journal_start_transaction (time (NULL));
	g_assert_cmpint (result, ==, TRUE);
	result = tracker_db_journal_append_resource (17, "http://resource");
	g_assert_cmpint (result, ==, TRUE);
	result = tracker_db_journal_append_resource (18, "http://predicate");
	g_assert_cmpint (result, ==, TRUE);
	result = tracker_db_journal_append_resource (19, "http://resource");
	g_assert_cmpint (result, ==, TRUE);
	result = tracker_db_journal_append_insert_statement_id (0, 17, 18, 19);
	g_assert_cmpint (result, ==, TRUE);
	result = tracker_db_journal_commit_db_transaction ();
	g_assert_cmpint (result, ==, TRUE);

	/* Test fsync */
	result = tracker_db_journal_fsync ();
	g_assert_cmpint (result, ==, TRUE);

	tracker_db_journal_shutdown ();

	g_free (path);
}

static void
test_read_functions (void)
{
	GError *error = NULL;
	gchar *path;
	gboolean result;
	TrackerDBJournalEntryType type;
	gint id, s_id, p_id, o_id;
	const gchar *uri, *str;

	path = g_build_filename (TOP_BUILDDIR, "tests", "libtracker-db", "tracker-store.journal", NULL);

	/* NOTE: we don't unlink here so we can use the data from the write tests */

	/* Create an iterator */
	result = tracker_db_journal_reader_init (path);
	g_assert_cmpint (result, ==, TRUE);

	type = tracker_db_journal_reader_get_type ();
	g_assert_cmpint (type, ==, TRACKER_DB_JOURNAL_START);

	/* First transaction */
	result = tracker_db_journal_reader_next (&error);
	g_assert_no_error (error);
	g_assert_cmpint (result, ==, TRUE);

	type = tracker_db_journal_reader_get_type ();
	g_assert_cmpint (type, ==, TRACKER_DB_JOURNAL_START_TRANSACTION);

	result = tracker_db_journal_reader_next (&error);
	g_assert_no_error (error);
	g_assert_cmpint (result, ==, TRUE);

	type = tracker_db_journal_reader_get_type ();
	g_assert_cmpint (type, ==, TRACKER_DB_JOURNAL_RESOURCE);

	result = tracker_db_journal_reader_get_resource (&id, &uri);
	g_assert_cmpint (result, ==, TRUE);
	g_assert_cmpint (id, ==, 12);
	g_assert_cmpstr (uri, ==, "http://resource");

	result = tracker_db_journal_reader_next (&error);
	g_assert_no_error (error);
	g_assert_cmpint (result, ==, TRUE);

	type = tracker_db_journal_reader_get_type ();
	g_assert_cmpint (type, ==, TRACKER_DB_JOURNAL_RESOURCE);

	result = tracker_db_journal_reader_get_resource (&id, &uri);
	g_assert_cmpint (result, ==, TRUE);
	g_assert_cmpint (id, ==, 13);
	g_assert_cmpstr (uri, ==, "http://predicate");

	result = tracker_db_journal_reader_next (&error);
	g_assert_no_error (error);
	g_assert_cmpint (result, ==, TRUE);

	type = tracker_db_journal_reader_get_type ();
	g_assert_cmpint (type, ==, TRACKER_DB_JOURNAL_RESOURCE);

	result = tracker_db_journal_reader_get_resource (&id, &uri);
	g_assert_cmpint (result, ==, TRUE);
	g_assert_cmpint (id, ==, 14);
	g_assert_cmpstr (uri, ==, "http://resource");

	result = tracker_db_journal_reader_next (&error);
	g_assert_no_error (error);
	g_assert_cmpint (result, ==, TRUE);

	type = tracker_db_journal_reader_get_type ();
	g_assert_cmpint (type, ==, TRACKER_DB_JOURNAL_DELETE_STATEMENT_ID);

	result = tracker_db_journal_reader_get_statement_id (NULL, &s_id, &p_id, &o_id);
	g_assert_cmpint (result, ==, TRUE);
	g_assert_cmpint (s_id, ==, 12);
	g_assert_cmpint (p_id, ==, 13);
	g_assert_cmpint (o_id, ==, 14);

	result = tracker_db_journal_reader_next (&error);
	g_assert_no_error (error);
	g_assert_cmpint (result, ==, TRUE);

	type = tracker_db_journal_reader_get_type ();
	g_assert_cmpint (type, ==, TRACKER_DB_JOURNAL_END_TRANSACTION);

	/* Second transaction */
	result = tracker_db_journal_reader_next (&error);
	g_assert_no_error (error);
	g_assert_cmpint (result, ==, TRUE);

	type = tracker_db_journal_reader_get_type ();
	g_assert_cmpint (type, ==, TRACKER_DB_JOURNAL_START_TRANSACTION);

	result = tracker_db_journal_reader_next (&error);
	g_assert_no_error (error);
	g_assert_cmpint (result, ==, TRUE);

	type = tracker_db_journal_reader_get_type ();
	g_assert_cmpint (type, ==, TRACKER_DB_JOURNAL_RESOURCE);

	result = tracker_db_journal_reader_get_resource (&id, &uri);
	g_assert_cmpint (result, ==, TRUE);
	g_assert_cmpint (id, ==, 15);
	g_assert_cmpstr (uri, ==, "http://resource");

	result = tracker_db_journal_reader_next (&error);
	g_assert_no_error (error);
	g_assert_cmpint (result, ==, TRUE);

	type = tracker_db_journal_reader_get_type ();
	g_assert_cmpint (type, ==, TRACKER_DB_JOURNAL_RESOURCE);

	result = tracker_db_journal_reader_get_resource (&id, &uri);
	g_assert_cmpint (result, ==, TRUE);
	g_assert_cmpint (id, ==, 16);
	g_assert_cmpstr (uri, ==, "http://predicate");

	result = tracker_db_journal_reader_next (&error);
	g_assert_no_error (error);
	g_assert_cmpint (result, ==, TRUE);

	type = tracker_db_journal_reader_get_type ();
	g_assert_cmpint (type, ==, TRACKER_DB_JOURNAL_INSERT_STATEMENT);

	result = tracker_db_journal_reader_get_statement (NULL, &s_id, &p_id, &str);
	g_assert_cmpint (result, ==, TRUE);
	g_assert_cmpint (s_id, ==, 15);
	g_assert_cmpint (p_id, ==, 16);
	g_assert_cmpstr (str, ==, "test");

	result = tracker_db_journal_reader_next (&error);
	g_assert_no_error (error);
	g_assert_cmpint (result, ==, TRUE);

	type = tracker_db_journal_reader_get_type ();
	g_assert_cmpint (type, ==, TRACKER_DB_JOURNAL_END_TRANSACTION);

	/* Third transaction */
	result = tracker_db_journal_reader_next (&error);
	g_assert_no_error (error);
	g_assert_cmpint (result, ==, TRUE);

	type = tracker_db_journal_reader_get_type ();
	g_assert_cmpint (type, ==, TRACKER_DB_JOURNAL_START_TRANSACTION);

	result = tracker_db_journal_reader_next (&error);
	g_assert_no_error (error);
	g_assert_cmpint (result, ==, TRUE);

	type = tracker_db_journal_reader_get_type ();
	g_assert_cmpint (type, ==, TRACKER_DB_JOURNAL_RESOURCE);

	result = tracker_db_journal_reader_get_resource (&id, &uri);
	g_assert_cmpint (result, ==, TRUE);
	g_assert_cmpint (id, ==, 17);
	g_assert_cmpstr (uri, ==, "http://resource");

	result = tracker_db_journal_reader_next (&error);
	g_assert_no_error (error);
	g_assert_cmpint (result, ==, TRUE);

	type = tracker_db_journal_reader_get_type ();
	g_assert_cmpint (type, ==, TRACKER_DB_JOURNAL_RESOURCE);

	result = tracker_db_journal_reader_get_resource (&id, &uri);
	g_assert_cmpint (result, ==, TRUE);
	g_assert_cmpint (id, ==, 18);
	g_assert_cmpstr (uri, ==, "http://predicate");

	result = tracker_db_journal_reader_next (&error);
	g_assert_no_error (error);
	g_assert_cmpint (result, ==, TRUE);

	type = tracker_db_journal_reader_get_type ();
	g_assert_cmpint (type, ==, TRACKER_DB_JOURNAL_RESOURCE);

	result = tracker_db_journal_reader_get_resource (&id, &uri);
	g_assert_cmpint (result, ==, TRUE);
	g_assert_cmpint (id, ==, 19);
	g_assert_cmpstr (uri, ==, "http://resource");

	result = tracker_db_journal_reader_next (&error);
	g_assert_no_error (error);
	g_assert_cmpint (result, ==, TRUE);

	type = tracker_db_journal_reader_get_type ();
	g_assert_cmpint (type, ==, TRACKER_DB_JOURNAL_INSERT_STATEMENT_ID);

	result = tracker_db_journal_reader_get_statement_id (NULL, &s_id, &p_id, &o_id);
	g_assert_cmpint (result, ==, TRUE);
	g_assert_cmpint (s_id, ==, 17);
	g_assert_cmpint (p_id, ==, 18);
	g_assert_cmpint (o_id, ==, 19);

	result = tracker_db_journal_reader_next (&error);
	g_assert_no_error (error);
	g_assert_cmpint (result, ==, TRUE);

	type = tracker_db_journal_reader_get_type ();
	g_assert_cmpint (type, ==, TRACKER_DB_JOURNAL_END_TRANSACTION);

	/* Shutdown */
	result = tracker_db_journal_reader_shutdown ();
	g_assert_cmpint (result, ==, TRUE);

	g_free (path);
}

int
main (int argc, char **argv) 
{
	gchar *path;
	int result;

	g_type_init ();
	g_thread_init (NULL);
	g_test_init (&argc, &argv, NULL);

	g_test_add_func ("/libtracker-db/tracker-db-journal/init-and-shutdown",
	                 test_init_and_shutdown);
	g_test_add_func ("/libtracker-db/tracker-db-journal/write-functions",
	                 test_write_functions);
	g_test_add_func ("/libtracker-db/tracker-db-journal/read-functions",
	                 test_read_functions);

	result = g_test_run ();

	/* Clean up */
	path = g_build_filename (TOP_BUILDDIR, "tests", "libtracker-db", "tracker-store.journal", NULL);
	g_unlink (path);
	g_free (path);

	return result;
}
