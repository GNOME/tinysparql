/*
 * Copyright (C) 2014 Carlos Garnacho  <carlosg@gnome.org>
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
#include "tracker-decorator.h"

#define QUERY_BATCH_SIZE 100
#define DEFAULT_BATCH_SIZE 100
#define TRACKER_DECORATOR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_DECORATOR, TrackerDecoratorPrivate))

typedef struct _TrackerDecoratorPrivate TrackerDecoratorPrivate;
typedef struct _TaskData TaskData;
typedef struct _ElemNode ElemNode;

struct _TrackerDecoratorInfo {
	GTask *task;
	gchar *urn;
	gchar *url;
	gchar *mimetype;
	gint ref_count;
};

struct _TaskData {
	TrackerDecorator *decorator;
	GList *first;
	GList *last;
};

struct _ElemNode {
	TrackerDecoratorInfo *info;
	gint id;
};

struct _TrackerDecoratorPrivate {
	guint graph_updated_signal_id;
	gchar *data_source;
	GStrv class_names;

	GQueue *elem_queue;
	GHashTable *elems;
	GPtrArray *sparql_buffer;
	GTimer *timer;

	GArray *class_name_ids;
	gint rdf_type_id;
	gint nie_data_source_id;
	gint data_source_id;
	gint batch_size;

	gint stats_n_elems;
};

enum {
	PROP_DATA_SOURCE = 1,
	PROP_CLASS_NAMES,
	PROP_COMMIT_BATCH_SIZE
};

enum {
	ITEMS_AVAILABLE,
	FINISHED,
	LAST_SIGNAL
};

typedef enum {
	TRACKER_DECORATOR_ERROR_EMPTY,
	TRACKER_DECORATOR_ERROR_PAUSED
} TrackerDecoratorError;

static guint signals[LAST_SIGNAL] = { 0 };
static GInitableIface *parent_initable_iface;

static GQuark tracker_decorator_error_quark         (void);
static void   tracker_decorator_initable_iface_init (GInitableIface   *iface);
static void   query_next_items                      (TrackerDecorator *decorator,
                                                     GTask            *task);

G_DEFINE_QUARK (TrackerDecoratorError, tracker_decorator_error)

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (TrackerDecorator, tracker_decorator, TRACKER_TYPE_MINER,
                                  G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, tracker_decorator_initable_iface_init))

static TrackerDecoratorInfo *
tracker_decorator_info_new (TrackerSparqlCursor *cursor)
{
	TrackerDecoratorInfo *info;

	info = g_new0 (TrackerDecoratorInfo, 1);
	info->urn = g_strdup (tracker_sparql_cursor_get_string (cursor, 0, NULL));
	info->url = g_strdup (tracker_sparql_cursor_get_string (cursor, 2, NULL));
	info->mimetype = g_strdup (tracker_sparql_cursor_get_string (cursor, 3, NULL));
	info->ref_count = 1;

	return info;
}

static TrackerDecoratorInfo *
tracker_decorator_info_ref (TrackerDecoratorInfo *info)
{
	g_atomic_int_inc (&info->ref_count);
	return info;
}

static void
tracker_decorator_info_unref (TrackerDecoratorInfo *info)
{
	if (!g_atomic_int_dec_and_test (&info->ref_count))
		return;

	if (info->task)
		g_object_unref (info->task);
	g_free (info->urn);
	g_free (info->url);
	g_free (info->mimetype);
	g_free (info);
}

static TaskData *
task_data_new (TrackerDecorator *decorator,
               GList            *first,
               GList            *last)
{
	TaskData *data;

	data = g_new0 (TaskData, 1);
	data->decorator = decorator;
	data->first = first;
	data->last = last;

	return data;
}

static void
decorator_update_state (TrackerDecorator *decorator,
                        const gchar      *message,
                        gboolean          estimate_time)
{
	TrackerDecoratorPrivate *priv;
	gint remaining_time = -1;
	gdouble progress = 1;

	priv = decorator->priv;

	if (priv->elem_queue->length > 0) {
		progress = 1 - ((gdouble) priv->elem_queue->length /
		                priv->stats_n_elems);
		remaining_time = 0;
	}

	if (priv->timer && estimate_time &&
	    !tracker_miner_is_paused (TRACKER_MINER (decorator))) {
		gdouble elapsed;
		gint elems_done;

		/* FIXME: Quite naive calculation */
		elapsed = g_timer_elapsed (priv->timer, NULL);
		elems_done = priv->stats_n_elems - priv->elem_queue->length;
		remaining_time = (priv->elem_queue->length * elapsed) /
			elems_done;
	}

	g_object_set (decorator,
	              "progress", progress,
	              "remaining-time", remaining_time,
	              NULL);

	if (message)
		g_object_set (decorator, "status", message, NULL);
}

