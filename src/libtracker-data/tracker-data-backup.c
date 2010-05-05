/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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

#include <glib.h>
#include <glib/gstdio.h>

#include <libtracker-db/tracker-db-manager.h>
#include <libtracker-db/tracker-db-journal.h>
#include <libtracker-data/tracker-data-manager.h>

#include "tracker-data-backup.h"

typedef struct {
	GFile *destination, *journal;
	TrackerDataBackupFinished callback;
	gpointer user_data;
	GDestroyNotify destroy;
	GError *error;
} BackupSaveInfo;

GQuark
tracker_data_backup_error_quark (void)
{
	return g_quark_from_static_string (TRACKER_DATA_BACKUP_ERROR_DOMAIN);
}

static void
free_backup_save_info (BackupSaveInfo *info)
{
	if (info->destination) {
		g_object_unref (info->destination);
	}

	if (info->journal) {
		g_object_unref (info->journal);
	}

	if (info->destroy) {
		info->destroy (info->user_data);
	}

	g_clear_error (&info->error);

	g_free (info);
}

static void
on_journal_copied (GObject *source_object,
                   GAsyncResult *res,
                   gpointer user_data)
{
	BackupSaveInfo *info = user_data;
	GError *error = NULL;

	g_file_copy_finish (info->journal, res, &error);

	if (info->callback) {
		info->callback (error, info->user_data);
	}

	free_backup_save_info (info);

	g_clear_error (&error);
}

void
tracker_data_backup_save (GFile *destination,
                          TrackerDataBackupFinished callback,
                          gpointer user_data,
                          GDestroyNotify destroy)
{
	BackupSaveInfo *info;

	info = g_new0 (BackupSaveInfo, 1);
	info->destination = g_object_ref (destination);
	info->journal = g_file_new_for_path (tracker_db_journal_get_filename ());
	info->callback = callback;
	info->user_data = user_data;
	info->destroy = destroy;

	/* It's fine to copy this asynchronous: the journal replay code can or 
	 * should cope with unfinished entries at the end of the file, while
	 * restoring a backup made this way. */

	g_file_copy_async (info->journal, info->destination,
	                   G_FILE_COPY_OVERWRITE,
	                   G_PRIORITY_HIGH,
	                   NULL, NULL, NULL,
	                   on_journal_copied,
	                   info);
}

static gboolean
on_restore_done (gpointer user_data)
{
	BackupSaveInfo *info = user_data;

	if (info->callback) {
		info->callback (info->error, info->user_data);
	}

	free_backup_save_info (info);

	return FALSE;
}

void
tracker_data_backup_restore (GFile *journal,
                             TrackerDataBackupFinished callback,
                             gpointer user_data,
                             GDestroyNotify destroy,
                             const gchar **test_schemas,
                             TrackerBusyCallback busy_callback,
                             gpointer busy_user_data)
{
	BackupSaveInfo *info;

	info = g_new0 (BackupSaveInfo, 1);
	info->destination = g_file_new_for_path (tracker_db_journal_get_filename ());
	info->journal = g_object_ref (journal);
	info->callback = callback;
	info->user_data = user_data;
	info->destroy = destroy;

	if (g_file_query_exists (info->journal, NULL)) {
		TrackerDBManagerFlags flags = tracker_db_manager_get_flags ();
		gboolean is_first;
		gsize chunk_size = 0;
		gboolean do_rotating = FALSE;

		tracker_db_manager_move_to_temp ();
		tracker_data_manager_shutdown ();

		/* Synchronous: we don't want the mainloop to run while copying the
		 * journal, as nobody should be writing anything at this point */

		g_file_copy (info->journal, info->destination,
		             G_FILE_COPY_OVERWRITE,
		             NULL, NULL, NULL,
		             &info->error);

		tracker_db_manager_init_locations ();
		tracker_db_journal_get_rotating (&do_rotating, &chunk_size);
		tracker_db_journal_init (NULL, FALSE, do_rotating, chunk_size);

		if (info->error) {
			tracker_db_manager_restore_from_temp ();
		} else {
			tracker_db_manager_remove_temp ();
		}

		tracker_db_journal_shutdown ();

		tracker_data_manager_init (flags, do_rotating, chunk_size,
		                           test_schemas, &is_first, TRUE,
		                           busy_callback, busy_user_data,
		                           "Restoring backup");

	} else {
		g_set_error (&info->error, TRACKER_DATA_BACKUP_ERROR, 0, 
		             "Backup file doesn't exist");
	}

	on_restore_done (info);
}

