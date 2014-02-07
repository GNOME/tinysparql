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
#include "tracker-decorator-internal.h"

#define QUERY_BATCH_SIZE 100
#define DEFAULT_BATCH_SIZE 100

#define TRACKER_DECORATOR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_DECORATOR, TrackerDecoratorPrivate))

/**
 * SECTION:tracker-decorator
 * @short_description: A miner tasked with listening for DB resource changes and extracting metadata
 * @include: libtracker-miner/tracker-miner.h
 * @title: TrackerDecorator
 * @see_also: #TrackerDecoratorFS
 * #TrackerDecorator watches for signal updates based on file changes
 * in the database. When new files are added initially, only simple
 * metadata exists, for example, name, size, mtime, etc. The
 * #TrackerDecorator queues files for extended metadata extraction
 * (i.e. for tracker-extract to fetch metadata specific to the file
 * type) for example 'nmm:whiteBalance' for a picture.
**/

typedef struct _TrackerDecoratorPrivate TrackerDecoratorPrivate;
typedef struct _ElemNode ElemNode;

struct _TrackerDecoratorInfo {
	GTask *task;
	gchar *urn;
	gchar *url;
	gchar *mimetype;
	gint ref_count;
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
	GQueue next_elem_queue;

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

G_DEFINE_QUARK (TrackerDecoratorError, tracker_decorator_error)

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (TrackerDecorator, tracker_decorator, TRACKER_TYPE_MINER,
                                  G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, tracker_decorator_initable_iface_init))

static TrackerDecoratorInfo *
tracker_decorator_info_new (TrackerSparqlCursor *cursor)
{
	TrackerDecoratorInfo *info;

	info = g_slice_new0 (TrackerDecoratorInfo);
	info->urn = g_strdup (tracker_sparql_cursor_get_string (cursor, 0, NULL));
	info->url = g_strdup (tracker_sparql_cursor_get_string (cursor, 2, NULL));
	info->mimetype = g_strdup (tracker_sparql_cursor_get_string (cursor, 3, NULL));
	info->ref_count = 1;

	return info;
}

TrackerDecoratorInfo *
tracker_decorator_info_ref (TrackerDecoratorInfo *info)
{
	g_atomic_int_inc (&info->ref_count);
	return info;
}

void
tracker_decorator_info_unref (TrackerDecoratorInfo *info)
{
	if (!g_atomic_int_dec_and_test (&info->ref_count))
		return;

	if (info->task)
		g_object_unref (info->task);
	g_free (info->urn);
	g_free (info->url);
	g_free (info->mimetype);
	g_slice_free (TrackerDecoratorInfo, info);
}

G_DEFINE_BOXED_TYPE (TrackerDecoratorInfo,
                     tracker_decorator_info,
                     tracker_decorator_info_ref,
                     tracker_decorator_info_unref)

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

		if (elems_done > 0)
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
                     GList            *elem_link)
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

	if (g_hash_table_size (priv->elems) == 0) {
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

	element_remove_link (decorator, elem_link);
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
		g_critical ("Could not get class ID for '%s': %s\n",
		            class, error->message);
		g_error_free (error);
		return -1;
	}

	if (tracker_sparql_cursor_next (cursor, NULL, NULL))
		id = tracker_sparql_cursor_get_integer (cursor, 0);
	else
		g_critical ("'%s' didn't resolve to a known class ID", class);

	g_object_unref (cursor);

	return id;
}

