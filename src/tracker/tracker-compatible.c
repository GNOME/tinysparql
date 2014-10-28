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

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gi18n.h>

#include "tracker-compatible.h"

#warning "This help module 'tracker-compatible' is here for migration only, please remove by 1.6.0"

static void
prepend_args (const char   *prepend_args,
              int           n_prepend_args,
              int           at_index,
              int           argc,
              const char  **argv,
              int          *new_argc,
              char       ***new_argv)
{
	gchar *new_cmd, *old_args;

	old_args = g_strjoinv (" ", (char**) argv + at_index);
	new_cmd = g_strdup_printf ("%s %s", prepend_args, old_args);
	g_free (old_args);

	*new_argv = g_strsplit (new_cmd, " ", -1);
	*new_argc = argc + n_prepend_args;
	g_free (new_cmd);
}

static gboolean
find_arg (const char  *arg_long,
          const char  *arg_short,
          int          argc,
          const char **argv)
{
	const char *p;
	int i, arg_long_len, arg_short_len;

	if (argc < 1) {
		return FALSE;
	}

	arg_long_len = arg_long ? strlen (arg_long) : 0;
	arg_short_len = arg_short ? strlen (arg_short) : 0;

	for (i = 0, p = argv[0]; i < argc; p = argv[++i]) {
		if (arg_long_len > 0 && strncmp (arg_long, p, arg_long_len) == 0) {
			return TRUE;
		} else if (arg_short_len > 0 && strncmp (arg_short, p, arg_short_len) == 0) {
			return TRUE;
		}
	}

	return FALSE;
}

gboolean
tracker_compatible_commands (int           argc,
                             const char  **argv,
                             int          *new_argc,
                             char       ***new_argv)
{
	const gchar *old_cmd;
	gchar *new_cmd, *old_args;

	/* Get symlink name */
	old_cmd = g_getenv ("TRACKER_OLD_CMD");

	if (!old_cmd) {
		return FALSE;
	}

	if (strcmp (old_cmd, "tracker") == 0 ||
	    strncmp (old_cmd, "tracker-", 8) != 0) {
		return FALSE;
	}

	old_cmd += 8;

	if (*old_cmd == '\0') {
		return FALSE;
	}

	old_args = g_strjoinv (" ", (char**) argv + 1);
	new_cmd = g_strdup_printf ("tracker %s %s", old_cmd, old_args);
	g_free (old_args);

	*new_argv = g_strsplit (new_cmd, " ", -1);
	*new_argc = argc + 1;
	g_free (new_cmd);

	return TRUE;
}

