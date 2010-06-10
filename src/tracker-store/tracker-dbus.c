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

#include <libtracker-db/tracker-db-dbus.h>
#include <libtracker-db/tracker-db-manager.h>

#include <libtracker-data/tracker-data.h>

#include "tracker-dbus.h"
#include "tracker-resources.h"
#include "tracker-resources-glue.h"
#include "tracker-resource-class.h"
#include "tracker-resources-class-glue.h"
#include "tracker-status.h"
#include "tracker-status-glue.h"
#include "tracker-statistics.h"
#include "tracker-statistics-glue.h"
#include "tracker-backup.h"
#include "tracker-backup-glue.h"
#include "tracker-marshal.h"

static DBusGConnection *connection;
static DBusGProxy      *gproxy;
static GSList          *objects;
TrackerStatus          *notifier = NULL;
TrackerBackup          *backup = NULL;

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

	/* Register the service name for org.freedesktop.Tracker */
	if (!dbus_register_service (gproxy, TRACKER_STATISTICS_SERVICE)) {
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

	dbus_g_proxy_add_signal (gproxy, "NameOwnerChanged",
	                         G_TYPE_STRING,
	                         G_TYPE_STRING,
	                         G_TYPE_STRING,
	                         G_TYPE_INVALID);

	return TRUE;
}

void
tracker_dbus_shutdown (void)
{
	tracker_dbus_set_available (FALSE);

	if (backup) {
		g_object_unref (backup);
	}

	if (notifier) {
		g_object_unref (notifier);
	}

	if (gproxy) {
		g_object_unref (gproxy);
		gproxy = NULL;
	}

	connection = NULL;
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

void
tracker_dbus_set_available (gboolean available)
{
	if (available) {
		if (!objects) {
			tracker_dbus_register_objects ();
		}
	} else {
		if (objects) {
			dbus_g_proxy_disconnect_signal (gproxy,
			                                "NameOwnerChanged",
			                                G_CALLBACK (name_owner_changed_cb),
			                                tracker_dbus_get_object (TRACKER_TYPE_RESOURCES));
			g_slist_foreach (objects, (GFunc) g_object_unref, NULL);
			g_slist_free (objects);
			objects = NULL;
		}
	}
}

static void
name_owner_changed_closure (gpointer  data,
                            GClosure *closure)
{
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
	TrackerDBResultSet *result_set;
	GSList *event_sources = NULL;
	GStrv classes, p;
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
	object = resources = tracker_resources_new ();
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

	/* Reverse list since we added objects at the top each time */
	objects = g_slist_reverse (objects);

	if (backup == NULL) {
		/* Add org.freedesktop.Tracker1.Backup */
		backup = tracker_backup_new ();

		if (!object) {
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

	result_set = tracker_data_query_sparql ("SELECT ?class WHERE { ?class tracker:notify true }", NULL);

	if (!result_set) {
		g_message ("No Nepomuk classes to register on D-Bus");
		return TRUE;
	}

	classes = tracker_dbus_query_result_to_strv (result_set, 0, NULL);
	g_object_unref (result_set);

	if (!classes) {
		g_message ("No Nepomuk classes to register on D-Bus");
		return TRUE;
	}

	for (p = classes; *p; p++) {
		TrackerNamespace *namespace;
		const gchar *rdf_class;
		gchar *namespace_uri;
		gchar *replaced, *path, *hash;

		rdf_class = *p;
		hash = strrchr (rdf_class, '#');

		if (!hash) {
			/* Support ontologies whose namespace
			 * uri does not end in a hash, e.g.
			 * dc.
			 */
			hash = strrchr (rdf_class, '/');
		}

		if (!hash) {
			g_critical ("Unknown namespace for class:'%s'",
			            rdf_class);
			continue;
		}

		namespace_uri = g_strndup (rdf_class, hash - rdf_class + 1);
		namespace = tracker_ontologies_get_namespace_by_uri (namespace_uri);
		g_free (namespace_uri);

		if (!namespace) {
			g_critical ("Unknown namespace:'%s' for class:'%s'",
			            namespace_uri,
			            rdf_class);
			continue;
		}

		replaced = g_strdup_printf ("%s/%s",
		                            tracker_namespace_get_prefix (namespace),
		                            hash + 1);
		path = g_strdup_printf (TRACKER_RESOURCES_CLASS_PATH,
		                        replaced);
		g_free (replaced);

		/* Add a org.freedesktop.Tracker1.Resources.Class */
		object = tracker_resource_class_new (rdf_class, path, connection);
		if (!object) {
			g_critical ("Could not create TrackerResourcesClass object to register:'%s' class",
			            rdf_class);
			g_free (path);
			return FALSE;
		}

		dbus_register_object (connection,
		                      gproxy,
		                      G_OBJECT (object),
		                      &dbus_glib_tracker_resources_class_object_info,
		                      path);
		g_free (path);

		/* TrackerResources takes over ownership and unrefs
		 * the gobjects too.
		 */
		event_sources = g_slist_prepend (event_sources, g_object_ref (object));
		objects = g_slist_prepend (objects, object);
	}

	g_strfreev (classes);

	tracker_resources_prepare (resources, event_sources);

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

	if (notifier && type == TRACKER_TYPE_STATUS) {
		return G_OBJECT (notifier);
	}

	if (backup && type == TRACKER_TYPE_BACKUP) {
		return G_OBJECT (backup);
	}

	return NULL;
}

