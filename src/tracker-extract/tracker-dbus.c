/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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

#include <libtracker-common/tracker-dbus.h>

#include "tracker-dbus.h"
#include "tracker-extract.h"
#include "tracker-extract-glue.h"

static DBusGConnection *connection;
static DBusGProxy      *gproxy;
static GSList          *objects;

static gboolean
dbus_register_service (DBusGProxy  *proxy,
                       const gchar *name)
{
	GError *error = NULL;
	guint   result;

	g_message ("Registering D-Bus service...\n"
	           "  Name:'%s'",
	           name);

	if (!org_freedesktop_DBus_request_name (proxy,
	                                        name,
	                                        DBUS_NAME_FLAG_DO_NOT_QUEUE,
	                                        &result, &error)) {
		g_critical ("Could not aquire name:'%s', %s",
		            name,
		            error ? error->message : "no error given");
		g_error_free (error);

		return FALSE;
	}

	if (result != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
		g_critical ("D-Bus service name:'%s' is already taken, "
		            "perhaps the daemon is already running?",
		            name);
		return FALSE;
	}

	return TRUE;
}

static void
dbus_register_object (DBusGConnection       *lconnection,
                      DBusGProxy            *proxy,
                      GObject               *object,
                      const DBusGObjectInfo *info,
                      const gchar           *path)
{
	g_message ("Registering D-Bus object...");
	g_message ("  Path:'%s'", path);
	g_message ("  Type:'%s'", G_OBJECT_TYPE_NAME (object));

	dbus_g_object_type_install_info (G_OBJECT_TYPE (object), info);
	dbus_g_connection_register_g_object (lconnection, path, object);
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
		g_critical ("Could not connect to the D-Bus session bus, %s",
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

	/* Register the service name for org.freedesktop.Tracker1.Extract */
	if (!dbus_register_service (gproxy, TRACKER_EXTRACT_SERVICE)) {
		return FALSE;
	}

	return TRUE;
}

gboolean
tracker_dbus_init (void)
{
	/* Don't reinitialize */
	if (objects) {
		return TRUE;
	}

	/* Register names and get proxy/connection details */
	if (!dbus_register_names ()) {
		return FALSE;
	}

	return TRUE;
}

void
tracker_dbus_shutdown (void)
{
	if (objects) {
		g_slist_foreach (objects, (GFunc) g_object_unref, NULL);
		g_slist_free (objects);
		objects = NULL;
	}

	if (gproxy) {
		g_object_unref (gproxy);
		gproxy = NULL;
	}

	connection = NULL;
}

gboolean
tracker_dbus_register_objects (gpointer object)
{
	g_return_val_if_fail (TRACKER_IS_EXTRACT (object), FALSE);

	if (!connection || !gproxy) {
		g_critical ("D-Bus support must be initialized before registering objects!");
		return FALSE;
	}

	/* Add org.freedesktop.Tracker1.Extract */
	if (!object) {
		g_critical ("Could not create TrackerExtract object to register");
		return FALSE;
	}

	dbus_register_object (connection,
	                      gproxy,
	                      G_OBJECT (object),
	                      &dbus_glib_tracker_extract_object_info,
	                      TRACKER_EXTRACT_PATH);
	objects = g_slist_prepend (objects, object);

	return TRUE;
}

#ifdef HAVE_DBUS_FD_PASSING

gboolean
tracker_dbus_connection_add_filter (DBusHandleMessageFunction  function,
                                    void                      *user_data)
{
	if (!connection) {
		g_critical ("D-Bus support must be initialized before adding connection filters!");
		return FALSE;
	}

	dbus_connection_add_filter (dbus_g_connection_get_connection (connection),
	                            function,
	                            user_data,
	                            NULL);

	return TRUE;
}

#endif /* HAVE_DBUS_FD_PASSING */

GObject *
tracker_dbus_get_object (GType type)
{
	GSList *l;

	for (l = objects; l; l = l->next) {
		if (G_OBJECT_TYPE (l->data) == type) {
			return l->data;
		}
	}

	return NULL;
}
