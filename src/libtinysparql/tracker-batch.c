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
 * TrackerBatch:
 *
 * `TrackerBatch` executes a series of SPARQL updates and RDF data
 * insertions within a transaction.
 *
 * A batch is created with [method@SparqlConnection.create_batch].
 * To add resources use [method@Batch.add_resource],
 * [method@Batch.add_sparql] or [method@Batch.add_statement].
 *
 * When a batch is ready for execution, use [method@Batch.execute]
 * or [method@Batch.execute_async]. The batch is executed as a single
 * transaction, it will succeed or fail entirely.
 *
 * This object has a single use, after the batch is executed it can
 * only be finished and freed.
 *
 * The mapping of blank node labels is global in a `TrackerBatch`,
 * referencing the same blank node label in different operations in
 * a batch will resolve to the same resource.
 *
 * Since: 3.1
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
	 * The [class@SparqlConnection] the batch belongs to.
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

void
tracker_batch_add_dbus_fd (TrackerBatch *batch,
                           GInputStream *istream)
{
	TrackerBatchPrivate *priv = tracker_batch_get_instance_private (batch);

	g_return_if_fail (TRACKER_IS_BATCH (batch));
	g_return_if_fail (G_IS_INPUT_STREAM (istream));
	g_return_if_fail (!priv->already_executed);

	TRACKER_BATCH_GET_CLASS (batch)->add_dbus_fd (batch, istream);
}

/**
 * tracker_batch_get_connection:
 * @batch: A `TrackerBatch`
 *
 * Returns the [class@SparqlConnection] that this batch was created
 * from.
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
 * @batch: A `TrackerBatch`
 * @sparql: A SPARQL update string
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
 * @batch: A `TrackerBatch`
 * @graph: (nullable): RDF graph to insert the resource to
 * @resource: A [class@Resource]
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
 * @batch: a `TrackerBatch`
 * @stmt: a [class@SparqlStatement] containing a SPARQL update
 * @...: NULL-terminated list of parameters bound to @stmt, in triplets of name, type and value.
 *
 * Adds a [class@SparqlStatement] containing an SPARQL update. The statement will
 * be executed once in the batch, with the parameters bound as specified in the
 * variable arguments.
 *
 * The variable arguments are a NULL terminated set of variable name, type [type@GObject.Type],
 * and value. The value C type must correspond to the given [type@GObject.Type]. For example, for
 * a statement that has a single `~name` parameter, it could be given a value for execution
 * with the following code:
 *
 * ```c
 * tracker_batch_add_statement (batch, stmt,
 *                              "name", G_TYPE_STRING, "John Smith",
 *                              NULL);
 * ```
 *
 * A [class@SparqlStatement] may be used on multiple [method@Batch.add_statement]
 * calls with the same or different values, on the same or different `TrackerBatch`
 * objects.
 *
 * This function should only be called on [class@SparqlStatement] objects
 * obtained through [method@SparqlConnection.update_statement] or
 * update statements loaded through [method@SparqlConnection.load_statement_from_gresource].
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
 * @batch: A `TrackerBatch`
 * @stmt: A [class@SparqlStatement] containing a SPARQL update
 * @n_values: The number of bound parameters
 * @variable_names: (array length=n_values): The names of each bound parameter
 * @values: (array length=n_values): The values of each bound parameter
 *
 * Adds a [class@SparqlStatement] containing an SPARQL update. The statement will
 * be executed once in the batch, with the values bound as specified by @variable_names
 * and @values.
 *
 * For example, for a statement that has a single `~name` parameter,
 * it could be given a value for execution with the given code:
 *
 * ```c
 * const char *names = { "name" };
 * const GValue values[G_N_ELEMENTS (names)] = { 0, };
 *
 * g_value_init (&values[0], G_TYPE_STRING);
 * g_value_set_string (&values[0], "John Smith");
 * tracker_batch_add_statementv (batch, stmt,
 *                               G_N_ELEMENTS (names),
 *                               names, values);
 * ```
 * ```python
 * batch.add_statement(stmt, ['name'], ['John Smith']);
 * ```
 * ```js
 * batch.add_statement(stmt, ['name'], ['John Smith']);
 * ```
 *
 * A [class@SparqlStatement] may be used on multiple [method@Batch.add_statement]
 * calls with the same or different values, on the same or different `TrackerBatch`
 * objects.
 *
 * This function should only be called on [class@SparqlStatement] objects
 * obtained through [method@SparqlConnection.update_statement] or
 * update statements loaded through [method@SparqlConnection.load_statement_from_gresource].
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
 * tracker_batch_add_rdf:
 * @batch: A `TrackerBatch`
 * @flags: Deserialization flags
 * @format: RDF format of data in stream
 * @default_graph: Default graph that will receive the RDF data
 * @stream: Input stream with RDF data
 *
 * Inserts the RDF data contained in @stream as part of @batch.
 *
 * The RDF data will be inserted in the given @default_graph if one is provided,
 * or the anonymous graph if @default_graph is %NULL. Any RDF data that has a
 * graph specified (e.g. using the `GRAPH` clause in the Trig format) will
 * be inserted in the specified graph instead of @default_graph.
 *
 * The @flags argument is reserved for future expansions, currently
 * %TRACKER_DESERIALIZE_FLAGS_NONE must be passed.
 *
 * Since: 3.6
 **/
void
tracker_batch_add_rdf (TrackerBatch            *batch,
                       TrackerDeserializeFlags  flags,
                       TrackerRdfFormat         format,
                       const gchar             *default_graph,
                       GInputStream            *stream)
{
	TrackerBatchPrivate *priv = tracker_batch_get_instance_private (batch);

	g_return_if_fail (TRACKER_IS_BATCH (batch));
	g_return_if_fail (G_IS_INPUT_STREAM (stream));
	g_return_if_fail (!priv->already_executed);

	TRACKER_BATCH_GET_CLASS (batch)->add_rdf (batch,
	                                          flags,
	                                          format,
	                                          default_graph,
	                                          stream);
}

/**
 * tracker_batch_execute:
 * @batch: a `TrackerBatch`
 * @cancellable: (nullable): Optional [type@Gio.Cancellable]
 * @error: Error location
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

	if (tracker_sparql_connection_set_error_on_closed (priv->connection, error))
		return FALSE;

	return TRACKER_BATCH_GET_CLASS (batch)->execute (batch, cancellable, error);
}

/**
 * tracker_batch_execute_async:
 * @batch: A `TrackerBatch`
 * @cancellable: (nullable): Optional [type@Gio.Cancellable]
 * @callback: User-defined [type@Gio.AsyncReadyCallback] to be called when
 *            the asynchronous operation is finished.
 * @user_data: User-defined data to be passed to @callback
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

	if (tracker_sparql_connection_report_async_error_on_closed (priv->connection,
	                                                            callback,
	                                                            user_data))
		return;

	TRACKER_BATCH_GET_CLASS (batch)->execute_async (batch, cancellable, callback, user_data);
}

/**
 * tracker_batch_execute_finish:
 * @batch: A `TrackerBatch`
 * @res: A [type@Gio.AsyncResult] with the result of the operation
 * @error: Error location
 *
 * Finishes the operation started with [method@Batch.execute_async].
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
