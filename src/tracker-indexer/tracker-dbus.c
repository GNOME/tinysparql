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

#include <string.h>

#include <libtracker-common/tracker-log.h>

#include "tracker-dbus.h"
#include "tracker-indexer.h"
#include "tracker-indexer-glue.h"

#define THUMBNAILER_SERVICE	 "org.freedesktop.thumbnailer"
#define THUMBNAILER_PATH	 "/org/freedesktop/thumbnailer/Generic"
#define THUMBNAILER_INTERFACE	 "org.freedesktop.thumbnailer.Generic"

static DBusGConnection *connection;
static DBusGProxy      *proxy;
static DBusGProxy      *thumb_proxy;

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
		g_critical ("Could not aquire name:'%s', %s",
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
	if (strcmp (name, TRACKER_DAEMON_SERVICE) == 0 && (!new_owner || !*new_owner)) {
		/* Tracker daemon has dissapeared from
		 * the bus, shutdown the indexer.
		 */
		tracker_indexer_stop (TRACKER_INDEXER (user_data));
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

DBusGProxy*
tracker_dbus_get_thumbnailer (void)
{
	return thumb_proxy;
}

static gboolean
dbus_register_names (void)
{
	GError *error = NULL;

	if (connection) {
		g_critical ("The DBusGConnection is already set, have we already initialized?");
		return FALSE;
	}

	if (proxy) {
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
	proxy = dbus_g_proxy_new_for_name (connection,
					   DBUS_SERVICE_DBUS,
					   DBUS_PATH_DBUS,
					   DBUS_INTERFACE_DBUS);

	/* Register the service name for org.freedesktop.Tracker */
	if (!dbus_register_service (proxy, TRACKER_INDEXER_SERVICE)) {
		return FALSE;
	}

	thumb_proxy = dbus_g_proxy_new_for_name (connection,
						 THUMBNAILER_SERVICE,
						 THUMBNAILER_PATH,
						 THUMBNAILER_INTERFACE);
	return TRUE;
}

gboolean
tracker_dbus_init (void)
{
	/* Don't reinitialize */
	if (connection && proxy) {
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
	if (proxy) {
		g_object_unref (proxy);
		proxy = NULL;
	}

	connection = NULL;
}

gboolean
tracker_dbus_register_object (GObject *object)
{
	if (!connection || !proxy) {
		g_critical ("DBus support must be initialized before registering objects!");
		return FALSE;
	}

	if (TRACKER_IS_INDEXER (object)) {
		return dbus_register_object (object,
					     connection,
					     proxy,
					     &dbus_glib_tracker_indexer_object_info,
					     TRACKER_INDEXER_PATH);
	} else {
		g_warning ("Object not handled by DBus");
	}

	return FALSE;
}