static void
tracker_decorator_validate_class_ids (TrackerDecorator *decorator,
                                      const GStrv       class_names)
{
	TrackerSparqlConnection *sparql_conn;
	TrackerDecoratorPrivate *priv;
	GPtrArray *strings;
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

	strings = g_ptr_array_new ();
	if (class_names) {
		while (class_names[i]) {
			gchar *copy;
			gint id;

			id = get_class_id (sparql_conn, class_names[i]);

			if (id >= 0) {
				copy = g_strdup (class_names[i]);
				g_ptr_array_add (strings, copy);
				g_array_append_val (priv->class_name_ids, id);
			}

			i++;
		}
	}
	g_ptr_array_add (strings, NULL);

	g_strfreev (priv->class_names);
	priv->class_names = (GStrv) g_ptr_array_free (strings, FALSE);

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
	g_variant_iter_free (iter1);
	g_variant_iter_free (iter2);
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

void
_tracker_decorator_query_append_rdf_type_filter (TrackerDecorator *decorator,
                                                 GString          *query)
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

	_tracker_decorator_query_append_rdf_type_filter (decorator, query);
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
	/**
	 * TrackerDecorator::items-available:
	 * @decorator: the #TrackerDecorator
	 *
	 * The ::items-available signal will be emitted whenever the
	 * #TrackerDecorator sees resources that are available for
	 * extended metadata extraction.
	 *
	 * Since: 0.18
	 **/
	signals[ITEMS_AVAILABLE] =
		g_signal_new ("items-available",
		              G_OBJECT_CLASS_TYPE (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (TrackerDecoratorClass,
		                               items_available),
		              NULL, NULL, NULL,
		              G_TYPE_NONE, 0);
	/**
	 * TrackerDecorator::finished:
	 * @decorator: the #TrackerDecorator
	 *
	 * The ::finished signal will be emitted whenever the
	 * #TrackerDecorator has finished extracted extended metadata
	 * for resources in the database.
	 *
	 * Since: 0.18
	 **/
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

/**
 * tracker_decorator_get_data_source:
 * @decorator: a #TrackerDecorator.
 *
 * The unique string identifying this #TrackerDecorator that has
 * extracted the extended metadata. This is essentially an identifier
 * so it's clear WHO has extracted this extended metadata.
 *
 * Returns: a const gchar* or #NULL if an error happened.
 *
 * Since: 0.18
 **/
const gchar *
tracker_decorator_get_data_source (TrackerDecorator *decorator)
{
	TrackerDecoratorPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_DECORATOR (decorator), NULL);

	priv = decorator->priv;
	return priv->data_source;
}

/**
 * tracker_decorator_get_class_names:
 * @decorator: a #TrackerDecorator.
 *
 * This function returns a string list of class names which are being
 * updated with extended metadata. An example would be 'nfo:Document'.
 *
 * Returns: (transfer none): a const gchar** or #NULL.
 *
 * Since: 0.18
 **/
const gchar **
tracker_decorator_get_class_names (TrackerDecorator *decorator)
{
	TrackerDecoratorPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_DECORATOR (decorator), NULL);

	priv = decorator->priv;
	return (const gchar **) priv->class_names;
}

/**
 * tracker_decorator_get_n_items:
 * @decorator: a #TrackerDecorator.
 *
 * Returns: the number of items queued to be processed, always >= 0.
 *
 * Since: 0.18
 **/
guint
tracker_decorator_get_n_items (TrackerDecorator *decorator)
{
	TrackerDecoratorPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_DECORATOR (decorator), 0);

	priv = decorator->priv;
	return g_hash_table_size (priv->elems);
}

/**
 * tracker_decorator_prepend_id:
 * @decorator: a #TrackerDecorator.
 * @id: an ID.
 *
 * Adds resource needing extended metadata extraction to the queue.
 * @id is the same IDs emitted by tracker-store when the database is updated for
 * consistency. For details, see the GraphUpdated signal.
 *
 * Since: 0.18
 **/
void
tracker_decorator_prepend_id (TrackerDecorator *decorator,
                              gint              id)
{
	g_return_if_fail (TRACKER_IS_DECORATOR (decorator));

	element_add (decorator, id, TRUE);
}

/**
 * tracker_decorator_delete_id:
 * @decorator: a #TrackerDecorator.
 * @id: an ID.
 *
 * Deletes resource needing extended metadata extraction from the
 * queue. @id is the same IDs emitted by tracker-store when the database is
 * updated for consistency. For details, see the GraphUpdated signal.
 *
 * Since: 0.18
 **/
void
tracker_decorator_delete_id (TrackerDecorator *decorator,
                             gint              id)
{
	g_return_if_fail (TRACKER_IS_DECORATOR (decorator));

	element_remove_by_id (decorator, id);
}

