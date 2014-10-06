/*
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
 * Copyright (C) 2014, Lanedo <martyn@lanedo.com>
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

#include "config.h"

#include <gio/gio.h>
#include <glib/gi18n.h>

#include <libtracker-common/tracker-common.h>

#include "tracker-dbus.h"

gboolean
tracker_dbus_get_connection (const gchar      *name,
                             const gchar      *object_path,
                             const gchar      *interface_name,
                             GDBusProxyFlags   flags,
                             GDBusConnection **connection,
                             GDBusProxy      **proxy)
{
	GError *error = NULL;

	*connection = g_bus_get_sync (TRACKER_IPC_BUS, NULL, &error);

	if (!*connection) {
		g_critical ("%s, %s",
		            _("Could not get D-Bus connection"),
		            error ? error->message : _("No error given"));
		g_clear_error (&error);

		return FALSE;
	}

	*proxy = g_dbus_proxy_new_sync (*connection,
	                                flags,
	                                NULL,
	                                name,
	                                object_path,
	                                interface_name,
	                                NULL,
	                                &error);

	if (error) {
		g_critical ("%s, %s",
		            _("Could not create D-Bus proxy to tracker-store"),
		            error ? error->message : _("No error given"));
		g_clear_error (&error);

		return FALSE;
	}

	return TRUE;
}