static void
element_add (TrackerDecorator *decorator,
             gint              id,
             gboolean          prepend)
{
	TrackerDecoratorPrivate *priv;
	gboolean first_elem;
	ElemNode *node;
	GList *elem;

	priv = decorator->priv;

	if (g_hash_table_contains (priv->elems, GINT_TO_POINTER (id)))
		return;

	first_elem = g_hash_table_size (priv->elems) == 0;
	node = g_new0 (ElemNode, 1);
	node->id = id;

	if (prepend) {
		g_queue_push_head (priv->elem_queue, node);
		elem = priv->elem_queue->head;
	} else {
		g_queue_push_tail (priv->elem_queue, node);
		elem = priv->elem_queue->tail;
	}

	g_hash_table_insert (priv->elems, GINT_TO_POINTER (id), elem);
	priv->stats_n_elems++;

	if (first_elem) {
		g_signal_emit (decorator, signals[ITEMS_AVAILABLE], 0);
		decorator_update_state (decorator, "Extracting metadata", TRUE);
	}
}

static void
element_remove_link (TrackerDecorator *decorator,
                     GList            *elem_link,
                     gboolean          emit)
{
	TrackerDecoratorPrivate *priv;
	ElemNode *node;

	priv = decorator->priv;
	node = elem_link->data;

	if (node->info && node->info->task) {
		/* A GTask is running on this element, cancel it
		 * and wait for the task callback to delete this node.
		 */
		g_cancellable_cancel (g_task_get_cancellable (node->info->task));
		return;
	}

	g_queue_delete_link (priv->elem_queue, elem_link);
	g_hash_table_remove (priv->elems, GINT_TO_POINTER (node->id));

	if (emit && g_hash_table_size (priv->elems) == 0) {
		g_signal_emit (decorator, signals[FINISHED], 0);
		decorator_update_state (decorator, "Idle", FALSE);
		priv->stats_n_elems = 0;
	}

	if (node->info)
		tracker_decorator_info_unref (node->info);

	g_free (node);
}

static void
element_remove_by_id (TrackerDecorator *decorator,
                      gint              id)
{
	TrackerDecoratorPrivate *priv;
	GList *elem_link;

	priv = decorator->priv;
	elem_link = g_hash_table_lookup (priv->elems, GINT_TO_POINTER (id));

	if (!elem_link)
		return;

	element_remove_link (decorator, elem_link, TRUE);
}

static void
decorator_commit_cb (GObject      *object,
                     GAsyncResult *result,
                     gpointer      user_data)
{
	TrackerSparqlConnection *conn;
	GPtrArray *errors, *sparql;
	GError *error = NULL;
	guint i;

	sparql = user_data;
	conn = TRACKER_SPARQL_CONNECTION (object);
	errors = tracker_sparql_connection_update_array_finish (conn, result, &error);

	if (error) {
		g_warning ("There was an error pushing metadata: %s\n", error->message);
	}

	for (i = 0; i < errors->len; i++) {
		GError *child_error;

		child_error = g_ptr_array_index (errors, i);

		if (child_error) {
			g_warning ("Task %d, error: %s", i, child_error->message);
			g_warning ("Sparql update was:\n%s\n",
			           (gchar *) g_ptr_array_index (sparql, i));
		}
	}

	g_ptr_array_unref (errors);
	g_ptr_array_unref (sparql);
}

