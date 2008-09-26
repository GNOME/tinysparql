/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia
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
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>
#include <glib/gstdio.h>

#include "tracker-nfs-lock.h"
#include "tracker-log.h"

static gchar *lock_filename;
static gchar *tmp_dir;

static gboolean use_nfs_safe_locking;

/* Get no of links to a file - used for safe NFS atomic file locking */
static gint
get_nlinks (const gchar *filename)
{
	struct stat st;

	if (g_stat (filename, &st) == 0) {
		return st.st_nlink;
	} else {
		return -1;
	}
}

static gint
get_mtime (const gchar *filename)
{
	struct stat st;

	if (g_stat (filename, &st) == 0) {
		return st.st_mtime;
	} else {
		return -1;
	}
}

static gboolean
is_initialized (void)
{
	return lock_filename != NULL || tmp_dir != NULL;
}

/* Serialises db access via a lock file for safe use on (lock broken)
 * NFS mounts.
 */
gboolean
tracker_nfs_lock_obtain (void)
{
	gchar *filename;
	gint   attempt;
	gint   fd;

	if (!use_nfs_safe_locking) {
		return TRUE;
	}

	if (!is_initialized()) {
		g_critical ("Could not initialize NFS lock");
		return FALSE;
	}

	filename = g_strdup_printf ("%s_%s.lock",
				    tmp_dir,
				    g_get_user_name ());

	for (attempt = 0; attempt < 10000; ++attempt) {
		/* Delete existing lock file if older than 5 mins */
		if (g_file_test (lock_filename, G_FILE_TEST_EXISTS) &&
		    time ((time_t *) - get_mtime (lock_filename)) > 300) {
			g_unlink (lock_filename);
		}

		fd = g_open (lock_filename, O_CREAT | O_EXCL, 0644);

		if (fd >= 0) {
			/* Create host specific file and link to lock file */
			if (link (lock_filename, filename) == -1) {
				goto error;
			}

			/* For atomic NFS-safe locks, stat links = 2
			 * if file locked. If greater than 2 then we
			 * have a race condition.
			 */
			if (get_nlinks (lock_filename) == 2) {
				close (fd);
				g_free (filename);

				return TRUE;
			} else {
				close (fd);
				g_usleep (g_random_int_range (1000, 100000));
			}
		}
	}

error:
	g_critical ("Could not get NFS lock state");
	g_free (filename);

	return FALSE;
}

void
tracker_nfs_lock_release (void)
{
	gchar *filename;

	if (!use_nfs_safe_locking) {
		return;
	}

	if (!is_initialized ()) {
		g_critical ("Could not initialize NFS lock");
		return;
	}

	filename = g_strdup_printf ("%s_%s.lock",
				    tmp_dir,
				    g_get_user_name ());

	g_unlink (filename);
	g_unlink (lock_filename);

	g_free (filename);
}

void
tracker_nfs_lock_init (gboolean nfs)
{
	if (is_initialized ()) {
		return;
	}

	use_nfs_safe_locking = nfs;

	if (lock_filename == NULL) {
		lock_filename = g_build_filename (g_get_user_data_dir (),
						  "tracker",
						  "tracker.lock",
						  NULL);
	}

	if (tmp_dir == NULL) {
		tmp_dir = g_build_filename (g_get_user_data_dir (),
					    "tracker",
					    g_get_host_name (),
					    NULL);
	}

	g_message ("NFS lock initialized %s",
		   use_nfs_safe_locking ? "" : "(safe locking not in use)");
}

void
tracker_nfs_lock_shutdown (void)
{
	if (!is_initialized ()) {
		return;
	}

	if (lock_filename) {
		g_free (lock_filename);
		lock_filename = NULL;
	}

	if (tmp_dir) {
		g_free (tmp_dir);
		tmp_dir = NULL;
	}

	g_message ("NFS lock finalized");
}
