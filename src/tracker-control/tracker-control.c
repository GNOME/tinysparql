/*
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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
#include <glib/gstdio.h>

#include <libtracker-common/tracker-common.h>
#include <libtracker-data/tracker-db-config.h>
#include <libtracker-data/tracker-db-journal.h>
#include <libtracker-data/tracker-db-manager.h>
#include <libtracker-miner/tracker-miner.h>

#include "tracker-miner-files-index-client.h"

#define ABOUT	  \
	"Tracker " PACKAGE_VERSION "\n"

#define LICENSE	  \
	"This program is free software and comes without any warranty.\n" \
	"It is licensed under version 2 or later of the General Public " \
	"License which can be viewed at:\n" \
	"\n" \
	"  http://www.gnu.org/licenses/gpl.txt\n"

#define OPTION_TERM_ALL "all"
#define OPTION_TERM_STORE "store"
#define OPTION_TERM_MINERS "miners"

typedef enum {
	TERM_NONE,
	TERM_ALL,
	TERM_STORE,
	TERM_MINERS
} TermOption;

static TermOption kill_option = TERM_NONE;
static TermOption terminate_option = TERM_NONE;
static gboolean hard_reset;
static gboolean soft_reset;
static gboolean remove_config;
static gboolean start;
static const gchar **reindex_mime_types;
static gboolean print_version;
static gchar *index_file;

static gboolean term_option_arg_func (const gchar  *option_value,
                                      const gchar  *value,
                                      gpointer      data,
                                      GError      **error);

static GOptionEntry entries[] = {
	{ "kill", 'k', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, term_option_arg_func,
	  N_("Use SIGKILL to stop all matching processes, either \"store\", \"miners\" or \"all\" may be used, no parameter equals \"all\""),
	  N_("APPS") },
	{ "terminate", 't', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, term_option_arg_func,
	  N_("Use SIGTERM to stop all matching processes, either \"store\", \"miners\" or \"all\" may be used, no parameter equals \"all\""),
	  N_("APPS") },
	{ "hard-reset", 'r', 0, G_OPTION_ARG_NONE, &hard_reset,
	  N_("Kill all Tracker processes and remove all databases"),
	  NULL },
	{ "soft-reset", 'e', 0, G_OPTION_ARG_NONE, &soft_reset,
	  N_("Same as --hard-reset but the backup & journal are restored after restart"),
	  NULL },
	{ "remove-config", 'c', 0, G_OPTION_ARG_NONE, &remove_config,
	  N_("Remove all configuration files so they are re-generated on next start"),
	  NULL },
	{ "start", 's', 0, G_OPTION_ARG_NONE, &start,
	  N_("Starts miners (which indirectly starts tracker-store too)"),
	  NULL },
	{ "reindex-mime-type", 'm', 0, G_OPTION_ARG_STRING_ARRAY, &reindex_mime_types,
	  N_("Reindex files which match the mime type supplied (for new extractors), use -m MIME1 -m MIME2"),
	  N_("MIME") },
	{ "index-file", 'f', 0, G_OPTION_ARG_FILENAME, &index_file,
	  N_("(Re)Index a given file"),
	  N_("File") },
	{ "version", 'V', 0, G_OPTION_ARG_NONE, &print_version,
	  N_("Print version"),
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
             gpointer        user_data)
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
	default:
		g_fprintf (stdout, "%s\n", message);
		fflush (stdout);
		break;
	}
}

static gboolean
crawler_check_file_cb (TrackerCrawler *crawler,
                       GFile          *file,
                       gpointer        user_data)
{
	const gchar **suffix;
	gchar *path;
	gboolean should_remove;

	suffix = user_data;
	path = g_file_get_path (file);

	if (suffix) {
		should_remove = g_str_has_suffix (path, *suffix);
	} else {
		should_remove = TRUE;
	}

	if (!should_remove) {
		g_free (path);
		return FALSE;
	}

	/* Remove file */
	if (g_unlink (path) == 0) {
		g_print ("  %s\n", path);
	}

	g_free (path);

	return should_remove;
}

static void
crawler_finished_cb (TrackerCrawler *crawler,
                     gboolean        was_interrupted,
                     gpointer        user_data)
{
	g_main_loop_quit (user_data);
}

