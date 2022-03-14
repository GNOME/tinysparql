/*
 * Copyright (C) 2020, Red Hat Ltd.
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
/**
 * SECTION: tracker-batch
 * @short_description: Update batches
 * @title: TrackerBatch
 * @stability: Stable
 * @include: tracker-sparql.h
 *
 * #TrackerBatch is an object containing a series of SPARQL updates,
 * in either SPARQL string or #TrackerResource form. This object has
 * a single use, after the batch is executed, it can only be finished
 * and freed.
 *
 * A batch is created with tracker_sparql_connection_create_batch().
 * To add resources use tracker_batch_add_resource() or
 * tracker_batch_add_sparql().
 *
 * When a batch is ready for execution, use tracker_batch_execute()
 * or tracker_batch_execute_async(). The batch is executed as a single
 * transaction, it will succeed or fail entirely.
 *
 * The mapping of blank node labels is global in a #TrackerBatch,
 * referencing the same blank node label in different operations in
 * a batch will resolve to the same resource.
 *
 * This object was added in Tracker 3.1.
 */
#include "config.h"

#include "tracker-batch.h"
#include "tracker-connection.h"
#include "tracker-private.h"

enum {
	PROP_0,
	PROP_CONNECTION,
	N_PROPS
};

static GParamSpec *props[N_PROPS];

typedef struct {
	TrackerSparqlConnection *connection;
	gchar *sparql;
	guint already_executed : 1;
} TrackerBatchPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (TrackerBatch,
                                     tracker_batch,
                                     G_TYPE_OBJECT)

static void
tracker_batch_init (TrackerBatch *batch)
{
}

static void
tracker_batch_finalize (GObject *object)
{
	TrackerBatch *batch = TRACKER_BATCH (object);
	TrackerBatchPrivate *priv = tracker_batch_get_instance_private (batch);

	g_clear_object (&priv->connection);
	G_OBJECT_CLASS (tracker_batch_parent_class)->finalize (object);
}

