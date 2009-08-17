/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008, Nokia
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

#include <string.h>

#include "tracker-dbus.h"
#include "tracker-miner-dbus.h"
#include "tracker-miner-glue.h"

typedef struct {
	DBusGConnection *connection;
	DBusGProxy      *gproxy;
	GHashTable      *name_monitors;
} TrackerDBusData;

static GQuark dbus_data = 0;

static gboolean
dbus_register_service (DBusGProxy  *proxy,
		       const gchar *name)
{
	GError *error = NULL;
	guint	result;

	g_message ("Registering DBus service...\n"
		   "  Name:'%s'",
		   name);

	if (!org_freedesktop_DBus_request_name (proxy,
						name,
						DBUS_NAME_FLAG_DO_NOT_QUEUE,
						&result, &error)) {
		g_critical ("Could not acquire name:'%s', %s",
			    name,
			    error ? error->message : "no error given");
		g_error_free (error);

		return FALSE;
	}

	if (result != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
		g_critical ("DBus service name:'%s' is already taken, "
			    "perhaps the application is already running?",
			    name);
		return FALSE;
	}

	return TRUE;
}

static gboolean
dbus_register_object (GObject		    *object,
		      DBusGConnection	    *connection,
		      DBusGProxy	    *proxy,
		      const DBusGObjectInfo *info,
		      const gchar	    *path)
{
	g_message ("Registering DBus object...");
	g_message ("  Path:'%s'", path);
	g_message ("  Object Type:'%s'", G_OBJECT_TYPE_NAME (object));

	dbus_g_object_type_install_info (G_OBJECT_TYPE (object), info);
	dbus_g_connection_register_g_object (connection, path, object);

	return TRUE;
}

static void
dbus_data_destroy (gpointer data)
{
	TrackerDBusData *dd;

	dd = data;

	if (dd->gproxy) {
		g_object_unref (dd->gproxy);
	}

	if (dd->connection) {
		dbus_g_connection_unref (dd->connection);
	}

	if (dd->name_monitors) {
		g_hash_table_destroy (dd->name_monitors);
	}

	g_slice_free (TrackerDBusData, dd);
}

static TrackerDBusData *
dbus_data_create (TrackerMiner *miner,
		  const gchar  *name)
{
	TrackerDBusData *data;
	DBusGConnection *connection;
	DBusGProxy *gproxy;
	GError *error = NULL;
	gchar *full_name, *full_path;

	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

	if (!connection) {
		g_critical ("Could not connect to the DBus session bus, %s",
			    error ? error->message : "no error given.");
		g_error_free (error);
		return NULL;
	}

	gproxy = dbus_g_proxy_new_for_name (connection,
					    DBUS_SERVICE_DBUS,
					    DBUS_PATH_DBUS,
					    DBUS_INTERFACE_DBUS);

	/* Register the service name for the miner */
	full_name = g_strconcat ("org.freedesktop.Tracker.Miner.", name, NULL);

	if (!dbus_register_service (gproxy, full_name)) {
		g_object_unref (gproxy);
		g_free (full_name);
		return NULL;
	}

	g_free (full_name);

	full_path = g_strconcat ("/org/freedesktop/Tracker/Miner/", name, NULL);

	if (!dbus_register_object (G_OBJECT (miner),
				   connection, gproxy,
				   &dbus_glib_tracker_miner_object_info,
				   full_path)) {
		g_object_unref (gproxy);
		g_free (full_path);
		return NULL;
	}

	g_free (full_path);

	/* Now we're successfully connected and registered, create the data */
	data = g_slice_new0 (TrackerDBusData);
	data->connection = dbus_g_connection_ref (connection);
	data->gproxy = g_object_ref (gproxy);

	return data;
}

gboolean
tracker_dbus_init (TrackerMiner *miner)
{
	TrackerDBusData *data;

	g_return_val_if_fail (TRACKER_IS_MINER (miner), FALSE);

	if (G_UNLIKELY (dbus_data == 0)) {
		dbus_data = g_quark_from_static_string ("tracker-miner-dbus-data");
	}

	data = g_object_get_qdata (G_OBJECT (miner), dbus_data);

	if (G_LIKELY (!data)) {
		const gchar *name;

		name = tracker_miner_get_name (miner);
		data = dbus_data_create (miner, name);
	}

	if (G_UNLIKELY (!data)) {
		return FALSE;
	}

	g_object_set_qdata_full (G_OBJECT (miner), 
				 dbus_data, 
				 data,
				 dbus_data_destroy);

	return TRUE;
}

void
tracker_dbus_shutdown (TrackerMiner *miner)
{
	if (dbus_data == 0) {
		return;
	}

	g_object_set_qdata (G_OBJECT (miner), dbus_data, NULL);
}
