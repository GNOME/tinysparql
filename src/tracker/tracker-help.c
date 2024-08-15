/*
 * Copyright (C) 2014, Lanedo <martyn@lanedo.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include "config.h"

#include <unistd.h>
#include <errno.h>

#include <glib/gi18n.h>

#include "tracker-help.h"

static void
setup_man_path (void)
{
	GString *new_path = g_string_new ("");
	const char *old_path = g_getenv ("MANPATH");

	/* We should always put ':' after our path. If there is no
	 * old_path, the ':' at the end will let 'man' to try
	 * system-wide paths after ours to find the manual page. If
	 * there is old_path, we need ':' as delimiter. */
	g_string_append (new_path, MANDIR);
	g_string_append_c (new_path, ':');

	if (old_path) {
		g_string_append (new_path, old_path); 
	}

	if (!g_setenv ("MANPATH", new_path->str, TRUE))
		g_warning ("Could not set MANPATH");

	g_string_free (new_path, TRUE);
}

static int
exec_man_man (const char *path, const char *page)
{
	const gchar *argv[3] = { 0, };
	gboolean retval;
	int status;

	if (!path) {
		path = "man";
	}

	argv[0] = path;
	argv[1] = page;
	retval = g_spawn_sync (NULL, (gchar**) argv, NULL,
	                       G_SPAWN_SEARCH_PATH,
	                       NULL, NULL, NULL, NULL,
	                       &status, NULL);

	return (!retval || !g_spawn_check_wait_status (status, NULL)) ? -1 : 0;
}

static char *
cmd_to_page (const char *cmd)
{
	if (!cmd) {
		return g_strdup (MAIN_COMMAND_NAME);
	} else if (g_str_has_prefix (cmd, MAIN_COMMAND_NAME "-")) {
		return g_strdup (cmd);
	} else {
		return g_strdup_printf (MAIN_COMMAND_NAME "-%s", cmd);
	}
}

int
tracker_help_show_man_page (const char *cmd)
{
	char *page = cmd_to_page (cmd);
	int retval;

	setup_man_path ();

	retval = exec_man_man ("man", page);
	g_free (page);

	return retval;
}

int
tracker_help (int argc, const char **argv)
{
	return tracker_help_show_man_page (argv[1]);
}
