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

enum {
	PROP_0,
	PROP_SPARQL_CONNECTION,
	PROP_READONLY,
	N_PROPS
};

static GParamSpec *props[N_PROPS] = { 0 };

typedef struct {
	TrackerSparqlConnection *sparql_connection;
	gboolean readonly;
} TrackerEndpointPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (TrackerEndpoint, tracker_endpoint, G_TYPE_OBJECT)

/**
 * TrackerEndpoint:
 *
 * `TrackerEndpoint` is a helper object to make RDF triple stores represented
 * by a [class@Tracker.SparqlConnection] publicly available to other processes/hosts.
 *
 * This is a base abstract object, see [class@Tracker.EndpointDBus] to make
 * RDF triple stores available to other processes in the same machine, and
 * [class@Tracker.EndpointHttp] to make it available to other hosts in the
 * network.
 *
 * When the RDF triple store represented by a [class@Tracker.SparqlConnection]
 * is made public this way, other peers may connect to the database using
 * [ctor@Tracker.SparqlConnection.bus_new] or [ctor@Tracker.SparqlConnection.remote_new]
 * to access this endpoint exclusively, or they may use the `SERVICE <uri> { ... }` SPARQL
 * syntax from their own [class@Tracker.SparqlConnection]s to expand their data set.
 *
 * By default, and as long as the underlying [class@Tracker.SparqlConnection]
 * allows SPARQL updates and RDF graph changes, endpoints will allow updates
 * and modifications to happen through them. Use [method@Tracker.Endpoint.set_readonly]
 * to change this behavior.
 */

static void
tracker_endpoint_finalize (GObject *object)
{
	TrackerEndpoint *endpoint = TRACKER_ENDPOINT (object);
	TrackerEndpointPrivate *priv = tracker_endpoint_get_instance_private (endpoint);

	g_clear_object (&priv->sparql_connection);

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
	 * The [class@Tracker.SparqlConnection] being proxied by this endpoint.
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

	g_object_class_install_properties (object_class, N_PROPS, props);
}

static void
tracker_endpoint_init (TrackerEndpoint *endpoint)
{
}

/**
 * tracker_endpoint_get_sparql_connection:
 * @endpoint: a `TrackerEndpoint`
 *
 * Returns the [class@Tracker.SparqlConnection] that this endpoint proxies
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
 * [class@Tracker.SparqlConnection] may be readonly of its own, this
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
