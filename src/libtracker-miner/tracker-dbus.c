/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
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

#include <stdlib.h>

#include <dbus/dbus-glib-bindings.h>

#include "tracker-dbus.h"
#include "tracker-miner-dbus.h"

typedef struct {
        DBusGConnection *connection;
        DBusGProxy *gproxy;
        GHashTable *name_monitors;
} DBusData;

static GQuark dbus_data = 0;

static gboolean
dbus_register_service (DBusGProxy  *proxy,
		       const gchar *name)
{
	GError *error = NULL;
	guint	result;

	g_message ("Registering D-Bus service...\n"
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
		g_critical ("D-Bus service name:'%s' is already taken, "
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
	g_message ("Registering D-Bus object...");
	g_message ("  Path:'%s'", path);
	g_message ("  Object Type:'%s'", G_OBJECT_TYPE_NAME (object));

	dbus_g_object_type_install_info (G_OBJECT_TYPE (object), info);
	dbus_g_connection_register_g_object (connection, path, object);

	return TRUE;
}

static void
name_owner_changed_cb (DBusGProxy *proxy,
		       gchar	  *name,
		       gchar	  *old_owner,
		       gchar	  *new_owner,
		       gpointer    user_data)
{
	TrackerMinerDBusNameFunc func;
        TrackerMiner *miner;
        gboolean available;
        DBusData *data;

        miner = user_data;

        if (!name || !*name) {
                return;
        }

	data = g_object_get_qdata (G_OBJECT (miner), dbus_data);

        if (!data) {
                return;
        }

        func = g_hash_table_lookup (data->name_monitors, name);

        if (!func) {
                return;
        }

	available = (new_owner && *new_owner);
        (func) (miner, name, available);
}

static void
dbus_set_name_monitor (TrackerMiner *miner,
		       DBusGProxy   *proxy)
{
	dbus_g_proxy_add_signal (proxy, "NameOwnerChanged",
				 G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
				 G_TYPE_INVALID);

	dbus_g_proxy_connect_signal (proxy, "NameOwnerChanged",
				     G_CALLBACK (name_owner_changed_cb),
				     miner, NULL);
}

static void
dbus_data_destroy (gpointer data)
{
	DBusData *dd;

	dd = data;

	if (dd->gproxy) {
		g_object_unref (dd->gproxy);
	}

	if (dd->connection) {
		dbus_g_connection_unref (dd->connection);
	}

	if (dd->name_monitors) {
		g_hash_table_unref (dd->name_monitors);
	}

	g_slice_free (DBusData, dd);
}

static DBusData *
dbus_data_create (TrackerMiner          *miner,
		  const gchar           *name,
                  const DBusGObjectInfo *info)
{
	DBusData *data;
	DBusGConnection *connection;
	DBusGProxy *gproxy;
	GError *error = NULL;
	gchar *full_name, *full_path;

	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

	if (!connection) {
		g_critical ("Could not connect to the D-Bus session bus, %s",
			    error ? error->message : "no error given.");
		g_error_free (error);
		return NULL;
	}

	gproxy = dbus_g_proxy_new_for_name (connection,
					    DBUS_SERVICE_DBUS,
					    DBUS_PATH_DBUS,
					    DBUS_INTERFACE_DBUS);

	/* Register the service name for the miner */
	full_name = g_strconcat (TRACKER_MINER_DBUS_NAME_PREFIX, name, NULL);

	if (!dbus_register_service (gproxy, full_name)) {
		g_object_unref (gproxy);
		g_free (full_name);
		return NULL;
	}

	g_free (full_name);

	full_path = g_strconcat (TRACKER_MINER_DBUS_PATH_PREFIX, name, NULL);

	if (!dbus_register_object (G_OBJECT (miner),
				   connection, gproxy,
                                   info,
				   full_path)) {
		g_object_unref (gproxy);
		g_free (full_path);
		return NULL;
	}

        dbus_set_name_monitor (miner, gproxy);

	g_free (full_path);

	/* Now we're successfully connected and registered, create the data */
	data = g_slice_new0 (DBusData);
	/* Connection object is a shared one, so we need to keep our own
	 * reference to it */
	data->connection = dbus_g_connection_ref (connection);
	data->gproxy = gproxy;
	data->name_monitors = g_hash_table_new_full (g_str_hash,
	                                             g_str_equal,
	                                             (GDestroyNotify) g_free,
	                                             NULL);

	return data;
}

void
_tracker_miner_dbus_init (TrackerMiner          *miner,
                          const DBusGObjectInfo *info)
{
	DBusData *data;
        gchar *name;

	g_return_if_fail (TRACKER_IS_MINER (miner));
	g_return_if_fail (info != NULL);

	if (G_UNLIKELY (dbus_data == 0)) {
		dbus_data = g_quark_from_static_string ("tracker-miner-dbus-data");
	}

	data = g_object_get_qdata (G_OBJECT (miner), dbus_data);

        if (data) {
                return;
        }

        g_object_get (miner, "name", &name, NULL);

	if (!name) {
		g_critical ("Miner '%s' should have been given a name, bailing out",
                            G_OBJECT_TYPE_NAME (miner));
		g_assert_not_reached ();
	}

        data = dbus_data_create (miner, name, info);

	if (G_UNLIKELY (!data)) {
		g_critical ("Miner could not register object on D-Bus session");
		exit (EXIT_FAILURE);
		return;
	}

	g_object_set_qdata_full (G_OBJECT (miner),
				 dbus_data,
				 data,
				 dbus_data_destroy);

	g_free (name);
}

void
_tracker_miner_dbus_shutdown (TrackerMiner *miner)
{
	g_return_if_fail (TRACKER_IS_MINER (miner));

	if (G_UNLIKELY (dbus_data == 0)) {
		return;
	}

	g_object_set_qdata (G_OBJECT (miner), dbus_data, NULL);
}

void
_tracker_miner_dbus_add_name_watch (TrackerMiner             *miner,
                                    const gchar              *name,
                                    TrackerMinerDBusNameFunc  func)
{
        DBusData *data;

	g_return_if_fail (TRACKER_IS_MINER (miner));
	g_return_if_fail (name != NULL);
	g_return_if_fail (func != NULL);

	data = g_object_get_qdata (G_OBJECT (miner), dbus_data);

        if (!data) {
                g_critical ("Miner '%s' was not registered on "
                            "DBus, can watch for owner changes",
                            G_OBJECT_TYPE_NAME (miner));
                return;
        }

        g_hash_table_insert (data->name_monitors,
                             g_strdup (name),
                             func);
}
