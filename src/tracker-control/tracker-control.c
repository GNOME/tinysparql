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

static GDBusConnection *connection = NULL;
static GDBusProxy *proxy = NULL;
static GMainLoop *main_loop;
static GHashTable *miners_progress;
static GHashTable *miners_status;
static gint longest_miner_name_length = 0;
static gint paused_length = 0;

/* Control options */
static TermOption kill_option = TERM_NONE;
static TermOption terminate_option = TERM_NONE;
static gboolean hard_reset;
static gboolean soft_reset;
static gboolean remove_config;
static gboolean start;
static const gchar **reindex_mime_types;
static gchar *index_file;
static gchar *miner_name;
static gchar *pause_reason;
static gint resume_cookie = -1;
#define CONTROL_OPTION_ENABLED()	  \
	(kill_option != TERM_NONE || \
	 terminate_option != TERM_NONE || \
	 hard_reset || \
	 soft_reset || \
	 remove_config || \
	 start || \
	 reindex_mime_types != NULL || \
	 index_file != NULL || \
	 miner_name != NULL || \
	 pause_reason != NULL || \
	 resume_cookie >= 0)

/* Status options */
static gboolean status_details;
static gboolean follow;
static gboolean detailed;
static gboolean list_common_statuses;
static gboolean list_miners_running;
static gboolean list_miners_available;
static gboolean pause_details;
#define STATUS_OPTION_ENABLED()	  \
	(status_details || \
	 follow || \
	 detailed || \
	 list_common_statuses || \
	 list_miners_running || \
	 list_miners_available || \
	 pause_details)

/* Common options */
static gboolean print_version;

/* Make sure our statuses are translated (all from libtracker-miner except one) */
static const gchar *statuses[7] = {
	N_("Initializing"),
	N_("Processing…"),
	N_("Fetching…"), /* miner/rss */
	N_("Crawling single directory '%s'"),
	N_("Crawling recursively directory '%s'"),
	N_("Paused"),
	N_("Idle")
};

static gboolean term_option_arg_func (const gchar  *option_value,
                                      const gchar  *value,
                                      gpointer      data,
                                      GError      **error);

/* ---- CONTROL options ---- */
static GOptionEntry control_entries[] = {
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
	  N_("FILE") },
	{ "pause", 0 , 0, G_OPTION_ARG_STRING, &pause_reason,
	  N_("Pause a miner (you must use this with --miner)"),
	  N_("REASON")
	},
	{ "resume", 0 , 0, G_OPTION_ARG_INT, &resume_cookie,
	  N_("Resume a miner (you must use this with --miner)"),
	  N_("COOKIE")
	},
	{ "miner", 0 , 0, G_OPTION_ARG_STRING, &miner_name,
	  N_("Miner to use with --resume or --pause (you can use suffixes, e.g. Files or Applications)"),
	  N_("MINER")
	},
	{ NULL }
};

/* ---- STATUS options ---- */
static GOptionEntry status_entries[] = {
	{ "status-details", 'S', 0, G_OPTION_ARG_NONE, &status_details,
	  N_("Show current status"),
	  NULL
	},
	{ "follow", 'F', 0, G_OPTION_ARG_NONE, &follow,
	  N_("Follow status changes as they happen"),
	  NULL
	},
	{ "detailed", 'D', 0, G_OPTION_ARG_NONE, &detailed,
	  N_("Include details with state updates (only applies to --follow)"),
	  NULL
	},
	{ "list-common-statuses", 'C', 0, G_OPTION_ARG_NONE, &list_common_statuses,
	  N_("List common statuses for miners and the store"),
	  NULL
	},
	{ "list-miners-running", 'L', 0, G_OPTION_ARG_NONE, &list_miners_running,
	  N_("List all miners currently running"),
	  NULL
	},
	{ "list-miners-available", 'A', 0, G_OPTION_ARG_NONE, &list_miners_available,
	  N_("List all miners installed"),
	  NULL
	},
	{ "pause-details", 'I', 0, G_OPTION_ARG_NONE, &pause_details,
	  N_("List pause reasons"),
	  NULL
	},
	{ NULL }
};

