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

#include <glib-object.h>

#include <libnm-glib/nm-client.h>
#include <libnm-glib/nm-device-ethernet.h>
#include <libnm-glib/nm-device-wifi.h>
#include <libnm-glib/nm-gsm-device.h>
#include <libnm-glib/nm-cdma-device.h>

#include "tracker-network-provider.h"

#define TRACKER_TYPE_NETWORK_PROVIDER_NETWORK_MANAGER         (tracker_network_provider_network_manager_get_type())
#define TRACKER_NETWORK_PROVIDER_NETWORK_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_NETWORK_PROVIDER_NETWORK_MANAGER, TrackerNetworkProviderNetworkManager))
#define TRACKER_NETWORK_PROVIDER_NETWORK_MANAGER_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    TRACKER_TYPE_NETWORK_PROVIDER_NETWORK_MANAGER, TrackerNetworkProviderNetworkManagerClass))
#define TRACKER_IS_NETWORK_PROVIDER_NETWORK_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_NETWORK_PROVIDER_NETWORK_MANAGER))
#define TRACKER_IS_NETWORK_PROVIDER_NETWORK_MANAGER_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    TRACKER_TYPE_NETWORK_PROVIDER_NETWORK_MANAGER))
#define TRACKER_NETWORK_PROVIDER_NETWORK_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  TRACKER_TYPE_NETWORK_PROVIDER_NETWORK_MANAGER, TrackerNetworkProviderNetworkManagerClass))

#define TRACKER_NETWORK_PROVIDER_NETWORK_MANAGER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_NETWORK_PROVIDER_NETWORK_MANAGER, TrackerNetworkProviderNetworkManagerPrivate))

#define NETWORK_PROVIDER_NETWORK_MANAGER_NAME "NetworkManager"

typedef struct TrackerNetworkProviderNetworkManager TrackerNetworkProviderNetworkManager;
typedef struct TrackerNetworkProviderNetworkManagerClass TrackerNetworkProviderNetworkManagerClass;
typedef struct TrackerNetworkProviderNetworkManagerPrivate TrackerNetworkProviderNetworkManagerPrivate;

struct TrackerNetworkProviderNetworkManager {
	GObject parent;
};

struct TrackerNetworkProviderNetworkManagerClass {
	GObjectClass parent_class;
};

struct TrackerNetworkProviderNetworkManagerPrivate {
	gchar *name;

	NMClient *client;
	TrackerNetworkProviderStatus last_emitted_status;
};

GType        tracker_network_provider_network_manager_get_type (void) G_GNUC_CONST;
static void  tracker_network_provider_iface_init               (TrackerNetworkProviderIface *iface);
static void  network_provider_finalize                         (GObject                *object);
static void  network_provider_set_property                     (GObject                *object,
                                                                guint                   prop_id,
                                                                const GValue           *value,
                                                                GParamSpec             *pspec);
static void  network_provider_get_property                     (GObject                *object,
                                                                guint                   prop_id,
                                                                GValue                 *value,
                                                                GParamSpec             *pspec);
static void  network_provider_connections_changed              (GObject                *object,
                                                                GParamSpec             *pspec,
                                                                gpointer                user_data);
static TrackerNetworkProviderStatus
             network_provider_get_status                       (TrackerNetworkProvider *provider);

enum {
	PROP_0,
	PROP_NAME
};

G_DEFINE_TYPE_WITH_CODE (TrackerNetworkProviderNetworkManager,
                         tracker_network_provider_network_manager,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (TRACKER_TYPE_NETWORK_PROVIDER,
                                                tracker_network_provider_iface_init))

static void
tracker_network_provider_network_manager_class_init (TrackerNetworkProviderNetworkManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = network_provider_set_property;
	object_class->get_property = network_provider_get_property;
	object_class->finalize     = network_provider_finalize;

	g_object_class_override_property (object_class, PROP_NAME, "name");

	g_type_class_add_private (object_class, sizeof (TrackerNetworkProviderNetworkManagerPrivate));
}

static void
tracker_network_provider_network_manager_init (TrackerNetworkProviderNetworkManager *provider)
{
	TrackerNetworkProviderNetworkManagerPrivate *priv;

	priv = TRACKER_NETWORK_PROVIDER_NETWORK_MANAGER_GET_PRIVATE (provider);

	priv->client = nm_client_new ();
	priv->last_emitted_status = TRACKER_NETWORK_PROVIDER_DISCONNECTED;
	g_signal_connect (priv->client, "notify::state",
	                  G_CALLBACK (network_provider_connections_changed),
	                  provider);
}

static void
tracker_network_provider_iface_init (TrackerNetworkProviderIface *iface)
{
	iface->get_status = network_provider_get_status;
}

/*
 * Returns the first NMActiveConnection with the "default" property set, or
 * NULL if none is found.
 */
static NMActiveConnection*
find_default_active_connection (NMClient *client)
{
	const GPtrArray *active_connections;
	gint i;
	NMActiveConnection *active_connection = NULL;

	active_connections = nm_client_get_active_connections (client);

	for (i = 0; i < active_connections->len; i++) {
		active_connection = g_ptr_array_index (active_connections, i);

		if (nm_active_connection_get_default (active_connection)) {
			break;
		}
	}

	return active_connection;
}

