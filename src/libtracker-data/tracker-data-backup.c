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

#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <libtracker-common/tracker-os-dependant.h>

#include "tracker-data-backup.h"
#include "tracker-data-manager.h"
#include "tracker-db-manager.h"
#include "tracker-db-journal.h"

typedef struct {
	GFile *destination, *journal;
	TrackerDataBackupFinished callback;
	gpointer user_data;
	GDestroyNotify destroy;
	GError *error;
} BackupSaveInfo;

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
	ProcessContext *context;

	context = user_data;

	if (condition & G_IO_ERR || condition & G_IO_HUP) {
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
			return FALSE;
		}
	}

	if (condition & G_IO_ERR || condition & G_IO_HUP) {
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
		g_set_error (&error, TRACKER_DATA_BACKUP_ERROR, 0,
		             "%s", context->lines->str);
	}

	process_context_destroy (context, error);
}

void
tracker_data_backup_save (GFile *destination,
                          TrackerDataBackupFinished callback,
                          gpointer user_data,
                          GDestroyNotify destroy)
{
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

	argv = g_new0 (gchar*, files->len + 7);

	argv[0] = g_strdup ("tar");
	argv[1] = g_strdup ("-zcf");
	argv[2] = path;
	argv[3] = g_strdup ("-C");
	argv[4] = directory;
	argv[5] = g_strdup (TRACKER_DB_JOURNAL_FILENAME);

	for (i = 0; i < files->len; i++) {
		argv[i+6] = g_ptr_array_index (files, i);
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
		g_set_error (&error, TRACKER_DATA_BACKUP_ERROR, 0,
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
}

static gboolean
on_restore_done (BackupSaveInfo *info)
{
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
		GFile *parent = g_file_get_parent (info->destination);
		gchar *tmp_stdout = NULL;
		gchar *tmp_stderr = NULL;
		gchar **argv;
		gint exit_status;

		tracker_db_manager_move_to_temp ();
		tracker_data_manager_shutdown ();

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
			g_set_error (&info->error, TRACKER_DATA_BACKUP_ERROR, 0,
			             "Error starting tar program");
		}

		if (!info->error && tmp_stderr) {
			if (strlen (tmp_stderr) > 0) {
				g_set_error (&info->error, TRACKER_DATA_BACKUP_ERROR, 0,
				             "%s", tmp_stderr);
			}
		}

		if (!info->error && exit_status != 0) {
			g_set_error (&info->error, TRACKER_DATA_BACKUP_ERROR, 0,
			             "Unknown error, tar exited with exit status %d", exit_status);
		}

		g_free (tmp_stderr);
		g_free (tmp_stdout);
		g_strfreev (argv);

		tracker_db_manager_init_locations ();
		tracker_db_journal_init (NULL, FALSE);

		if (info->error) {
			tracker_db_manager_restore_from_temp ();
		} else {
			tracker_db_manager_remove_temp ();
		}

		tracker_db_journal_shutdown ();

		tracker_data_manager_init (flags, test_schemas, &is_first, TRUE,
		                           busy_callback, busy_user_data,
		                           "Restoring backup");

	} else {
		g_set_error (&info->error, TRACKER_DATA_BACKUP_ERROR, 0,
		             "Backup file doesn't exist");
	}

	on_restore_done (info);
}

