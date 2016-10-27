/*
 * Copyright (C) 2016 Red Hat
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

#include "tracker-init.h"

#include <errno.h>
#include <glib/gstdio.h>

/* Stamp files to know crawling/indexing state */
#define FIRST_INDEX_FILENAME          "first-index.txt"
#define LAST_CRAWL_FILENAME           "last-crawl.txt"
#define NEED_MTIME_CHECK_FILENAME     "no-need-mtime-check.txt"

inline static gchar *
get_first_index_filename (void)
{
        return g_build_filename (g_get_user_cache_dir (),
                                 "tracker",
                                 FIRST_INDEX_FILENAME,
                                 NULL);
}

/**
 * tracker_init_get_first_index_done:
 *
 * Check if first full index of files was already done.
 *
 * Returns: %TRUE if a first full index have been done, %FALSE otherwise.
 **/
gboolean
tracker_init_get_first_index_done (void)
{
        gboolean exists;
        gchar *filename;

        filename = get_first_index_filename ();
        exists = g_file_test (filename, G_FILE_TEST_EXISTS);
        g_free (filename);

        return exists;
}

/**
 * tracker_init_set_first_index_done:
 *
 * Set the status of the first full index of files. Should be set to
 *  %FALSE if the index was never done or if a reindex is needed. When
 *  the index is completed, should be set to %TRUE.
 **/
void
tracker_init_set_first_index_done (gboolean done)
{
        gboolean already_exists;
        gchar *filename;

        filename = get_first_index_filename ();
        already_exists = g_file_test (filename, G_FILE_TEST_EXISTS);

        if (done && !already_exists) {
                GError *error = NULL;

                /* If done, create stamp file if not already there */
                if (!g_file_set_contents (filename, PACKAGE_VERSION, -1, &error)) {
                        g_warning ("  Could not create file:'%s' failed, %s",
                                   filename,
                                   error->message);
                        g_error_free (error);
                } else {
                        g_message ("  First index file:'%s' created",
                                   filename);
                }
        } else if (!done && already_exists) {
                /* If NOT done, remove stamp file */
                g_message ("  Removing first index file:'%s'", filename);

                if (g_remove (filename)) {
                        g_warning ("    Could not remove file:'%s', %m",
                                   filename);
                }
        }

        g_free (filename);
}

inline static gchar *
get_last_crawl_filename (void)
{
        return g_build_filename (g_get_user_cache_dir (),
                                 "tracker",
                                 LAST_CRAWL_FILENAME,
                                 NULL);
}

/**
 * tracker_init_get_last_crawl_done:
 *
 * Check when last crawl was performed.
 *
 * Returns: time_t() value when last crawl occurred, otherwise 0.
 **/
guint64
tracker_init_get_last_crawl_done (void)
{
        gchar *filename;
        gchar *content;
        guint64 then;

        filename = get_last_crawl_filename ();

        if (!g_file_get_contents (filename, &content, NULL, NULL)) {
                g_message ("  No previous timestamp, crawling forced");
                return 0;
        }

        then = g_ascii_strtoull (content, NULL, 10);
        g_free (content);

        return then;
}

/**
 * tracker_db_manager_set_last_crawl_done:
 *
 * Set the status of the first full index of files. Should be set to
 *  %FALSE if the index was never done or if a reindex is needed. When
 *  the index is completed, should be set to %TRUE.
 **/
void
tracker_init_set_last_crawl_done (gboolean done)
{
        gboolean already_exists;
        gchar *filename;

        filename = get_last_crawl_filename ();
        already_exists = g_file_test (filename, G_FILE_TEST_EXISTS);

        if (done && !already_exists) {
                GError *error = NULL;
                gchar *content;

                content = g_strdup_printf ("%" G_GUINT64_FORMAT, (guint64) time (NULL));

                /* If done, create stamp file if not already there */
                if (!g_file_set_contents (filename, content, -1, &error)) {
                        g_warning ("  Could not create file:'%s' failed, %s",
                                   filename,
                                   error->message);
                        g_error_free (error);
                } else {
                        g_message ("  Last crawl file:'%s' created",
                                   filename);
                }

                g_free (content);
        } else if (!done && already_exists) {
                /* If NOT done, remove stamp file */
                g_message ("  Removing last crawl file:'%s'", filename);

                if (g_remove (filename)) {
                        g_warning ("    Could not remove file:'%s', %m",
                                   filename);
                }
        }

        g_free (filename);
}

inline static gchar *
get_need_mtime_check_filename (void)
{
	return g_build_filename (g_get_user_cache_dir (),
	                         "tracker",
	                         NEED_MTIME_CHECK_FILENAME,
	                         NULL);
}

/**
 * tracker_init_get_need_mtime_check:
 *
 * Check if the miner-fs was cleanly shutdown or not.
 *
 * Returns: %TRUE if we need to check mtimes for directories against
 * the database on the next start for the miner-fs, %FALSE otherwise.
 *
 * Since: 0.10
 **/
gboolean
tracker_init_get_need_mtime_check (void)
{
	gboolean exists;
	gchar *filename;

	filename = get_need_mtime_check_filename ();
	exists = g_file_test (filename, G_FILE_TEST_EXISTS);
	g_free (filename);

	/* Existence of the file means we cleanly shutdown before and
	 * don't need to do the mtime check again on this start.
	 */
	return !exists;
}

/**
 * tracker_init_set_need_mtime_check:
 * @needed: a #gboolean
 *
 * If the next start of miner-fs should perform a full mtime check
 * against each directory found and those in the database (for
 * complete synchronisation), then @needed should be #TRUE, otherwise
 * #FALSE.
 *
 * Creates a file in $HOME/.cache/tracker/ if an mtime check is not
 * needed. The idea behind this is that a check is forced if the file
 * is not cleaned up properly on shutdown (i.e. due to a crash or any
 * other uncontrolled shutdown reason).
 *
 * Since: 0.10
 **/
void
tracker_init_set_need_mtime_check (gboolean needed)
{
	gboolean already_exists;
	gchar *filename;

	filename = get_need_mtime_check_filename ();
	already_exists = g_file_test (filename, G_FILE_TEST_EXISTS);

	/* !needed = add file
	 *  needed = remove file
	 */
	if (!needed && !already_exists) {
		GError *error = NULL;

		/* Create stamp file if not already there */
		if (!g_file_set_contents (filename, PACKAGE_VERSION, -1, &error)) {
			g_warning ("  Could not create file:'%s' failed, %s",
			           filename,
			           error->message);
			g_error_free (error);
		} else {
			g_message ("  Need mtime check file:'%s' created",
			           filename);
		}
	} else if (needed && already_exists) {
		/* Remove stamp file */
		g_message ("  Removing need mtime check file:'%s'", filename);

		if (g_remove (filename)) {
			g_warning ("    Could not remove file:'%s', %m",
			           filename);
		}
	}

	g_free (filename);
}
