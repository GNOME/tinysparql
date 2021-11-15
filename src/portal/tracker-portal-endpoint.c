/*
 * Copyright (C) 2020, Red Hat Inc.
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
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */
#include "config.h"

#include <libtracker-sparql/tracker-sparql.h>

#include "libtracker-sparql/tracker-private.h"

#include "tracker-portal-endpoint.h"

#define GRAPH_ALL "*"
#define GRAPH_DEFAULT "default"

typedef struct _TrackerPortalEndpointClass TrackerPortalEndpointClass;

struct _TrackerPortalEndpoint
{
	TrackerEndpointDBus parent_instance;
	gchar *peer;
	gchar *prologue;
	GStrv graphs;
	guint watch_id;
};

struct _TrackerPortalEndpointClass
{
	TrackerEndpointDBusClass parent_class;
};

enum {
	PROP_GRAPHS = 1,
	PROP_PEER,
	N_PROPS
};

enum {
	CLOSED,
	N_SIGNALS
};

static GParamSpec *props[N_PROPS] = { 0 };
static guint signals[N_SIGNALS] = { 0 };

G_DEFINE_TYPE (TrackerPortalEndpoint, tracker_portal_endpoint, TRACKER_TYPE_ENDPOINT_DBUS)

static gboolean
tracker_portal_endpoint_forbid_operation (TrackerEndpointDBus   *endpoint_dbus,
                                          GDBusMethodInvocation *invocation,
                                          TrackerOperationType   operation_type)
{
	TrackerPortalEndpoint *endpoint = TRACKER_PORTAL_ENDPOINT (endpoint_dbus);

	if (operation_type == TRACKER_OPERATION_TYPE_UPDATE)
		return TRUE;
	if (g_strcmp0 (endpoint->peer, g_dbus_method_invocation_get_sender (invocation)) != 0)
		return TRUE;

	return FALSE;
}

static gboolean
tracker_portal_endpoint_filter_graph (TrackerEndpointDBus *endpoint_dbus,
                                      const gchar         *graph_name)
{
	TrackerPortalEndpoint *endpoint = TRACKER_PORTAL_ENDPOINT (endpoint_dbus);
	gint i;

	for (i = 0; endpoint->graphs[i]; i++) {
		if (g_strcmp0 (endpoint->graphs[i], GRAPH_ALL) == 0) {
			return FALSE;
		} else if (!graph_name &&
		           g_strcmp0 (endpoint->graphs[i], GRAPH_DEFAULT) == 0) {
			return FALSE;
		} else if (g_strcmp0 (graph_name, endpoint->graphs[i]) == 0) {
			return FALSE;
		} else {
			TrackerSparqlConnection *connection;
			TrackerNamespaceManager *namespaces;
			gchar *expanded;

			/* We may have been given a prefixed name instead of an
			 * URI, expand it in order to check with the given graph.
			 */
			connection = tracker_endpoint_get_sparql_connection (TRACKER_ENDPOINT (endpoint));
			namespaces = tracker_sparql_connection_get_namespace_manager (connection);
			expanded = tracker_namespace_manager_expand_uri (namespaces,
			                                                 endpoint->graphs[i]);

			if (g_strcmp0 (graph_name, expanded) == 0) {
				g_free (expanded);
				return FALSE;
			}

			g_free (expanded);
		}
	}

	return TRUE;
}

static gchar *
tracker_portal_endpoint_add_prologue (TrackerEndpointDBus *endpoint_dbus)
{
	TrackerPortalEndpoint *endpoint = TRACKER_PORTAL_ENDPOINT (endpoint_dbus);

	if (!endpoint->prologue) {
		GString *str;
		gint i;

		str = g_string_new ("CONSTRAINT SERVICE \n"
		                    "CONSTRAINT GRAPH ");

		for (i = 0; endpoint->graphs[i]; i++) {
			if (i != 0)
				g_string_append (str, ", ");

			if (g_strcmp0 (endpoint->graphs[i], GRAPH_ALL) == 0)
				g_string_append (str, "ALL ");
			else if (g_strcmp0 (endpoint->graphs[i], GRAPH_DEFAULT) == 0)
				g_string_append (str, "DEFAULT ");
			else if (strstr (endpoint->graphs[i], ":/"))
				g_string_append_printf (str, "<%s> ", endpoint->graphs[i]);
			else
				g_string_append_printf (str, "%s ", endpoint->graphs[i]);
		}

		endpoint->prologue = g_string_free (str, FALSE);
	}

	return g_strdup (endpoint->prologue);
}

