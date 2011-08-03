/*
 * Copyright (C) 2011, Nokia <ivan.frade@nokia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <string.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <libtracker-miner/tracker-indexing-tree.h>

/*
 * Test directory structure:
 *  - Directory A
 *     -- Directory AA
 *         --- Directory AAA
 *              ---- Directory AAAA
 *              ---- Directory AAAB
 *         --- Directory AAB
 *     -- Directory AB
 *         --- Directory ABA
 *         --- Directory ABB
 */
typedef enum {
	TEST_DIRECTORY_A = 0,
	TEST_DIRECTORY_AA,
	TEST_DIRECTORY_AAA,
	TEST_DIRECTORY_AAAA,
	TEST_DIRECTORY_AAAB,
	TEST_DIRECTORY_AAB,
	TEST_DIRECTORY_AB,
	TEST_DIRECTORY_ABA,
	TEST_DIRECTORY_ABB,
	TEST_DIRECTORY_LAST
} TestDirectory;

/* Fixture struct */
typedef struct {
	/* Array with all existing test directories */
	GFile *test_dir[TEST_DIRECTORY_LAST];
	/* The tree to test */
	TrackerIndexingTree *tree;
} TestCommonContext;

#define ASSERT_INDEXABLE(fixture, id)	  \
	g_assert (tracker_indexing_tree_file_is_indexable (fixture->tree, \
	                                                   fixture->test_dir[id], \
	                                                   G_FILE_TYPE_DIRECTORY) == TRUE)
#define ASSERT_NOT_INDEXABLE(fixture, id)	  \
	g_assert (tracker_indexing_tree_file_is_indexable (fixture->tree, \
	                                                   fixture->test_dir[id], \
	                                                   G_FILE_TYPE_DIRECTORY) == FALSE)

#define test_add(path,fun)	  \
	g_test_add (path, \
	            TestCommonContext, \
	            NULL, \
	            test_common_context_setup, \
	            fun, \
	            test_common_context_teardown)

void
test_common_context_setup (TestCommonContext *fixture,
                           gconstpointer      data)
{
	guint i;
	static const gchar *test_directories_subpaths [TEST_DIRECTORY_LAST] = {
		"/A",
		"/A/A",
		"/A/A/A",
		"/A/A/A/A",
		"/A/A/A/B",
		"/A/A/B",
		"/A/B/",
		"/A/B/A",
		"/A/B/B"
	};

	/* Initialize aux directories */
	for (i = 0; i < TEST_DIRECTORY_LAST; i++)
		fixture->test_dir[i] = g_file_new_for_path (test_directories_subpaths[i]);

	fixture->tree = tracker_indexing_tree_new ();
}

void
test_common_context_teardown (TestCommonContext *fixture,
                              gconstpointer      data)
{
	gint i;

	/* Deinit aux directories, from last to first */
	for (i = TEST_DIRECTORY_LAST-1; i >= 0; i--) {
		if (fixture->test_dir[i])
			g_object_unref (fixture->test_dir[i]);
	}

	if (fixture->tree)
		g_object_unref (fixture->tree);
}

/* If A is ignored,
 *  -A, AA, AB, AAA, AAB, ABA and ABB are not indexable
 */
static void
test_indexing_tree_001 (TestCommonContext *fixture,
                        gconstpointer      data)
{
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_A],
	                           TRACKER_DIRECTORY_FLAG_IGNORE);

	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_A);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_AA);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_AAA);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_AAAA);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_AAAB);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_AAB);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_AB);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_ABA);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_ABA);
}

/* If A is monitored (NOT recursively):
 *  -A, AA, AB are indexable
 *  -AAA, AAB, ABA and ABB are not indexable
 */
static void
test_indexing_tree_002 (TestCommonContext *fixture,
                        gconstpointer      data)
{
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_A],
	                           TRACKER_DIRECTORY_FLAG_MONITOR);

	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_A);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AA);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_AAA);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_AAAA);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_AAAB);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_AAB);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AB);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_ABA);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_ABA);
}

gint
main (gint    argc,
      gchar **argv)
{
	g_thread_init (NULL);
	g_type_init ();
	g_test_init (&argc, &argv, NULL);

	g_test_message ("Testing indexing tree");

	test_add ("/libtracker-miner/indexing-tree/001", test_indexing_tree_001);
	test_add ("/libtracker-miner/indexing-tree/002", test_indexing_tree_002);

	return g_test_run ();
}