static void
tracker_batch_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
	TrackerBatch *batch = TRACKER_BATCH (object);
	TrackerBatchPrivate *priv = tracker_batch_get_instance_private (batch);

	switch (prop_id) {
	case PROP_CONNECTION:
		priv->connection = g_value_dup_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
tracker_batch_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
	TrackerBatch *batch = TRACKER_BATCH (object);
	TrackerBatchPrivate *priv = tracker_batch_get_instance_private (batch);

	switch (prop_id) {
	case PROP_CONNECTION:
		g_value_set_object (value, priv->connection);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
tracker_batch_class_init (TrackerBatchClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_batch_finalize;
	object_class->set_property = tracker_batch_set_property;
	object_class->get_property = tracker_batch_get_property;

	/**
	 * TrackerBatch:connection:
	 *
	 * The #TrackerSparqlConnection the batch belongs to.
	 */
	props[PROP_CONNECTION] =
		g_param_spec_object ("connection",
		                     "connection",
		                     "connection",
		                     TRACKER_TYPE_SPARQL_CONNECTION,
		                     G_PARAM_CONSTRUCT_ONLY |
		                     G_PARAM_STATIC_STRINGS |
		                     G_PARAM_READABLE |
		                     G_PARAM_WRITABLE);

	g_object_class_install_properties (object_class, N_PROPS, props);
}

/**
 * tracker_batch_get_connection:
 * @batch: a #TrackerBatch
 *
 * Returns the #TrackerSparqlConnection that this batch was created from.
 *
 * Returns: (transfer none): The SPARQL connection of this batch.
 **/
TrackerSparqlConnection *
tracker_batch_get_connection (TrackerBatch *batch)
{
	TrackerBatchPrivate *priv = tracker_batch_get_instance_private (batch);

	g_return_val_if_fail (TRACKER_IS_BATCH (batch), NULL);

	return priv->connection;
}

/**
 * tracker_batch_add_sparql:
 * @batch: a #TrackerBatch
 * @sparql: a SPARQL update string
 *
 * Adds an SPARQL update string to @batch.
 *
 * Since: 3.1
 **/
void
tracker_batch_add_sparql (TrackerBatch *batch,
                          const gchar  *sparql)
{
	TrackerBatchPrivate *priv = tracker_batch_get_instance_private (batch);

	g_return_if_fail (TRACKER_IS_BATCH (batch));
	g_return_if_fail (sparql != NULL);
	g_return_if_fail (!priv->already_executed);

	TRACKER_BATCH_GET_CLASS (batch)->add_sparql (batch, sparql);
}

/**
 * tracker_batch_add_resource:
 * @batch: a #TrackerBatch
 * @graph: (nullable): RDF graph to insert the resource to
 * @resource: a #TrackerResource
 *
 * Adds the RDF represented by @resource to @batch.
 *
 * Since: 3.1
 **/
void
tracker_batch_add_resource (TrackerBatch    *batch,
                            const gchar     *graph,
                            TrackerResource *resource)
{
	TrackerBatchPrivate *priv = tracker_batch_get_instance_private (batch);

	g_return_if_fail (TRACKER_IS_BATCH (batch));
	g_return_if_fail (TRACKER_IS_RESOURCE (resource));
	g_return_if_fail (!priv->already_executed);

	TRACKER_BATCH_GET_CLASS (batch)->add_resource (batch, graph, resource);
}

/**
 * tracker_batch_execute:
 * @batch: a #TrackerBatch
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @error: location for a #GError, or %NULL
 *
 * Executes the batch. This operations happens synchronously.
 *
 * Returns: %TRUE of there were no errors, %FALSE otherwise
 *
 * Since: 3.1
 **/
gboolean
tracker_batch_execute (TrackerBatch  *batch,
                       GCancellable  *cancellable,
                       GError       **error)
{
	TrackerBatchPrivate *priv = tracker_batch_get_instance_private (batch);

	g_return_val_if_fail (TRACKER_IS_BATCH (batch), FALSE);
	g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);
	g_return_val_if_fail (!priv->already_executed, FALSE);

	priv->already_executed = TRUE;

	return TRACKER_BATCH_GET_CLASS (batch)->execute (batch, cancellable, error);
}

/**
 * tracker_batch_execute_async:
 * @batch: a #TrackerBatch
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @callback: user-defined #GAsyncReadyCallback to be called when
 *            asynchronous operation is finished.
 * @user_data: user-defined data to be passed to @callback
 *
 * Executes the batch. This operation happens asynchronously, when
 * finished @callback will be executed.
 *
 * Since: 3.1
 **/
void
tracker_batch_execute_async (TrackerBatch        *batch,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
	TrackerBatchPrivate *priv = tracker_batch_get_instance_private (batch);

	g_return_if_fail (TRACKER_IS_BATCH (batch));
	g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (callback != NULL);
	g_return_if_fail (!priv->already_executed);

	priv->already_executed = TRUE;
	TRACKER_BATCH_GET_CLASS (batch)->execute_async (batch, cancellable, callback, user_data);
}

/**
 * tracker_batch_execute_finish:
 * @batch: a #TrackerBatch
 * @res: a #GAsyncResult with the result of the operation
 * @error: location for a #GError, or %NULL
 *
 * Finishes the operation started with tracker_batch_execute_async().
 *
 * Returns: %TRUE of there were no errors, %FALSE otherwise
 *
 * Since: 3.1
 **/
gboolean
tracker_batch_execute_finish (TrackerBatch  *batch,
                              GAsyncResult  *res,
                              GError       **error)
{
	g_return_val_if_fail (TRACKER_IS_BATCH (batch), FALSE);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (res), FALSE);
	g_return_val_if_fail (!error || !*error, FALSE);

	return TRACKER_BATCH_GET_CLASS (batch)->execute_finish (batch, res, error);
}
