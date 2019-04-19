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
#include <glib/gi18n.h>

#include "tracker-data-backup.h"
#include "tracker-data-manager.h"
#include "tracker-db-manager.h"
#include "tracker-db-journal.h"
#include "tracker-db-backup.h"

typedef struct {
	GFile   *destination, *journal;
	TrackerDataBackupFinished callback;
	gpointer user_data;
	GDestroyNotify destroy;
	GError  *error;
} BackupSaveInfo;

#ifndef DISABLE_JOURNAL

typedef struct {
	GPid  pid;
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
on_backup_finished (GError  *error,
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
dir_move_to_temp (const gchar *path,
                  const gchar *tmpname)
{
	gchar *temp_dir;

	temp_dir = g_build_filename (path, tmpname, NULL);
	if (g_mkdir (temp_dir, 0777) < 0) {
		g_critical ("Could not move %s to temp directory: %m",
		            path);
		g_free (temp_dir);
		return;
	}

	/* ensure that no obsolete temporary files are around */
	dir_remove_files (temp_dir);
	dir_move_files (path, temp_dir);

	g_free (temp_dir);
}

static void
dir_move_from_temp (const gchar *path,
                    const gchar *tmpname)
{
	gchar *temp_dir;

	temp_dir = g_build_filename (path, tmpname, NULL);

	/* ensure that no obsolete files are around */
	dir_remove_files (path);
	dir_move_files (temp_dir, path);

	g_rmdir (temp_dir);

	g_free (temp_dir);
}

static void
move_to_temp (GFile *cache_location,
              GFile *data_location)
{
	gchar *data_dir, *cache_dir;

	g_info ("Moving all database files to temporary location");

	data_dir = g_file_get_path (data_location);
	cache_dir = g_file_get_path (cache_location);

	dir_move_to_temp (data_dir, "tmp.data");
	dir_move_to_temp (cache_dir, "tmp.cache");

	g_free (cache_dir);
	g_free (data_dir);
}

static void
remove_temp (GFile *cache_location,
             GFile *data_location)
{
	gchar *tmp_data_dir, *tmp_cache_dir;
	GFile *child;

	g_info ("Removing all database files from temporary location");

	child = g_file_get_child (data_location, "tmp.data");
	tmp_data_dir = g_file_get_path (child);
	g_object_unref (child);

	child = g_file_get_child (cache_location, "tmp.cache");
	tmp_cache_dir = g_file_get_path (child);
	g_object_unref (child);

	dir_remove_files (tmp_data_dir);
	dir_remove_files (tmp_cache_dir);

	g_rmdir (tmp_data_dir);
	g_rmdir (tmp_cache_dir);

	g_free (tmp_cache_dir);
	g_free (tmp_data_dir);
}

static void
restore_from_temp (GFile *cache_location,
                   GFile *data_location)
{
	gchar *data_dir, *cache_dir;

	g_info ("Restoring all database files from temporary location");

	data_dir = g_file_get_path (data_location);
	cache_dir = g_file_get_path (cache_location);

	dir_move_from_temp (data_dir, "tmp.data");
	dir_move_from_temp (cache_dir, "tmp.cache");

	g_free (cache_dir);
	g_free (data_dir);
}

void
tracker_data_backup_save (TrackerDataManager        *data_manager,
                          GFile                     *destination,
                          GFile                     *data_location,
                          TrackerDataBackupFinished  callback,
                          gpointer                   user_data,
                          GDestroyNotify             destroy)
{
#ifndef DISABLE_JOURNAL
	BackupSaveInfo *info;
	ProcessContext *context;
	gchar **argv;
	gchar  *path, *directory;
	GError *local_error = NULL;
	GDir *journal_dir;
	GPid  pid;
	GPtrArray *files;
	const gchar *f_name;
	gboolean result;
	gint  stdin_fd, stdout_fd, stderr_fd;
	guint i;

	info = g_new0 (BackupSaveInfo, 1);
	info->destination = g_object_ref (destination);
	info->callback = callback;
	info->user_data = user_data;
	info->destroy = destroy;

	path = g_file_get_path (destination);

	directory = g_file_get_path (data_location);
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
	result = g_spawn_async_with_pipes (NULL, /* working dir */
	                                   (gchar **) argv,
	                                   NULL, /* env */
	                                   G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
	                                   NULL, /* func to call before exec() */
	                                   0,
	                                   &pid,
	                                   &stdin_fd,
	                                   &stdout_fd,
	                                   &stderr_fd,
	                                   &local_error);

	if (!result || local_error) {
		GError *error = NULL;

		g_set_error (&error,
		             TRACKER_DATA_BACKUP_ERROR,
		             TRACKER_DATA_BACKUP_ERROR_UNKNOWN,
		             "%s, %s",
		             _("Error starting “tar” program"),
		             local_error ? local_error->message : _("No error given"));

		g_warning ("%s", error->message);

		on_journal_copied (info, error);

		g_strfreev (argv);
		g_clear_error (&local_error);

		return;
	}

	context = g_new0 (ProcessContext, 1);
	context->lines = NULL;
	context->data = info;
	context->pid = pid;
	context->stdin_channel = g_io_channel_unix_new (stdin_fd);
	context->stdout_channel = g_io_channel_unix_new (stdout_fd);
	context->stderr_channel = g_io_channel_unix_new (stderr_fd);
	context->stdout_watch_id = g_io_add_watch (context->stdout_channel,
	                                           G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP,
	                                           read_line_of_tar_output,
	                                           context);
	context->stderr_watch_id = g_io_add_watch (context->stderr_channel,
	                                           G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP,
	                                           read_error_of_tar_output,
	                                           context);

	g_child_watch_add (context->pid, process_context_child_watch_cb, context);

	g_debug ("Process '%d' spawned for command:'%s %s %s'",
	         pid, argv[0], argv[1], argv[2]);

	g_strfreev (argv);
#else
	BackupSaveInfo *info;
	TrackerDBManager *db_manager;
	GFile *db_file;

	info = g_new0 (BackupSaveInfo, 1);
	info->destination = g_object_ref (destination);
	info->callback = callback;
	info->user_data = user_data;
	info->destroy = destroy;

	db_manager = tracker_data_manager_get_db_manager (data_manager);
	db_file = g_file_new_for_path (tracker_db_manager_get_file (db_manager));

	tracker_db_backup_save (destination, db_file,
	                        on_backup_finished,
	                        info,
	                        NULL);

	g_object_unref (db_file);
#endif /* DISABLE_JOURNAL */
}

void
tracker_data_backup_restore (TrackerDataManager   *manager,
                             GFile                *journal,
                             GFile                *cache_location,
                             GFile                *data_location,
                             GFile                *ontology_location,
                             TrackerBusyCallback   busy_callback,
                             gpointer              busy_user_data,
                             GError              **error)
{
	BackupSaveInfo *info;
	GError *internal_error = NULL;
	TrackerDBManager *db_manager = NULL;

	if (!cache_location || !data_location || !ontology_location) {
		g_set_error (error,
		             TRACKER_DATA_ONTOLOGY_ERROR,
		             TRACKER_DATA_UNSUPPORTED_LOCATION,
		             "All data storage and ontology locations must be provided");
		return;
	}

	db_manager = tracker_data_manager_get_db_manager (manager);
	info = g_new0 (BackupSaveInfo, 1);
#ifndef DISABLE_JOURNAL
	info->destination = g_file_get_child (data_location, TRACKER_DB_JOURNAL_FILENAME);
#else
	info->destination = g_file_new_for_path (tracker_db_manager_get_file (db_manager));
#endif /* DISABLE_JOURNAL */

	info->journal = g_object_ref (journal);

	if (g_file_query_exists (info->journal, NULL)) {
		TrackerDBManagerFlags flags;
		TrackerDBJournal *journal_writer;
		guint select_cache_size, update_cache_size;
#ifndef DISABLE_JOURNAL
		GError  *n_error = NULL;
		GFile   *parent = g_file_get_parent (info->destination);
		gchar   *tmp_stdout = NULL;
		gchar   *tmp_stderr = NULL;
		gchar  **argv;
		gboolean result;
		gint exit_status;
#endif /* DISABLE_JOURNAL */

		flags = tracker_db_manager_get_flags (db_manager, &select_cache_size, &update_cache_size);

		//tracker_data_manager_shutdown ();

		move_to_temp (cache_location, data_location);

#ifndef DISABLE_JOURNAL
		argv = g_new0 (char*, 6);

		argv[0] = g_strdup ("tar");
		argv[1] = g_strdup ("-zxf");
		argv[2] = g_file_get_path (info->journal);
		argv[3] = g_strdup ("-C");
		argv[4] = g_file_get_path (parent);

		g_object_unref (parent);

		/* Synchronous: we don't want the mainloop to run while copying the
		 * journal, as nobody should be writing anything at this point
		 */
		result = g_spawn_sync (NULL, /* working dir */
		                       argv,
		                       NULL, /* env */
		                       G_SPAWN_SEARCH_PATH,
		                       NULL,
		                       0,    /* timeout */
		                       &tmp_stdout,
		                       &tmp_stderr,
		                       &exit_status,
		                       &n_error);

		if (!result || n_error) {
			g_set_error (&info->error,
			             TRACKER_DATA_BACKUP_ERROR,
			             TRACKER_DATA_BACKUP_ERROR_UNKNOWN,
			             "%s, %s",
			             _("Error starting “tar” program"),
			             n_error ? n_error->message : _("No error given"));
			g_warning ("%s", info->error->message);
			g_clear_error (&n_error);
		} else if (tmp_stderr && strlen (tmp_stderr) > 0) {
			g_set_error (&info->error,
			             TRACKER_DATA_BACKUP_ERROR,
			             TRACKER_DATA_BACKUP_ERROR_UNKNOWN,
			             "%s",
			             tmp_stderr);
		} else if (exit_status != 0) {
			g_set_error (&info->error,
			             TRACKER_DATA_BACKUP_ERROR,
			             TRACKER_DATA_BACKUP_ERROR_UNKNOWN,
			             _("Unknown error, “tar” exited with status %d"),
			             exit_status);
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

		tracker_db_manager_ensure_locations (db_manager, cache_location, data_location);

		/* Re-set the DB version file, so that its mtime changes. The mtime of this
		 * file will change only when the whole DB is recreated (after a hard reset
		 * or after a backup restoration). */
		tracker_db_manager_create_version_file (db_manager);

#ifndef DISABLE_JOURNAL
		journal_writer = tracker_db_journal_new (data_location, FALSE, &n_error);

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
			restore_from_temp (cache_location, data_location);
		} else {
			remove_temp (cache_location, data_location);
		}

		tracker_db_journal_free (journal_writer, &n_error);

		if (n_error) {
			g_warning ("Ignored error while shuting down journal during backup: %s",
			           n_error->message ? n_error->message : "No error given");
			g_error_free (n_error);
		}
#endif /* DISABLE_JOURNAL */

		manager = tracker_data_manager_new (flags, cache_location, data_location, ontology_location,
		                                    TRUE, TRUE, select_cache_size, update_cache_size);
		g_initable_init (G_INITABLE (manager), NULL, &internal_error);

#ifdef DISABLE_JOURNAL
		if (internal_error) {
			restore_from_temp (cache_location, data_location);
			g_object_unref (manager);

			manager = tracker_data_manager_new (flags, cache_location, data_location, ontology_location,
			                                    TRUE, TRUE, select_cache_size, update_cache_size);
			g_initable_init (G_INITABLE (manager), NULL, &internal_error);
		} else {
			remove_temp (cache_location, data_location);
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