/* ---- COMMON options ---- */
static GOptionEntry common_entries[] = {
	{ "version", 'V', 0, G_OPTION_ARG_NONE, &print_version,
	  N_("Print version"),
	  NULL },
	{ NULL }
};

static void
signal_handler (int signo)
{
	static gboolean in_loop = FALSE;

	/* Die if we get re-entrant signals handler calls */
	if (in_loop) {
		exit (EXIT_FAILURE);
	}

	switch (signo) {
	case SIGTERM:
	case SIGINT:
		in_loop = TRUE;
		g_main_loop_quit (main_loop);

		/* Fall through */
	default:
		if (g_strsignal (signo)) {
			g_print ("\n");
			g_print ("Received signal:%d->'%s'\n",
			         signo,
			         g_strsignal (signo));
		}
		break;
	}
}

static void
initialize_signal_handler (void)
{
	struct sigaction act;
	sigset_t empty_mask;

	sigemptyset (&empty_mask);
	act.sa_handler = signal_handler;
	act.sa_mask = empty_mask;
	act.sa_flags = 0;

	sigaction (SIGTERM, &act, NULL);
	sigaction (SIGINT, &act, NULL);
	sigaction (SIGHUP, &act, NULL);
}

static int
miner_pause (const gchar *miner,
             const gchar *reason)
{
	TrackerMinerManager *manager;
	gchar *str;
	gint cookie;

	manager = tracker_miner_manager_new ();
	str = g_strdup_printf (_("Attempting to pause miner '%s' with reason '%s'"),
	                       miner,
	                       reason);
	g_print ("%s\n", str);
	g_free (str);

	if (!tracker_miner_manager_pause (manager, miner, reason, &cookie)) {
		g_printerr (_("Could not pause miner: %s"), miner);
		g_printerr ("\n");
		return EXIT_FAILURE;
	}

	str = g_strdup_printf (_("Cookie is %d"), cookie);
	g_print ("  %s\n", str);
	g_free (str);
	g_object_unref (manager);

	return EXIT_SUCCESS;
}

static int
miner_resume (const gchar *miner,
              gint         cookie)
{
	TrackerMinerManager *manager;
	gchar *str;

	manager = tracker_miner_manager_new ();
	str = g_strdup_printf (_("Attempting to resume miner %s with cookie %d"),
	                       miner,
	                       cookie);
	g_print ("%s\n", str);
	g_free (str);

	if (!tracker_miner_manager_resume (manager, miner, cookie)) {
		g_printerr (_("Could not resume miner: %s"), miner);
		return EXIT_FAILURE;
	}

	g_print ("  %s\n", _("Done"));

	g_object_unref (manager);

	return EXIT_SUCCESS;
}

static gboolean
miner_get_details (TrackerMinerManager  *manager,
                   const gchar          *miner,
                   gchar               **status,
                   gdouble              *progress,
                   GStrv                *pause_applications,
                   GStrv                *pause_reasons)
{
	if ((status || progress) &&
	    !tracker_miner_manager_get_status (manager, miner,
	                                       status, progress)) {
		g_printerr (_("Could not get status from miner: %s"), miner);
		return FALSE;
	}

	tracker_miner_manager_is_paused (manager, miner,
	                                 pause_applications,
	                                 pause_reasons);

	return TRUE;
}

