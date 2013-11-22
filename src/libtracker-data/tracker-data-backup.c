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

#include <errno.h>
#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <libtracker-common/tracker-os-dependant.h>

#include "tracker-data-backup.h"
#include "tracker-data-manager.h"
#include "tracker-db-manager.h"
#include "tracker-db-journal.h"
#include "tracker-db-backup.h"

typedef struct {
	GFile *destination, *journal;
	TrackerDataBackupFinished callback;
	gpointer user_data;
	GDestroyNotify destroy;
	GError *error;
} BackupSaveInfo;

#ifndef DISABLE_JOURNAL

typedef struct {
	GPid pid;
	guint stdout_watch_id;
	guint stderr_watch_id;
	GIOChannel *stdin_channel;
	GIOChannel *stdout_channel;
	GIOChannel *stderr_channel;
	gpointer data;
	GString *lines;
} ProcessContext;

#endif /* DISABLE_JOURNAL */

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


GQuark
tracker_data_backup_error_quark (void)
{
	return g_quark_from_static_string (TRACKER_DATA_BACKUP_ERROR_DOMAIN);
}

#ifndef DISABLE_JOURNAL

static void
on_journal_copied (BackupSaveInfo *info, GError *error)
{
	if (info->callback) {
		info->callback (error, info->user_data);
	}

	free_backup_save_info (info);
}



static void
process_context_destroy (ProcessContext *context, GError *error)
{
	on_journal_copied (context->data, error);

	if (context->lines) {
		g_string_free (context->lines, TRUE);
	}

	if (context->stdin_channel) {
		g_io_channel_shutdown (context->stdin_channel, FALSE, NULL);
		g_io_channel_unref (context->stdin_channel);
		context->stdin_channel = NULL;
	}

	if (context->stdout_watch_id != 0) {
		g_source_remove (context->stdout_watch_id);
		context->stdout_watch_id = 0;
	}

	if (context->stdout_channel) {
		g_io_channel_shutdown (context->stdout_channel, FALSE, NULL);
		g_io_channel_unref (context->stdout_channel);
		context->stdout_channel = NULL;
	}

	if (context->stderr_watch_id != 0) {
		g_source_remove (context->stderr_watch_id);
		context->stderr_watch_id = 0;
	}

	if (context->stderr_channel) {
		g_io_channel_shutdown (context->stderr_channel, FALSE, NULL);
		g_io_channel_unref (context->stderr_channel);
		context->stderr_channel = NULL;
	}

	if (context->pid != 0) {
		g_spawn_close_pid (context->pid);
		context->pid = 0;
	}

	g_free (context);
}

static gboolean
read_line_of_tar_output (GIOChannel  *channel,
                         GIOCondition condition,
                         gpointer     user_data)
{

	if (condition & G_IO_ERR || condition & G_IO_HUP) {
		ProcessContext *context = user_data;

		context->stdout_watch_id = 0;
		return FALSE;
	}

	/* TODO: progress support */
	return TRUE;
}

static gboolean
read_error_of_tar_output (GIOChannel  *channel,
                          GIOCondition condition,
                          gpointer     user_data)
{
	ProcessContext *context;
	GIOStatus status;
	gchar *line;

	context = user_data;
	status = G_IO_STATUS_NORMAL;

	if (condition & G_IO_IN || condition & G_IO_PRI) {
		do {
			GError *error = NULL;

			status = g_io_channel_read_line (channel, &line, NULL, NULL, &error);

			if (status == G_IO_STATUS_NORMAL) {
				if (context->lines == NULL)
					context->lines = g_string_new (NULL);
				g_string_append (context->lines, line);
				g_free (line);
			} else if (error) {
				g_warning ("%s", error->message);
				g_error_free (error);
			}
		} while (status == G_IO_STATUS_NORMAL);

		if (status == G_IO_STATUS_EOF ||
		    status == G_IO_STATUS_ERROR) {
			context->stderr_watch_id = 0;
			return FALSE;
		}
	}

	if (condition & G_IO_ERR || condition & G_IO_HUP) {
		context->stderr_watch_id = 0;
		return FALSE;
	}

	return TRUE;
}

static void
process_context_child_watch_cb (GPid     pid,
                                gint     status,
                                gpointer user_data)
{
	ProcessContext *context;
	GError *error = NULL;

	g_debug ("Process '%d' exited with code %d", pid, status);

	context = (ProcessContext *) user_data;

	if (context->lines) {
		g_set_error (&error, TRACKER_DATA_BACKUP_ERROR,
		             TRACKER_DATA_BACKUP_ERROR_UNKNOWN,
		             "%s", context->lines->str);
	}

	process_context_destroy (context, error);
}
#endif /* DISABLE_JOURNAL */