static void
network_provider_finalize (GObject *object)
{
	TrackerNetworkProviderNetworkManagerPrivate *priv;

	priv = TRACKER_NETWORK_PROVIDER_NETWORK_MANAGER_GET_PRIVATE (object);

	g_object_unref (priv->client);
	g_free (priv->name);
}

static void
network_provider_set_property (GObject       *object,
                               guint          prop_id,
                               const GValue  *value,
                               GParamSpec    *pspec)
{
	TrackerNetworkProviderNetworkManagerPrivate *priv;

	priv = TRACKER_NETWORK_PROVIDER_NETWORK_MANAGER_GET_PRIVATE (object);

	switch (prop_id) {
	case PROP_NAME:
		g_free (priv->name);
		priv->name = g_value_dup_string (value);
		g_object_notify (object, "name");
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	};
}

static void
network_provider_get_property (GObject     *object,
                               guint        prop_id,
                               GValue      *value,
                               GParamSpec  *pspec)
{
	TrackerNetworkProviderNetworkManagerPrivate *priv;

	priv = TRACKER_NETWORK_PROVIDER_NETWORK_MANAGER_GET_PRIVATE (object);

	switch (prop_id) {
	case PROP_NAME:
		g_value_set_string (value, priv->name);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	};
}

static void
network_provider_connections_changed (GObject    *object,
                                      GParamSpec *pspec,
                                      gpointer    user_data)
{
	TrackerNetworkProviderNetworkManagerPrivate *priv;
	TrackerNetworkProvider *provider;
	TrackerNetworkProviderStatus status;

	g_return_if_fail (TRACKER_IS_NETWORK_PROVIDER_NETWORK_MANAGER (user_data));

	priv = TRACKER_NETWORK_PROVIDER_NETWORK_MANAGER_GET_PRIVATE (object);

	provider = user_data;
	status = tracker_network_provider_get_status (provider);

	if (status == priv->last_emitted_status) {
		return;
	}

	priv->last_emitted_status = status;

	g_signal_emit_by_name (provider, "status-changed", status);
}

static TrackerNetworkProviderStatus
network_provider_get_status (TrackerNetworkProvider *provider)
{
	TrackerNetworkProviderNetworkManagerPrivate *priv;
	NMActiveConnection *default_active_connection;
	const GPtrArray *devices;
	NMDevice *device;

	priv = TRACKER_NETWORK_PROVIDER_NETWORK_MANAGER_GET_PRIVATE (provider);

	if (!nm_client_get_manager_running (priv->client)) {
		return TRACKER_NETWORK_PROVIDER_UNKNOWN;
	}

	switch (nm_client_get_state (priv->client)) {
	case NM_STATE_UNKNOWN:
		return TRACKER_NETWORK_PROVIDER_UNKNOWN;
	case NM_STATE_CONNECTED:
		break;
	default:
		return TRACKER_NETWORK_PROVIDER_DISCONNECTED;
	}

	default_active_connection = find_default_active_connection (priv->client);

	if (!default_active_connection) {
		return TRACKER_NETWORK_PROVIDER_DISCONNECTED;
	}

	switch (nm_active_connection_get_state (default_active_connection)) {
	case NM_ACTIVE_CONNECTION_STATE_UNKNOWN:
		return TRACKER_NETWORK_PROVIDER_UNKNOWN;
	case NM_ACTIVE_CONNECTION_STATE_ACTIVATED:
		break;
	default:
		return TRACKER_NETWORK_PROVIDER_DISCONNECTED;
	}

	devices = nm_active_connection_get_devices (default_active_connection);

	if (!devices->len) {
		return TRACKER_NETWORK_PROVIDER_DISCONNECTED;
	}

	/* Pick the first device, I don't know when there are more than one */
	device = g_ptr_array_index (devices, 0);

	switch (nm_device_get_state (device)) {
	case NM_DEVICE_STATE_UNKNOWN:
		return TRACKER_NETWORK_PROVIDER_UNKNOWN;
		break;
	case NM_DEVICE_STATE_ACTIVATED:
		break;
	default:
		return TRACKER_NETWORK_PROVIDER_DISCONNECTED;
	}

	if (NM_IS_DEVICE_ETHERNET (device) || NM_IS_DEVICE_WIFI (device)) {
		return TRACKER_NETWORK_PROVIDER_LAN;
	}

	if (NM_IS_SERIAL_DEVICE (device)) {
		if (NM_IS_GSM_DEVICE (device)) {
			return TRACKER_NETWORK_PROVIDER_GPRS;
		}

		if (NM_IS_CDMA_DEVICE (device)) {
			return TRACKER_NETWORK_PROVIDER_3G;
		}
	}

	/* We know the device is activated, but we don't know the type of device */
	return TRACKER_NETWORK_PROVIDER_UNKNOWN;
}

TrackerNetworkProvider *
tracker_network_provider_get (void)
{
	static TrackerNetworkProvider *instance = NULL;
	static GStaticMutex mutex = G_STATIC_MUTEX_INIT;

	g_static_mutex_lock (&mutex);

	if (!instance) {
		instance = g_object_new (TRACKER_TYPE_NETWORK_PROVIDER_NETWORK_MANAGER,
		                         "name", NETWORK_PROVIDER_NETWORK_MANAGER_NAME,
		                         NULL);
	}

	g_static_mutex_unlock (&mutex);

	return instance;
}