static void
decorator_commit_info (TrackerDecorator *decorator)
{
	TrackerSparqlConnection *sparql_conn;
	TrackerDecoratorPrivate *priv;
	GPtrArray *array;

	priv = decorator->priv;

	if (priv->sparql_buffer->len == 0)
		return;

	array = priv->sparql_buffer;
	priv->sparql_buffer = g_ptr_array_new_with_free_func (g_free);

	sparql_conn = tracker_miner_get_connection (TRACKER_MINER (decorator));
	tracker_sparql_connection_update_array_async (sparql_conn,
	                                              (gchar **) array->pdata,
	                                              array->len,
	                                              G_PRIORITY_DEFAULT,
	                                              NULL,
	                                              decorator_commit_cb,
	                                              array);

	decorator_update_state (decorator, NULL, TRUE);
}

static void
decorator_check_commit (TrackerDecorator *decorator)
{
	TrackerDecoratorPrivate *priv;

	priv = decorator->priv;

	if (priv->sparql_buffer->len < (guint) priv->batch_size)
		return;

	decorator_commit_info (decorator);
}

/* This function is called after the caller has completed the
 * GTask given on the TrackerDecoratorInfo, this definitely removes
 * the element being processed from queues.
 */
static void
decorator_task_done (GObject      *object,
                     GAsyncResult *result,
                     gpointer      user_data)
{
	TrackerDecorator *decorator = TRACKER_DECORATOR (object);
	TrackerDecoratorPrivate *priv;
	ElemNode *node = user_data;

	priv = decorator->priv;

	if (g_task_had_error (G_TASK (result))) {
		GError *error = NULL;

		g_task_propagate_pointer (G_TASK (result), &error);
		g_warning ("Task for '%s' finished with error: %s\n",
		           node->info->url, error->message);
		g_error_free (error);
	} else {
		TrackerSparqlBuilder *sparql;

		/* Add resulting sparql to buffer and check whether flushing */
		sparql = g_task_get_task_data (G_TASK (result));
		g_ptr_array_add (priv->sparql_buffer,
		                 g_strdup (tracker_sparql_builder_get_result (sparql)));

		decorator_check_commit (decorator);
	}

	/* Detach task first, so the node is removed for good */
	g_clear_object (&node->info->task);
	element_remove_by_id (decorator, node->id);
}

static void
element_ensure_task (ElemNode         *node,
                     TrackerDecorator *decorator)
{
	TrackerSparqlBuilder *sparql;
	TrackerDecoratorInfo *info;
	GCancellable *cancellable;

	g_return_if_fail (node->info != NULL);

	info = node->info;

	if (info->task)
		return;

	cancellable = g_cancellable_new ();
	info->task = g_task_new (decorator, cancellable,
	                         decorator_task_done, node);
	g_object_unref (cancellable);

	sparql = tracker_sparql_builder_new_update ();
	g_task_set_task_data (info->task, sparql,
	                      (GDestroyNotify) g_object_unref);
}

static gint
get_class_id (TrackerSparqlConnection *conn,
              const gchar             *class)
{
	TrackerSparqlCursor *cursor;
	GError *error = NULL;
	gchar *query;
	gint id = -1;

	query = g_strdup_printf ("select tracker:id (%s) { }", class);
	cursor = tracker_sparql_connection_query (conn, query, NULL, &error);
	g_free (query);

	if (error) {
		g_critical ("Could not get ID of class '%s': %s\n",
		            class, error->message);
		g_error_free (error);
		return -1;
	}

	if (tracker_sparql_cursor_next (cursor, NULL, NULL))
		id = tracker_sparql_cursor_get_integer (cursor, 0);
	else
		g_critical ("'%s' didn't resolve to a known ID", class);

	g_object_unref (cursor);

	return id;
}

