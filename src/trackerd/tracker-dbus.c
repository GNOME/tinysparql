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

#include <libtracker-common/tracker-config.h>
#include <libtracker-common/tracker-dbus.h>
#include <libtracker-common/tracker-log.h>
#include <libtracker-common/tracker-utils.h>

#include <libtracker-db/tracker-db-manager.h>

#include "tracker-db.h"
#include "tracker-dbus.h"
#include "tracker-daemon.h"
#include "tracker-daemon-glue.h"
#include "tracker-files.h"
#include "tracker-files-glue.h"
#include "tracker-keywords.h"
#include "tracker-keywords-glue.h"
#include "tracker-metadata.h"
#include "tracker-metadata-glue.h"
#include "tracker-search.h"
#include "tracker-search-glue.h"
#include "tracker-xesam.h"
#include "tracker-xesam-glue.h"
#include "tracker-indexer-client.h"
#include "tracker-utils.h"
#include "tracker-marshal.h"
#include "tracker-status.h"
#include "tracker-main.h"

#define INDEXER_PAUSE_TIME_FOR_REQUESTS 10 /* seconds */

static DBusGConnection *connection;
static DBusGProxy      *proxy;
static DBusGProxy      *proxy_for_indexer;
static GSList	       *objects;
static guint		indexer_resume_timeout_id;

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
			    "perhaps the daemon is already running?",
			    name);
		return FALSE;
	}

	return TRUE;
}

static void
dbus_register_object (DBusGConnection	    *connection,
		      DBusGProxy	    *proxy,
		      GObject		    *object,
		      const DBusGObjectInfo *info,
		      const gchar	    *path)
{
	g_message ("Registering DBus object...");
	g_message ("  Path:'%s'", path);
	g_message ("  Type:'%s'", G_OBJECT_TYPE_NAME (object));

	dbus_g_object_type_install_info (G_OBJECT_TYPE (object), info);
	dbus_g_connection_register_g_object (connection, path, object);
}

static void
dbus_name_owner_changed (gpointer  data,
			 GClosure *closure)
{
	g_object_unref (data);
}

static gboolean
dbus_register_names (TrackerConfig *config)
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
	if (!dbus_register_service (proxy, TRACKER_DAEMON_SERVICE)) {
		return FALSE;
	}

	/* Register the service name for org.freedesktop.xesam if XESAM is enabled */
	if (tracker_config_get_enable_xesam (config)) {
		if (!dbus_register_service (proxy, TRACKER_XESAM_SERVICE)) {
			return FALSE;
		}
	}

	return TRUE;
}

static void
indexer_continue_async_cb (DBusGProxy *proxy,
			   GError     *error,
			   gpointer    user_data)
{
	if (error) {
		g_message ("Couldn't resume the indexer: %s", error->message);
		g_error_free (error);
	}

	g_object_unref (proxy);
}

static gboolean
indexer_resume_cb (gpointer user_data)
{
	DBusGProxy *proxy;

	proxy = user_data;

	if (!tracker_status_get_is_paused_manually () &&
	    !tracker_status_get_is_paused_for_io ()) {
		org_freedesktop_Tracker_Indexer_continue_async (g_object_ref (proxy),
								indexer_continue_async_cb,
								NULL);
	}

	return FALSE;
}

static void
indexer_resume_destroy_notify_cb (gpointer user_data)
{
	g_object_unref (user_data);
	indexer_resume_timeout_id = 0;
}

