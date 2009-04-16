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

#define TRACKER_TYPE_G_STRV_ARRAY  (dbus_g_type_get_collection ("GPtrArray", G_TYPE_STRV))

#define DETAIL_MAX_WIDTH 30

#define g_marshal_value_peek_boolean(v)  g_value_get_boolean (v)
#define g_marshal_value_peek_string(v)	 (char*) g_value_get_string (v)
#define g_marshal_value_peek_int(v)      g_value_get_int (v)
#define g_marshal_value_peek_double(v)   g_value_get_double (v)

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

static gchar *
seconds_to_string (gdouble  seconds_elapsed,
		   gboolean short_string)
{
	GString *s;
	gchar	*str;
	gdouble  total;
	gint	 days, hours, minutes, seconds;

	g_return_val_if_fail (seconds_elapsed >= 0.0, g_strdup (_("less than one second")));

	total	 = seconds_elapsed;

	seconds  = (gint) total % 60;
	total	/= 60;
	minutes  = (gint) total % 60;
	total	/= 60;
	hours	 = (gint) total % 24;
	days	 = (gint) total / 24;

	s = g_string_new ("");

	if (short_string) {
		if (days) {
			g_string_append_printf (s, " %dd", days);
		}

		if (hours) {
			g_string_append_printf (s, " %2.2dh", hours);
		}

		if (minutes) {
			g_string_append_printf (s, " %2.2dm", minutes);
		}

		if (seconds) {
			g_string_append_printf (s, " %2.2ds", seconds);
		}
	} else {
		if (days) {
			g_string_append_printf (s, " %d day%s",
						days,
						days == 1 ? "" : "s");
		}

		if (hours) {
			g_string_append_printf (s, " %2.2d hour%s",
						hours,
						hours == 1 ? "" : "s");
		}

		if (minutes) {
			g_string_append_printf (s, " %2.2d minute%s",
						minutes,
						minutes == 1 ? "" : "s");
		}

		if (seconds) {
			g_string_append_printf (s, " %2.2d second%s",
						seconds,
						seconds == 1 ? "" : "s");
		}
	}

	str = g_string_free (s, FALSE);

	if (str[0] == '\0') {
		g_free (str);
		str = g_strdup (_("less than one second"));
	} else {
		g_strchug (str);
	}

	return str;
}

static gchar *
seconds_estimate_to_string (gdouble  seconds_elapsed,
			    gboolean short_string,
			    guint    items_done,
			    guint    items_remaining)
{
	gdouble per_item;
	gdouble total;

	g_return_val_if_fail (seconds_elapsed >= 0.0, g_strdup (_("unknown time")));

	/* We don't want division by 0 or if total is 0 because items
	 * remaining is 0 then, equally pointless.
	 */
	if (items_done < 1 ||
	    items_remaining < 1) {
		return g_strdup (_("unknown time"));
	}

	per_item = seconds_elapsed / items_done;
	total = per_item * items_remaining;

	return seconds_to_string (total, short_string);
}

static void
index_service_stats_updated (DBusGProxy *proxy,
                             GPtrArray  *new_stats,
                             gpointer    user_data)
{
	gint i;
	
	g_print ("%s:\n", _("Statistics have been updated"));

	for (i = 0; i < new_stats->len; i++) {
                const gchar **p;
                const gchar  *service_type = NULL;
		gchar        *str;
                
                p = g_ptr_array_index (new_stats, i);
                
                service_type = p[0];
		
                if (!service_type) {
                        continue;
                }
		
		str = g_strdup_printf (_("Updating '%s' with new count:%s"), 
				       service_type,
				       p[1]);
		g_print ("  %s\n", str);
		g_free (str);
        }
}

static void
index_progress_changed (DBusGProxy  *proxy,
			const gchar *current_service,
			const gchar *uri,
			gint	     items_processed,
			gint	     items_remaining,
			gint	     items_total,
			gdouble      seconds_elapsed,
			gpointer     user_data)
{
	gchar *str1, *str2, *str3;

	str1 = seconds_estimate_to_string (seconds_elapsed,
					   TRUE,
					   items_processed,
					   items_remaining);
	str2 = seconds_to_string (seconds_elapsed, TRUE);

	str3 = g_strdup_printf (_("Processed %d/%d, current service:'%s', %s left, %s elapsed"),
				items_processed,
				items_total,
				current_service,
				str1,
				str2);

	g_free (str2);
	g_free (str1);

	g_print ("%s\n", str3);
	g_free (str3);

	if (detailed && uri && *uri) {
		gchar *str;

		str = g_strdup_printf (_("Last file to be indexed was '%s'"),
				       uri);
		g_print ("  %s\n", str);
		g_free (str);
	}
}

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

/* VOID:STRING,STRING,INT,INT,INT,DOUBLE (tracker-marshal.list:2) */
static void
tracker_VOID__STRING_STRING_INT_INT_INT_DOUBLE (GClosure     *closure,
						GValue       *return_value G_GNUC_UNUSED,
						guint         n_param_values,
						const GValue *param_values,
						gpointer      invocation_hint G_GNUC_UNUSED,
						gpointer      marshal_data)
{
	typedef void (*GMarshalFunc_VOID__STRING_STRING_INT_INT_INT_DOUBLE) (gpointer     data1,
									     gpointer     arg_1,
									     gpointer     arg_2,
									     gint         arg_3,
									     gint         arg_4,
									     gint         arg_5,
									     gdouble      arg_6,
									     gpointer     data2);
	register GMarshalFunc_VOID__STRING_STRING_INT_INT_INT_DOUBLE callback;
	register GCClosure *cc = (GCClosure*) closure;
	register gpointer data1, data2;
	
	g_return_if_fail (n_param_values == 7);
	
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
	callback = (GMarshalFunc_VOID__STRING_STRING_INT_INT_INT_DOUBLE) (marshal_data ? marshal_data : cc->callback);
	
	callback (data1,
		  g_marshal_value_peek_string (param_values + 1),
		  g_marshal_value_peek_string (param_values + 2),
		  g_marshal_value_peek_int (param_values + 3),
		  g_marshal_value_peek_int (param_values + 4),
		  g_marshal_value_peek_int (param_values + 5),
		  g_marshal_value_peek_double (param_values + 6),
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
		dbus_g_object_register_marshaller (tracker_VOID__STRING_STRING_INT_INT_INT_DOUBLE,
						   G_TYPE_NONE,
						   G_TYPE_STRING,
						   G_TYPE_STRING,
						   G_TYPE_INT,
						   G_TYPE_INT,
						   G_TYPE_INT,
						   G_TYPE_DOUBLE,
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
		dbus_g_proxy_add_signal (proxy,
					 "IndexProgress",
					 G_TYPE_STRING,
					 G_TYPE_STRING,
					 G_TYPE_INT,
					 G_TYPE_INT,
					 G_TYPE_INT,
					 G_TYPE_DOUBLE,
					 G_TYPE_INVALID);
		dbus_g_proxy_add_signal (proxy,
					 "ServiceStatisticsUpdated",
					 TRACKER_TYPE_G_STRV_ARRAY,
					 G_TYPE_INVALID);
				
		dbus_g_proxy_connect_signal (proxy,
					     "IndexStateChange",
					     G_CALLBACK (index_state_changed),
					     NULL,
					     NULL);
		dbus_g_proxy_connect_signal (proxy,
					     "IndexProgress",
					     G_CALLBACK (index_progress_changed),
					     NULL,
					     NULL);
		dbus_g_proxy_connect_signal (proxy,
					     "ServiceStatisticsUpdated",
					     G_CALLBACK (index_service_stats_updated),
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
