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

#include <gio/gio.h>
#include <libhal.h>
#include <libhal-storage.h>

#include <dbus/dbus-glib-lowlevel.h>

#include "tracker-log.h"
#include "tracker-storage.h"
#include "tracker-utils.h"
#include "tracker-marshal.h"

#define CAPABILITY_VOLUME      "volume"

#define PROP_IS_MOUNTED        "volume.is_mounted"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_STORAGE, TrackerStoragePriv))

typedef struct {
	LibHalContext *context;
	DBusConnection *connection;

	GHashTable    *all_devices;
	GHashTable    *mounted_devices;
	GHashTable    *removable_devices;
} TrackerStoragePriv;

typedef struct {
	LibHalContext *context;
	GList	      *roots;
} GetRoots;

static void	tracker_storage_finalize	(GObject	 *object);
static void	hal_get_property		(GObject	 *object,
						 guint		  param_id,
						 GValue		 *value,
						 GParamSpec	 *pspec);
static gboolean hal_setup_devices		(TrackerStorage	 *hal);

static gboolean hal_device_add			(TrackerStorage	 *hal,
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
};

enum {
	MOUNT_POINT_ADDED,
	MOUNT_POINT_REMOVED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};

G_DEFINE_TYPE (TrackerStorage, tracker_storage, G_TYPE_OBJECT);

static void
tracker_storage_class_init (TrackerStorageClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize	   = tracker_storage_finalize;
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

	g_type_class_add_private (object_class, sizeof (TrackerStoragePriv));
}