static void
dbus_request_new_cb (guint    request_id,
		     gpointer user_data)
{
	DBusGProxy *proxy;
	GError	   *error = NULL;
	gboolean    set_paused = TRUE;

	if (tracker_status_get () != TRACKER_STATUS_INDEXING) {
		return;
	}

	g_message ("New DBus request, checking indexer is paused...");

	/* First remove the timeout */
	if (indexer_resume_timeout_id != 0) {
		set_paused = FALSE;

		g_source_remove (indexer_resume_timeout_id);
		indexer_resume_timeout_id = 0;
	}

	/* Second reset it so we have another 10 seconds before
	 * continuing.
	 */
	proxy = tracker_dbus_indexer_get_proxy ();
	indexer_resume_timeout_id =
		g_timeout_add_seconds_full (G_PRIORITY_DEFAULT,
					    INDEXER_PAUSE_TIME_FOR_REQUESTS,
					    indexer_resume_cb,
					    g_object_ref (proxy),
					    indexer_resume_destroy_notify_cb);

	/* Third check if we are already paused, if we are there is
	 * no need to tell the indexer.
	 */
	if (tracker_status_get_is_paused_manually ()) {
		g_message ("Tracker is already manually paused, doing nothing");
		return;
	}

	/* We really only do this because of the chance that we tell
	 * the indexer to pause but don't get notified until the next
	 * request. When we are notified of being paused,
	 * tracker_get_is_paused_manually() returns TRUE.
	 */
	if (!set_paused) {
		return;
	}

	/* We use the blocking call here because this function
	 * proceeds a call which needs the database to be available.
	 * Therefore the indexer must reply to tell us it has paused
	 * so we can actually use the database.
	 */
	org_freedesktop_Tracker_Indexer_pause (proxy, &error);

	if (error) {
		g_message ("Couldn't pause the indexer, "
			   "we may have to wait for it to finish");
		g_error_free (error);
	}
}

gboolean
tracker_dbus_init (TrackerConfig *config)
{
	g_return_val_if_fail (TRACKER_IS_CONFIG (config), FALSE);

	/* Don't reinitialize */
	if (objects) {
		return TRUE;
	}

	/* Register names and get proxy/connection details */
	if (!dbus_register_names (config)) {
		return FALSE;
	}

	/* Register request handler so we can pause the indexer */
	tracker_dbus_request_add_hook (dbus_request_new_cb,
				       NULL,
				       NULL);

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

	if (proxy) {
		g_object_unref (proxy);
		proxy = NULL;
	}

	if (proxy_for_indexer) {
		g_object_unref (proxy_for_indexer);
		proxy_for_indexer = NULL;
	}

	connection = NULL;
}

gboolean
tracker_dbus_register_objects (TrackerConfig	*config,
			       TrackerLanguage	*language,
			       TrackerDBIndex	*file_index,
			       TrackerDBIndex	*email_index,
			       TrackerProcessor *processor)
{
	gpointer object;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), FALSE);
	g_return_val_if_fail (TRACKER_IS_LANGUAGE (language), FALSE);
	g_return_val_if_fail (TRACKER_IS_DB_INDEX (file_index), FALSE);
	g_return_val_if_fail (TRACKER_IS_DB_INDEX (email_index), FALSE);

	if (!connection || !proxy) {
		g_critical ("DBus support must be initialized before registering objects!");
		return FALSE;
	}

	/* Add org.freedesktop.Tracker */
	object = tracker_daemon_new (config, processor);
	if (!object) {
		g_critical ("Could not create TrackerDaemon object to register");
		return FALSE;
	}

	dbus_register_object (connection,
			      proxy,
			      G_OBJECT (object),
			      &dbus_glib_tracker_daemon_object_info,
			      TRACKER_DAEMON_PATH);
	objects = g_slist_prepend (objects, object);

	/* Add org.freedesktop.Tracker.Files */
	object = tracker_files_new (processor);
	if (!object) {
		g_critical ("Could not create TrackerFiles object to register");
		return FALSE;
	}

	dbus_register_object (connection,
			      proxy,
			      G_OBJECT (object),
			      &dbus_glib_tracker_files_object_info,
			      TRACKER_FILES_PATH);
	objects = g_slist_prepend (objects, object);

	/* Add org.freedesktop.Tracker.Keywords */
	object = tracker_keywords_new ();
	if (!object) {
		g_critical ("Could not create TrackerKeywords object to register");
		return FALSE;
	}

	dbus_register_object (connection,
			      proxy,
			      G_OBJECT (object),
			      &dbus_glib_tracker_keywords_object_info,
			      TRACKER_KEYWORDS_PATH);
	objects = g_slist_prepend (objects, object);

	/* Add org.freedesktop.Tracker.Metadata */
	object = tracker_metadata_new ();
	if (!object) {
		g_critical ("Could not create TrackerMetadata object to register");
		return FALSE;
	}

	dbus_register_object (connection,
			      proxy,
			      G_OBJECT (object),
			      &dbus_glib_tracker_metadata_object_info,
			      TRACKER_METADATA_PATH);
	objects = g_slist_prepend (objects, object);

	/* Add org.freedesktop.Tracker.Search */
	object = tracker_search_new (config, language, file_index, email_index);
	if (!object) {
		g_critical ("Could not create TrackerSearch object to register");
		return FALSE;
	}

	dbus_register_object (connection,
			      proxy,
			      G_OBJECT (object),
			      &dbus_glib_tracker_search_object_info,
			      TRACKER_SEARCH_PATH);
	objects = g_slist_prepend (objects, object);

	/* Register the XESAM object if enabled */
	if (tracker_config_get_enable_xesam (config)) {
		object = tracker_xesam_new ();
		if (!object) {
			g_critical ("Could not create TrackerXesam object to register");
			return FALSE;
		}

		dbus_register_object (connection,
				      proxy,
				      G_OBJECT (object),
				      &dbus_glib_tracker_xesam_object_info,
				      TRACKER_XESAM_PATH);
		objects = g_slist_prepend (objects, object);

		dbus_g_proxy_add_signal (proxy, "NameOwnerChanged",
					 G_TYPE_STRING, G_TYPE_STRING,
					 G_TYPE_STRING, G_TYPE_INVALID);

		dbus_g_proxy_connect_signal (proxy, "NameOwnerChanged",
					     G_CALLBACK (tracker_xesam_name_owner_changed),
					     g_object_ref (G_OBJECT (object)),
					     dbus_name_owner_changed);
	}

	/* Reverse list since we added objects at the top each time */
	objects = g_slist_reverse (objects);

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

	return NULL;
}