static void
tracker_portal_endpoint_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
	TrackerPortalEndpoint *endpoint = TRACKER_PORTAL_ENDPOINT (object);

	switch (prop_id) {
	case PROP_GRAPHS:
		endpoint->graphs = g_value_dup_boxed (value);
		break;
	case PROP_PEER:
		endpoint->peer = g_value_dup_string (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
tracker_portal_endpoint_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
	TrackerPortalEndpoint *endpoint = TRACKER_PORTAL_ENDPOINT (object);

	switch (prop_id) {
	case PROP_GRAPHS:
		g_value_set_boxed (value, endpoint->graphs);
		break;
	case PROP_PEER:
		g_value_set_string (value, endpoint->peer);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
peer_vanished_cb (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
	TrackerPortalEndpoint *endpoint = user_data;

	g_signal_emit (endpoint, signals[CLOSED], 0);
}

static void
tracker_portal_endpoint_constructed (GObject *object)
{
	TrackerPortalEndpoint *endpoint = TRACKER_PORTAL_ENDPOINT (object);
	GDBusConnection *connection;

	g_object_get (object, "dbus-connection", &connection, NULL);
	endpoint->watch_id = g_bus_watch_name_on_connection (connection,
	                                                     endpoint->peer,
	                                                     G_BUS_NAME_WATCHER_FLAGS_NONE,
	                                                     NULL,
	                                                     peer_vanished_cb,
	                                                     object,
	                                                     NULL);
	g_object_unref (connection);

	if (G_OBJECT_CLASS (tracker_portal_endpoint_parent_class)->constructed)
		G_OBJECT_CLASS (tracker_portal_endpoint_parent_class)->constructed (object);
}

static void
tracker_portal_endpoint_finalize (GObject *object)
{
	TrackerPortalEndpoint *endpoint = TRACKER_PORTAL_ENDPOINT (object);

	g_bus_unwatch_name (endpoint->watch_id);
	g_strfreev (endpoint->graphs);
	g_free (endpoint->peer);
	g_free (endpoint->prologue);

	G_OBJECT_CLASS (tracker_portal_endpoint_parent_class)->finalize (object);
}

static void
tracker_portal_endpoint_class_init (TrackerPortalEndpointClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	TrackerEndpointDBusClass *endpoint_dbus_class = TRACKER_ENDPOINT_DBUS_CLASS (klass);

	object_class->set_property = tracker_portal_endpoint_set_property;
	object_class->get_property = tracker_portal_endpoint_get_property;
	object_class->constructed = tracker_portal_endpoint_constructed;
	object_class->finalize = tracker_portal_endpoint_finalize;

	endpoint_dbus_class->forbid_operation = tracker_portal_endpoint_forbid_operation;
	endpoint_dbus_class->filter_graph = tracker_portal_endpoint_filter_graph;
	endpoint_dbus_class->add_prologue = tracker_portal_endpoint_add_prologue;

	signals[CLOSED] =
		g_signal_new ("closed",
		              TRACKER_TYPE_PORTAL_ENDPOINT, 0,
		              0, NULL, NULL, NULL,
		              G_TYPE_NONE, 0, G_TYPE_NONE);

	props[PROP_GRAPHS] =
		g_param_spec_boxed ("graphs",
		                    "Graphs",
		                    "Graphs",
		                    G_TYPE_STRV,
		                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
	props[PROP_PEER] =
		g_param_spec_string ("peer",
		                     "DBus peer",
		                     "DBus peer",
		                     NULL,
		                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

	g_object_class_install_properties (object_class, N_PROPS, props);
}

static void
tracker_portal_endpoint_init (TrackerPortalEndpoint *portal_endpoint)
{
}

TrackerEndpoint *
tracker_portal_endpoint_new (TrackerSparqlConnection  *sparql_connection,
                             GDBusConnection          *dbus_connection,
                             const gchar              *object_path,
                             const gchar              *peer,
                             const gchar * const      *graphs,
                             GCancellable             *cancellable,
                             GError                  **error)
{
	g_return_val_if_fail (TRACKER_IS_SPARQL_CONNECTION (sparql_connection), NULL);
	g_return_val_if_fail (G_IS_DBUS_CONNECTION (dbus_connection), NULL);
	g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), NULL);
	g_return_val_if_fail (!error || !*error, NULL);

	return g_initable_new (TRACKER_TYPE_PORTAL_ENDPOINT, cancellable, error,
	                       "dbus-connection", dbus_connection,
	                       "sparql-connection", sparql_connection,
	                       "object-path", object_path,
	                       "peer", peer,
	                       "graphs", graphs,
	                       NULL);

}
