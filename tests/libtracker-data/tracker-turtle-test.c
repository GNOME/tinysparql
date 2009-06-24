/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008, Nokia (urho.konttori@nokia.com)
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

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

#include <libtracker-common/tracker-common.h>

#include <libtracker-data/tracker-turtle.h>
#include <libtracker-data/tracker-data-metadata.h>

#include <tracker-test-helpers.h>

#define TEST_FILE "./test-file.ttl"

static void
clean_test_file (void) 
{
	if (g_file_test (TEST_FILE, G_FILE_TEST_EXISTS)) {
		g_unlink (TEST_FILE);
	}
}

static void
count_statements_turtle_cb (void *user_data, const TrackerRaptorStatement *triple)
{
	gint *counter = (gint *)user_data;
	(*counter)++;
}

static gint
count_stmt_in_file (const gchar *filename) 
{
	gint counter = 0;

	tracker_turtle_process (filename, 
				".", 
				count_statements_turtle_cb, 
				&counter);
	return counter;
}

static void
init_ontology ()
{
	TrackerField *field, *field2;

	field = tracker_field_new ();
	tracker_field_set_id (field, "1");
	tracker_field_set_name (field, "test:cool");
	tracker_field_set_data_type (field, TRACKER_FIELD_TYPE_STRING);

	field2 = tracker_field_new ();
	tracker_field_set_id (field2, "2");
	tracker_field_set_name (field2, "test:playcount");
	tracker_field_set_multiple_values (field2, FALSE);
	tracker_field_set_data_type (field2, TRACKER_FIELD_TYPE_INTEGER);

	tracker_ontology_init ();

	tracker_ontology_field_add (field);
	tracker_ontology_field_add (field2);
}

static void
shutdown_ontology ()
{
	tracker_ontology_shutdown ();
}

static void
test_initialization (void)
{
	tracker_turtle_init ();
	tracker_turtle_shutdown ();
	tracker_turtle_init ();
	tracker_turtle_shutdown ();
}

static void
test_open_close (void)
{
	TurtleFile *file = NULL;

	clean_test_file ();

	tracker_turtle_init ();
	
	/* Open and close NULL */
	if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR)) {
		file = tracker_turtle_open (NULL);
	}
	g_test_trap_assert_failed ();

	if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR)) {
		tracker_turtle_close (file);
	}
	g_test_trap_assert_failed ();

	/* Force a NULL return */
	file = tracker_turtle_open ("/unlikely/that/exists/file.ttl");
	g_assert (file == NULL);

	file = tracker_turtle_open ("./test-file.ttl");
	tracker_turtle_close (file);

	g_assert (g_file_test (TEST_FILE, G_FILE_TEST_EXISTS));
	g_unlink (TEST_FILE);
}

static void
test_process (void)
{
	TurtleFile *file;
	gint        counter = 0;

	/* No initialization */
	if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR)) {
		tracker_turtle_process ("./data/100-stmt.ttl", 
					".", 
					count_statements_turtle_cb, 
					&counter);
	}
	g_test_trap_assert_failed ();


	tracker_turtle_init ();

	file = tracker_turtle_open ("./data/100-stmt.ttl");

	tracker_turtle_process ("./data/100-stmt.ttl", 
				".", 
				count_statements_turtle_cb, 
				&counter);
	g_assert_cmpint (counter, ==, 100);

	tracker_turtle_shutdown ();
}

