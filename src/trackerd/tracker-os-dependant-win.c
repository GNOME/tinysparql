/* Tracker - indexer and metadata database engine
 * Copyright (C) 2007, Mr Jamie McCracken (jamiemcc@gnome.org)
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

#include <glib/gspawn.h>
#include <glib/gstring.h>

#include "mingw-compat.h"
#include "tracker-os-dependant.h"


gboolean
tracker_check_uri (const gchar *uri)
{
        return uri != NULL;
}


gboolean
tracker_spawn (gchar **argv, gint timeout, gchar **tmp_stdout, gint *exit_status)
{
        gint length;
        gint i;

        for (i = 0; argv[i]; i++)
                ;
        length = i;

        gchar **new_argv = g_new0 (gchar *, length + 3);

        new_argv[0] = "cmd.exe";
        new_argv[1] = "/c";

        for (i = 0; argv[i]; i++) {
                new_argv[i + 2] = argv[i];
        }

	GSpawnFlags     flags;
	GError          *error = NULL;

	if (!tmp_stdout) {
		flags = G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL;
	} else {
		flags = G_SPAWN_SEARCH_PATH | G_SPAWN_STDERR_TO_DEV_NULL;
	}

	gboolean status = g_spawn_sync (NULL,
                                        new_argv,
                                        NULL,
                                        flags,
                                        NULL,
                                        GINT_TO_POINTER (timeout),
                                        tmp_stdout,
                                        NULL,
                                        exit_status,
                                        &error);

	if (!status) {
                tracker_log (error->message);
                g_error_free (error);	
	}

        g_strfreev (new_argv);

	return status;
}


gchar *
tracker_create_permission_string (struct stat finfo)
{
        gchar   *str;
	gint    n, bit;

	/* create permissions string */
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


void
tracker_child_cb (gpointer user_data)
{
}
