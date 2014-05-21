/*
 * Copyright (C) 2014, Softathome <contact@softathome.com>
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

#include <libtracker-miner/tracker-file-enumerator.h>

static void
test_enumerator_crawl (void)
{
	TrackerEnumerator *enumerator;
	GFile *dir;
	GSList *files, *l;
	GError *error = NULL;
	const gchar *path;

	setlocale (LC_ALL, "");

	enumerator = tracker_file_enumerator_new ();
	g_assert (enumerator != NULL);

	/* FIXME: Use better tmp data structure */
	path = "/tmp";
	dir = g_file_new_for_path (path);
	g_print ("'%s'\n", path);

	files = tracker_enumerator_get_children (enumerator,
	                                         dir,
	                                         G_FILE_ATTRIBUTE_STANDARD_NAME "," \
	                                         G_FILE_ATTRIBUTE_STANDARD_TYPE,
	                                         G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
	                                         NULL,
	                                         &error);
	g_assert_no_error (error);
	g_assert (files != NULL);
	g_assert (g_slist_length (files) > 0);

	for (l = files; l; l = l->next) {
		GFileInfo *info = l->data;

		g_print ("-> '%s'\n", g_file_info_get_name (info));
	}

	g_object_unref (dir);
}

int
main (int    argc,
      char **argv)
{
	g_test_init (&argc, &argv, NULL);

	g_test_message ("Testing file enumerator");

	g_test_add_func ("/libtracker-miner/tracker-enumerator/crawl",
	                 test_enumerator_crawl);

	return g_test_run ();
}
