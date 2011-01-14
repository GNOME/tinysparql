/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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

#ifdef HAVE_UPOWER

#include <upower.h>

#include "tracker-power.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_POWER, TrackerPowerPriv))

typedef struct {
	UpClient  *client;
	gboolean   on_battery;
	gboolean   on_low_battery;
} TrackerPowerPriv;

static void     tracker_power_finalize          (GObject         *object);
static void     tracker_power_get_property      (GObject         *object,
                                                 guint            param_id,
                                                 GValue                  *value,
                                                 GParamSpec      *pspec);
static void     tracker_power_client_changed_cb (UpClient        *client,
                                                 TrackerPower    *power);

enum {
	PROP_0,
	PROP_ON_BATTERY,
	PROP_ON_LOW_BATTERY
};

G_DEFINE_TYPE (TrackerPower, tracker_power, G_TYPE_OBJECT);

static void
tracker_power_class_init (TrackerPowerClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize     = tracker_power_finalize;
	object_class->get_property = tracker_power_get_property;

	g_object_class_install_property (object_class,
	                                 PROP_ON_BATTERY,
	                                 g_param_spec_boolean ("on-battery",
	                                                       "Battery in use",
	                                                       "Whether the battery is being used",
	                                                       FALSE,
	                                                       G_PARAM_READABLE));

	g_object_class_install_property (object_class,
	                                 PROP_ON_LOW_BATTERY,
	                                 g_param_spec_boolean ("on-low-battery",
	                                                       "Battery low",
	                                                       "Whether the battery is low",
	                                                       FALSE,
	                                                       G_PARAM_READABLE));

	g_type_class_add_private (object_class, sizeof (TrackerPowerPriv));
}

static void
tracker_power_init (TrackerPower *power)
{
	TrackerPowerPriv *priv;

	g_message ("Initializing UPower...");

	priv = GET_PRIV (power);

	/* connect to a UPower instance */
	priv->client = up_client_new ();
	g_signal_connect (priv->client, "changed",
	                  G_CALLBACK (tracker_power_client_changed_cb), power);

	/* coldplug */
	priv->on_battery = up_client_get_on_battery (priv->client);
	priv->on_low_battery = up_client_get_on_low_battery (priv->client);
}

static void
tracker_power_finalize (GObject *object)
{
	TrackerPowerPriv *priv;

	priv = GET_PRIV (object);

	g_object_unref (priv->client);

	(G_OBJECT_CLASS (tracker_power_parent_class)->finalize) (object);
}

static void
tracker_power_get_property (GObject    *object,
                            guint       param_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
	TrackerPowerPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_ON_BATTERY:
		g_value_set_boolean (value, priv->on_battery);
		break;
	case PROP_ON_LOW_BATTERY:
		g_value_set_boolean (value, priv->on_low_battery);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

/**
 * tracker_power_client_changed_cb:
 **/
static void
tracker_power_client_changed_cb (UpClient *client, TrackerPower *power)
{
	gboolean on_battery;
	gboolean on_low_battery;
	TrackerPowerPriv *priv;

	priv = GET_PRIV (power);

	/* get the on-battery state */
	on_battery = up_client_get_on_battery (priv->client);
	if (on_battery != priv->on_battery) {
		priv->on_battery = on_battery;
		g_object_notify (G_OBJECT (power), "on-battery");
	}

	/* get the on-low-battery state */
	on_low_battery = up_client_get_on_low_battery (priv->client);
	if (on_low_battery != priv->on_low_battery) {
		priv->on_low_battery = on_low_battery;
		g_object_notify (G_OBJECT (power), "on-low-battery");
	}
}

/**
 * tracker_power_new:
 *
 * Creates a new instance of #TrackerPower.
 *
 * Returns: The newly created #TrackerPower.
 **/
TrackerPower *
tracker_power_new ()
{
	return g_object_new (TRACKER_TYPE_POWER, NULL);
}

/**
 * tracker_power_get_on_battery:
 * @power: A #TrackerPower.
 *
 * Returns whether the computer battery (if any) is currently in use.
 *
 * Returns: #TRUE if the computer is running on battery power.
 **/
gboolean
tracker_power_get_on_battery (TrackerPower *power)
{
	TrackerPowerPriv *priv;

	g_return_val_if_fail (TRACKER_IS_POWER (power), TRUE);

	priv = GET_PRIV (power);

	return priv->on_battery;
}

/**
 * tracker_power_get_on_low_battery:
 * @power: A #TrackerPower
 *
 * Returns whether the computer has batteries.
 *
 * Returns: #TRUE if the computer has batteries available.
 **/
gboolean
tracker_power_get_on_low_battery (TrackerPower *power)
{
	TrackerPowerPriv *priv;

	g_return_val_if_fail (TRACKER_IS_POWER (power), TRUE);

	priv = GET_PRIV (power);

	return priv->on_low_battery;
}

#endif /* HAVE_UPOWER */