static void
miner_print_state (TrackerMinerManager *manager,
                   const gchar         *miner_name,
                   const gchar         *status,
                   gdouble              progress,
                   gboolean             is_running,
                   gboolean             is_paused)
{
	const gchar *name;
	time_t now;
	gchar time_str[64];
	size_t len;
	struct tm *local_time;
	gchar *progress_str;

	if (detailed) {
		now = time ((time_t *) NULL);
		local_time = localtime (&now);
		len = strftime (time_str,
		                sizeof (time_str) - 1,
		                "%d %b %Y, %H:%M:%S:",
		                local_time);
		time_str[len] = '\0';
	} else {
		time_str[0] = '\0';
	}

	name = tracker_miner_manager_get_display_name (manager, miner_name);

	if (!is_running) {
		progress_str = g_strdup_printf ("✗   ");
	} else if (progress > 0.0 && progress < 1.0) {
		progress_str = g_strdup_printf ("%-3.0f%%", progress * 100);
	} else {
		progress_str = g_strdup_printf ("✓   ");
	}

	if (is_running) {
		g_print ("%s  %s  %-*.*s %s%-*.*s%s %s %s\n",
		         time_str,
		         progress_str,
		         longest_miner_name_length,
		         longest_miner_name_length,
		         name,
		         is_paused ? "(" : " ",
		         paused_length,
		         paused_length,
		         is_paused ? _("PAUSED") : " ",
		         is_paused ? ")" : " ",
		         status ? "-" : "",
		         status ? _(status) : "");
	} else {
		g_print ("%s  %s  %-*.*s  %-*.*s  - %s\n",
		         time_str,
		         progress_str,
		         longest_miner_name_length,
		         longest_miner_name_length,
		         name,
		         paused_length,
		         paused_length,
		         " ",
		         _("Not running or is a disabled plugin"));
	}

	g_free (progress_str);
}

static void
store_print_state (const gchar *status,
                   gdouble      progress)
{
	gchar *operation = NULL;
	gchar *operation_status = NULL;
	gchar time_str[64];
	gchar *progress_str;

	if (status && strstr (status, "-")) {
		gchar **status_split;

		status_split = g_strsplit (status, "-", 2);
		if (status_split[0] && status_split[1]) {
			operation = status_split[0];
			operation_status = status_split[1];
			/* Free the array, not the contents */
			g_free (status_split);
		} else {
			/* Free everything */
			g_strfreev (status_split);
		}
	}

	if (detailed) {
		struct tm *local_time;
		time_t now;
		size_t len;

		now = time ((time_t *) NULL);
		local_time = localtime (&now);
		len = strftime (time_str,
		                sizeof (time_str) - 1,
		                "%d %b %Y, %H:%M:%S:",
		                local_time);
		time_str[len] = '\0';
	} else {
		time_str[0] = '\0';
	}

	if (progress > 0.0 && progress < 1.0) {
		progress_str = g_strdup_printf ("%-3.0f%%", progress * 100);
	} else {
		progress_str = g_strdup_printf ("✓   ");
	}

	g_print ("%s  %s  %-*.*s    %s %s\n",
	         time_str,
	         progress_str,
	         longest_miner_name_length + paused_length,
	         longest_miner_name_length + paused_length,
	         operation ? _(operation) : _(status),
	         operation_status ? "-" : "",
	         operation_status ? _(operation_status) : "");

	g_free (progress_str);
	g_free (operation);
	g_free (operation_status);
}

static void
store_get_and_print_state (void)
{
	GVariant *v_status, *v_progress;
	const gchar *status = NULL;
	gdouble progress = -1.0;
	GError *error = NULL;

	/* Status */
	v_status = g_dbus_proxy_call_sync (proxy,
	                                   "GetStatus",
	                                   NULL,
	                                   G_DBUS_CALL_FLAGS_NONE,
	                                   -1,
	                                   NULL,
	                                   &error);

	g_variant_get (v_status, "(&s)", &status);

	if (!status || error) {
		g_critical ("Could not retrieve tracker-store status: %s",
		            error ? error->message : "no error given");
		g_clear_error (&error);
		return;
	}

	/* Progress */
	v_progress = g_dbus_proxy_call_sync (proxy,
	                                     "GetProgress",
	                                     NULL,
	                                     G_DBUS_CALL_FLAGS_NONE,
	                                     -1,
	                                     NULL,
	                                     &error);

	g_variant_get (v_progress, "(d)", &progress);

	if (progress < 0.0 || error) {
		g_critical ("Could not retrieve tracker-store progress: %s",
		            error ? error->message : "no error given");
		g_clear_error (&error);
		return;
	}

	/* Print */
	store_print_state (status, progress);

	g_variant_unref (v_progress);
	g_variant_unref (v_status);
}

