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

#define DEVICE_AC_ADAPTER  "ac_adapter"
#define DEVICE_VOLUME	   "volume"

#define PROP_AC_ADAPTER_ON "ac_adapter.present"
#define PROP_IS_MOUNTED    "volume.is_mounted"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_HAL, TrackerHalPriv))

typedef struct {
	LibHalContext *context;

	GHashTable    *all_devices;
	GHashTable    *mounted_devices;
	GHashTable    *removable_devices;

	gchar	      *battery_udi;
	gboolean       battery_in_use;
} TrackerHalPriv;

typedef struct {
	LibHalContext *context;
	GSList	      *roots;
} GetRoots;

static void	tracker_hal_class_init		(TrackerHalClass *klass);
static void	tracker_hal_init		(TrackerHal	 *hal);
static void	tracker_hal_finalize		(GObject	 *object);
static void	hal_get_property		(GObject	 *object,
						 guint		  param_id,
						 GValue		 *value,
						 GParamSpec	 *pspec);
static gboolean hal_setup_devices		(TrackerHal	 *hal);
static gboolean hal_setup_batteries		(TrackerHal	 *hal);

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
};

enum {
	SIG_MOUNT_POINT_ADDED,
	SIG_MOUNT_POINT_REMOVED,
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

	signals[SIG_MOUNT_POINT_ADDED] =
		g_signal_new ("mount-point-added",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE,
			      1, G_TYPE_STRING);

	signals[SIG_MOUNT_POINT_REMOVED] =
		g_signal_new ("mount-point-removed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE,
			      1, G_TYPE_STRING);

	g_object_class_install_property (object_class,
					 PROP_BATTERY_IN_USE,
					 g_param_spec_boolean ("battery-in-use",
							       "Battery exists",
							       "There is a battery on this machine",
							       FALSE,
							       G_PARAM_READABLE));

	g_object_class_install_property (object_class,
					 PROP_BATTERY_EXISTS,
					 g_param_spec_boolean ("battery-exists",
							       "Battery exists",
							       "There is a battery on this machine",
							       FALSE,
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

	dbus_error_init (&error);

	connection = dbus_bus_get (DBUS_BUS_SYSTEM, &error);
	if (dbus_error_is_set (&error)) {
		g_critical ("Could not get the system DBus connection, %s",
			    error.message);
		dbus_error_free (&error);
		return;
	}

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

	g_free (priv->battery_udi);

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
		g_value_set_boolean (value, priv->battery_udi != NULL);
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
						    DEVICE_VOLUME,
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

		g_message ("HAL device found:\n"
			   " - udi	  : %s\n"
			   " - mount point: %s\n"
			   " - device file: %s\n"
			   " - uuid	  : %s\n"
			   " - mounted	  : %s\n"
			   " - file system: %s\n"
			   " - label	  : %s",
			   libhal_volume_get_udi (volume),
			   libhal_volume_get_mount_point (volume),
			   libhal_volume_get_device_file (volume),
			   libhal_volume_get_uuid (volume),
			   libhal_volume_is_mounted (volume) ? "yes" : "no",
			   libhal_volume_get_fstype (volume),
			   libhal_volume_get_label (volume));

		hal_device_add (hal, volume);
		libhal_volume_free (volume);
	}

	libhal_free_string_array (devices);

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
						    DEVICE_AC_ADAPTER,
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

		priv->battery_udi = NULL;
		g_object_notify (G_OBJECT (hal), "battery-exists");

		return TRUE;
	}

	for (p = devices; *p; p++) {
		if (!priv->battery_udi) {
			/* For now just use the first one we find */
			priv->battery_udi = g_strdup (*p);
			g_object_notify (G_OBJECT (hal), "battery-exists");

			g_message (" - Device '%s' (default)", *p);
		} else {
			g_message (" - Device '%s'", *p);
		}
	}

	libhal_free_string_array (devices);

	/* Make sure we watch changes to the battery use */
	libhal_device_add_property_watch (priv->context,
					  priv->battery_udi,
					  &error);

	if (dbus_error_is_set (&error)) {
		g_critical ("Could not add device:'%s' to property watch, %s",
			       priv->battery_udi, error.message);
		dbus_error_free (&error);
		return FALSE;
	}

	/* Get current state, are we using the battery now? */
	priv->battery_in_use = !libhal_device_get_property_bool (priv->context,
								 priv->battery_udi,
								 PROP_AC_ADAPTER_ON,
								 NULL);

	g_message ("HAL reports system is currently powered by %s",
		   priv->battery_in_use ? "battery" : "AC adapter");

	g_object_notify (G_OBJECT (hal), "battery-in-use");

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