DBusGProxy *
tracker_dbus_indexer_get_proxy (void)
{
	if (!connection) {
		g_critical ("DBus support must be initialized before starting the indexer!");
		return NULL;
	}

	if (!proxy_for_indexer) {
		/* Get proxy for Service / Path / Interface of the indexer */
		proxy_for_indexer = dbus_g_proxy_new_for_name (connection,
							       "org.freedesktop.Tracker.Indexer",
							       "/org/freedesktop/Tracker/Indexer",
							       "org.freedesktop.Tracker.Indexer");

		if (!proxy_for_indexer) {
			g_critical ("Couldn't create a DBusGProxy to the indexer service");
			return NULL;
		}

		/* Add marshallers */
		dbus_g_object_register_marshaller (tracker_marshal_VOID__DOUBLE_STRING_UINT_UINT,
						   G_TYPE_NONE,
						   G_TYPE_DOUBLE,
						   G_TYPE_STRING,
						   G_TYPE_UINT,
						   G_TYPE_UINT,
						   G_TYPE_INVALID);
		dbus_g_object_register_marshaller (tracker_marshal_VOID__DOUBLE_UINT_BOOL,
						   G_TYPE_NONE,
						   G_TYPE_DOUBLE,
						   G_TYPE_UINT,
						   G_TYPE_BOOLEAN,
						   G_TYPE_INVALID);

		/* Add signals, why can't we use introspection for this? */
		dbus_g_proxy_add_signal (proxy_for_indexer,
					 "Status",
					 G_TYPE_DOUBLE,
					 G_TYPE_STRING,
					 G_TYPE_UINT,
					 G_TYPE_UINT,
					 G_TYPE_INVALID);
		dbus_g_proxy_add_signal (proxy_for_indexer,
					 "Started",
					 G_TYPE_INVALID);
		dbus_g_proxy_add_signal (proxy_for_indexer,
					 "Paused",
					 G_TYPE_INVALID);
		dbus_g_proxy_add_signal (proxy_for_indexer,
					 "Continued",
					 G_TYPE_INVALID);
		dbus_g_proxy_add_signal (proxy_for_indexer,
					 "Finished",
					 G_TYPE_DOUBLE,
					 G_TYPE_UINT,
					 G_TYPE_BOOLEAN,
					 G_TYPE_INVALID);
		dbus_g_proxy_add_signal (proxy_for_indexer,
					 "ModuleStarted",
					 G_TYPE_STRING,
					 G_TYPE_INVALID);
		dbus_g_proxy_add_signal (proxy_for_indexer,
					 "ModuleFinished",
					 G_TYPE_STRING,
					 G_TYPE_INVALID);
	}

	return proxy_for_indexer;
}
