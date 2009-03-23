/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia (urho.konttori@nokia.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#ifdef HAVE_HAL

#include <string.h>

#include <libhal.h>
#include <libhal-storage.h>

#include <dbus/dbus-glib-lowlevel.h>

#include "tracker-log.h"
#include "tracker-hal.h"
#include "tracker-utils.h"
#include "tracker-marshal.h"

#define CAPABILITY_AC_ADAPTER  "ac_adapter"
#define CAPABILITY_BATTERY     "battery"
#define CAPABILITY_VOLUME      "volume"

#define PROP_AC_ADAPTER_ON     "ac_adapter.present"
#define PROP_BATT_PERCENTAGE   "battery.charge_level.percentage"
#define PROP_IS_MOUNTED        "volume.is_mounted"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_HAL, TrackerHalPriv))

typedef struct {
	LibHalContext *context;

	GHashTable    *all_devices;
	GHashTable    *mounted_devices;
	GHashTable    *removable_devices;
	GHashTable    *batteries;

	gchar	      *ac_adapter_udi;
	gboolean       battery_in_use;
	gdouble        battery_percentage;
} TrackerHalPriv;

typedef struct {
	LibHalContext *context;
	GList	      *roots;
} GetRoots;

static void	tracker_hal_finalize		(GObject	 *object);
static void	hal_get_property		(GObject	 *object,
						 guint		  param_id,
						 GValue		 *value,
						 GParamSpec	 *pspec);
static gboolean hal_setup_devices		(TrackerHal	 *hal);
static gboolean hal_setup_ac_adapters		(TrackerHal	 *hal);
static gboolean hal_setup_batteries		(TrackerHal	 *hal);

static void     hal_battery_modify              (TrackerHal      *hal,
						 const gchar     *udi);
static void     hal_battery_remove              (TrackerHal      *hal,
						 const gchar     *udi);

static gboolean hal_device_add			(TrackerHal	 *hal,
						 LibHalVolume	 *volume);
static void	hal_device_added_cb		(LibHalContext	 *context,
						 const gchar	 *udi);
static void	hal_device_removed_cb		(LibHalContext	 *context,
						 const gchar	 *udi);
static void	hal_device_property_modified_cb (LibHalContext	 *context,
						 const char	 *udi,
						 const char	 *key,
						 dbus_bool_t	  is_removed,
						 dbus_bool_t	  is_added);

enum {
	PROP_0,
	PROP_BATTERY_IN_USE,
	PROP_BATTERY_EXISTS,
	PROP_BATTERY_PERCENTAGE
};

enum {
	MOUNT_POINT_ADDED,
	MOUNT_POINT_REMOVED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};

G_DEFINE_TYPE (TrackerHal, tracker_hal, G_TYPE_OBJECT);

static void
tracker_hal_class_init (TrackerHalClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize	   = tracker_hal_finalize;
	object_class->get_property = hal_get_property;

	signals[MOUNT_POINT_ADDED] =
		g_signal_new ("mount-point-added",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      tracker_marshal_VOID__STRING_STRING,
			      G_TYPE_NONE,
			      2, 
			      G_TYPE_STRING,
			      G_TYPE_STRING);

	signals[MOUNT_POINT_REMOVED] =
		g_signal_new ("mount-point-removed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      tracker_marshal_VOID__STRING_STRING,
			      G_TYPE_NONE,
			      2, 
			      G_TYPE_STRING,
			      G_TYPE_STRING);

	g_object_class_install_property (object_class,
					 PROP_BATTERY_IN_USE,
					 g_param_spec_boolean ("battery-in-use",
							       "Battery in use",
							       "Whether the battery is being used",
							       FALSE,
							       G_PARAM_READABLE));

	g_object_class_install_property (object_class,
					 PROP_BATTERY_EXISTS,
					 g_param_spec_boolean ("battery-exists",
							       "Battery exists",
							       "There is a battery on this machine",
							       FALSE,
							       G_PARAM_READABLE));
	g_object_class_install_property (object_class,
					 PROP_BATTERY_PERCENTAGE,
					 g_param_spec_double ("battery-percentage",
							      "Battery percentage",
							      "Battery percentage",
							      0.0, 1.0, 0.0,
							      G_PARAM_READABLE));

	g_type_class_add_private (object_class, sizeof (TrackerHalPriv));
}

static void
tracker_hal_init (TrackerHal *hal)
{
	TrackerHalPriv *priv;
	DBusError	error;
	DBusConnection *connection;

	g_message ("Initializing HAL...");

	priv = GET_PRIV (hal);

	priv->all_devices = g_hash_table_new_full (g_str_hash,
						   g_str_equal,
						   (GDestroyNotify) g_free,
						   (GDestroyNotify) g_free);

	priv->mounted_devices = g_hash_table_new_full (g_str_hash,
						       g_str_equal,
						       (GDestroyNotify) g_free,
						       (GDestroyNotify) g_free);

	priv->removable_devices = g_hash_table_new_full (g_str_hash,
							 g_str_equal,
							 (GDestroyNotify) g_free,
							 (GDestroyNotify) g_free);
	priv->batteries = g_hash_table_new_full (g_str_hash,
						 g_str_equal,
						 (GDestroyNotify) g_free,
						 NULL);

	dbus_error_init (&error);

	connection = dbus_bus_get (DBUS_BUS_SYSTEM, &error);
	if (dbus_error_is_set (&error)) {
		g_critical ("Could not get the system DBus connection, %s",
			    error.message);
		dbus_error_free (&error);
		return;
	}

	dbus_connection_set_exit_on_disconnect (connection, FALSE);
	dbus_connection_setup_with_g_main (connection, NULL);

	priv->context = libhal_ctx_new ();

	if (!priv->context) {
		g_critical ("Could not create HAL context");
		return;
	}

	libhal_ctx_set_user_data (priv->context, hal);
	libhal_ctx_set_dbus_connection (priv->context, connection);

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

	/* Get all devices which are mountable and set them up */
	if (!hal_setup_devices (hal)) {
		return;
	}

	/* Get all AC adapters info and set them up */
	if (!hal_setup_ac_adapters (hal)) {
		return;
	}

	/* Get all battery devices and set them up */
	if (!hal_setup_batteries (hal)) {
		return;
	}
}

static void
tracker_hal_finalize (GObject *object)
{
	TrackerHalPriv *priv;

	priv = GET_PRIV (object);

	if (priv->removable_devices) {
		g_hash_table_unref (priv->removable_devices);
	}

	if (priv->mounted_devices) {
		g_hash_table_unref (priv->mounted_devices);
	}

	if (priv->all_devices) {
		g_hash_table_unref (priv->all_devices);
	}

	if (priv->batteries) {
		g_hash_table_unref (priv->batteries);
	}

	g_free (priv->ac_adapter_udi);

	if (priv->context) {
		libhal_ctx_set_user_data (priv->context, NULL);
		libhal_ctx_free (priv->context);
	}

	(G_OBJECT_CLASS (tracker_hal_parent_class)->finalize) (object);
}

static void
hal_get_property (GObject    *object,
		  guint       param_id,
		  GValue     *value,
		  GParamSpec *pspec)
{
	TrackerHalPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_BATTERY_IN_USE:
		g_value_set_boolean (value, priv->battery_in_use);
		break;
	case PROP_BATTERY_EXISTS:
		g_value_set_boolean (value, priv->ac_adapter_udi != NULL);
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
hal_setup_devices (TrackerHal *hal)
{
	TrackerHalPriv	*priv;
	DBusError	 error;
	gchar	       **devices, **p;
	gint		 num;

	priv = GET_PRIV (hal);

	dbus_error_init (&error);

	devices = libhal_find_device_by_capability (priv->context,
						    CAPABILITY_VOLUME,
						    &num,
						    &error);

	if (dbus_error_is_set (&error)) {
		g_critical ("Could not get devices with 'volume' capability, %s",
			    error.message);
		dbus_error_free (&error);
		return FALSE;
	}

	if (!devices || !devices[0]) {
		g_message ("HAL devices not found with 'volume' capability");
		return TRUE;
	}

	for (p = devices; *p; p++) {
		LibHalVolume *volume;

		volume = libhal_volume_from_udi (priv->context, *p);
		if (!volume) {
			continue;
		}

		g_debug ("HAL device:'%s' found:",
			   libhal_volume_get_device_file (volume));
		g_debug ("  UDI	 : %s",
			   libhal_volume_get_udi (volume));
		g_debug ("  Mount point: %s",
			   libhal_volume_get_mount_point (volume));
 		g_debug ("  UUID	 : %s",
			   libhal_volume_get_uuid (volume));
		g_debug ("  Mounted    : %s",
			   libhal_volume_is_mounted (volume) ? "yes" : "no");
		g_debug ("  File system: %s",
			   libhal_volume_get_fstype (volume));
		g_debug ("  Label	 : %s",
			   libhal_volume_get_label (volume));

		hal_device_add (hal, volume);
		libhal_volume_free (volume);
	}

	libhal_free_string_array (devices);

	return TRUE;
}

static gboolean
hal_setup_ac_adapters (TrackerHal *hal)
{
	TrackerHalPriv	*priv;
	DBusError	 error;
	gchar	       **devices, **p;
	gint		 num;

	priv = GET_PRIV (hal);

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

		priv->battery_in_use = FALSE;
		g_object_notify (G_OBJECT (hal), "battery-in-use");

		priv->ac_adapter_udi = NULL;
		g_object_notify (G_OBJECT (hal), "battery-exists");

		return TRUE;
	}

	for (p = devices; *p; p++) {
		if (!priv->ac_adapter_udi) {
			/* For now just use the first one we find */
			priv->ac_adapter_udi = g_strdup (*p);
			g_object_notify (G_OBJECT (hal), "battery-exists");

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
	priv->battery_in_use = !libhal_device_get_property_bool (priv->context,
								 priv->ac_adapter_udi,
								 PROP_AC_ADAPTER_ON,
								 NULL);

	g_message ("HAL reports system is currently powered by %s",
		   priv->battery_in_use ? "battery" : "AC adapter");

	g_object_notify (G_OBJECT (hal), "battery-in-use");

	return TRUE;
}

static gboolean
hal_setup_batteries (TrackerHal *hal)
{
	TrackerHalPriv	*priv;
	DBusError	 error;
	gchar	       **devices, **p;
	gint		 num;

	priv = GET_PRIV (hal);

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

		hal_battery_modify (hal, *p);
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
hal_mount_point_add (TrackerHal  *hal,
		     const gchar *udi,
		     const gchar *mount_point,
		     gboolean	  removable_device)
{
	TrackerHalPriv *priv;

	priv = GET_PRIV (hal);

	g_message ("HAL device:'%s' with mount point:'%s', removable:%s now being tracked",
		   (const gchar*) g_hash_table_lookup (priv->all_devices, udi),
		   mount_point,
		   removable_device ? "yes" : "no");
	
	g_hash_table_insert (priv->mounted_devices,
			     g_strdup (udi),
			     g_strdup (mount_point));

	if (removable_device) {
		g_hash_table_insert (priv->removable_devices,
				     g_strdup (udi),
				     g_strdup (mount_point));
	}

	g_signal_emit (hal, signals[MOUNT_POINT_ADDED], 0, udi, mount_point, NULL);
}

static void
hal_mount_point_remove (TrackerHal  *hal,
			const gchar *udi)
{
	TrackerHalPriv *priv;
	const gchar    *mount_point;

	priv = GET_PRIV (hal);

	mount_point = g_hash_table_lookup (priv->mounted_devices, udi);

	if (!mount_point) {
		return;
	}

	g_message ("HAL device:'%s' with mount point:'%s' (uuid:'%s'), removable:%s NO LONGER being tracked",
		   (const gchar*) g_hash_table_lookup (priv->all_devices, udi),
		   mount_point,
		   udi,
		   g_hash_table_remove (priv->removable_devices, udi) ? "yes" : "no");
	
	g_signal_emit (hal, signals[MOUNT_POINT_REMOVED], 0, udi, mount_point, NULL);

	g_hash_table_remove (priv->mounted_devices, udi);
	g_hash_table_remove (priv->removable_devices, udi);
}

static const gchar *
hal_drive_type_to_string (LibHalDriveType type)
{
	switch (type) {
	case LIBHAL_DRIVE_TYPE_REMOVABLE_DISK:
		return "LIBHAL_DRIVE_TYPE_REMOVABLE_DISK";
	case LIBHAL_DRIVE_TYPE_DISK:
		return "LIBHAL_DRIVE_TYPE_DISK";
	case LIBHAL_DRIVE_TYPE_CDROM:
		return "LIBHAL_DRIVE_TYPE_CDROM";
	case LIBHAL_DRIVE_TYPE_FLOPPY:
		return "LIBHAL_DRIVE_TYPE_FLOPPY";
	case LIBHAL_DRIVE_TYPE_TAPE:
		return "LIBHAL_DRIVE_TYPE_TAPE";
	case LIBHAL_DRIVE_TYPE_COMPACT_FLASH:
		return "LIBHAL_DRIVE_TYPE_COMPACT_FLASH";
	case LIBHAL_DRIVE_TYPE_MEMORY_STICK:
		return "LIBHAL_DRIVE_TYPE_MEMORY_STICK";
	case LIBHAL_DRIVE_TYPE_SMART_MEDIA:
		return "LIBHAL_DRIVE_TYPE_SMART_MEDIA";
	case LIBHAL_DRIVE_TYPE_SD_MMC:
		return "LIBHAL_DRIVE_TYPE_SD_MMC";
	case LIBHAL_DRIVE_TYPE_CAMERA:
		return "LIBHAL_DRIVE_TYPE_CAMERA";
	case LIBHAL_DRIVE_TYPE_PORTABLE_AUDIO_PLAYER:
		return "LIBHAL_DRIVE_TYPE_PORTABLE_AUDIO_PLAYER";
	case LIBHAL_DRIVE_TYPE_ZIP:
		return "LIBHAL_DRIVE_TYPE_ZIP";
	case LIBHAL_DRIVE_TYPE_JAZ:
		return "LIBHAL_DRIVE_TYPE_JAZ";
	case LIBHAL_DRIVE_TYPE_FLASHKEY:
		return "LIBHAL_DRIVE_TYPE_FLASHKEY";
	case LIBHAL_DRIVE_TYPE_MO:
		return "LIBHAL_DRIVE_TYPE_MO";
	default:
		return "";
	}
}

static gboolean
hal_device_is_removable (TrackerHal  *hal,
			 const gchar *device_file)
{
	TrackerHalPriv	*priv;
	LibHalDrive	*drive;
	gboolean	 removable;

	if (!device_file) {
		return FALSE;
	}

	priv = GET_PRIV (hal);

	drive = libhal_drive_from_device_file (priv->context, device_file);
	if (!drive) {
		return FALSE;
	}

	removable = libhal_drive_uses_removable_media (drive);
	libhal_drive_free (drive);

	return removable;
}

static gboolean
hal_device_should_be_tracked (TrackerHal  *hal,
			      const gchar *device_file)
{
	TrackerHalPriv	*priv;
	LibHalDrive	*drive;
	LibHalDriveType  drive_type;
	gboolean	 eligible;

	if (!device_file) {
		return FALSE;
	}

	priv = GET_PRIV (hal);

	drive = libhal_drive_from_device_file (priv->context, device_file);
	if (!drive) {
		return FALSE;
	}

	/* From the list, the first one below seems to be the ONLY one
	 * to ignore:
	 *
	 * LIBHAL_DRIVE_TYPE_REMOVABLE_DISK	   = 0x00,
	 * LIBHAL_DRIVE_TYPE_DISK		   = 0x01,
	 * LIBHAL_DRIVE_TYPE_CDROM		   = 0x02,
	 * LIBHAL_DRIVE_TYPE_FLOPPY		   = 0x03,
	 * LIBHAL_DRIVE_TYPE_TAPE		   = 0x04,
	 * LIBHAL_DRIVE_TYPE_COMPACT_FLASH	   = 0x05,
	 * LIBHAL_DRIVE_TYPE_MEMORY_STICK	   = 0x06,
	 * LIBHAL_DRIVE_TYPE_SMART_MEDIA	   = 0x07,
	 * LIBHAL_DRIVE_TYPE_SD_MMC		   = 0x08,
	 * LIBHAL_DRIVE_TYPE_CAMERA		   = 0x09,
	 * LIBHAL_DRIVE_TYPE_PORTABLE_AUDIO_PLAYER = 0x0a,
	 * LIBHAL_DRIVE_TYPE_ZIP		   = 0x0b,
	 * LIBHAL_DRIVE_TYPE_JAZ		   = 0x0c,
	 * LIBHAL_DRIVE_TYPE_FLASHKEY		   = 0x0d,
	 * LIBHAL_DRIVE_TYPE_MO			   = 0x0e
	 *
	 */

	drive_type = libhal_drive_get_type (drive);

	/* So here we don't track CDROM devices or the hard disks in
	 * the machine, we simply track devices which are added or
	 * removed in real time which we are interested in and which
	 * are viable for tracking. CDROMs are too slow.
	 */
	eligible = TRUE;
	eligible &= drive_type != LIBHAL_DRIVE_TYPE_DISK;
	eligible &= drive_type != LIBHAL_DRIVE_TYPE_CDROM;

	libhal_drive_free (drive);

	if (!eligible) {
		g_message ("HAL device:'%s' is not eligible for tracking, type is '%s'",
			   device_file,
			   hal_drive_type_to_string (drive_type));
	} else {
		g_message ("HAL device:'%s' is eligible for tracking, type is '%s'",
			   device_file,
			   hal_drive_type_to_string (drive_type));
	}

	return eligible;
}

static void
hal_battery_notify (TrackerHal *hal)
{
	TrackerHalPriv *priv;
	GList *values, *v;
	gint percentage, n_values;

	priv = GET_PRIV (hal);
	percentage = n_values = 0;

	values = g_hash_table_get_values (priv->batteries);

	for (v = values; v; v = v->next) {
		percentage += GPOINTER_TO_INT (v->data);
		n_values++;
	}

	if (n_values > 0) {
		priv->battery_percentage = (gdouble) percentage / n_values;
		priv->battery_percentage /= 100;
	} else {
		priv->battery_percentage = 0;
	}

	g_list_free (values);

	g_object_notify (G_OBJECT (hal), "battery-percentage");
}

static void
hal_battery_modify (TrackerHal  *hal,
		    const gchar *udi)
{
	TrackerHalPriv *priv;
	gint percentage;

	priv = GET_PRIV (hal);
	percentage = libhal_device_get_property_int (priv->context, udi,
						     PROP_BATT_PERCENTAGE,
						     NULL);

	g_hash_table_insert (priv->batteries,
			     g_strdup (udi),
			     GINT_TO_POINTER (percentage));

	hal_battery_notify (hal);
}

static void
hal_battery_remove (TrackerHal  *hal,
		    const gchar *udi)
{
	TrackerHalPriv *priv;

	priv = GET_PRIV (hal);

	g_hash_table_remove (priv->batteries, udi);
	hal_battery_notify (hal);
}

static gboolean
hal_device_add (TrackerHal   *hal,
		LibHalVolume *volume)
{
	TrackerHalPriv *priv;
	DBusError	error;
	const gchar    *udi;
	const gchar    *mount_point;
	const gchar    *device_file;

	priv = GET_PRIV (hal);

	dbus_error_init (&error);

	udi = libhal_volume_get_udi (volume);
	mount_point = libhal_volume_get_mount_point (volume);
	device_file = libhal_volume_get_device_file (volume);

	if (g_hash_table_lookup (priv->all_devices, udi)) {
		return TRUE;
	}

	/* If there is no mount point, then there is nothing to track */
	if (!hal_device_should_be_tracked (hal, device_file)) {
		return TRUE;
	}

	/* Make sure we watch changes to the mount/umount state */
	libhal_device_add_property_watch (priv->context, udi, &error);

	if (dbus_error_is_set (&error)) {
		g_critical ("Could not add device:'%s' property watch for udi:'%s', %s",
			    device_file, 
			    udi, 
			    error.message);
		dbus_error_free (&error);
		return FALSE;
	}

	g_hash_table_insert (priv->all_devices,
			     g_strdup (udi),
			     g_strdup (device_file));

	if (mount_point) {
		hal_mount_point_add (hal,
				     udi,
				     mount_point,
				     hal_device_is_removable (hal, device_file));
	}

	return TRUE;
}

static void
hal_device_added_cb (LibHalContext *context,
		     const gchar   *udi)
{
	TrackerHal   *hal;
	LibHalVolume *volume;

	hal = libhal_ctx_get_user_data (context);

	if (libhal_device_query_capability (context, udi, CAPABILITY_BATTERY, NULL)) {
		hal_battery_modify (hal, udi);
	} else if (libhal_device_query_capability (context, udi, CAPABILITY_VOLUME, NULL)) {
		volume = libhal_volume_from_udi (context, udi);

		if (!volume) {
			/* Not a device with a volume */
			return;
		}

		g_message ("HAL device:'%s' added:",
			   libhal_volume_get_device_file (volume));
		g_message ("  UDI	 : %s",
			   udi);
		g_message ("  Mount point: %s",
			   libhal_volume_get_mount_point (volume));
 		g_message ("  UUID	 : %s",
			   libhal_volume_get_uuid (volume));
		g_message ("  Mounted    : %s",
			   libhal_volume_is_mounted (volume) ? "yes" : "no");
		g_message ("  File system: %s",
			   libhal_volume_get_fstype (volume));
		g_message ("  Label	 : %s",
			   libhal_volume_get_label (volume));

		hal_device_add (hal, volume);
		libhal_volume_free (volume);
	}
}

static void
hal_device_removed_cb (LibHalContext *context,
		       const gchar   *udi)
{
	TrackerHal     *hal;
	TrackerHalPriv *priv;
	const gchar    *device_file;
	const gchar    *mount_point;

	hal = (TrackerHal*) libhal_ctx_get_user_data (context);
	priv = GET_PRIV (hal);

	if (g_hash_table_lookup (priv->batteries, udi)) {
		hal_battery_remove (hal, udi);
	} else if (g_hash_table_lookup (priv->all_devices, udi)) {
		device_file = g_hash_table_lookup (priv->all_devices, udi);

		if (!device_file) {
			/* Don't report about unknown devices */
			return;
		}

		mount_point = g_hash_table_lookup (priv->mounted_devices, udi);

		g_message ("HAL device:'%s' removed:",
			   device_file);
		g_message ("  UDI	 : %s",
			   udi);
		g_message ("  Mount point: %s",
			   mount_point);

		g_hash_table_remove (priv->all_devices, udi);

		hal_mount_point_remove (hal, udi);
	}
}

static void
hal_device_property_modified_cb (LibHalContext *context,
				 const char    *udi,
				 const char    *key,
				 dbus_bool_t	is_removed,
				 dbus_bool_t	is_added)
{
	TrackerHal     *hal;
	TrackerHalPriv *priv;
	DBusError	error;

	hal = (TrackerHal*) libhal_ctx_get_user_data (context);
	priv = GET_PRIV (hal);

	dbus_error_init (&error);

	if (priv->ac_adapter_udi && strcmp (priv->ac_adapter_udi, udi) == 0) {
		/* Property change is on the AC adapter */
		priv->battery_in_use = !libhal_device_get_property_bool (priv->context,
									 priv->ac_adapter_udi,
									 PROP_AC_ADAPTER_ON,
									 &error);
		g_message ("HAL reports system is now powered by %s",
			   priv->battery_in_use ? "battery" : "AC adapter");

		g_object_notify (G_OBJECT (hal), "battery-in-use");

		if (dbus_error_is_set (&error)) {
			g_critical ("Could not get device property:'%s' for udi:'%s', %s",
				    udi, PROP_AC_ADAPTER_ON, error.message);
			dbus_error_free (&error);
			return;
		}
	} else if (g_hash_table_lookup (priv->batteries, udi)) {
		/* Property change is on any battery */
		if (strcmp (key, PROP_BATT_PERCENTAGE) == 0) {
			hal_battery_modify (hal, udi);
		}
	} else if (g_hash_table_lookup (priv->all_devices, udi)) {
		const gchar *device_file;
		gboolean is_mounted;

		device_file = g_hash_table_lookup (priv->all_devices, udi);

		g_message ("HAL device:'%s' property change for udi:'%s' and key:'%s'",
			   device_file,
			   udi, 
			   key);

		if (strcmp (key, PROP_IS_MOUNTED) != 0) {
			return;
		}

		is_mounted = libhal_device_get_property_bool (context,
							      udi,
							      key,
							      &error);

		if (dbus_error_is_set (&error)) {
			g_message ("Could not get device property:'%s' for udi:'%s', %s",
				   udi, key, error.message);
			dbus_error_free (&error);

			g_message ("HAL device:'%s' with udi:'%s' is now unmounted (due to error)",
				   device_file, 
				   udi);
			hal_mount_point_remove (hal, udi);
			return;
		}

		if (is_mounted) {
			LibHalVolume *volume;
			const gchar  *mount_point;
			const gchar  *device_file;

			volume = libhal_volume_from_udi (context, udi);
			mount_point = libhal_volume_get_mount_point (volume);
			device_file = libhal_volume_get_device_file (volume);

			g_message ("HAL device:'%s' with udi:'%s' is now mounted",
				   device_file,
				   udi);

			hal_mount_point_add (hal,
					     udi,
					     mount_point,
					     hal_device_is_removable (hal, device_file));

			libhal_volume_free (volume);
		} else {
			g_message ("HAL device:'%s' with udi:'%s' is now unmounted",
				   device_file,
				   udi);

			hal_mount_point_remove (hal, udi);
		}
	}
}

/**
 * tracker_hal_new:
 *
 * Creates a new instance of #TrackerHal.
 *
 * Returns: The newly created #TrackerHal.
 **/
TrackerHal *
tracker_hal_new ()
{
	return g_object_new (TRACKER_TYPE_HAL, NULL);
}

/**
 * tracker_hal_get_battery_in_use:
 * @hal: A #TrackerHal.
 *
 * Returns whether the computer battery (if any) is currently in use.
 *
 * Returns: #TRUE if the computer is running on battery power.
 **/
gboolean
tracker_hal_get_battery_in_use (TrackerHal *hal)
{
	TrackerHalPriv *priv;

	g_return_val_if_fail (TRACKER_IS_HAL (hal), TRUE);

	priv = GET_PRIV (hal);

	return priv->battery_in_use;
}

/**
 * tracker_hal_get_battery_exists:
 * @hal: A #TrackerHal
 *
 * Returns whether the computer has batteries.
 *
 * Returns: #TRUE if the computer has batteries available.
 **/
gboolean
tracker_hal_get_battery_exists (TrackerHal *hal)
{
	TrackerHalPriv *priv;

	g_return_val_if_fail (TRACKER_IS_HAL (hal), TRUE);

	priv = GET_PRIV (hal);

	return priv->ac_adapter_udi != NULL;
}

/**
 * tracker_hal_get_battery_percentage:
 * @hal: A #TrackerHal
 *
 * Returns the battery percentage left on the
 * computer, or #0.0 if no batteries are present.
 *
 * Returns: The battery percentage left.
 **/
gdouble
tracker_hal_get_battery_percentage (TrackerHal *hal)
{
	TrackerHalPriv *priv;

	g_return_val_if_fail (TRACKER_IS_HAL (hal), 0.0);

	priv = GET_PRIV (hal);

	return priv->battery_percentage;
}

static void
hal_get_mount_point_by_udi_foreach (gpointer key,
				    gpointer value,
				    gpointer user_data)
{
	LibHalVolume  *volume;
	GetRoots      *gr;
	const gchar   *udi;
	const gchar   *mount_point;
	gboolean       is_mounted;

	gr = (GetRoots*) user_data;
	udi = key;

	volume = libhal_volume_from_udi (gr->context, udi);
	if (!volume) {
		g_message ("HAL device with udi:'%s' has no volume, "
			   "should we delete?",
			   udi);
		return;
	}

	mount_point = libhal_volume_get_mount_point (volume);
	is_mounted = libhal_volume_is_mounted (volume);

	if (is_mounted && mount_point) {
		gr->roots = g_list_prepend (gr->roots, g_strdup (mount_point));
	}

	libhal_volume_free (volume);
}

/**
 * tracker_hal_get_mounted_directory_roots:
 * @hal: A #TrackerHal
 *
 * Returns a #Glist of strings containing the root directories for mounted devices.
 * Each element must be freed using g_free() and the list itself using g_list_free().
 *
 * Returns: The list of root directories.
 **/
GList *
tracker_hal_get_mounted_directory_roots (TrackerHal *hal)
{
	TrackerHalPriv *priv;
	GetRoots	gr;

	g_return_val_if_fail (TRACKER_IS_HAL (hal), NULL);

	priv = GET_PRIV (hal);

	gr.context = priv->context;
	gr.roots = NULL;

	g_hash_table_foreach (priv->mounted_devices,
			      hal_get_mount_point_by_udi_foreach,
			      &gr);

	return gr.roots;
}

/**
 * tracker_hal_get_removable_device_roots:
 * @hal: A #TrackerHal
 *
 * Returns a #GList of strings containing the root directories for removable devices.
 * Each element must be freed using g_free() and the list itself through g_list_free().
 *
 * Returns: The list of root directories.
 **/
GList *
tracker_hal_get_removable_device_roots (TrackerHal *hal)
{
	TrackerHalPriv *priv;
	GetRoots	gr;

	g_return_val_if_fail (TRACKER_IS_HAL (hal), NULL);

	priv = GET_PRIV (hal);

	gr.context = priv->context;
	gr.roots = NULL;

	g_hash_table_foreach (priv->removable_devices,
			      hal_get_mount_point_by_udi_foreach,
			      &gr);

	return gr.roots;
}

/**
 * tracker_hal_path_is_on_removable_device:
 * @hal: A #TrackerHal
 * @path: a path
 * @mount_mount: if @path is on a removable device, the mount point will
 * be filled in here. You must free the returned result
 * @available: if @path is on a removable device, this will be set to 
 * TRUE in case the file is available right now
 *
 * Returns Whether or not @path is on a known removable device
 *
 * Returns: TRUE if @path on a known removable device, FALSE otherwise
 **/
gboolean
tracker_hal_path_is_on_removable_device (TrackerHal  *hal,
					 const gchar *path,
					 gchar      **mount_point,
					 gboolean    *available)
{
	TrackerHalPriv *priv;
	GHashTableIter  iter;
	gboolean        found = FALSE;
	gpointer        key, value;

	g_return_val_if_fail (TRACKER_IS_HAL (hal), FALSE);

	if (!path)
		return FALSE;

	priv = GET_PRIV (hal);

	g_hash_table_iter_init (&iter, priv->removable_devices);

	while (g_hash_table_iter_next (&iter, &key, &value)) {
		LibHalVolume  *volume;
		const gchar   *udi;
		const gchar   *mp;

		udi = key;

		volume = libhal_volume_from_udi (priv->context, udi);

		if (!volume) {
			g_message ("HAL device with udi:'%s' has no volume, "
				   "should we delete?",
				   udi);
			continue;
		}

		mp = libhal_volume_get_mount_point (volume);

		if (g_strcmp0 (mp, path) != 0) {
			if (g_strrstr (path, mp)) {
				found = TRUE;

				if (mount_point)
					*mount_point = g_strdup (mp);

				if (available)
					*available = libhal_volume_is_mounted (volume);

				libhal_volume_free (volume);
				break;
			}
		}

		libhal_volume_free (volume);
	}

	return found;
}


/**
 * tracker_hal_get_removable_device_udis:
 * @hal: A #TrackerHal
 *
 * Returns a #GList of strings containing the UDI for removable devices.
 * Each element is owned by the #GHashTable internally, the list
 * itself through should be freed using g_list_free().
 *
 * Returns: The list of UDIs.
 **/
GList *
tracker_hal_get_removable_device_udis (TrackerHal *hal)
{
	TrackerHalPriv *priv;

	g_return_val_if_fail (TRACKER_IS_HAL (hal), NULL);

	priv = GET_PRIV (hal);
	
	return g_hash_table_get_keys (priv->removable_devices);
}

/**
 * tracker_hal_udi_get_mount_point:
 * @hal: A #TrackerHal
 * @udi: A string pointer to the UDI for the device.
 *
 * Returns: The mount point for @udi, this should not be freed.
 **/
const gchar *
tracker_hal_udi_get_mount_point (TrackerHal  *hal,
				 const gchar *udi)
{
	TrackerHalPriv *priv;

	g_return_val_if_fail (TRACKER_IS_HAL (hal), NULL);
	g_return_val_if_fail (udi != NULL, NULL);

	priv = GET_PRIV (hal);
	
	return g_hash_table_lookup (priv->removable_devices, udi);
}

/**
 * tracker_hal_udi_get_mount_point:
 * @hal: A #TrackerHal
 * @udi: A #gboolean
 *
 * Returns: The %TRUE if @udi is mounted or %FALSE if it isn't.
 **/
gboolean    
tracker_hal_udi_get_is_mounted (TrackerHal  *hal,
				const gchar *udi)
{
	TrackerHalPriv *priv;
	LibHalVolume   *volume;
	const gchar    *mount_point;
	gboolean        is_mounted;

	g_return_val_if_fail (TRACKER_IS_HAL (hal), FALSE);
	g_return_val_if_fail (udi != NULL, FALSE);

	priv = GET_PRIV (hal);

	volume = libhal_volume_from_udi (priv->context, udi);
	if (!volume) {
		g_message ("HAL device with udi:'%s' has no volume, "
			   "should we delete?",
			   udi);
		return FALSE;
	}

	mount_point = libhal_volume_get_mount_point (volume);
	is_mounted = libhal_volume_is_mounted (volume);

	libhal_volume_free (volume);

	return is_mounted && mount_point;

}

#endif /* HAVE_HAL */
