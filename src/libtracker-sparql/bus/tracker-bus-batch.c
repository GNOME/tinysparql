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

#include "tracker-bus-batch.h"

struct _TrackerBusBatch
{
	TrackerBatch parent_instance;
	GPtrArray *updates;
};

typedef struct {
	GMainLoop *loop;
	gboolean retval;
	GError *error;
} ExecuteAsyncData;

G_DEFINE_TYPE (TrackerBusBatch, tracker_bus_batch, TRACKER_TYPE_BATCH)

static void tracker_bus_batch_execute_async (TrackerBatch        *batch,
                                             GCancellable        *cancellable,
                                             GAsyncReadyCallback  callback,
                                             gpointer             user_data);

static void
tracker_bus_batch_finalize (GObject *object)
{
	TrackerBusBatch *bus_batch = TRACKER_BUS_BATCH (object);

	g_ptr_array_unref (bus_batch->updates);

	G_OBJECT_CLASS (tracker_bus_batch_parent_class)->finalize (object);
}

static void
tracker_bus_batch_add_sparql (TrackerBatch *batch,
                              const gchar  *sparql)
{
	TrackerBusBatch *bus_batch = TRACKER_BUS_BATCH (batch);

	g_ptr_array_add (bus_batch->updates, g_strdup (sparql));
}

static void
tracker_bus_batch_add_resource (TrackerBatch    *batch,
                                const gchar     *graph,
                                TrackerResource *resource)
{
	TrackerBusBatch *bus_batch = TRACKER_BUS_BATCH (batch);
	TrackerSparqlConnection *conn;
	TrackerNamespaceManager *namespaces;
	gchar *sparql;

	conn = tracker_batch_get_connection (batch);
	namespaces = tracker_sparql_connection_get_namespace_manager (conn);
	sparql = tracker_resource_print_sparql_update (resource, namespaces, graph);
	g_ptr_array_add (bus_batch->updates, sparql);
}

static void
execute_cb (GObject      *source,
            GAsyncResult *res,
            gpointer      user_data)
{
	ExecuteAsyncData *data = user_data;

	data->retval =
		tracker_batch_execute_finish (TRACKER_BATCH (source),
		                              res, &data->error);
	g_main_loop_quit (data->loop);
}

static gboolean
tracker_bus_batch_execute (TrackerBatch  *batch,
                           GCancellable  *cancellable,
                           GError       **error)
{
	GMainContext *context;
	ExecuteAsyncData data = { 0, };

	context = g_main_context_new ();
	data.loop = g_main_loop_new (context, FALSE);
	g_main_context_push_thread_default (context);

	tracker_bus_batch_execute_async (batch,
	                                 cancellable,
	                                 execute_cb,
	                                 &data);

	g_main_loop_run (data.loop);

	g_main_context_pop_thread_default (context);

	g_main_loop_unref (data.loop);

	if (data.error) {
		g_propagate_error (error, data.error);
		return FALSE;
	}

	return data.retval;
}

static void
update_array_cb (GObject      *source,
                 GAsyncResult *res,
                 gpointer      user_data)
{
	GTask *task = user_data;
	GError *error = NULL;

	if (!tracker_sparql_connection_update_array_finish (TRACKER_SPARQL_CONNECTION (source),
	                                                    res, &error))
		g_task_return_error (task, error);
	else
		g_task_return_boolean (task, TRUE);

	g_object_unref (task);
}

static void
tracker_bus_batch_execute_async (TrackerBatch        *batch,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
	TrackerBusBatch *bus_batch = TRACKER_BUS_BATCH (batch);
	TrackerSparqlConnection *conn;
	GTask *task;

	task = g_task_new (batch, cancellable, callback, user_data);
	conn = tracker_batch_get_connection (batch);
	tracker_sparql_connection_update_array_async (conn,
	                                              (gchar **) bus_batch->updates->pdata,
	                                              bus_batch->updates->len,
	                                              cancellable,
	                                              update_array_cb,
	                                              task);
}

static gboolean
tracker_bus_batch_execute_finish (TrackerBatch  *batch,
                                  GAsyncResult  *res,
                                  GError       **error)
{
	return g_task_propagate_boolean (G_TASK (res), error);
}

static void
tracker_bus_batch_class_init (TrackerBusBatchClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	TrackerBatchClass *batch_class = TRACKER_BATCH_CLASS (klass);

	object_class->finalize = tracker_bus_batch_finalize;

	batch_class->add_sparql = tracker_bus_batch_add_sparql;
	batch_class->add_resource = tracker_bus_batch_add_resource;
	batch_class->execute = tracker_bus_batch_execute;
	batch_class->execute_async = tracker_bus_batch_execute_async;
	batch_class->execute_finish = tracker_bus_batch_execute_finish;
}

static void
tracker_bus_batch_init (TrackerBusBatch *batch)
{
	batch->updates = g_ptr_array_new ();
	g_ptr_array_set_free_func (batch->updates, g_free);
}

TrackerBatch *
tracker_bus_batch_new (TrackerBusConnection *connection)
{
	return g_object_new (TRACKER_TYPE_BUS_BATCH,
	                     "connection", connection,
	                     NULL);
}
