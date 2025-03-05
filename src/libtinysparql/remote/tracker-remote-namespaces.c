/*
 * Copyright (C) 2021, Red Hat Inc.
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
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#include "config.h"

#include "remote/tracker-remote-namespaces.h"

#include "tracker-deserializer.h"
#include "tracker-private.h"

struct _TrackerRemoteNamespaceManager {
	TrackerNamespaceManager parent_instance;
	TrackerSparqlConnection *conn;
};

typedef struct {
	TrackerNamespaceManager *manager;
	GMainLoop *main_loop;
	GError *error;
} SerializeSyncData;

enum {
	PROP_0,
	PROP_CONNECTION,
	N_PROPS
};

static GParamSpec *props[N_PROPS];

static void
tracker_remote_namespace_manager_async_initable_iface_init (GAsyncInitableIface *iface);
static void
tracker_remote_namespace_manager_initable_iface_init (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (TrackerRemoteNamespaceManager,
                         tracker_remote_namespace_manager,
                         TRACKER_TYPE_NAMESPACE_MANAGER,
			 G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
						tracker_remote_namespace_manager_initable_iface_init)
                         G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE,
                                                tracker_remote_namespace_manager_async_initable_iface_init))

static gboolean
initialize_namespaces_from_rdf (TrackerNamespaceManager  *manager,
                                GInputStream             *istream,
                                GError                  **error)
{
	TrackerSparqlCursor *deserializer;
	GError *inner_error = NULL;

	deserializer = tracker_deserializer_new (istream, manager, TRACKER_SERIALIZER_FORMAT_TTL);

	/* We don't care much about the content, just to have the
	 * prefixes parsed.
	 */
	if (tracker_sparql_cursor_next (deserializer, NULL, &inner_error))
		g_warning ("The remote endpoint replied with unexpected data");
	g_object_unref (deserializer);

	if (inner_error) {
		g_propagate_error (error, inner_error);
		return FALSE;
	}

	return TRUE;
}

static void
serialize_cb (GObject      *source,
              GAsyncResult *res,
              gpointer      user_data)
{
	GTask *task = user_data;
	TrackerNamespaceManager *manager =
		TRACKER_NAMESPACE_MANAGER (g_task_get_source_object (task));
	TrackerRemoteNamespaceManager *remote_manager =
		TRACKER_REMOTE_NAMESPACE_MANAGER (manager);
	GInputStream *istream;
	GError *error = NULL;

	istream = tracker_sparql_connection_serialize_finish (remote_manager->conn,
	                                                      res,
	                                                      &error);
	if (istream)
		initialize_namespaces_from_rdf (manager, istream, &error);

	if (error)
		g_task_return_error (task, error);
	else
		g_task_return_boolean (task, TRUE);

	g_object_unref (task);
}

static void
tracker_remote_namespace_manager_init_async (GAsyncInitable      *initable,
                                             int                  io_priority,
                                             GCancellable        *cancellable,
                                             GAsyncReadyCallback  callback,
                                             gpointer             user_data)
{
	TrackerRemoteNamespaceManager *manager =
		TRACKER_REMOTE_NAMESPACE_MANAGER (initable);
	GTask *task;

	task = g_task_new (manager, NULL, callback, user_data);

	/* Make a query for an empty URI, we only want the namespaces */
	tracker_sparql_connection_serialize_async (manager->conn,
	                                           TRACKER_SERIALIZE_FLAGS_NONE,
	                                           TRACKER_RDF_FORMAT_TURTLE,
	                                           "DESCRIBE <>",
	                                           NULL,
	                                           serialize_cb,
	                                           task);
}

static gboolean
tracker_remote_namespace_manager_init_finish (GAsyncInitable  *initable,
                                              GAsyncResult    *res,
                                              GError         **error)
{
	return g_task_propagate_boolean (G_TASK (res), error);
}

static void
serialize_sync_cb (GObject      *source,
		   GAsyncResult *res,
		   gpointer      user_data)
{
	SerializeSyncData *data = user_data;

	g_async_initable_init_finish (G_ASYNC_INITABLE (source), res, &data->error);
	g_main_loop_quit (data->main_loop);
}

static gboolean
tracker_remote_namespace_manager_initable_init (GInitable     *initable,
						GCancellable  *cancellable,
						GError       **error)
{
	TrackerRemoteNamespaceManager *manager =
		TRACKER_REMOTE_NAMESPACE_MANAGER (initable);
	SerializeSyncData data = { 0 };

	data.manager = TRACKER_NAMESPACE_MANAGER (manager);
	data.main_loop = g_main_loop_new (g_main_context_get_thread_default (), TRUE);
	g_async_initable_init_async (G_ASYNC_INITABLE (initable),
				     G_PRIORITY_DEFAULT,
				     cancellable,
				     serialize_sync_cb,
				     &data);

	g_main_loop_run (data.main_loop);
	g_main_loop_unref (data.main_loop);

	if (data.error) {
		g_propagate_error (error, data.error);
		return FALSE;
	}

	return TRUE;
}

static void
tracker_remote_namespace_manager_async_initable_iface_init (GAsyncInitableIface *iface)
{
	iface->init_async = tracker_remote_namespace_manager_init_async;
	iface->init_finish = tracker_remote_namespace_manager_init_finish;
}

static void
tracker_remote_namespace_manager_initable_iface_init (GInitableIface *iface)
{
	iface->init = tracker_remote_namespace_manager_initable_init;
}

static void
tracker_remote_namespace_manager_set_property (GObject      *object,
                                               guint         prop_id,
                                               const GValue *value,
                                               GParamSpec   *pspec)
{
	TrackerRemoteNamespaceManager *manager =
		TRACKER_REMOTE_NAMESPACE_MANAGER (object);

	switch (prop_id) {
	case PROP_CONNECTION:
		manager->conn = g_value_get_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
tracker_remote_namespace_manager_class_init (TrackerRemoteNamespaceManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = tracker_remote_namespace_manager_set_property;

	props[PROP_CONNECTION] =
		g_param_spec_object ("connection",
		                     "Connection",
		                     "Connection",
		                     TRACKER_TYPE_SPARQL_CONNECTION,
		                     G_PARAM_WRITABLE |
		                     G_PARAM_CONSTRUCT_ONLY);

	g_object_class_install_properties (object_class, N_PROPS, props);

}

static void
tracker_remote_namespace_manager_init (TrackerRemoteNamespaceManager *manager)
{
}

TrackerNamespaceManager *
tracker_remote_namespace_manager_new (TrackerSparqlConnection *conn)
{
	return g_initable_new (TRACKER_TYPE_REMOTE_NAMESPACE_MANAGER,
			       NULL, NULL,
			       "connection", conn,
			       NULL);
}
