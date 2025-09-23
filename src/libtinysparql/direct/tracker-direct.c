/*
 * Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
 * Copyright (C) 2017, Red Hat, Inc.
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
 */

#include "config.h"

#include "tracker-direct.h"
#include "tracker-direct-batch.h"
#include "tracker-direct-statement.h"

#include <tracker-common.h>

#include "core/tracker-data.h"
#include "tracker-notifier-private.h"
#include "tracker-private.h"
#include "tracker-serializer.h"

typedef struct _TrackerDirectConnectionPrivate TrackerDirectConnectionPrivate;

struct _TrackerDirectConnectionPrivate
{
	TrackerSparqlConnectionFlags flags;
	GFile *store;
	GFile *ontology;

	TrackerNamespaceManager *namespace_manager;
	TrackerDataManager *data_manager;
	GMutex update_mutex;

	GThreadPool *update_thread; /* Contains 1 exclusive thread */
	GThreadPool *select_pool;

	GList *notifiers;

	gint64 timestamp;
	gint64 cleanup_timestamp;

	guint cleanup_timeout_id;

	guint initialized : 1;
	guint closing     : 1;
};

typedef enum {
	TASK_TYPE_QUERY,
	TASK_TYPE_QUERY_STATEMENT,
	TASK_TYPE_SERIALIZE,
	TASK_TYPE_SERIALIZE_STATEMENT,
	TASK_TYPE_UPDATE,
	TASK_TYPE_UPDATE_BLANK,
	TASK_TYPE_UPDATE_RESOURCE,
	TASK_TYPE_UPDATE_BATCH,
	TASK_TYPE_UPDATE_STATEMENT,
	TASK_TYPE_DESERIALIZE,
	TASK_TYPE_RELEASE_MEMORY,
} TaskType;

typedef struct {
	TaskType type;

	union {
		gchar *sparql;

		TrackerBatch *batch;

		struct {
			TrackerSparqlStatement *stmt;
			GHashTable *parameters;
		} statement;

		struct {
			gchar *graph;
			TrackerResource *resource;
		} update_resource;

		struct {
			gchar *sparql;
			TrackerRdfFormat format;
			TrackerSerializeFlags flags;
		} serialize;

		struct {
			TrackerSparqlStatement *stmt;
			GHashTable *parameters;
			TrackerRdfFormat format;
			TrackerSerializeFlags flags;
		} serialize_statement;

		struct {
			GInputStream *stream;
			gchar *default_graph;
			TrackerRdfFormat format;
			TrackerDeserializeFlags flags;
		} deserialize;
	} d;
} TaskData;

enum {
	PROP_0,
	PROP_FLAGS,
	PROP_STORE_LOCATION,
	PROP_ONTOLOGY_LOCATION,
	N_PROPS
};

static GParamSpec *props[N_PROPS] = { NULL };

static void tracker_direct_connection_initable_iface_init (GInitableIface *iface);
static void tracker_direct_connection_async_initable_iface_init (GAsyncInitableIface *iface);

G_DEFINE_QUARK (TrackerDirectNotifier, tracker_direct_notifier)

G_DEFINE_TYPE_WITH_CODE (TrackerDirectConnection, tracker_direct_connection,
                         TRACKER_TYPE_SPARQL_CONNECTION,
                         G_ADD_PRIVATE (TrackerDirectConnection)
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                tracker_direct_connection_initable_iface_init)
                         G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE,
                                                tracker_direct_connection_async_initable_iface_init))

static TaskData *
task_data_new (TaskType type)
{
	TaskData *task;

	task = g_new0 (TaskData, 1);
	task->type = type;

	return task;
}

static void
task_data_free (TaskData *task)
{
	switch (task->type) {
	case TASK_TYPE_QUERY:
	case TASK_TYPE_UPDATE:
	case TASK_TYPE_UPDATE_BLANK:
		g_free (task->d.sparql);
		break;
	case TASK_TYPE_SERIALIZE:
		g_free (task->d.serialize.sparql);
		break;
	case TASK_TYPE_UPDATE_RESOURCE:
		g_free (task->d.update_resource.graph);
		g_object_unref (task->d.update_resource.resource);
		break;
	case TASK_TYPE_UPDATE_BATCH:
		g_clear_object (&task->d.batch);
		break;
	case TASK_TYPE_UPDATE_STATEMENT:
	case TASK_TYPE_QUERY_STATEMENT:
		g_clear_object (&task->d.statement.stmt);
		g_clear_pointer (&task->d.statement.parameters, g_hash_table_unref);
		break;
	case TASK_TYPE_SERIALIZE_STATEMENT:
		g_clear_object (&task->d.serialize_statement.stmt);
		g_clear_pointer (&task->d.serialize_statement.parameters,
		                 g_hash_table_unref);
		break;
	case TASK_TYPE_RELEASE_MEMORY:
		break;
	case TASK_TYPE_DESERIALIZE:
		g_clear_object (&task->d.deserialize.stream);
		g_free (task->d.deserialize.default_graph);
		break;
	}
	g_free (task);
}

static gboolean
cleanup_timeout_cb (gpointer user_data)
{
	TrackerDirectConnection *conn = user_data;
	TrackerDirectConnectionPrivate *priv;
	gint64 timestamp;
	GTask *task;

	priv = tracker_direct_connection_get_instance_private (conn);
	timestamp = g_get_monotonic_time ();

	/* If we already cleaned up */
	if (priv->timestamp < priv->cleanup_timestamp)
		return G_SOURCE_CONTINUE;
	/* If the connection was used less than 10s ago */
	if (timestamp - priv->timestamp < 10 * G_USEC_PER_SEC)
		return G_SOURCE_CONTINUE;

	priv->cleanup_timestamp = timestamp;

	task = g_task_new (conn, NULL, NULL, NULL);
	g_task_set_task_data (task,
	                      task_data_new (TASK_TYPE_RELEASE_MEMORY),
	                      (GDestroyNotify) task_data_free);

	g_thread_pool_push (priv->update_thread, task, NULL);

	return G_SOURCE_CONTINUE;
}

