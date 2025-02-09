/*
 * Copyright (C) 2019, Red Hat, Inc
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#include "config.h"

#include "tracker-endpoint.h"
#include "tracker-private.h"

#define MAX_CACHED_STMTS 50

enum {
	PROP_0,
	PROP_SPARQL_CONNECTION,
	PROP_READONLY,
	PROP_ALLOWED_SERVICES,
	PROP_ALLOWED_GRAPHS,
	N_PROPS
};

static GParamSpec *props[N_PROPS] = { 0 };

typedef struct {
	GHashTable *stmts; /* char* -> GList* in mru */
	GQueue mru;
} CachedStmts;

typedef struct {
	TrackerSparqlConnection *sparql_connection;
	CachedStmts select_stmts;
	GStrv allowed_services;
	GStrv allowed_graphs;
	gchar *prologue;
	gboolean readonly;
} TrackerEndpointPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (TrackerEndpoint, tracker_endpoint, G_TYPE_OBJECT)

/**
 * TrackerEndpoint:
 *
 * `TrackerEndpoint` is a helper object to make RDF triple stores represented
 * by a [class@SparqlConnection] publicly available to other processes/hosts.
 *
 * This is a base abstract object, see [class@EndpointDBus] to make
 * RDF triple stores available to other processes in the same machine, and
 * [class@EndpointHttp] to make it available to other hosts in the
 * network.
 *
 * When the RDF triple store represented by a [class@SparqlConnection]
 * is made public this way, other peers may connect to the database using
 * [ctor@SparqlConnection.bus_new] or [ctor@SparqlConnection.remote_new]
 * to access this endpoint exclusively, or they may use the `SERVICE <uri> { ... }` SPARQL
 * syntax from their own [class@SparqlConnection]s to expand their data set.
 *
 * By default, and as long as the underlying [class@SparqlConnection]
 * allows SPARQL updates and RDF graph changes, endpoints will allow updates
 * and modifications to happen through them. Use [method@Endpoint.set_readonly]
 * to change this behavior.
 *
 * By default, endpoints allow access to every RDF graph in the triple store
 * and further external SPARQL endpoints to the queries performed on it. Use
 * [method@Endpoint.set_allowed_graphs] and
 * [method@Endpoint.set_allowed_services] to change this behavior. Users do
 * not typically need to do this for D-Bus endpoints, as these do already have a layer
 * of protection with the Tracker portal. This is the mechanism used by the portal
 * itself. This access control API may not interoperate with other SPARQL endpoint
 * implementations than Tracker.
 */

static void
tracker_endpoint_finalize (GObject *object)
{
	TrackerEndpoint *endpoint = TRACKER_ENDPOINT (object);
	TrackerEndpointPrivate *priv = tracker_endpoint_get_instance_private (endpoint);

	g_clear_object (&priv->sparql_connection);
	g_clear_pointer (&priv->allowed_services, g_strfreev);
	g_clear_pointer (&priv->allowed_graphs, g_strfreev);
	g_clear_pointer (&priv->prologue, g_free);

	g_queue_clear_full (&priv->select_stmts.mru, g_object_unref);
	g_clear_pointer (&priv->select_stmts.stmts, g_hash_table_unref);

	G_OBJECT_CLASS (tracker_endpoint_parent_class)->finalize (object);
}

