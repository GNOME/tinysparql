/*
 * Copyright (C) 2010, Adrien Bustany <abustany@gnome.org>
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
 */

#include "config.h"

#include "tracker-network-provider.h"

/**
 * SECTION:tracker-network-provider
 * @short_description: Network status interface for cross platform backends
 * @include: libtracker-miner/tracker-miner.h
 *
 * The #TrackerNetworkProvider allows different backends to be written for
 * retrieving network connectivity status information. This can be used to
 * avoid heavy transfers when on a slow connection, or on a connection where
 * costs may apply. Currently, there are two implementations. The
 * NetworkManager one uses NetworkManager, and the Dummy one will always expose
 * the network as connected, no matter what the connectivity status actually
 * is.
 *
 * Since: 0.9
 **/

static void
tracker_network_provider_init (gpointer object_class)
{
	static gboolean is_initialized = FALSE;

	if (!is_initialized) {
		g_object_interface_install_property (object_class,
		                                     g_param_spec_string ("name",
		                                                          "Network provider name",
		                                                          "Network provider name",
		                                                          NULL,
		                                                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
		/**
		 * TrackerNetworkProvider::status-changed:
		 * @provider: the TrackerNetworkProvider
		 * @status: a TrackerNetworkProviderStatus describing the new network
		 * status
		 *
		 * the ::status-changed signal is emitted whenever the backend informs
		 * that the network status changed.
		 **/
		g_signal_new ("status-changed",
		              TRACKER_TYPE_NETWORK_PROVIDER,
		              G_SIGNAL_RUN_LAST,
		              0,
		              NULL, NULL,
		              g_cclosure_marshal_VOID__UINT,
		              G_TYPE_NONE, 1,
		              G_TYPE_UINT);
		is_initialized = TRUE;
	}
}

/**
 * tracker_network_provider_get_type:
 *
 * Returns: a #GType representing a %TrackerNetworkProvider.
 **/
GType
tracker_network_provider_get_type (void)
{
	static GType iface_type = 0;

	if (iface_type == 0) {
		static const GTypeInfo info = {
			sizeof (TrackerNetworkProviderIface),
			tracker_network_provider_init,
			NULL
		};

		iface_type = g_type_register_static (G_TYPE_INTERFACE,
		                                     "TrackerNetworkProvider",
		                                     &info,
		                                     0);
	}

	return iface_type;
}

/**
 * tracker_network_provider_get_name:
 * @provider: a TrackerNetworkProvider
 *
 * At the moment there are only two providers, "Dummy" and
 * "NetworkManager". Either of these is what will be returned unless new
 * providers are written.
 *
 * Returns: a newly allocated string representing the #Object:name
 * which must be freed with g_free().
 **/
gchar *
tracker_network_provider_get_name (TrackerNetworkProvider *provider)
{
	gchar *name;

	g_return_val_if_fail (TRACKER_IS_NETWORK_PROVIDER (provider), NULL);

	g_object_get (provider, "name", &name, NULL);

	return name;
}

/**
 * tracker_network_provider_get_status:
 * @provider: a TrackerNetworkProvider
 *
 * This function calls the network provider's "get_status" implementation.
 *
 * Returns: a TrackerNetworkProviderStatus decribing the current network
 * status.
 **/
TrackerNetworkProviderStatus
tracker_network_provider_get_status (TrackerNetworkProvider *provider)
{
	TrackerNetworkProviderIface *iface;

	g_return_val_if_fail (TRACKER_IS_NETWORK_PROVIDER (provider), TRACKER_NETWORK_PROVIDER_UNKNOWN);

	iface = TRACKER_NETWORK_PROVIDER_GET_INTERFACE (provider);

	if (!iface->get_status) {
		return TRACKER_NETWORK_PROVIDER_UNKNOWN;
	}

	return iface->get_status (provider);
}
