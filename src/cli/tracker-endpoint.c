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

#ifdef HAVE_AVAHI
#include <avahi-common/malloc.h>
#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-glib/glib-watch.h>
#endif

#include <tinysparql.h>

#include "tracker-endpoint.h"

static gchar *database_path = NULL;
static gchar *dbus_service = NULL;
static gchar *ontology_name = NULL;
static gchar *ontology_path = NULL;
static gboolean session_bus = FALSE;
static gboolean system_bus = FALSE;
static gboolean name_owned = FALSE;
static gboolean list = FALSE;
static gboolean list_http = FALSE;
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
	{ "list-http", 'L', 0, G_OPTION_ARG_NONE, &list_http,
	  N_("List network-local HTTP SPARQL endpoints"),
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

	if (!list && !list_http && !!ontology_path == !!ontology_name) {
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

	if (inner_error) {
		g_propagate_error (error, inner_error);
		return FALSE;
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

#ifdef HAVE_AVAHI
static GList *resolvers = NULL;
static GList *services = NULL;

static void
check_quit (GMainLoop *main_loop)
{
	if (resolvers)
		return;
	g_main_loop_quit (main_loop);
}

static void
add_service (const gchar *uri)
{
	if (g_list_find_custom (services, uri, (GCompareFunc) g_strcmp0))
		return;

	services = g_list_prepend (services, g_strdup (uri));
}

static gboolean
validate_service (AvahiStringList  *list,
                  gchar           **path_out)
{
	AvahiStringList *txtvers, *protovers, *binding, *path;
	gchar *txtvers_value = NULL, *protovers_value = NULL, *binding_value = NULL, *path_value = NULL;
	gboolean valid = FALSE;

	txtvers = avahi_string_list_find (list, "txtvers");
	protovers = avahi_string_list_find (list, "protovers");
	binding = avahi_string_list_find (list, "binding");
	path = avahi_string_list_find (list, "path");

	if (txtvers && protovers && binding && path) {
		avahi_string_list_get_pair (txtvers, NULL, &txtvers_value, NULL);
		avahi_string_list_get_pair (protovers, NULL, &protovers_value, NULL);
		avahi_string_list_get_pair (binding, NULL, &binding_value, NULL);
		avahi_string_list_get_pair (path, NULL, &path_value, NULL);

		valid = (g_strcmp0 (txtvers_value, "1") == 0 &&
		         g_strcmp0 (protovers_value, "1.1") == 0 &&
		         g_strcmp0 (binding_value, "HTTP") == 0);
		if (valid && path_out)
			*path_out = g_strdup (path_value);
	}

	g_clear_pointer (&txtvers_value, avahi_free);
	g_clear_pointer (&protovers_value, avahi_free);
	g_clear_pointer (&binding_value, avahi_free);
	g_clear_pointer (&path_value, avahi_free);

	return valid;
}

static void
service_resolver_cb (AvahiServiceResolver   *service_resolver,
                     AvahiIfIndex            interface,
                     AvahiProtocol           protocol,
                     AvahiResolverEvent      event,
                     const char             *name,
                     const char             *type,
                     const char             *domain,
                     const char             *host_name,
                     const AvahiAddress     *address,
                     uint16_t                port,
                     AvahiStringList        *txt,
                     AvahiLookupResultFlags  flags,
                     gpointer                user_data)
{
	GMainLoop *main_loop = user_data;
	gchar *path = NULL;

	switch (event) {
	case AVAHI_RESOLVER_FOUND:
		if (validate_service (txt, &path)) {
			if (g_str_has_prefix (path, "http")) {
				add_service (path);
			} else {
				char address_str[AVAHI_ADDRESS_STR_MAX];
				gchar *full_uri;

				/* Add the full URI, by IP address */
				avahi_address_snprint ((char *) &address_str,
				                       AVAHI_ADDRESS_STR_MAX,
				                       address);
				full_uri = g_strdup_printf ("http://%s%s%s:%d%s",
				                            protocol == AVAHI_PROTO_INET6 ? "[" : "",
				                            address_str,
				                            protocol == AVAHI_PROTO_INET6 ? "]" : "",
				                            port,
				                            path ? path : "/");
				add_service (full_uri);
				g_free (full_uri);

				/* Add the full URI, by host name */
				full_uri = g_strdup_printf ("http://%s:%d%s",
				                            host_name,
				                            port,
				                            path ? path : "/");
				add_service (full_uri);
				g_free (full_uri);
			}

			g_free (path);
		}
		break;
	case AVAHI_RESOLVER_FAILURE:
		break;
	}

	resolvers = g_list_remove (resolvers, service_resolver);
	avahi_service_resolver_free (service_resolver);
	check_quit (main_loop);
}

static void
service_browser_cb (AvahiServiceBrowser    *service_browser,
                    AvahiIfIndex            interface,
                    AvahiProtocol           protocol,
                    AvahiBrowserEvent       event,
                    const char             *name,
                    const char             *type,
                    const char             *domain,
                    AvahiLookupResultFlags  flags,
                    gpointer                user_data)
{
	GMainLoop *main_loop = user_data;
	AvahiServiceResolver *resolver;

	switch (event) {
	case AVAHI_BROWSER_NEW:
		resolver = avahi_service_resolver_new (avahi_service_browser_get_client (service_browser),
		                                       interface,
		                                       protocol,
		                                       name,
		                                       type,
		                                       domain,
		                                       AVAHI_PROTO_UNSPEC,
		                                       0,
		                                       service_resolver_cb,
		                                       main_loop);
		resolvers = g_list_prepend (resolvers, resolver);
		break;
	case AVAHI_BROWSER_ALL_FOR_NOW:
	case AVAHI_BROWSER_FAILURE:
		check_quit (main_loop);
		break;
	case AVAHI_BROWSER_REMOVE:
	case AVAHI_BROWSER_CACHE_EXHAUSTED:
		break;
	}
}

int
run_list_http_endpoints (void)
{
	AvahiGLibPoll *avahi_glib_poll;
	AvahiClient *avahi_client = NULL;
	AvahiServiceBrowser *avahi_service_browser = NULL;
	GMainLoop *main_loop;
	GList *l;

	main_loop = g_main_loop_new (NULL, FALSE);

	/* Set up avahi service browser */
	avahi_glib_poll =
		avahi_glib_poll_new (NULL, G_PRIORITY_DEFAULT);
	if (avahi_glib_poll) {
		avahi_client =
			avahi_client_new (avahi_glib_poll_get (avahi_glib_poll),
			                  AVAHI_CLIENT_IGNORE_USER_CONFIG |
			                  AVAHI_CLIENT_NO_FAIL,
			                  NULL, NULL, NULL);
	}

	if (avahi_client) {
		avahi_service_browser =
			avahi_service_browser_new (avahi_client,
			                           AVAHI_IF_UNSPEC,
			                           AVAHI_PROTO_UNSPEC,
			                           "_sparql._tcp",
			                           NULL,
			                           0,
			                           service_browser_cb,
			                           main_loop);
	}

	/* Collect running HTTP services */
	if (avahi_service_browser)
		g_main_loop_run (main_loop);

	g_main_loop_unref (main_loop);

	/* Sort and print the collected services */
	services = g_list_sort (services, (GCompareFunc) g_strcmp0);
	for (l = services; l; l = l->next)
		g_print ("%s\n", (gchar *) l->data);

	g_clear_pointer (&avahi_service_browser, avahi_service_browser_free);
	g_clear_pointer (&avahi_client, avahi_client_free);
	g_clear_pointer (&avahi_glib_poll, avahi_glib_poll_free);
	g_list_free_full (services, g_free);

	return EXIT_SUCCESS;
}
#endif /* HAVE_AVAHI */

int
tracker_endpoint (int argc, const char **argv)
{
	TrackerSparqlConnection *connection;
	GOptionContext *context;
	GError *error = NULL;
	GFile *database = NULL, *ontology = NULL;
	gboolean success = FALSE;

	context = g_option_context_new (NULL);
	g_option_context_add_main_entries (context, entries, NULL);

	argv[0] = "tinysparql endpoint";

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
	} else if (list_http) {
#ifdef HAVE_AVAHI
		return run_list_http_endpoints ();
#else
		return EXIT_FAILURE;
#endif
	}

	if (database_path)
		database = g_file_new_for_commandline_arg (database_path);

	if (ontology_path) {
		ontology = g_file_new_for_commandline_arg (ontology_path);
	} else if (ontology_name) {
		if (g_strcmp0 (ontology_name, "nepomuk") == 0) {
			ontology = tracker_sparql_get_ontology_nepomuk ();
		} else {
			gchar *path = g_build_filename (SHAREDIR, "tracker3", "ontologies", ontology_name, NULL);
			ontology = g_file_new_for_path (path);
			g_free (path);
		}
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
		success = run_http_endpoint (connection, &error);

		if (error) {
			g_printerr ("%s\n", error->message);
			g_error_free (error);
		}
	} else if (dbus_service) {
		success = run_endpoint (connection, &error);

		if (error) {
			g_printerr ("%s\n", error->message);
			g_error_free (error);
		}
	} else {
		success = TRUE;
		g_print (_("New database created. Use the “--dbus-service” option to "
		           "share this database on a message bus."));
		g_print ("\n");
	}

	if (connection) {
		tracker_sparql_connection_close (connection);
		g_clear_object (&connection);
	}

	g_option_context_free (context);

	return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
