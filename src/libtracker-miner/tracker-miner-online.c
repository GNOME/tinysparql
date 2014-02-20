/*
 * Copyright (C) 2009-2014, Adrien Bustany <abustany@gnome.org>
 * Copyright (C) 2014, Carlos Garnacho <carlosg@gnome.org>
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

#include "tracker-miner-online.h"
#include "tracker-miner-enum-types.h"

#include <glib/gi18n.h>

#ifdef HAVE_NETWORK_MANAGER
#include <libnm-glib/nm-client.h>

#ifndef NM_CHECK_VERSION
#define NM_CHECK_VERSION(x,y,z) (0)
#endif /* NM_CHECK_VERSION */

#include <libnm-glib/nm-device-ethernet.h>
#include <libnm-glib/nm-device-wifi.h>
#if (NM_CHECK_VERSION (0,8,992))
#include <libnm-glib/nm-device-modem.h>
#include <libnm-glib/nm-device-wimax.h>
#else
#include <libnm-glib/nm-gsm-device.h>
#include <libnm-glib/nm-cdma-device.h>
#endif
#endif /* HAVE_NETWORK_MANAGER */

/**
 * SECTION:tracker-miner-online
 * @short_description: Abstract base class for miners connecting to
 *   online resources
 * @include: libtracker-miner/tracker-miner.h
 *
 * #TrackerMinerOnline is an abstract base class for miners retrieving data
 * from online resources. It's a very thin layer above #TrackerMiner that
 * additionally handles network connection status.
 *
 * #TrackerMinerOnline implementations can implement the
 * <literal>connected</literal> vmethod in order to tell the miner whether
 * a connection is valid to retrieve data or not. The miner data extraction
 * still must be dictated through the #TrackerMiner vmethods.
 **/

typedef struct _TrackerMinerOnlinePrivate TrackerMinerOnlinePrivate;

struct _TrackerMinerOnlinePrivate {
#ifdef HAVE_NETWORK_MANAGER
	NMClient *client;
#endif
	TrackerNetworkType network_type;
	gint pause_id;
};

enum {
	PROP_NETWORK_TYPE = 1
};

enum {
	CONNECTED,
	DISCONNECTED,
	N_SIGNALS
};

static void       miner_online_initable_iface_init (GInitableIface         *iface);

static GInitableIface* miner_online_initable_parent_iface;
static guint signals[N_SIGNALS] = { 0 };

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (TrackerMinerOnline, tracker_miner_online, TRACKER_TYPE_MINER,
                                  G_ADD_PRIVATE (TrackerMinerOnline)
                                  G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                         miner_online_initable_iface_init));

static void
miner_online_finalize (GObject *object)
{
#ifdef HAVE_NETWORK_MANAGER
	TrackerMinerOnlinePrivate *priv;
	TrackerMinerOnline *miner;

	miner = TRACKER_MINER_ONLINE (object);
	priv = tracker_miner_online_get_instance_private (miner);

	if (priv->client)
		g_object_unref (priv->client);
#endif /* HAVE_NETWORK_MANAGER */

	G_OBJECT_CLASS (tracker_miner_online_parent_class)->finalize (object);
}

