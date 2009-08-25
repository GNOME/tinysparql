/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
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

#include <signal.h>
#include <locale.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gi18n.h>

#include <libtracker/tracker.h>

#include <libtracker-common/tracker-type-utils.h>

#include <libtracker-miner/tracker-miner.h>
#include <libtracker-miner/tracker-miner-discover.h>

#include "tracker-miner-client.h"

#define TRACKER_TYPE_G_STRV_ARRAY  (dbus_g_type_get_collection ("GPtrArray", G_TYPE_STRV))

/* #define g_marshal_value_peek_boolean(v)  g_value_get_boolean (v) */
/* #define g_marshal_value_peek_string(v)	 (char*) g_value_get_string (v) */
/* #define g_marshal_value_peek_int(v)      g_value_get_int (v) */
/* #define g_marshal_value_peek_double(v)   g_value_get_double (v) */

static GMainLoop *main_loop;

static gboolean   list_miners_running;
static gboolean   list_miners_available;
static gchar     *miner_name;
static gchar     *pause_reason;
static gint       resume_cookie = -1;
static gboolean   follow;
static gboolean   detailed;

static GOptionEntry entries[] = {
	{ "follow", 'f', 0, G_OPTION_ARG_NONE, &follow,
	  N_("Follow status changes as they happen"),
	  NULL 
	},
	{ "detailed", 'd', 0, G_OPTION_ARG_NONE, &detailed,
	  N_("Include details with state updates (only applies to --follow)"),
	  NULL 
	},
	{ "list-miners-running", 'l', 0, G_OPTION_ARG_NONE, &list_miners_running,
	  N_("List all miners installed"),
	  NULL 
	},
	{ "list-miners-available", 'a', 0, G_OPTION_ARG_NONE, &list_miners_available,
	  N_("List all miners installed"),
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

static gchar *
get_dbus_name (const gchar *name_provided)
{
	gchar *name;

	if (g_str_has_prefix (name_provided, TRACKER_MINER_DBUS_NAME_PREFIX)) {
		name = g_strdup (name_provided);
	} else {
		name = g_strconcat (TRACKER_MINER_DBUS_NAME_PREFIX, 
				    name_provided, 
				    NULL);
	}

	return name;
}

static gchar *
get_dbus_path (const gchar *name)
{
	GStrv strv;
	gchar *path;
	gchar *str;

	/* Create path from name */
	strv = g_strsplit (name, ".", -1);
	str = g_strjoinv ("/", strv);
	g_strfreev (strv);
	path = g_strconcat ("/", str, NULL);
	g_free (str);

	return path;
}

static DBusGProxy *
get_dbus_proxy (const gchar *name)
{
	GError *error = NULL;
	DBusGConnection *connection;
	DBusGProxy *proxy;
	gchar *path;

	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	
	if (!connection) {
		g_printerr ("%s. %s\n",
			    _("Could not connect to the DBus session bus"),
			    error ? error->message : _("No error given"));
		g_clear_error (&error);
		return NULL;
	}
	
	path = get_dbus_path (name);
	proxy = dbus_g_proxy_new_for_name (connection,
					   name,
					   path,
					   TRACKER_MINER_DBUS_INTERFACE);
	g_free (path);
	
	if (!proxy) {
		gchar *str;

		str = g_strdup_printf (_("Could not DBusGProxy for that miner: %s"),
				       name);
		g_printerr ("%s\n", str);
		g_free (str);

		return NULL;
	}

	return proxy;
}

static int
miner_pause (const gchar *miner,
	     const gchar *reason)
{
	GError *error = NULL;
	DBusGProxy *proxy;
	gchar *name;
	gchar *str;
	gint cookie;
	
	name = get_dbus_name (miner);
	
	proxy = get_dbus_proxy (name);
	if (!proxy) {
		g_free (name);
		return EXIT_FAILURE;
	}

	str = g_strdup_printf (_("Attempting to pause miner '%s' with reason '%s'"),
			       name,
			       reason);
	g_print ("%s\n", str);
	g_free (str);
	
	if (!org_freedesktop_Tracker_Miner_pause (proxy, 
						  g_get_application_name (),
						  reason,
						  &cookie, 
						  &error)) {
		str = g_strdup_printf (_("Could not pause miner: %s"),
				       name);
		g_printerr ("  %s. %s\n", 
			    str,
			    error ? error->message : _("No error given"));

		g_free (str);
		g_clear_error (&error);
		g_free (name);
		
		return EXIT_FAILURE;
	}
	
	str = g_strdup_printf (_("Cookie is %d"), cookie);
	g_print ("  %s\n", str);
	g_free (str);
	g_free (name);
	
	return EXIT_SUCCESS;
}

static int
miner_resume (const gchar *miner,
	      gint         cookie)
{
	GError *error = NULL;
	DBusGProxy *proxy;
	gchar *name;
	gchar *str;
	
	name = get_dbus_name (miner);
	
	proxy = get_dbus_proxy (name);
	if (!proxy) {
		g_free (name);
		return EXIT_FAILURE;
	}

	str = g_strdup_printf (_("Attempting to resume miner %s with cookie %d"), 
			       name,
			       cookie);
	g_print ("%s\n", str);
	g_free (str);
	
	if (!org_freedesktop_Tracker_Miner_resume (proxy, 
						   cookie, 
						   &error)) {
		str = g_strdup_printf (_("Could not resume miner: %s"),
				       name);
		g_printerr ("  %s. %s\n", 
			    str,
			    error ? error->message : _("No error given"));

		g_free (str);
		g_clear_error (&error);
		g_free (name);
		
		return EXIT_FAILURE;
	}
	
	g_print ("  %s\n", _("Done"));
	g_free (name);
	
	return EXIT_SUCCESS;
}

static gboolean
miner_get_details (const gchar  *miner,
		   gchar       **status,
		   gdouble      *progress)
{
	GError *error = NULL;
	DBusGProxy *proxy;
	gchar *name;

	*status = NULL;
	*progress = 0.0;
	
	name = get_dbus_name (miner);
	
	proxy = get_dbus_proxy (name);
	if (!proxy) {
		g_free (name);
		return FALSE;
	}
	
	if (!org_freedesktop_Tracker_Miner_get_status (proxy, 
						       status,
						       &error)) {
		gchar *str;

		str = g_strdup_printf (_("Could not get status from miner: %s"),
				       name);
		g_printerr ("  %s. %s\n", 
			    str,
			    error ? error->message : _("No error given"));

		g_free (str);
		g_clear_error (&error);
		g_free (name);
		
		return FALSE;
	}

	if (!org_freedesktop_Tracker_Miner_get_progress (proxy, 
							 progress,
							 &error)) {
		gchar *str;

		str = g_strdup_printf (_("Could not get progress from miner: %s"),
				       name);
		g_printerr ("  %s. %s\n", 
			    str,
			    error ? error->message : _("No error given"));

		g_free (str);
		g_clear_error (&error);
		g_free (name);
		
		return FALSE;
	}
	
	g_free (name);
	
	return TRUE;
}

gint
main (gint argc, gchar *argv[])
{
	GOptionContext *context;
	DBusGProxy *proxy;
	TrackerClient *client;
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

	g_option_context_free (context);

	g_type_init ();
	
	if (!g_thread_supported ()) {
		g_thread_init (NULL);
	}

	client = tracker_connect (FALSE, -1);

	if (!client) {
		g_printerr ("%s\n",
			    _("Could not establish a DBus connection to Tracker"));

		return EXIT_FAILURE;
	}

	miners_available = tracker_miner_discover_get_available ();
	miners_running = tracker_miner_discover_get_running ();

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
		return miner_pause (miner_name, pause_reason);
	}

	if (resume_cookie != -1) {
		return miner_resume (miner_name, resume_cookie);
	}

	if (list_miners_available || list_miners_running) {
		/* Don't list miners be request AND then anyway later */
		g_slist_foreach (miners_available, (GFunc) g_free, NULL);
		g_slist_free (miners_available);

		return EXIT_SUCCESS;
	}

	/* Show status of all miners */
	g_print ("%s: (%s)\n", 
		 _("Miners"),
		 _("The 'x' indicates the miner is running"));
	
	for (l = miners_available; l; l = l->next) {
		gboolean is_running;

		is_running = tracker_string_in_gslist (l->data, miners_running);

		if (is_running) {
			gchar *status = NULL;
			gdouble progress;

			if (!miner_get_details (l->data, &status, &progress)) {
				continue;
			}

			g_print ("  [x] %s - status: %s, progress: %.0f%%\n", 
				 (gchar*) l->data,
				 status ? status : "Unknown",
				 progress * 100);

			g_free (status);
		} else {
			g_print ("  [ ] %s\n", 
				 (gchar*) l->data);
		}
	}

	g_slist_foreach (miners_available, (GFunc) g_free, NULL);
	g_slist_free (miners_available);

	if (!follow) {
		GError *error = NULL;
		gchar *state;

 		/* state = tracker_get_status (client, &error); */
		state = "Idle";
		
		if (error) {
			g_printerr ("%s, %s\n",
				    _("Could not get Tracker status"),
				    error->message);
			g_error_free (error);
			
			return EXIT_FAILURE;
		}


		if (state) {
			gchar *str;
			
			str = g_strdup_printf (_("Tracker status is '%s'"), state);
			g_print ("%s\n", str);
			g_free (str);
		}

		tracker_disconnect (client);

		return EXIT_SUCCESS;
	}

	g_print ("Press Ctrl+C to end follow of Tracker state\n");
	
	proxy = client->proxy_statistics;
	
	/* Set signal handlers */
	/* dbus_g_object_register_marshaller (tracker_VOID__STRING_BOOLEAN_BOOLEAN_BOOLEAN_BOOLEAN_BOOLEAN, */
	/* 				   G_TYPE_NONE, */
	/* 				   G_TYPE_STRING, */
	/* 				   G_TYPE_BOOLEAN, */
	/* 				   G_TYPE_BOOLEAN, */
	/* 				   G_TYPE_BOOLEAN, */
	/* 				   G_TYPE_BOOLEAN, */
	/* 				   G_TYPE_BOOLEAN, */
	/* 				   G_TYPE_BOOLEAN, */
	/* 				   G_TYPE_INVALID); */
	/* dbus_g_object_register_marshaller (tracker_VOID__STRING_STRING_INT_INT_INT_DOUBLE, */
	/* 				   G_TYPE_NONE, */
	/* 				   G_TYPE_STRING, */
	/* 				   G_TYPE_STRING, */
	/* 				   G_TYPE_INT, */
	/* 				   G_TYPE_INT, */
	/* 				   G_TYPE_INT, */
	/* 				   G_TYPE_DOUBLE, */
	/* 				   G_TYPE_INVALID); */
	
	/* dbus_g_proxy_add_signal (proxy, */
	/* 			 "IndexStateChange", */
	/* 				 G_TYPE_STRING, */
	/* 			 G_TYPE_BOOLEAN, */
	/* 			 G_TYPE_BOOLEAN, */
	/* 			 G_TYPE_BOOLEAN, */
	/* 			 G_TYPE_BOOLEAN, */
	/* 			 G_TYPE_BOOLEAN, */
	/* 			 G_TYPE_BOOLEAN, */
	/* 			 G_TYPE_INVALID); */
	/* dbus_g_proxy_add_signal (proxy, */
	/* 			 "IndexProgress", */
	/* 			 G_TYPE_STRING, */
	/* 			 G_TYPE_STRING, */
	/* 			 G_TYPE_INT, */
	/* 			 G_TYPE_INT, */
	/* 			 G_TYPE_INT, */
	/* 			 G_TYPE_DOUBLE, */
	/* 			 G_TYPE_INVALID); */
	/* dbus_g_proxy_add_signal (proxy, */
	/* 			 "ServiceStatisticsUpdated", */
	/* 			 TRACKER_TYPE_G_STRV_ARRAY, */
	/* 			 G_TYPE_INVALID); */
	
	/* dbus_g_proxy_connect_signal (proxy, */
	/* 			     "IndexStateChange", */
	/* 			     G_CALLBACK (index_state_changed), */
	/* 			     NULL, */
	/* 			     NULL); */
	/* dbus_g_proxy_connect_signal (proxy, */
	/* 			     "IndexProgress", */
	/* 			     G_CALLBACK (index_progress_changed), */
	/* 			     NULL, */
	/* 			     NULL); */
	/* dbus_g_proxy_connect_signal (proxy, */
	/* 			     "ServiceStatisticsUpdated", */
	/* 			     G_CALLBACK (index_service_stats_updated), */
	/* 			     NULL, */
	/* 			     NULL); */
	
	initialize_signal_handler ();
	
	main_loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (main_loop);
	g_main_loop_unref (main_loop);
	
	tracker_disconnect (client);

	return EXIT_SUCCESS;
}