static void
manager_miner_progress_cb (TrackerMinerManager *manager,
                           const gchar         *miner_name,
                           const gchar         *status,
                           gdouble              progress)
{
	GValue *gvalue;

	gvalue = g_slice_new0 (GValue);

	g_value_init (gvalue, G_TYPE_DOUBLE);
	g_value_set_double (gvalue, progress);

	miner_print_state (manager, miner_name, status, progress, TRUE, FALSE);

	g_hash_table_replace (miners_status,
	                      g_strdup (miner_name),
	                      g_strdup (status));
	g_hash_table_replace (miners_progress,
	                      g_strdup (miner_name),
	                      gvalue);
}

static void
manager_miner_paused_cb (TrackerMinerManager *manager,
                         const gchar         *miner_name)
{
	GValue *gvalue;

	gvalue = g_hash_table_lookup (miners_progress, miner_name);

	miner_print_state (manager, miner_name,
	                   g_hash_table_lookup (miners_status, miner_name),
	                   gvalue ? g_value_get_double (gvalue) : 0.0,
	                   TRUE,
	                   TRUE);
}

static void
manager_miner_resumed_cb (TrackerMinerManager *manager,
                          const gchar         *miner_name)
{
	GValue *gvalue;

	gvalue = g_hash_table_lookup (miners_progress, miner_name);

	miner_print_state (manager, miner_name,
	                   g_hash_table_lookup (miners_status, miner_name),
	                   gvalue ? g_value_get_double (gvalue) : 0.0,
	                   TRUE,
	                   FALSE);
}

static void
miners_progress_destroy_notify (gpointer data)
{
	GValue *value;

	value = data;
	g_value_unset (value);
	g_slice_free (GValue, value);
}

static void
store_progress (GDBusConnection *connection,
                const gchar     *sender_name,
                const gchar     *object_path,
                const gchar     *interface_name,
                const gchar     *signal_name,
                GVariant        *parameters,
                gpointer         user_data)
{
	const gchar *status = NULL;
	gdouble progress = 0.0;

	g_variant_get (parameters, "(sd)", &status, &progress);
	store_print_state (status, progress);
}

static gboolean
store_init (void)
{
	GError *error = NULL;

	if (connection && proxy) {
		return TRUE;
	}

	connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);

	if (!connection) {
		g_critical ("Could not connect to the D-Bus session bus, %s",
		            error ? error->message : "no error given.");
		g_clear_error (&error);
		return FALSE;
	}

	proxy = g_dbus_proxy_new_sync (connection,
	                               G_DBUS_PROXY_FLAGS_NONE,
	                               NULL,
	                               "org.freedesktop.Tracker1",
	                               "/org/freedesktop/Tracker1/Status",
	                               "org.freedesktop.Tracker1.Status",
	                               NULL,
	                               &error);

	if (error) {
		g_critical ("Could not create proxy on the D-Bus session bus, %s",
		            error ? error->message : "no error given.");
		g_clear_error (&error);
		return FALSE;
	}

	g_dbus_connection_signal_subscribe (connection,
	                                    "org.freedesktop.Tracker1",
	                                    "org.freedesktop.Tracker1.Status",
	                                    "Progress",
	                                    "/org/freedesktop/Tracker1/Status",
	                                    NULL,
	                                    G_DBUS_SIGNAL_FLAGS_NONE,
	                                    store_progress,
	                                    NULL,
	                                    NULL);

	return TRUE;
}

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