static void
tracker_endpoint_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
	TrackerEndpoint *endpoint = TRACKER_ENDPOINT (object);
	TrackerEndpointPrivate *priv = tracker_endpoint_get_instance_private (endpoint);

	switch (prop_id) {
	case PROP_SPARQL_CONNECTION:
		priv->sparql_connection = g_value_dup_object (value);
		break;
	case PROP_READONLY:
		tracker_endpoint_set_readonly (endpoint,
		                               g_value_get_boolean (value));
		break;
	case PROP_ALLOWED_SERVICES:
		tracker_endpoint_set_allowed_services (endpoint,
		                                       g_value_get_boxed (value));
		break;
	case PROP_ALLOWED_GRAPHS:
		tracker_endpoint_set_allowed_graphs (endpoint,
		                                     g_value_get_boxed (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
tracker_endpoint_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
	TrackerEndpoint *endpoint = TRACKER_ENDPOINT (object);
	TrackerEndpointPrivate *priv = tracker_endpoint_get_instance_private (endpoint);

	switch (prop_id) {
	case PROP_SPARQL_CONNECTION:
		g_value_set_object (value, priv->sparql_connection);
		break;
	case PROP_READONLY:
		g_value_set_boolean (value, priv->readonly);
		break;
	case PROP_ALLOWED_SERVICES:
		g_value_set_boxed (value, priv->allowed_services);
		break;
	case PROP_ALLOWED_GRAPHS:
		g_value_set_boxed (value, priv->allowed_graphs);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
tracker_endpoint_class_init (TrackerEndpointClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_endpoint_finalize;
	object_class->set_property = tracker_endpoint_set_property;
	object_class->get_property = tracker_endpoint_get_property;

	/**
	 * TrackerEndpoint:sparql-connection:
	 *
	 * The [class@SparqlConnection] being proxied by this endpoint.
	 */
	props[PROP_SPARQL_CONNECTION] =
		g_param_spec_object ("sparql-connection",
		                     "Sparql connection",
		                     "Sparql connection",
		                     TRACKER_SPARQL_TYPE_CONNECTION,
		                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
	/**
	 * TrackerEndpoint:readonly:
	 *
	 * Whether the endpoint allows SPARQL updates or not. See
	 * tracker_endpoint_set_readonly().
	 *
	 * Since: 3.7
	 */
	props[PROP_READONLY] =
		g_param_spec_boolean ("readonly",
		                      "Readonly",
		                      "Readonly",
		                      FALSE,
		                      G_PARAM_READWRITE);

	/**
	 * TrackerEndpoint:allowed-services:
	 *
	 * External SPARQL endpoints that are allowed to be
	 * accessed through queries to this endpint. See
	 * tracker_endpoint_set_allowed_services().
	 *
	 * Since: 3.7
	 */
	props[PROP_ALLOWED_SERVICES] =
		g_param_spec_boxed ("allowed-services",
		                    NULL, NULL,
		                    G_TYPE_STRV,
		                    G_PARAM_READWRITE);

	/**
	 * TrackerEndpoint:allowed-graphs:
	 *
	 * RDF graphs that are allowed to be accessed
	 * through queries to this endpoint. See
	 * tracker_endpoint_set_allowed_graphs().
	 *
	 * Since: 3.7
	 */
	props[PROP_ALLOWED_GRAPHS] =
		g_param_spec_boxed ("allowed-graphs",
		                    NULL, NULL,
		                    G_TYPE_STRV,
		                    G_PARAM_READWRITE);

	g_object_class_install_properties (object_class, N_PROPS, props);
}

static void
tracker_endpoint_init (TrackerEndpoint *endpoint)
{
	TrackerEndpointPrivate *priv =
		tracker_endpoint_get_instance_private (endpoint);

	g_queue_init (&priv->select_stmts.mru);
	priv->select_stmts.stmts = g_hash_table_new (g_str_hash, g_str_equal);
}

static void
update_prologue (TrackerEndpoint *endpoint)
{
	TrackerEndpointPrivate *priv =
		tracker_endpoint_get_instance_private (endpoint);
	GString *str;
	gint i;

	g_clear_pointer (&priv->prologue, g_free);

	if (priv->allowed_services == NULL && priv->allowed_graphs == NULL)
		return;

	str = g_string_new (NULL);

	if (priv->allowed_services != NULL) {
		g_string_append (str, "CONSTRAINT SERVICE ");

		for (i = 0; priv->allowed_services[i]; i++) {
			if (i != 0)
				g_string_append (str, ", ");

			g_string_append_printf (str, "<%s> ",
			                        priv->allowed_services[i]);
		}
	}

	if (priv->allowed_graphs != NULL) {
		g_string_append (str, "CONSTRAINT GRAPH ");

		for (i = 0; priv->allowed_graphs[i]; i++) {
			if (i != 0)
				g_string_append (str, ", ");

			if (!*priv->allowed_graphs[i]) {
				g_string_append (str, "DEFAULT ");
			} else {
				TrackerNamespaceManager *namespaces;
				TrackerSparqlConnection *conn;
				gchar *expanded;

				conn = tracker_endpoint_get_sparql_connection (endpoint);
				namespaces = tracker_sparql_connection_get_namespace_manager (conn);
				expanded = tracker_namespace_manager_expand_uri (namespaces,
				                                                 priv->allowed_graphs[i]);
				g_string_append_printf (str, "<%s> ", expanded);
				g_free (expanded);
			}
		}
	}

	priv->prologue = g_string_free (str, FALSE);
}

void
tracker_endpoint_rewrite_query (TrackerEndpoint  *endpoint,
                                gchar           **query)
{
	TrackerEndpointPrivate *priv =
		tracker_endpoint_get_instance_private (endpoint);
	gchar *rewritten;

	if (!priv->prologue)
		return;

	rewritten = g_strconcat (priv->prologue, " ", *query, NULL);
	g_free (*query);
	*query = rewritten;
}

gboolean
tracker_endpoint_is_graph_filtered (TrackerEndpoint *endpoint,
                                    const gchar     *graph)
{
	TrackerEndpointPrivate *priv =
		tracker_endpoint_get_instance_private (endpoint);
	gint i;

	if (!priv->allowed_graphs)
		return FALSE;

	for (i = 0; priv->allowed_graphs[i]; i++) {
		if (!graph && !*priv->allowed_graphs[i]) {
			return FALSE;
		} else if (graph) {
			gboolean match = FALSE;

			match = g_strcmp0 (graph, priv->allowed_graphs[i]) == 0;

			if (!match) {
				TrackerNamespaceManager *namespaces;
				TrackerSparqlConnection *conn;
				gchar *expanded;

				conn = tracker_endpoint_get_sparql_connection (endpoint);
				namespaces = tracker_sparql_connection_get_namespace_manager (conn);
				expanded = tracker_namespace_manager_expand_uri (namespaces,
				                                                 priv->allowed_graphs[i]);

				match = g_strcmp0 (graph, expanded) == 0;
				g_free (expanded);
			}

			if (match)
				return FALSE;
		}
	}

	return TRUE;
}

static void
update_mru (CachedStmts *cached_stmts,
            GList       *link)
{
	/* Put element first in the MRU */
	g_queue_unlink (&cached_stmts->mru, link);
	g_queue_push_head_link (&cached_stmts->mru, link);
}

static void
insert_to_mru (CachedStmts            *cached_stmts,
               TrackerSparqlStatement *stmt)
{
	GList *link;

	/* Insert stmt to MRU/HT */
	g_queue_push_head (&cached_stmts->mru, stmt);
	link = cached_stmts->mru.head;
	g_hash_table_insert (cached_stmts->stmts,
	                     (gpointer) tracker_sparql_statement_get_sparql (stmt),
	                     link);

	/* Check for possibly evicted items */
	while (cached_stmts->mru.length > MAX_CACHED_STMTS) {
		stmt = g_queue_pop_tail (&cached_stmts->mru);
		g_hash_table_remove (cached_stmts->stmts,
		                     (gpointer) tracker_sparql_statement_get_sparql (stmt));
		g_object_unref (stmt);
	}
}

TrackerSparqlStatement *
tracker_endpoint_cache_select_sparql (TrackerEndpoint  *endpoint,
                                      const gchar      *sparql,
                                      GCancellable     *cancellable,
                                      GError          **error)
{
	TrackerEndpointPrivate *priv =
		tracker_endpoint_get_instance_private (endpoint);
	TrackerSparqlStatement *stmt;
	GList *link;

	link = g_hash_table_lookup (priv->select_stmts.stmts, sparql);

	if (link) {
		update_mru (&priv->select_stmts, link);
		stmt = g_object_ref (link->data);
		tracker_sparql_statement_clear_bindings (stmt);
	} else {
		stmt = tracker_sparql_connection_query_statement (priv->sparql_connection,
		                                                  sparql,
		                                                  cancellable,
		                                                  error);
		if (stmt)
			insert_to_mru (&priv->select_stmts, g_object_ref (stmt));
	}

	return stmt;
}

/**
 * tracker_endpoint_get_sparql_connection:
 * @endpoint: a `TrackerEndpoint`
 *
 * Returns the [class@SparqlConnection] that this endpoint proxies
 * to a wider audience.
 *
 * Returns: (transfer none): The proxied SPARQL connection
 **/
TrackerSparqlConnection *
tracker_endpoint_get_sparql_connection (TrackerEndpoint *endpoint)
{
	TrackerEndpointPrivate *priv = tracker_endpoint_get_instance_private (endpoint);

	return priv->sparql_connection;
}

/**
 * tracker_endpoint_set_readonly:
 * @endpoint: The endpoint
 * @readonly: Whether the endpoint will be readonly
 *
 * Sets whether the endpoint will be readonly. Readonly endpoints
 * will not allow SPARQL update queries. The underlying
 * [class@SparqlConnection] may be readonly of its own, this
 * method does not change its behavior in any way.
 *
 * Since: 3.7
 **/
void
tracker_endpoint_set_readonly (TrackerEndpoint *endpoint,
                               gboolean         readonly)
{
	TrackerEndpointPrivate *priv =
		tracker_endpoint_get_instance_private (endpoint);

	g_return_if_fail (TRACKER_IS_ENDPOINT (endpoint));

	if (priv->readonly == !!readonly)
		return;

	priv->readonly = !!readonly;
	g_object_notify (G_OBJECT (endpoint), "readonly");
}

/**
 * tracker_endpoint_get_readonly:
 * @endpoint: The endpoint
 *
 * Returns whether the endpoint is readonly, thus SPARQL update
 * queries are disallowed.
 *
 * Returns: %TRUE if the endpoint is readonly
 *
 * Since: 3.7
 **/
gboolean
tracker_endpoint_get_readonly (TrackerEndpoint *endpoint)
{
	TrackerEndpointPrivate *priv =
		tracker_endpoint_get_instance_private (endpoint);

	g_return_val_if_fail (TRACKER_IS_ENDPOINT (endpoint), FALSE);

	return priv->readonly;
}

/**
 * tracker_endpoint_set_allowed_services:
 * @endpoint: The endpoint
 * @services: List of allowed services, or %NULL to allow all services
 *
 * Sets the list of external SPARQL endpoints that this endpoint
 * will allow access for. Access through the `SERVICE` SPARQL syntax
 * will fail for services not specified in this list.
 *
 * If @services is %NULL, access will be allowed to every external endpoint,
 * this is the default behavior. If you want to forbid access to all
 * external SPARQL endpoints, use an empty list.
 *
 * This affects both remote SPARQL endpoints accessed through HTTP,
 * and external SPARQL endpoints offered through D-Bus. For the latter,
 * the following syntax is allowed to describe them as an URI:

 * `DBUS_URI = 'dbus:' [ ('system' | 'session') ':' ]? dbus-name [ ':' object-path ]?`
 *
 * If the system/session part is omitted, it will default to the session
 * bus. If the object path is omitted, the `/org/freedesktop/Tracker3/Endpoint`
 * [class@EndpointDBus] default will be assumed.
 *
 * Since: 3.7
 **/
void
tracker_endpoint_set_allowed_services (TrackerEndpoint     *endpoint,
                                       const gchar * const *services)
{
	TrackerEndpointPrivate *priv =
		tracker_endpoint_get_instance_private (endpoint);

	g_return_if_fail (TRACKER_IS_ENDPOINT (endpoint));

	g_clear_pointer (&priv->allowed_services, g_strfreev);
	priv->allowed_services = g_strdupv ((gchar **) services);
	update_prologue (endpoint);
	g_object_notify (G_OBJECT (endpoint), "allowed-services");
}

/**
 * tracker_endpoint_get_allowed_services:
 * @endpoint: The endpoint
 *
 * Returns the list of external SPARQL endpoints that are
 * allowed to be accessed through this endpoint.
 *
 * Returns: (transfer full): The list of allowed services
 *
 * Since: 3.7
 **/
GStrv
tracker_endpoint_get_allowed_services (TrackerEndpoint *endpoint)
{
	TrackerEndpointPrivate *priv =
		tracker_endpoint_get_instance_private (endpoint);

	g_return_val_if_fail (TRACKER_IS_ENDPOINT (endpoint), NULL);

	return g_strdupv (priv->allowed_services);
}

/**
 * tracker_endpoint_set_allowed_graphs:
 * @endpoint: The endpoint
 * @graphs: List of allowed graphs, or %NULL to allow all graphs
 *
 * Sets the list of RDF graphs that this endpoint will allow access
 * for. Any explicit (e.g. `GRAPH` keyword) or implicit (e.g. through the
 * default anonymous graph) access to RDF graphs unespecified in this
 * list in SPARQL queries will be seen as if those graphs did not exist, or
 * (equivalently) had an empty set. Changes to these graphs through SPARQL
 * updates will also be disallowed.
 *
 * If @graphs is %NULL, access will be allowed to every RDF graph stored
 * in the endpoint, this is the default behavior. If you want to forbid access
 * to all RDF graphs, use an empty list.
 *
 * The empty string (`""`) is allowed as a special value, to allow access
 * to the stock anonymous graph. All graph names are otherwise dependent
 * on the endpoint and its contained data.
 *
 * Since: 3.7
 **/
void
tracker_endpoint_set_allowed_graphs (TrackerEndpoint     *endpoint,
                                     const gchar * const *graphs)
{
	TrackerEndpointPrivate *priv =
		tracker_endpoint_get_instance_private (endpoint);

	g_return_if_fail (TRACKER_IS_ENDPOINT (endpoint));

	g_clear_pointer (&priv->allowed_graphs, g_strfreev);
	priv->allowed_graphs = g_strdupv ((gchar **) graphs);
	update_prologue (endpoint);
	g_object_notify (G_OBJECT (endpoint), "allowed-graphs");
}

/**
 * tracker_endpoint_get_allowed_graphs:
 * @endpoint: The endpoint
 *
 * Returns the list of RDF graphs that the endpoint allows
 * access for.
 *
 * Returns: (transfer full): The list of allowed RDF graphs
 *
 * Since: 3.7
 **/
GStrv
tracker_endpoint_get_allowed_graphs (TrackerEndpoint *endpoint)
{
	TrackerEndpointPrivate *priv =
		tracker_endpoint_get_instance_private (endpoint);

	g_return_val_if_fail (TRACKER_IS_ENDPOINT (endpoint), NULL);

	return g_strdupv (priv->allowed_graphs);
}
