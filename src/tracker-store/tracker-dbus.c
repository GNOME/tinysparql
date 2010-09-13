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
#include <libtracker-common/tracker-log.h>
#include <libtracker-common/tracker-utils.h>
#include <libtracker-common/tracker-ontologies.h>

#include <libtracker-data/tracker-data.h>
#include <libtracker-data/tracker-db-dbus.h>
#include <libtracker-data/tracker-db-manager.h>

#include "tracker-dbus.h"
#include "tracker-resources.h"
#include "tracker-resources-glue.h"
#include "tracker-status.h"
#include "tracker-status-glue.h"
#include "tracker-statistics.h"
#include "tracker-statistics-glue.h"
#include "tracker-backup.h"
#include "tracker-backup-glue.h"
#include "tracker-marshal.h"
#include "tracker-steroids.h"

static DBusGConnection *connection;
static DBusGProxy      *gproxy;
static GSList          *objects;
static TrackerStatus   *notifier;
static TrackerBackup   *backup;
static TrackerSteroids *steroids;

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

gboolean
tracker_dbus_register_names (void)
{
	/* Register the service name for org.freedesktop.Tracker */
	if (!dbus_register_service (gproxy, TRACKER_STATISTICS_SERVICE)) {
		return FALSE;
	}

	return TRUE;
}

gboolean
tracker_dbus_init (void)
{
	GError *error = NULL;

	/* Don't reinitialize */
	if (objects) {
		return TRUE;
	}

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

	dbus_g_proxy_add_signal (gproxy, "NameOwnerChanged",
	                         G_TYPE_STRING,
	                         G_TYPE_STRING,
	                         G_TYPE_STRING,
	                         G_TYPE_INVALID);

	return TRUE;
}

static void
name_owner_changed_cb (DBusGProxy *proxy,
                       gchar      *name,
                       gchar      *old_owner,
                       gchar      *new_owner,
                       gpointer    user_data)
{
	if (tracker_is_empty_string (new_owner) && !tracker_is_empty_string (old_owner)) {
		/* This means that old_owner got removed */
		tracker_resources_unreg_batches (user_data, old_owner);
	}
}

static void
name_owner_changed_closure (gpointer  data,
                            GClosure *closure)
{
}

static void
dbus_set_available (gboolean available)
{
	if (available) {
		if (!objects) {
			tracker_dbus_register_objects ();
		}
	} else {
		GSList *l;

		if (objects) {
			dbus_g_proxy_disconnect_signal (gproxy,
			                                "NameOwnerChanged",
			                                G_CALLBACK (name_owner_changed_cb),
			                                tracker_dbus_get_object (TRACKER_TYPE_RESOURCES));
		}

		if (steroids) {
			dbus_connection_remove_filter (dbus_g_connection_get_connection (connection),
			                               tracker_steroids_connection_filter,
			                               steroids);
			g_object_unref (steroids);
			steroids = NULL;
		}


		for (l = objects; l; l = l->next) {
			dbus_g_connection_unregister_g_object (connection, l->data);
			g_object_unref (l->data);
		}

		g_slist_free (objects);
		objects = NULL;
	}
}

void
tracker_dbus_shutdown (void)
{
	dbus_set_available (FALSE);

	if (backup) {
		dbus_g_connection_unregister_g_object (connection, G_OBJECT (backup));
		g_object_unref (backup);
	}

	if (notifier) {
		dbus_g_connection_unregister_g_object (connection, G_OBJECT (notifier));
		g_object_unref (notifier);
	}

	if (gproxy) {
		g_object_unref (gproxy);
		gproxy = NULL;
	}

	connection = NULL;
}