gboolean
update_resource (TrackerData      *data,
                 const gchar      *graph,
                 TrackerResource  *resource,
                 GError          **error)
{
	GError *inner_error = NULL;
	GHashTable *visited;

	if (!tracker_data_begin_transaction (data, &inner_error))
		goto error;

	visited = g_hash_table_new_full (NULL, NULL, NULL,
	                                 (GDestroyNotify) tracker_rowid_free);

	tracker_data_update_resource (data,
	                              graph,
	                              resource,
	                              NULL,
	                              visited,
	                              &inner_error);

	g_hash_table_unref (visited);

	if (inner_error) {
		tracker_data_rollback_transaction (data);
		goto error;
	}

	if (!tracker_data_commit_transaction (data, &inner_error))
		goto error;

	return TRUE;

error:
	g_propagate_error (error, inner_error);
	return FALSE;
}

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
update_thread_func (gpointer data,
                    gpointer user_data)
{
	TrackerDirectConnectionPrivate *priv;
	TrackerDirectConnection *conn;
	GTask *task = data;
	TaskData *task_data = g_task_get_task_data (task);
	TrackerData *tracker_data;
	GError *error = NULL;
	gpointer retval = NULL;
	GDestroyNotify destroy_notify = NULL;
	gboolean update_timestamp = TRUE;

	conn = user_data;
	priv = tracker_direct_connection_get_instance_private (conn);

	g_mutex_lock (&priv->update_mutex);
	tracker_data = tracker_data_manager_get_data (priv->data_manager);

	switch (task_data->type) {
	case TASK_TYPE_QUERY:
	case TASK_TYPE_QUERY_STATEMENT:
	case TASK_TYPE_SERIALIZE:
	case TASK_TYPE_SERIALIZE_STATEMENT:
		g_warning ("Queries don't go through this thread");
		break;
	case TASK_TYPE_UPDATE:
		tracker_data_update_sparql (tracker_data, task_data->d.sparql, &error);
		break;
	case TASK_TYPE_UPDATE_BLANK:
		retval = tracker_data_update_sparql_blank (tracker_data, task_data->d.sparql, &error);
		destroy_notify = (GDestroyNotify) g_variant_unref;
		break;
	case TASK_TYPE_UPDATE_RESOURCE:
		update_resource (tracker_data,
		                 task_data->d.update_resource.graph,
		                 task_data->d.update_resource.resource,
		                 &error);
		break;
	case TASK_TYPE_DESERIALIZE: {
		TrackerSparqlCursor *deserializer;

		if (!tracker_data_begin_transaction (tracker_data, &error))
			break;

		deserializer = tracker_deserializer_new (task_data->d.deserialize.stream,
							 NULL,
		                                         convert_format (task_data->d.deserialize.format));

		if (tracker_data_load_from_deserializer (tracker_data,
		                                         TRACKER_DESERIALIZER (deserializer),
		                                         task_data->d.deserialize.default_graph,
		                                         "<stream>",
		                                         NULL,
		                                         &error)) {
			tracker_data_commit_transaction (tracker_data, &error);
		} else {
			tracker_data_rollback_transaction (tracker_data);
		}
		g_object_unref (deserializer);
		break;
	}
	case TASK_TYPE_UPDATE_BATCH:
		tracker_direct_batch_update (TRACKER_DIRECT_BATCH (task_data->d.batch),
		                             priv->data_manager, &error);
		break;
	case TASK_TYPE_UPDATE_STATEMENT:
		if (!tracker_data_begin_transaction (tracker_data, &error))
			break;

		if (tracker_direct_statement_execute_update (task_data->d.statement.stmt,
		                                             task_data->d.statement.parameters,
		                                             NULL,
		                                             &error)) {
			tracker_data_commit_transaction (tracker_data, &error);
		} else {
			tracker_data_rollback_transaction (tracker_data);
		}
		break;
	case TASK_TYPE_RELEASE_MEMORY:
		tracker_data_manager_release_memory (priv->data_manager);
		update_timestamp = FALSE;
		break;
	}

	if (error)
		g_task_return_error (task, error);
	else if (retval)
		g_task_return_pointer (task, retval, destroy_notify);
	else
		g_task_return_boolean (task, TRUE);

	g_object_unref (task);

	if (update_timestamp)
		tracker_direct_connection_update_timestamp (conn);

	g_mutex_unlock (&priv->update_mutex);
}

static void
execute_query_in_thread (GTask    *task,
                         TaskData *task_data)
{
	TrackerSparqlConnection *conn;
	TrackerSparqlCursor *cursor;
	GError *error = NULL;

	if (g_task_return_error_if_cancelled (task))
		return;

	conn = g_task_get_source_object (task);

	if (task_data->type == TASK_TYPE_QUERY) {
		cursor = tracker_sparql_connection_query (conn,
		                                          task_data->d.sparql,
		                                          g_task_get_cancellable (task),
		                                          &error);
	} else if (task_data->type == TASK_TYPE_QUERY_STATEMENT) {
		TrackerSparql *sparql;

		sparql = tracker_direct_statement_get_sparql (task_data->d.statement.stmt);
		cursor = tracker_sparql_execute_cursor (sparql,
		                                        task_data->d.statement.parameters,
		                                        &error);
	} else {
		g_assert_not_reached ();
	}

	if (cursor) {
		tracker_direct_connection_update_timestamp (TRACKER_DIRECT_CONNECTION (conn));
		g_task_return_pointer (task, cursor, g_object_unref);
	} else {
		g_task_return_error (task, error);
	}
}

static void
serialize_in_thread (GTask    *task,
                     TaskData *task_data)
{
	TrackerDirectConnectionPrivate *priv;
	TrackerDirectConnection *conn;
	TrackerSparql *query = NULL;
	TrackerSparqlCursor *cursor = NULL;
	TrackerNamespaceManager *namespaces;
	TrackerRdfFormat format;
	GInputStream *istream = NULL;
	GHashTable *parameters = NULL;
	GError *error = NULL;

	conn = g_task_get_source_object (task);
	priv = tracker_direct_connection_get_instance_private (conn);

	if (task_data->type == TASK_TYPE_SERIALIZE) {
		format = task_data->d.serialize.format;
		query = tracker_sparql_new (priv->data_manager,
		                            task_data->d.serialize.sparql,
		                            &error);
		if (!query)
			goto out;
	} else if (task_data->type == TASK_TYPE_SERIALIZE_STATEMENT) {
		TrackerSparqlStatement *stmt;

		format = task_data->d.serialize_statement.format;
		stmt = task_data->d.serialize_statement.stmt;
		query = g_object_ref (tracker_direct_statement_get_sparql (stmt));
		parameters = task_data->d.serialize_statement.parameters;
	} else {
		g_assert_not_reached ();
	}

	if (!tracker_sparql_is_serializable (query)) {
		g_set_error (&error,
		             TRACKER_SPARQL_ERROR,
		             TRACKER_SPARQL_ERROR_PARSE,
		             "Query is not DESCRIBE or CONSTRUCT");
		goto out;
	}

	cursor = tracker_sparql_execute_cursor (query, parameters, &error);
	if (!cursor)
		goto out;

	tracker_direct_connection_update_timestamp (conn);
	tracker_sparql_cursor_set_connection (cursor, TRACKER_SPARQL_CONNECTION (conn));
	namespaces = tracker_sparql_connection_get_namespace_manager (TRACKER_SPARQL_CONNECTION (conn));
	istream = tracker_serializer_new (cursor, namespaces, convert_format (format));

 out:
	g_clear_object (&query);
	g_clear_object (&cursor);

	if (istream)
		g_task_return_pointer (task, istream, g_object_unref);
	else
		g_task_return_error (task, error);
}

static void
query_thread_pool_func (gpointer data,
                        gpointer user_data)
{
	TrackerDirectConnection *conn = user_data;
	TrackerDirectConnectionPrivate *priv;
	GTask *task = data;
	TaskData *task_data = g_task_get_task_data (task);

	priv = tracker_direct_connection_get_instance_private (conn);

	if (priv->closing) {
		g_task_return_new_error (task,
		                         G_IO_ERROR,
		                         G_IO_ERROR_CONNECTION_CLOSED,
		                         "Connection is closed");
		g_object_unref (task);
		return;
	}

	switch (task_data->type) {
	case TASK_TYPE_QUERY:
	case TASK_TYPE_QUERY_STATEMENT:
		execute_query_in_thread (task, task_data);
		break;
	case TASK_TYPE_SERIALIZE:
	case TASK_TYPE_SERIALIZE_STATEMENT:
		serialize_in_thread (task, task_data);
		break;
	default:
		g_assert_not_reached ();
	}

	g_object_unref (task);
}