#ifdef DISABLE_JOURNAL
static void
on_backup_finished (GError *error,
                    gpointer user_data)
{
	BackupSaveInfo *info = user_data;

	if (info->callback) {
		info->callback (error, info->user_data);
	}

	free_backup_save_info (info);
}

#endif /* DISABLE_JOURNAL */

/* delete all regular files from the directory */
static void
dir_remove_files (const gchar *path)
{
	GDir *dir;
	const gchar *name;

	dir = g_dir_open (path, 0, NULL);
	if (dir == NULL) {
		return;
	}

	while ((name = g_dir_read_name (dir)) != NULL) {
		gchar *filename;

		filename = g_build_filename (path, name, NULL);

		if (g_file_test (filename, G_FILE_TEST_IS_REGULAR)) {
			g_debug ("Removing '%s'", filename);
			if (g_unlink (filename) == -1) {
				g_warning ("Unable to remove '%s': %s", filename, g_strerror (errno));
			}
		}

		g_free (filename);
	}

	g_dir_close (dir);
}

/* move all regular files from the source directory to the destination directory */
static void
dir_move_files (const gchar *src_path, const gchar *dest_path)
{
	GDir *src_dir;
	const gchar *src_name;

	src_dir = g_dir_open (src_path, 0, NULL);
	if (src_dir == NULL) {
		return;
	}

	while ((src_name = g_dir_read_name (src_dir)) != NULL) {
		gchar *src_filename, *dest_filename;

		src_filename = g_build_filename (src_path, src_name, NULL);

		if (g_file_test (src_filename, G_FILE_TEST_IS_REGULAR)) {
			dest_filename = g_build_filename (dest_path, src_name, NULL);

			g_debug ("Renaming '%s' to '%s'", src_filename, dest_filename);
			if (g_rename (src_filename, dest_filename) == -1) {
				g_warning ("Unable to rename '%s' to '%s': %s", src_filename, dest_filename, g_strerror (errno));
			}

			g_free (dest_filename);
		}

		g_free (src_filename);
	}

	g_dir_close (src_dir);
}

static void
dir_move_to_temp (const gchar *path)
{
	gchar *temp_dir;

	temp_dir = g_build_filename (path, "tmp", NULL);
	g_mkdir (temp_dir, 0777);

	/* ensure that no obsolete temporary files are around */
	dir_remove_files (temp_dir);
	dir_move_files (path, temp_dir);

	g_free (temp_dir);
}

static void
dir_move_from_temp (const gchar *path)
{
	gchar *temp_dir;

	temp_dir = g_build_filename (path, "tmp", NULL);

	/* ensure that no obsolete files are around */
	dir_remove_files (path);
	dir_move_files (temp_dir, path);

	g_rmdir (temp_dir);

	g_free (temp_dir);
}

static void
move_to_temp (void)
{
	gchar *data_dir, *cache_dir;

	g_message ("Moving all database files to temporary location");

	data_dir = g_build_filename (g_get_user_data_dir (),
	                             "tracker",
	                             "data",
	                             NULL);

	cache_dir = g_build_filename (g_get_user_cache_dir (),
	                              "tracker",
	                              NULL);

	dir_move_to_temp (data_dir);
	dir_move_to_temp (cache_dir);

	g_free (cache_dir);
	g_free (data_dir);
}

static void
remove_temp (void)
{
	gchar *tmp_data_dir, *tmp_cache_dir;

	g_message ("Removing all database files from temporary location");

	tmp_data_dir = g_build_filename (g_get_user_data_dir (),
	                                 "tracker",
	                                 "data",
	                                 "tmp",
	                                 NULL);

	tmp_cache_dir = g_build_filename (g_get_user_cache_dir (),
	                                  "tracker",
	                                  "tmp",
	                                  NULL);

	dir_remove_files (tmp_data_dir);
	dir_remove_files (tmp_cache_dir);

	g_rmdir (tmp_data_dir);
	g_rmdir (tmp_cache_dir);

	g_free (tmp_cache_dir);
	g_free (tmp_data_dir);
}

static void
restore_from_temp (void)
{
	gchar *data_dir, *cache_dir;

	g_message ("Restoring all database files from temporary location");

	data_dir = g_build_filename (g_get_user_data_dir (),
	                             "tracker",
	                             "data",
	                             NULL);

	cache_dir = g_build_filename (g_get_user_cache_dir (),
	                              "tracker",
	                              NULL);

	dir_move_from_temp (data_dir);
	dir_move_from_temp (cache_dir);

	g_free (cache_dir);
	g_free (data_dir);
}