static void
tracker_decorator_validate_class_ids (TrackerDecorator *decorator,
                                      const GStrv       class_names)
{
	TrackerSparqlConnection *sparql_conn;
	TrackerDecoratorPrivate *priv;
	GArray *strings;
	gint i = 0;

	priv = decorator->priv;
	sparql_conn = tracker_miner_get_connection (TRACKER_MINER (decorator));

	if (!sparql_conn) {
		/* Copy as-is and postpone validation */
		g_strfreev (priv->class_names);
		priv->class_names = g_strdupv (class_names);
		return;
	}

	if (priv->class_name_ids->len > 0)
		g_array_remove_range (priv->class_name_ids, 0,
		                      priv->class_name_ids->len);

	if (class_names) {
		strings = g_array_new (TRUE, FALSE, sizeof (gchar *));

		while (class_names[i]) {
			gchar *copy;
			gint id;

			id = get_class_id (sparql_conn, class_names[i]);

			if (id >= 0) {
				copy = g_strdup (class_names[i]);
				g_array_append_val (strings, copy);
				g_array_append_val (priv->class_name_ids, id);
			}

			i++;
		}
	}

	if (priv->class_names) {
		g_strfreev (priv->class_names);
		priv->class_names = NULL;
	}

	if (priv->class_name_ids->len > 0)
		priv->class_names = (GStrv) g_array_free (strings, FALSE);
	else
		g_array_free (strings, TRUE);

	g_object_notify (G_OBJECT (decorator), "class-names");
}

static void
tracker_decorator_get_property (GObject    *object,
                                guint       param_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
	TrackerDecoratorPrivate *priv;

	priv = TRACKER_DECORATOR (object)->priv;

	switch (param_id) {
	case PROP_DATA_SOURCE:
		g_value_set_string (value, priv->data_source);
		break;
	case PROP_CLASS_NAMES:
		g_value_set_boxed (value, priv->class_names);
		break;
	case PROP_COMMIT_BATCH_SIZE:
		g_value_set_int (value, priv->batch_size);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
	}
}

