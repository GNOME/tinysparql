/*
 * Copyright (C) 2007, Jamie McCracken <jamiemcc@gnome.org>
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

#include "mingw-compat.h"

#include "tracker-log.h"
#include "tracker-os-dependant.h"

gboolean
tracker_spawn (gchar **argv,
               gint    timeout,
               gchar **tmp_stdout,
               gint   *exit_status)
{
	GSpawnFlags   flags;
	GError       *error = NULL;
	gchar       **new_argv;
	gboolean      result;
	gint          length;
	gint          i;

	g_return_val_if_fail (argv != NULL, FALSE);
	g_return_val_if_fail (argv[0] != NULL, FALSE);
	g_return_val_if_fail (timeout > 0, FALSE);

	length = g_strv_length (argv);

	new_argv = g_new0 (gchar*, length + 3);

	new_argv[0] = "cmd.exe";
	new_argv[1] = "/c";

	for (i = 0; argv[i]; i++) {
		new_argv[i + 2] = argv[i];
	}

	flags = G_SPAWN_SEARCH_PATH |
		G_SPAWN_STDERR_TO_DEV_NULL;

	if (!tmp_stdout) {
		flags |= G_SPAWN_STDOUT_TO_DEV_NULL;
	}

	result = g_spawn_sync (NULL,
	                       new_argv,
	                       NULL,
	                       flags,
	                       NULL,
	                       GINT_TO_POINTER (timeout),
	                       tmp_stdout,
	                       NULL,
	                       exit_status,
	                       &error);

	if (error) {
		g_warning ("Could not spawn command:'%s', %s",
		           argv[0],
		           error->message);
		g_error_free (error);
	}

	g_strfreev (new_argv);

	return result;
}

gboolean
tracker_spawn_async_with_channels (const gchar **argv,
                                   gint        timeout,
                                   GPid        *pid,
                                   GIOChannel  **stdin_channel,
                                   GIOChannel  **stdout_channel,
                                   GIOChannel  **strerr_channel)
{
	GError   *error = NULL;
	gboolean  result;
	gint      stdin, stdout, stderr;

	g_return_val_if_fail (argv != NULL, FALSE);
	g_return_val_if_fail (argv[0] != NULL, FALSE);
	g_return_val_if_fail (timeout > 0, FALSE);
	g_return_val_if_fail (pid != NULL, FALSE);

	/* Note: PID must be non-NULL because we're using the
	 *  G_SPAWN_DO_NOT_REAP_CHILD option, so an explicit call to
	 *  g_spawn_close_pid () will be needed afterwards */

	result = g_spawn_async_with_pipes (NULL,
	                                   (gchar **) argv,
	                                   NULL,
	                                   G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
	                                   tracker_spawn_child_func,
	                                   GINT_TO_POINTER (timeout),
	                                   pid,
	                                   stdin_channel ? &stdin : NULL,
	                                   stdout_channel ? &stdout : NULL,
	                                   stderr_channel ? &stderr : NULL,
	                                   &error);

	if (error) {
		g_warning ("Could not spawn command:'%s', %s",
		           argv[0],
		           error->message);
		g_error_free (error);
	}

	if (stdin_channel) {
		*stdin_channel = result ? g_io_channel_win32_new_fd (stdin) : NULL;
	}

	if (stdout_channel) {
		*stdout_channel = result ? g_io_channel_win32_new_fd (stdout) : NULL;
	}

	if (stderr_channel) {
		*stderr_channel = result ? g_io_channel_win32_new_fd (stderr) : NULL;
	}

	return result;
}

void
tracker_spawn_child_func (gpointer user_data)
{
}

gchar *
tracker_create_permission_string (struct stat finfo)
{
	gchar *str;
	gint   n, bit;

	/* Create permissions string */
	str = g_strdup ("?rwxrwxrwx");

	for (bit = 0400, n = 1; bit; bit >>= 1, ++n) {
		if (!(finfo.st_mode & bit)) {
			str[n] = '-';
		}
	}

	if (finfo.st_mode & S_ISUID) {
		str[3] = (finfo.st_mode & S_IXUSR) ? 's' : 'S';
	}

	if (finfo.st_mode & S_ISGID) {
		str[6] = (finfo.st_mode & S_IXGRP) ? 's' : 'S';
	}

	if (finfo.st_mode & S_ISVTX) {
		str[9] = (finfo.st_mode & S_IXOTH) ? 't' : 'T';
	}

	return str;
}

gboolean
tracker_memory_setrlimits (void)
{
	return TRUE;
}
