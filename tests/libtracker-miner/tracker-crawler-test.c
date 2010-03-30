/*
 * Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
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

#include <libtracker-miner/tracker-miner.h>

typedef struct CrawlerTest CrawlerTest;

struct CrawlerTest {
	GMainLoop *main_loop;
	guint directories_found;
	guint directories_ignored;
	guint files_found;
	guint files_ignored;
	gboolean interrupted;

	/* signals statistics */
	guint n_check_directory;
	guint n_check_directory_contents;
	guint n_check_file;
};

static void
crawler_finished_cb (TrackerCrawler *crawler,
                     gboolean        interrupted,
                     gpointer        user_data)
{
	CrawlerTest *test = user_data;

	test->interrupted = interrupted;

	if (test->main_loop) {
		g_main_loop_quit (test->main_loop);
	}
}

static void
crawler_directory_crawled_cb (TrackerCrawler *crawler,
                              GFile          *directory,
                              GNode          *tree,
                              guint           directories_found,
                              guint           directories_ignored,
                              guint           files_found,
                              guint           files_ignored,
                              gpointer        user_data)
{
	CrawlerTest *test = user_data;

	test->directories_found = directories_found;
	test->directories_ignored = directories_ignored;
	test->files_found = files_found;
	test->files_ignored = files_ignored;

	g_assert_cmpint (g_node_n_nodes (tree, G_TRAVERSE_ALL), ==, directories_found + files_found);
}

static gboolean
crawler_check_directory_cb (TrackerCrawler *crawler,
			    GFile          *file,
			    gpointer        user_data)
{
	CrawlerTest *test = user_data;

	test->n_check_directory++;

	return TRUE;
}

static gboolean
crawler_check_file_cb (TrackerCrawler *crawler,
		       GFile          *file,
		       gpointer        user_data)
{
	CrawlerTest *test = user_data;

	test->n_check_file++;

	return TRUE;
}

static gboolean
crawler_check_directory_contents_cb (TrackerCrawler *crawler,
				     GFile          *file,
				     GList          *contents,
				     gpointer        user_data)
{
	CrawlerTest *test = user_data;

	test->n_check_directory_contents++;

	return TRUE;
}

static void
test_crawler_crawl (void)
{
	TrackerCrawler *crawler;
	CrawlerTest test = { 0 };
	gboolean started;
	GFile *file;

	test.main_loop = g_main_loop_new (NULL, FALSE);

	crawler = tracker_crawler_new ();
	g_signal_connect (crawler, "finished",
			  G_CALLBACK (crawler_finished_cb), &test);

	file = g_file_new_for_path (TEST_DATA_DIR);

	started = tracker_crawler_start (crawler, file, TRUE);

	g_assert_cmpint (started, ==, 1);

	g_main_loop_run (test.main_loop);

	g_assert_cmpint (test.interrupted, ==, 0);

	g_main_loop_unref (test.main_loop);
	g_object_unref (crawler);
	g_object_unref (file);
}

static void
test_crawler_crawl_interrupted (void)
{
	TrackerCrawler *crawler;
	CrawlerTest test = { 0 };
	gboolean started;
	GFile *file;

	crawler = tracker_crawler_new ();
	g_signal_connect (crawler, "finished",
			  G_CALLBACK (crawler_finished_cb), &test);

	file = g_file_new_for_path (TEST_DATA_DIR);

	started = tracker_crawler_start (crawler, file, TRUE);

	g_assert_cmpint (started, ==, 1);

	tracker_crawler_stop (crawler);

	g_assert_cmpint (test.interrupted, ==, 1);

	g_object_unref (crawler);
	g_object_unref (file);
}

static void
test_crawler_crawl_nonexisting (void)
{
	TrackerCrawler *crawler;
	GFile *file;
	gboolean started;

	crawler = tracker_crawler_new ();
	file = g_file_new_for_path (TEST_DATA_DIR "-idontexist");

	started = tracker_crawler_start (crawler, file, TRUE);

	g_assert_cmpint (started, ==, 0);

	g_object_unref (crawler);
	g_object_unref (file);
}

static void
test_crawler_crawl_recursive (void)
{
	TrackerCrawler *crawler;
	CrawlerTest test = { 0 };
	GFile *file;

	test.main_loop = g_main_loop_new (NULL, FALSE);

	crawler = tracker_crawler_new ();
	g_signal_connect (crawler, "finished",
			  G_CALLBACK (crawler_finished_cb), &test);
	g_signal_connect (crawler, "directory-crawled",
			  G_CALLBACK (crawler_directory_crawled_cb), &test);

	file = g_file_new_for_path (TEST_DATA_DIR);

	tracker_crawler_start (crawler, file, TRUE);

	g_main_loop_run (test.main_loop);

	/* There are 4 directories and 5 (2 hidden) files */
	g_assert_cmpint (test.directories_found, ==, 4);
	g_assert_cmpint (test.directories_ignored, ==, 0);
	g_assert_cmpint (test.files_found, ==, 5);
	g_assert_cmpint (test.files_ignored, ==, 0);

	g_main_loop_unref (test.main_loop);
	g_object_unref (crawler);
	g_object_unref (file);
}

