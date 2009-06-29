/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
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

#include <libtracker-common/tracker-log.h>

#include "tracker-dbus.h"

static DBusGConnection *connection;
static DBusGProxy      *gproxy;
static GHashTable      *name_monitors;

typedef struct {
	TrackerDBusNameMonitorFunc func;
	gpointer user_data;
	GDestroyNotify destroy_func;
} TrackerDBusNameMonitor;

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

static void
name_owner_changed_cb (DBusGProxy *proxy,
		       gchar	  *name,
		       gchar	  *old_owner,
		       gchar	  *new_owner,
		       gpointer    user_data)
{
	TrackerDBusNameMonitor *name_monitor;

	name_monitor = g_hash_table_lookup (name_monitors, name);

	if (name_monitor) {
		gboolean available;

		available = (new_owner && *new_owner);
		(name_monitor->func) (name, available, name_monitor->user_data);
	}
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

	dbus_g_proxy_add_signal (proxy, "NameOwnerChanged",
				 G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
				 G_TYPE_INVALID);

	dbus_g_proxy_connect_signal (proxy, "NameOwnerChanged",
				     G_CALLBACK (name_owner_changed_cb),
				     object, NULL);
	return TRUE;
}

static gboolean
dbus_register_names (void)
{
	GError *error = NULL;

	if (connection) {
		g_critical ("The DBusGConnection is already set, have we already initialized?");
		return FALSE;
	}

	if (gproxy) {
		g_critical ("The DBusGProxy is already set, have we already initialized?");
		return FALSE;
	}

	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

	if (!connection) {
		g_critical ("Could not connect to the DBus session bus, %s",
			    error ? error->message : "no error given.");
		g_clear_error (&error);
		return FALSE;
	}

	/* The definitions below (DBUS_SERVICE_DBUS, etc) are
	 * predefined for us to just use (dbus_g_proxy_...)
	 */
	gproxy = dbus_g_proxy_new_for_name (connection,
					    DBUS_SERVICE_DBUS,
					    DBUS_PATH_DBUS,
					    DBUS_INTERFACE_DBUS);

	/* Register the service name for org.freedesktop.Tracker */
	if (!dbus_register_service (gproxy, "org.freedesktop.Tracker.Indexer")) {
		return FALSE;
	}

	return TRUE;
}

static TrackerDBusNameMonitor *
name_monitor_new (TrackerDBusNameMonitorFunc func,
		  gpointer                   user_data,
		  GDestroyNotify             destroy_func)
{
	TrackerDBusNameMonitor *name_monitor;

	name_monitor = g_slice_new (TrackerDBusNameMonitor);
	name_monitor->func = func;
	name_monitor->user_data = user_data;
	name_monitor->destroy_func = destroy_func;

	return name_monitor;
}

static void
name_monitor_free (TrackerDBusNameMonitor *name_monitor)
{
	if (name_monitor->user_data && name_monitor->destroy_func) {
		(name_monitor->destroy_func) (name_monitor->user_data);
	}

	g_slice_free (TrackerDBusNameMonitor, name_monitor);
}

gboolean
tracker_dbus_init (void)
{
	/* Don't reinitialize */
	if (connection && gproxy) {
		return TRUE;
	}

	/* Register names and get proxy/connection details */
	if (!dbus_register_names ()) {
		return FALSE;
	}

	name_monitors = g_hash_table_new_full (g_str_hash,
					       g_str_equal,
					       (GDestroyNotify) g_free,
					       (GDestroyNotify) name_monitor_free);
	return TRUE;
}

void
tracker_dbus_shutdown (void)
{
	if (gproxy) {
		g_object_unref (gproxy);
		gproxy = NULL;
	}

	connection = NULL;

	g_hash_table_destroy (name_monitors);
	name_monitors = NULL;
}

gboolean
tracker_dbus_register_object (GObject               *object,
			      const DBusGObjectInfo *info,
			      const gchar	    *path)
{
	if (!connection || !gproxy) {
		g_critical ("DBus support must be initialized before registering objects!");
		return FALSE;
	}

	return dbus_register_object (object,
				     connection,
				     gproxy,
				     info,
				     path);
}

void
tracker_dbus_add_name_monitor (const gchar                *name,
			       TrackerDBusNameMonitorFunc  func,
			       gpointer                    user_data,
			       GDestroyNotify              destroy_func)
{
	g_return_if_fail (name != NULL);
	g_return_if_fail (func != NULL);

	if (!name_monitors) {
		g_critical ("DBus support must be initialized before adding name monitors!");
		return;
	}

	if (g_hash_table_lookup (name_monitors, name) != NULL) {
		g_critical ("There is already a name monitor for such name");
		return;
	}

	g_hash_table_insert (name_monitors,
			     g_strdup (name),
			     name_monitor_new (func, user_data, destroy_func));
}

void
tracker_dbus_remove_name_monitor (const gchar *name)
{
	g_return_if_fail (name != NULL);

	if (!name_monitors) {
		g_critical ("DBus support must be initialized before removing name monitors!");
		return;
	}

	g_hash_table_remove (name_monitors, name);
}