void
tracker_data_backup_save (GFile *destination,
                          TrackerDataBackupFinished callback,
                          gpointer user_data,
                          GDestroyNotify destroy)
{
#ifndef DISABLE_JOURNAL
	BackupSaveInfo *info;
	ProcessContext *context;
	gchar **argv;
	gchar *path, *directory;
	GDir *journal_dir;
	GFile *parent;
	GIOChannel *stdin_channel, *stdout_channel, *stderr_channel;
	GPid pid;
	GPtrArray *files;
	const gchar *f_name;
	guint i;

	info = g_new0 (BackupSaveInfo, 1);
	info->destination = g_object_ref (destination);
	info->journal = g_file_new_for_path (tracker_db_journal_get_filename ());
	info->callback = callback;
	info->user_data = user_data;
	info->destroy = destroy;

	parent = g_file_get_parent (info->journal);
	directory = g_file_get_path (parent);
	g_object_unref (parent);
	path = g_file_get_path (destination);

	journal_dir = g_dir_open (directory, 0, NULL);
	f_name = g_dir_read_name (journal_dir);
	files = g_ptr_array_new ();

	while (f_name) {
		if (f_name) {
			if (!g_str_has_prefix (f_name, TRACKER_DB_JOURNAL_FILENAME ".")) {
				f_name = g_dir_read_name (journal_dir);
				continue;
			}
			g_ptr_array_add (files, g_strdup (f_name));
		}
		f_name = g_dir_read_name (journal_dir);
	}

	g_dir_close (journal_dir);

	argv = g_new0 (gchar*, files->len + 8);

	argv[0] = g_strdup ("tar");
	argv[1] = g_strdup ("-zcf");
	argv[2] = path;
	argv[3] = g_strdup ("-C");
	argv[4] = directory;
	argv[5] = g_strdup (TRACKER_DB_JOURNAL_FILENAME);
	argv[6] = g_strdup (TRACKER_DB_JOURNAL_ONTOLOGY_FILENAME);

	for (i = 0; i < files->len; i++) {
		argv[i+7] = g_ptr_array_index (files, i);
	}

	/* It's fine to untar this asynchronous: the journal replay code can or
	 * should cope with unfinished entries at the end of the file, while
	 * restoring a backup made this way. */

	if (!tracker_spawn_async_with_channels ((const gchar **) argv,
	                                        0, &pid,
	                                        &stdin_channel,
	                                        &stdout_channel,
	                                        &stderr_channel)) {
		GError *error = NULL;
		g_set_error (&error, TRACKER_DATA_BACKUP_ERROR,
		             TRACKER_DATA_BACKUP_ERROR_UNKNOWN,
		             "Error starting tar program");
		on_journal_copied (info, error);
		g_strfreev (argv);
		g_error_free (error);
		return;
	}

	context = g_new0 (ProcessContext, 1);
	context->lines = NULL;
	context->data = info;
	context->pid = pid;
	context->stdin_channel = stdin_channel;
	context->stdout_channel = stdout_channel;
	context->stderr_channel = stderr_channel;
	context->stdout_watch_id = g_io_add_watch (stdout_channel,
	                                           G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP,
	                                           read_line_of_tar_output,
	                                           context);
	context->stderr_watch_id = g_io_add_watch (stderr_channel,
	                                           G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP,
	                                           read_error_of_tar_output,
	                                           context);

	g_child_watch_add (context->pid, process_context_child_watch_cb, context);

	g_debug ("Process '%d' spawned for command:'%s %s %s'",
	         pid, argv[0], argv[1], argv[2]);

	g_strfreev (argv);
#else
	BackupSaveInfo *info;

	info = g_new0 (BackupSaveInfo, 1);
	info->destination = g_object_ref (destination);
	info->callback = callback;
	info->user_data = user_data;
	info->destroy = destroy;

	tracker_db_backup_save (destination,
	                        on_backup_finished, 
	                        info,
	                        NULL);
#endif /* DISABLE_JOURNAL */
}

