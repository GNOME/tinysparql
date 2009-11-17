/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
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

#include <libtracker-common/tracker-dbus.h>
#include <libtracker-client/tracker.h>

#include "tracker-writeback-dispatcher.h"
#include "tracker-writeback-dbus.h"
#include "tracker-writeback-glue.h"
#include "tracker-writeback-module.h"
#include "tracker-marshal.h"

typedef struct {
	DBusGConnection *connection;
	DBusGProxy *gproxy, *tproxy;
} DBusData;

typedef struct {
	GHashTable *modules;
	DBusData *dbus_data;
	TrackerClient *client;
} TrackerWritebackDispatcherPrivate;

typedef struct {
	TrackerWritebackDispatcher *dispatcher;
	GStrv rdf_types;
} QueryData;

#define TRACKER_WRITEBACK_DISPATCHER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_WRITEBACK_DISPATCHER, TrackerWritebackDispatcherPrivate))

#define TRACKER_SERVICE			"org.freedesktop.Tracker1"
#define TRACKER_RESOURCES_OBJECT	"/org/freedesktop/Tracker1/Resources"
#define TRACKER_INTERFACE_RESOURCES	"org.freedesktop.Tracker1.Resources"

static void tracker_writeback_dispatcher_finalize    (GObject                    *object);
static void tracker_writeback_dispatcher_constructed (GObject                    *object);
static void on_writeback_cb                          (DBusGProxy                 *proxy,
                                                      GHashTable                 *subjects,
                                                      TrackerWritebackDispatcher *object);


G_DEFINE_TYPE (TrackerWritebackDispatcher, tracker_writeback_dispatcher, G_TYPE_OBJECT)


static void
tracker_writeback_dispatcher_class_init (TrackerWritebackDispatcherClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_writeback_dispatcher_finalize;
	object_class->constructed = tracker_writeback_dispatcher_constructed;

	g_type_class_add_private (object_class, sizeof (TrackerWritebackDispatcherPrivate));
}

static gboolean
dbus_register_service (DBusGProxy  *proxy,
                       const gchar *name)
{
	GError *error = NULL;
	guint	result;

	g_message ("Registering D-Bus service '%s'...", name);

	if (!org_freedesktop_DBus_request_name (proxy,
	                                        name,
	                                        DBUS_NAME_FLAG_DO_NOT_QUEUE,
	                                        &result, &error)) {
		g_critical ("Could not acquire name:'%s', %s",
		            name,
		            error ? error->message : "no error given");
		g_error_free (error);

		return FALSE;
	}

	if (result != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
		g_critical ("D-Bus service name:'%s' is already taken, "
		            "perhaps the application is already running?",
		            name);
		return FALSE;
	}

	return TRUE;
}

static gboolean
dbus_register_object (GObject		    *object,
                      DBusGConnection	    *connection,
                      DBusGProxy	    *proxy,
                      const DBusGObjectInfo *info,
                      const gchar	    *path)
{
	g_message ("Registering D-Bus object...");
	g_message ("  Path:'%s'", path);
	g_message ("  Object Type:'%s'", G_OBJECT_TYPE_NAME (object));

	dbus_g_object_type_install_info (G_OBJECT_TYPE (object), info);
	dbus_g_connection_register_g_object (connection, path, object);

	return TRUE;
}

static DBusData *
dbus_data_create (GObject *object)
{
	DBusData *data;
	DBusGConnection *connection;
	DBusGProxy *gproxy;
	GError *error = NULL;

	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

	if (!connection) {
		g_critical ("Could not connect to the D-Bus session bus, %s",
			    error ? error->message : "no error given.");
		g_error_free (error);
		return NULL;
	}

	gproxy = dbus_g_proxy_new_for_name (connection,
	                                    DBUS_SERVICE_DBUS,
	                                    DBUS_PATH_DBUS,
	                                    DBUS_INTERFACE_DBUS);

	if (!dbus_register_service (gproxy,
	                            TRACKER_WRITEBACK_DBUS_NAME)) {
		g_object_unref (gproxy);
		return NULL;
	}

	if (!dbus_register_object (object,
	                           connection, gproxy,
	                           &dbus_glib_tracker_writeback_object_info,
	                           TRACKER_WRITEBACK_DBUS_PATH)) {
		g_object_unref (gproxy);
		return NULL;
	}

	dbus_g_object_register_marshaller (tracker_marshal_VOID__BOXED,
	                                   G_TYPE_NONE,
	                                   TRACKER_TYPE_STR_STRV_MAP,
	                                   G_TYPE_INVALID);

	/* Now we're successfully connected and registered, create the data */
	data = g_new0 (DBusData, 1);
	data->connection = dbus_g_connection_ref (connection);
	data->gproxy = gproxy;

	return data;
}

static void
dbus_data_free (DBusData *data)
{
	dbus_g_connection_unref (data->connection);

	if (data->gproxy)
		g_object_unref (data->gproxy);

	if (data->tproxy)
		g_object_unref (data->tproxy);

	g_free (data);
}