static void
tracker_storage_init (TrackerStorage *storage)
{
	TrackerStoragePriv *priv;
	DBusError	error;

	g_message ("Initializing HAL Storage...");

	priv = GET_PRIV (storage);

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

	priv->connection = dbus_bus_get (DBUS_BUS_SYSTEM, &error);
	if (dbus_error_is_set (&error)) {
		g_critical ("Could not get the system DBus connection, %s",
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

	libhal_ctx_set_user_data (priv->context, storage);
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

	/* Get all devices which are mountable and set them up */
	if (!hal_setup_devices (storage)) {
		return;
	}
}

static void
tracker_storage_finalize (GObject *object)
{
	TrackerStoragePriv *priv;

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

	if (priv->context) {
		libhal_ctx_shutdown (priv->context, NULL);
		libhal_ctx_set_user_data (priv->context, NULL);
		libhal_ctx_free (priv->context);
	}

	if (priv->connection) {
		dbus_connection_unref (priv->connection);
	}

	(G_OBJECT_CLASS (tracker_storage_parent_class)->finalize) (object);
}

static void
hal_get_property (GObject    *object,
		  guint       param_id,
		  GValue     *value,
		  GParamSpec *pspec)
{
	TrackerStoragePriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static gboolean
hal_setup_devices (TrackerStorage *storage)
{
	TrackerStoragePriv	*priv;
	DBusError	 error;
	gchar	       **devices, **p;
	gint		 num;

	priv = GET_PRIV (storage);

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

		hal_device_add (storage, volume);
		libhal_volume_free (volume);
	}

	libhal_free_string_array (devices);

	return TRUE;
}

static void
hal_mount_point_add (TrackerStorage *storage,
		     const gchar    *udi,
		     const gchar    *mount_point,
		     gboolean	     removable_device)
{
	TrackerStoragePriv *priv;

	priv = GET_PRIV (storage);

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

	g_signal_emit (storage, signals[MOUNT_POINT_ADDED], 0, udi, mount_point, NULL);
}

static void
hal_mount_point_remove (TrackerStorage *storage,
			const gchar    *udi)
{
	TrackerStoragePriv *priv;
	const gchar    *mount_point;

	priv = GET_PRIV (storage);

	mount_point = g_hash_table_lookup (priv->mounted_devices, udi);

	if (!mount_point) {
		return;
	}

	g_message ("HAL device:'%s' with mount point:'%s' (uuid:'%s'), removable:%s NO LONGER being tracked",
		   (const gchar*) g_hash_table_lookup (priv->all_devices, udi),
		   mount_point,
		   udi,
		   g_hash_table_remove (priv->removable_devices, udi) ? "yes" : "no");
	
	g_signal_emit (storage, signals[MOUNT_POINT_REMOVED], 0, udi, mount_point, NULL);

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
hal_device_is_removable (TrackerStorage *storage,
			 const gchar    *device_file)
{
	TrackerStoragePriv	*priv;
	LibHalDrive	*drive;
	gboolean	 removable;

	if (!device_file) {
		return FALSE;
	}

	priv = GET_PRIV (storage);

	drive = libhal_drive_from_device_file (priv->context, device_file);
	if (!drive) {
		return FALSE;
	}

	removable = libhal_drive_uses_removable_media (drive);
	libhal_drive_free (drive);

	return removable;
}

static gboolean
hal_device_should_be_tracked (TrackerStorage *storage,
			      const gchar    *device_file)
{
	TrackerStoragePriv	*priv;
	LibHalDrive	*drive;
	LibHalDriveType  drive_type;
	gboolean	 eligible;

	if (!device_file) {
		return FALSE;
	}

	priv = GET_PRIV (storage);

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

static gboolean
hal_device_add (TrackerStorage *storage,
		LibHalVolume   *volume)
{
	TrackerStoragePriv *priv;
	DBusError	error;
	const gchar    *udi;
	const gchar    *mount_point;
	const gchar    *device_file;

	priv = GET_PRIV (storage);

	dbus_error_init (&error);

	udi = libhal_volume_get_udi (volume);
	mount_point = libhal_volume_get_mount_point (volume);
	device_file = libhal_volume_get_device_file (volume);

	if (g_hash_table_lookup (priv->all_devices, udi)) {
		return TRUE;
	}

	/* If there is no mount point, then there is nothing to track */
	if (!hal_device_should_be_tracked (storage, device_file)) {
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
		hal_mount_point_add (storage,
				     udi,
				     mount_point,
				     hal_device_is_removable (storage, device_file));
	}

	return TRUE;
}

static void
hal_device_added_cb (LibHalContext *context,
		     const gchar   *udi)
{
	TrackerStorage *storage;
	LibHalVolume   *volume;

	storage = libhal_ctx_get_user_data (context);

	if (libhal_device_query_capability (context, udi, CAPABILITY_VOLUME, NULL)) {
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

		hal_device_add (storage, volume);
		libhal_volume_free (volume);
	}
}

static void
hal_device_removed_cb (LibHalContext *context,
		       const gchar   *udi)
{
	TrackerStorage     *storage;
	TrackerStoragePriv *priv;
	const gchar        *device_file;
	const gchar        *mount_point;

	storage = (TrackerStorage*) libhal_ctx_get_user_data (context);
	priv = GET_PRIV (storage);

	if (g_hash_table_lookup (priv->all_devices, udi)) {
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

		hal_mount_point_remove (storage, udi);
	}
}

static void
hal_device_property_modified_cb (LibHalContext *context,
				 const char    *udi,
				 const char    *key,
				 dbus_bool_t	is_removed,
				 dbus_bool_t	is_added)
{
	TrackerStorage     *storage;
	TrackerStoragePriv *priv;
	DBusError	    error;

	storage = (TrackerStorage*) libhal_ctx_get_user_data (context);
	priv = GET_PRIV (storage);

	dbus_error_init (&error);

	if (g_hash_table_lookup (priv->all_devices, udi)) {
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
			hal_mount_point_remove (storage, udi);
			return;
		}

		if (is_mounted) {
			LibHalVolume *volume;
			const gchar  *mount_point;

			volume = libhal_volume_from_udi (context, udi);
			mount_point = libhal_volume_get_mount_point (volume);

			g_message ("HAL device:'%s' with udi:'%s' is now mounted",
				   device_file,
				   udi);

			hal_mount_point_add (storage,
					     udi,
					     mount_point,
					     hal_device_is_removable (storage, device_file));

			libhal_volume_free (volume);
		} else {
			g_message ("HAL device:'%s' with udi:'%s' is now unmounted",
				   device_file,
				   udi);

			hal_mount_point_remove (storage, udi);
		}
	}
}

/**
 * tracker_storage_new:
 *
 * Creates a new instance of #TrackerStorage.
 *
 * Returns: The newly created #TrackerStorage.
 **/
TrackerStorage *
tracker_storage_new ()
{
	return g_object_new (TRACKER_TYPE_STORAGE, NULL);
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
 * tracker_storage_get_mounted_directory_roots:
 * @storage: A #TrackerStorage
 *
 * Returns a #Glist of strings containing the root directories for mounted devices.
 * Each element must be freed using g_free() and the list itself using g_list_free().
 *
 * Returns: The list of root directories.
 **/
GList *
tracker_storage_get_mounted_directory_roots (TrackerStorage *storage)
{
	TrackerStoragePriv *priv;
	GetRoots	gr;

	g_return_val_if_fail (TRACKER_IS_STORAGE (storage), NULL);

	priv = GET_PRIV (storage);

	gr.context = priv->context;
	gr.roots = NULL;

	g_hash_table_foreach (priv->mounted_devices,
			      hal_get_mount_point_by_udi_foreach,
			      &gr);

	return gr.roots;
}

/**
 * tracker_storage_get_removable_device_roots:
 * @storage: A #TrackerStorage
 *
 * Returns a #GList of strings containing the root directories for removable devices.
 * Each element must be freed using g_free() and the list itself through g_list_free().
 *
 * Returns: The list of root directories.
 **/
GList *
tracker_storage_get_removable_device_roots (TrackerStorage *storage)
{
	TrackerStoragePriv *priv;
	GetRoots	gr;

	g_return_val_if_fail (TRACKER_IS_STORAGE (storage), NULL);

	priv = GET_PRIV (storage);

	gr.context = priv->context;
	gr.roots = NULL;

	g_hash_table_foreach (priv->removable_devices,
			      hal_get_mount_point_by_udi_foreach,
			      &gr);

	return gr.roots;
}

/**
 * tracker_storage_path_is_on_removable_device:
 * @storage: A #TrackerStorage
 * @uri: a uri
 * @mount_mount: if @uri is on a removable device, the mount point will
 * be filled in here. You must free the returned result
 * @available: if @uri is on a removable device, this will be set to 
 * TRUE in case the file is available right now
 *
 * Returns Whether or not @uri is on a known removable device
 *
 * Returns: TRUE if @uri on a known removable device, FALSE otherwise
 **/
gboolean
tracker_storage_uri_is_on_removable_device (TrackerStorage *storage,
					    const gchar    *uri,
					    gchar         **mount_point,
					    gboolean       *available)
{
	TrackerStoragePriv *priv;
	GHashTableIter  iter;
	gboolean        found = FALSE;
	gpointer        key, value;
	gchar          *path;
	GFile          *file;

	g_return_val_if_fail (TRACKER_IS_STORAGE (storage), FALSE);

	file = g_file_new_for_uri (uri);
	path = g_file_get_path (file);

	if (!path) {
		g_object_unref (file);
		return FALSE;
	}

	priv = GET_PRIV (storage);

	g_hash_table_iter_init (&iter, priv->removable_devices);

	while (g_hash_table_iter_next (&iter, &key, &value)) {
		const gchar *mp;

		mp = value;

		if (mp && path &&
		    g_str_has_prefix (path, mp)) {
			found = TRUE;

			if (mount_point) {
				*mount_point = g_strdup (mp);
			}

			if (available) {
				*available = TRUE;
			}

			break;
		}
	}

	g_free (path);
	g_object_unref (file);

	return found;
}


/**
 * tracker_storage_get_removable_device_udis:
 * @storage: A #TrackerStorage
 *
 * Returns a #GList of strings containing the UDI for removable devices.
 * Each element is owned by the #GHashTable internally, the list
 * itself through should be freed using g_list_free().
 *
 * Returns: The list of UDIs.
 **/
GList *
tracker_storage_get_removable_device_udis (TrackerStorage *storage)
{
	TrackerStoragePriv *priv;

	g_return_val_if_fail (TRACKER_IS_STORAGE (storage), NULL);

	priv = GET_PRIV (storage);
	
	return g_hash_table_get_keys (priv->removable_devices);
}

/**
 * tracker_storage_udi_get_mount_point:
 * @storage: A #TrackerStorage
 * @udi: A string pointer to the UDI for the device.
 *
 * Returns: The mount point for @udi, this should not be freed.
 **/
const gchar *
tracker_storage_udi_get_mount_point (TrackerStorage *storage,
				     const gchar    *udi)
{
	TrackerStoragePriv *priv;

	g_return_val_if_fail (TRACKER_IS_STORAGE (storage), NULL);
	g_return_val_if_fail (udi != NULL, NULL);

	priv = GET_PRIV (storage);
	
	return g_hash_table_lookup (priv->removable_devices, udi);
}

/**
 * tracker_storage_udi_get_mount_point:
 * @storage: A #TrackerStorage
 * @udi: A #gboolean
 *
 * Returns: The %TRUE if @udi is mounted or %FALSE if it isn't.
 **/
gboolean    
tracker_storage_udi_get_is_mounted (TrackerStorage *storage,
				    const gchar    *udi)
{
	TrackerStoragePriv *priv;
	LibHalVolume   *volume;
	const gchar    *mount_point;
	gboolean        is_mounted;

	g_return_val_if_fail (TRACKER_IS_STORAGE (storage), FALSE);
	g_return_val_if_fail (udi != NULL, FALSE);

	priv = GET_PRIV (storage);

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



/**
 * tracker_storage_get_volume_udi_for_file:
 * @storage: A #TrackerStorage
 * @file: a file
 *
 * Returns the UDI of the removable device for @file
 *
 * Returns: Returns the UDI of the removable device for @file
 **/
const gchar *
tracker_storage_get_volume_udi_for_file (TrackerStorage *storage,
					 GFile          *file)
{
	TrackerStoragePriv *priv;
	GHashTableIter  iter;
	gboolean        found = FALSE;
	gpointer        key, value;
	gchar          *path;
	const gchar    *udi;

	g_return_val_if_fail (TRACKER_IS_STORAGE (storage), FALSE);

	path = g_file_get_path (file);

	if (!path) {
		return NULL;
	}

	priv = GET_PRIV (storage);

	g_hash_table_iter_init (&iter, priv->removable_devices);

	while (g_hash_table_iter_next (&iter, &key, &value)) {
		LibHalVolume  *volume;
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
				libhal_volume_free (volume);
				break;
			}
		}

		libhal_volume_free (volume);
	}

	g_free (path);

	if (!found)
		udi = NULL;

	return udi;
}
#endif /* HAVE_HAL */
