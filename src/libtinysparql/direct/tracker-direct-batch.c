/*
 * Copyright (C) 2020, Red Hat, Inc.
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

#include "core/tracker-data.h"

#include "direct/tracker-direct-batch.h"
#include "direct/tracker-direct-statement.h"
#include "direct/tracker-direct.h"

#include "tracker-private.h"

typedef struct _TrackerDirectBatchPrivate TrackerDirectBatchPrivate;
typedef struct _TrackerBatchElem TrackerBatchElem;

struct _TrackerBatchElem
{
	guint type;

	union {
		gchar *sparql;

		struct {
			gchar *graph;
			TrackerResource *resource;
		} resource;

		struct {
			TrackerSparqlStatement *stmt;
			GHashTable *parameters;
		} statement;

		struct {
			TrackerDeserializeFlags flags;
			TrackerRdfFormat format;
			gchar *default_graph;
			GInputStream *stream;
		} rdf;

		GInputStream *dbus_fd;
	} d;
};

struct _TrackerDirectBatchPrivate
{
	GArray *array;
};

enum {
	TRACKER_DIRECT_BATCH_RESOURCE,
	TRACKER_DIRECT_BATCH_SPARQL,
	TRACKER_DIRECT_BATCH_STATEMENT,
	TRACKER_DIRECT_BATCH_RDF,
	TRACKER_DIRECT_BATCH_DBUS_FD,
};

G_DEFINE_TYPE_WITH_PRIVATE (TrackerDirectBatch,
                            tracker_direct_batch,
                            TRACKER_TYPE_BATCH)

static TrackerSerializerFormat
convert_format (TrackerRdfFormat format)
{
	switch (format) {
	case TRACKER_RDF_FORMAT_TURTLE:
		return TRACKER_SERIALIZER_FORMAT_TTL;
	case TRACKER_RDF_FORMAT_TRIG:
		return TRACKER_SERIALIZER_FORMAT_TRIG;
	case TRACKER_RDF_FORMAT_JSON_LD:
		return TRACKER_SERIALIZER_FORMAT_JSON_LD;
	default:
		g_assert_not_reached ();
	}
}

static void
tracker_direct_batch_finalize (GObject *object)
{
	TrackerDirectBatchPrivate *priv;

	priv = tracker_direct_batch_get_instance_private (TRACKER_DIRECT_BATCH (object));
	g_array_unref (priv->array);

	G_OBJECT_CLASS (tracker_direct_batch_parent_class)->finalize (object);
}

static void
tracker_direct_batch_add_sparql (TrackerBatch *batch,
                                 const gchar  *sparql)
{
	TrackerDirectBatch *direct = TRACKER_DIRECT_BATCH (batch);
	TrackerDirectBatchPrivate *priv = tracker_direct_batch_get_instance_private (direct);
	TrackerBatchElem elem;

	elem.type = TRACKER_DIRECT_BATCH_SPARQL;
	elem.d.sparql = g_strdup (sparql);
	g_array_append_val (priv->array, elem);
}

static void
tracker_direct_batch_add_resource (TrackerBatch    *batch,
                                   const gchar     *graph,
                                   TrackerResource *resource)
{
	TrackerDirectBatch *direct = TRACKER_DIRECT_BATCH (batch);
	TrackerDirectBatchPrivate *priv = tracker_direct_batch_get_instance_private (direct);
	TrackerBatchElem elem;

	elem.type = TRACKER_DIRECT_BATCH_RESOURCE;
	elem.d.resource.graph = g_strdup (graph);
	elem.d.resource.resource = g_object_ref (resource);
	g_array_append_val (priv->array, elem);
}

static void
free_value (gpointer data)
{
	GValue *value = data;

	g_value_unset (value);
	g_free (value);
}

static void
tracker_direct_batch_add_statement (TrackerBatch           *batch,
                                    TrackerSparqlStatement *stmt,
                                    guint                   n_values,
                                    const gchar            *binding_names[],
                                    const GValue            bindings[])
{
	TrackerDirectBatch *direct = TRACKER_DIRECT_BATCH (batch);
	TrackerDirectBatchPrivate *priv = tracker_direct_batch_get_instance_private (direct);
	TrackerBatchElem elem;
	GHashTable *parameters = NULL;
	guint i;

	for (i = 0; i < n_values; i++) {
		GValue *copy;

		if (!parameters) {
			parameters = g_hash_table_new_full (g_str_hash,
			                                    g_str_equal,
			                                    g_free,
			                                    (GDestroyNotify) free_value);
		}

		copy = g_new0 (GValue, 1);
		g_value_init (copy, G_VALUE_TYPE (&bindings[i]));
		g_value_copy (&bindings[i], copy);
		g_hash_table_insert (parameters,
		                     g_strdup (binding_names[i]),
		                     copy);
	}

	elem.type = TRACKER_DIRECT_BATCH_STATEMENT;
	elem.d.statement.stmt = g_object_ref (stmt);
	elem.d.statement.parameters = parameters;
	g_array_append_val (priv->array, elem);
}

void
tracker_direct_batch_add_rdf (TrackerBatch            *batch,
                              TrackerDeserializeFlags  flags,
                              TrackerRdfFormat         format,
                              const gchar             *default_graph,
                              GInputStream            *stream)
{
	TrackerDirectBatch *direct = TRACKER_DIRECT_BATCH (batch);
	TrackerDirectBatchPrivate *priv = tracker_direct_batch_get_instance_private (direct);
	TrackerBatchElem elem;

	elem.type = TRACKER_DIRECT_BATCH_RDF;
	elem.d.rdf.default_graph = g_strdup (default_graph);
	elem.d.rdf.format = format;
	elem.d.rdf.flags = flags;
	elem.d.rdf.stream = g_object_ref (stream);
	g_array_append_val (priv->array, elem);
}

void
tracker_direct_batch_add_dbus_fd (TrackerBatch *batch,
                                  GInputStream *istream)
{
	TrackerDirectBatch *direct = TRACKER_DIRECT_BATCH (batch);
	TrackerDirectBatchPrivate *priv = tracker_direct_batch_get_instance_private (direct);
	TrackerBatchElem elem;

	elem.type = TRACKER_DIRECT_BATCH_DBUS_FD;
	elem.d.dbus_fd = g_object_ref (istream);
	g_array_append_val (priv->array, elem);
}

static gboolean
tracker_direct_batch_execute (TrackerBatch  *batch,
                              GCancellable  *cancellable,
                              GError       **error)
{
	TrackerDirectConnection *conn;

	conn = TRACKER_DIRECT_CONNECTION (tracker_batch_get_connection (batch));

	return tracker_direct_connection_update_batch (conn, batch, error);
}

static void
tracker_direct_batch_execute_async (TrackerBatch        *batch,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
	TrackerDirectConnection *conn;

	conn = TRACKER_DIRECT_CONNECTION (tracker_batch_get_connection (batch));

	tracker_direct_connection_update_batch_async (conn, batch,
	                                              cancellable,
	                                              callback,
	                                              user_data);
}

static gboolean
tracker_direct_batch_execute_finish (TrackerBatch  *batch,
                                     GAsyncResult  *res,
                                     GError       **error)
{
	TrackerDirectConnection *conn;

	conn = TRACKER_DIRECT_CONNECTION (tracker_batch_get_connection (batch));

	return tracker_direct_connection_update_batch_finish (conn, res, error);
}

static void
tracker_direct_batch_class_init (TrackerDirectBatchClass *klass)
{
	TrackerBatchClass *batch_class = (TrackerBatchClass *) klass;
	GObjectClass *object_class = (GObjectClass *) klass;

	object_class->finalize = tracker_direct_batch_finalize;

	batch_class->add_sparql = tracker_direct_batch_add_sparql;
	batch_class->add_resource = tracker_direct_batch_add_resource;
	batch_class->add_statement = tracker_direct_batch_add_statement;
	batch_class->add_rdf = tracker_direct_batch_add_rdf;
	batch_class->add_dbus_fd = tracker_direct_batch_add_dbus_fd;
	batch_class->execute = tracker_direct_batch_execute;
	batch_class->execute_async = tracker_direct_batch_execute_async;
	batch_class->execute_finish = tracker_direct_batch_execute_finish;
}

static void
tracker_batch_elem_clear (TrackerBatchElem *elem)
{
	if (elem->type == TRACKER_DIRECT_BATCH_RESOURCE) {
		g_object_run_dispose (G_OBJECT (elem->d.resource.resource));
		g_object_unref (elem->d.resource.resource);
		g_free (elem->d.resource.graph);
	} else if (elem->type == TRACKER_DIRECT_BATCH_STATEMENT) {
		g_object_unref (elem->d.statement.stmt);
		g_clear_pointer (&elem->d.statement.parameters,
		                 g_hash_table_unref);
	} else if (elem->type == TRACKER_DIRECT_BATCH_SPARQL) {
		g_free (elem->d.sparql);
	} else if (elem->type == TRACKER_DIRECT_BATCH_RDF) {
		g_free (elem->d.rdf.default_graph);
		g_clear_object (&elem->d.rdf.stream);
	} else if (elem->type == TRACKER_DIRECT_BATCH_DBUS_FD) {
		g_clear_object (&elem->d.dbus_fd);
	}
}

static void
tracker_direct_batch_init (TrackerDirectBatch *batch)
{
	TrackerDirectBatchPrivate *priv;

	priv = tracker_direct_batch_get_instance_private (batch);
	priv->array = g_array_new (FALSE, FALSE, sizeof (TrackerBatchElem));
	g_array_set_clear_func (priv->array, (GDestroyNotify) tracker_batch_elem_clear);
}

TrackerBatch *
tracker_direct_batch_new (TrackerSparqlConnection *conn)
{
	return g_object_new (TRACKER_TYPE_DIRECT_BATCH,
	                     "connection", conn,
	                     NULL);
}

/* Executes with the update lock held */
gboolean
tracker_direct_batch_update (TrackerDirectBatch  *batch,
                             TrackerDataManager  *data_manager,
                             GError             **error)
{
	TrackerDirectBatchPrivate *priv;
	GError *inner_error = NULL;
	GHashTable *bnodes, *visited;
	TrackerData *data;
	const gchar *last_graph = NULL;
	guint i;

	priv = tracker_direct_batch_get_instance_private (batch);
	data = tracker_data_manager_get_data (data_manager);
	bnodes = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
	                                (GDestroyNotify) tracker_rowid_free);
	visited = g_hash_table_new_full (NULL, NULL, NULL, (GDestroyNotify) tracker_rowid_free);

	if (!tracker_data_begin_transaction (data, &inner_error))
		goto error;

	for (i = 0; i < priv->array->len; i++) {
		TrackerBatchElem *elem;

		elem = &g_array_index (priv->array, TrackerBatchElem, i);

		if (elem->type == TRACKER_DIRECT_BATCH_RESOURCE) {
			/* Clear the visited resources set on graph changes, there
			 * might be resources that are referenced from multiple
			 * graphs.
			 */
			if (g_strcmp0 (last_graph, elem->d.resource.graph) != 0)
				g_hash_table_remove_all (visited);

			tracker_data_update_resource (data,
			                              elem->d.resource.graph,
			                              elem->d.resource.resource,
			                              bnodes,
			                              visited,
			                              &inner_error);
			last_graph = elem->d.resource.graph;
		} else if (elem->type == TRACKER_DIRECT_BATCH_SPARQL) {
			TrackerSparql *query;

			query = tracker_sparql_new_update (data_manager,
			                                   elem->d.sparql,
			                                   &inner_error);
			if (query) {
				tracker_sparql_execute_update (query,
				                               NULL,
				                               bnodes,
				                               NULL,
				                               &inner_error);
				g_object_unref (query);
			}
		} else if (elem->type == TRACKER_DIRECT_BATCH_STATEMENT) {
			tracker_direct_statement_execute_update (elem->d.statement.stmt,
			                                         elem->d.statement.parameters,
			                                         bnodes,
			                                         &inner_error);
		} else if (elem->type == TRACKER_DIRECT_BATCH_RDF) {
			TrackerSparqlCursor *deserializer;

			deserializer = tracker_deserializer_new (elem->d.rdf.stream,
			                                         NULL,
			                                         convert_format (elem->d.rdf.format));

			tracker_data_load_from_deserializer (data,
			                                     TRACKER_DESERIALIZER (deserializer),
			                                     elem->d.rdf.default_graph,
			                                     "<stream>",
			                                     bnodes,
			                                     &inner_error);
			g_object_unref (deserializer);
		} else if (elem->type == TRACKER_DIRECT_BATCH_DBUS_FD) {
			tracker_data_load_from_dbus_fd (data, elem->d.dbus_fd,
			                                bnodes, NULL,
			                                &inner_error);
		} else {
			g_assert_not_reached ();
		}

		if (inner_error)
			break;
	}

	if (inner_error) {
		tracker_data_rollback_transaction (data);
		goto error;
	}

	if (!tracker_data_commit_transaction (data, &inner_error))
		goto error;

	g_array_set_size (priv->array, 0);
	g_hash_table_unref (bnodes);
	g_hash_table_unref (visited);

	return TRUE;

error:
	g_hash_table_unref (bnodes);
	g_hash_table_unref (visited);
	g_propagate_error (error, inner_error);
	return FALSE;
}