static gboolean
set_up_thread_pools (TrackerDirectConnection  *conn,
		     GError                  **error)
{
	TrackerDirectConnectionPrivate *priv;

	priv = tracker_direct_connection_get_instance_private (conn);

	priv->select_pool = g_thread_pool_new (query_thread_pool_func,
	                                       conn, 16, FALSE, error);
	if (!priv->select_pool)
		return FALSE;

	priv->update_thread = g_thread_pool_new (update_thread_func,
	                                         conn, 1, TRUE, error);
	if (!priv->update_thread)
		return FALSE;

	return TRUE;
}

static TrackerDBManagerFlags
translate_flags (TrackerSparqlConnectionFlags flags)
{
	TrackerDBManagerFlags db_flags = TRACKER_DB_MANAGER_FLAGS_NONE;

	if ((flags & TRACKER_SPARQL_CONNECTION_FLAGS_READONLY) != 0)
		db_flags |= TRACKER_DB_MANAGER_READONLY;
	if ((flags & TRACKER_SPARQL_CONNECTION_FLAGS_FTS_ENABLE_STEMMER) != 0)
		db_flags |= TRACKER_DB_MANAGER_FTS_ENABLE_STEMMER;
	if ((flags & TRACKER_SPARQL_CONNECTION_FLAGS_FTS_ENABLE_UNACCENT) != 0)
		db_flags |= TRACKER_DB_MANAGER_FTS_ENABLE_UNACCENT;
	if ((flags & TRACKER_SPARQL_CONNECTION_FLAGS_FTS_ENABLE_STOP_WORDS) != 0)
		db_flags |= TRACKER_DB_MANAGER_FTS_ENABLE_STOP_WORDS;
	if ((flags & TRACKER_SPARQL_CONNECTION_FLAGS_FTS_IGNORE_NUMBERS) != 0)
		db_flags |= TRACKER_DB_MANAGER_FTS_IGNORE_NUMBERS;
	if ((flags & TRACKER_SPARQL_CONNECTION_FLAGS_ANONYMOUS_BNODES) != 0)
		db_flags |= TRACKER_DB_MANAGER_ANONYMOUS_BNODES;

	return db_flags;
}

static gboolean
tracker_direct_connection_initable_init (GInitable     *initable,
                                         GCancellable  *cancellable,
                                         GError       **error)
{
	TrackerDirectConnectionPrivate *priv;
	TrackerDirectConnection *conn;
	TrackerDBManagerFlags db_flags;
	GHashTable *namespaces;
	GHashTableIter iter;
	gchar *prefix, *ns;
	GError *inner_error = NULL;

	conn = TRACKER_DIRECT_CONNECTION (initable);
	priv = tracker_direct_connection_get_instance_private (conn);

	if (!set_up_thread_pools (conn, error))
		return FALSE;

	db_flags = translate_flags (priv->flags);

	if (!priv->store) {
		db_flags |= TRACKER_DB_MANAGER_IN_MEMORY;
	}

	priv->data_manager = tracker_data_manager_new (db_flags, priv->store,
	                                               priv->ontology,
	                                               100);
	if (!g_initable_init (G_INITABLE (priv->data_manager), cancellable, &inner_error)) {
		g_propagate_error (error, _translate_internal_error (inner_error));
		g_clear_object (&priv->data_manager);
		return FALSE;
	}

	/* Initialize namespace manager */
	priv->namespace_manager = tracker_namespace_manager_new ();
	namespaces = tracker_data_manager_get_namespaces (priv->data_manager);
	g_hash_table_iter_init (&iter, namespaces);

	while (g_hash_table_iter_next (&iter, (gpointer*) &prefix, (gpointer*) &ns)) {
		tracker_namespace_manager_add_prefix (priv->namespace_manager,
		                                      prefix, ns);
	}

	g_hash_table_unref (namespaces);

	priv->cleanup_timeout_id =
		g_timeout_add_seconds (30, cleanup_timeout_cb, conn);

	return TRUE;
}

static void
tracker_direct_connection_initable_iface_init (GInitableIface *iface)
{
	iface->init = tracker_direct_connection_initable_init;
}

static void
async_initable_thread_func (GTask        *task,
                            gpointer      source_object,
                            gpointer      task_data,
                            GCancellable *cancellable)
{
	GError *error = NULL;

	if (!g_initable_init (G_INITABLE (source_object), cancellable, &error))
		g_task_return_error (task, error);
	else
		g_task_return_boolean (task, TRUE);

	g_object_unref (task);
}

static void
tracker_direct_connection_async_initable_init_async (GAsyncInitable      *async_initable,
                                                     gint                 priority,
                                                     GCancellable        *cancellable,
                                                     GAsyncReadyCallback  callback,
                                                     gpointer             user_data)
{
	GTask *task;

	task = g_task_new (async_initable, cancellable, callback, user_data);
	g_task_set_priority (task, priority);
	g_task_run_in_thread (task, async_initable_thread_func);
}

static gboolean
tracker_direct_connection_async_initable_init_finish (GAsyncInitable  *async_initable,
                                                      GAsyncResult    *res,
                                                      GError         **error)
{
	return g_task_propagate_boolean (G_TASK (res), error);
}

static void
tracker_direct_connection_async_initable_iface_init (GAsyncInitableIface *iface)
{
	iface->init_async = tracker_direct_connection_async_initable_init_async;
	iface->init_finish = tracker_direct_connection_async_initable_init_finish;
}

static void
tracker_direct_connection_init (TrackerDirectConnection *conn)
{
}

static GHashTable *
get_event_cache_ht (TrackerNotifier *notifier)
{
	GHashTable *events;

	events = g_object_get_qdata (G_OBJECT (notifier), tracker_direct_notifier_quark ());
	if (!events) {
		events = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
		                                (GDestroyNotify) _tracker_notifier_event_cache_free);
		g_object_set_qdata_full (G_OBJECT (notifier), tracker_direct_notifier_quark (),
		                         events, (GDestroyNotify) g_hash_table_unref);
	}

	return events;
}

static TrackerNotifierEventCache *
lookup_event_cache (TrackerNotifier *notifier,
                    const gchar     *graph)
{
	TrackerNotifierEventCache *cache;
	GHashTable *events;

	if (!graph)
		graph = "";

	events = get_event_cache_ht (notifier);
	cache = g_hash_table_lookup (events, graph);

	if (!cache) {
		cache = _tracker_notifier_event_cache_new (notifier, graph);
		g_hash_table_insert (events,
		                     (gpointer) tracker_notifier_event_cache_get_graph (cache),
		                     cache);
	}

	return cache;
}

/* These callbacks will be called from a different thread
 * (always the same one though), handle with care.
 */
