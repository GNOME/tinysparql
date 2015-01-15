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

#include "config.h"

#include <locale.h>

/* Not normally public */
#include <libtracker-miner/tracker-crawler.h>
#include <libtracker-miner/tracker-file-data-provider.h>

typedef struct CrawlerTest CrawlerTest;

struct CrawlerTest {
	TrackerDataProvider *data_provider;

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
test_crawler_common_setup (CrawlerTest   *fixture,
                           gconstpointer  data)
{
	fixture->data_provider = tracker_file_data_provider_new ();
	fixture->main_loop = g_main_loop_new (NULL, FALSE);
}

static void
test_crawler_common_teardown (CrawlerTest   *fixture,
                              gconstpointer  data)
{
	if (fixture->data_provider) {
		g_object_unref (fixture->data_provider);
	}

	if (fixture->main_loop) {
		g_main_loop_quit (fixture->main_loop);
		g_main_loop_unref (fixture->main_loop);
	}
}

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
test_crawler_crawl (CrawlerTest   *fixture,
                    gconstpointer  data)
{
	TrackerDataProvider *data_provider;
	TrackerCrawler *crawler;
	gboolean started;
	GFile *file;

	crawler = tracker_crawler_new (fixture->data_provider);
	g_signal_connect (crawler, "finished",
			  G_CALLBACK (crawler_finished_cb), fixture);

	file = g_file_new_for_path (TEST_DATA_DIR);

	started = tracker_crawler_start (crawler, file, TRACKER_DIRECTORY_FLAG_NONE, -1);

	g_assert_cmpint (started, ==, 1);

	g_main_loop_run (fixture->main_loop);

	g_assert_cmpint (fixture->interrupted, ==, 0);

	g_object_unref (crawler);
	g_object_unref (file);
}

static void
test_crawler_crawl_interrupted (CrawlerTest   *fixture,
                                gconstpointer  data)
{
	TrackerCrawler *crawler;
	gboolean started;
	GFile *file;

	crawler = tracker_crawler_new (fixture->data_provider);
	g_signal_connect (crawler, "finished",
			  G_CALLBACK (crawler_finished_cb), fixture);

	file = g_file_new_for_path (TEST_DATA_DIR);

	started = tracker_crawler_start (crawler, file, TRACKER_DIRECTORY_FLAG_NONE, -1);

	g_assert_cmpint (started, ==, 1);

	tracker_crawler_stop (crawler);

	g_assert_cmpint (fixture->interrupted, ==, 1);

	g_object_unref (crawler);
	g_object_unref (file);
}

static void
test_crawler_crawl_nonexisting (CrawlerTest   *fixture,
                                gconstpointer  data)
{
	TrackerCrawler *crawler;
	GFile *file;
	gboolean started;

	crawler = tracker_crawler_new (fixture->data_provider);
	file = g_file_new_for_path (TEST_DATA_DIR "-idontexist");

	started = tracker_crawler_start (crawler, file, TRACKER_DIRECTORY_FLAG_NONE, -1);

	g_assert_cmpint (started, ==, 0);

	g_object_unref (crawler);
	g_object_unref (file);
}

static void
test_crawler_crawl_recursive (CrawlerTest   *fixture,
                              gconstpointer  data)
{
	TrackerCrawler *crawler;
	GFile *file;

	crawler = tracker_crawler_new (fixture->data_provider);
	g_signal_connect (crawler, "finished",
			  G_CALLBACK (crawler_finished_cb), fixture);
	g_signal_connect (crawler, "directory-crawled",
			  G_CALLBACK (crawler_directory_crawled_cb), fixture);

	file = g_file_new_for_path (TEST_DATA_DIR);

	tracker_crawler_start (crawler, file, TRACKER_DIRECTORY_FLAG_NONE, -1);

	g_main_loop_run (fixture->main_loop);

	/* There are 4 directories and 5 (2 hidden) files */
	g_assert_cmpint (fixture->directories_found, ==, 4);
	g_assert_cmpint (fixture->directories_ignored, ==, 0);
	g_assert_cmpint (fixture->files_found, ==, 5);
	g_assert_cmpint (fixture->files_ignored, ==, 0);

	g_object_unref (crawler);
	g_object_unref (file);
}

static void
test_crawler_crawl_non_recursive (CrawlerTest   *fixture,
                                  gconstpointer  data)
{
	TrackerCrawler *crawler;
	GFile *file;

	crawler = tracker_crawler_new (fixture->data_provider);
	g_signal_connect (crawler, "finished",
			  G_CALLBACK (crawler_finished_cb), fixture);
	g_signal_connect (crawler, "directory-crawled",
			  G_CALLBACK (crawler_directory_crawled_cb), fixture);

	file = g_file_new_for_path (TEST_DATA_DIR);

	tracker_crawler_start (crawler, file, TRACKER_DIRECTORY_FLAG_NONE, 1);

	g_main_loop_run (fixture->main_loop);

	/* There are 3 directories (including parent) and 1 file in toplevel dir */
	g_assert_cmpint (fixture->directories_found, ==, 3);
	g_assert_cmpint (fixture->directories_ignored, ==, 0);
	g_assert_cmpint (fixture->files_found, ==, 1);
	g_assert_cmpint (fixture->files_ignored, ==, 0);

	g_object_unref (crawler);
	g_object_unref (file);
}

static void
test_crawler_crawl_n_signals (CrawlerTest   *fixture,
                              gconstpointer  data)
{
	TrackerCrawler *crawler;
	GFile *file;

	crawler = tracker_crawler_new (fixture->data_provider);
	g_signal_connect (crawler, "finished",
			  G_CALLBACK (crawler_finished_cb), fixture);
	g_signal_connect (crawler, "directory-crawled",
			  G_CALLBACK (crawler_directory_crawled_cb), fixture);
	g_signal_connect (crawler, "check-directory",
			  G_CALLBACK (crawler_check_directory_cb), fixture);
	g_signal_connect (crawler, "check-directory-contents",
			  G_CALLBACK (crawler_check_directory_contents_cb), fixture);
	g_signal_connect (crawler, "check-file",
			  G_CALLBACK (crawler_check_file_cb), fixture);

	file = g_file_new_for_path (TEST_DATA_DIR);

	tracker_crawler_start (crawler, file, TRACKER_DIRECTORY_FLAG_NONE, -1);

	g_main_loop_run (fixture->main_loop);

	g_assert_cmpint (fixture->directories_found, ==, fixture->n_check_directory);
	g_assert_cmpint (fixture->directories_found, ==, fixture->n_check_directory_contents);
	g_assert_cmpint (fixture->files_found, ==, fixture->n_check_file);

	g_object_unref (crawler);
	g_object_unref (file);
}

static void
test_crawler_crawl_n_signals_non_recursive (CrawlerTest   *fixture,
                                            gconstpointer  data)
{
	TrackerCrawler *crawler;
	GFile *file;

	crawler = tracker_crawler_new (fixture->data_provider);
	g_signal_connect (crawler, "finished",
			  G_CALLBACK (crawler_finished_cb), fixture);
	g_signal_connect (crawler, "directory-crawled",
			  G_CALLBACK (crawler_directory_crawled_cb), fixture);
	g_signal_connect (crawler, "check-directory",
			  G_CALLBACK (crawler_check_directory_cb), fixture);
	g_signal_connect (crawler, "check-directory-contents",
			  G_CALLBACK (crawler_check_directory_contents_cb), fixture);
	g_signal_connect (crawler, "check-file",
			  G_CALLBACK (crawler_check_file_cb), fixture);

	file = g_file_new_for_path (TEST_DATA_DIR);

	tracker_crawler_start (crawler, file, TRACKER_DIRECTORY_FLAG_NONE, 1);

	g_main_loop_run (fixture->main_loop);

	g_assert_cmpint (fixture->directories_found, ==, fixture->n_check_directory);
	g_assert_cmpint (1, ==, fixture->n_check_directory_contents);
	g_assert_cmpint (fixture->files_found, ==, fixture->n_check_file);

	g_object_unref (crawler);
	g_object_unref (file);
}

int
main (int argc, char **argv)
{
	setlocale (LC_ALL, "");

	g_test_init (&argc, &argv, NULL);

	g_test_message ("Testing filesystem crawler");

	g_test_add ("/libtracker-miner/tracker-crawler/crawl/normal",
	            CrawlerTest,
	            NULL,
	            test_crawler_common_setup,
	            test_crawler_crawl,
	            test_crawler_common_teardown);
	g_test_add ("/libtracker-miner/tracker-crawler/crawl/interrupted",
	            CrawlerTest,
	            NULL,
	            test_crawler_common_setup,
	            test_crawler_crawl_interrupted,
	            test_crawler_common_teardown);
	g_test_add ("/libtracker-miner/tracker-crawler/crawl/non-existing",
	            CrawlerTest,
	            NULL,
	            test_crawler_common_setup,
	            test_crawler_crawl_nonexisting,
	            test_crawler_common_teardown);

	g_test_add ("/libtracker-miner/tracker-crawler/crawl/recursive",
	            CrawlerTest,
	            NULL,
	            test_crawler_common_setup,
	            test_crawler_crawl_recursive,
	            test_crawler_common_teardown);
	g_test_add ("/libtracker-miner/tracker-crawler/crawl/non-recursive",
	            CrawlerTest,
	            NULL,
	            test_crawler_common_setup,
	            test_crawler_crawl_non_recursive,
	            test_crawler_common_teardown);

	g_test_add ("/libtracker-miner/tracker-crawler/crawl/n-signals",
	            CrawlerTest,
	            NULL,
	            test_crawler_common_setup,
	            test_crawler_crawl_n_signals,
	            test_crawler_common_teardown);
	g_test_add ("/libtracker-miner/tracker-crawler/crawl/n-signals-non-recursive",
	            CrawlerTest,
	            NULL,
	            test_crawler_common_setup,
	            test_crawler_crawl_n_signals_non_recursive,
	            test_crawler_common_teardown);

	return g_test_run ();
}
