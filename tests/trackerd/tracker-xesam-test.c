/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
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
#include <glib.h>
#include <glib/gtestutils.h>

#include <dbus/dbus-glib-bindings.h>

#include "tracker-xesam-session-test.h"
#include "tracker-xesam-hits-test.h"
#include "tracker-xesam-hit-test.h"

#include "tracker-xesam-test.h"

/*
 * This is a hack to initialize the dbus glib specialized types.
 * See bug https://bugs.freedesktop.org/show_bug.cgi?id=13908
 */
static void
init_dbus_glib_types (void)
{
	DBusGConnection *connection;
	GError			*error;
	error = NULL;
	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	dbus_g_connection_unref (connection);
}

int
main (int argc, char **argv) {

	int result;

	g_type_init ();
	g_test_init (&argc, &argv, NULL);

	init_dbus_glib_types();

	g_test_bug_base ("http://bugzilla.gnome.org/");

	g_test_add_session_tests ();
	g_test_add_hit_tests ();
	g_test_add_hits_tests ();

	result = g_test_run ();

	return result;
}
