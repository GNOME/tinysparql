/*
 * Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <glib-object.h>
#include <gobject/gvaluecollector.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus-glib-bindings.h>
#include "mock-dbus-gproxy.h"
#include <sys/types.h>
#include <unistd.h>
#include <string.h>

static gint connection_counter = 0;

DBusGConnection *
dbus_g_bus_get (DBusBusType type, GError **error)
{
	connection_counter += 1;
	return (DBusGConnection *)g_strdup ("hi");
}

DBusGProxy *
dbus_g_proxy_new_for_name (DBusGConnection *conn,
                           const gchar *service,
                           const gchar *path,
                           const gchar *interface)
{
	return (DBusGProxy *)mock_dbus_gproxy_new ();
}

DBusGConnection *
dbus_g_connection_ref (DBusGConnection *conn)
{
	connection_counter += 1;
	return conn;
}

void
dbus_g_connection_unref (DBusGConnection *connection)
{
	connection_counter -= 1;
}

gchar *
dbus_g_method_get_sender (DBusGMethodInvocation *context)
{
	return g_strdup ("hardcoded sender");
}

gboolean
dbus_g_proxy_call (DBusGProxy *proxy,
                   const gchar *function_name,
                   GError  **error,
                   GType first_arg_type, ...)
{
	va_list  args;
	GType    arg_type;

	va_start (args, first_arg_type);
	if (g_strcmp0 (function_name, "GetConnectionUnixProcessID") == 0) {
		/*
		 * G_TYPE_STRING, sender
		 * G_TYPE_INVALID,
		 * G_TYPE_UINT, pid
		 * G_TYPE_INVALID,
		 */
		GValue value = { 0, };
		gchar *local_error = NULL;

		/* Input string (ignore) */
		g_value_init (&value, G_TYPE_STRING);
		G_VALUE_COLLECT (&value, args, 0, &local_error);
		g_free (local_error);
		g_value_unset (&value);

		arg_type = va_arg (args, GType);
		g_assert (arg_type == G_TYPE_INVALID);

		arg_type = va_arg (args, GType);
		g_assert (arg_type == G_TYPE_UINT);

		g_value_init (&value, arg_type);
		g_value_set_uint (&value, (guint) getpid ());
		G_VALUE_LCOPY (&value,
		               args, 0,
		               &local_error);
		g_free (local_error);
		g_value_unset (&value);
	} else {
		g_critical ("dbus_g_proxy_call '%s' unsupported", function_name);
	}
	va_end (args);
	return TRUE;
}
