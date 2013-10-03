/*
 * Copyright (C) 2009, Nokia
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
 *
 * Author: Philip Van Hoof <philip@codeminded.be>
 */

#include "config.h"

#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>

#include <sqlite3.h>

#include <libtracker-data/tracker-ontology.h>
#include <libtracker-data/tracker-ontologies.h>
#include <libtracker-data/tracker-db-manager.h>
#include <libtracker-data/tracker-db-interface-sqlite.h>

#include "tracker-db-backup.h"

#define TRACKER_DB_BACKUP_META_FILENAME_T	"meta-backup.db.tmp"

typedef struct {
	GFile *destination;
	TrackerDBBackupFinished callback;
	gpointer user_data;
	GDestroyNotify destroy;
	GError *error;
} BackupInfo;

GQuark
tracker_db_backup_error_quark (void)
{
	return g_quark_from_static_string ("tracker-db-backup-error-quark");
}

static gboolean
perform_callback (gpointer user_data)
{
	BackupInfo *info = user_data;

	if (info->callback) {
		info->callback (info->error, info->user_data);
	}

	return FALSE;
}

static void
backup_info_free (gpointer user_data)
{
	BackupInfo *info = user_data;

	if (info->destination) {
		g_object_unref (info->destination);
	}

	if (info->destroy) {
		info->destroy (info->user_data);
	}

	g_clear_error (&info->error);

	g_slice_free (BackupInfo, info);
}

static void
backup_job (GTask        *task,
            gpointer      source_object,
            gpointer      task_data,
            GCancellable *cancellable)
{
	BackupInfo *info = task_data;

	const gchar *src_path;
	GFile *parent_file, *temp_file;
	gchar *temp_path;

	sqlite3 *src_db = NULL;
	sqlite3 *temp_db = NULL;
	sqlite3_backup *backup = NULL;

	src_path = tracker_db_manager_get_file (TRACKER_DB_METADATA);
	parent_file = g_file_get_parent (info->destination);
	temp_file = g_file_get_child (parent_file, TRACKER_DB_BACKUP_META_FILENAME_T);
	g_file_delete (temp_file, NULL, NULL);
	temp_path = g_file_get_path (temp_file);

	if (sqlite3_open_v2 (src_path, &src_db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
		g_set_error (&info->error, TRACKER_DB_BACKUP_ERROR, TRACKER_DB_BACKUP_ERROR_UNKNOWN,
		             "Could not open sqlite3 database:'%s'", src_path);
	}

	if (!info->error && sqlite3_open (temp_path, &temp_db) != SQLITE_OK) {
		g_set_error (&info->error, TRACKER_DB_BACKUP_ERROR, TRACKER_DB_BACKUP_ERROR_UNKNOWN,
		             "Could not open sqlite3 database:'%s'", temp_path);
	}

	if (!info->error) {
		backup = sqlite3_backup_init (temp_db, "main", src_db, "main");

		if (!backup) {
			g_set_error (&info->error, TRACKER_DB_BACKUP_ERROR, TRACKER_DB_BACKUP_ERROR_UNKNOWN,
				     "Unable to initialize sqlite3 backup from '%s' to '%s'", src_path, temp_path);
		}
	}

	if (!info->error && sqlite3_backup_step (backup, -1) != SQLITE_DONE) {
		g_set_error (&info->error, TRACKER_DB_BACKUP_ERROR, TRACKER_DB_BACKUP_ERROR_UNKNOWN,
		             "Unable to complete sqlite3 backup");
	}

	if (backup) {
		if (sqlite3_backup_finish (backup) != SQLITE_OK) {
			if (info->error) {
				/* sqlite3_backup_finish can provide more detailed error message */
				g_clear_error (&info->error);
			}
			g_set_error (&info->error,
			             TRACKER_DB_BACKUP_ERROR,
			             TRACKER_DB_BACKUP_ERROR_UNKNOWN,
				     "Unable to finish sqlite3 backup: %s",
				     sqlite3_errmsg (temp_db));
		}
		backup = NULL;
	}

	if (temp_db) {
		sqlite3_close (temp_db);
		temp_db = NULL;
	}

	if (src_db) {
		sqlite3_close (src_db);
		src_db = NULL;
	}

	if (!info->error) {
		g_file_move (temp_file, info->destination,
		             G_FILE_COPY_OVERWRITE,
		             NULL, NULL, NULL,
		             &info->error);
	}

	g_free (temp_path);
	g_object_unref (temp_file);
	g_object_unref (parent_file);

	g_idle_add_full (G_PRIORITY_DEFAULT, perform_callback, info,
	                 backup_info_free);
}

void
tracker_db_backup_save (GFile                   *destination,
                        TrackerDBBackupFinished  callback,
                        gpointer                 user_data,
                        GDestroyNotify           destroy)
{
	GTask *task;
	BackupInfo *info;

	info = g_slice_new0 (BackupInfo);

	info->destination = g_object_ref (destination);

	info->callback = callback;
	info->user_data = user_data;
	info->destroy = destroy;

	task = g_task_new (NULL, NULL, NULL, NULL);

	g_task_set_task_data (task, info, NULL);
	g_task_run_in_thread (task, backup_job);
	g_object_unref (task);
}

