/*
 * Copyright (C) 2016-2018 Red Hat Inc.
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

/**
 * TrackerNotifier:
 *
 * `TrackerNotifier` allows receiving notification on changes
 * in the data stored by a [class@SparqlConnection].
 *
 * This object may be created through [method@SparqlConnection.create_notifier],
 * events can then be listened for by connecting to the
 * [signal@Notifier::events] signal.
 *
 * Not every change is notified, only RDF resources with a
 * class that has the [nrl:notify](nrl-ontology.html#nrl:notify)
 * property defined by the ontology will be notified upon changes.
 *
 * Database changes are communicated through [struct@NotifierEvent] events on
 * individual graph/resource pairs. The event type obtained through
 * [method@NotifierEvent.get_event_type] will determine the type of event.
 * Insertion of new resources is notified through
 * %TRACKER_NOTIFIER_EVENT_CREATE events, deletion of
 * resources is notified through %TRACKER_NOTIFIER_EVENT_DELETE
 * events, and changes on any property of the resource is notified
 * through %TRACKER_NOTIFIER_EVENT_UPDATE events.
 *
 * The events happen in reaction to database changes, after a `TrackerNotifier`
 * received an event of type %TRACKER_NOTIFIER_EVENT_DELETE, the resource will
 * not exist anymore and only the information in the [struct@NotifierEvent]
 * will remain.
 *
 * Similarly, when receiving an event of type %TRACKER_NOTIFIER_EVENT_UPDATE,
 * the resource will have already changed, so the data previous to the update is
 * no longer available.
 *
 * The [signal@Notifier::events] signal is emitted in the thread-default
 * main context of the thread where the `TrackerNotifier` instance was created.
 */

#include "config.h"

#include "tracker-connection.h"
#include "tracker-notifier.h"
#include "tracker-notifier-private.h"
#include "tracker-private.h"
#include "tracker-sparql-enum-types.h"
#include <direct/tracker-direct.h>

typedef struct _TrackerNotifierPrivate TrackerNotifierPrivate;
typedef struct _TrackerNotifierSubscription TrackerNotifierSubscription;

struct _TrackerNotifierSubscription {
	GDBusConnection *connection;
	TrackerNotifier *notifier;
	TrackerSparqlStatement *statement;
	gint n_statement_slots;
	gchar *service;
	gchar *object_path;
	guint handler_id;
};

struct _TrackerNotifierPrivate {
	TrackerSparqlConnection *connection;
	GHashTable *subscriptions; /* guint -> TrackerNotifierSubscription */
	GCancellable *cancellable;
	TrackerSparqlStatement *local_statement;
	GAsyncQueue *queue;
	GMainContext *main_context;
	gint n_local_statement_slots;
	guint querying : 1;
	guint urn_query_disabled : 1;
	GMutex mutex;
};

struct _TrackerNotifierEventCache {
	gchar *service;
	gchar *graph;
	GWeakRef notifier;
	GCancellable *cancellable;
	TrackerSparqlStatement *stmt;
	GPtrArray *events;
	GHashTable *events_by_id;
	guint first;
};

struct _TrackerNotifierEvent {
	gint8 type;
	gint64 id;
	gchar *urn;
	guint ref_count;
};

enum {
	PROP_0,
	PROP_CONNECTION,
	N_PROPS
};

