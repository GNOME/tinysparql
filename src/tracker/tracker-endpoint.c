/*
 * Copyright (C) 2020, Red Hat Inc.
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
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <locale.h>

#include <glib.h>
#include <glib-unix.h>
#include <glib/gi18n.h>

#include <libtracker-sparql/tracker-sparql.h>

#include "tracker-endpoint.h"

static gchar *database_path = NULL;
static gchar *dbus_service = NULL;
static gchar *ontology_name = NULL;
static gchar *ontology_path = NULL;
static gboolean session_bus = FALSE;
static gboolean system_bus = FALSE;
static gboolean name_owned = FALSE;
static gboolean list = FALSE;
static gint http_port = -1;
static gboolean http_loopback;

static GOptionEntry entries[] = {
	{ "database", 'd', 0, G_OPTION_ARG_FILENAME, &database_path,
	  N_("Location of the database"),
	  N_("DIR")
	},
	{ "dbus-service", 'b', 0, G_OPTION_ARG_STRING, &dbus_service,
	  N_("Specify the DBus name of this endpoint"),
	  N_("NAME")
	},
	{ "ontology", 'o', 0, G_OPTION_ARG_STRING, &ontology_name,
	  N_("Specify the ontology name used in this endpoint"),
	  N_("NAME")
	},
	{ "ontology-path", 'p', 0, G_OPTION_ARG_FILENAME, &ontology_path,
	  N_("Specify a path to an ontology to be used in this endpoint"),
	  N_("DIR")
	},
	{ "http-port", 0, 0, G_OPTION_ARG_INT, &http_port,
	  N_("HTTP port"),
	  NULL
	},
	{ "loopback", 0, 0, G_OPTION_ARG_NONE, &http_loopback,
	  N_("Whether to only allow HTTP connections in the loopback device"),
	  NULL
	},
	{ "session", 0, 0, G_OPTION_ARG_NONE, &session_bus,
	  N_("Use session bus"),
	  NULL
	},
	{ "system", 0, 0, G_OPTION_ARG_NONE, &system_bus,
	  N_("Use system bus"),
	  NULL
	},
	{ "list", 'l', 0, G_OPTION_ARG_NONE, &list,
	  N_("List SPARQL endpoints available in DBus"),
	  NULL
	},
	{ NULL }
};

#define TRACKER_ENDPOINT_ERROR tracker_endpoint_error_quark ()

G_DEFINE_QUARK (tracker-endpoint-error-quark, tracker_endpoint_error)

typedef enum _TrackerEndpointError {
	TRACKER_ENDPOINT_ERROR_COULD_NOT_OWN_NAME,
	TRACKER_ENDPOINT_ERROR_NAME_LOST,
} TrackerEndpointError;

static gboolean
sanity_check (void)
{
	if (list &&
	    (ontology_path || ontology_name || dbus_service || database_path)) {
		/* TRANSLATORS: these are commandline arguments */
		g_printerr ("%s\n", _("--list can only be used with --session or --system"));
		return FALSE;
	}

	if (!list && !!ontology_path == !!ontology_name) {
		/* TRANSLATORS: those are commandline arguments */
		g_printerr ("%s\n", _("One “ontology” or “ontology-path” option should be provided"));
		return FALSE;
	}

	if (http_port > 0 && dbus_service) {
		/* TRANSLATORS: those are commandline arguments */
		g_printerr ("%s\n", _("--http-port cannot be used with --dbus-service"));
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

static void
name_acquired_cb (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
	name_owned = TRUE;
	g_main_loop_quit (user_data);
}

static void
name_lost_cb (GDBusConnection *connection,
              const gchar     *name,
              gpointer         user_data)
{
	name_owned = FALSE;
	g_main_loop_quit (user_data);
}

static gboolean
block_http_handler (TrackerEndpointHttp *endpoint_http,
                    GSocketAddress      *address,
                    gpointer             user_data)
{
	GInetAddress *inet_address;

	if (!G_IS_INET_SOCKET_ADDRESS (address))
		return TRUE;

	inet_address = g_inet_socket_address_get_address (G_INET_SOCKET_ADDRESS (address));

	if (http_loopback) {
		if (g_inet_address_get_is_loopback (inet_address))
			return FALSE;

		return TRUE;
	}

	return FALSE;
}

static gboolean
run_http_endpoint (TrackerSparqlConnection  *connection,
                   GError                  **error)
{
	TrackerEndpoint *endpoint = NULL;
	g_autoptr(GMainLoop) main_loop = NULL;
	GError *inner_error = NULL;
	GInetAddress *loopback;
	gchar *loopback_str, *address;

	loopback = g_inet_address_new_loopback (G_SOCKET_FAMILY_IPV4);
	loopback_str = g_inet_address_to_string (loopback);
	address = g_strdup_printf ("http://%s:%d/sparql/",
				   loopback_str,
				   http_port);

	g_print (_("Creating HTTP endpoint at %s…"), address);
	g_print ("\n");
	g_free (address);
	g_free (loopback_str);
	g_object_unref (loopback);

	endpoint = TRACKER_ENDPOINT (tracker_endpoint_http_new (connection,
	                                                        http_port,
	                                                        NULL, NULL, &inner_error));

	if (inner_error) {
		g_propagate_error (error, inner_error);
		return FALSE;
	}

	g_signal_connect (endpoint, "block-remote-address",
	                  G_CALLBACK (block_http_handler), NULL);

	main_loop = g_main_loop_new (NULL, FALSE);

	g_print ("%s\n", _("Listening to SPARQL commands. Press Ctrl-C to stop."));

	g_unix_signal_add (SIGINT, sigterm_cb, main_loop);
	g_unix_signal_add (SIGTERM, sigterm_cb, main_loop);

	g_main_loop_run (main_loop);

	/* Carriage return, so we paper over the ^C */
	g_print ("\r%s\n", _("Closing connection…"));
	g_clear_object (&endpoint);

	return TRUE;
}

static gboolean
run_endpoint (TrackerSparqlConnection  *connection,
              GError                  **error)
{
	TrackerEndpoint *endpoint = NULL;
	GDBusConnection *dbus_connection;
	g_autoptr(GMainLoop) main_loop = NULL;
	GError *inner_error = NULL;

	g_print (_("Creating endpoint at %s…"), dbus_service);
	g_print ("\n");

	if (system_bus) {
		dbus_connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &inner_error);
	} else {
		dbus_connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &inner_error);
	}

	if (dbus_connection) {
		endpoint = TRACKER_ENDPOINT (tracker_endpoint_dbus_new (connection,
		                                                        dbus_connection,
		                                                        NULL, NULL, &inner_error));
	}

	main_loop = g_main_loop_new (NULL, FALSE);

	if (endpoint) {
		g_bus_own_name_on_connection (dbus_connection,
		                              dbus_service,
		                              G_BUS_NAME_OWNER_FLAGS_NONE,
		                              name_acquired_cb,
		                              name_lost_cb,
		                              main_loop, NULL);

		g_main_loop_run (main_loop);
	}

	if (inner_error) {
		g_propagate_error (error, inner_error);
		return FALSE;
	}

	if (!name_owned) {
		g_set_error_literal (error, TRACKER_ENDPOINT_ERROR,
		                     TRACKER_ENDPOINT_ERROR_COULD_NOT_OWN_NAME,
		                     _("Could not own DBus name"));
		return FALSE;
	}

	g_print ("%s\n", _("Listening to SPARQL commands. Press Ctrl-C to stop."));

	g_unix_signal_add (SIGINT, sigterm_cb, main_loop);
	g_unix_signal_add (SIGTERM, sigterm_cb, main_loop);

	g_main_loop_run (main_loop);

	if (!name_owned) {
		g_set_error_literal (error, TRACKER_ENDPOINT_ERROR,
		                     TRACKER_ENDPOINT_ERROR_COULD_NOT_OWN_NAME,
		                     _("DBus name lost"));
		return FALSE;
	}

	/* Carriage return, so we paper over the ^C */
	g_print ("\r%s\n", _("Closing connection…"));
	g_clear_object (&endpoint);

	return TRUE;
}

