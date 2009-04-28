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
#include <libtracker-data/tracker-turtle.h>
#include <libtracker-common/tracker-common.h>

#define PROP_TAG "test:tag"
#define PROP_PLAYCOUNT "test:playcount"

static void
init_ontology ()
{
	TrackerField *field, *field2;

	field = tracker_field_new ();
	tracker_field_set_id (field, "1");
	tracker_field_set_name (field, PROP_TAG);
        tracker_field_set_multiple_values (field, TRUE);
	tracker_field_set_data_type (field, TRACKER_FIELD_TYPE_STRING);

	field2 = tracker_field_new ();
	tracker_field_set_id (field2, "2");
	tracker_field_set_name (field2, PROP_PLAYCOUNT);
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
test_new_free ()
{
        TrackerDataMetadata *metadata = NULL;

        metadata = tracker_data_metadata_new ();
        g_assert (metadata != NULL);
        tracker_data_metadata_free (metadata);
        
        if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR)) {
                tracker_data_metadata_free (NULL);
        }
        g_test_trap_assert_failed ();
}

static void
test_single_valued ()
{
        TrackerDataMetadata *metadata = NULL;
        const gchar *result = NULL;

        metadata = tracker_data_metadata_new ();
        g_assert (metadata != NULL);

        result = tracker_data_metadata_lookup (metadata, 
                                               PROP_PLAYCOUNT);

        g_assert (!result);
        
        tracker_data_metadata_insert (metadata, PROP_PLAYCOUNT, "12");

        result = tracker_data_metadata_lookup (metadata, PROP_PLAYCOUNT);
        g_assert (result);
        g_assert (g_strcmp0 (result, "12") == 0);

        /* Overwrite the value */
        tracker_data_metadata_insert (metadata, PROP_PLAYCOUNT, "13");

        result = tracker_data_metadata_lookup (metadata, PROP_PLAYCOUNT);
        g_assert (result);
        g_assert (g_strcmp0 (result, "13") == 0);

        tracker_data_metadata_free (metadata);
}

static void
test_multiple_valued ()
{
        TrackerDataMetadata *metadata = NULL;
        const GList *result = NULL;

        metadata = tracker_data_metadata_new ();
        g_assert (metadata != NULL);

        result = tracker_data_metadata_lookup_values (metadata, 
                                                      PROP_TAG);
        g_assert (!result);

        GList *tags = NULL;
        tags = g_list_prepend (tags, "tag 1");
        tags = g_list_prepend (tags, "tag 2");

        tracker_data_metadata_insert_values (metadata, PROP_TAG, tags);

        /* We are responsible of the original list! 
         *  (Dont free the contents, are static strings)
         */
        g_list_free (tags);
        tags = NULL;

        result = tracker_data_metadata_lookup_values (metadata, PROP_TAG);
        g_assert_cmpint (g_list_length ((GList*)result), ==, 2);
        g_assert (g_list_find_custom ((GList*)result, "tag 1", g_str_equal));
        g_assert (g_list_find_custom ((GList*)result, "tag 2", g_str_equal));

        tags = g_list_prepend (tags, "tag X");
        tracker_data_metadata_insert_values (metadata, PROP_TAG, tags);
        g_assert_cmpint (g_list_length ((GList*)result), ==, 1);
        g_assert (g_list_find_custom ((GList*)result, "tag X", g_str_equal));

        g_list_free (tags);
        tags = NULL;

        tracker_data_metadata_free (metadata);
}

static void
test_invalid_keys ()
{
        TrackerDataMetadata *metadata = NULL;
        const gchar *result = NULL;
        const GList *result_list = NULL;

        metadata = tracker_data_metadata_new ();
        if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR)) {
                result = tracker_data_metadata_lookup (metadata, "test:no_existing_key");
        }
        g_test_trap_assert_stderr ("*TRACKER_IS_FIELD (field)' failed*");


        if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR)) {
                result_list = tracker_data_metadata_lookup_values (metadata, 
                                                                   "test:no_existing_key");
        }
        g_test_trap_assert_stderr ("*TRACKER_IS_FIELD (field)' failed*");
}

gint
main (gint argc, gchar **argv) 
{
	gint result;

	g_type_init ();
	g_test_init (&argc, &argv, NULL);

	init_ontology ();

        g_test_add_func ("/libtracker-data/tracker-data-metadata/new_free",
                         test_new_free);

        g_test_add_func ("/libtracker-data/tracker-data-metadata/single_valued",
                         test_single_valued);

        g_test_add_func ("/libtracker-data/tracker-data-metadata/multiple_valued",
                         test_multiple_valued);

        g_test_add_func ("/libtracker-data/tracker-data-metadata/invalid_keys",
                         test_invalid_keys);

	result = g_test_run ();

	shutdown_ontology ();

	return result;
}