static void
tracker_writeback_dispatcher_init (TrackerWritebackDispatcher *dispatcher)
{
	TrackerWritebackDispatcherPrivate *priv;

	priv = TRACKER_WRITEBACK_DISPATCHER_GET_PRIVATE (dispatcher);

	priv->client = tracker_connect (TRUE, 0);
	priv->dbus_data = dbus_data_create (G_OBJECT (dispatcher));
	priv->modules = g_hash_table_new_full (g_str_hash,
	                                       g_str_equal,
	                                       (GDestroyNotify) g_free,
	                                       NULL);

	priv->dbus_data->tproxy = dbus_g_proxy_new_for_name (priv->dbus_data->connection,
	                                                     TRACKER_SERVICE,
	                                                     TRACKER_RESOURCES_OBJECT,
	                                                     TRACKER_INTERFACE_RESOURCES);

	dbus_g_proxy_add_signal (priv->dbus_data->tproxy, "Writeback",
	                         TRACKER_TYPE_STR_STRV_MAP,
	                         G_TYPE_INVALID);

	dbus_g_proxy_connect_signal (priv->dbus_data->tproxy, "Writeback",
	                             G_CALLBACK (on_writeback_cb),
	                             dispatcher,
	                             NULL);
}

static void
tracker_writeback_dispatcher_finalize (GObject *object)
{
	TrackerWritebackDispatcherPrivate *priv;

	priv = TRACKER_WRITEBACK_DISPATCHER_GET_PRIVATE (object);

	if (priv->client) {
		tracker_disconnect (priv->client);
	}

	dbus_data_free (priv->dbus_data);

	G_OBJECT_CLASS (tracker_writeback_dispatcher_parent_class)->finalize (object);
}

static void
tracker_writeback_dispatcher_constructed (GObject *object)
{
	GList *modules;
	TrackerWritebackDispatcherPrivate *priv;

	priv = TRACKER_WRITEBACK_DISPATCHER_GET_PRIVATE (object);
	modules = tracker_writeback_modules_list ();

	while (modules) {
		TrackerWritebackModule *module;
		const gchar *path;

		path = modules->data;
		module = tracker_writeback_module_get (path);

		g_hash_table_insert (priv->modules, g_strdup (path), module);

		modules = modules->next;
	}
}

TrackerWritebackDispatcher *
tracker_writeback_dispatcher_new ()
{
	return g_object_new (TRACKER_TYPE_WRITEBACK_DISPATCHER, NULL);
}

static gboolean
types_match (const gchar **module_types, gchar **rdf_types)
{
	guint n;

	for (n = 0; rdf_types[n] != NULL; n++) {
		guint i;

		for (i = 0; module_types[i] != NULL; i++) {
			if (g_strcmp0 (module_types[i], rdf_types[n]) == 0) {
				return TRUE;
			}
		}
	}

	return FALSE;
}

static void
on_sparql_result_received (GPtrArray *result,
                           GError    *error,
                           gpointer   user_data)
{
	TrackerWritebackDispatcherPrivate *priv;
	TrackerWritebackModule *module;
	GHashTableIter iter;
	gpointer key, value;
	QueryData *data;
	const gchar **module_types;
	gchar **rdf_types;

	data = user_data;
	rdf_types = data->rdf_types;
	priv = TRACKER_WRITEBACK_DISPATCHER_GET_PRIVATE (data->dispatcher);

	g_hash_table_iter_init (&iter, priv->modules);

	while (g_hash_table_iter_next (&iter, &key, &value)) {
		module = value;

		module_types = tracker_writeback_module_get_rdftypes (module);

		if (types_match (module_types, rdf_types)) {
			TrackerWriteback *writeback;

			writeback = tracker_writeback_module_create (module);
			tracker_writeback_update_metadata (writeback, result);
			g_object_unref (writeback);
		}

	}

	g_strfreev (data->rdf_types);
	g_slice_free (QueryData, data);
}

static void
on_writeback_cb (DBusGProxy                 *proxy,
                 GHashTable                 *subjects,
                 TrackerWritebackDispatcher *object)
{
	TrackerWritebackDispatcherPrivate *priv;
	QueryData *data;
	GHashTableIter iter;
	gpointer key, value;

	priv = TRACKER_WRITEBACK_DISPATCHER_GET_PRIVATE (object);

	g_hash_table_iter_init (&iter, subjects);

	while (g_hash_table_iter_next (&iter, &key, &value)) {
		const gchar *subject = key;
		const GStrv rdf_types = value;
		gchar *query;

		query = g_strdup_printf ("SELECT ?url ?predicate ?object { "
		                                "<%s> ?predicate ?object ; "
		                                     "nie:isStoredAs ?url . "
		                                "?predicate tracker:writeback true "
		                         "}", subject);

		data = g_slice_new (QueryData);
		data->dispatcher = object;
		data->rdf_types = g_strdupv (rdf_types);

		tracker_resources_sparql_query_async (priv->client,
		                                      query,
		                                      on_sparql_result_received,
		                                      data);

		g_free (query);
	}
}
