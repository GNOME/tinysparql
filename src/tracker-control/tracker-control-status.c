/*
 * Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
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

#include <glib/gi18n.h>

#include <libtracker-common/tracker-common.h>
#include <libtracker-miner/tracker-miner.h>

#include "tracker-control.h"

static GDBusConnection *connection = NULL;
static GDBusProxy *proxy = NULL;
static GMainLoop *main_loop;
static GHashTable *miners_progress;
static GHashTable *miners_status;
static gint longest_miner_name_length = 0;
static gint paused_length = 0;

/* Note:
 * Every time a new option is added, make sure it is considered in the
 * 'STATUS_OPTIONS_ENABLED' macro below
 */
static gboolean status;
static gboolean follow;
static gboolean list_common_statuses;

#define STATUS_OPTIONS_ENABLED() \
	(status || follow || list_common_statuses)

/* Make sure our statuses are translated (most from libtracker-miner) */
static const gchar *statuses[8] = {
	N_("Unavailable"), /* generic */
	N_("Initializing"),
	N_("Processing…"),
	N_("Fetching…"), /* miner/rss */
	N_("Crawling single directory '%s'"),
	N_("Crawling recursively directory '%s'"),
	N_("Paused"),
	N_("Idle")
};

static GOptionEntry entries[] = {
	{ "status", 'S', 0, G_OPTION_ARG_NONE, &status,
	  N_("Show current status"),
	  NULL
	},
	{ "follow", 'F', 0, G_OPTION_ARG_NONE, &follow,
	  N_("Follow status changes as they happen"),
	  NULL
	},
	{ "list-common-statuses", 'C', 0, G_OPTION_ARG_NONE, &list_common_statuses,
	  N_("List common statuses for miners and the store"),
	  NULL
	},
	{ NULL }
};

gboolean
tracker_control_status_options_enabled (void)
{
	return STATUS_OPTIONS_ENABLED ();
}

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

	now = time ((time_t *) NULL);
	local_time = localtime (&now);
	len = strftime (time_str,
	                sizeof (time_str) - 1,
	                "%d %b %Y, %H:%M:%S:",
	                local_time);
	time_str[len] = '\0';

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
	gchar time_str[64];
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

	if (status) {
		gchar *operation = NULL;
		gchar *operation_status = NULL;
		gchar *progress_str;

		if (strstr (status, "-")) {
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

		if (progress > 0.0 && progress < 1.0) {
			progress_str = g_strdup_printf ("%-3.0f%%", progress * 100);
		} else {
			progress_str = g_strdup_printf ("✓   ");
		}

		g_print ("%s  %s  %-*.*s    %s %s\n",
		         time_str,
		         progress_str ? progress_str : "    ",
		         longest_miner_name_length + paused_length,
		         longest_miner_name_length + paused_length,
		         (operation ? _(operation) : _(status)),
		         operation_status ? "-" : "",
		         operation_status ? _(operation_status) : "");

		g_free (progress_str);
		g_free (operation);
		g_free (operation_status);
	} else {
		g_print ("%s        %s\n",
		         time_str,
		         _("Unavailable"));
	}
}

static void
store_get_and_print_state (void)
{
	GVariant *v_status, *v_progress;
	const gchar *status = NULL;
	gdouble progress = -1.0;
	GError *error = NULL;
	gchar *owner;

	owner = g_dbus_proxy_get_name_owner (proxy);
	if (!owner) {
		/* Name is not owned yet, store is not running */
		store_print_state (NULL, -1);
		return;
	}
	g_free (owner);

	/* Status */
	v_status = g_dbus_proxy_call_sync (proxy,
	                                   "GetStatus",
	                                   NULL,
	                                   G_DBUS_CALL_FLAGS_NONE,
	                                   -1,
	                                   NULL,
	                                   &error);

	if (!v_status || error) {
		g_critical ("Could not retrieve tracker-store status: %s",
		            error ? error->message : "no error given");
		g_clear_error (&error);
		return;
	}

	g_variant_get (v_status, "(&s)", &status);

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
	                               G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
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

void
tracker_control_status_run_default (void)
{
	/* Enable status output in the default run */
	status = TRUE;

	tracker_control_status_run ();
}

gint
tracker_control_status_run (void)
{
	TrackerMinerManager *manager;

	/* --follow implies --status */
	if (follow) {
		status = TRUE;
	}

	if (list_common_statuses) {
		gint i;

		g_print ("%s:\n", _("Common statuses include"));

		for (i = 0; i < G_N_ELEMENTS (statuses); i++) {
			g_print ("  %s\n", _(statuses[i]));
		}

		return EXIT_SUCCESS;
	}

	if (status) {
		GError *error = NULL;
		GSList *miners_available;
		GSList *miners_running;
		GSList *l;

		/* Don't auto-start the miners here */
		manager = tracker_miner_manager_new_full (FALSE, &error);
		if (!manager) {
			g_printerr (_("Could not get status, manager could not be created, %s"),
			            error ? error->message : "unknown error");
			g_printerr ("\n");
			g_clear_error (&error);
			return EXIT_FAILURE;
		}

		miners_available = tracker_miner_manager_get_available (manager);
		miners_running = tracker_miner_manager_get_running (manager);

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
	g_warn_if_reached ();

	return EXIT_FAILURE;
}

GOptionGroup *
tracker_control_status_get_option_group (void)
{
	GOptionGroup *group;

	/* Status options */
	group = g_option_group_new ("status",
	                            _("Status options"),
	                            _("Show status options"),
	                            NULL,
	                            NULL);
	g_option_group_add_entries (group, entries);

	return group;
}