enum {
	EVENTS,
	N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

#define N_SLOTS 50 /* In sync with tracker-vtab-service.c parameters */

#define DEFAULT_OBJECT_PATH "/org/freedesktop/Tracker3/Endpoint"

G_DEFINE_TYPE_WITH_CODE (TrackerNotifier, tracker_notifier, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (TrackerNotifier))

static void tracker_notifier_query_extra_info (TrackerNotifier           *notifier,
                                               TrackerNotifierEventCache *cache);

static gchar * get_service_name (TrackerNotifier             *notifier,
                                 TrackerNotifierSubscription *subscription);

static TrackerSparqlStatement * ensure_extra_info_statement (TrackerNotifier             *notifier,
                                                             TrackerNotifierSubscription *subscription);

static TrackerNotifierSubscription *
tracker_notifier_subscription_new (TrackerNotifier *notifier,
                                   GDBusConnection *connection,
                                   const gchar     *service,
                                   const gchar     *object_path)
{
	TrackerNotifierSubscription *subscription;

	subscription = g_new0 (TrackerNotifierSubscription, 1);
	subscription->connection = g_object_ref (connection);
	subscription->notifier = notifier;
	subscription->service = g_strdup (service);
	subscription->object_path = g_strdup (object_path);

	return subscription;
}

static void
tracker_notifier_subscription_free (TrackerNotifierSubscription *subscription)
{
	g_dbus_connection_signal_unsubscribe (subscription->connection,
	                                      subscription->handler_id);
	g_object_unref (subscription->connection);
	g_clear_object (&subscription->statement);
	g_free (subscription->service);
	g_free (subscription->object_path);
	g_free (subscription);
}

static TrackerNotifierEvent *
tracker_notifier_event_new (gint64 id)
{
	TrackerNotifierEvent *event;

	event = g_new0 (TrackerNotifierEvent, 1);
	event->type = -1;
	event->id = id;
	event->ref_count = 1;
	return event;
}

static TrackerNotifierEvent *
tracker_notifier_event_ref (TrackerNotifierEvent *event)
{
	g_atomic_int_inc (&event->ref_count);
	return event;
}

static void
tracker_notifier_event_unref (TrackerNotifierEvent *event)
{
	if (g_atomic_int_dec_and_test (&event->ref_count)) {
		g_free (event->urn);
		g_free (event);
	}
}

G_DEFINE_BOXED_TYPE (TrackerNotifierEvent,
                     tracker_notifier_event,
                     tracker_notifier_event_ref,
                     tracker_notifier_event_unref)

static TrackerNotifierEventCache *
_tracker_notifier_event_cache_new_full (TrackerNotifier             *notifier,
                                        TrackerNotifierSubscription *subscription,
                                        const gchar                 *graph)
{
	TrackerNotifierEventCache *event_cache;
	TrackerNotifierPrivate *priv;

	priv = tracker_notifier_get_instance_private (notifier);

	event_cache = g_new0 (TrackerNotifierEventCache, 1);
	g_weak_ref_init (&event_cache->notifier, notifier);
	event_cache->graph = g_strdup (graph);
	event_cache->cancellable = g_object_ref (priv->cancellable);
	event_cache->events = g_ptr_array_new_with_free_func ((GDestroyNotify) tracker_notifier_event_unref);
	event_cache->events_by_id = g_hash_table_new (g_int64_hash, g_int64_equal);
	event_cache->stmt = ensure_extra_info_statement (notifier, subscription);

	if (subscription)
		event_cache->service = get_service_name (notifier, subscription);

	return event_cache;
}

TrackerNotifierEventCache *
_tracker_notifier_event_cache_new (TrackerNotifier *notifier,
                                   const gchar     *graph)
{
	return _tracker_notifier_event_cache_new_full (notifier, NULL, graph);
}

void
_tracker_notifier_event_cache_free (TrackerNotifierEventCache *event_cache)
{
	g_hash_table_unref (event_cache->events_by_id);
	g_clear_pointer (&event_cache->events, g_ptr_array_unref);
	g_weak_ref_clear (&event_cache->notifier);
	g_object_unref (event_cache->cancellable);
	g_free (event_cache->service);
	g_free (event_cache->graph);
	g_free (event_cache);
}

/* This is always meant to return a pointer */
static TrackerNotifierEvent *
tracker_notifier_event_cache_get_event (TrackerNotifierEventCache *cache,
                                        gint64                     id)
{
	TrackerNotifierEvent *event;

	event = g_hash_table_lookup (cache->events_by_id, &id);

	if (!event) {
		g_assert (cache->events != NULL);
		event = tracker_notifier_event_new (id);
		g_ptr_array_add (cache->events, event);
		g_hash_table_insert (cache->events_by_id,
		                     &event->id, event);
	}

	return event;
}

void
_tracker_notifier_event_cache_push_event (TrackerNotifierEventCache *cache,
                                          gint64                     id,
                                          TrackerNotifierEventType   event_type)
{
	TrackerNotifierEvent *event;

	event = tracker_notifier_event_cache_get_event (cache, id);

	if (event->type < 0 || event_type != TRACKER_NOTIFIER_EVENT_UPDATE)
		event->type = event_type;
}

const gchar *
tracker_notifier_event_cache_get_graph (TrackerNotifierEventCache *cache)
{
	return cache->graph ? cache->graph : "";
}

static void
handle_events (TrackerNotifier           *notifier,
               TrackerNotifierEventCache *cache,
               GVariantIter              *iter)
{
	gint32 type, resource;

	while (g_variant_iter_loop (iter, "{ii}", &type, &resource))
		_tracker_notifier_event_cache_push_event (cache, resource, type);
}

static GPtrArray *
tracker_notifier_event_cache_take_events (TrackerNotifierEventCache *cache)
{
	GPtrArray *events;

	g_hash_table_remove_all (cache->events_by_id);
	events = g_steal_pointer (&cache->events);

	return events;
}

static gchar *
compose_uri (const gchar *service,
             const gchar *object_path)
{
	if (object_path && g_strcmp0 (object_path, DEFAULT_OBJECT_PATH) != 0)
		return g_strdup_printf ("dbus:%s:%s", service, object_path);
	else
		return g_strdup_printf ("dbus:%s", service);
}

static gchar *
get_service_name (TrackerNotifier             *notifier,
                  TrackerNotifierSubscription *subscription)
{
	TrackerNotifierPrivate *priv;

	priv = tracker_notifier_get_instance_private (notifier);

	if (!subscription)
		return NULL;

	/* This is a hackish way to find out we are dealing with DBus connections,
	 * without pulling its header.
	 */
	if (g_object_class_find_property (G_OBJECT_GET_CLASS (priv->connection), "bus-name")) {
		gchar *bus_name, *bus_object_path;
		gboolean is_self;

		g_object_get (priv->connection,
		              "bus-name", &bus_name,
		              "bus-object-path", &bus_object_path,
		              NULL);

		is_self = (g_strcmp0 (bus_name, subscription->service) == 0 &&
		           g_strcmp0 (bus_object_path, subscription->object_path) == 0);
		g_free (bus_name);
		g_free (bus_object_path);

		if (is_self)
			return NULL;
	}

	return compose_uri (subscription->service, subscription->object_path);
}

static gboolean
tracker_notifier_emit_events (TrackerNotifierEventCache *cache)
{
	TrackerNotifier *notifier;
	GPtrArray *events;

	notifier = g_weak_ref_get (&cache->notifier);
	if (!notifier)
		return G_SOURCE_REMOVE;

	events = tracker_notifier_event_cache_take_events (cache);

	if (events) {
		g_signal_emit (notifier, signals[EVENTS], 0,
		               cache->service, cache->graph, events);
		g_ptr_array_unref (events);
	}

	g_object_unref (notifier);

	return G_SOURCE_REMOVE;
}

static void
tracker_notifier_emit_events_in_idle (TrackerNotifier           *notifier,
                                      TrackerNotifierEventCache *cache)
{
	TrackerNotifierPrivate *priv;
	GSource *source;

	priv = tracker_notifier_get_instance_private (notifier);

	source = g_idle_source_new ();
	g_source_set_callback (source,
			       (GSourceFunc) tracker_notifier_emit_events,
			       cache,
			       (GDestroyNotify) _tracker_notifier_event_cache_free);
	g_source_attach (source, priv->main_context);
	g_source_unref (source);
}

static gchar *
create_extra_info_query (TrackerNotifier             *notifier,
                         TrackerNotifierSubscription *subscription)
{
	GString *sparql;
	gchar *service;
	gint i;

	sparql = g_string_new ("SELECT ?id ?uri ");

	service = get_service_name (notifier, subscription);

	if (service) {
		g_string_append_printf (sparql,
		                        "{ SERVICE <%s> ",
		                        service);
	}

	g_string_append (sparql, "{ VALUES ?id { ");

	for (i = 0; i < N_SLOTS; i++) {
		g_string_append_printf (sparql, "~arg%d ", i + 1);
	}

	g_string_append (sparql,
	                 "  } ."
	                 "  BIND (tracker:uri(xsd:integer(?id)) AS ?uri) ."
	                 "  FILTER (?id > 0) ."
	                 "} ");

	if (service)
		g_string_append (sparql, "} ");

	g_free (service);

	return g_string_free (sparql, FALSE);
}

static TrackerSparqlStatement *
ensure_extra_info_statement (TrackerNotifier             *notifier,
                             TrackerNotifierSubscription *subscription)
{
	TrackerSparqlStatement **ptr;
	TrackerNotifierPrivate *priv;
	gchar *sparql;
	GError *error = NULL;

	priv = tracker_notifier_get_instance_private (notifier);

	if (subscription) {
		ptr = &subscription->statement;
	} else {
		ptr = &priv->local_statement;
	}

	if (*ptr) {
		return *ptr;
	}

	sparql = create_extra_info_query (notifier, subscription);
	*ptr = tracker_sparql_connection_query_statement (priv->connection,
	                                                  sparql,
	                                                  priv->cancellable,
	                                                  &error);
	g_free (sparql);

	if (error) {
		g_warning ("Error querying notifier info: %s\n", error->message);
		g_error_free (error);
		return NULL;
	}

	return *ptr;
}

static void
handle_cursor (GTask        *task,
	       gpointer      source_object,
	       gpointer      task_data,
	       GCancellable *cancellable)
{
	TrackerNotifierEventCache *cache = task_data;
	TrackerSparqlCursor *cursor = source_object;
	TrackerNotifier *notifier;
	TrackerNotifierPrivate *priv;
	TrackerNotifierEvent *event;
	gint64 id;

	while (tracker_sparql_cursor_next (cursor, cancellable, NULL)) {
		id = tracker_sparql_cursor_get_integer (cursor, 0);
		event = g_ptr_array_index (cache->events, cache->first);

		if (!event || event->id != id) {
			g_critical ("Queried for id %" G_GINT64_FORMAT " but it is not "
			            "found, bailing out", id);
			break;
		}

		event->urn = g_strdup (tracker_sparql_cursor_get_string (cursor, 1, NULL));
		cache->first++;
	}

	tracker_sparql_cursor_close (cursor);

	if (g_task_return_error_if_cancelled (task)) {
		_tracker_notifier_event_cache_free (cache);
		return;
	}

	notifier = g_weak_ref_get (&cache->notifier);
	if (!notifier) {
		_tracker_notifier_event_cache_free (cache);
		return;
	}

	priv = tracker_notifier_get_instance_private (notifier);

	if (cache->first >= cache->events->len) {
		TrackerNotifierEventCache *next;

		tracker_notifier_emit_events_in_idle (notifier, cache);

		g_async_queue_lock (priv->queue);
		next = g_async_queue_try_pop_unlocked (priv->queue);
		if (next)
			tracker_notifier_query_extra_info (notifier, next);
		else
			priv->querying = FALSE;
		g_async_queue_unlock (priv->queue);
	} else {
		tracker_notifier_query_extra_info (notifier, cache);
	}

	g_task_return_boolean (task, TRUE);
	g_object_unref (notifier);
}

static void
finish_query (GObject      *source_object,
              GAsyncResult *res,
              gpointer      user_data)
{
	TrackerSparqlCursor *cursor = TRACKER_SPARQL_CURSOR (source_object);
	GError *error = NULL;

	if (!g_task_propagate_boolean (G_TASK (res), &error)) {
		if (!g_error_matches (error,
				      G_IO_ERROR,
				      G_IO_ERROR_CANCELLED)) {
			g_critical ("Error querying notified data: %s\n", error->message);
		}
	}

	g_object_unref (cursor);
	g_clear_error (&error);
}

static void
query_extra_info_cb (GObject      *object,
                     GAsyncResult *res,
                     gpointer      user_data)
{
	TrackerNotifierEventCache *cache = user_data;
	TrackerSparqlStatement *statement;
	TrackerSparqlCursor *cursor;
	GError *error = NULL;
	GTask *task;

	statement = TRACKER_SPARQL_STATEMENT (object);
	cursor = tracker_sparql_statement_execute_finish (statement, res, &error);

	if (!cursor) {
		if (!g_error_matches (error,
				      G_IO_ERROR,
				      G_IO_ERROR_CANCELLED)) {
			g_critical ("Could not get cursor: %s\n", error->message);
		}

		_tracker_notifier_event_cache_free (cache);
		g_clear_error (&error);
		return;
	}

	task = g_task_new (cursor, cache->cancellable, finish_query, NULL);
	g_task_set_task_data (task, cache, NULL);
	g_task_run_in_thread (task, handle_cursor);
	g_object_unref (task);
}

static void
bind_arguments (TrackerSparqlStatement    *statement,
                TrackerNotifierEventCache *cache)
{
	gchar *arg_name;
	guint i = 0, n_args = 0;

	tracker_sparql_statement_clear_bindings (statement);

	for (i = cache->first;
	     (n_args < N_SLOTS && i < cache->events->len);
	     i++) {
		TrackerNotifierEvent *event;

		event = g_ptr_array_index (cache->events, i);

		arg_name = g_strdup_printf ("arg%d", n_args + 1);
		tracker_sparql_statement_bind_int (statement, arg_name, event->id);
		g_free (arg_name);
		n_args++;
	}

	/* Fill in missing slots with 0's */
	while (n_args < N_SLOTS) {
		arg_name = g_strdup_printf ("arg%d", n_args + 1);
		tracker_sparql_statement_bind_int (statement, arg_name, 0);
		g_free (arg_name);
		n_args++;
	}
}

static void
tracker_notifier_query_extra_info (TrackerNotifier           *notifier,
                                   TrackerNotifierEventCache *cache)
{
	TrackerNotifierPrivate *priv;

	priv = tracker_notifier_get_instance_private (notifier);

	g_mutex_lock (&priv->mutex);

	bind_arguments (cache->stmt, cache);
	tracker_sparql_statement_execute_async (cache->stmt,
	                                        cache->cancellable,
	                                        query_extra_info_cb,
	                                        cache);

	g_mutex_unlock (&priv->mutex);
}

void
_tracker_notifier_event_cache_flush_events (TrackerNotifier           *notifier,
                                            TrackerNotifierEventCache *cache)
{
	TrackerNotifierPrivate *priv = tracker_notifier_get_instance_private (notifier);

	if (cache->events->len == 0) {
		_tracker_notifier_event_cache_free (cache);
		return;
	}

	cache->first = 0;

	g_async_queue_lock (priv->queue);
	if (priv->urn_query_disabled) {
		tracker_notifier_emit_events_in_idle (notifier, cache);
	} else if (priv->querying) {
		g_async_queue_push_unlocked (priv->queue, cache);
	} else {
		priv->querying = TRUE;
		tracker_notifier_query_extra_info (notifier, cache);
	}
	g_async_queue_unlock (priv->queue);
}

void
tracker_notifier_stop (TrackerNotifier *notifier)
{
	TrackerNotifierPrivate *priv = tracker_notifier_get_instance_private (notifier);

	g_cancellable_cancel (priv->cancellable);
}

static void
graph_updated_cb (GDBusConnection *connection,
                  const gchar     *sender_name,
                  const gchar     *object_path,
                  const gchar     *interface_name,
                  const gchar     *signal_name,
                  GVariant        *parameters,
                  gpointer         user_data)
{
	TrackerNotifierSubscription *subscription = user_data;
	TrackerNotifier *notifier = subscription->notifier;
	TrackerNotifierPrivate *priv =
		tracker_notifier_get_instance_private (notifier);
	TrackerNotifierEventCache *cache;
	GVariantIter *events;
	const gchar *graph;

	if (g_cancellable_is_cancelled (priv->cancellable))
		return;

	g_variant_get (parameters, "(&sa{ii})", &graph, &events);

	cache = _tracker_notifier_event_cache_new_full (notifier, subscription, graph);
	handle_events (notifier, cache, events);
	g_variant_iter_free (events);

	_tracker_notifier_event_cache_flush_events (notifier, cache);
}

static void
tracker_notifier_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
	TrackerNotifier *notifier = TRACKER_NOTIFIER (object);
	TrackerNotifierPrivate *priv = tracker_notifier_get_instance_private (notifier);