TrackerStatus*
tracker_dbus_register_notifier (void)
{
	if (!connection || !gproxy) {
		g_critical ("D-Bus support must be initialized before registering objects!");
		return FALSE;
	}

	/* Add org.freedesktop.Tracker */
	notifier = tracker_status_new ();
	if (!notifier) {
		g_critical ("Could not create TrackerStatus object to register");
		return FALSE;
	}

	dbus_register_object (connection,
	                      gproxy,
	                      G_OBJECT (notifier),
	                      &dbus_glib_tracker_status_object_info,
	                      TRACKER_STATUS_PATH);

	return g_object_ref (notifier);
}

gboolean
tracker_dbus_register_objects (void)
{
	gpointer object, resources;

	if (!connection || !gproxy) {
		g_critical ("D-Bus support must be initialized before registering objects!");
		return FALSE;
	}

	/* Add org.freedesktop.Tracker */
	object = tracker_statistics_new ();
	if (!object) {
		g_critical ("Could not create TrackerStatistics object to register");
		return FALSE;
	}

	dbus_register_object (connection,
	                      gproxy,
	                      G_OBJECT (object),
	                      &dbus_glib_tracker_statistics_object_info,
	                      TRACKER_STATISTICS_PATH);
	objects = g_slist_prepend (objects, object);

	/* Add org.freedesktop.Tracker1.Resources */
	object = resources = tracker_resources_new (connection);
	if (!object) {
		g_critical ("Could not create TrackerResources object to register");
		return FALSE;
	}

	dbus_g_proxy_connect_signal (gproxy, "NameOwnerChanged",
	                             G_CALLBACK (name_owner_changed_cb),
	                             object,
	                             name_owner_changed_closure);

	dbus_register_object (connection,
	                      gproxy,
	                      G_OBJECT (object),
	                      &dbus_glib_tracker_resources_object_info,
	                      TRACKER_RESOURCES_PATH);
	objects = g_slist_prepend (objects, object);

	if (!steroids) {
		/* Add org.freedesktop.Tracker1.Steroids */
		steroids = tracker_steroids_new ();
		if (!steroids) {
			g_critical ("Could not create TrackerSteroids object to register");
			return FALSE;
		}

		dbus_connection_add_filter (dbus_g_connection_get_connection (connection),
		                            tracker_steroids_connection_filter,
		                            G_OBJECT (steroids),
		                            NULL);
		/* Note: TrackerSteroids should not go to the 'objects' list, as it is
		 * a filter, not an object registered */
	}

	/* Reverse list since we added objects at the top each time */
	objects = g_slist_reverse (objects);

	if (!backup) {
		/* Add org.freedesktop.Tracker1.Backup */
		backup = tracker_backup_new ();
		if (!backup) {
			g_critical ("Could not create TrackerBackup object to register");
			return FALSE;
		}

		dbus_register_object (connection,
		                      gproxy,
		                      G_OBJECT (backup),
		                      &dbus_glib_tracker_backup_object_info,
		                      TRACKER_BACKUP_PATH);
		/* Backup object isn't part of the linked list, set_available wouldn't
		 * work correctly from the dbus call otherwise */
	}

	return TRUE;
}

gboolean
tracker_dbus_register_prepare_class_signal (void)
{
	gpointer resources;

	resources = tracker_dbus_get_object (TRACKER_TYPE_RESOURCES);

	if (!resources) {
		g_message ("Error during initialization, Resources DBus object not available");
		return FALSE;
	}

	tracker_resources_prepare (resources);

	return TRUE;
}

GObject *
tracker_dbus_get_object (GType type)
{
	GSList *l;

	for (l = objects; l; l = l->next) {
		if (G_OBJECT_TYPE (l->data) == type) {
			return l->data;
		}
	}

	if (steroids && type == TRACKER_TYPE_STEROIDS) {
		return G_OBJECT (steroids);
	}

	if (notifier && type == TRACKER_TYPE_STATUS) {
		return G_OBJECT (notifier);
	}

	if (backup && type == TRACKER_TYPE_BACKUP) {
		return G_OBJECT (backup);
	}

	return NULL;
}

