/*
 * Copyright (C) 2011, Nokia <ivan.frade@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <glib.h>

#include <libtracker-extract/tracker-extract.h>

static void
test_extract_info_setters (void)
{
        TrackerExtractInfo *info, *info_ref;
        GFile *file;

        file = g_file_new_for_path ("./imaginary-file-2");

        info = tracker_extract_info_new (file, "imaginary/mime", "test-graph");
        info_ref = tracker_extract_info_ref (info);

        g_assert (g_file_equal (file, tracker_extract_info_get_file (info)));

        g_assert_cmpstr (tracker_extract_info_get_mimetype (info), ==, "imaginary/mime");
        g_assert_cmpstr (tracker_extract_info_get_graph (info), ==, "test-graph");
        g_assert (tracker_extract_info_get_preupdate_builder (info));
        g_assert (tracker_extract_info_get_postupdate_builder (info));
        g_assert (tracker_extract_info_get_metadata_builder (info));

        g_assert (!tracker_extract_info_get_where_clause (info));
        tracker_extract_info_set_where_clause (info, "where stuff");
        g_assert_cmpstr (tracker_extract_info_get_where_clause (info), ==, "where stuff");

        tracker_extract_info_unref (info_ref);
        tracker_extract_info_unref (info);

        g_object_unref (file);
}

static void
test_extract_info_empty_objects (void)
{
        TrackerExtractInfo *info, *info_ref;
        GFile *file;

        file = g_file_new_for_path ("./imaginary-file");

        info = tracker_extract_info_new (file, "imaginary/mime", "test-graph");
        info_ref = tracker_extract_info_ref (info);

        tracker_extract_info_unref (info_ref);
        tracker_extract_info_unref (info);

        g_object_unref (file);
}

int
main (int argc, char **argv)
{
        g_test_init (&argc, &argv, NULL);

        g_test_add_func ("/libtracker-extract/extract-info/empty_objects",
                         test_extract_info_empty_objects);
        g_test_add_func ("/libtracker-extract/extract-info/setters",
                         test_extract_info_setters);

        return g_test_run ();
}