void
tracker_data_backup_restore (GFile                *journal,
                             const gchar         **test_schemas,
                             TrackerBusyCallback   busy_callback,
                             gpointer              busy_user_data,
                             GError              **error)
{
	BackupSaveInfo *info;
	GError *internal_error = NULL;

	info = g_new0 (BackupSaveInfo, 1);
#ifndef DISABLE_JOURNAL
	info->destination = g_file_new_for_path (tracker_db_journal_get_filename ());
#else
	info->destination = g_file_new_for_path (tracker_db_manager_get_file (TRACKER_DB_METADATA));
#endif /* DISABLE_JOURNAL */

	info->journal = g_object_ref (journal);

	if (g_file_query_exists (info->journal, NULL)) {
		TrackerDBManagerFlags flags;
		guint select_cache_size, update_cache_size;
		gboolean is_first;
#ifndef DISABLE_JOURNAL
		GError *n_error = NULL;
		GFile *parent = g_file_get_parent (info->destination);
		gchar *tmp_stdout = NULL;
		gchar *tmp_stderr = NULL;
		gchar **argv;
		gint exit_status;
#endif /* DISABLE_JOURNAL */

		flags = tracker_db_manager_get_flags (&select_cache_size, &update_cache_size);

		tracker_data_manager_shutdown ();

		move_to_temp ();

#ifndef DISABLE_JOURNAL
		argv = g_new0 (char*, 6);

		argv[0] = g_strdup ("tar");
		argv[1] = g_strdup ("-zxf");
		argv[2] = g_file_get_path (info->journal);
		argv[3] = g_strdup ("-C");
		argv[4] = g_file_get_path (parent);

		g_object_unref (parent);

		/* Synchronous: we don't want the mainloop to run while copying the
		 * journal, as nobody should be writing anything at this point */

		if (!tracker_spawn (argv, 0, &tmp_stdout, &tmp_stderr, &exit_status)) {
			g_set_error (&info->error, TRACKER_DATA_BACKUP_ERROR,
			             TRACKER_DATA_BACKUP_ERROR_UNKNOWN,
			             "Error starting tar program");
		}

		if (!info->error && tmp_stderr) {
			if (strlen (tmp_stderr) > 0) {
				g_set_error (&info->error, TRACKER_DATA_BACKUP_ERROR,
				             TRACKER_DATA_BACKUP_ERROR_UNKNOWN,
				             "%s", tmp_stderr);
			}
		}

		if (!info->error && exit_status != 0) {
			g_set_error (&info->error, TRACKER_DATA_BACKUP_ERROR,
			             TRACKER_DATA_BACKUP_ERROR_UNKNOWN,
			             "Unknown error, tar exited with exit status %d", exit_status);
		}

		g_free (tmp_stderr);
		g_free (tmp_stdout);
		g_strfreev (argv);
#else
		/* Turn off force-reindex here, no journal to replay so it wouldn't work */
		flags &= ~TRACKER_DB_MANAGER_FORCE_REINDEX;

		g_file_copy (info->journal, info->destination,
		             G_FILE_COPY_OVERWRITE, 
		             NULL, NULL, NULL,
		             &info->error);
#endif /* DISABLE_JOURNAL */

		tracker_db_manager_init_locations ();

		/* Re-set the DB version file, so that its mtime changes. The mtime of this
		 * file will change only when the whole DB is recreated (after a hard reset
		 * or after a backup restoration). */
		tracker_db_manager_create_version_file ();

		/* Given we're radically changing the database, we
		 * force a full mtime check against all known files in
		 * the database for complete synchronisation. */
		tracker_db_manager_set_need_mtime_check (TRUE);

#ifndef DISABLE_JOURNAL
		tracker_db_journal_init (NULL, FALSE, &n_error);

		if (n_error) {
			if (!info->error) {
				g_propagate_error (&info->error, n_error);
			} else {
				g_warning ("Ignored error while initializing journal during backup (another higher priority error already took place): %s",
				           n_error->message ? n_error->message : "No error given");
				g_error_free (n_error);
			}
			n_error = NULL;
		}

		if (info->error) {
			restore_from_temp ();
		} else {
			remove_temp ();
		}

		tracker_db_journal_shutdown (&n_error);

		if (n_error) {
			g_warning ("Ignored error while shuting down journal during backup: %s",
			           n_error->message ? n_error->message : "No error given");
			g_error_free (n_error);
		}
#endif /* DISABLE_JOURNAL */

		tracker_data_manager_init (flags, test_schemas, &is_first, TRUE, TRUE,
		                           select_cache_size, update_cache_size,
		                           busy_callback, busy_user_data,
		                           "Restoring backup", &internal_error);

#ifdef DISABLE_JOURNAL
		if (internal_error) {
			restore_from_temp ();

			tracker_data_manager_init (flags, test_schemas, &is_first, TRUE, TRUE,
			                           select_cache_size, update_cache_size,
			                           busy_callback, busy_user_data,
			                           "Restoring backup", &internal_error);
		} else {
			remove_temp ();
		}
#endif /* DISABLE_JOURNAL */

		if (internal_error) {
			g_propagate_error (error, internal_error);
		}
	} else {
		g_set_error (&info->error, TRACKER_DATA_BACKUP_ERROR,
		             TRACKER_DATA_BACKUP_ERROR_UNKNOWN,
		             "Backup file doesn't exist");
	}

	if (info->error) {
		g_propagate_error (error, info->error);
		info->error = NULL;
	}

	free_backup_save_info (info);
}

