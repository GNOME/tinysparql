/*
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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

#include <dbus/dbus-glib-bindings.h>

#include "tracker-dbus-glib.h"

TrackerDBusRequest *
tracker_dbus_g_request_begin (DBusGMethodInvocation *context,
                              const gchar           *format,
                              ...)
{
	TrackerDBusRequest *request;
	gchar *str, *sender;
	va_list args;

	va_start (args, format);
	str = g_strdup_vprintf (format, args);
	va_end (args);

	sender = dbus_g_method_get_sender (context);

	request = tracker_dbus_request_begin (sender, "%s", str);

	g_free (sender);

	g_free (str);

	return request;
}
