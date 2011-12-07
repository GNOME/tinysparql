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

static void
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

static void
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

/* If A is monitored (recursively):
 *  -A, AA, AB, AAA, AAB, ABA and ABB are indexable
 */
static void
test_indexing_tree_003 (TestCommonContext *fixture,
                        gconstpointer      data)
{
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_A],
	                           TRACKER_DIRECTORY_FLAG_MONITOR | TRACKER_DIRECTORY_FLAG_RECURSE);

	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_A);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AA);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAA);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAAA);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAAB);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAB);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AB);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_ABA);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_ABA);
}

/* If A is ignored and AA is ignored:
 *  -A, AA, AB, AAA, AAB, ABA and ABB are not indexable
 */
static void
test_indexing_tree_004 (TestCommonContext *fixture,
                        gconstpointer      data)
{
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_A],
	                           TRACKER_DIRECTORY_FLAG_IGNORE);
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_AA],
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

/* If A is ignored and AA is monitored (not recursively):
 *  -AA, AAA, AAB are indexable
 *  -A, AAAA, AAAB, AB, ABA, ABB are not indexable
 */
static void
test_indexing_tree_005 (TestCommonContext *fixture,
                        gconstpointer      data)
{
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_A],
	                           TRACKER_DIRECTORY_FLAG_IGNORE);
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_AA],
	                           TRACKER_DIRECTORY_FLAG_MONITOR);

	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_A);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AA);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAA);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_AAAA);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_AAAB);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAB);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_AB);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_ABA);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_ABA);
}

/* If A is ignored and AA is monitored (recursively):
 *  -AA, AAA, AAAA, AAAB, AAB are indexable
 *  -A, AB, ABA, ABB are not indexable
 */
static void
test_indexing_tree_006 (TestCommonContext *fixture,
                        gconstpointer      data)
{
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_A],
	                           TRACKER_DIRECTORY_FLAG_IGNORE);
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_AA],
	                           TRACKER_DIRECTORY_FLAG_MONITOR | TRACKER_DIRECTORY_FLAG_RECURSE);

	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_A);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AA);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAA);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAAA);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAAB);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAB);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_AB);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_ABA);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_ABA);
}

/* If A is monitored (not recursively) and AA is ignored:
 *  -A and AB are indexable
 *  -AA, AAA, AAAA, AAAB, AAB, ABA, ABB are not indexable
 */
static void
test_indexing_tree_007 (TestCommonContext *fixture,
                        gconstpointer      data)
{
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_A],
	                           TRACKER_DIRECTORY_FLAG_MONITOR);
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_AA],
	                           TRACKER_DIRECTORY_FLAG_IGNORE);

	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_A);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_AA);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_AAA);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_AAAA);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_AAAB);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_AAB);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AB);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_ABA);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_ABA);
}

/* If A is monitored (not recursively) and AA is monitored (not recursively):
 *  -A, AA, AAA, AAB, AB are indexable
 *  -AAAA, AAAB, ABA, ABB are not indexable
 */
static void
test_indexing_tree_008 (TestCommonContext *fixture,
                        gconstpointer      data)
{
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_A],
	                           TRACKER_DIRECTORY_FLAG_MONITOR);
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_AA],
	                           TRACKER_DIRECTORY_FLAG_MONITOR);

	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_A);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AA);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAA);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_AAAA);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_AAAB);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAB);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AB);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_ABA);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_ABA);
}

/* If A is monitored (not recursively) and AA is monitored (recursively):
 *  -A, AA, AAA, AAAA, AAAB, AAB, AB are indexable
 *  -ABA, ABB are not indexable
 */
static void
test_indexing_tree_009 (TestCommonContext *fixture,
                        gconstpointer      data)
{
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_A],
	                           TRACKER_DIRECTORY_FLAG_MONITOR);
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_AA],
	                           TRACKER_DIRECTORY_FLAG_MONITOR | TRACKER_DIRECTORY_FLAG_RECURSE);

	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_A);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AA);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAA);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAAA);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAAB);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAB);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AB);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_ABA);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_ABA);
}

/* If A is monitored (recursively) and AA is ignored:
 *  -A, AB, ABA, ABB are indexable
 *  -AA, AAA, AAAA, AAAB, AAB are not indexable
 */