static void complete_tasks_or_query (TrackerDecorator *decorator);

static void
query_next_items_cb (GObject      *object,
                     GAsyncResult *result,
                     gpointer      user_data)
{
	TrackerDecorator *decorator = user_data;
	TrackerDecoratorPrivate *priv;
	TrackerSparqlConnection *conn;
	TrackerSparqlCursor *cursor;
	GError *error = NULL;

	conn = TRACKER_SPARQL_CONNECTION (object);
	cursor = tracker_sparql_connection_query_finish (conn, result, &error);
	priv = decorator->priv;

	if (error) {
		GTask *task;

		while ((task = g_queue_pop_head (&priv->next_elem_queue))) {
			g_task_return_error (task, g_error_copy (error));
			g_object_unref (task);
		}

		g_clear_error (&error);
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

	complete_tasks_or_query (decorator);
}

static void
query_next_items (TrackerDecorator *decorator,
                  GList            *l)
{
	TrackerSparqlConnection *sparql_conn;
	TrackerDecoratorPrivate *priv;
	GString *id_string;
	gchar *query;
	guint count = 0;

	priv = decorator->priv;

	id_string = g_string_new (NULL);
	for (; l != NULL && count < QUERY_BATCH_SIZE; l = l->next) {
		ElemNode *node = l->data;

		if (node->info)
			continue;

		if (id_string->len > 0)
			g_string_append_c (id_string, ',');

		g_string_append_printf (id_string, "%d", node->id);
		count++;
	}

	g_assert (count > 0);

	query = g_strdup_printf ("SELECT ?urn"
	                         "       tracker:id(?urn) "
	                         "       nie:url(?urn) "
	                         "       nie:mimeType(?urn) { "
	                         "  ?urn tracker:available true . "
	                         "  FILTER (tracker:id(?urn) IN (%s) && "
	                         "          ! EXISTS { ?urn nie:dataSource <%s> })"
	                         "}", id_string->str, priv->data_source);

	sparql_conn = tracker_miner_get_connection (TRACKER_MINER (decorator));
	tracker_sparql_connection_query_async (sparql_conn, query,
	                                       NULL,
	                                       query_next_items_cb, decorator);
	g_string_free (id_string, TRUE);
	g_free (query);
}

static void
complete_tasks_or_query (TrackerDecorator *decorator)
{
	TrackerDecoratorPrivate *priv;
	GList *l;
	GTask *task;

	priv = decorator->priv;

	for (l = priv->elem_queue->head; l != NULL; l = l->next) {
		ElemNode *node = l->data;

		/* The next item isn't queried yet, do it now */
		if (!node->info) {
			query_next_items (decorator, l);
			return;
		}

		/* If the item is not already being processed, we can complete a
		 * task with it. */
		if (!node->info->task) {
			task = g_queue_pop_head (&priv->next_elem_queue);
			element_ensure_task (node, decorator);
			g_task_return_pointer (task,
			                       tracker_decorator_info_ref (node->info),
			                       (GDestroyNotify) tracker_decorator_info_unref);
			g_object_unref (task);

			if (g_queue_is_empty (&priv->next_elem_queue))
				return;
		}
	}

	/* There is no element left, or they are all being processed already */
	while ((task = g_queue_pop_head (&priv->next_elem_queue))) {
		g_task_return_new_error (task,
		                         tracker_decorator_error_quark (),
		                         TRACKER_DECORATOR_ERROR_EMPTY,
		                         "There are no items left");
		g_object_unref (task);
	}
}

/**
 * tracker_decorator_next:
 * @decorator: a #TrackerDecorator.
 * @cancellable: a #GCancellable.
 * @callback: a #GAsyncReadyCallback.
 * @user_data: user_data for @callback.
 *
 * Processes the next resource in the queue to have extended metadata
 * extracted. If the item in the queue has been completed already, it
 * signals it's completion instead.
 *
 * This function will give a #GError if the miner is paused at the
 * time it is called.
 *
 * Since: 0.18
 **/
void
tracker_decorator_next (TrackerDecorator    *decorator,
                        GCancellable        *cancellable,
                        GAsyncReadyCallback  callback,
                        gpointer             user_data)
{
	TrackerDecoratorPrivate *priv;
	GTask *task;

	g_return_if_fail (TRACKER_IS_DECORATOR (decorator));

	priv = decorator->priv;

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

	/* Push the task in a queue, for the case this function is called
	 * multiple before it finishes. */
	g_queue_push_tail (&priv->next_elem_queue, task);
	if (g_queue_get_length (&priv->next_elem_queue) == 1) {
		complete_tasks_or_query (decorator);
	}
}

/**
 * tracker_decorator_next_finish:
 * @decorator: a #TrackerDecorator.
 * @result: a #GAsyncResult.
 * @error: return location for a #GError, or NULL.
 *
 * Should be called in the callback function provided to
 * tracker_decorator_next() to return the result of the task be it an
 * error or not.
 *
 * Returns: (transfer full) (boxed): a #TrackerDecoratorInfo on success or
 *  #NULL on error. Free with tracker_decorator_info_unref().
 *
 * Since: 0.18
 **/
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

/**
 * tracker_decorator_info_get_urn:
 * @info: a #TrackerDecoratorInfo.
 *
 * A URN is a Uniform Resource Name and should be a unique identifier
 * for a resource in the database.
 *
 * Returns: the URN for #TrackerDecoratorInfo on success or #NULL on error.
 *
 * Since: 0.18
 **/
const gchar *
tracker_decorator_info_get_urn (TrackerDecoratorInfo *info)
{
	g_return_val_if_fail (info != NULL, NULL);
	return info->urn;
}

/**
 * tracker_decorator_info_get_url:
 * @info: a #TrackerDecoratorInfo.
 *
 * A URL is a Uniform Resource Locator and should be a location associated
 * with a resource in the database. For example, 'file:///tmp/foo.txt'.
 *
 * Returns: the URL for #TrackerDecoratorInfo on success or #NULL on error.
 *
 * Since: 0.18
 **/
const gchar *
tracker_decorator_info_get_url (TrackerDecoratorInfo *info)
{
	g_return_val_if_fail (info != NULL, NULL);
	return info->url;
}

/**
 * tracker_decorator_info_get_mimetype:
 * @info: a #TrackerDecoratorInfo.
 *
 * A MIME¹ type is a way of describing the content type of a file or
 * set of data. An example would be 'text/plain' for a clear text
 * document or file.
 *
 * ¹: http://en.wikipedia.org/wiki/MIME
 *
 * Returns: the MIME type for #TrackerDecoratorInfo on success or #NULL on error.
 *
 * Since: 0.18
 **/
const gchar *
tracker_decorator_info_get_mimetype (TrackerDecoratorInfo *info)
{
	g_return_val_if_fail (info != NULL, NULL);
	return info->mimetype;
}

/**
 * tracker_decorator_info_get_task:
 * @info: a #TrackerDecoratorInfo.
 *
 * When processing resource updates in the database, the #GTask APIs
 * are used. This function returns the particular #GTask used for
 * @info.
 *
 * Returns: (transfer none): the #GTask on success or #NULL on error.
 *
 * Since: 0.18
 **/
GTask *
tracker_decorator_info_get_task (TrackerDecoratorInfo *info)
{
	g_return_val_if_fail (info != NULL, NULL);
	return info->task;
}

/**
 * tracker_decorator_info_get_sparql:
 * @info: a #TrackerDecoratorInfo.
 *
 * A #TrackerSparqlBuilder allows the caller to extract the final
 * SPARQL used to insert the extracted metadata into the database for
 * the resource being processed.
 *
 * This function calls g_task_get_task_data() on the return value of
 * tracker_decorator_info_get_task().
 *
 * Returns: (transfer none): a #TrackerSparqlBuilder on success or #NULL on error.
 *
 * Since: 0.18
 **/
TrackerSparqlBuilder *
tracker_decorator_info_get_sparql (TrackerDecoratorInfo *info)
{
	g_return_val_if_fail (info != NULL, NULL);

	if (!info->task)
		return NULL;

	return g_task_get_task_data (info->task);
}
