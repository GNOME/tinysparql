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

#include <gobject/gvaluecollector.h>

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
 * tracker_batch_add_statement: (skip):
 * @batch: a #TrackerBatch
 * @stmt: a #TrackerSparqlStatement containing a SPARQL update
 * @...: parameters bound to @stmt, in triples of name/type/value
 *
 * Adds a #TrackerSparqlStatement containing an SPARQL update. The statement will
 * be executed once in the batch, with the parameters bound as specified in the
 * variable arguments.
 *
 * The variable arguments are a NULL terminated set of variable name, value GType,
 * and actual value. For example, for a statement that has a single `~name` parameter,
 * it could be given a value for execution with the given code:
 *
 * ```c
 * tracker_batch_add_statement (batch, stmt,
 *                              "name", G_TYPE_STRING, "John Smith",
 *                              NULL);
 * ```
 *
 * The #TrackerSparqlStatement may be used on multiple tracker_batch_add_statement()
 * calls with the same or different values, on the same or different #TrackerBatch
 * objects.
 *
 * This function should only be called on #TrackerSparqlStatement objects
 * obtained through tracker_sparql_connection_update_statement() or
 * update statements loaded through tracker_sparql_connection_load_statement_from_gresource().
 *
 * Since: 3.5
 **/
void
tracker_batch_add_statement (TrackerBatch           *batch,
                             TrackerSparqlStatement *stmt,
                             ...)
{
	GPtrArray *variable_names;
	GArray *values;
	va_list varargs;
	const gchar *var_name;

	variable_names = g_ptr_array_new ();
	g_ptr_array_set_free_func (variable_names, g_free);

	values = g_array_new (FALSE, TRUE, sizeof (GValue));
	g_array_set_clear_func (values, (GDestroyNotify) g_value_unset);

	va_start (varargs, stmt);

	var_name = va_arg (varargs, const gchar*);

	while (var_name) {
		GType var_type;
		GValue var_value = G_VALUE_INIT;
		gchar *error = NULL;

		var_type = va_arg (varargs, GType);

		G_VALUE_COLLECT_INIT (&var_value, var_type, varargs, 0, &error);

		if (error) {
			g_warning ("%s: %s", G_STRFUNC, error);
			g_free (error);
			goto error;
		}

		g_ptr_array_add (variable_names, g_strdup (var_name));
		g_array_append_val (values, var_value);

		var_name = va_arg (varargs, const gchar *);
	}

	tracker_batch_add_statementv (batch, stmt,
	                              variable_names->len,
	                              (const gchar **) variable_names->pdata,
	                              (const GValue *) values->data);
 error:
	va_end (varargs);
	g_ptr_array_unref (variable_names);
	g_array_unref (values);
}

/**
 * tracker_batch_add_statementv: (rename-to tracker_batch_add_statement)
 * @batch: a #TrackerBatch
 * @stmt: a #TrackerSparqlStatement containing a SPARQL update
 * @n_values: the number of bound parameters
 * @variable_names: (array length=n_values): the names of each bound parameter
 * @values: (array length=n_values): the values of each bound parameter
 *
 * Adds a #TrackerSparqlStatement containing an SPARQL update. The statement will
 * be executed once in the batch, with the values bound as specified by @variable_names
 * and @values.
 *
 * For example, for a statement that has a single `~name` parameter,
 * it could be given a value for execution with the given code:
 *
 * <div class="gi-lang-c"><pre><code class="language-c">
 *
 * ```
 * const char *names = { "name" };
 * const GValue values[G_N_ELEMENTS (names)] = { 0, };
 *
 * g_value_init (&values[0], G_TYPE_STRING);
 * g_value_set_string (&values[0], "John Smith");
 * tracker_batch_add_statementv (batch, stmt,
 *                               G_N_ELEMENTS (names),
 *                               names, values);
 * ```
 * </code></pre></div>
 *
 * <div class="gi-lang-python"><pre><code class="language-python">
 *
 * ```
 * batch.add_statement(stmt, ['name'], ['John Smith']);
 * ```
 * </code></pre></div>
 *
 * <div class="gi-lang-javascript"><pre><code class="language-javascript">
 *
 * ```
 * batch.add_statement(stmt, ['name'], ['John Smith']);
 * ```
 * </code></pre></div>
 *
 * The #TrackerSparqlStatement may be used on multiple tracker_batch_add_statement()
 * calls with the same or different values, on the same or different #TrackerBatch
 * objects.
 *
 * This function should only be called on #TrackerSparqlStatement objects
 * obtained through tracker_sparql_connection_update_statement() or
 * update statements loaded through tracker_sparql_connection_load_statement_from_gresource().
 *
 * Since: 3.5
 **/
void
tracker_batch_add_statementv (TrackerBatch           *batch,
                              TrackerSparqlStatement *stmt,
                              guint                   n_values,
                              const gchar            *variable_names[],
                              const GValue            values[])
{
	TrackerBatchPrivate *priv = tracker_batch_get_instance_private (batch);

	g_return_if_fail (TRACKER_IS_BATCH (batch));
	g_return_if_fail (TRACKER_IS_SPARQL_STATEMENT (stmt));
	g_return_if_fail (!priv->already_executed);

	TRACKER_BATCH_GET_CLASS (batch)->add_statement (batch, stmt,
	                                                n_values,
	                                                variable_names, values);
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