static gboolean
term_option_arg_func (const gchar  *option_value,
                      const gchar  *value,
                      gpointer      data,
                      GError      **error)
{
	TermOption option;

	if (!value) {
		value = OPTION_TERM_ALL;
	}

	if (strcmp (value, OPTION_TERM_ALL) == 0) {
		option = TERM_ALL;
	} else if (strcmp (value, OPTION_TERM_STORE) == 0) {
		option = TERM_STORE;
	} else if (strcmp (value, OPTION_TERM_MINERS) == 0) {
		option = TERM_MINERS;
	} else {
		g_set_error_literal (error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
		                     "Only one of 'all', 'store' and 'miners' are allowed");
		return FALSE;
	}

	if (strcmp (option_value, "-k") == 0 ||
	    strcmp (option_value, "--kill") == 0) {
		kill_option = option;
	} else if (strcmp (option_value, "-t") == 0 ||
	           strcmp (option_value, "--terminate") == 0) {
		terminate_option = option;
	}

	return TRUE;
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

	if (!g_thread_supported ()) {
		g_thread_init (NULL);
	}

	/* Translators: this messagge will apper immediately after the  */
	/* usage string - Usage: COMMAND [OPTION]... <THIS_MESSAGE>     */
	context = g_option_context_new (_(" - Manage Tracker processes and data"));
	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	if (print_version) {
		g_print ("\n" ABOUT "\n" LICENSE "\n");
		return EXIT_SUCCESS;
	}

	if (kill_option != TERM_NONE && terminate_option != TERM_NONE) {
		g_printerr ("%s\n",
		            _("You can not use the --kill and --terminate arguments together"));
		return EXIT_FAILURE;
	} else if ((hard_reset || soft_reset) && terminate_option != TERM_NONE) {
		g_printerr ("%s\n",
		            _("You can not use the --terminate with --hard-reset or --soft-reset, --kill is implied"));
		return EXIT_FAILURE;
	} else if (hard_reset && soft_reset) {
		g_printerr ("%s\n",
		            _("You can not use the --hard-reset and --soft-reset arguments together"));
		return EXIT_FAILURE;
	}

	if (hard_reset || soft_reset) {
		/* Imply --kill */
		kill_option = TERM_ALL;
	}

	/* Unless we are stopping processes or listing processes,
	 * don't iterate them.
	 */
	if (kill_option != TERM_NONE ||
	    terminate_option != TERM_NONE ||
	    (!start && !remove_config && !reindex_mime_types && 
	     !print_version && !index_file)) {
		pids = get_pids ();
		str = g_strdup_printf (g_dngettext (NULL,
		                                    "Found %d PID…",
		                                    "Found %d PIDs…",
		                                    g_slist_length (pids)),
		                       g_slist_length (pids));
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

				if ((g_str_has_prefix (basename, "tracker") == TRUE ||
				     g_str_has_prefix (basename, "lt-tracker") == TRUE) &&
				    g_str_has_suffix (basename, "-control") == FALSE) {
					pid_t pid;

					pid = atoi (l->data);
					str = g_strdup_printf (_("Found process ID %d for '%s'"), pid, basename);
					g_print ("%s\n", str);
					g_free (str);

					if (terminate_option != TERM_NONE) {
						if ((terminate_option == TERM_STORE &&
						     !g_str_has_suffix (basename, "tracker-store")) ||
						    (terminate_option == TERM_MINERS &&
						     !strstr (basename, "tracker-miner"))) {
							continue;
						}

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
					} else if (kill_option != TERM_NONE) {
						if ((kill_option == TERM_STORE &&
						     !g_str_has_suffix (basename, "tracker-store")) ||
						    (kill_option == TERM_MINERS &&
						     !strstr (basename, "tracker-miner"))) {
							continue;
						}

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
	}

	if (hard_reset || soft_reset) {
		guint log_handler_id;
		const gchar *rotate_to = NULL;
		TrackerDBConfig *db_config;
		gsize chunk_size;
		gint chunk_size_mb;


		db_config = tracker_db_config_new ();

		/* Set log handler for library messages */
		log_handler_id = g_log_set_handler (NULL,
		                                    G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL,
		                                    log_handler,
		                                    NULL);

		g_log_set_default_handler (log_handler, NULL);

		chunk_size_mb = tracker_db_config_get_journal_chunk_size (db_config);
		chunk_size = (gsize) ((gsize) chunk_size_mb * (gsize) 1024 * (gsize) 1024);
		rotate_to = tracker_db_config_get_journal_rotate_destination (db_config);

		/* This call is needed to set the journal's filename */

		tracker_db_journal_set_rotating ((chunk_size_mb != -1),
		                                 chunk_size, rotate_to);

		tracker_db_journal_init (NULL, FALSE);

		g_object_unref (db_config);

		/* Clean up */
		if (!tracker_db_manager_init (TRACKER_DB_MANAGER_REMOVE_ALL, NULL, FALSE)) {
			return EXIT_FAILURE;
		}

		tracker_db_manager_remove_all (hard_reset);
		tracker_db_manager_shutdown ();
		tracker_db_journal_shutdown ();

		/* Unset log handler */
		g_log_remove_handler (NULL, log_handler_id);
	}

	if (remove_config) {
		GMainLoop *main_loop;
		GFile *file;
		TrackerCrawler *crawler;
		const gchar *suffix = ".cfg";
		const gchar *home_conf_dir;
		gchar *path;

		crawler = tracker_crawler_new ();
		main_loop = g_main_loop_new (NULL, FALSE);

		g_signal_connect (crawler, "check-file",
		                  G_CALLBACK (crawler_check_file_cb),
		                  &suffix);
		g_signal_connect (crawler, "finished",
		                  G_CALLBACK (crawler_finished_cb),
		                  main_loop);

		/* Go through service files */


		/* Check the default XDG_DATA_HOME location */
		home_conf_dir = g_getenv ("XDG_CONFIG_HOME");

		if (home_conf_dir && tracker_path_has_write_access_or_was_created (home_conf_dir)) {
			path = g_build_path (G_DIR_SEPARATOR_S, home_conf_dir, "tracker", NULL);
		} else {
			home_conf_dir = g_getenv ("HOME");

			if (!home_conf_dir || !tracker_path_has_write_access_or_was_created (home_conf_dir)) {
				home_conf_dir = g_get_home_dir ();
			}
			path = g_build_path (G_DIR_SEPARATOR_S, home_conf_dir, ".config", "tracker", NULL);
		}


		file = g_file_new_for_path (path);
		g_free (path);

		g_print ("%s\n", _("Removing configuration files…"));

		tracker_crawler_start (crawler, file, FALSE);
		g_object_unref (file);

		g_main_loop_run (main_loop);
		g_object_unref (crawler);
	}

	if (start) {
		TrackerMinerManager *manager;
		GSList *miners, *l;

		manager = tracker_miner_manager_new ();
		miners = tracker_miner_manager_get_available (manager);

		if (hard_reset || soft_reset) {
			g_print ("%s\n", _("Waiting one second before starting miners…"));

			/* Give a second's grace to avoid race conditions */
			g_usleep (G_USEC_PER_SEC);
		}

		g_print ("%s\n", _("Starting miners…"));

		/* Get the status of all miners, this will start all
		 * miners not already running.
		 */

		for (l = miners; l; l = l->next) {
			const gchar *display_name;
			gdouble progress = 0.0;

			display_name = tracker_miner_manager_get_display_name (manager, l->data);

			if (!tracker_miner_manager_get_status (manager, l->data, NULL, &progress)) {
				g_printerr ("  ✗ %s (%s)\n",
				            display_name,
				            _("perhaps a disabled plugin?"));
			} else {
				g_print ("  ✓ %s\n",
				         display_name);
			}

			g_free (l->data);
		}

		g_slist_free (miners);
		g_object_unref (manager);
	}

	if (reindex_mime_types) {
		TrackerMinerManager *manager;

		manager = tracker_miner_manager_new ();
		tracker_miner_manager_reindex_by_mimetype (manager, reindex_mime_types, &error);

		if (error) {
			g_print ("Could not reindex mimetypes: %s\n",
			         error->message);
			g_error_free (error);
			return EXIT_FAILURE;
		}

		g_object_unref (manager);
	}

	if (index_file) {
		TrackerMinerManager *manager;
		GError *error = NULL;
		GFile *file;

		file = g_file_new_for_commandline_arg (index_file);
		manager = tracker_miner_manager_new ();

		tracker_miner_manager_index_file (manager, file, &error);

		if (error) {
			g_print ("Could not (re)index file '%s': %s\n",
			         index_file, error->message);
			g_error_free (error);
			return EXIT_FAILURE;
		}

		g_object_unref (manager);
		g_object_unref (file);
	}

	return EXIT_SUCCESS;
}
