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

#include "tracker-network-provider.h"

#define TRACKER_TYPE_NETWORK_PROVIDER_DUMMY         (tracker_network_provider_dummy_get_type())
#define TRACKER_NETWORK_PROVIDER_DUMMY(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_NETWORK_PROVIDER_DUMMY, TrackerNetworkProviderDummy))
#define TRACKER_NETWORK_PROVIDER_DUMMY_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    TRACKER_TYPE_NETWORK_PROVIDER_DUMMY, TrackerNetworkProviderDummyClass))
#define TRACKER_IS_NETWORK_PROVIDER_DUMMY(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_NETWORK_PROVIDER_DUMMY))
#define TRACKER_IS_NETWORK_PROVIDER_DUMMY_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    TRACKER_TYPE_NETWORK_PROVIDER_DUMMY))
#define TRACKER_NETWORK_PROVIDER_DUMMY_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  TRACKER_TYPE_NETWORK_PROVIDER_DUMMY, TrackerNetworkProviderDummyClass))

#define TRACKER_NETWORK_PROVIDER_DUMMY_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_NETWORK_PROVIDER_DUMMY, TrackerNetworkProviderDummyPrivate))

#define NETWORK_PROVIDER_DUMMY_NAME "Dummy"

typedef struct TrackerNetworkProviderDummy TrackerNetworkProviderDummy;
typedef struct TrackerNetworkProviderDummyClass TrackerNetworkProviderDummyClass;
typedef struct TrackerNetworkProviderDummyPrivate TrackerNetworkProviderDummyPrivate;

struct TrackerNetworkProviderDummy {
	GObject parent;
};

struct TrackerNetworkProviderDummyClass {
	GObjectClass parent_class;
};

struct TrackerNetworkProviderDummyPrivate {
	gchar *name;
};

GType        tracker_network_provider_dummy_get_type (void) G_GNUC_CONST;
static void  tracker_network_provider_iface_init     (TrackerNetworkProviderIface *iface);
static void  network_provider_set_property           (GObject                     *object,
                                                      guint                        prop_id,
                                                      const GValue                *value,
                                                      GParamSpec                  *pspec);
static void  network_provider_get_property           (GObject                     *object,
                                                      guint                        prop_id,
                                                      GValue                      *value,
                                                      GParamSpec                  *pspec);
static TrackerNetworkProviderStatus
             network_provider_dummy_get_status       (TrackerNetworkProvider      *provider);

enum {
	PROP_0,
	PROP_NAME
};

G_DEFINE_TYPE_WITH_CODE (TrackerNetworkProviderDummy,
                         tracker_network_provider_dummy,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (TRACKER_TYPE_NETWORK_PROVIDER,
                                                tracker_network_provider_iface_init))

static void
tracker_network_provider_dummy_class_init (TrackerNetworkProviderDummyClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = network_provider_set_property;
	object_class->get_property = network_provider_get_property;

	g_object_class_override_property (object_class, PROP_NAME, "name");

	g_type_class_add_private (object_class, sizeof (TrackerNetworkProviderDummyPrivate));
}

static void
tracker_network_provider_dummy_init (TrackerNetworkProviderDummy *provider)
{
}

static void
tracker_network_provider_iface_init (TrackerNetworkProviderIface *iface)
{
	iface->get_status = network_provider_dummy_get_status;
}

static void
network_provider_set_property (GObject       *object,
                               guint          prop_id,
                               const GValue  *value,
                               GParamSpec    *pspec)
{
	TrackerNetworkProviderDummyPrivate *priv;

	priv = TRACKER_NETWORK_PROVIDER_DUMMY_GET_PRIVATE (object);

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
	TrackerNetworkProviderDummyPrivate *priv;

	priv = TRACKER_NETWORK_PROVIDER_DUMMY_GET_PRIVATE (object);

	switch (prop_id) {
	case PROP_NAME:
		g_value_set_string (value, priv->name);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	};
}

static TrackerNetworkProviderStatus
network_provider_dummy_get_status (TrackerNetworkProvider *provider)
{
	return TRACKER_NETWORK_PROVIDER_UNKNOWN;
}

TrackerNetworkProvider *
tracker_network_provider_get (void)
{
	static TrackerNetworkProvider *instance = NULL;
	static GStaticMutex mutex = G_STATIC_MUTEX_INIT;

	g_static_mutex_lock (&mutex);

	if (!instance) {
		instance = g_object_new (TRACKER_TYPE_NETWORK_PROVIDER_DUMMY,
		                         "name", NETWORK_PROVIDER_DUMMY_NAME,
		                         NULL);
	}

	g_static_mutex_unlock (&mutex);

	return instance;
}
