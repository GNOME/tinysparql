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

#include <tracker-indexing-tree.h>

static void
test_indexing_tree_recursive (void)
{
	TrackerIndexingTree *tree;
	GFile *recursive;
	GFile *aux;

	tree = tracker_indexing_tree_new ();

	recursive = g_file_new_for_path ("/home/user/Music");
	tracker_indexing_tree_add (tree,
	                           recursive,
	                           (TRACKER_DIRECTORY_FLAG_MONITOR | TRACKER_DIRECTORY_FLAG_RECURSE));

	aux = g_file_new_for_path ("/home/user/Music/Album");
	g_assert (tracker_indexing_tree_file_is_indexable (tree, aux, G_FILE_TYPE_DIRECTORY) == TRUE);
	g_object_unref (aux);

	aux = g_file_new_for_path ("/home/user/Music/File.mp3");
	g_assert (tracker_indexing_tree_file_is_indexable (tree, aux, G_FILE_TYPE_REGULAR) == TRUE);
	g_object_unref (aux);

	aux = g_file_new_for_path ("/home/user/Music/Album/Artist");
	g_assert (tracker_indexing_tree_file_is_indexable (tree, aux, G_FILE_TYPE_DIRECTORY) == TRUE);
	g_object_unref (aux);

	aux = g_file_new_for_path ("/home/user/Music/Artist/File.mp3");
	g_assert (tracker_indexing_tree_file_is_indexable (tree, aux, G_FILE_TYPE_REGULAR) == TRUE);
	g_object_unref (aux);


	g_object_unref (aux);
	g_object_unref (tree);
}

static void
test_indexing_tree_non_recursive (void)
{
	TrackerIndexingTree *tree;
	GFile *recursive;
	GFile *aux;

	tree = tracker_indexing_tree_new ();

	recursive = g_file_new_for_path ("/home/user/Music");
	tracker_indexing_tree_add (tree,
	                           recursive,
	                           (TRACKER_DIRECTORY_FLAG_MONITOR);

	aux = g_file_new_for_path ("/home/user/Music/Album");
	g_assert (tracker_indexing_tree_file_is_indexable (tree, aux, G_FILE_TYPE_DIRECTORY) == TRUE);
	g_object_unref (aux);

	aux = g_file_new_for_path ("/home/user/Music/File.mp3");
	g_assert (tracker_indexing_tree_file_is_indexable (tree, aux, G_FILE_TYPE_REGULAR) == TRUE);
	g_object_unref (aux);

	aux = g_file_new_for_path ("/home/user/Music/Album/Artist");
	g_assert (tracker_indexing_tree_file_is_indexable (tree, aux, G_FILE_TYPE_DIRECTORY) == FALSE);
	g_object_unref (aux);

	aux = g_file_new_for_path ("/home/user/Music/Artist/File.mp3");
	g_assert (tracker_indexing_tree_file_is_indexable (tree, aux, G_FILE_TYPE_REGULAR) == FALSE);
	g_object_unref (aux);

	g_object_unref (aux);
	g_object_unref (tree);
}

gint
main (gint    argc,
      gchar **argv)
{
	g_thread_init (NULL);
	g_type_init ();
	g_test_init (&argc, &argv, NULL);

	g_test_message ("Testing indexing tree");

	g_test_add_func ("/libtracker-miner/tracker-indexing-tree/recursive",
	                 test_indexing_tree_recursive);
	g_test_add_func ("/libtracker-miner/tracker-indexing-tree/non-recursive",
	                 test_indexing_tree_non_recursive);

	return g_test_run ();
}