	switch (prop_id) {
	case PROP_CONNECTION:
		priv->connection = g_value_dup_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
tracker_notifier_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
	TrackerNotifier *notifier = TRACKER_NOTIFIER (object);
	TrackerNotifierPrivate *priv = tracker_notifier_get_instance_private (notifier);

	switch (prop_id) {
	case PROP_CONNECTION:
		g_value_set_object (value, priv->connection);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
tracker_notifier_finalize (GObject *object)
{
	TrackerNotifierPrivate *priv;

	priv = tracker_notifier_get_instance_private (TRACKER_NOTIFIER (object));

	g_cancellable_cancel (priv->cancellable);
	g_clear_object (&priv->cancellable);
	g_clear_object (&priv->local_statement);
	g_async_queue_unref (priv->queue);

	if (priv->connection)
		g_object_unref (priv->connection);

	g_hash_table_unref (priv->subscriptions);

	G_OBJECT_CLASS (tracker_notifier_parent_class)->finalize (object);
}

static void
tracker_notifier_class_init (TrackerNotifierClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GParamSpec *pspecs[N_PROPS] = { 0 };

	object_class->set_property = tracker_notifier_set_property;
	object_class->get_property = tracker_notifier_get_property;
	object_class->finalize = tracker_notifier_finalize;

	/**
	 * TrackerNotifier::events:
	 * @self: The `TrackerNotifier`
	 * @service: The SPARQL service that originated the events, %NULL for the local store
	 * @graph: The graph where the events happened on, %NULL for the default anonymous graph
	 * @events: (transfer none) (type GLib.PtrArray) (element-type TrackerNotifierEvent): A [type@GLib.PtrArray] of [struct@NotifierEvent]
	 *
	 * Notifies of changes in the Tracker database.
	 */
	signals[EVENTS] =
		g_signal_new ("events",
		              TRACKER_TYPE_NOTIFIER, 0,
		              G_STRUCT_OFFSET (TrackerNotifierClass, events),
		              NULL, NULL, NULL,
		              G_TYPE_NONE, 3,
		              G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
		              G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
		              G_TYPE_PTR_ARRAY | G_SIGNAL_TYPE_STATIC_SCOPE);

	/**
	 * TrackerNotifier:connection:
	 *
	 * SPARQL connection to listen to.
	 */
	pspecs[PROP_CONNECTION] =
		g_param_spec_object ("connection",
		                     "SPARQL connection",
		                     "SPARQL connection",
		                     TRACKER_SPARQL_TYPE_CONNECTION,
		                     G_PARAM_READWRITE |
		                     G_PARAM_STATIC_STRINGS |
		                     G_PARAM_CONSTRUCT_ONLY);

	g_object_class_install_properties (object_class, N_PROPS, pspecs);
}

static void
tracker_notifier_init (TrackerNotifier *notifier)
{
	TrackerNotifierPrivate *priv;

	priv = tracker_notifier_get_instance_private (notifier);
	priv->subscriptions = g_hash_table_new_full (NULL, NULL, NULL,
	                                             (GDestroyNotify) tracker_notifier_subscription_free);
	priv->cancellable = g_cancellable_new ();
	priv->queue = g_async_queue_new ();
	priv->main_context = g_main_context_get_thread_default ();
}

/**
 * tracker_notifier_signal_subscribe:
 * @notifier: A `TrackerNotifier`
 * @connection: A [class@Gio.DBusConnection]
 * @service: (nullable): DBus service name to subscribe to events for, or %NULL
 * @object_path: (nullable): DBus object path to subscribe to events for, or %NULL
 * @graph: (nullable): Graph to listen events for, or %NULL
 *
 * Listens to notification events from a remote DBus SPARQL endpoint.
 *
 * If @connection refers to a message bus (system/session), @service must refer
 * to a D-Bus name (either unique or well-known). If @connection is a non-message
 * bus (e.g. a peer-to-peer D-Bus connection) the @service argument may be %NULL.
 *
 * If the @object_path argument is %NULL, the default
 * `/org/freedesktop/Tracker3/Endpoint` path will be
 * used. If @graph is %NULL, all graphs will be listened for.
 *
 * The signal subscription can be removed with
 * [method@Notifier.signal_unsubscribe].
 *
 * Note that this call is not necessary to receive notifications on
 * a connection obtained through [ctor@SparqlConnection.bus_new],
 * only to listen to update notifications from additional DBus endpoints.
 *
 * Returns: An ID for this subscription
 **/
guint
tracker_notifier_signal_subscribe (TrackerNotifier *notifier,
                                   GDBusConnection *connection,
                                   const gchar     *service,
                                   const gchar     *object_path,
                                   const gchar     *graph)
{
	TrackerNotifierSubscription *subscription;
	TrackerNotifierPrivate *priv;
	gchar *dbus_name = NULL, *dbus_path = NULL, *full_graph = NULL;

	g_return_val_if_fail (TRACKER_IS_NOTIFIER (notifier), 0);
	g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), 0);
	g_return_val_if_fail ((service == NULL &&
	                       (g_dbus_connection_get_flags (connection) & G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION) == 0) ||
	                      (service != NULL && g_dbus_is_name (service)), 0);

	priv = tracker_notifier_get_instance_private (notifier);

	if (!object_path)
		object_path = DEFAULT_OBJECT_PATH;

	if (graph) {
		TrackerNamespaceManager *namespaces;

		namespaces = tracker_sparql_connection_get_namespace_manager (priv->connection);
		if (namespaces) {
			full_graph = tracker_namespace_manager_expand_uri (namespaces,
			                                                   graph);
		}
	}

	tracker_sparql_connection_lookup_dbus_service (priv->connection,
	                                               service,
	                                               object_path,
	                                               &dbus_name,
	                                               &dbus_path);

	subscription = tracker_notifier_subscription_new (notifier, connection,
	                                                  service, object_path);

	subscription->handler_id =
		g_dbus_connection_signal_subscribe (connection,
		                                    dbus_name ? dbus_name : service,
		                                    "org.freedesktop.Tracker3.Endpoint",
		                                    "GraphUpdated",
		                                    dbus_path ? dbus_path : object_path,
		                                    full_graph ? full_graph : graph,
		                                    G_DBUS_SIGNAL_FLAGS_NONE,
		                                    graph_updated_cb,
		                                    subscription, NULL);

	g_hash_table_insert (priv->subscriptions,
	                     GUINT_TO_POINTER (subscription->handler_id),
	                     subscription);

	g_free (dbus_name);
	g_free (dbus_path);
	g_free (full_graph);

	return subscription->handler_id;
}

/**
 * tracker_notifier_signal_unsubscribe:
 * @notifier: A `TrackerNotifier`
 * @handler_id: A signal subscription handler ID
 *
 * Undoes a signal subscription done through [method@Notifier.signal_subscribe].
 *
 * The @handler_id argument was previously obtained during signal subscription creation.
 **/
void
tracker_notifier_signal_unsubscribe (TrackerNotifier *notifier,
                                     guint            handler_id)
{
	TrackerNotifierPrivate *priv;

	g_return_if_fail (TRACKER_IS_NOTIFIER (notifier));
	g_return_if_fail (handler_id != 0);

	priv = tracker_notifier_get_instance_private (notifier);

	g_hash_table_remove (priv->subscriptions, GUINT_TO_POINTER (handler_id));
}

gpointer
_tracker_notifier_get_connection (TrackerNotifier *notifier)
{
	TrackerNotifierPrivate *priv;

	priv = tracker_notifier_get_instance_private (notifier);

	return priv->connection;
}

/**
 * tracker_notifier_event_get_event_type:
 * @event: A `TrackerNotifierEvent`
 *
 * Returns the event type.
 *
 * Returns: The event type
 **/
TrackerNotifierEventType
tracker_notifier_event_get_event_type (TrackerNotifierEvent *event)
{
	g_return_val_if_fail (event != NULL, -1);
	return event->type;
}

/**
 * tracker_notifier_event_get_id:
 * @event: A `TrackerNotifierEvent`
 *
 * Returns the tracker:id of the element being notified upon. This is a #gint64
 * which is used as efficient internal identifier for the resource.
 *
 * Returns: the resource ID
 **/
gint64
tracker_notifier_event_get_id (TrackerNotifierEvent *event)
{
	g_return_val_if_fail (event != NULL, 0);
	return event->id;
}

/**
 * tracker_notifier_event_get_urn:
 * @event: A `TrackerNotifierEvent`
 *
 * Returns the Uniform Resource Name of the element. This is Tracker's
 * public identifier for the resource.
 *
 * This URN is an unique string identifier for the resource being
 * notified upon, typically of the form `urn:uuid:...`.
 *
 * Returns: The element URN
 **/
const gchar *
tracker_notifier_event_get_urn (TrackerNotifierEvent *event)
{
	g_return_val_if_fail (event != NULL, NULL);
	return event->urn;
}

void
tracker_notifier_disable_urn_query (TrackerNotifier *notifier)
{
	TrackerNotifierPrivate *priv;

	priv = tracker_notifier_get_instance_private (notifier);
	priv->urn_query_disabled = TRUE;
}