static int
run_list_endpoints (void)
{
	GDBusConnection *connection;
	GDBusMessage *message, *reply;
	GVariant *variant;
	GStrv names;
	guint i;

	if (system_bus) {
		connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);
	} else {
		connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
	}

	message = g_dbus_message_new_method_call ("org.freedesktop.DBus",
	                                          "/org/freedesktop/DBus",
	                                          "org.freedesktop.DBus",
	                                          "ListNames");
	reply = g_dbus_connection_send_message_with_reply_sync (connection,
	                                                        message,
	                                                        G_DBUS_SEND_MESSAGE_FLAGS_NONE,
	                                                        -1,
	                                                        NULL,
	                                                        NULL,
	                                                        NULL);
	g_object_unref (message);

	if (!reply)
		return EXIT_FAILURE;

	if (g_dbus_message_get_error_name (reply)) {
		g_object_unref (reply);
		return EXIT_FAILURE;
	}

	variant = g_dbus_message_get_body (reply);
	g_variant_get (variant, "(^a&s)", &names);

	for (i = 0; names[i]; i++) {
		GDBusMessage *check;
		GError *error = NULL;

		if (names[i][0] == ':')
			continue;

		/* Do a 'Query' method call, we don't mind the wrong message arguments,
		 * and even look for that specific error to detect at least the interface
		 * is implemented by this DBus service.
		 */
		message = g_dbus_message_new_method_call (names[i],
		                                          "/org/freedesktop/Tracker3/Endpoint",
		                                          "org.freedesktop.Tracker3.Endpoint",
		                                          "Query");
		check = g_dbus_connection_send_message_with_reply_sync (connection,
		                                                       message,
		                                                       G_DBUS_SEND_MESSAGE_FLAGS_NONE,
		                                                       -1,
		                                                       NULL,
		                                                       NULL,
		                                                       NULL);
		g_object_unref (message);

		if (!check)
			continue;

		if (!g_dbus_message_to_gerror (check, &error)) {
			g_object_unref (check);
			continue;
		}

		if (g_error_matches (error, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS))
			g_print ("%s\n", names[i]);

		g_clear_error (&error);
		g_object_unref (check);
	}

	g_object_unref (reply);

	return EXIT_SUCCESS;
}

