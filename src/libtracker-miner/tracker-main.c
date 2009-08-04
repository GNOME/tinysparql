/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2009, Nokia

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

#include <libtracker-common/tracker-utils.h>

#include "tracker-miner-test.h"

static void
miner_terminated_cb (TrackerMiner *miner,
		     gpointer      user_data)
{
        GMainLoop *main_loop = user_data;

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
check_directory_cb (TrackerProcessor *processor,
		    GFile	       *file,
		    gpointer          user_data)
{
	gchar *path;
	gchar *basename;
	gboolean should_process;

	should_process = FALSE;
	basename = NULL;
	path = g_file_get_path (file);

	if (tracker_is_empty_string (path)) {
		goto done;
	}

	if (!g_utf8_validate (path, -1, NULL)) {
		g_message ("Ignoring path:'%s', not valid UTF-8", path);
		goto done;
	}

	/* Most common things to ignore */
	if (strcmp (path, "/dev") == 0 ||
	    strcmp (path, "/lib") == 0 ||
	    strcmp (path, "/proc") == 0 ||
	    strcmp (path, "/sys") == 0) {
		goto done;
	}
	
	if (g_str_has_prefix (path, g_get_tmp_dir ())) {
		goto done;
	}
	
	/* Check ignored directories in config */
	basename = g_file_get_basename (file);

	if (!basename) {
		goto done;
	}
	
	/* If directory begins with ".", check it isn't one of
	 * the top level directories to watch/crawl if it
	 * isn't we ignore it. If it is, we don't.
	 */
	if (basename[0] == '.') {
		goto done;
	}
	
	/* Check module directory ignore patterns */
	should_process = TRUE;

done:
	g_free (path);
	g_free (basename);

	return should_process;
}

static gboolean
check_file_cb (TrackerProcessor *processor,
	       GFile	        *file,
	       gpointer	         user_data)
{
	gchar *path;
	gchar *basename;
	gboolean should_process;

	should_process = FALSE;
	basename = NULL;
	path = g_file_get_path (file);

	if (tracker_is_empty_string (path)) {
		goto done;
	}

	if (!g_utf8_validate (path, -1, NULL)) {
		g_message ("Ignoring path:'%s', not valid UTF-8", path);
		goto done;
	}

	/* Check basename against pattern matches */
	basename = g_file_get_basename (file);

	if (!basename || basename[0] == '.') {
		goto done;
	}

	should_process = TRUE;

done:
	g_free (path);
	g_free (basename);

	return should_process;
}

static void
process_file_cb (TrackerProcessor *processor,
		 GFile	          *file,
		 gpointer          user_data)
{
	gchar *path;

	path = g_file_get_path (file);
	g_print ("** PROCESSING FILE:'%s'\n", path);
	g_free (path);
}

static gboolean
monitor_directory_cb (TrackerProcessor *processor,
		      GFile	         *file,
		      gpointer          user_data)
{
	return TRUE;
}

int
main (int argc, char *argv[])
{
        TrackerMiner *miner;
        GMainLoop *main_loop;

        g_type_init ();

        if (!g_thread_supported ()) { 
		g_thread_init (NULL);
	}

        main_loop = g_main_loop_new (NULL, FALSE);

        miner = tracker_miner_test_new ("test");

	g_signal_connect (TRACKER_PROCESSOR (miner), "check-file",
			  G_CALLBACK (check_file_cb),
			  NULL);
	g_signal_connect (TRACKER_PROCESSOR (miner), "check-directory",
			  G_CALLBACK (check_directory_cb),
			  NULL);
	g_signal_connect (TRACKER_PROCESSOR (miner), "process-file",
			  G_CALLBACK (process_file_cb),
			  NULL);
	g_signal_connect (TRACKER_PROCESSOR (miner), "monitor-directory",
			  G_CALLBACK (monitor_directory_cb),
			  NULL);

	tracker_processor_add_directory (TRACKER_PROCESSOR (miner), 
					 g_get_home_dir (),
					 FALSE);
	tracker_processor_add_directory (TRACKER_PROCESSOR (miner), 
					 g_get_tmp_dir (),
					 TRUE);
	tracker_processor_add_directory (TRACKER_PROCESSOR (miner), 
					 g_get_current_dir (),
					 TRUE);
	tracker_processor_add_directory (TRACKER_PROCESSOR (miner), 
					 g_get_user_special_dir (G_USER_DIRECTORY_PICTURES),
					 TRUE);
	tracker_processor_add_directory (TRACKER_PROCESSOR (miner), 
					 g_get_user_special_dir (G_USER_DIRECTORY_MUSIC),
					 TRUE);
	tracker_processor_add_directory (TRACKER_PROCESSOR (miner), 
					 g_get_user_special_dir (G_USER_DIRECTORY_VIDEOS),
					 TRUE);
	tracker_processor_add_directory (TRACKER_PROCESSOR (miner), 
					 g_get_user_special_dir (G_USER_DIRECTORY_DOWNLOAD),
					 FALSE);
	tracker_processor_add_directory (TRACKER_PROCESSOR (miner), 
					 g_get_user_special_dir (G_USER_DIRECTORY_DOCUMENTS),
					 TRUE);
	tracker_processor_add_directory (TRACKER_PROCESSOR (miner), 
					 g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP),
					 TRUE);

        g_signal_connect (miner, "terminated",
                          G_CALLBACK (miner_terminated_cb), 
			  main_loop);
	g_timeout_add_seconds (1, miner_start_cb, miner);

        g_main_loop_run (main_loop);
	g_object_unref (main_loop);

        return EXIT_SUCCESS;
}