	g_message ("HAL device with mount point:'%s', removable:%s now being tracked",
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

	g_signal_emit (hal, signals[SIG_MOUNT_POINT_ADDED], 0, mount_point, NULL);
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

	g_message ("HAL device with mount point:'%s', removable:%s NO LONGER being tracked",
		     mount_point,
		     g_hash_table_remove (priv->removable_devices, udi) ? "yes" : "no");

	g_signal_emit (hal, signals[SIG_MOUNT_POINT_REMOVED], 0, mount_point, NULL);

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
	}

	return "";
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
		g_message ("HAL device is not eligible, type is '%s'",
			   hal_drive_type_to_string (drive_type));
	} else {
		g_message ("HAL device is eligible, type is '%s'",
			   hal_drive_type_to_string (drive_type));
	}

	return eligible;
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
		g_message ("HAL device should not be tracked (not eligible)");
		return TRUE;
	}

	/* Make sure we watch changes to the mount/umount state */
	libhal_device_add_property_watch (priv->context, udi, &error);

	if (dbus_error_is_set (&error)) {
		g_critical ("Could not add device property watch for udi:'%s', %s",
			       udi, error.message);
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
	DBusError     error;
	LibHalVolume *volume;

	dbus_error_init (&error);

	volume = libhal_volume_from_udi (context, udi);
	if (!volume) {
		/* Not a device with a volume */
		return;
	}

	g_message ("HAL device added:\n"
		     " - udi	    : %s\n"
		     " - mount point: %s\n"
		     " - device file: %s\n"
		     " - uuid	    : %s\n"
		     " - mounted    : %s\n"
		     " - file system: %s\n"
		     " - label	    : %s",
		     udi,
		     libhal_volume_get_mount_point (volume),
		     libhal_volume_get_device_file (volume),
		     libhal_volume_get_uuid (volume),
		     libhal_volume_is_mounted (volume) ? "yes" : "no",
		     libhal_volume_get_fstype (volume),
		     libhal_volume_get_label (volume));

	hal = (TrackerHal*) libhal_ctx_get_user_data (context);
	hal_device_add (hal, volume);
	libhal_volume_free (volume);
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

	device_file = g_hash_table_lookup (priv->all_devices, udi);

	if (!device_file) {
		/* Don't report about unknown devices */
		return;
	}

	mount_point = g_hash_table_lookup (priv->mounted_devices, udi);

	g_message ("HAL device removed:\n"
		     " - udi	    : %s\n"
		     " - mount point: %s\n"
		     " - device_file: %s",
		     udi,
		     mount_point,
		     device_file);

	g_hash_table_remove (priv->all_devices, udi);

	hal_mount_point_remove (hal, udi);
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
	gboolean	device_is_battery;
	gboolean	current_state;

	hal = (TrackerHal*) libhal_ctx_get_user_data (context);
	priv = GET_PRIV (hal);

	current_state = priv->battery_in_use;
	device_is_battery = priv->battery_udi && strcmp (priv->battery_udi, udi) == 0;

	if (!device_is_battery &&
	    !g_hash_table_lookup (priv->all_devices, udi)) {
		g_message ("HAL device property change for another device, ignoring");
		return;
	}

	dbus_error_init (&error);

	/* We either get notifications about the battery state OR a
	 * device being mounted/umounted.
	 */
	if (device_is_battery) {
		priv->battery_in_use = !libhal_device_get_property_bool (priv->context,
									 priv->battery_udi,
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
	} else {
		gboolean is_mounted;

		g_message ("HAL device property change for udi:'%s' and key:'%s'",
			   udi, key);

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

			g_message ("HAL device with udi:'%s' is now unmounted (due to error)",
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

			g_message ("HAL device with udi:'%s' is now mounted",
				   udi);

			hal_mount_point_add (hal,
					     udi,
					     mount_point,
					     hal_device_is_removable (hal, device_file));

			libhal_volume_free (volume);
		} else {
			g_message ("HAL device with udi:'%s' is now unmounted",
				   udi);

			hal_mount_point_remove (hal, udi);
		}
	}
}

TrackerHal *
tracker_hal_new (void)
{
	return g_object_new (TRACKER_TYPE_HAL, NULL);
}

gboolean
tracker_hal_get_battery_in_use (TrackerHal *hal)
{
	TrackerHalPriv *priv;

	g_return_val_if_fail (TRACKER_IS_HAL (hal), TRUE);

	priv = GET_PRIV (hal);

	return priv->battery_in_use;
}

gboolean
tracker_hal_get_battery_exists (TrackerHal *hal)
{
	TrackerHalPriv *priv;

	g_return_val_if_fail (TRACKER_IS_HAL (hal), TRUE);

	priv = GET_PRIV (hal);

	return priv->battery_udi != NULL;
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
	udi = (const gchar*) key;

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
		gr->roots = g_slist_prepend (gr->roots, g_strdup (mount_point));
	}

	libhal_volume_free (volume);
}

GSList *
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

GSList *
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

#endif /* HAVE_HAL */