static void
statement_cb (TrackerDataUpdateType  type,
              const gchar           *graph,
              TrackerRowid           subject_id,
              TrackerRowid           predicate_id,
              TrackerRowid           object_id,
              GPtrArray             *rdf_types,
              gpointer               user_data)
{
	TrackerNotifier *notifier = user_data;
	TrackerSparqlConnection *conn = _tracker_notifier_get_connection (notifier);
	TrackerDirectConnection *direct = TRACKER_DIRECT_CONNECTION (conn);
	TrackerDirectConnectionPrivate *priv = tracker_direct_connection_get_instance_private (direct);
	TrackerOntologies *ontologies = tracker_data_manager_get_ontologies (priv->data_manager);
	TrackerProperty *rdf_type = tracker_ontologies_get_rdf_type (ontologies);
	TrackerNotifierEventCache *cache;
	TrackerClass *rdf_type_class = NULL;
	guint i;

	cache = lookup_event_cache (notifier, graph);

	if (predicate_id == tracker_property_get_id (rdf_type)) {
		const gchar *uri;

		uri = tracker_ontologies_get_uri_by_id (ontologies, object_id);
		rdf_type_class = tracker_ontologies_get_class_by_uri (ontologies, uri);
	}

	for (i = 0; i < rdf_types->len; i++) {
		TrackerClass *class = g_ptr_array_index (rdf_types, i);
		TrackerNotifierEventType event_type;

		if (!tracker_class_get_notify (class))
			continue;

		if (rdf_type_class && class == rdf_type_class) {
			if (type == TRACKER_DATA_INSERT)
				event_type = TRACKER_NOTIFIER_EVENT_CREATE;
			else
				event_type = TRACKER_NOTIFIER_EVENT_DELETE;
		} else {
			event_type = TRACKER_NOTIFIER_EVENT_UPDATE;
		}

		_tracker_notifier_event_cache_push_event (cache, subject_id, event_type);
	}
}

static void
transaction_cb (TrackerDataTransactionType type,
                gpointer                   user_data)
{
	TrackerNotifier *notifier = user_data;
	GHashTable *events;

	events = get_event_cache_ht (notifier);

	if (type == TRACKER_DATA_COMMIT) {
		TrackerNotifierEventCache *cache;
		GHashTableIter iter;

		g_hash_table_iter_init (&iter, events);

		while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &cache)) {
			g_hash_table_iter_steal (&iter);
			_tracker_notifier_event_cache_flush_events (notifier, cache);
		}
	} else {
		g_hash_table_remove_all (events);
	}
}

static void
detach_notifier (TrackerDirectConnection *conn,
                 TrackerNotifier         *notifier)
{
	TrackerDirectConnectionPrivate *priv;
	TrackerData *tracker_data;

	priv = tracker_direct_connection_get_instance_private (conn);

	priv->notifiers = g_list_remove (priv->notifiers, notifier);

	tracker_notifier_stop (notifier);
	tracker_data = tracker_data_manager_get_data (priv->data_manager);
	tracker_data_remove_statement_callback (tracker_data,
	                                        statement_cb,
	                                        notifier);
	tracker_data_remove_transaction_callback (tracker_data,
	                                          transaction_cb,
	                                          notifier);
}

static void
weak_ref_notify (gpointer  data,
                 GObject  *prev_location)
{
	TrackerDirectConnection *conn = data;

	detach_notifier (conn, (TrackerNotifier *) prev_location);
}

static void
tracker_direct_connection_finalize (GObject *object)
{
	TrackerDirectConnectionPrivate *priv;
	TrackerDirectConnection *conn;

	conn = TRACKER_DIRECT_CONNECTION (object);
	priv = tracker_direct_connection_get_instance_private (conn);

	if (!priv->closing)
		tracker_sparql_connection_close (TRACKER_SPARQL_CONNECTION (object));

	g_clear_object (&priv->store);
	g_clear_object (&priv->ontology);
	g_clear_object (&priv->namespace_manager);

	G_OBJECT_CLASS (tracker_direct_connection_parent_class)->finalize (object);
}

