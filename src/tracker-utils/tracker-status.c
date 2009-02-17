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
#include <time.h>
#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>

#include <libtracker/tracker.h>

#define DETAIL_MAX_WIDTH 30

#define g_marshal_value_peek_boolean(v)  g_value_get_boolean (v)
#define g_marshal_value_peek_string(v)	 (char*) g_value_get_string (v)

static gchar    *last_state;   
static gboolean  follow;
static gboolean  detailed;

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
		     gboolean	  is_manual_paused,
		     gboolean	  is_battery_paused,
		     gboolean	  is_io_paused,
		     gboolean	  is_indexing_enabled,
		     gpointer     user_data)
{
	gchar *str;

	str = g_strdup_printf (_( "Tracker status changed from '%s' --> '%s'"), 
			       last_state, 
			       state);
	g_print ("%s\n", str);
	g_free (str);

	if (detailed) {
		g_print ("  %-*.*s: %s\n"
			 "  %-*.*s: %s\n"
			 "  %-*.*s: %s\n"
			 "  %-*.*s: %s\n"
			 "  %-*.*s: %s\n"
			 "  %-*.*s: %s\n"
			 "\n",
			 DETAIL_MAX_WIDTH, DETAIL_MAX_WIDTH,
			 _("Initial index"),
			 initial_index ? _("yes") : _("no"),
			 DETAIL_MAX_WIDTH, DETAIL_MAX_WIDTH,
			 _("In merge"),
			 in_merge ? _("yes") : _("no"),
			 DETAIL_MAX_WIDTH, DETAIL_MAX_WIDTH,
			 _("Is paused manually"),
			 is_manual_paused ? _("yes") : _("no"),
			 DETAIL_MAX_WIDTH, DETAIL_MAX_WIDTH,
			 _("Is paused for low battery"),
			 is_battery_paused ? _("yes") : _("no"),
			 DETAIL_MAX_WIDTH, DETAIL_MAX_WIDTH,
			 _("Is paused for IO"),
			 is_io_paused ? _("yes") : _("no"),
			 DETAIL_MAX_WIDTH, DETAIL_MAX_WIDTH,
			 _("Is indexing enabled"),
			 is_indexing_enabled ? _("yes") : _("no"));
	}

	/* Remember last state */
	g_free (last_state);
	last_state = g_strdup (state);
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

gint
main (gint argc, gchar *argv[])
{
	GOptionContext *context;
	GError	       *error = NULL;
	TrackerClient  *client;
	gchar 	       *state;

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

	if (follow) {
		GMainLoop *main_loop;
		DBusGProxy *proxy;

		proxy = client->proxy;

		/* Remember */
		last_state = g_strdup (state);

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
			
		main_loop = g_main_loop_new (NULL, FALSE);
		g_main_loop_run (main_loop);
		g_object_unref (main_loop);

		g_free (last_state);
	}

	tracker_disconnect (client);

	return EXIT_SUCCESS;
}