static int
control_actions (void)
{
	GError *error = NULL;
	GSList *pids;
	GSList *l;
	gchar  *str;

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
	} else if (pause_reason && resume_cookie != -1) {
		g_printerr ("%s\n",
		            _("You can not use miner pause and resume switches together"));
		return EXIT_FAILURE;
	} else if ((pause_reason || resume_cookie != -1) && !miner_name) {
		g_printerr ("%s\n",
		            _("You must provide the miner for pause or resume commands"));
		return EXIT_FAILURE;
	} else if ((!pause_reason && resume_cookie == -1) && miner_name) {
		g_printerr ("%s\n",
		            _("You must provide a pause or resume command for the miner"));
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
				    g_str_has_suffix (basename, "-control") == FALSE &&
				    g_str_has_suffix (basename, "-status-icon") == FALSE) {
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


		g_object_unref (db_config);

		/* Clean up (select_cache_size and update_cache_size don't matter here) */
		if (!tracker_db_manager_init (TRACKER_DB_MANAGER_REMOVE_ALL,
		                              NULL,
		                              FALSE,
		                              100,
		                              100,
		                              NULL,
		                              NULL,
		                              NULL)) {
			return EXIT_FAILURE;
		}

		tracker_db_journal_init (NULL, FALSE);

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

		if (hard_reset || soft_reset) {
			g_print ("%s\n", _("Waiting one second before starting miners…"));

			/* Give a second's grace to avoid race conditions */
			g_usleep (G_USEC_PER_SEC);
		}

		g_print ("%s\n", _("Starting miners…"));

		manager = tracker_miner_manager_new ();
		miners = tracker_miner_manager_get_available (manager);

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
		tracker_miner_manager_reindex_by_mimetype (manager, (GStrv)reindex_mime_types, &error);

		if (error) {
			g_printerr ("%s: %s\n",
			            _("Could not reindex mimetypes"),
			            error->message);
			g_error_free (error);
			return EXIT_FAILURE;
		}

		g_print ("%s\n", _("Reindexing mime types was successful"));

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
			g_printerr ("%s: %s\n",
			            _("Could not (re)index file"),
			            error->message);
			g_error_free (error);
			return EXIT_FAILURE;
		}

		g_print ("%s\n", _("(Re)indexing file was successful"));

		g_object_unref (manager);
		g_object_unref (file);
	}

	if (pause_reason) {
		return miner_pause (miner_name, pause_reason);
	}

	if (resume_cookie != -1) {
		return miner_resume (miner_name, resume_cookie);
	}

	return EXIT_SUCCESS;
}

