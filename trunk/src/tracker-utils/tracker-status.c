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

#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>

#include <libtracker/tracker.h>

#define DETAIL_MAX_WIDTH 30

#define g_marshal_value_peek_boolean(v)  g_value_get_boolean (v)
#define g_marshal_value_peek_string(v)	 (char*) g_value_get_string (v)

static GMainLoop *main_loop;
static gchar     *last_state;   
static gboolean   last_initial_index;
static gboolean   last_in_merge;
static gboolean   last_is_paused_manually;
static gboolean   last_is_paused_for_bat;
static gboolean   last_is_paused_for_io;
static gboolean   last_is_indexing_enabled;

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
	{ NULL }
};

static void
index_state_changed (DBusGProxy  *proxy,
		     const gchar *state,
		     gboolean	  initial_index,
		     gboolean	  in_merge,
		     gboolean	  is_paused_manually,
		     gboolean	  is_paused_for_bat,
		     gboolean	  is_paused_for_io,
		     gboolean	  is_indexing_enabled,
		     gpointer     user_data)
{
	static gboolean first_change = TRUE;
	gchar *str;
	
	str = g_strdup_printf (_( "Tracker status changed from '%s' --> '%s'"), 
			       last_state ? last_state : _("None"), 
			       state);
	g_print ("%s\n", str);
	g_free (str);

	if (detailed) {
		if (first_change || last_initial_index != initial_index) {
			g_print ("  %-*.*s: %s\n",
				 DETAIL_MAX_WIDTH, DETAIL_MAX_WIDTH,
				 _("Initial index"),
				 initial_index ? _("yes") : _("no"));			
		}
		
		if (first_change || last_in_merge != in_merge) {
			g_print ("  %-*.*s: %s\n",
				 DETAIL_MAX_WIDTH, DETAIL_MAX_WIDTH,
				 _("In merge"),
				 in_merge ? _("yes") : _("no"));
		}

		if (first_change || last_is_paused_manually != is_paused_manually) {
			g_print ("  %-*.*s: %s\n",
				 DETAIL_MAX_WIDTH, DETAIL_MAX_WIDTH,
				 _("Is paused manually"),
				 is_paused_manually ? _("yes") : _("no"));
		}

		if (first_change || last_is_paused_for_bat != is_paused_for_bat) {
			g_print ("  %-*.*s: %s\n",
				 DETAIL_MAX_WIDTH, DETAIL_MAX_WIDTH,
				 _("Is paused for low battery"),
				 is_paused_for_bat ? _("yes") : _("no"));
		}

		if (first_change || last_is_paused_for_io != is_paused_for_io) {
			g_print ("  %-*.*s: %s\n",
				 DETAIL_MAX_WIDTH, DETAIL_MAX_WIDTH,
				 _("Is paused for IO"),
				 is_paused_for_io ? _("yes") : _("no"));
		}

		if (first_change || last_is_indexing_enabled != is_indexing_enabled) {
			g_print ("  %-*.*s: %s\n",
				 DETAIL_MAX_WIDTH, DETAIL_MAX_WIDTH,
				 _("Is indexing enabled"),
				 is_indexing_enabled ? _("yes") : _("no"));
		}
	}

	/* Remember last details so we don't spam the same crap each
	 * time to console, only updates.
	 */
	g_free (last_state);
	last_state = g_strdup (state);

	last_initial_index = initial_index;
	last_in_merge = in_merge;
	last_is_paused_manually = is_paused_manually;
	last_is_paused_for_bat = is_paused_for_bat;
	last_is_paused_for_io = is_paused_for_io;
	last_is_indexing_enabled = is_indexing_enabled;
	
	first_change = FALSE;
}