static void
miner_online_set_property (GObject      *object,
                           guint         param_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
	switch (param_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
miner_online_get_property (GObject    *object,
                           guint       param_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
	TrackerMinerOnlinePrivate *priv;
	TrackerMinerOnline *miner;

	miner = TRACKER_MINER_ONLINE (object);
	priv = tracker_miner_online_get_instance_private (miner);

	switch (param_id) {
	case PROP_NETWORK_TYPE:
		g_value_set_enum (value, priv->network_type);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
tracker_miner_online_class_init (TrackerMinerOnlineClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize     = miner_online_finalize;
	object_class->set_property = miner_online_set_property;
	object_class->get_property = miner_online_get_property;

	g_object_class_install_property (object_class,
	                                 PROP_NETWORK_TYPE,
	                                 g_param_spec_enum ("network-type",
	                                                    "Network type",
	                                                    "Network type for the current connection",
	                                                    TRACKER_TYPE_NETWORK_TYPE,
	                                                    TRACKER_NETWORK_TYPE_NONE,
	                                                    G_PARAM_READABLE));
	signals[CONNECTED] =
		g_signal_new ("connected",
		              G_OBJECT_CLASS_TYPE (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (TrackerMinerOnlineClass, connected),
		              NULL, NULL, NULL,
		              G_TYPE_BOOLEAN, 1, TRACKER_TYPE_NETWORK_TYPE);
	signals[DISCONNECTED] =
		g_signal_new ("disconnected",
		              G_OBJECT_CLASS_TYPE (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (TrackerMinerOnlineClass, connected),
		              NULL, NULL, NULL,
		              G_TYPE_NONE, 0);
}

static void
tracker_miner_online_init (TrackerMinerOnline *miner)
{
	TrackerMinerOnlinePrivate *priv;

	priv = tracker_miner_online_get_instance_private (miner);
	priv->network_type = TRACKER_NETWORK_TYPE_NONE;
}

#ifdef HAVE_NETWORK_MANAGER
/*
 * Returns the first NMActiveConnection with the "default" property set, or
 * NULL if none is found.
 */
static NMActiveConnection*
find_default_active_connection (NMClient *client)
{
	NMActiveConnection *active_connection = NULL;
	const GPtrArray *active_connections;
	gint i;

	active_connections = nm_client_get_active_connections (client);

	for (i = 0; i < active_connections->len; i++) {
		active_connection = g_ptr_array_index (active_connections, i);

		if (nm_active_connection_get_default (active_connection)) {
			break;
		}
	}

	return active_connection;
}

static TrackerNetworkType
_nm_client_get_network_type (NMClient *nm_client)
{
	NMActiveConnection *default_active_connection;
	const GPtrArray *devices;
	NMDevice *device;

	if (!nm_client_get_manager_running (nm_client)) {
		return TRACKER_NETWORK_TYPE_UNKNOWN;
	}

	switch (nm_client_get_state (nm_client)) {
	case NM_STATE_UNKNOWN:
		return TRACKER_NETWORK_TYPE_UNKNOWN;
	case NM_STATE_CONNECTED:
		break;
	default:
		return TRACKER_NETWORK_TYPE_NONE;
	}

	default_active_connection = find_default_active_connection (nm_client);

	if (!default_active_connection) {
		return TRACKER_NETWORK_TYPE_NONE;
	}

	switch (nm_active_connection_get_state (default_active_connection)) {
	case NM_ACTIVE_CONNECTION_STATE_UNKNOWN:
		return TRACKER_NETWORK_TYPE_UNKNOWN;
	case NM_ACTIVE_CONNECTION_STATE_ACTIVATED:
		break;
	default:
		return TRACKER_NETWORK_TYPE_NONE;
	}

	devices = nm_active_connection_get_devices (default_active_connection);

	if (!devices->len) {
		return TRACKER_NETWORK_TYPE_NONE;
	}

	/* Pick the first device, I don't know when there are more than one */
	device = g_ptr_array_index (devices, 0);

	switch (nm_device_get_state (device)) {
	case NM_DEVICE_STATE_UNKNOWN:
		return TRACKER_NETWORK_TYPE_UNKNOWN;
		break;
	case NM_DEVICE_STATE_ACTIVATED:
		break;
	default:
		return TRACKER_NETWORK_TYPE_NONE;
	}

	if (NM_IS_DEVICE_ETHERNET (device) || NM_IS_DEVICE_WIFI (device)) {
		return TRACKER_NETWORK_TYPE_LAN;
	}

#if (NM_CHECK_VERSION (0,8,992))
	if (NM_IS_DEVICE_MODEM (device) || NM_IS_DEVICE_WIMAX (device)) {
		return TRACKER_NETWORK_TYPE_3G;
	}
#else
	if (NM_IS_GSM_DEVICE (device) || NM_IS_CDMA_DEVICE (device)) {
		return TRACKER_NETWORK_TYPE_3G;
	}
#endif

	/* We know the device is activated, but we don't know the type of device */
	return TRACKER_NETWORK_TYPE_UNKNOWN;
}

static void
_tracker_miner_online_set_network_type (TrackerMinerOnline *miner,
                                        TrackerNetworkType  type)
{
	TrackerMinerOnlinePrivate *priv;
	gboolean cont = FALSE;
	GError *error = NULL;

	priv = tracker_miner_online_get_instance_private (miner);

	if (type == priv->network_type) {
		return;
	}

	priv->network_type = type;

	if (type != TRACKER_NETWORK_TYPE_NONE) {
		g_signal_emit (miner, signals[CONNECTED], 0, type, &cont);
	} else {
		g_signal_emit (miner, signals[DISCONNECTED], 0);
	}

	if (cont && priv->pause_id)
		tracker_miner_resume (TRACKER_MINER (miner),
		                      priv->pause_id, &error);
	else if (!cont && !priv->pause_id) {
		const gchar *msg;

		msg = (type == TRACKER_NETWORK_TYPE_NONE) ?
			_("No network connection") :
			_("Indexing not recommended on this network connection");

		priv->pause_id =
			tracker_miner_pause (TRACKER_MINER (miner),
			                     msg, &error);
	}

	if (error) {
		g_warning ("There was an error after getting network type %d: %s",
		           type, error->message);
		g_error_free (error);
	}
}

static void
_nm_client_state_notify_cb (GObject            *object,
                            GParamSpec         *pspec,
                            TrackerMinerOnline *miner)
{
	TrackerMinerOnlinePrivate *priv;
	TrackerNetworkType type;

	priv = tracker_miner_online_get_instance_private (miner);
	type = _nm_client_get_network_type (priv->client);
	_tracker_miner_online_set_network_type (miner, type);
}
#endif /* HAVE_NETWORK_MANAGER */

static gboolean
miner_online_initable_init (GInitable     *initable,
                            GCancellable  *cancellable,
                            GError       **error)
{
#ifdef HAVE_NETWORK_MANAGER
	TrackerMinerOnlinePrivate *priv;
	TrackerNetworkType network_type;
	TrackerMinerOnline *miner;

	miner = TRACKER_MINER_ONLINE (initable);

	priv = tracker_miner_online_get_instance_private (miner);
#endif /* HAVE_NETWORK_MANAGER */

	if (!miner_online_initable_parent_iface->init (initable,
	                                               cancellable, error)) {
		return FALSE;
	}

#ifdef HAVE_NETWORK_MANAGER
	priv->client = nm_client_new ();
	g_signal_connect (priv->client, "notify::state",
	                  G_CALLBACK (_nm_client_state_notify_cb), miner);
	network_type = _nm_client_get_network_type (priv->client);
	_tracker_miner_online_set_network_type (miner, network_type);
#endif

	return TRUE;
}

static void
miner_online_initable_iface_init (GInitableIface *iface)
{
	miner_online_initable_parent_iface = g_type_interface_peek_parent (iface);
	iface->init = miner_online_initable_init;
}

TrackerNetworkType
tracker_miner_online_get_network_type (TrackerMinerOnline *miner)
{
	TrackerMinerOnlinePrivate *priv;

	priv = tracker_miner_online_get_instance_private (miner);

	return priv->network_type;
}
