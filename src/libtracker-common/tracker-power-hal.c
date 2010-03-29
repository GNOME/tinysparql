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

#ifdef HAVE_HAL

#include <string.h>

#include <libhal.h>

#include <dbus/dbus-glib-lowlevel.h>

#include "tracker-log.h"
#include "tracker-power.h"
#include "tracker-utils.h"
#include "tracker-marshal.h"

#define CAPABILITY_AC_ADAPTER  "ac_adapter"
#define CAPABILITY_BATTERY     "battery"

#define PROP_AC_ADAPTER_ON     "ac_adapter.present"
#define PROP_BATT_PERCENTAGE   "battery.charge_level.percentage"

#define BATTERY_LOW_THRESHOLD  0.05f

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_POWER, TrackerPowerPriv))

typedef struct {
	LibHalContext *context;
	DBusConnection *connection;

	GHashTable    *batteries;

	gchar         *ac_adapter_udi;
	gboolean       on_battery;
	gdouble        battery_percentage;
} TrackerPowerPriv;

static void     tracker_power_finalize          (GObject         *object);
static void     hal_get_property                (GObject         *object,
                                                 guint            param_id,
                                                 GValue                  *value,
                                                 GParamSpec      *pspec);
static gboolean hal_setup_ac_adapters           (TrackerPower    *power);
static gboolean hal_setup_batteries             (TrackerPower    *power);

static void     hal_battery_modify              (TrackerPower    *power,
                                                 const gchar     *udi);
static void     hal_battery_remove              (TrackerPower    *power,
                                                 const gchar     *udi);

static void     hal_device_added_cb             (LibHalContext   *context,
                                                 const gchar     *udi);
static void     hal_device_removed_cb           (LibHalContext   *context,
                                                 const gchar     *udi);
static void     hal_device_property_modified_cb (LibHalContext   *context,
                                                 const char      *udi,
                                                 const char      *key,
                                                 dbus_bool_t      is_removed,
                                                 dbus_bool_t      is_added);

enum {
	PROP_0,
	PROP_ON_BATTERY,
	PROP_ON_LOW_BATTERY,
	PROP_BATTERY_PERCENTAGE
};

G_DEFINE_TYPE (TrackerPower, tracker_power, G_TYPE_OBJECT);

static void
tracker_power_class_init (TrackerPowerClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize     = tracker_power_finalize;
	object_class->get_property = hal_get_property;

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
	g_object_class_install_property (object_class,
	                                 PROP_BATTERY_PERCENTAGE,
	                                 g_param_spec_double ("battery-percentage",
	                                                      "Battery percentage",
	                                                      "Current battery percentage left",
	                                                      0.0,
	                                                      1.0,
	                                                      0.0,
	                                                      G_PARAM_READABLE));

	g_type_class_add_private (object_class, sizeof (TrackerPowerPriv));
}

static void
tracker_power_init (TrackerPower *power)
{
	TrackerPowerPriv *priv;
	DBusError       error;

	g_message ("Initializing HAL Power...");

	priv = GET_PRIV (power);

	priv->batteries = g_hash_table_new_full (g_str_hash,
	                                         g_str_equal,
	                                         (GDestroyNotify) g_free,
	                                         NULL);

	dbus_error_init (&error);

	priv->connection = dbus_bus_get (DBUS_BUS_SYSTEM, &error);
	if (dbus_error_is_set (&error)) {
		g_critical ("Could not get the system D-Bus connection, %s",
		            error.message);
		dbus_error_free (&error);
		return;
	}

	dbus_connection_set_exit_on_disconnect (priv->connection, FALSE);
	dbus_connection_setup_with_g_main (priv->connection, NULL);

	priv->context = libhal_ctx_new ();

	if (!priv->context) {
		g_critical ("Could not create HAL context");
		return;
	}

	libhal_ctx_set_user_data (priv->context, power);
	libhal_ctx_set_dbus_connection (priv->context, priv->connection);

	if (!libhal_ctx_init (priv->context, &error)) {
		if (dbus_error_is_set (&error)) {
			g_critical ("Could not initialize the HAL context, %s",
			            error.message);
			dbus_error_free (&error);
		} else {
			g_critical ("Could not initialize the HAL context, "
			            "no error, is hald running?");
		}

		libhal_ctx_free (priv->context);
		priv->context = NULL;
		return;
	}


	/* Volume and property notification callbacks */
	g_message ("HAL monitors set for devices added/removed/mounted/umounted...");
	libhal_ctx_set_device_added (priv->context, hal_device_added_cb);
	libhal_ctx_set_device_removed (priv->context, hal_device_removed_cb);
	libhal_ctx_set_device_property_modified (priv->context, hal_device_property_modified_cb);

	/* Get all AC adapters info and set them up */
	if (!hal_setup_ac_adapters (power)) {
		return;
	}

	/* Get all battery devices and set them up */
	if (!hal_setup_batteries (power)) {
		return;
	}
}