static void
tracker_decorator_set_property (GObject      *object,
                                guint         param_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
	TrackerDecoratorPrivate *priv;

	priv = TRACKER_DECORATOR (object)->priv;

	switch (param_id) {
	case PROP_DATA_SOURCE:
		priv->data_source = g_value_dup_string (value);
		break;
	case PROP_CLASS_NAMES:
		tracker_decorator_validate_class_ids (TRACKER_DECORATOR (object),
		                                      g_value_get_boxed (value));
		break;
	case PROP_COMMIT_BATCH_SIZE:
		priv->batch_size = g_value_get_int (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
	}
}

static void
handle_deletes (TrackerDecorator *decorator,
                GVariantIter     *iter)
{
	gint graph, subject, predicate, object;
	TrackerDecoratorPrivate *priv;

	priv = decorator->priv;

	while (g_variant_iter_loop (iter, "(iiii)",
				    &graph, &subject, &predicate, &object)) {
		if (predicate == priv->rdf_type_id)
			element_remove_by_id (decorator, subject);
		else if (predicate == priv->nie_data_source_id &&
			 object == priv->data_source_id) {
			/* If only the decorator datasource is removed,
			 * re-process the file from scratch.
			 */
			element_add (decorator, subject, FALSE);
		}
	}
}

static gboolean
class_name_id_handled (TrackerDecorator *decorator,
                       gint              id)
{
	TrackerDecoratorPrivate *priv;
	guint i;

	priv = decorator->priv;

	for (i = 0; i < priv->class_name_ids->len; i++) {
		if (id == g_array_index (priv->class_name_ids, gint, i))
			return TRUE;
	}

	return FALSE;
}

static void
handle_updates (TrackerDecorator *decorator,
                GVariantIter     *iter)
{
	gint graph, subject, predicate, object;
	TrackerDecoratorPrivate *priv;

	priv = decorator->priv;

	while (g_variant_iter_loop (iter, "(iiii)",
	                            &graph, &subject, &predicate, &object)) {
		if (predicate == priv->rdf_type_id &&
		    class_name_id_handled (decorator, object))
			element_add (decorator, subject, FALSE);
	}
}

static void
class_signal_cb (GDBusConnection *connection,
                 const gchar     *sender_name,
                 const gchar     *object_path,
                 const gchar     *interface_name,
                 const gchar     *signal_name,
                 GVariant        *parameters,
                 gpointer         user_data)
{
	GVariantIter *iter1, *iter2;

	g_variant_get (parameters, "(&sa(iiii)a(iiii))", NULL, &iter1, &iter2);
	handle_deletes (user_data, iter1);
	handle_updates (user_data, iter2);
}

static gboolean
tracker_decorator_initable_init (GInitable     *initable,
                                 GCancellable  *cancellable,
                                 GError       **error)
{
	TrackerSparqlConnection *sparql_conn;
	TrackerDecoratorPrivate *priv;
	TrackerDecorator *decorator;
	GDBusConnection *conn;

	if (!parent_initable_iface->init (initable, cancellable, error))
		return FALSE;

	decorator = TRACKER_DECORATOR (initable);
	priv = decorator->priv;

	sparql_conn = tracker_miner_get_connection (TRACKER_MINER (initable));
	conn = tracker_miner_get_dbus_connection (TRACKER_MINER (initable));

	if (g_cancellable_is_cancelled (cancellable))
		return FALSE;

	priv->rdf_type_id = get_class_id (sparql_conn, "rdf:type");
	priv->nie_data_source_id = get_class_id (sparql_conn, "nie:dataSource");
	priv->data_source_id = get_class_id (sparql_conn, priv->data_source);
	tracker_decorator_validate_class_ids (decorator, priv->class_names);

	priv->graph_updated_signal_id =
		g_dbus_connection_signal_subscribe (conn,
		                                    TRACKER_DBUS_SERVICE,
		                                    TRACKER_DBUS_INTERFACE_RESOURCES,
		                                    "GraphUpdated",
		                                    TRACKER_DBUS_OBJECT_RESOURCES,
		                                    NULL,
		                                    G_DBUS_SIGNAL_FLAGS_NONE,
		                                    class_signal_cb,
		                                    initable, NULL);
	decorator_update_state (decorator, "Idle", FALSE);
	return TRUE;
}

static void
tracker_decorator_initable_iface_init (GInitableIface *iface)
{
	parent_initable_iface = g_type_interface_peek_parent (iface);
	iface->init = tracker_decorator_initable_init;
}


static void
tracker_decorator_constructed (GObject *object)
{
	TrackerDecoratorPrivate *priv;

	priv = TRACKER_DECORATOR (object)->priv;
	g_assert (priv->data_source);
}

static void
tracker_decorator_finalize (GObject *object)
{
	TrackerDecoratorPrivate *priv;
	TrackerDecorator *decorator;
	GDBusConnection *conn;

	decorator = TRACKER_DECORATOR (object);
	priv = decorator->priv;

	if (priv->graph_updated_signal_id) {
		conn = tracker_miner_get_dbus_connection (TRACKER_MINER (object));
		g_dbus_connection_signal_unsubscribe (conn,
		                                      priv->graph_updated_signal_id);
	}

	while (priv->elem_queue->head)
		element_remove_link (decorator, priv->elem_queue->head, FALSE);

	g_array_unref (priv->class_name_ids);
	g_queue_free (priv->elem_queue);
	g_hash_table_unref (priv->elems);
	g_free (priv->data_source);
	g_strfreev (priv->class_names);
	g_timer_destroy (priv->timer);

	if (priv->sparql_buffer)
		g_ptr_array_unref (priv->sparql_buffer);

	G_OBJECT_CLASS (tracker_decorator_parent_class)->finalize (object);
}

static void
query_append_rdf_type_filter (GString          *query,
                              TrackerDecorator *decorator)
{
	const gchar **class_names;
	gint i = 0;

	class_names = tracker_decorator_get_class_names (decorator);

	if (!class_names || !*class_names)
		return;

	g_string_append (query, "&& (");

	while (class_names[i]) {
		if (i != 0)
			g_string_append (query, "||");

		g_string_append_printf (query, "EXISTS { ?urn a %s }",
		                        class_names[i]);
		i++;
	}

	g_string_append (query, ") ");
}

static void
query_elements_cb (GObject      *object,
                   GAsyncResult *result,
                   gpointer      user_data)
{
	TrackerSparqlConnection *conn;
	TrackerSparqlCursor *cursor;
	GError *error = NULL;

	conn = TRACKER_SPARQL_CONNECTION (object);
	cursor = tracker_sparql_connection_query_finish (conn, result, &error);

        if (error) {
                g_critical ("Could not load files missing metadata: %s", error->message);
                g_error_free (error);
		return;
	}

	while (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
		gint id = tracker_sparql_cursor_get_integer (cursor, 0);
		element_add (user_data, id, TRUE);
	}

	g_object_unref (cursor);
}

static void
tracker_decorator_paused (TrackerMiner *miner)
{
	TrackerDecoratorPrivate *priv;

	priv = TRACKER_DECORATOR (miner)->priv;
	g_timer_stop (priv->timer);
}

static void
tracker_decorator_resumed (TrackerMiner *miner)
{
	TrackerDecoratorPrivate *priv;

	priv = TRACKER_DECORATOR (miner)->priv;
	g_timer_continue (priv->timer);
}

static void
tracker_decorator_stopped (TrackerMiner *miner)
{
	TrackerDecoratorPrivate *priv;

	priv = TRACKER_DECORATOR (miner)->priv;
	g_timer_stop (priv->timer);
}

static void
tracker_decorator_started (TrackerMiner *miner)
{
	TrackerSparqlConnection *sparql_conn;
	TrackerDecoratorPrivate *priv;
	TrackerDecorator *decorator;
	const gchar *data_source;
	GString *query;

	decorator = TRACKER_DECORATOR (miner);
	priv = decorator->priv;

	g_timer_start (priv->timer);
	data_source = tracker_decorator_get_data_source (decorator);
	query = g_string_new ("SELECT tracker:id(?urn) { "
	                      "  ?urn a rdfs:Resource . ");

	g_string_append_printf (query,
	                        "FILTER (! EXISTS { ?urn nie:dataSource <%s> } ",
	                        data_source);

	query_append_rdf_type_filter (query, decorator);
	g_string_append (query, "&& BOUND(tracker:available(?urn)))}");

	sparql_conn = tracker_miner_get_connection (miner);
	tracker_sparql_connection_query_async (sparql_conn, query->str,
	                                       NULL, query_elements_cb,
	                                       decorator);
	g_string_free (query, TRUE);
}

static void
tracker_decorator_class_init (TrackerDecoratorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	TrackerMinerClass *miner_class = TRACKER_MINER_CLASS (klass);

	object_class->get_property = tracker_decorator_get_property;
	object_class->set_property = tracker_decorator_set_property;
	object_class->constructed = tracker_decorator_constructed;
	object_class->finalize = tracker_decorator_finalize;

	miner_class->paused = tracker_decorator_paused;
	miner_class->resumed = tracker_decorator_resumed;
	miner_class->started = tracker_decorator_started;
	miner_class->stopped = tracker_decorator_stopped;

	g_object_class_install_property (object_class,
	                                 PROP_DATA_SOURCE,
	                                 g_param_spec_string ("data-source",
	                                                      "Data source URN",
	                                                      "nie:DataSource to use in this decorator",
	                                                      NULL,
	                                                      G_PARAM_READWRITE |
	                                                      G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class,
	                                 PROP_CLASS_NAMES,
	                                 g_param_spec_boxed ("class-names",
	                                                     "Class names",
	                                                     "rdfs:Class objects to listen to for changes",
	                                                     G_TYPE_STRV,
	                                                     G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_COMMIT_BATCH_SIZE,
	                                 g_param_spec_int ("commit-batch-size",
	                                                   "Commit batch size",
	                                                   "Number of items per update batch",
	                                                   0, G_MAXINT, DEFAULT_BATCH_SIZE,
	                                                   G_PARAM_READWRITE));
	signals[ITEMS_AVAILABLE] =
		g_signal_new ("items-available",
		              G_OBJECT_CLASS_TYPE (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (TrackerDecoratorClass,
		                               items_available),
		              NULL, NULL, NULL,
		              G_TYPE_NONE, 0);
	signals[FINISHED] =
		g_signal_new ("finished",
		              G_OBJECT_CLASS_TYPE (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (TrackerDecoratorClass, finished),
		              NULL, NULL, NULL,
		              G_TYPE_NONE, 0);

	g_type_class_add_private (object_class, sizeof (TrackerDecoratorPrivate));
}

static void
tracker_decorator_init (TrackerDecorator *decorator)
{
	TrackerDecoratorPrivate *priv;

	decorator->priv = priv = TRACKER_DECORATOR_GET_PRIVATE (decorator);
	priv->elems = g_hash_table_new (NULL, NULL);
	priv->elem_queue = g_queue_new ();
	priv->class_name_ids = g_array_new (FALSE, FALSE, sizeof (gint));
	priv->batch_size = DEFAULT_BATCH_SIZE;
	priv->sparql_buffer = g_ptr_array_new_with_free_func (g_free);
	priv->timer = g_timer_new ();
}

const gchar *
tracker_decorator_get_data_source (TrackerDecorator *decorator)
{
	TrackerDecoratorPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_DECORATOR (decorator), NULL);

	priv = decorator->priv;
	return priv->data_source;
}

const gchar **
tracker_decorator_get_class_names (TrackerDecorator *decorator)
{
	TrackerDecoratorPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_DECORATOR (decorator), NULL);

	priv = decorator->priv;
	return (const gchar **) priv->class_names;
}

guint
tracker_decorator_get_n_items (TrackerDecorator *decorator)
{
	TrackerDecoratorPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_DECORATOR (decorator), 0);

	priv = decorator->priv;
	return g_hash_table_size (priv->elems);
}

void
tracker_decorator_prepend_ids (TrackerDecorator *decorator,
                               gint             *ids,
                               gint              n_ids)
{
	gint i;

	g_return_if_fail (TRACKER_IS_DECORATOR (decorator));
	g_return_if_fail (ids != NULL);
	g_return_if_fail (n_ids >= 0);

	/* Prepend in inverse order to preserve ordering */
	for (i = n_ids; i >= 0; i--)
		element_add (decorator, ids[i], TRUE);
}

void
tracker_decorator_delete_ids (TrackerDecorator *decorator,
                              gint             *ids,
                              gint              n_ids)
{
	TrackerDecoratorPrivate *priv;
	gint i;

	g_return_if_fail (TRACKER_IS_DECORATOR (decorator));
	g_return_if_fail (ids != NULL);
	g_return_if_fail (n_ids > 0);

	priv = decorator->priv;

	if (priv->elem_queue->length == 0)
		return;

	for (i = 0; i < n_ids; i++)
		element_remove_by_id (decorator, ids[i]);
}

static GList *
find_first_free_node (TrackerDecorator *decorator)
{
	TrackerDecoratorPrivate *priv;
	GList *l;

	priv = decorator->priv;

	if (!priv->elem_queue->head)
		return NULL;

	for (l = priv->elem_queue->head; l; l = l->next) {
		ElemNode *node = l->data;

		if (!node->info || !node->info->task)
			return l;
	}

	return NULL;
}

static void
complete_task (GTask    *task,
               ElemNode *node)
{
	g_assert (node->info);

	element_ensure_task (node, g_task_get_source_object (task));
	g_task_return_pointer (task, tracker_decorator_info_ref (node->info),
	                       (GDestroyNotify) tracker_decorator_info_unref);
	g_object_unref (task);
}

static void
check_task_complete (TrackerDecorator *decorator,
                     GTask            *task)
{
	GList *first, *last;
	TaskData *data;

	data = g_task_get_task_data (task);
	first = data->first;
	last = data->last->next;

	while (first != last) {
		ElemNode *node;
		GList *next;

		node = first->data;
		next = first->next;

		if (node->info) {
			complete_task (task, node);
			return;
		}

		element_remove_link (decorator, first, TRUE);
		first = next;
	}

	/* If this is reached, no queried IDs
	 * got data in the query, so try again.
	 */
	query_next_items (decorator, task);
}

static void
query_next_items_cb (GObject      *object,
                     GAsyncResult *result,
                     gpointer      user_data)
{
	TrackerDecoratorPrivate *priv;
	TrackerSparqlConnection *conn;
	TrackerSparqlCursor *cursor;
	GTask *task = user_data;
	GError *error = NULL;
	TaskData *data;

	conn = TRACKER_SPARQL_CONNECTION (object);
	cursor = tracker_sparql_connection_query_finish (conn, result, &error);
	data = g_task_get_task_data (task);
	priv = data->decorator->priv;

	if (error) {
		g_task_return_error (task, error);
		g_object_unref (task);
		return;
	}

	while (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
		GList *elem;
		ElemNode *node;
		gint id;

		id = tracker_sparql_cursor_get_integer (cursor, 1);
		elem = g_hash_table_lookup (priv->elems, GINT_TO_POINTER (id));
		node = elem->data;
		node->info = tracker_decorator_info_new (cursor);
	}

	g_object_unref (cursor);

	check_task_complete (data->decorator, task);
}

static void
query_next_items (TrackerDecorator *decorator,
                  GTask            *task)
{
	TrackerSparqlConnection *sparql_conn;
	TrackerDecoratorPrivate *priv;
	GList *items, *last;
	GString *id_string;
	TaskData *data;
	gchar *query;
	gint i;

	priv = decorator->priv;
	items = find_first_free_node (decorator);

	if (!items) {
		GError *error;

		error = g_error_new (tracker_decorator_error_quark (),
		                     TRACKER_DECORATOR_ERROR_EMPTY,
		                     "There are no items left");
		g_task_return_error (task, error);
		g_object_unref (task);
		return;
	}

	sparql_conn = tracker_miner_get_connection (TRACKER_MINER (decorator));
	id_string = g_string_new (NULL);

	for (i = 0; i < QUERY_BATCH_SIZE && items; i++) {
		ElemNode *node;

		last = items;
		node = items->data;
		items = items->next;

		if (id_string->len > 0)
			g_string_append_c (id_string, ',');

		g_string_append_printf (id_string, "%d", node->id);
	}

	query = g_strdup_printf ("SELECT ?urn"
	                         "       tracker:id(?urn) "
	                         "       nie:url(?urn) "
	                         "       nie:mimeType(?urn) { "
	                         "  ?urn tracker:available true . "
	                         "  FILTER (tracker:id(?urn) IN (%s) && "
	                         "          ! EXISTS { ?urn nie:dataSource <%s> })"
	                         "}", id_string->str, priv->data_source);

	data = task_data_new (decorator, priv->elem_queue->head, last);
	g_task_set_task_data (task, data, (GDestroyNotify) g_free);
	tracker_sparql_connection_query_async (sparql_conn, query,
	                                       g_task_get_cancellable (task),
	                                       query_next_items_cb, task);
	g_string_free (id_string, TRUE);
	g_free (query);
}

void
tracker_decorator_next (TrackerDecorator    *decorator,
                        GCancellable        *cancellable,
                        GAsyncReadyCallback  callback,
                        gpointer             user_data)
{
	ElemNode *node = NULL;
	GTask *task;
	GList *elem;

	g_return_if_fail (TRACKER_IS_DECORATOR (decorator));

	task = g_task_new (decorator, cancellable, callback, user_data);

	if (tracker_miner_is_paused (TRACKER_MINER (decorator))) {
		GError *error;

		error = g_error_new (tracker_decorator_error_quark (),
		                     TRACKER_DECORATOR_ERROR_PAUSED,
		                     "Decorator is paused");
		g_task_return_error (task, error);
		g_object_unref (task);
		return;
	}

	elem = find_first_free_node (decorator);

	if (elem)
		node = elem->data;

	if (node && node->info)
		complete_task (task, node);
	else
		query_next_items (decorator, task);
}

TrackerDecoratorInfo *
tracker_decorator_next_finish (TrackerDecorator  *decorator,
                               GAsyncResult      *result,
                               GError           **error)
{
	g_return_val_if_fail (TRACKER_DECORATOR (decorator), NULL);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);
	g_return_val_if_fail (!error || !*error, NULL);

	return g_task_propagate_pointer (G_TASK (result), error);
}

const gchar *
tracker_decorator_info_get_urn (TrackerDecoratorInfo *info)
{
	g_return_val_if_fail (info != NULL, NULL);
	return info->urn;
}

const gchar *
tracker_decorator_info_get_url (TrackerDecoratorInfo *info)
{
	g_return_val_if_fail (info != NULL, NULL);
	return info->url;
}

const gchar *
tracker_decorator_info_get_mimetype (TrackerDecoratorInfo *info)
{
	g_return_val_if_fail (info != NULL, NULL);
	return info->mimetype;
}

GTask *
tracker_decorator_info_get_task (TrackerDecoratorInfo *info)
{
	g_return_val_if_fail (info != NULL, NULL);
	return info->task;
}

TrackerSparqlBuilder *
tracker_decorator_info_get_sparql (TrackerDecoratorInfo *info)
{
	g_return_val_if_fail (info != NULL, NULL);

	if (!info->task)
		return NULL;

	return g_task_get_task_data (info->task);
}
