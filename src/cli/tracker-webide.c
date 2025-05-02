/*
 * Copyright (C) 2024, Divyansh Jain <divyanshjain.2206@gmail.com>
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

#include <glib.h>
#include <glib-unix.h>
#include <glib/gi18n.h>

#include "web-ide/tracker-webide.h"

#include "tracker-webide.h"

static gint port = -1;

static GOptionEntry entries[] = {
	{ "port", 'p', 0, G_OPTION_ARG_INT, &port,
	  /* Translators: this is a HTTP port */
	  N_("Port to listen on"),
	  NULL },
	{ NULL }
};

static gboolean
sanity_check ()
{
	if (port == -1) {
		/* Translators: this is a HTTP port */
		g_print ("%s\n", _("Port not specified"));

		return FALSE;
	}

	return TRUE;
}

static gboolean
sigterm_cb (gpointer user_data)
{
	g_main_loop_quit (user_data);

	return G_SOURCE_REMOVE;
}

static gboolean
run_webide (GError **error)
{
	TrackerWebide *webide = NULL;
	g_autoptr(GMainLoop) main_loop = NULL;
	GError *inner_error = NULL;
	GInetAddress *loopback;
	gchar *loopback_str, *web_ide_address;

	loopback = g_inet_address_new_loopback (G_SOCKET_FAMILY_IPV4);
	loopback_str = g_inet_address_to_string (loopback);
	web_ide_address = g_strdup_printf ("http://%s:%d/",
	                                   loopback_str,
	                                   port);
	/* Translators: This will point to a local HTTP address */
	g_print (_("Creating Web IDE at %s…"), web_ide_address);
	g_print ("\n");
	g_free (loopback_str);
	g_free (web_ide_address);
	g_object_unref (loopback);

	webide = TRACKER_WEBIDE (tracker_webide_new (port, NULL,
	                                             NULL, &inner_error));

	if (inner_error || !webide) {
		g_propagate_error (error, inner_error);
		return FALSE;
	}

	main_loop = g_main_loop_new (NULL, FALSE);

	g_unix_signal_add (SIGINT, sigterm_cb, main_loop);
	g_unix_signal_add (SIGTERM, sigterm_cb, main_loop);

	g_print ("%s\n", _("Listening to SPARQL commands. Press Ctrl-C to stop."));

	g_main_loop_run (main_loop);

	/* Carriage return, so we paper over the ^C */
	g_print ("\r%s\n", _("Closing connection…"));
	g_clear_object (&webide);

	return TRUE;
}

int tracker_webide (int argc, const char **argv)
{
	GOptionContext *context;
	GError *error = NULL;

	context = g_option_context_new (NULL);
	g_option_context_add_main_entries (context, entries, NULL);

	argv[0] = "tinysparql webide";

	if (!g_option_context_parse (context, &argc, (char***) &argv, &error)) {
		g_printerr ("%s, %s\n", _("Unrecognized options"), error->message);
		g_error_free (error);
		g_option_context_free (context);
		return EXIT_FAILURE;
	}

	if (!sanity_check ()) {
		gchar *help;

		help = g_option_context_get_help (context, TRUE, NULL);
		g_printerr ("%s\n", help);
		g_free (help);
		g_option_context_free (context);
		return EXIT_FAILURE;
	}

	run_webide (&error);

	g_option_context_free (context);

	if (error) {
		g_printerr ("%s\n", error->message);
		g_error_free (error);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