static void
test_indexing_tree_010 (TestCommonContext *fixture,
                        gconstpointer      data)
{
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_A],
	                           TRACKER_DIRECTORY_FLAG_MONITOR | TRACKER_DIRECTORY_FLAG_RECURSE);
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_AA],
	                           TRACKER_DIRECTORY_FLAG_IGNORE);

	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_A);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_AA);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_AAA);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_AAAA);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_AAAB);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_AAB);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AB);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_ABA);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_ABA);
}

/* If A is monitored (recursively) and AA is monitored (not recursively):
 *  -A, AA, AAA, AAB, AB, ABA, ABB are indexable
 *  -AAAA, AAAB are not indexable
 */
static void
test_indexing_tree_011 (TestCommonContext *fixture,
                        gconstpointer      data)
{
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_A],
	                           TRACKER_DIRECTORY_FLAG_MONITOR | TRACKER_DIRECTORY_FLAG_RECURSE);
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_AA],
	                           TRACKER_DIRECTORY_FLAG_MONITOR);

	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_A);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AA);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAA);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_AAAA);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_AAAB);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAB);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AB);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_ABA);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_ABA);
}

/* If A is monitored (recursively) and AA is monitored (recursively):
 *  -A, AA, AAA, AAAA, AAAB, AAB, AB, ABA, ABB are indexable
 */
static void
test_indexing_tree_012 (TestCommonContext *fixture,
                        gconstpointer      data)
{
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_A],
	                           TRACKER_DIRECTORY_FLAG_MONITOR | TRACKER_DIRECTORY_FLAG_RECURSE);
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_AA],
	                           TRACKER_DIRECTORY_FLAG_MONITOR | TRACKER_DIRECTORY_FLAG_RECURSE);

	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_A);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AA);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAA);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAAA);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAAB);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAB);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AB);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_ABA);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_ABA);
}

/* If A is ignored and AA is ignored, then A is removed from tree
 *  -A, AA, AAA, AAAA, AAAB, AAB, AB, ABA and ABB are not indexable
 */
static void
test_indexing_tree_013 (TestCommonContext *fixture,
                        gconstpointer      data)
{
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_A],
	                           TRACKER_DIRECTORY_FLAG_IGNORE);
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_AA],
	                           TRACKER_DIRECTORY_FLAG_IGNORE);

	tracker_indexing_tree_remove (fixture->tree,
	                              fixture->test_dir[TEST_DIRECTORY_A]);

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

/* If A is ignored and AA is ignored, then AA is removed from tree
 *  -A, AA, AAA, AAAA, AAAB, AAB, AB, ABA and ABB are not indexable
 */
static void
test_indexing_tree_014 (TestCommonContext *fixture,
                        gconstpointer      data)
{
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_A],
	                           TRACKER_DIRECTORY_FLAG_IGNORE);
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_AA],
	                           TRACKER_DIRECTORY_FLAG_IGNORE);

	tracker_indexing_tree_remove (fixture->tree,
	                              fixture->test_dir[TEST_DIRECTORY_AA]);

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

/* If A is ignored and AA is monitored (not recursively), then A is removed
 * from tree.
 *  -AA, AAA, AAB are indexable
 *  -A, AAAA, AAAB, AB, ABA and ABB are not indexable
 */
static void
test_indexing_tree_015 (TestCommonContext *fixture,
                        gconstpointer      data)
{
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_A],
	                           TRACKER_DIRECTORY_FLAG_IGNORE);
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_AA],
	                           TRACKER_DIRECTORY_FLAG_MONITOR);

	tracker_indexing_tree_remove (fixture->tree,
	                              fixture->test_dir[TEST_DIRECTORY_A]);

	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_A);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AA);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAA);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_AAAA);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_AAAB);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAB);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_AB);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_ABA);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_ABA);
}

/* If A is ignored and AA is monitored (not recursively), then AA is removed
 * from tree.
 *  -A, AA, AAA, AAAA, AAAB, AAB, AB, ABA and ABB are not indexable
 */
static void
test_indexing_tree_016 (TestCommonContext *fixture,
                        gconstpointer      data)
{
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_A],
	                           TRACKER_DIRECTORY_FLAG_IGNORE);
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_AA],
	                           TRACKER_DIRECTORY_FLAG_MONITOR);

	tracker_indexing_tree_remove (fixture->tree,
	                              fixture->test_dir[TEST_DIRECTORY_AA]);

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

/* If A is ignored and AA is monitored (recursively), then A is removed from
 * tree.
 *  -AA, AAA, AAAA, AAAB, AAB are indexable
 *  -A, AB, ABA and ABB are not indexable
 */
