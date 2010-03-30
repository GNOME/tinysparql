/*
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

/*
 * Overload of some dbus functions to test tracker-thumbnailer.
 */
#include <glib.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus-glib.h>
#include "mock-dbus-gproxy.h"

DBusGConnection*
dbus_g_bus_get (DBusBusType type, GError **error)
{
	return (DBusGConnection *)g_strdup ("mock connection");
}

DBusGProxy*
dbus_g_proxy_new_for_name (DBusGConnection *connection,
                           const char *name,
                           const char *path,
                           const char *interface)
{
	return (DBusGProxy *)mock_dbus_gproxy_new ();
}

gboolean
dbus_g_proxy_call (DBusGProxy *proxy,
                   const char *method,
                   GError **error,
                   GType first_arg_type,
                   ...)
{
	va_list args;

	g_message ("DBUS-CALL: %s", method);

	va_start (args, first_arg_type);

	if (g_strcmp0 (method, "GetSupported") == 0) {
		GType *t;
		GStrv *mime_types;
		gchar *mimetypes[] = {"image/jpeg", "image/png", NULL};

		t = va_arg (args, GType*);
		mime_types = va_arg (args, GStrv*);

		*mime_types = g_strdupv (mimetypes);
	}

	va_end (args);
	return TRUE;
}

void
dbus_g_proxy_call_no_reply (DBusGProxy *proxy,
                            const char *method,
                            GType first_arg_type,...)
{
	g_message ("DBUS-CALL: %s", method);
}

