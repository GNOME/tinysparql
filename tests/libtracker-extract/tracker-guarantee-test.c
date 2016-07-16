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

#include <locale.h>

#include <glib.h>

#include <libtracker-sparql/tracker-sparql.h>
#include <libtracker-extract/tracker-guarantee.h>

typedef struct {
	const gchar *test_name;
	gchar *uri;
	const gchar *extracted;
	const gchar *expected;
	TrackerResource *resource;
} TestInfo;

TestInfo title_tests [] = {
	{ "normal-extraction", "file:///a/b/a_video_with_metadata.avi", "extracted title", "extracted title" },
	{ "empty-extraction", "file:///a/b/a_video_with_no_metadata.avi", NULL, "a video with no metadata" },
	{ "underscore-separators", "file:///a/b/a_video_with_no_metadata.avi", "", "a video with no metadata" },
	{ "dot-separators", "file:///a/b/a.video.with.no.metadata.avi", NULL, "a.video.with.no.metadata" },
	{ "no-extension", "file:///a/b/a video without extension", NULL, "a video without extension" },
	{ "hidden-files", "file:///a/b/.hidden_file", NULL, "hidden file" },

	{ NULL, NULL, NULL }
};

TestInfo date_tests [] = {
	{ "date-normal", "file:///does/not/matter/here", NULL, "2011-10-10T12:13:14Z0300" },
	{ "date-is-null", NULL, NULL, NULL },
	{ "date-is-empty-string", NULL, NULL, NULL },
	{ NULL, NULL, NULL }
};

static void
test_title (TestInfo      *info,
            gconstpointer  context)
{
#ifdef GUARANTEE_METADATA
	gchar *title_guaranteed;
	gboolean title_retrieved;

	title_retrieved = tracker_guarantee_resource_title_from_file (info->resource, "nie:title", info->extracted, info->uri, &title_guaranteed);
	g_assert_true (title_retrieved);

	g_assert_cmpstr (tracker_resource_get_first_string (info->resource, "nie:title"), ==, info->expected);
	g_assert_cmpstr (title_guaranteed, ==, info->expected);

	g_free (title_guaranteed);
#else  /* GUARANTEE_METADATA */
	g_test_skip ("Not built with --enable-guarantee-metadata");
#endif /* GUARANTEE_METADATA */
}

static void
test_date (TestInfo      *info,
           gconstpointer  context)
{
#ifdef GUARANTEE_METADATA
	gboolean date_retrieved;

	date_retrieved = tracker_guarantee_resource_date_from_file_mtime (info->resource, "test:mtime", info->extracted, info->uri);
	g_assert_true (date_retrieved);

	/* mtime can change in the file so we just check that the property is in the output */
	g_assert_nonnull (tracker_resource_get_first_string (info->resource, "test:mtime"));
#else  /* GUARANTEE_METADATA */
	g_test_skip ("Not built with --enable-guarantee-metadata");
#endif /* GUARANTEE_METADATA */
}

static void
setup (TestInfo *info,
       gint      i)
{
	info->resource = tracker_resource_new (NULL);
	g_assert_nonnull (info->resource);

	if (strstr (info->test_name, "date")) {
		GFile *f;

		f = g_file_new_for_path (TOP_SRCDIR "/tests/libtracker-extract/guarantee-mtime-test.txt");
		info->uri = g_file_get_uri (f);
		g_object_unref (f);
	}
}

static void
setup_title (TestInfo      *info,
             gconstpointer  context)
{
	gint i = GPOINTER_TO_INT (context);

	*info = title_tests[i];
	setup (info, i);
}

static void
setup_date (TestInfo      *info,
            gconstpointer  context)
{
	gint i = GPOINTER_TO_INT (context);

	*info = date_tests[i];
	setup (info, i);
}

static void
teardown (TestInfo      *info,
          gconstpointer  context)
{
	if (strstr (info->test_name, "date")) {
		g_free (info->uri);
	}

	g_object_unref (info->resource);
}

int
main (int argc, char** argv)
{
	gint i;

	setlocale (LC_COLLATE, "en_US.utf8");

	g_test_init (&argc, &argv, NULL);

	for (i = 0; title_tests[i].test_name != NULL; i++) {
		gchar *testpath;

		testpath = g_strdup_printf ("/libtracker-extract/guarantee/title/%s", title_tests[i].test_name);
		g_test_add (testpath, TestInfo, GINT_TO_POINTER(i), setup_title, test_title, teardown);
		g_free (testpath);
	}

	for (i = 0; date_tests[i].test_name != NULL; i++) {
		gchar *testpath;

		testpath = g_strdup_printf ("/libtracker-extract/guarantee/date/%s", date_tests[i].test_name);
		g_test_add (testpath, TestInfo, GINT_TO_POINTER(i), setup_date, test_date, teardown);
		g_free (testpath);
	}

	return g_test_run ();
}