/* Taken from tracker-applet */
static void
tracker_VOID__STRING_BOOLEAN_BOOLEAN_BOOLEAN_BOOLEAN_BOOLEAN (GClosure	   *closure,
							      GValue	   *return_value,
							      guint	    n_param_values,
							      const GValue *param_values,
							      gpointer	    invocation_hint,
							      gpointer	    marshal_data)
{
	typedef void (*GMarshalFunc_VOID__STRING_BOOLEAN_BOOLEAN_BOOLEAN_BOOLEAN_BOOLEAN) (gpointer	  data1,
											   gpointer	  arg_1,
											   gboolean	  arg_2,
											   gboolean	  arg_3,
											   gboolean	  arg_4,
											   gboolean	  arg_5,
											   gboolean	  arg_6,
											   gboolean	  arg_7,
											   gpointer	  data2);
	register GMarshalFunc_VOID__STRING_BOOLEAN_BOOLEAN_BOOLEAN_BOOLEAN_BOOLEAN callback;
	register GCClosure *cc = (GCClosure*) closure;
	register gpointer data1, data2;
	
	g_return_if_fail (n_param_values == 8);
	
	if (G_CCLOSURE_SWAP_DATA (closure))
	{
		data1 = closure->data;
		data2 = g_value_peek_pointer (param_values + 0);
	}
	else
	{
		data1 = g_value_peek_pointer (param_values + 0);
		data2 = closure->data;
	}
	callback = (GMarshalFunc_VOID__STRING_BOOLEAN_BOOLEAN_BOOLEAN_BOOLEAN_BOOLEAN) (marshal_data ? marshal_data : cc->callback);
	
	callback (data1,
		  g_marshal_value_peek_string (param_values + 1),
		  g_marshal_value_peek_boolean (param_values + 2),
		  g_marshal_value_peek_boolean (param_values + 3),
		  g_marshal_value_peek_boolean (param_values + 4),
		  g_marshal_value_peek_boolean (param_values + 5),
		  g_marshal_value_peek_boolean (param_values + 6),
		  g_marshal_value_peek_boolean (param_values + 7),
		  data2);
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

	default:
		if (g_strsignal (signo)) {
			g_print ("\n");
			g_print ("Received signal:%d->'%s'",
				 signo,
				 g_strsignal (signo));
		}
		break;
	}
}

static void
initialize_signal_handler (void)
{
#ifndef G_OS_WIN32
	struct sigaction act;
	sigset_t	 empty_mask;

	sigemptyset (&empty_mask);
	act.sa_handler = signal_handler;
	act.sa_mask    = empty_mask;
	act.sa_flags   = 0;

	sigaction (SIGTERM, &act, NULL);
	sigaction (SIGINT,  &act, NULL);
	sigaction (SIGHUP,  &act, NULL);
#endif /* G_OS_WIN32 */
}

gint
main (gint argc, gchar *argv[])
{
	GOptionContext *context;
	TrackerClient  *client;

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	context = g_option_context_new (_("- Report current status"));
	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	client = tracker_connect (FALSE);

	if (!client) {
		g_printerr ("%s\n",
			    _("Could not establish a DBus connection to Tracker"));

		return EXIT_FAILURE;
	}

	if (!follow) {
		GError *error = NULL;
		gchar *state;

		state = tracker_get_status (client, &error);
		
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
	} else {
		DBusGProxy *proxy;

		g_print ("Press Ctrl+C to end follow of Tracker state\n");

		proxy = client->proxy;

		/* Set signal handlers */
		dbus_g_object_register_marshaller (tracker_VOID__STRING_BOOLEAN_BOOLEAN_BOOLEAN_BOOLEAN_BOOLEAN,
						   G_TYPE_NONE,
						   G_TYPE_STRING,
						   G_TYPE_BOOLEAN,
						   G_TYPE_BOOLEAN,
						   G_TYPE_BOOLEAN,
						   G_TYPE_BOOLEAN,
						   G_TYPE_BOOLEAN,
						   G_TYPE_BOOLEAN,
						   G_TYPE_INVALID);
			
		dbus_g_proxy_add_signal (proxy,
					 "IndexStateChange",
					 G_TYPE_STRING,
					 G_TYPE_BOOLEAN,
					 G_TYPE_BOOLEAN,
					 G_TYPE_BOOLEAN,
					 G_TYPE_BOOLEAN,
					 G_TYPE_BOOLEAN,
					 G_TYPE_BOOLEAN,
					 G_TYPE_INVALID);
				
		dbus_g_proxy_connect_signal (proxy,
					     "IndexStateChange",
					     G_CALLBACK (index_state_changed),
					     NULL,
					     NULL);

		initialize_signal_handler ();
			
		main_loop = g_main_loop_new (NULL, FALSE);
		g_main_loop_run (main_loop);
		g_main_loop_unref (main_loop);

		g_free (last_state);
	}

	tracker_disconnect (client);

	return EXIT_SUCCESS;
}