static void
test_transaction (void)
{
	TurtleFile   *file = NULL;
	const gchar  *uri_pattern = "file:///a/b/c/%d.mp3";
	const gchar  *mock_uri = "file:///1/2/3.mp3";
	TrackerField *prop = NULL;
	const gchar  *value = "True";
	gint i;

	clean_test_file ();
	prop = tracker_field_new ();
	tracker_field_set_name (prop, "test:cool");

	/* Uninitialized */
	if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR)) {
		tracker_turtle_add_triple (NULL, mock_uri, prop, value);
	}
	g_test_trap_assert_failed ();

	/* Initialized but NULL as parameter */
	tracker_turtle_init ();
	if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR)) {
		tracker_turtle_add_triple (NULL, mock_uri, NULL, value);
	}
	g_test_trap_assert_failed ();
	
	/* Create a file, but NULL parameters */
	file = tracker_turtle_open (TEST_FILE);
	g_assert (file != NULL);
	if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR)) {
		tracker_turtle_add_triple (file, NULL, NULL, NULL);
	}
	g_test_trap_assert_failed ();
	tracker_turtle_close (file);
	clean_test_file ();


	/* Now with a proper property */
	file = tracker_turtle_open (TEST_FILE);
	g_assert (file != NULL);
	if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR)) {
		tracker_turtle_add_triple (file, NULL, prop, NULL);
	}
	g_test_trap_assert_failed ();
	tracker_turtle_close (file);
	clean_test_file ();
	
	/* Now 10 real insertions */
	file = tracker_turtle_open (TEST_FILE);
	g_assert (file != NULL);
	for (i = 0; i < 10; i++) {
		gchar *uri = g_strdup_printf (uri_pattern, i);
		tracker_turtle_add_triple (file, uri, prop, value);
		g_free (uri);
	}
	tracker_turtle_close (file);

	g_assert_cmpint (count_stmt_in_file (TEST_FILE), ==, 10);
	clean_test_file ();

	tracker_turtle_shutdown ();
}

static void
test_add_metadata (void)
{
	TrackerDataMetadata *metadata = NULL;
	TurtleFile          *file;
	const gchar         *mock_uri = "file:///1/2/3.mp3";

	/* No initialization */
	if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR)) {
		tracker_turtle_add_metadata (NULL, mock_uri, metadata);
	}
	g_test_trap_assert_failed ();

	/* Init but NULL */
	tracker_turtle_init ();
	if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR)) {
		tracker_turtle_add_metadata (NULL, mock_uri, metadata);
	}
	g_test_trap_assert_failed ();

	clean_test_file ();

	/* NULL metadata */
	file = tracker_turtle_open (TEST_FILE);
	g_assert (file != NULL);
	if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR)) {
		tracker_turtle_add_metadata (file, mock_uri, NULL);
	}
	g_test_trap_assert_failed ();
	tracker_turtle_close (file);
	g_assert_cmpint (count_stmt_in_file (TEST_FILE), ==, 0);
	clean_test_file ();

	metadata = tracker_data_metadata_new ();

	/* No metadata = No lines */
	file = tracker_turtle_open (TEST_FILE);
	g_assert (file != NULL);
	tracker_turtle_add_metadata (file, mock_uri, metadata);
	tracker_turtle_close (file);
	g_assert_cmpint (count_stmt_in_file (TEST_FILE), ==, 0);
	clean_test_file ();

	/* Some metadata */
	tracker_data_metadata_insert (metadata, "test:cool", "true");
	tracker_data_metadata_insert (metadata, "test:playcount", "10");
	file = tracker_turtle_open (TEST_FILE);
	g_assert (file != NULL);
	tracker_turtle_add_metadata (file, mock_uri, metadata);
	tracker_turtle_close (file);
	g_assert_cmpint (count_stmt_in_file (TEST_FILE), ==, 2);
	clean_test_file ();

	tracker_turtle_shutdown ();
}