static void
test_crawler_crawl_non_recursive (void)
{
	TrackerCrawler *crawler;
	CrawlerTest test = { 0 };
	GFile *file;

	test.main_loop = g_main_loop_new (NULL, FALSE);

	crawler = tracker_crawler_new ();
	g_signal_connect (crawler, "finished",
			  G_CALLBACK (crawler_finished_cb), &test);
	g_signal_connect (crawler, "directory-crawled",
			  G_CALLBACK (crawler_directory_crawled_cb), &test);

	file = g_file_new_for_path (TEST_DATA_DIR);

	tracker_crawler_start (crawler, file, FALSE);

	g_main_loop_run (test.main_loop);

	/* There are 3 directories (including parent) and 1 file in toplevel dir */
	g_assert_cmpint (test.directories_found, ==, 3);
	g_assert_cmpint (test.directories_ignored, ==, 0);
	g_assert_cmpint (test.files_found, ==, 1);
	g_assert_cmpint (test.files_ignored, ==, 0);

	g_main_loop_unref (test.main_loop);
	g_object_unref (crawler);
	g_object_unref (file);
}

static void
test_crawler_crawl_n_signals (void)
{
	TrackerCrawler *crawler;
	CrawlerTest test = { 0 };
	GFile *file;

	test.main_loop = g_main_loop_new (NULL, FALSE);

	crawler = tracker_crawler_new ();
	g_signal_connect (crawler, "finished",
			  G_CALLBACK (crawler_finished_cb), &test);
	g_signal_connect (crawler, "directory-crawled",
			  G_CALLBACK (crawler_directory_crawled_cb), &test);
	g_signal_connect (crawler, "check-directory",
			  G_CALLBACK (crawler_check_directory_cb), &test);
	g_signal_connect (crawler, "check-directory-contents",
			  G_CALLBACK (crawler_check_directory_contents_cb), &test);
	g_signal_connect (crawler, "check-file",
			  G_CALLBACK (crawler_check_file_cb), &test);

	file = g_file_new_for_path (TEST_DATA_DIR);

	tracker_crawler_start (crawler, file, TRUE);

	g_main_loop_run (test.main_loop);

	g_assert_cmpint (test.directories_found, ==, test.n_check_directory);
	g_assert_cmpint (test.directories_found, ==, test.n_check_directory_contents);
	g_assert_cmpint (test.files_found, ==, test.n_check_file);

	g_main_loop_unref (test.main_loop);
	g_object_unref (crawler);
	g_object_unref (file);
}

static void
test_crawler_crawl_n_signals_non_recursive (void)
{
	TrackerCrawler *crawler;
	CrawlerTest test = { 0 };
	GFile *file;

	test.main_loop = g_main_loop_new (NULL, FALSE);

	crawler = tracker_crawler_new ();
	g_signal_connect (crawler, "finished",
			  G_CALLBACK (crawler_finished_cb), &test);
	g_signal_connect (crawler, "directory-crawled",
			  G_CALLBACK (crawler_directory_crawled_cb), &test);
	g_signal_connect (crawler, "check-directory",
			  G_CALLBACK (crawler_check_directory_cb), &test);
	g_signal_connect (crawler, "check-directory-contents",
			  G_CALLBACK (crawler_check_directory_contents_cb), &test);
	g_signal_connect (crawler, "check-file",
			  G_CALLBACK (crawler_check_file_cb), &test);

	file = g_file_new_for_path (TEST_DATA_DIR);

	tracker_crawler_start (crawler, file, FALSE);

	g_main_loop_run (test.main_loop);

	g_assert_cmpint (test.directories_found, ==, test.n_check_directory);
	g_assert_cmpint (1, ==, test.n_check_directory_contents);
	g_assert_cmpint (test.files_found, ==, test.n_check_file);

	g_main_loop_unref (test.main_loop);
	g_object_unref (crawler);
	g_object_unref (file);
}

int
main (int    argc,
      char **argv)
{
	g_type_init ();
	g_thread_init (NULL);
	g_test_init (&argc, &argv, NULL);

	g_test_message ("Testing filesystem crawler");

	g_test_add_func ("/libtracker-miner/tracker-crawler/crawl",
	                 test_crawler_crawl);
	g_test_add_func ("/libtracker-miner/tracker-crawler/crawl-interrupted",
	                 test_crawler_crawl_interrupted);
	g_test_add_func ("/libtracker-miner/tracker-crawler/crawl-nonexisting",
	                 test_crawler_crawl_nonexisting);

	g_test_add_func ("/libtracker-miner/tracker-crawler/crawl-recursive",
	                 test_crawler_crawl_recursive);
	g_test_add_func ("/libtracker-miner/tracker-crawler/crawl-non-recursive",
	                 test_crawler_crawl_non_recursive);

	g_test_add_func ("/libtracker-miner/tracker-crawler/crawl-n-signals",
	                 test_crawler_crawl_n_signals);
	g_test_add_func ("/libtracker-miner/tracker-crawler/crawl-n-signals-non-recursive",
	                 test_crawler_crawl_n_signals_non_recursive);

	return g_test_run ();
}
