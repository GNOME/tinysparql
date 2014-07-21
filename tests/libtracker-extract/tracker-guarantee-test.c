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
#include "config.h"
#include <glib.h>
#include <libtracker-sparql/tracker-sparql.h>
#include <libtracker-extract/tracker-guarantee.h>

typedef struct {
        gchar *file_uri;
        gchar *extracted_title;
        gchar *expected_title;
} TestCase;

TestCase test_cases_title [] = {
        { "file:///a/b/a_video_with_metadata.avi", "extracted title", "extracted title" },

        { "file:///a/b/a_video_with_no_metadata.avi", NULL, "a video with no metadata" },
        { "file:///a/b/a_video_with_no_metadata.avi", "", "a video with no metadata" },
        { "file:///a/b/a.video.with.no.metadata.avi", NULL, "a.video.with.no.metadata" },
        { "file:///a/b/a video without extension", NULL, "a video without extension" },
        { "file:///a/b/.hidden_file", NULL, "hidden file" },

        { NULL, NULL, NULL}
};

/*
 * @uri of the file that is being processed
 * @value is the title returned by the extractor
 * @expected can be either the title of the extractor (if not NULL or empty) or calculated from the filename
 */
static void
internal_test_title (const gchar *uri,
                     const gchar *value,
                     const gchar *expected)
{
        TrackerSparqlBuilder *builder;
        gchar                *sparql;
        gchar                *title_guaranteed;

        builder = tracker_sparql_builder_new_update ();
        tracker_sparql_builder_insert_open (builder, "test");
        tracker_sparql_builder_subject_iri (builder, "test://resource");
        g_assert (tracker_guarantee_title_from_file (builder, 
                                                     "nie:title",
                                                     value,
                                                     uri,
                                                     &title_guaranteed));
        tracker_sparql_builder_insert_close (builder);

        sparql = g_strdup_printf ("INSERT INTO <test> {\n<test://resource> nie:title \"%s\" .\n}\n",
                                  expected);
        g_assert_cmpstr (sparql,
                         ==,
                         tracker_sparql_builder_get_result (builder));

        g_assert_cmpstr (title_guaranteed,
                         ==,
                         expected);

        g_object_unref (builder);
        g_free (sparql);
}

static void
internal_test_date (const gchar *uri,
                    const gchar *value)
{
        TrackerSparqlBuilder *builder;

        builder = tracker_sparql_builder_new_update ();
        tracker_sparql_builder_insert_open (builder, "test");
        tracker_sparql_builder_subject_iri (builder, "test://resource");
        g_assert (tracker_guarantee_date_from_file_mtime (builder, 
                                                          "test:mtime",
                                                          value,
                                                          uri));
        tracker_sparql_builder_insert_close (builder);
        /* mtime can change in the file so we just check that the property is in the output */
        g_assert  (g_strstr_len (tracker_sparql_builder_get_result (builder), -1, "test:mtime"));

        g_object_unref (builder);
}

static void
test_guarantee_title (void)
{
        int i;

        for (i = 0; test_cases_title[i].file_uri != NULL; i++) {
                internal_test_title (test_cases_title[i].file_uri,
                                     test_cases_title[i].extracted_title,
                                     test_cases_title[i].expected_title);
        }

        g_print ("%d test cases (guarantee metadata enabled) ", i);
}

static void
test_guarantee_date (void)
{
        GFile *f;
        gchar *uri;

        internal_test_date ("file:///does/not/matter/here", "2011-10-10T12:13:14Z0300");

        f = g_file_new_for_path (TOP_SRCDIR "/tests/libtracker-extract/guarantee-mtime-test.txt");
        uri = g_file_get_uri (f);

        internal_test_date (uri, NULL);
        internal_test_date (uri, "");

        g_free (uri);
        g_object_unref (f);
}


int
main (int argc, char** argv)
{
        g_test_init (&argc, &argv, NULL);

        g_test_add_func ("/libtracker-extract/guarantee/title",
                         test_guarantee_title);
        g_test_add_func ("/libtracker-extract/guarantee/date",
                         test_guarantee_date);

        return g_test_run ();
}