static void
test_indexing_tree_017 (TestCommonContext *fixture,
                        gconstpointer      data)
{
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_A],
	                           TRACKER_DIRECTORY_FLAG_IGNORE);
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_AA],
	                           TRACKER_DIRECTORY_FLAG_MONITOR | TRACKER_DIRECTORY_FLAG_RECURSE);

	tracker_indexing_tree_remove (fixture->tree,
	                              fixture->test_dir[TEST_DIRECTORY_A]);

	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_A);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AA);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAA);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAAA);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAAB);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAB);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_AB);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_ABA);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_ABA);
}

/* If A is ignored and AA is monitored (recursively), then AA is removed
 * from tree.
 *  -A, AA, AAA, AAAA, AAAB, AAB, AB, ABA and ABB are not indexable
 */
static void
test_indexing_tree_018 (TestCommonContext *fixture,
                        gconstpointer      data)
{
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_A],
	                           TRACKER_DIRECTORY_FLAG_IGNORE);
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_AA],
	                           TRACKER_DIRECTORY_FLAG_MONITOR | TRACKER_DIRECTORY_FLAG_RECURSE);

	tracker_indexing_tree_remove (fixture->tree,
	                              fixture->test_dir[TEST_DIRECTORY_AA]);

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

/* If A is monitored (not recursively) and AA is ignored, then A is removed
 * from tree.
 *  -A, AA, AAA, AAAA, AAAB, AAB, AB, ABA and ABB are not indexable
 */
static void
test_indexing_tree_019 (TestCommonContext *fixture,
                        gconstpointer      data)
{
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_A],
	                           TRACKER_DIRECTORY_FLAG_MONITOR);
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_AA],
	                           TRACKER_DIRECTORY_FLAG_IGNORE);

	tracker_indexing_tree_remove (fixture->tree,
	                              fixture->test_dir[TEST_DIRECTORY_A]);

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

/* If A is monitored (not recursively) and AA is ignored, then AA is removed
 * from tree.
 *  -A, AA, AB are indexable.
 *  -AAA, AAAA, AAAB, AAB, ABA and ABB are not indexable
 */
static void
test_indexing_tree_020 (TestCommonContext *fixture,
                        gconstpointer      data)
{
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_A],
	                           TRACKER_DIRECTORY_FLAG_MONITOR);
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_AA],
	                           TRACKER_DIRECTORY_FLAG_IGNORE);

	tracker_indexing_tree_remove (fixture->tree,
	                              fixture->test_dir[TEST_DIRECTORY_AA]);

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

/* If A is monitored (not recursively) and AA is monitored (not recursively),
 *  then A is removed from tree.
 *  -AA, AAA, AAB are indexable.
 *  -A, AAAA, AAAB, AB, ABA and ABB are not indexable
 */
static void
test_indexing_tree_021 (TestCommonContext *fixture,
                        gconstpointer      data)
{
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_A],
	                           TRACKER_DIRECTORY_FLAG_MONITOR);
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_AA],
	                           TRACKER_DIRECTORY_FLAG_MONITOR);

	tracker_indexing_tree_remove (fixture->tree,
	                              fixture->test_dir[TEST_DIRECTORY_A]);

	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_A);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AA);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAA);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_AAAA);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_AAAB);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAB);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_AB);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_ABA);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_ABA);
}

/* If A is monitored (not recursively) and AA is monitored (not recursively),
 *  then AA is removed from tree.
 *  -A, AA, AB are indexable.
 *  -AAA, AAAA, AAAB, AAB, ABA and ABB are not indexable
 */
static void
test_indexing_tree_022 (TestCommonContext *fixture,
                        gconstpointer      data)
{
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_A],
	                           TRACKER_DIRECTORY_FLAG_MONITOR);
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_AA],
	                           TRACKER_DIRECTORY_FLAG_MONITOR);

	tracker_indexing_tree_remove (fixture->tree,
	                              fixture->test_dir[TEST_DIRECTORY_AA]);

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

/* If A is monitored (not recursively) and AA is monitored (recursively),
 *  then A is removed from tree.
 *  -AA, AAA, AAAA, AAAB, AAB are indexable.
 *  -A, AB, ABA and ABB are not indexable
 */