static void
tracker_direct_connection_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
	TrackerDirectConnectionPrivate *priv;
	TrackerDirectConnection *conn;

	conn = TRACKER_DIRECT_CONNECTION (object);
	priv = tracker_direct_connection_get_instance_private (conn);

	switch (prop_id) {
	case PROP_FLAGS:
		priv->flags = g_value_get_flags (value);
		break;
	case PROP_STORE_LOCATION:
		priv->store = g_value_dup_object (value);
		break;
	case PROP_ONTOLOGY_LOCATION:
		priv->ontology = g_value_dup_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
tracker_direct_connection_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
	TrackerDirectConnectionPrivate *priv;
	TrackerDirectConnection *conn;

	conn = TRACKER_DIRECT_CONNECTION (object);
	priv = tracker_direct_connection_get_instance_private (conn);

	switch (prop_id) {
	case PROP_FLAGS:
		g_value_set_flags (value, priv->flags);
		break;
	case PROP_STORE_LOCATION:
		g_value_set_object (value, priv->store);
		break;
	case PROP_ONTOLOGY_LOCATION:
		g_value_set_object (value, priv->ontology);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static TrackerSparqlCursor *
tracker_direct_connection_query (TrackerSparqlConnection  *self,
                                 const gchar              *sparql,
                                 GCancellable             *cancellable,
                                 GError                  **error)
{
	TrackerDirectConnectionPrivate *priv;
	TrackerDirectConnection *conn;
	TrackerSparql *query;
	TrackerSparqlCursor *cursor = NULL;
	GError *inner_error = NULL;

	conn = TRACKER_DIRECT_CONNECTION (self);
	priv = tracker_direct_connection_get_instance_private (conn);

	query = tracker_sparql_new (priv->data_manager, sparql, &inner_error);
	if (query) {
		cursor = tracker_sparql_execute_cursor (query, NULL, &inner_error);
		tracker_direct_connection_update_timestamp (conn);
		g_object_unref (query);
	}

	if (inner_error)
		g_propagate_error (error, _translate_internal_error (inner_error));

	return cursor;
}

static void
tracker_direct_connection_query_async (TrackerSparqlConnection *self,
                                       const gchar             *sparql,
                                       GCancellable            *cancellable,
                                       GAsyncReadyCallback      callback,
                                       gpointer                 user_data)
{
	TrackerDirectConnectionPrivate *priv;
	TrackerDirectConnection *conn;
	TaskData *task_data;
	GError *error = NULL;
	GTask *task;

	conn = TRACKER_DIRECT_CONNECTION (self);
	priv = tracker_direct_connection_get_instance_private (conn);

	task_data = task_data_new (TASK_TYPE_QUERY);
	task_data->d.sparql = g_strdup (sparql);

	task = g_task_new (self, cancellable, callback, user_data);
	g_task_set_task_data (task, task_data,
	                      (GDestroyNotify) task_data_free);

	if (!g_thread_pool_push (priv->select_pool, task, &error)) {
		g_task_return_error (task, _translate_internal_error (error));
		g_object_unref (task);
	}
}

static TrackerSparqlCursor *
tracker_direct_connection_query_finish (TrackerSparqlConnection  *self,
                                        GAsyncResult             *res,
                                        GError                  **error)
{
	return g_task_propagate_pointer (G_TASK (res), error);
}

static TrackerSparqlStatement *
tracker_direct_connection_query_statement (TrackerSparqlConnection  *self,
                                            const gchar              *query,
                                            GCancellable             *cancellable,
                                            GError                  **error)
{
	return TRACKER_SPARQL_STATEMENT (tracker_direct_statement_new (self, query, error));
}

static TrackerSparqlStatement *
tracker_direct_connection_update_statement (TrackerSparqlConnection  *self,
                                            const gchar              *query,
                                            GCancellable             *cancellable,
                                            GError                  **error)
{
	return TRACKER_SPARQL_STATEMENT (tracker_direct_statement_new_update (self, query, error));
}

static void
tracker_direct_connection_update (TrackerSparqlConnection  *self,
                                  const gchar              *sparql,
                                  GCancellable             *cancellable,
                                  GError                  **error)
{
	TrackerDirectConnectionPrivate *priv;
	TrackerDirectConnection *conn;
	TrackerData *data;
	GError *inner_error = NULL;

	conn = TRACKER_DIRECT_CONNECTION (self);
	priv = tracker_direct_connection_get_instance_private (conn);

	g_mutex_lock (&priv->update_mutex);
	data = tracker_data_manager_get_data (priv->data_manager);
	tracker_data_update_sparql (data, sparql, &inner_error);
	tracker_direct_connection_update_timestamp (conn);
	g_mutex_unlock (&priv->update_mutex);

	if (inner_error)
		g_propagate_error (error, inner_error);
}

static void
tracker_direct_connection_update_async (TrackerSparqlConnection *self,
                                        const gchar             *sparql,
                                        GCancellable            *cancellable,
                                        GAsyncReadyCallback      callback,
                                        gpointer                 user_data)
{
	TrackerDirectConnectionPrivate *priv;
	TrackerDirectConnection *conn;
	TaskData *task_data;
	GTask *task;

	conn = TRACKER_DIRECT_CONNECTION (self);
	priv = tracker_direct_connection_get_instance_private (conn);

	task_data = task_data_new (TASK_TYPE_UPDATE);
	task_data->d.sparql = g_strdup (sparql);

	task = g_task_new (self, cancellable, callback, user_data);
	g_task_set_task_data (task, task_data,
	                      (GDestroyNotify) task_data_free);

	g_thread_pool_push (priv->update_thread, task, NULL);
}

static void
tracker_direct_connection_update_finish (TrackerSparqlConnection  *self,
                                         GAsyncResult             *res,
                                         GError                  **error)
{
	GError *inner_error = NULL;

	g_task_propagate_boolean (G_TASK (res), &inner_error);
	if (inner_error)
		g_propagate_error (error, _translate_internal_error (inner_error));
}

static void
on_batch_finished (GObject      *source,
                   GAsyncResult *result,
                   gpointer      user_data)
{
	TrackerBatch *batch = TRACKER_BATCH (source);
	GTask *task = user_data;
	GError *error = NULL;
	gboolean retval;

	retval = tracker_batch_execute_finish (batch, result, &error);

	if (retval)
		g_task_return_boolean (task, TRUE);
	else
		g_task_return_error (task, error);

	g_object_unref (task);
}

static void
tracker_direct_connection_update_array_async (TrackerSparqlConnection  *self,
                                              gchar                   **updates,
                                              gint                      n_updates,
                                              GCancellable             *cancellable,
                                              GAsyncReadyCallback       callback,
                                              gpointer                  user_data)
{
	TrackerBatch *batch;
	GTask *task;
	gint i;

	batch = tracker_sparql_connection_create_batch (self);

	for (i = 0; i < n_updates; i++)
		tracker_batch_add_sparql (batch, updates[i]);

	task = g_task_new (self, cancellable, callback, user_data);
	tracker_batch_execute_async (batch, cancellable, on_batch_finished, task);
	g_object_unref (batch);
}

static gboolean
tracker_direct_connection_update_array_finish (TrackerSparqlConnection  *self,
                                               GAsyncResult             *res,
                                               GError                  **error)
{
	GError *inner_error = NULL;
	gboolean result;

	result = g_task_propagate_boolean (G_TASK (res), &inner_error);
	if (inner_error)
		g_propagate_error (error, _translate_internal_error (inner_error));

	return result;
}

static GVariant *
tracker_direct_connection_update_blank (TrackerSparqlConnection  *self,
                                        const gchar              *sparql,
                                        GCancellable             *cancellable,
                                        GError                  **error)
{
	TrackerDirectConnectionPrivate *priv;
	TrackerDirectConnection *conn;
	TrackerData *data;
	GVariant *blank_nodes;
	GError *inner_error = NULL;

	conn = TRACKER_DIRECT_CONNECTION (self);
	priv = tracker_direct_connection_get_instance_private (conn);

	g_mutex_lock (&priv->update_mutex);
	data = tracker_data_manager_get_data (priv->data_manager);
	blank_nodes = tracker_data_update_sparql_blank (data, sparql, &inner_error);
	tracker_direct_connection_update_timestamp (conn);
	g_mutex_unlock (&priv->update_mutex);

	if (inner_error)
		g_propagate_error (error, _translate_internal_error (inner_error));
	return blank_nodes;
}

static void
tracker_direct_connection_update_blank_async (TrackerSparqlConnection *self,
                                              const gchar             *sparql,
                                              GCancellable            *cancellable,
                                              GAsyncReadyCallback      callback,
                                              gpointer                 user_data)
{
	TrackerDirectConnectionPrivate *priv;
	TrackerDirectConnection *conn;
	TaskData *task_data;
	GTask *task;

	conn = TRACKER_DIRECT_CONNECTION (self);
	priv = tracker_direct_connection_get_instance_private (conn);

	task_data = task_data_new (TASK_TYPE_UPDATE_BLANK);
	task_data->d.sparql = g_strdup (sparql);

	task = g_task_new (self, cancellable, callback, user_data);
	g_task_set_task_data (task, task_data,
	                      (GDestroyNotify) task_data_free);

	g_thread_pool_push (priv->update_thread, task, NULL);
}

static GVariant *
tracker_direct_connection_update_blank_finish (TrackerSparqlConnection  *self,
                                               GAsyncResult             *res,
                                               GError                  **error)
{
	GError *inner_error = NULL;
	GVariant *result;

	result = g_task_propagate_pointer (G_TASK (res), &inner_error);
	if (inner_error)
		g_propagate_error (error, _translate_internal_error (inner_error));

	return result;
}

static TrackerNamespaceManager *
tracker_direct_connection_get_namespace_manager (TrackerSparqlConnection *self)
{
	TrackerDirectConnectionPrivate *priv;

	priv = tracker_direct_connection_get_instance_private (TRACKER_DIRECT_CONNECTION (self));

	return priv->namespace_manager;
}

static TrackerNotifier *
tracker_direct_connection_create_notifier (TrackerSparqlConnection *self)
{
	TrackerDirectConnectionPrivate *priv;
	TrackerNotifier *notifier;
	TrackerData *tracker_data;

	priv = tracker_direct_connection_get_instance_private (TRACKER_DIRECT_CONNECTION (self));

	notifier = g_object_new (TRACKER_TYPE_NOTIFIER,
	                         "connection", self,
				 NULL);

	tracker_data = tracker_data_manager_get_data (priv->data_manager);
	tracker_data_add_statement_callback (tracker_data,
	                                     statement_cb,
	                                     notifier);
	tracker_data_add_transaction_callback (tracker_data,
	                                       transaction_cb,
	                                       notifier);

	g_object_weak_ref (G_OBJECT (notifier), weak_ref_notify, self);
	priv->notifiers = g_list_prepend (priv->notifiers, notifier);

	return notifier;
}

static void
tracker_direct_connection_close (TrackerSparqlConnection *self)
{
	TrackerDirectConnectionPrivate *priv;
	TrackerDirectConnection *conn;

	conn = TRACKER_DIRECT_CONNECTION (self);
	priv = tracker_direct_connection_get_instance_private (conn);
	priv->closing = TRUE;

	if (priv->cleanup_timeout_id) {
		g_source_remove (priv->cleanup_timeout_id);
		priv->cleanup_timeout_id = 0;
	}

	if (priv->update_thread) {
		g_thread_pool_free (priv->update_thread, TRUE, TRUE);
		priv->update_thread = NULL;
	}

	if (priv->select_pool) {
		g_thread_pool_free (priv->select_pool, TRUE, TRUE);
		priv->select_pool = NULL;
	}

	while (priv->notifiers) {
		TrackerNotifier *notifier = priv->notifiers->data;

		g_object_weak_unref (G_OBJECT (notifier),
		                     weak_ref_notify,
		                     conn);
		detach_notifier (conn, notifier);
	}

	if (priv->data_manager) {
		tracker_data_manager_shutdown (priv->data_manager);
		g_clear_object (&priv->data_manager);
	}
}

static void
async_close_thread_func (GTask        *task,
                         gpointer      source_object,
                         gpointer      task_data,
                         GCancellable *cancellable)
{
	if (g_task_return_error_if_cancelled (task))
		return;

	tracker_sparql_connection_close (source_object);
	g_task_return_boolean (task, TRUE);
}

static void
tracker_direct_connection_close_async (TrackerSparqlConnection *connection,
                                       GCancellable            *cancellable,
                                       GAsyncReadyCallback      callback,
                                       gpointer                 user_data)
{
	GTask *task;

	task = g_task_new (connection, cancellable, callback, user_data);
	g_task_run_in_thread (task, async_close_thread_func);
	g_object_unref (task);
}

static gboolean
tracker_direct_connection_close_finish (TrackerSparqlConnection  *connection,
                                        GAsyncResult             *res,
                                        GError                  **error)
{
	return g_task_propagate_boolean (G_TASK (res), error);
}

static gboolean
tracker_direct_connection_update_resource (TrackerSparqlConnection  *self,
                                           const gchar              *graph,
                                           TrackerResource          *resource,
                                           GCancellable             *cancellable,
                                           GError                  **error)
{
	TrackerDirectConnectionPrivate *priv;
	TrackerDirectConnection *conn;
	TrackerData *data;
	GError *inner_error = NULL;

	conn = TRACKER_DIRECT_CONNECTION (self);
	priv = tracker_direct_connection_get_instance_private (conn);

	g_mutex_lock (&priv->update_mutex);
	data = tracker_data_manager_get_data (priv->data_manager);
	update_resource (data, graph, resource, &inner_error);
	tracker_direct_connection_update_timestamp (conn);
	g_mutex_unlock (&priv->update_mutex);

	if (inner_error) {
		g_propagate_error (error, inner_error);
		return FALSE;
	}

	return TRUE;
}

static void
tracker_direct_connection_update_resource_async (TrackerSparqlConnection *self,
                                                 const gchar             *graph,
                                                 TrackerResource         *resource,
                                                 GCancellable            *cancellable,
                                                 GAsyncReadyCallback      callback,
                                                 gpointer                 user_data)
{
	TrackerDirectConnectionPrivate *priv;
	TrackerDirectConnection *conn;
	TaskData *task_data;
	GTask *task;

	conn = TRACKER_DIRECT_CONNECTION (self);
	priv = tracker_direct_connection_get_instance_private (conn);

	task_data = task_data_new (TASK_TYPE_UPDATE_RESOURCE);
	task_data->d.update_resource.graph = g_strdup (graph);
	task_data->d.update_resource.resource = g_object_ref (resource);

	task = g_task_new (self, cancellable, callback, user_data);
	g_task_set_task_data (task, task_data,
	                      (GDestroyNotify) task_data_free);

	g_thread_pool_push (priv->update_thread, task, NULL);
}

static gboolean
tracker_direct_connection_update_resource_finish (TrackerSparqlConnection  *connection,
                                                  GAsyncResult             *res,
                                                  GError                  **error)
{
	return g_task_propagate_boolean (G_TASK (res), error);
}

static TrackerBatch *
tracker_direct_connection_create_batch (TrackerSparqlConnection *connection)
{
	TrackerDirectConnectionPrivate *priv;
	TrackerDirectConnection *conn;

	conn = TRACKER_DIRECT_CONNECTION (connection);
	priv = tracker_direct_connection_get_instance_private (conn);

	if (priv->flags & TRACKER_SPARQL_CONNECTION_FLAGS_READONLY)
		return NULL;

	return tracker_direct_batch_new (connection);
}

static gboolean
tracker_direct_connection_lookup_dbus_service (TrackerSparqlConnection  *connection,
                                               const gchar              *dbus_name,
                                               const gchar              *dbus_path,
                                               gchar                   **name,
                                               gchar                   **path)
{
	TrackerDirectConnectionPrivate *priv;
	TrackerDirectConnection *conn;
	TrackerSparqlConnection *remote;
	GError *error = NULL;
	gchar *uri;

	conn = TRACKER_DIRECT_CONNECTION (connection);
	priv = tracker_direct_connection_get_instance_private (conn);

	uri = tracker_util_build_dbus_uri (G_BUS_TYPE_SESSION,
	                                   dbus_name, dbus_path);
	remote = tracker_data_manager_get_remote_connection (priv->data_manager,
	                                                     uri, &error);
	if (error) {
		g_warning ("Error getting remote connection '%s': %s", uri, error->message);
		g_error_free (error);
	}

	g_free (uri);

	if (!remote)
		return FALSE;
	if (!g_object_class_find_property (G_OBJECT_GET_CLASS (remote), "bus-name"))
		return FALSE;

	g_object_get (remote,
	              "bus-name", name,
	              "bus-object-path", path,
	              NULL);

	return TRUE;
}

static void
tracker_direct_connection_serialize_async (TrackerSparqlConnection  *self,
                                           TrackerSerializeFlags     flags,
                                           TrackerRdfFormat          format,
                                           const gchar              *query,
                                           GCancellable             *cancellable,
                                           GAsyncReadyCallback      callback,
                                           gpointer                 user_data)
{
	TrackerDirectConnectionPrivate *priv;
	TrackerDirectConnection *conn;
	GError *error = NULL;
	TaskData *task_data;
	GTask *task;

	conn = TRACKER_DIRECT_CONNECTION (self);
	priv = tracker_direct_connection_get_instance_private (conn);

	task_data = task_data_new (TASK_TYPE_SERIALIZE);
	task_data->d.serialize.sparql = g_strdup (query);
	task_data->d.serialize.format = format;
	task_data->d.serialize.flags = flags;

	task = g_task_new (self, cancellable, callback, user_data);
	g_task_set_task_data (task, task_data,
	                      (GDestroyNotify) task_data_free);

	if (!g_thread_pool_push (priv->select_pool, task, &error)) {
		g_task_return_error (task, _translate_internal_error (error));
		g_object_unref (task);
	}
}

static GInputStream *
tracker_direct_connection_serialize_finish (TrackerSparqlConnection  *connection,
                                            GAsyncResult             *res,
                                            GError                  **error)
{
	return g_task_propagate_pointer (G_TASK (res), error);
}

static void
tracker_direct_connection_deserialize_async (TrackerSparqlConnection *self,
                                             TrackerDeserializeFlags  flags,
                                             TrackerRdfFormat         format,
                                             const gchar             *default_graph,
                                             GInputStream            *stream,
                                             GCancellable            *cancellable,
                                             GAsyncReadyCallback      callback,
                                             gpointer                 user_data)
{
	TrackerDirectConnectionPrivate *priv;
	TrackerDirectConnection *conn;
	TaskData *task_data;
	GTask *task;

	conn = TRACKER_DIRECT_CONNECTION (self);
	priv = tracker_direct_connection_get_instance_private (conn);

	task_data = task_data_new (TASK_TYPE_DESERIALIZE);
	task_data->d.deserialize.stream = g_object_ref (stream);
	task_data->d.deserialize.default_graph = g_strdup (default_graph);
	task_data->d.deserialize.format = format;
	task_data->d.deserialize.flags = flags;

	task = g_task_new (self, cancellable, callback, user_data);
	g_task_set_task_data (task, task_data,
	                      (GDestroyNotify) task_data_free);

	g_thread_pool_push (priv->update_thread, task, NULL);
}

static gboolean
tracker_direct_connection_deserialize_finish (TrackerSparqlConnection  *connection,
                                              GAsyncResult             *res,
                                              GError                  **error)
{
	return g_task_propagate_boolean (G_TASK (res), error);
}

static void
tracker_direct_connection_map_connection (TrackerSparqlConnection *connection,
					  const gchar             *handle_name,
					  TrackerSparqlConnection *service_connection)
{
	TrackerDirectConnectionPrivate *priv;
	TrackerDirectConnection *conn;

	conn = TRACKER_DIRECT_CONNECTION (connection);
	priv = tracker_direct_connection_get_instance_private (conn);

	tracker_data_manager_map_connection (priv->data_manager,
	                                     handle_name,
	                                     service_connection);
}

static void
tracker_direct_connection_class_init (TrackerDirectConnectionClass *klass)
{
	TrackerSparqlConnectionClass *sparql_connection_class;
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	sparql_connection_class = TRACKER_SPARQL_CONNECTION_CLASS (klass);

	object_class->finalize = tracker_direct_connection_finalize;
	object_class->set_property = tracker_direct_connection_set_property;
	object_class->get_property = tracker_direct_connection_get_property;

	sparql_connection_class->query = tracker_direct_connection_query;
	sparql_connection_class->query_async = tracker_direct_connection_query_async;
	sparql_connection_class->query_finish = tracker_direct_connection_query_finish;
	sparql_connection_class->query_statement = tracker_direct_connection_query_statement;
	sparql_connection_class->update_statement = tracker_direct_connection_update_statement;
	sparql_connection_class->update = tracker_direct_connection_update;
	sparql_connection_class->update_async = tracker_direct_connection_update_async;
	sparql_connection_class->update_finish = tracker_direct_connection_update_finish;
	sparql_connection_class->update_array_async = tracker_direct_connection_update_array_async;
	sparql_connection_class->update_array_finish = tracker_direct_connection_update_array_finish;
	sparql_connection_class->update_blank = tracker_direct_connection_update_blank;
	sparql_connection_class->update_blank_async = tracker_direct_connection_update_blank_async;
	sparql_connection_class->update_blank_finish = tracker_direct_connection_update_blank_finish;
	sparql_connection_class->get_namespace_manager = tracker_direct_connection_get_namespace_manager;
	sparql_connection_class->create_notifier = tracker_direct_connection_create_notifier;
	sparql_connection_class->close = tracker_direct_connection_close;
	sparql_connection_class->close_async = tracker_direct_connection_close_async;
	sparql_connection_class->close_finish = tracker_direct_connection_close_finish;
	sparql_connection_class->update_resource = tracker_direct_connection_update_resource;
	sparql_connection_class->update_resource_async = tracker_direct_connection_update_resource_async;
	sparql_connection_class->update_resource_finish = tracker_direct_connection_update_resource_finish;
	sparql_connection_class->create_batch = tracker_direct_connection_create_batch;
	sparql_connection_class->lookup_dbus_service = tracker_direct_connection_lookup_dbus_service;
	sparql_connection_class->serialize_async = tracker_direct_connection_serialize_async;
	sparql_connection_class->serialize_finish = tracker_direct_connection_serialize_finish;
	sparql_connection_class->deserialize_async = tracker_direct_connection_deserialize_async;
	sparql_connection_class->deserialize_finish = tracker_direct_connection_deserialize_finish;
	sparql_connection_class->map_connection = tracker_direct_connection_map_connection;

	props[PROP_FLAGS] =
		g_param_spec_flags ("flags",
		                    "Flags",
		                    "Flags",
		                    TRACKER_TYPE_SPARQL_CONNECTION_FLAGS,
		                    TRACKER_SPARQL_CONNECTION_FLAGS_NONE,
		                    G_PARAM_READWRITE |
		                    G_PARAM_CONSTRUCT_ONLY);
	props[PROP_STORE_LOCATION] =
		g_param_spec_object ("store-location",
		                     "Store location",
		                     "Store location",
		                     G_TYPE_FILE,
		                     G_PARAM_READWRITE |
		                     G_PARAM_CONSTRUCT_ONLY);
	props[PROP_ONTOLOGY_LOCATION] =
		g_param_spec_object ("ontology-location",
		                     "Ontology location",
		                     "Ontology location",
		                     G_TYPE_FILE,
		                     G_PARAM_READWRITE |
		                     G_PARAM_CONSTRUCT_ONLY);

	g_object_class_install_properties (object_class, N_PROPS, props);
}

TrackerSparqlConnection *
tracker_direct_connection_new (TrackerSparqlConnectionFlags   flags,
                               GFile                         *store,
                               GFile                         *ontology,
                               GError                       **error)
{
	return g_initable_new (TRACKER_TYPE_DIRECT_CONNECTION,
	                       NULL, error,
	                       "flags", flags,
	                       "store-location", store,
	                       "ontology-location", ontology,
	                       NULL);
}

void
tracker_direct_connection_new_async (TrackerSparqlConnectionFlags  flags,
                                     GFile                        *store,
                                     GFile                        *ontology,
                                     GCancellable                 *cancellable,
                                     GAsyncReadyCallback           cb,
                                     gpointer                      user_data)
{
	g_async_initable_new_async (TRACKER_TYPE_DIRECT_CONNECTION,
	                            G_PRIORITY_DEFAULT,
	                            cancellable,
	                            cb,
	                            user_data,
	                            "flags", flags,
	                            "store-location", store,
	                            "ontology-location", ontology,
	                            NULL);
}

TrackerSparqlConnection *
tracker_direct_connection_new_finish (GAsyncResult  *res,
                                      GError       **error)
{
	GAsyncInitable *initable;

	initable = g_task_get_source_object (G_TASK (res));

	return TRACKER_SPARQL_CONNECTION (g_async_initable_new_finish (initable,
	                                                               res,
	                                                               error));
}

TrackerDataManager *
tracker_direct_connection_get_data_manager (TrackerDirectConnection *conn)
{
	TrackerDirectConnectionPrivate *priv;

	priv = tracker_direct_connection_get_instance_private (conn);
	return priv->data_manager;
}

void
tracker_direct_connection_update_timestamp (TrackerDirectConnection *conn)
{
	TrackerDirectConnectionPrivate *priv;

	priv = tracker_direct_connection_get_instance_private (conn);
	priv->timestamp = g_get_monotonic_time ();
}

gboolean
tracker_direct_connection_update_batch (TrackerDirectConnection  *conn,
                                        TrackerBatch             *batch,
                                        GError                  **error)
{
	TrackerDirectConnectionPrivate *priv;
	GError *inner_error = NULL;

	priv = tracker_direct_connection_get_instance_private (conn);

	g_mutex_lock (&priv->update_mutex);
	tracker_direct_batch_update (TRACKER_DIRECT_BATCH (batch),
	                             priv->data_manager, &inner_error);
	tracker_direct_connection_update_timestamp (conn);
	g_mutex_unlock (&priv->update_mutex);

	if (inner_error) {
		g_propagate_error (error, inner_error);
		return FALSE;
	}

	return TRUE;
}

void
tracker_direct_connection_update_batch_async (TrackerDirectConnection  *conn,
                                              TrackerBatch             *batch,
                                              GCancellable             *cancellable,
                                              GAsyncReadyCallback       callback,
                                              gpointer                  user_data)
{
	TrackerDirectConnectionPrivate *priv;
	TaskData *task_data;
	GTask *task;

	priv = tracker_direct_connection_get_instance_private (conn);

	task_data = task_data_new (TASK_TYPE_UPDATE_BATCH);
	task_data->d.batch = g_object_ref (batch);

	task = g_task_new (batch, cancellable, callback, user_data);
	g_task_set_task_data (task, task_data,
	                      (GDestroyNotify) task_data_free);

	g_thread_pool_push (priv->update_thread, task, NULL);
}

gboolean
tracker_direct_connection_update_batch_finish (TrackerDirectConnection  *conn,
                                               GAsyncResult             *res,
                                               GError                  **error)
{
	GError *inner_error = NULL;

	g_task_propagate_boolean (G_TASK (res), &inner_error);
	if (inner_error) {
		g_propagate_error (error, _translate_internal_error (inner_error));
		return FALSE;
	}

	return TRUE;
}

gboolean
tracker_direct_connection_execute_update_statement (TrackerDirectConnection  *conn,
                                                    TrackerSparqlStatement   *stmt,
                                                    GHashTable               *parameters,
                                                    GError                  **error)
{
	TrackerDirectConnectionPrivate *priv;
	TrackerData *tracker_data;
	GError *inner_error = NULL;

	priv = tracker_direct_connection_get_instance_private (conn);

	g_mutex_lock (&priv->update_mutex);

	tracker_data = tracker_data_manager_get_data (priv->data_manager);
	if (!tracker_data_begin_transaction (tracker_data, &inner_error))
		goto out;

	if (tracker_direct_statement_execute_update (stmt, parameters, NULL, &inner_error)) {
		if (tracker_data_commit_transaction (tracker_data, &inner_error))
			tracker_direct_connection_update_timestamp (conn);
	} else {
		tracker_data_rollback_transaction (tracker_data);
	}

 out:
	g_mutex_unlock (&priv->update_mutex);

	if (inner_error) {
		g_propagate_error (error, inner_error);
		return FALSE;
	}

	return TRUE;
}

void
tracker_direct_connection_execute_query_statement_async (TrackerDirectConnection *conn,
                                                         TrackerSparqlStatement  *stmt,
                                                         GHashTable              *parameters,
                                                         GCancellable            *cancellable,
                                                         GAsyncReadyCallback      callback,
                                                         gpointer                 user_data)
{
	TrackerDirectConnectionPrivate *priv =
		tracker_direct_connection_get_instance_private (conn);
	GError *error = NULL;
	TaskData *task_data;
	GTask *task;

	task_data = task_data_new (TASK_TYPE_QUERY_STATEMENT);
	task_data->d.statement.stmt = g_object_ref (stmt);
	task_data->d.statement.parameters =
		parameters ? g_hash_table_ref (parameters) : NULL;

	task = g_task_new (conn, cancellable, callback, user_data);
	g_task_set_task_data (task, task_data,
	                      (GDestroyNotify) task_data_free);

	if (!g_thread_pool_push (priv->select_pool, task, &error)) {
		g_task_return_error (task, _translate_internal_error (error));
		g_object_unref (task);
	}
}

TrackerSparqlCursor *
tracker_direct_connection_execute_query_statement_finish (TrackerDirectConnection  *conn,
                                                          GAsyncResult             *res,
                                                          GError                  **error)
{
	return g_task_propagate_pointer (G_TASK (res), error);
}

void
tracker_direct_connection_execute_serialize_statement_async (TrackerDirectConnection *conn,
                                                             TrackerSparqlStatement  *stmt,
                                                             GHashTable              *parameters,
                                                             TrackerSerializeFlags    flags,
                                                             TrackerRdfFormat         format,
                                                             GCancellable            *cancellable,
                                                             GAsyncReadyCallback      callback,
                                                             gpointer                 user_data)
{
	TrackerDirectConnectionPrivate *priv =
		tracker_direct_connection_get_instance_private (conn);
	GError *error = NULL;
	TaskData *task_data;
	GTask *task;

	task_data = task_data_new (TASK_TYPE_SERIALIZE_STATEMENT);
	task_data->d.serialize_statement.stmt = g_object_ref (stmt);
	task_data->d.serialize_statement.parameters =
		parameters ? g_hash_table_ref (parameters) : NULL;
	task_data->d.serialize_statement.flags = flags;
	task_data->d.serialize_statement.format = format;

	task = g_task_new (conn, cancellable, callback, user_data);
	g_task_set_task_data (task, task_data,
	                      (GDestroyNotify) task_data_free);

	if (!g_thread_pool_push (priv->select_pool, task, &error)) {
		g_task_return_error (task, _translate_internal_error (error));
		g_object_unref (task);
	}
}

GInputStream *
tracker_direct_connection_execute_serialize_statement_finish (TrackerDirectConnection  *conn,
                                                              GAsyncResult             *res,
                                                              GError                  **error)
{
	return g_task_propagate_pointer (G_TASK (res), error);
}


void
tracker_direct_connection_execute_update_statement_async (TrackerDirectConnection  *conn,
                                                          TrackerSparqlStatement   *stmt,
                                                          GHashTable               *parameters,
                                                          GCancellable             *cancellable,
                                                          GAsyncReadyCallback       callback,
                                                          gpointer                  user_data)
{
	TrackerDirectConnectionPrivate *priv;
	TaskData *task_data;
	GTask *task;

	priv = tracker_direct_connection_get_instance_private (conn);

	task_data = task_data_new (TASK_TYPE_UPDATE_STATEMENT);
	task_data->d.statement.stmt = g_object_ref (stmt);
	task_data->d.statement.parameters =
		parameters ? g_hash_table_ref (parameters) : NULL;

	task = g_task_new (stmt, cancellable, callback, user_data);
	g_task_set_task_data (task, task_data,
	                      (GDestroyNotify) task_data_free);

	g_thread_pool_push (priv->update_thread, task, NULL);
}

gboolean
tracker_direct_connection_execute_update_statement_finish (TrackerDirectConnection  *conn,
                                                           GAsyncResult             *res,
                                                           GError                  **error)
{
	GError *inner_error = NULL;

	g_task_propagate_boolean (G_TASK (res), &inner_error);
	if (inner_error) {
		g_propagate_error (error, _translate_internal_error (inner_error));
		return FALSE;
	}

	return TRUE;
}
