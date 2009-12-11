/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia
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
#include <libtracker-db/tracker-db-backup.h>

#include "tracker-data-backup.h"

typedef struct {
	GFile *destination, *journal, *file;
	TrackerDataBackupFinished callback;
	gpointer user_data;
	GDestroyNotify destroy;
	GError *error;
} BackupSaveInfo;

static void
free_backup_save_info (BackupSaveInfo *info)
{
	if (info->destination) {
		g_object_unref (info->destination);
	}
	if (info->journal) {
		g_object_unref (info->journal);
	}
	if (info->file) {
		g_object_unref (info->file);
	}

	if (info->destroy) {
		info->destroy (info->user_data);
	}

	g_clear_error (&info->error);

	g_free (info);
}


static void
on_meta_copied (GObject *source_object,
                GAsyncResult *res,
                gpointer user_data)
{
	BackupSaveInfo *info = user_data;
	GError *error = NULL;

	g_file_copy_finish (info->file, res, &error);

	if (info->callback) {
		info->callback (error, info->user_data);
	}

	free_backup_save_info (info);

	g_clear_error (&error);
}

static void
on_journal_copied (GObject *source_object,
                   GAsyncResult *res,
                   gpointer user_data)
{
	BackupSaveInfo *info = user_data;
	GError *error = NULL;

	if (!g_file_copy_finish (info->journal, res, &error)) {
		if (info->callback) {
			info->callback (error, info->user_data);
		}
		free_backup_save_info (info);
	} else {
		g_file_copy_async (info->file, info->destination,
		                   G_FILE_COPY_OVERWRITE,
		                   G_PRIORITY_HIGH,
		                   NULL, NULL, NULL,
		                   on_meta_copied,
		                   info);
	}

	g_clear_error (&error);
}

static void
save_copy_procedure (BackupSaveInfo *info)
{
	GFile *journal_o;

	journal_o = g_file_new_for_path (tracker_db_journal_filename ());

	if (g_file_query_exists (journal_o, NULL)) {
		g_file_copy_async (journal_o, info->journal,
		                   G_FILE_COPY_OVERWRITE,
		                   G_PRIORITY_HIGH,
		                   NULL, NULL, NULL,
		                   on_journal_copied,
		                   info);
	} else {
		g_file_copy_async (info->file, info->destination,
		                   G_FILE_COPY_OVERWRITE,
		                   G_PRIORITY_HIGH,
		                   NULL, NULL, NULL,
		                   on_meta_copied,
		                   info);
	}

	g_object_unref (journal_o);
}

static void
on_backup_finished (GError *error, gpointer user_data)
{
	BackupSaveInfo *info = user_data;

	if (error) {
		if (info->callback) {
			info->callback (error, info->user_data);
		}
		free_backup_save_info (info);
		return;
	}

	save_copy_procedure (info);
}

void
tracker_data_backup_save (GFile *destination,
                          GFile *journal,
                          TrackerDataBackupFinished callback,
                          gpointer user_data,
                          GDestroyNotify destroy)
{
	BackupSaveInfo *info;

	info = g_new0 (BackupSaveInfo, 1);
	info->destination = g_object_ref (destination);
	info->journal = g_object_ref (journal);
	info->callback = callback;
	info->user_data = user_data;
	info->destroy = destroy;

	info->file = tracker_db_backup_file (NULL, TRACKER_DB_BACKUP_META_FILENAME);

	if (g_file_query_exists (info->file, NULL)) {
		/* Making a backup just means copying meta-backup.db */
		save_copy_procedure (info);
	} else {
		/* If we don't have a meta-backup.db yet, we first make one */
		tracker_db_backup_save (on_backup_finished,
		                        info, NULL);
	}
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

static void
restore_copy_procedure (BackupSaveInfo *info)
{
	GFile *journal_d;
	GError *error = NULL;

	/* Restore should block the mainloop until finished */

	journal_d = g_file_new_for_path (tracker_db_journal_filename ());

	if (g_file_query_exists (journal_d, NULL)) {
		g_file_copy (info->journal, journal_d,
		             G_FILE_COPY_OVERWRITE,
		             NULL, NULL, NULL,
		             &error);
	}

	g_object_unref (journal_d);

	if (error) {
		goto error_handle;
	}

	g_file_copy (info->destination, info->file,
	             G_FILE_COPY_OVERWRITE,
	             NULL, NULL, NULL,
	             &error);

	if (error) {
		goto error_handle;
	}

 error_handle:

	info->error = error;
}

void
tracker_data_backup_restore (GFile *backup,
                             GFile *journal,
                             TrackerDataBackupFinished callback,
                             gpointer user_data,
                             GDestroyNotify destroy)
{
	BackupSaveInfo *info;

	tracker_db_manager_disconnect ();
	tracker_db_journal_close ();

	info = g_new0 (BackupSaveInfo, 1);
	info->destination = g_object_ref (backup);
	info->journal = g_object_ref (journal);
	info->callback = callback;
	info->user_data = user_data;
	info->destroy = destroy;

	info->file = tracker_db_backup_file (NULL, TRACKER_DB_BACKUP_META_FILENAME);

	/* This is all synchronous, blocking the mainloop indeed */

	restore_copy_procedure (info);

	tracker_db_journal_open ();
	tracker_db_manager_reconnect ();

	tracker_db_backup_sync_fts ();

	g_idle_add (on_restore_done, info);
}