static void
test_indexing_tree_023 (TestCommonContext *fixture,
                        gconstpointer      data)
{
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_A],
	                           TRACKER_DIRECTORY_FLAG_MONITOR);
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_AA],
	                           TRACKER_DIRECTORY_FLAG_MONITOR | TRACKER_DIRECTORY_FLAG_RECURSE);

	tracker_indexing_tree_remove (fixture->tree,
	                              fixture->test_dir[TEST_DIRECTORY_A]);

	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_A);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AA);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAA);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAAA);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAAB);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAB);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_AB);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_ABA);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_ABA);
}

/* If A is monitored (not recursively) and AA is monitored (recursively),
 *  then AA is removed from tree.
 *  -A, AA, AB are indexable.
 *  -AAA, AAAA, AAAB, AAB, ABA and ABB are not indexable
 */
static void
test_indexing_tree_024 (TestCommonContext *fixture,
                        gconstpointer      data)
{
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_A],
	                           TRACKER_DIRECTORY_FLAG_MONITOR);
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_AA],
	                           TRACKER_DIRECTORY_FLAG_MONITOR | TRACKER_DIRECTORY_FLAG_RECURSE);

	tracker_indexing_tree_remove (fixture->tree,
	                              fixture->test_dir[TEST_DIRECTORY_AA]);

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

/* If A is monitored (recursively) and AA is ignored, then A is removed
 * from tree.
 *  -A, AA, AAA, AAAA, AAAB, AAB, AB, ABA and ABB are not indexable
 */
static void
test_indexing_tree_025 (TestCommonContext *fixture,
                        gconstpointer      data)
{
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_A],
	                           TRACKER_DIRECTORY_FLAG_MONITOR | TRACKER_DIRECTORY_FLAG_RECURSE);
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_AA],
	                           TRACKER_DIRECTORY_FLAG_IGNORE);

	tracker_indexing_tree_remove (fixture->tree,
	                              fixture->test_dir[TEST_DIRECTORY_A]);

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

/* If A is monitored (recursively) and AA is ignored, then AA is removed
 * from tree.
 *  -A, AA, AAA, AAAA, AAAB, AAB, AB, ABA and ABB are indexable
 */
static void
test_indexing_tree_026 (TestCommonContext *fixture,
                        gconstpointer      data)
{
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_A],
	                           TRACKER_DIRECTORY_FLAG_MONITOR | TRACKER_DIRECTORY_FLAG_RECURSE);
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_AA],
	                           TRACKER_DIRECTORY_FLAG_IGNORE);

	tracker_indexing_tree_remove (fixture->tree,
	                              fixture->test_dir[TEST_DIRECTORY_AA]);

	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_A);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AA);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAA);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAAA);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAAB);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAB);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AB);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_ABA);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_ABA);
}

/* If A is monitored (recursively) and AA is monitored (not recursively), then
 *  A is removed from tree.
 *  -AA, AAA, AAB are indexable
 *  -A, AAAA, AAAB, AB, ABA and ABB are not indexable
 */
static void
test_indexing_tree_027 (TestCommonContext *fixture,
                        gconstpointer      data)
{
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_A],
	                           TRACKER_DIRECTORY_FLAG_MONITOR | TRACKER_DIRECTORY_FLAG_RECURSE);
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_AA],
	                           TRACKER_DIRECTORY_FLAG_MONITOR);

	tracker_indexing_tree_remove (fixture->tree,
	                              fixture->test_dir[TEST_DIRECTORY_A]);

	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_A);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AA);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAA);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_AAAA);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_AAAB);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAB);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_AB);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_ABA);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_ABA);
}

/* If A is monitored (recursively) and AA is monitored (not recursively), then
 *  AA is removed from tree.
 *  -A, AA, AAA, AAAA, AAAB, AAB, AB, ABA and ABB are indexable
 */
static void
test_indexing_tree_028 (TestCommonContext *fixture,
                        gconstpointer      data)
{
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_A],
	                           TRACKER_DIRECTORY_FLAG_MONITOR | TRACKER_DIRECTORY_FLAG_RECURSE);
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_AA],
	                           TRACKER_DIRECTORY_FLAG_MONITOR);

	tracker_indexing_tree_remove (fixture->tree,
	                              fixture->test_dir[TEST_DIRECTORY_AA]);

	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_A);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AA);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAA);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAAA);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAAB);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAB);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AB);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_ABA);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_ABA);
}

/* If A is monitored (recursively) and AA is monitored (recursively),
 * then A is removed from tree.
 *  -AA, AAA, AAAA, AAAB, AAB are indexable
 *  -A, AB, ABA and ABB are not indexable
 */
