/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <locale.h>
#include <errno.h>
#include <sys/types.h>
#include <signal.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>

#include <libtracker-db/tracker-db-manager.h>
#include <libtracker-db/tracker-db-index-manager.h>

static gboolean     should_kill;
static gboolean     should_terminate;
static gboolean     hard_reset;

static GOptionEntry entries[] = {
	{ "kill", 'k', 0, G_OPTION_ARG_NONE, &should_kill,
	  N_("Use SIGKILL to stop all tracker processes found - guarantees death :)"),
	  NULL },
	{ "terminate", 't', 0, G_OPTION_ARG_NONE, &should_terminate,
	  N_("Use SIGTERM to stop all tracker processes found"),
	  NULL 
	},
	{ "hard-reset", 'r', 0, G_OPTION_ARG_NONE, &hard_reset,
	  N_("This will kill all Tracker processes and remove all databases"),
	  NULL },
	{ NULL }
};

static GSList *
get_pids (void)
{
	GError *error = NULL;
	GDir *dir;
	GSList *pids = NULL;
	const gchar *name;

	dir = g_dir_open ("/proc", 0, &error);
	if (error) {
		g_printerr ("%s, %s\n",
			    _("Could not open /proc"),
			    error ? error->message : _("no error given"));
		g_clear_error (&error);
		return NULL;
	}

	while ((name = g_dir_read_name (dir)) != NULL) { 
		gchar c;
		gboolean is_pid = TRUE;

		for (c = *name; c && c != ':' && is_pid; c++) {		
			is_pid &= g_ascii_isdigit (c);
		}

		if (!is_pid) {
			continue;
		}

		pids = g_slist_prepend (pids, g_strdup (name));
	}

	g_dir_close (dir);

	return g_slist_reverse (pids);
}

static void
log_handler (const gchar    *domain,
	     GLogLevelFlags  log_level,
	     const gchar    *message,
	     gpointer	     user_data)
{
	switch (log_level) {
	case G_LOG_LEVEL_WARNING:
	case G_LOG_LEVEL_CRITICAL:
	case G_LOG_LEVEL_ERROR:
	case G_LOG_FLAG_RECURSION:
	case G_LOG_FLAG_FATAL:
		g_fprintf (stderr, "%s\n", message);
		fflush (stderr);
		break;
	case G_LOG_LEVEL_MESSAGE:
	case G_LOG_LEVEL_INFO:
	case G_LOG_LEVEL_DEBUG:
	case G_LOG_LEVEL_MASK:
		g_fprintf (stdout, "%s\n", message);
		fflush (stdout);
		break;
	}	
}

int
main (int argc, char **argv)
{
	GOptionContext *context;
	GError *error = NULL;
	GSList *pids;
	GSList *l;
	gchar  *str;

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	g_type_init ();

	/* Translators: this messagge will apper immediately after the	*/
	/* usage string - Usage: COMMAND [OPTION]... <THIS_MESSAGE>	*/
	context = g_option_context_new (_(" - Manage Tracker processes and data"));
	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	if (should_kill && should_terminate) {
		g_printerr ("%s\n",
			    _("You can not use the --kill and --terminate arguments together"));
		return EXIT_FAILURE;
	} else if (hard_reset && should_terminate) {
		g_printerr ("%s\n",
			    _("You can not use the --terminate with --hard-reset, --kill is implied"));
		return EXIT_FAILURE;
	}

	if (hard_reset) {
		/* Imply --kill */
		should_kill = TRUE;
	}

	pids = get_pids ();
	str = g_strdup_printf (_("Found %d pids..."), g_slist_length (pids));
	g_print ("%s\n", str);
	g_free (str);

	for (l = pids; l; l = l->next) {
		gchar *filename;
		gchar *contents = NULL;
		gchar **strv;

		filename = g_build_filename ("/proc", l->data, "cmdline", NULL);
		if (!g_file_get_contents (filename, &contents, NULL, &error)) {
			str = g_strdup_printf (_("Could not open '%s'"), filename);
			g_printerr ("%s, %s\n", 
				    str,
				    error ? error->message : _("no error given"));
			g_free (str);
			g_clear_error (&error);
			g_free (contents);
			g_free (filename);

			continue;
		}
		
		strv = g_strsplit (contents, "^@", 2);
		if (strv && strv[0]) {
			gchar *basename;

			basename = g_path_get_basename (strv[0]);
			if (g_str_has_prefix (basename, "tracker") == TRUE &&
			    g_str_has_suffix (basename, "-processes") == FALSE) {
				pid_t pid;

				pid = atoi (l->data);
				str = g_strdup_printf (_("Found process ID %d for '%s'"), pid, basename);
				g_print ("%s\n", str);
				g_free (str);

				if (should_terminate) {
					if (kill (pid, SIGTERM) == -1) {
						const gchar *errstr = g_strerror (errno);
						
						str = g_strdup_printf (_("Could not terminate process %d"), pid);
						g_printerr ("  %s, %s\n", 
							    str,
							    errstr ? errstr : _("no error given"));
						g_free (str);
					} else {
						str = g_strdup_printf (_("Terminated process %d"), pid);
						g_print ("  %s\n", str);
						g_free (str);
					}
				} else if (should_kill) {
					if (kill (pid, SIGKILL) == -1) {
						const gchar *errstr = g_strerror (errno);
						
						str = g_strdup_printf (_("Could not kill process %d"), pid);
						g_printerr ("  %s, %s\n", 
							    str,
							    errstr ? errstr : _("no error given"));
						g_free (str);
					} else {
						str = g_strdup_printf (_("Killed process %d"), pid);
						g_print ("  %s\n", str);
						g_free (str);
					}
				}
			}

			g_free (basename);
		}

		g_strfreev (strv);
		g_free (contents);
		g_free (filename);
	}

	g_slist_foreach (pids, (GFunc) g_free, NULL);
	g_slist_free (pids);

	if (hard_reset) {
		guint log_handler_id;

		/* Set log handler for library messages */
		log_handler_id = g_log_set_handler (NULL,
						    G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL,
						    log_handler,
						    NULL);
		
		g_log_set_default_handler (log_handler, NULL);

		/* Clean up */
		if (!tracker_db_manager_init (TRACKER_DB_MANAGER_REMOVE_ALL, NULL, FALSE, NULL)) {
			return EXIT_FAILURE;
		}

		tracker_db_manager_remove_all ();
		tracker_db_manager_shutdown ();

		if (!tracker_db_index_manager_init (TRACKER_DB_INDEX_MANAGER_REMOVE_ALL, 0, 0)) {
			return EXIT_FAILURE;
		}

		tracker_db_index_manager_remove_all ();
		tracker_db_index_manager_shutdown ();

		/* Unset log handler */
		g_log_remove_handler (NULL, log_handler_id);
	}

	return EXIT_SUCCESS;
}