gboolean
tracker_compatible (int           argc,
                    const char  **argv,
                    int          *new_argc,
                    char       ***new_argv)
{
	*new_argc = 0;
	*new_argv = NULL;

	/*
	 * Conversions:
	 * $ tracker-control
	 *   --list-processes | -p    --> 'tracker daemon $same_args'
	 *   --kill | -k              --> 'tracker daemon $same_args'
	 *   --terminate | -t         --> 'tracker daemon $same_args'
	 *   --hard-reset | -r        --> 'tracker reset --hard | -r'
	 *   --soft-reset | -e        --> 'tracker reset --soft | -e'
	 *   --remove-config | -c     --> 'tracker reset --config | -c'
	 *   --start | -s             --> 'tracker daemon $same_args'
	 *   --backup | -b            --> 'tracker index $same_args'
	 *   --restore | -o           --> 'tracker index $same_args'
	 *   --set-log-verbosity      --> 'tracker daemon $same_args'
	 *   --get-log-verbosity      --> 'tracker daemon $same_args'
	 *   --collect-debug-info     --> 'tracker status $same_args'
	 *   --watch | -w             --> 'tracker daemon --watch | -w'
	 *   --follow | -F            --> 'tracker daemon --follow | -f'
	 *   --list-common-statuses   --> 'tracker daemon $same_args'
	 *   --list-miners-running    --> 'tracker daemon $same_args'
	 *   --list-miners-available  --> 'tracker daemon $same_args'
	 *   --miner                  --> 'tracker daemon $same_args'
	 *   --pause-details          --> 'tracker daemon $same_args'
	 *   --pause                  --> 'tracker daemon $same_args'
	 *   --pause-for-process      --> 'tracker daemon $same_args'
	 *   --resume                 --> 'tracker daemon $same_args'
	 *   --status | -S            --> 'tracker daemon' (no args)
	 *   --reindex-mime-type | -m --> 'tracker index $same_args'
	 *   --index-file | -f        --> 'tracker index --file | -f'
	 *
	 * $ tracker-stats            --> 'tracker status --stat $same_args'
	 *
	 * $ tracker-import           --> 'tracker index --import | -i'
	 * 
	 */
	if (strcmp (argv[0], "stats") == 0) {
		prepend_args ("status --stat", 1, 1, argc, argv, new_argc, new_argv);
		return TRUE;
	} else if (strcmp (argv[0], "import") == 0) {
		prepend_args ("index --import", 1, 1, argc, argv, new_argc, new_argv);
		return TRUE;
	} else if (strcmp (argv[0], "control") == 0) {
		if (find_arg ("--list-processes", "-p", argc, argv) ||
		    find_arg ("--kill", "-k", argc, argv) ||
		    find_arg ("--terminate", "-t", argc, argv) ||
		    find_arg ("--start", "-s", argc, argv) ||
		    find_arg ("--set-log-verbosity", NULL, argc, argv) ||
		    find_arg ("--get-log-verbosity", NULL, argc, argv) ||
		    find_arg ("--watch", "-w", argc, argv) ||
		    find_arg ("--list-common-statuses", NULL, argc, argv) ||
		    find_arg ("--list-miners-running", NULL, argc, argv) ||
		    find_arg ("--list-miners-available", NULL, argc, argv) ||
		    find_arg ("--miner", NULL, argc, argv) ||
		    find_arg ("--pause-details", NULL, argc, argv) ||
		    find_arg ("--pause", NULL, argc, argv) ||
		    find_arg ("--pause-for-process", NULL, argc, argv) ||
		    find_arg ("--resume", NULL, argc, argv)) {
			argv[0] = "daemon";
			return FALSE;
		} else if (find_arg ("--follow", "-F", argc, argv)) {
			/* Previously, we had -F, now we support -f */
			*new_argv = g_strsplit ("daemon --follow", " ", -1);
			*new_argc = 2;
			return TRUE;
		} else if (find_arg ("--collect-debug-info", NULL, argc, argv)) {
			argv[0] = "status";
			return FALSE;
		} else if (find_arg ("--hard-reset", "-r", argc, argv)) {
			*new_argv = g_strsplit ("reset --hard", " ", -1);
			*new_argc = 2;
			return TRUE;
		} else if (find_arg ("--soft-reset", "-e", argc, argv)) {
			*new_argv = g_strsplit ("reset --soft", " ", -1);
			*new_argc = 2;
			return TRUE;
		} else if (find_arg ("--remove-config", "-c", argc, argv)) {
			*new_argv = g_strsplit ("reset --config", " ", -1);
			*new_argc = 2;
			return TRUE;
		} else if (find_arg ("--backup", "-b", argc, argv)) {
			prepend_args ("index --backup", 1, 1, argc, argv, new_argc, new_argv);
			return TRUE;
		} else if (find_arg ("--restore", "-o", argc, argv)) {
			prepend_args ("index --restore", 1, 1, argc, argv, new_argc, new_argv);
			return TRUE;
		} else if (find_arg ("--reindex-mime-type", "-m", argc, argv)) {
			prepend_args ("index --reindex-mime-type", 1, 1, argc, argv, new_argc, new_argv);
			return TRUE;
		} else if (find_arg ("--index-file", "-f", argc, argv)) {
			prepend_args ("index --file", 1, 1, argc, argv, new_argc, new_argv);
			return TRUE;
		} else if (find_arg ("--help", "-h", argc, argv)) {
			g_printerr ("%s\n\n",
			            _("The 'tracker-control' command is no longer available"));
			*new_argv = g_strsplit ("daemon -h", " ", -1);
			*new_argc = 2;
			return TRUE;
		} else {
			/* This includes --status and -S */
			*new_argv = g_strsplit ("daemon", " ", -1);
			*new_argc = 1;
			return TRUE;
		}
	}

	/* Return TRUE if we have new argv/argc, otherwise FALSE */
	return FALSE;
}
