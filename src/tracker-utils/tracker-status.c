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

#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>

#include <libtracker/tracker.h>

#include <libtracker-miner/tracker-miner-discover.h>

#define TRACKER_TYPE_G_STRV_ARRAY  (dbus_g_type_get_collection ("GPtrArray", G_TYPE_STRV))

/* #define g_marshal_value_peek_boolean(v)  g_value_get_boolean (v) */
/* #define g_marshal_value_peek_string(v)	 (char*) g_value_get_string (v) */
/* #define g_marshal_value_peek_int(v)      g_value_get_int (v) */
/* #define g_marshal_value_peek_double(v)   g_value_get_double (v) */

static GMainLoop *main_loop;

static gboolean   list_miners_running;
static gboolean   list_miners_available;
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
	{ "list-miners-running", 'r', 0, G_OPTION_ARG_NONE, &list_miners_running,
	  N_("List all miners installed"),
	  NULL 
	},
	{ "list-miners-available", 'a', 0, G_OPTION_ARG_NONE, &list_miners_available,
	  N_("List all miners installed"),
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

gint
main (gint argc, gchar *argv[])
{
	GOptionContext *context;
	DBusGProxy *proxy;
	TrackerClient *client;

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	context = g_option_context_new (_("- Report current status"));
	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);
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

	if (list_miners_available) {
		GSList *list, *l;
		gchar *str;
		
		list = tracker_miner_discover_get_available ();

		str = g_strdup_printf (_("Found %d miners installed"), g_slist_length (list));
		g_print ("%s%s\n", str, g_slist_length (list) > 0 ? ":" : "");
		g_free (str);

		for (l = list; l; l = l->next) {
			g_print ("  %s\n", (gchar*) l->data);
			g_free (l->data);
		}
 		
		g_slist_free (list);
	}

	if (list_miners_running) {
		GSList *list, *l;
		gchar *str;
		
		list = tracker_miner_discover_get_running ();

		str = g_strdup_printf (_("Found %d miners running"), g_slist_length (list));
		g_print ("%s%s\n", str, g_slist_length (list) > 0 ? ":" : "");
		g_free (str);

		for (l = list; l; l = l->next) {
			g_print ("  %s\n", (gchar*) l->data);
			g_free (l->data);
		}
 		
		g_slist_free (list);
	}
	
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