static int
status_actions (void)
{
	TrackerMinerManager *manager;
	GSList *miners_available;
	GSList *miners_running;
	GSList *l;

	/* --follow implies --status-details */
	if (follow) {
		status_details = TRUE;
	}

	manager = tracker_miner_manager_new ();
	miners_available = tracker_miner_manager_get_available (manager);
	miners_running = tracker_miner_manager_get_running (manager);

	if (list_common_statuses) {
		gint i;

		g_print ("%s:\n", _("Common statuses include"));

		for (i = 0; i < G_N_ELEMENTS(statuses); i++) {
			g_print ("  %s\n", _(statuses[i]));
		}

		return EXIT_SUCCESS;
	}

	if (list_miners_available) {
		gchar *str;

		str = g_strdup_printf (_("Found %d miners installed"), g_slist_length (miners_available));
		g_print ("%s%s\n", str, g_slist_length (miners_available) > 0 ? ":" : "");
		g_free (str);

		for (l = miners_available; l; l = l->next) {
			g_print ("  %s\n", (gchar*) l->data);
		}
	}

	if (list_miners_running) {
		gchar *str;

		str = g_strdup_printf (_("Found %d miners running"), g_slist_length (miners_running));
		g_print ("%s%s\n", str, g_slist_length (miners_running) > 0 ? ":" : "");
		g_free (str);

		for (l = miners_running; l; l = l->next) {
			g_print ("  %s\n", (gchar*) l->data);
		}
	}

	if (list_miners_available || list_miners_running) {
		/* Don't list miners be request AND then anyway later */
		g_slist_foreach (miners_available, (GFunc) g_free, NULL);
		g_slist_free (miners_available);

		g_slist_foreach (miners_running, (GFunc) g_free, NULL);
		g_slist_free (miners_running);

		if (proxy) {
			g_object_unref (proxy);
		}

		return EXIT_SUCCESS;
	}

	if (pause_details) {
		gint paused_miners = 0;

		if (!miners_running) {
			g_print ("%s\n", _("No miners are running"));

			g_slist_foreach (miners_available, (GFunc) g_free, NULL);
			g_slist_free (miners_available);

			g_slist_foreach (miners_running, (GFunc) g_free, NULL);
			g_slist_free (miners_running);

			return EXIT_SUCCESS;
		}

		for (l = miners_running; l; l = l->next) {
			const gchar *name;
			GStrv pause_applications, pause_reasons;
			gint i;

			name = tracker_miner_manager_get_display_name (manager, l->data);

			if (!name) {
				g_critical ("Could not get name for '%s'", (gchar *) l->data);
				continue;
			}

			if (!miner_get_details (manager,
			                        l->data,
			                        NULL,
			                        NULL,
			                        &pause_applications,
			                        &pause_reasons)) {
				continue;
			}

			if (!(*pause_applications) || !(*pause_reasons)) {
				g_strfreev (pause_applications);
				g_strfreev (pause_reasons);
				continue;
			}

			paused_miners++;
			if (paused_miners == 1) {
				g_print ("%s:\n", _("Miners"));
			}

			g_print ("  %s:\n", name);

			for (i = 0; pause_applications[i] != NULL; i++) {
				g_print ("    %s: '%s', %s: '%s'\n",
				         _("Application"),
				         pause_applications[i],
				         _("Reason"),
				         pause_reasons[i]);
			}

			g_strfreev (pause_applications);
			g_strfreev (pause_reasons);
		}

		if (paused_miners < 1) {
			g_print ("%s\n", _("No miners are paused"));
		}

		g_slist_foreach (miners_available, (GFunc) g_free, NULL);
		g_slist_free (miners_available);

		g_slist_foreach (miners_running, (GFunc) g_free, NULL);
		g_slist_free (miners_running);

		return EXIT_SUCCESS;
	}


	if (status_details) {
		/* Work out lengths for output spacing */
		paused_length = strlen (_("PAUSED"));

		for (l = miners_available; l; l = l->next) {
			const gchar *name;

			name = tracker_miner_manager_get_display_name (manager, l->data);
			longest_miner_name_length = MAX (longest_miner_name_length, strlen (name));
		}

		/* Display states */
		g_print ("%s:\n", _("Store"));
		store_init ();
		store_get_and_print_state ();

		g_print ("\n");

		g_print ("%s:\n", _("Miners"));

		for (l = miners_available; l; l = l->next) {
			const gchar *name;
			gboolean is_running;

			name = tracker_miner_manager_get_display_name (manager, l->data);

			if (!name) {
				g_critical ("Could not get name for '%s'", (gchar *) l->data);
				continue;
			}

			is_running = tracker_string_in_gslist (l->data, miners_running);

			if (is_running) {
				GStrv pause_applications, pause_reasons;
				gchar *status = NULL;
				gdouble progress;
				gboolean is_paused;

				if (!miner_get_details (manager,
				                        l->data,
				                        &status,
				                        &progress,
				                        &pause_applications,
				                        &pause_reasons)) {
					continue;
				}

				is_paused = *pause_applications || *pause_reasons;

				miner_print_state (manager, l->data, status, progress, TRUE, is_paused);

				g_strfreev (pause_applications);
				g_strfreev (pause_reasons);
				g_free (status);
			} else {
				miner_print_state (manager, l->data, NULL, 0.0, FALSE, FALSE);
			}
		}

		g_slist_foreach (miners_available, (GFunc) g_free, NULL);
		g_slist_free (miners_available);

		g_slist_foreach (miners_running, (GFunc) g_free, NULL);
		g_slist_free (miners_running);

		if (!follow) {
			/* Do nothing further */
			if (proxy) {
				g_object_unref (proxy);
			}
			g_print ("\n");
			return EXIT_SUCCESS;
		}

		g_print ("Press Ctrl+C to end follow of Tracker state\n");

		g_signal_connect (manager, "miner-progress",
		                  G_CALLBACK (manager_miner_progress_cb), NULL);
		g_signal_connect (manager, "miner-paused",
		                  G_CALLBACK (manager_miner_paused_cb), NULL);
		g_signal_connect (manager, "miner-resumed",
		                  G_CALLBACK (manager_miner_resumed_cb), NULL);

		initialize_signal_handler ();

		miners_progress = g_hash_table_new_full (g_str_hash,
		                                         g_str_equal,
		                                         (GDestroyNotify) g_free,
		                                         (GDestroyNotify) miners_progress_destroy_notify);
		miners_status = g_hash_table_new_full (g_str_hash,
		                                       g_str_equal,
		                                       (GDestroyNotify) g_free,
		                                       (GDestroyNotify) g_free);

		main_loop = g_main_loop_new (NULL, FALSE);
		g_main_loop_run (main_loop);
		g_main_loop_unref (main_loop);

		g_hash_table_unref (miners_progress);
		g_hash_table_unref (miners_status);

		if (proxy) {
			g_object_unref (proxy);
		}

		if (manager) {
			g_object_unref (manager);
		}

		return EXIT_SUCCESS;
	}

	/* All known options have their own exit points */
	g_warn_if_reached();

	return EXIT_FAILURE;
}

