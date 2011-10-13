#include <glib.h>
#include <libtracker-extract/tracker-extract-info.h>

void
test_extract_info_setters ()
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

void
test_extract_info_empty_objects ()
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
        g_type_init ();
        g_test_init (&argc, &argv, NULL);

        g_test_add_func ("/libtracker-extract/extract-info/empty_objects",
                         test_extract_info_empty_objects);
        g_test_add_func ("/libtracker-extract/extract-info/setters",
                         test_extract_info_setters);

        return g_test_run ();
}
