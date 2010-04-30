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

#ifndef __LIBTRACKER_MINER_NETWORK_PROVIDER_H__
#define __LIBTRACKER_MINER_NETWORK_PROVIDER_H__

#if !defined (__LIBTRACKER_MINER_H_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "Only <libtracker-miner/tracker-miner.h> can be included directly."
#endif

#include <glib-object.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_NETWORK_PROVIDER             (tracker_network_provider_get_type())
#define TRACKER_NETWORK_PROVIDER(o)               (G_TYPE_CHECK_INSTANCE_CAST ((o),    TRACKER_TYPE_NETWORK_PROVIDER, TrackerNetworkProvider))
#define TRACKER_IS_NETWORK_PROVIDER(o)            (G_TYPE_CHECK_INSTANCE_TYPE ((o),    TRACKER_TYPE_NETWORK_PROVIDER))
#define TRACKER_NETWORK_PROVIDER_GET_INTERFACE(o) (G_TYPE_INSTANCE_GET_INTERFACE ((o), TRACKER_TYPE_NETWORK_PROVIDER, TrackerNetworkProviderIface))

typedef struct TrackerNetworkProvider TrackerNetworkProvider;

/**
 * TrackerNetworkProviderStatus:
 * Enumerates the different types of connections that the device might use when
 * connected to internet. Note that not all providers might provide this
 * information.
 *
 * @TRACKER_NETWORK_PROVIDER_DISCONNECTED: Network is disconnected
 * @TRACKER_NETWORK_PROVIDER_UNKNOWN: Network status is unknown
 * @TRACKER_NETWORK_PROVIDER_GPRS: Network is connected over a GPRS connection
 * @TRACKER_NETWORK_PROVIDER_EDGE: Network is connected over an EDGE connection
 * @TRACKER_NETWORK_PROVIDER_3G: Network is connected over a 3G or faster
 * (HSDPA, UMTS, ...) connection
 * @TRACKER_NETWORK_PROVIDER_LAN: Network is connected over a local network
 * connection. This can be ethernet, wifi, etc.
 *
 * Since: 0.9
 **/
typedef enum {
	TRACKER_NETWORK_PROVIDER_DISCONNECTED,
	TRACKER_NETWORK_PROVIDER_UNKNOWN,
	TRACKER_NETWORK_PROVIDER_GPRS,
	TRACKER_NETWORK_PROVIDER_EDGE,
	TRACKER_NETWORK_PROVIDER_3G,
	TRACKER_NETWORK_PROVIDER_LAN
} TrackerNetworkProviderStatus;

/**
 * TrackerNetworkProviderIface
 * @parent_iface: parent object interface
 * @get_status: get the network status
 *
 * Since: 0.9
 **/
typedef struct {
	GTypeInterface parent_iface;

	TrackerNetworkProviderStatus (* get_status) (TrackerNetworkProvider *provider);
} TrackerNetworkProviderIface;

GType tracker_network_provider_get_type     (void) G_GNUC_CONST;

gchar * tracker_network_provider_get_name   (TrackerNetworkProvider *provider);
TrackerNetworkProviderStatus
        tracker_network_provider_get_status (TrackerNetworkProvider *provider);

/**
 * tracker_network_provider_get:
 *
 * This function <emphasis>MUST</emphasis> be defined by the implementation of
 * TrackerNetworkProvider.
 *
 * For example, tracker-network-provider-network-manager.c should include this
 * function for a NetworkManager implementation.
 *
 * Only one implementation can exist at once.
 *
 * Returns: a %TrackerNetworkProvider.
 *
 * Since: 0.9
 **/
TrackerNetworkProvider *
        tracker_network_provider_get        (void);

G_END_DECLS

#endif /* __LIBTRACKER_MINER_NETWORK_PROVIDER_H__ */