static void
tracker_power_finalize (GObject *object)
{
	TrackerPowerPriv *priv;

	priv = GET_PRIV (object);

	if (priv->batteries) {
		g_hash_table_unref (priv->batteries);
	}

	g_free (priv->ac_adapter_udi);

	if (priv->context) {
		libhal_ctx_shutdown (priv->context, NULL);
		libhal_ctx_set_user_data (priv->context, NULL);
		libhal_ctx_free (priv->context);
	}

	if (priv->connection) {
		dbus_connection_unref (priv->connection);
	}

	(G_OBJECT_CLASS (tracker_power_parent_class)->finalize) (object);
}

static void
hal_get_property (GObject    *object,
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
		/* hardcoded to 5% */
		g_value_set_boolean (value, priv->battery_percentage < BATTERY_LOW_THRESHOLD);
		break;
	case PROP_BATTERY_PERCENTAGE:
		g_value_set_double (value, priv->battery_percentage);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static gboolean
hal_setup_ac_adapters (TrackerPower *power)
{
	TrackerPowerPriv        *priv;
	DBusError        error;
	gchar          **devices, **p;
	gint             num;

	priv = GET_PRIV (power);

	dbus_error_init (&error);

	devices = libhal_find_device_by_capability (priv->context,
	                                            CAPABILITY_AC_ADAPTER,
	                                            &num,
	                                            &error);

	if (dbus_error_is_set (&error)) {
		g_critical ("Could not get AC adapter capable devices, %s",
		            error.message);
		dbus_error_free (&error);
		return FALSE;
	}

	g_message ("HAL found %d AC adapter capable devices", num);

	if (!devices || !devices[0]) {
		libhal_free_string_array (devices);

		priv->on_battery = FALSE;
		g_object_notify (G_OBJECT (power), "on-battery");

		priv->ac_adapter_udi = NULL;

		return TRUE;
	}

	for (p = devices; *p; p++) {
		if (!priv->ac_adapter_udi) {
			/* For now just use the first one we find */
			priv->ac_adapter_udi = g_strdup (*p);

			g_message ("  Device '%s' (default)", *p);
		} else {
			g_message ("  Device '%s'", *p);
		}
	}

	libhal_free_string_array (devices);

	/* Make sure we watch changes to the battery use */
	libhal_device_add_property_watch (priv->context,
	                                  priv->ac_adapter_udi,
	                                  &error);

	if (dbus_error_is_set (&error)) {
		g_critical ("Could not add device:'%s' to property watch, %s",
		            priv->ac_adapter_udi, error.message);
		dbus_error_free (&error);
		return FALSE;
	}

	/* Get current state, are we using the battery now? */
	priv->on_battery = !libhal_device_get_property_bool (priv->context,
	                                                     priv->ac_adapter_udi,
	                                                     PROP_AC_ADAPTER_ON,
	                                                     NULL);

	g_message ("HAL reports system is currently powered by %s",
	           priv->on_battery ? "battery" : "AC adapter");

	g_object_notify (G_OBJECT (power), "on-battery");

	return TRUE;
}

static gboolean
hal_setup_batteries (TrackerPower *power)
{
	TrackerPowerPriv        *priv;
	DBusError        error;
	gchar          **devices, **p;
	gint             num;

	priv = GET_PRIV (power);

	dbus_error_init (&error);

	devices = libhal_find_device_by_capability (priv->context,
	                                            CAPABILITY_BATTERY,
	                                            &num,
	                                            &error);

	if (dbus_error_is_set (&error)) {
		g_critical ("Could not get Battery HAL info, %s",
		            error.message);
		dbus_error_free (&error);
		return FALSE;
	}

	g_message ("HAL found %d batteries", num);

	if (!devices || !devices[0]) {
		libhal_free_string_array (devices);
		return TRUE;
	}

	for (p = devices; *p; p++) {
		g_message ("  Device '%s'", *p);

		hal_battery_modify (power, *p);
		libhal_device_add_property_watch (priv->context, *p, &error);

		if (dbus_error_is_set (&error)) {
			g_critical ("Could not add device:'%s' to property watch, %s",
			            *p, error.message);
			dbus_error_free (&error);
		}
	}

	libhal_free_string_array (devices);

	return TRUE;
}

static void
hal_battery_notify (TrackerPower *power)
{
	TrackerPowerPriv *priv;
	GList *values, *v;
	gint percentage, n_values;
	gdouble old_percentage;

	priv = GET_PRIV (power);
	percentage = n_values = 0;

	values = g_hash_table_get_values (priv->batteries);

	for (v = values; v; v = v->next) {
		percentage += GPOINTER_TO_INT (v->data);
		n_values++;
	}

	old_percentage = priv->battery_percentage;

	if (n_values > 0) {
		priv->battery_percentage = (gdouble) percentage / n_values;
		priv->battery_percentage /= 100;
	} else {
		priv->battery_percentage = 0;
	}

	/* only notify when we cross the threshold up or down */
	if ((priv->battery_percentage > BATTERY_LOW_THRESHOLD &&
	     old_percentage <= BATTERY_LOW_THRESHOLD) ||
	    (priv->battery_percentage <= BATTERY_LOW_THRESHOLD &&
	     old_percentage > BATTERY_LOW_THRESHOLD)) {
		g_object_notify (G_OBJECT (power), "on-low-battery");
	}

	if (old_percentage != priv->battery_percentage) {
		g_object_notify (G_OBJECT (power), "battery-percentage");
	}

	g_list_free (values);
}

static void
hal_battery_modify (TrackerPower *power,
                    const gchar  *udi)
{
	TrackerPowerPriv *priv;
	gint percentage;

	priv = GET_PRIV (power);
	percentage = libhal_device_get_property_int (priv->context, udi,
	                                             PROP_BATT_PERCENTAGE,
	                                             NULL);

	g_hash_table_insert (priv->batteries,
	                     g_strdup (udi),
	                     GINT_TO_POINTER (percentage));

	hal_battery_notify (power);
}

static void
hal_battery_remove (TrackerPower *power,
                    const gchar  *udi)
{
	TrackerPowerPriv *priv;

	priv = GET_PRIV (power);

	g_hash_table_remove (priv->batteries, udi);
	hal_battery_notify (power);
}

static void
hal_device_added_cb (LibHalContext *context,
                     const gchar   *udi)
{
	TrackerPower   *power;

	power = libhal_ctx_get_user_data (context);

	if (libhal_device_query_capability (context, udi, CAPABILITY_BATTERY, NULL)) {
		hal_battery_modify (power, udi);
	}
}

static void
hal_device_removed_cb (LibHalContext *context,
                       const gchar   *udi)
{
	TrackerPower     *power;
	TrackerPowerPriv *priv;

	power = (TrackerPower*) libhal_ctx_get_user_data (context);
	priv = GET_PRIV (power);

	if (g_hash_table_lookup (priv->batteries, udi)) {
		hal_battery_remove (power, udi);
	}
}

static void
hal_device_property_modified_cb (LibHalContext *context,
                                 const char    *udi,
                                 const char    *key,
                                 dbus_bool_t    is_removed,
                                 dbus_bool_t    is_added)
{
	TrackerPower     *power;
	TrackerPowerPriv *priv;
	DBusError       error;

	power = (TrackerPower*) libhal_ctx_get_user_data (context);
	priv = GET_PRIV (power);

	dbus_error_init (&error);

	if (priv->ac_adapter_udi && strcmp (priv->ac_adapter_udi, udi) == 0) {
		/* Property change is on the AC adapter */
		priv->on_battery = !libhal_device_get_property_bool (priv->context,
		                                                     priv->ac_adapter_udi,
		                                                     PROP_AC_ADAPTER_ON,
		                                                     &error);
		g_message ("HAL reports system is now powered by %s",
		           priv->on_battery ? "battery" : "AC adapter");

		g_object_notify (G_OBJECT (power), "on-battery");

		if (dbus_error_is_set (&error)) {
			g_critical ("Could not get device property:'%s' for udi:'%s', %s",
			            udi, PROP_AC_ADAPTER_ON, error.message);
			dbus_error_free (&error);
			return;
		}
	} else if (g_hash_table_lookup (priv->batteries, udi)) {
		/* Property change is on any battery */
		if (strcmp (key, PROP_BATT_PERCENTAGE) == 0) {
			hal_battery_modify (power, udi);
		}
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
 * tracker_hal_get_on_battery:
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

	return (priv->battery_percentage < BATTERY_LOW_THRESHOLD);
}

/**
 * tracker_power_get_battery_percentage:
 * @power: A #TrackerPower
 *
 * Returns the percentage of battery power available.
 *
 * Returns: #gdouble representing the percentage between 0.0 and 1.0.
 **/
gdouble
tracker_power_get_battery_percentage (TrackerPower *power)
{
	TrackerPowerPriv *priv;

	g_return_val_if_fail (TRACKER_IS_POWER (power), TRUE);

	priv = GET_PRIV (power);

	return priv->battery_percentage;
}

#endif /* HAVE_HAL */
