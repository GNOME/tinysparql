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

#include <libtracker-common/tracker-utils.h>

#include "tracker-dbus.h"
#include "tracker-miner-dbus.h"

typedef struct {
	DBusGConnection *connection;
	GHashTable *name_monitors;
} DBusData;

static GQuark dbus_data = 0;
static GQuark dbus_interface_quark = 0;
static GQuark name_owner_changed_signal_quark = 0;

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
dbus_register_object (GObject               *object,
                      DBusGConnection       *connection,
                      DBusGProxy            *proxy,
                      const DBusGObjectInfo *info,
                      const gchar           *path)
{
	g_message ("Registering D-Bus object...");
	g_message ("  Path:'%s'", path);
	g_message ("  Object Type:'%s'", G_OBJECT_TYPE_NAME (object));

	dbus_g_object_type_install_info (G_OBJECT_TYPE (object), info);
	dbus_g_connection_register_g_object (connection, path, object);

	return TRUE;
}

static gchar *
get_name_owner_changed_match_rule (const gchar *name)
{
	return g_strdup_printf ("type='signal',"
	                        "sender='" DBUS_SERVICE_DBUS "',"
	                        "interface='" DBUS_INTERFACE_DBUS "',"
	                        "path='" DBUS_PATH_DBUS "',"
	                        "member='NameOwnerChanged',"
	                        "arg0='%s'", name);
}

static void
name_owner_changed_cb (const gchar *name,
                       const gchar *old_owner,
                       const gchar *new_owner,
                       gpointer     user_data)
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

static DBusHandlerResult
message_filter (DBusConnection *connection,
                DBusMessage    *message,
                gpointer        user_data)
{
	const char *tmp;
	GQuark interface, member;
	int message_type;

	tmp = dbus_message_get_interface (message);
	interface = tmp ? g_quark_try_string (tmp) : 0;
	tmp = dbus_message_get_member (message);
	member = tmp ? g_quark_try_string (tmp) : 0;
	message_type = dbus_message_get_type (message);

	if (interface == dbus_interface_quark &&
		message_type == DBUS_MESSAGE_TYPE_SIGNAL &&
		member == name_owner_changed_signal_quark) {
		const gchar *name, *prev_owner, *new_owner;

		if (dbus_message_get_args (message, NULL,
		                           DBUS_TYPE_STRING, &name,
		                           DBUS_TYPE_STRING, &prev_owner,
		                           DBUS_TYPE_STRING, &new_owner,
		                           DBUS_TYPE_INVALID)) {
			name_owner_changed_cb (name, prev_owner, new_owner, user_data);
		}
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void
dbus_data_destroy (gpointer data)
{
	DBusData *dd;

	dd = data;

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
	                           connection,
	                           gproxy,
	                           info,
	                           full_path)) {
		g_object_unref (gproxy);
		g_free (full_path);
		return NULL;
	}

	g_free (full_path);

	dbus_connection_add_filter (dbus_g_connection_get_connection (connection),
	                            message_filter,
	                            miner,
	                            NULL);

	/* Now we're successfully connected and registered, create the data */
	data = g_slice_new0 (DBusData);

	/* Connection object is a shared one, so we need to keep our own
	 * reference to it
	 */
	data->connection = dbus_g_connection_ref (connection);
	data->name_monitors = g_hash_table_new_full (g_str_hash,
	                                             g_str_equal,
	                                             (GDestroyNotify) g_free,
	                                             NULL);

	g_object_unref (gproxy);

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

	dbus_interface_quark = g_quark_from_static_string ("org.freedesktop.DBus");
	name_owner_changed_signal_quark = g_quark_from_static_string ("NameOwnerChanged");

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
_tracker_miner_dbus_remove_name_watch (TrackerMiner             *miner,
                                       const gchar              *name,
                                       TrackerMinerDBusNameFunc  func)
{
	DBusData *data;
	gchar *match_rule;

	g_return_if_fail (TRACKER_IS_MINER (miner));
	g_return_if_fail (name != NULL);

	data = g_object_get_qdata (G_OBJECT (miner), dbus_data);

	if (!data) {
		g_critical ("Miner '%s' was not registered on "
		            "DBus, can watch for owner changes",
		            G_OBJECT_TYPE_NAME (miner));
		return;
	}

	g_hash_table_remove (data->name_monitors, name);

	match_rule = get_name_owner_changed_match_rule (name);
	dbus_bus_remove_match (dbus_g_connection_get_connection (data->connection),
	                       match_rule,
	                       NULL);
	g_free (match_rule);
}

void
_tracker_miner_dbus_add_name_watch (TrackerMiner             *miner,
                                    const gchar              *name,
                                    TrackerMinerDBusNameFunc  func)
{
	DBusData *data;
	DBusConnection *connection;
	gchar *match_rule;

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

	match_rule = get_name_owner_changed_match_rule (name);
	connection = dbus_g_connection_get_connection (data->connection);
	dbus_bus_add_match (connection, match_rule, NULL);

	if (!dbus_bus_name_has_owner (connection, name, NULL)) {
		/* Ops, the name went away before we could receive
		 * NameOwnerChanged for it.
		 */
		name_owner_changed_cb ("", name, name, miner);
	}

	g_free (match_rule);
}
