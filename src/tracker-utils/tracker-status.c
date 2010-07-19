/*
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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

#include <signal.h>
#include <locale.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gi18n.h>

#include <libtracker-sparql/tracker-sparql.h>
#include <libtracker-common/tracker-common.h>
#include <libtracker-miner/tracker-miner.h>

#include "tracker-status-client.h"

#define ABOUT	  \
	"Tracker " PACKAGE_VERSION "\n"

#define LICENSE	  \
	"This program is free software and comes without any warranty.\n" \
	"It is licensed under version 2 or later of the General Public " \
	"License which can be viewed at:\n" \
	"\n" \
	"  http://www.gnu.org/licenses/gpl.txt\n"

static DBusGConnection *connection = NULL;
static DBusGProxy *proxy = NULL;

static GMainLoop *main_loop;
static GHashTable *miners_progress;
static GHashTable *miners_status;
static gint longest_miner_name_length = 0;
static gint paused_length = 0;

static gboolean list_common_statuses;
static gboolean list_miners_running;
static gboolean list_miners_available;
static gboolean pause_details;
static gchar *miner_name;
static gchar *pause_reason;
static gint resume_cookie = -1;
static gboolean follow;
static gboolean detailed;
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

static GOptionEntry entries[] = {
	{ "follow", 'f', 0, G_OPTION_ARG_NONE, &follow,
	  N_("Follow status changes as they happen"),
	  NULL
	},
	{ "detailed", 'd', 0, G_OPTION_ARG_NONE, &detailed,
	  N_("Include details with state updates (only applies to --follow)"),
	  NULL
	},
	{ "list-common-statuses", 's', 0, G_OPTION_ARG_NONE, &list_common_statuses,
	  N_("List common statuses for miners and the store"),
	  NULL
	},
	{ "list-miners-running", 'l', 0, G_OPTION_ARG_NONE, &list_miners_running,
	  N_("List all miners currently running"),
	  NULL
	},
	{ "list-miners-available", 'a', 0, G_OPTION_ARG_NONE, &list_miners_available,
	  N_("List all miners installed"),
	  NULL
	},
	{ "pause-details", 'i', 0, G_OPTION_ARG_NONE, &pause_details,
	  N_("List pause reasons and applications for a miner"),
	  NULL
	},
	{ "miner", 'm', 0, G_OPTION_ARG_STRING, &miner_name,
	  N_("Miner to use with other commands (you can use suffixes, e.g. FS or Applications)"),
	  N_("MINER")
	},
	{ "pause", 'p', 0, G_OPTION_ARG_STRING, &pause_reason,
	  N_("Pause a miner (you must use this with --miner)"),
	  N_("REASON")
	},
	{ "resume", 'r', 0, G_OPTION_ARG_INT, &resume_cookie,
	  N_("Resume a miner (you must use this with --miner)"),
	  N_("COOKIE")
	},
	{ "version", 'V', 0, G_OPTION_ARG_NONE, &print_version,
	  N_("Print version"),
	  NULL
	},
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
miner_pause (TrackerMinerManager *manager,
             const gchar         *miner,
             const gchar         *reason)
{
	gchar *str;
	gint cookie;

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

	return EXIT_SUCCESS;
}

static int
miner_resume (TrackerMinerManager *manager,
              const gchar         *miner,
              gint                 cookie)
{
	gchar *str;

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
store_print_state (void)
{
	GError *error = NULL;
	gdouble progress;
	gchar *status;
	gchar time_str[64];
	gchar *progress_str;

	org_freedesktop_Tracker1_Status_get_progress (proxy, &progress, &error);

	if (error) {
		g_critical ("Could not retrieve tracker-store progress: %s", error->message);
		return;
	}

	org_freedesktop_Tracker1_Status_get_status (proxy, &status, &error);

	if (error) {
		g_critical ("Could not retrieve tracker-store status: %s", error->message);
		return;
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
	         _("Journal replay"),
	         status ? "-" : "",
	         status ? _(status) : "");
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
store_progress_cb (TrackerMinerManager *manager,
                   const gchar         *status,
                   gdouble              progress)
{
	store_print_state ();
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

static gboolean
init_store_proxy (void)
{
	GError *error = NULL;

	if (connection && proxy) {
		return TRUE;
	}

	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

	if (error) {
		g_critical ("Could not connect to the D-Bus session bus, %s",
		            error ? error->message : "no error given.");
		g_clear_error (&error);
		return FALSE;
	}

	proxy = dbus_g_proxy_new_for_name (connection,
	                                   "org.freedesktop.Tracker1",
	                                   "/org/freedesktop/Tracker1/Status",
	                                   "org.freedesktop.Tracker1.Status");

	dbus_g_proxy_add_signal (proxy,
	                         "Progress",
	                         G_TYPE_STRING,
	                         G_TYPE_DOUBLE,
	                         G_TYPE_INVALID);
	return TRUE;
}

gint
main (gint argc, gchar *argv[])
{
	TrackerMinerManager *manager;
	GOptionContext *context;
	TrackerSparqlConnection *connection;
	GSList *miners_available;
	GSList *miners_running;
	GSList *l;

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	context = g_option_context_new (_("- Monitor and control status"));
	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);

	if (print_version) {
		g_print ("\n" ABOUT "\n" LICENSE "\n");
		g_option_context_free (context);

		return EXIT_SUCCESS;
	}

	if (pause_reason && resume_cookie != -1) {
		gchar *help;

		g_printerr ("%s\n\n",
		            _("You can not use miner pause and resume switches together"));

		help = g_option_context_get_help (context, TRUE, NULL);
		g_option_context_free (context);
		g_printerr ("%s", help);
		g_free (help);

		return EXIT_FAILURE;
	}

	if ((pause_reason || resume_cookie != -1) && !miner_name) {
		gchar *help;

		g_printerr ("%s\n\n",
		            _("You must provide the miner for pause or resume commands"));

		help = g_option_context_get_help (context, TRUE, NULL);
		g_option_context_free (context);
		g_printerr ("%s", help);
		g_free (help);

		return EXIT_FAILURE;
	}

	if ((!pause_reason && resume_cookie == -1) && miner_name) {
		gchar *help;

		g_printerr ("%s\n\n",
		            _("You must provide a pause or resume command for the miner"));

		help = g_option_context_get_help (context, TRUE, NULL);
		g_option_context_free (context);
		g_printerr ("%s", help);
		g_free (help);

		return EXIT_FAILURE;
	}

	g_option_context_free (context);

	g_type_init ();

	if (!g_thread_supported ()) {
		g_thread_init (NULL);
	}

	connection = tracker_sparql_connection_get (NULL);

	if (!connection) {
		g_printerr ("%s\n",
		            _("Could not establish a D-Bus connection to Tracker"));

		return EXIT_FAILURE;
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

	if (pause_reason) {
		return miner_pause (manager, miner_name, pause_reason);
	}

	if (resume_cookie != -1) {
		return miner_resume (manager, miner_name, resume_cookie);
	}

	if (list_miners_available || list_miners_running) {
		/* Don't list miners be request AND then anyway later */
		g_slist_foreach (miners_available, (GFunc) g_free, NULL);
		g_slist_free (miners_available);

		g_slist_foreach (miners_running, (GFunc) g_free, NULL);
		g_slist_free (miners_running);

		g_object_unref (connection);
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

	/* Work out lengths for output spacing */
	paused_length = strlen (_("PAUSED"));

	for (l = miners_available; l; l = l->next) {
		const gchar *name;

		name = tracker_miner_manager_get_display_name (manager, l->data);
		longest_miner_name_length = MAX (longest_miner_name_length, strlen (name));
	}

	/* Display states */
	g_print ("%s:\n", _("Store"));
	init_store_proxy ();
	store_print_state ();

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
		g_object_unref (connection);
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
	dbus_g_proxy_connect_signal (proxy, "Progress",
	                             G_CALLBACK (store_progress_cb),
	                             NULL, NULL);

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

	g_object_unref (connection);
	g_object_unref (manager);

	return EXIT_SUCCESS;
}