int
main (int argc, char **argv)
{
	GOptionContext *context;
	GOptionGroup *control_group;
	GOptionGroup *status_group;

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
	/* Control options */
	control_group = g_option_group_new ("control",
	                                    _("Control options"),
	                                    _("Show control options"),
	                                    NULL,
	                                    NULL);
	g_option_group_add_entries (control_group, control_entries);
	g_option_context_add_group (context, control_group);
	/* Status options */
	status_group = g_option_group_new ("status",
	                                    _("Status options"),
	                                    _("Show status options"),
	                                    NULL,
	                                    NULL);
	g_option_group_add_entries (status_group, status_entries);
	g_option_context_add_group (context, status_group);
	/* Common options */
	g_option_context_add_main_entries (context, common_entries, NULL);

	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	if (print_version) {
		g_print ("\n" ABOUT "\n" LICENSE "\n");
		return EXIT_SUCCESS;
	}

	/* Run control actions? */
	if (CONTROL_OPTION_ENABLED ()) {
		if (STATUS_OPTION_ENABLED ()) {
			g_printerr ("%s\n",
			            _("You can not use status and control arguments together"));
			return EXIT_FAILURE;
		}

		return control_actions ();
	}

	/* Run status actions? */
	if (STATUS_OPTION_ENABLED ()) {
		return status_actions ();
	}

	if (argc > 1) {
		gint i = 1;

		g_printerr ("%s: ",
		            _("Unrecognized options"));
		for (i = 1; i < argc; i++) {
			g_printerr ("'%s'%s",
			            argv[i],
			            i == (argc - 1) ? "\n" : ", ");
		}
	} else {
		g_printerr ("%s\n",
		            _("No options specified"));
	}

	return EXIT_FAILURE;
}
