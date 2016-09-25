/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
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

#include <string.h>
#include <stdlib.h>

#include <glib.h>

#include "tracker-miner-test.h"

static void
miner_finished_cb (TrackerMiner *miner,
                   gdouble       seconds_elapsed,
                   guint         total_directories_found,
                   guint         total_directories_ignored,
                   guint         total_files_found,
                   guint         total_files_ignored,
                   gpointer      user_data)
{
	GMainLoop *main_loop = user_data;

	g_message ("Finished mining in seconds:%f, total directories:%d, total files:%d",
	           seconds_elapsed,
	           total_directories_found + total_directories_ignored,
	           total_files_found + total_files_ignored);

	g_main_loop_quit (main_loop);
}

static gboolean
miner_start_cb (gpointer user_data)
{
	TrackerMiner *miner = user_data;

	g_message ("Starting miner");
	tracker_miner_start (miner);

	return FALSE;
}

static gboolean
process_file_cb (TrackerMinerFS *fs,
                 GFile          *file,
                 gpointer        user_data)
{
	gchar *path;

	path = g_file_get_path (file);
	g_print ("** PROCESSING FILE:'%s'\n", path);
	g_free (path);

	/* Notify that processing is complete. */
	tracker_miner_fs_file_notify (fs, file, NULL);

	/* Return FALSE here if you ignored the file. */
	return TRUE;
}

static void
add_directory_path (TrackerMinerFS *fs,
                    const gchar    *path,
                    gboolean        recurse)
{
	GFile *file;

	file = g_file_new_for_path (path);
	tracker_miner_fs_directory_add (fs, file, recurse);
	g_object_unref (file);
}

static void
add_special_directory (TrackerMinerFS *fs,
                       GUserDirectory  dir,
                       const char     *dir_name,
                       gboolean        recurse)
{
	if (strcmp (g_get_user_special_dir (dir), g_get_home_dir ()) == 0) {
		g_message ("User dir %s is set to home directory; ignoring.", dir_name);
	} else {
		add_directory_path (fs,
		                    g_get_user_special_dir (dir),
		                    recurse);
	}
}

int
main (int argc, char *argv[])
{
	TrackerMiner *miner;
	TrackerIndexingTree *tree;
	GMainLoop *main_loop;

	main_loop = g_main_loop_new (NULL, FALSE);

	miner = tracker_miner_test_new ("test");

	g_signal_connect (TRACKER_MINER_FS (miner), "process-file",
	                  G_CALLBACK (process_file_cb),
	                  NULL);

	tree = tracker_miner_fs_get_indexing_tree (TRACKER_MINER_FS (miner));

	/* Ignore files that g_file_info_get_is_hidden() tells us are hidden files. */
	tracker_indexing_tree_set_filter_hidden (tree, TRUE);

	/* Ignore special filesystems that definitely shouldn't be indexed */
	/* FIXME: it would be better to avoid based on filesystem type; i.e. avoid
	 * devtmpfs, sysfs and procfs filesystems. */
	tracker_indexing_tree_add_filter(tree, TRACKER_FILTER_PARENT_DIRECTORY, "/dev");
	tracker_indexing_tree_add_filter(tree, TRACKER_FILTER_PARENT_DIRECTORY, "/proc");
	tracker_indexing_tree_add_filter(tree, TRACKER_FILTER_PARENT_DIRECTORY, "/sys");

	tracker_indexing_tree_add_filter(tree, TRACKER_FILTER_PARENT_DIRECTORY, g_get_tmp_dir());

	add_directory_path (TRACKER_MINER_FS (miner),
	                    g_get_home_dir (),
	                    FALSE);

	/* This should be ignored */
	add_directory_path (TRACKER_MINER_FS (miner),
	                    g_get_tmp_dir (),
	                    TRUE);

	add_special_directory (TRACKER_MINER_FS (miner), G_USER_DIRECTORY_PICTURES, "PICTURES", TRUE);
	add_special_directory (TRACKER_MINER_FS (miner), G_USER_DIRECTORY_MUSIC, "MUSIC", TRUE);
	add_special_directory (TRACKER_MINER_FS (miner), G_USER_DIRECTORY_VIDEOS, "VIDEOS", TRUE);
	add_special_directory (TRACKER_MINER_FS (miner), G_USER_DIRECTORY_DOWNLOAD, "DOWNLOAD", TRUE);
	add_special_directory (TRACKER_MINER_FS (miner), G_USER_DIRECTORY_DOCUMENTS, "DOCUMENTS", TRUE);
	add_special_directory (TRACKER_MINER_FS (miner), G_USER_DIRECTORY_DESKTOP, "DESKTOP", TRUE);

	g_signal_connect (miner, "finished",
	                  G_CALLBACK (miner_finished_cb),
	                  main_loop);
	g_timeout_add_seconds (1, miner_start_cb, miner);

	g_main_loop_run (main_loop);

	return EXIT_SUCCESS;
}