static void
test_indexing_tree_029 (TestCommonContext *fixture,
                        gconstpointer      data)
{
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_A],
	                           TRACKER_DIRECTORY_FLAG_MONITOR | TRACKER_DIRECTORY_FLAG_RECURSE);
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_AA],
	                           TRACKER_DIRECTORY_FLAG_MONITOR | TRACKER_DIRECTORY_FLAG_RECURSE);

	tracker_indexing_tree_remove (fixture->tree,
	                              fixture->test_dir[TEST_DIRECTORY_A]);

	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_A);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AA);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAA);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAAA);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAAB);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAB);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_AB);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_ABA);
	ASSERT_NOT_INDEXABLE (fixture, TEST_DIRECTORY_ABA);
}

/* If A is monitored (recursively) and AA is monitored (recursively),
 * then AA is removed from tree.
 *  -A, AA, AAA, AAAA, AAAB, AAB, AB, ABA and ABB are indexable
 */
static void
test_indexing_tree_030 (TestCommonContext *fixture,
                        gconstpointer      data)
{
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_A],
	                           TRACKER_DIRECTORY_FLAG_MONITOR | TRACKER_DIRECTORY_FLAG_RECURSE);
	tracker_indexing_tree_add (fixture->tree,
	                           fixture->test_dir[TEST_DIRECTORY_AA],
	                           TRACKER_DIRECTORY_FLAG_MONITOR | TRACKER_DIRECTORY_FLAG_RECURSE);

	tracker_indexing_tree_remove (fixture->tree,
	                              fixture->test_dir[TEST_DIRECTORY_AA]);

	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_A);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AA);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAA);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAAA);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAAB);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AAB);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_AB);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_ABA);
	ASSERT_INDEXABLE (fixture, TEST_DIRECTORY_ABA);
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
	test_add ("/libtracker-miner/indexing-tree/003", test_indexing_tree_003);
	test_add ("/libtracker-miner/indexing-tree/004", test_indexing_tree_004);
	test_add ("/libtracker-miner/indexing-tree/005", test_indexing_tree_005);
	test_add ("/libtracker-miner/indexing-tree/006", test_indexing_tree_006);
	test_add ("/libtracker-miner/indexing-tree/007", test_indexing_tree_007);
	test_add ("/libtracker-miner/indexing-tree/008", test_indexing_tree_008);
	test_add ("/libtracker-miner/indexing-tree/009", test_indexing_tree_009);
	test_add ("/libtracker-miner/indexing-tree/010", test_indexing_tree_010);
	test_add ("/libtracker-miner/indexing-tree/011", test_indexing_tree_011);
	test_add ("/libtracker-miner/indexing-tree/012", test_indexing_tree_012);
	test_add ("/libtracker-miner/indexing-tree/013", test_indexing_tree_013);
	test_add ("/libtracker-miner/indexing-tree/014", test_indexing_tree_014);
	test_add ("/libtracker-miner/indexing-tree/015", test_indexing_tree_015);
	test_add ("/libtracker-miner/indexing-tree/016", test_indexing_tree_016);
	test_add ("/libtracker-miner/indexing-tree/017", test_indexing_tree_017);
	test_add ("/libtracker-miner/indexing-tree/018", test_indexing_tree_018);
	test_add ("/libtracker-miner/indexing-tree/019", test_indexing_tree_019);
	test_add ("/libtracker-miner/indexing-tree/020", test_indexing_tree_020);
	test_add ("/libtracker-miner/indexing-tree/021", test_indexing_tree_021);
	test_add ("/libtracker-miner/indexing-tree/022", test_indexing_tree_022);
	test_add ("/libtracker-miner/indexing-tree/023", test_indexing_tree_023);
	test_add ("/libtracker-miner/indexing-tree/024", test_indexing_tree_024);
	test_add ("/libtracker-miner/indexing-tree/025", test_indexing_tree_025);
	test_add ("/libtracker-miner/indexing-tree/026", test_indexing_tree_026);
	test_add ("/libtracker-miner/indexing-tree/027", test_indexing_tree_027);
	test_add ("/libtracker-miner/indexing-tree/028", test_indexing_tree_028);
	test_add ("/libtracker-miner/indexing-tree/029", test_indexing_tree_029);
	test_add ("/libtracker-miner/indexing-tree/030", test_indexing_tree_030);

	return g_test_run ();
}