static void
test_add_metadatas (void)
{
	TurtleFile                *file = NULL;
	TrackerTurtleMetadataItem *item = NULL;
	GPtrArray                 *array;
	const gchar               *mock_uri = "file:///1/2/3.mp3";

	/* No initialization */
	if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR)) {
		tracker_turtle_add_metadatas (NULL, NULL);
	}
	g_test_trap_assert_failed ();

	/* Init but NULL */
	tracker_turtle_init ();
	if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR)) {
		tracker_turtle_add_metadatas (NULL, NULL);
	}
	g_test_trap_assert_failed ();
	
	clean_test_file ();

	/* NULL metadata */
	file = tracker_turtle_open (TEST_FILE);
	g_assert (file != NULL);
	if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR)) {
		tracker_turtle_add_metadatas (file, NULL);
	}
	g_test_trap_assert_failed ();
	tracker_turtle_close (file);
	g_assert_cmpint (count_stmt_in_file (TEST_FILE), ==, 0);
	clean_test_file ();
 
	/* Emtpy metadata = No lines */
	file = tracker_turtle_open (TEST_FILE);
	item = g_new0 (TrackerTurtleMetadataItem, 1);
	item->about_uri = g_strdup (mock_uri);
	item->metadata = tracker_data_metadata_new ();
	item->turtle = file; /* Internal Use! */
	g_assert (file != NULL);

	array = g_ptr_array_new ();
	g_ptr_array_add (array, item);

	tracker_turtle_add_metadatas (file, array);
	tracker_turtle_close (file);
	g_assert_cmpint (count_stmt_in_file (TEST_FILE), ==, 0);
	clean_test_file ();
	
	/* Some metadata */
	tracker_data_metadata_insert (item->metadata, "test:cool", "true");
	tracker_data_metadata_insert (item->metadata, "test:playcount", "10");

	file = tracker_turtle_open (TEST_FILE);
	g_assert (file != NULL);
	item->turtle = file;

	tracker_turtle_add_metadatas (file, array);
	tracker_turtle_close (file);
	g_assert_cmpint (count_stmt_in_file (TEST_FILE), ==, 2);
	clean_test_file ();
	
	g_free (item->about_uri);
	g_object_unref (item->metadata);

	tracker_turtle_shutdown ();
}

static void
test_optimize (void)
{
	GFile *input_data = NULL;
	GFile *optimizeme = NULL;

	/* No initialization */
	if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR)) {
		tracker_turtle_optimize (NULL);
	}
	g_test_trap_assert_failed ();

	/* Init and NULL */
	tracker_turtle_init ();
	if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR)) {
		tracker_turtle_optimize (NULL);
	}
	g_test_trap_assert_failed ();

	/* No target file */
	tracker_turtle_optimize ("/very/unlikely/to/have/this/file.tll");

	/* Optimize overwrite the file. Copy to a different location */
	input_data = g_file_new_for_path ("./data/unoptimized.ttl");
	optimizeme = g_file_new_for_path ("./optimized.ttl");
	g_file_copy (input_data, optimizeme, G_FILE_COPY_OVERWRITE, 
		     NULL, NULL, NULL, NULL);
	tracker_turtle_optimize (g_file_get_path (optimizeme));
	
	/* 12 statements  */
	//g_assert_cmpint (count_stmt_in_file (g_file_get_path (optimizeme)), ==, 12);
	g_file_delete (optimizeme, NULL, NULL);
	g_object_unref (input_data);
	g_object_unref (optimizeme);

	tracker_turtle_shutdown ();
}

gint
main (gint argc, gchar **argv) 
{
	gint result;

	g_type_init ();
	g_test_init (&argc, &argv, NULL);

	init_ontology ();

	#ifdef HAVE_RAPTOR
	g_test_add_func ("/libtracker-data/tracker-turtle/initialization",
			 test_initialization);

	g_test_add_func ("/libtracker-data/tracker-turtle/open_close",
			 test_open_close);

	g_test_add_func ("/libtracker-data/tracker-turtle/process",
			 test_process);

	g_test_add_func ("/libtracker-data/tracker-turtle/transaction",
			 test_transaction);

	g_test_add_func ("/libtracker-data/tracker-turtle/add_metadata",
			 test_add_metadata);

	g_test_add_func ("/libtracker_data/tracker-turtle/add_metadatas",
			 test_add_metadatas);

	g_test_add_func ("/libtracker_data/tracker-turtle/optimize",
			 test_optimize);
	#endif

	result = g_test_run ();

	shutdown_ontology ();

	return result;
}