int
tracker_endpoint (int argc, const char **argv)
{
	TrackerSparqlConnection *connection;
	GOptionContext *context;
	GError *error = NULL;
	GFile *database = NULL, *ontology = NULL;

	context = g_option_context_new (NULL);
	g_option_context_add_main_entries (context, entries, NULL);

	argv[0] = "tracker endpoint";

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

	if (list) {
		return run_list_endpoints ();
	}

	if (database_path)
		database = g_file_new_for_commandline_arg (database_path);

	if (ontology_path) {
		ontology = g_file_new_for_commandline_arg (ontology_path);
	} else if (ontology_name) {
		gchar *path = g_build_filename (SHAREDIR, "tracker3", "ontologies", ontology_name, NULL);
		ontology = g_file_new_for_path (path);
		g_free (path);
	}

	g_assert (ontology != NULL);

	if (database_path) {
		g_print (_("Opening database at %s…"), database_path);
		g_print ("\n");
	} else {
		g_print (_("Creating in-memory database"));
		g_print ("\n");
	}

	connection = tracker_sparql_connection_new (0, database, ontology, NULL, &error);
	if (!connection) {
		g_printerr ("%s\n", error->message);
		g_error_free (error);
		g_option_context_free (context);
		return EXIT_FAILURE;
	}

	if (http_port > 0) {
		run_http_endpoint (connection, &error);

		if (error) {
			g_printerr ("%s\n", error->message);
			g_error_free (error);
		}
	} else if (dbus_service) {
		run_endpoint (connection, &error);

		if (error) {
			g_printerr ("%s\n", error->message);
			g_error_free (error);
		}
	} else {
		g_print (_("New database created. Use the “--dbus-service” option to "
		           "share this database on a message bus."));
		g_print ("\n");
	}

	if (connection) {
		tracker_sparql_connection_close (connection);
		g_clear_object (&connection);
	}

	g_option_context_free (context);

	return EXIT_SUCCESS;
}
