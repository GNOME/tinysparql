/*
 * Copyright (C) 2011, Carlos Garnacho <carlos@lanedo.com>
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

#include <libtracker-sparql/tracker-sparql.h>
#include "tracker-sparql-buffer.h"

/* Maximum time (seconds) before forcing a sparql buffer flush */
#define MAX_SPARQL_BUFFER_TIME  15

typedef struct _TrackerSparqlBufferPrivate TrackerSparqlBufferPrivate;
typedef struct _SparqlTaskData SparqlTaskData;
typedef struct _UpdateArrayData UpdateArrayData;
typedef struct _BulkOperationMerge BulkOperationMerge;

enum {
	PROP_0,
	PROP_CONNECTION
};

enum {
	TASK_TYPE_SPARQL_STR,
	TASK_TYPE_SPARQL,
	TASK_TYPE_BULK
};

struct _TrackerSparqlBufferPrivate
{
	TrackerSparqlConnection *connection;
	guint flush_timeout_id;
	GPtrArray *tasks;
	guint n_updates;
};

struct _SparqlTaskData
{
	guint type;

	union {
		gchar *str;
		TrackerSparqlBuilder *builder;

		struct {
			gchar *str;
			guint flags;
		} bulk;
	} data;

	GSimpleAsyncResult *result;
};

struct _UpdateArrayData {
	TrackerSparqlBuffer *buffer;
	GPtrArray *tasks;
	GArray *sparql_array;
	GArray *error_map;
	GPtrArray *bulk_ops;
	guint n_bulk_operations;
};

struct _BulkOperationMerge {
	const gchar *bulk_operation;
	GList *tasks;
	gchar *sparql;
};



G_DEFINE_TYPE (TrackerSparqlBuffer, tracker_sparql_buffer, TRACKER_TYPE_TASK_POOL)

static void
tracker_sparql_buffer_finalize (GObject *object)
{
	TrackerSparqlBufferPrivate *priv;

	priv = TRACKER_SPARQL_BUFFER (object)->priv;

	if (priv->flush_timeout_id != 0) {
		g_source_remove (priv->flush_timeout_id);
	}

	G_OBJECT_CLASS (tracker_sparql_buffer_parent_class)->finalize (object);
}

static void
tracker_sparql_buffer_set_property (GObject      *object,
                                    guint         param_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
	TrackerSparqlBufferPrivate *priv;

	priv = TRACKER_SPARQL_BUFFER (object)->priv;

	switch (param_id) {
	case PROP_CONNECTION:
		priv->connection = g_value_dup_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
tracker_sparql_buffer_get_property (GObject    *object,
                                    guint       param_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
	TrackerSparqlBufferPrivate *priv;

	priv = TRACKER_SPARQL_BUFFER (object)->priv;

	switch (param_id) {
	case PROP_CONNECTION:
		g_value_set_object (value,
		                    priv->connection);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
tracker_sparql_buffer_class_init (TrackerSparqlBufferClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_sparql_buffer_finalize;
	object_class->set_property = tracker_sparql_buffer_set_property;
	object_class->get_property = tracker_sparql_buffer_get_property;

	g_object_class_install_property (object_class,
	                                 PROP_CONNECTION,
	                                 g_param_spec_object ("connection",
	                                                      "sparql connection",
	                                                      "Sparql Connection",
	                                                      TRACKER_SPARQL_TYPE_CONNECTION,
	                                                      G_PARAM_READWRITE |
	                                                      G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (object_class,
	                          sizeof (TrackerSparqlBufferPrivate));
}

static gboolean
flush_timeout_cb (gpointer user_data)
{
	TrackerSparqlBuffer *buffer = user_data;
	TrackerSparqlBufferPrivate *priv = buffer->priv;

	tracker_sparql_buffer_flush (buffer, "Buffer time reached");
	priv->flush_timeout_id = 0;

	return FALSE;
}

static void
reset_flush_timeout (TrackerSparqlBuffer *buffer)
{
	TrackerSparqlBufferPrivate *priv;

	priv = buffer->priv;

	if (priv->flush_timeout_id != 0) {
		g_source_remove (priv->flush_timeout_id);
	}

	priv->flush_timeout_id = g_timeout_add_seconds (MAX_SPARQL_BUFFER_TIME,
	                                                flush_timeout_cb,
	                                                buffer);
}

static void
tracker_sparql_buffer_init (TrackerSparqlBuffer *buffer)
{
	buffer->priv = G_TYPE_INSTANCE_GET_PRIVATE (buffer,
	                                            TRACKER_TYPE_SPARQL_BUFFER,
	                                            TrackerSparqlBufferPrivate);
}

TrackerSparqlBuffer *
tracker_sparql_buffer_new (TrackerSparqlConnection *connection,
                           guint                    limit)
{
	return g_object_new (TRACKER_TYPE_SPARQL_BUFFER,
	                     "connection", connection,
	                     "limit", limit,
	                     NULL);
}

static void
remove_task_foreach (TrackerTask     *task,
                     TrackerTaskPool *pool)
{
	tracker_task_pool_remove (pool, task);
}

static void
update_array_data_free (UpdateArrayData *update_data)
{
	if (!update_data)
		return;

	if (update_data->sparql_array) {
		/* The array contains pointers to strings in the tasks, so no need to
		 * deallocate its pointed contents, just the array itself. */
		g_array_free (update_data->sparql_array, TRUE);
	}

	if (update_data->bulk_ops) {
		/* The BulkOperationMerge structs which contain the sparql strings
		 * are deallocated here */
		g_ptr_array_free (update_data->bulk_ops, TRUE);
	}

	g_ptr_array_foreach (update_data->tasks,
	                     (GFunc) remove_task_foreach,
	                     update_data->buffer);
	g_ptr_array_free (update_data->tasks, TRUE);

	g_array_free (update_data->error_map, TRUE);
	g_slice_free (UpdateArrayData, update_data);
}

static void
tracker_sparql_buffer_update_array_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
	TrackerSparqlBufferPrivate *priv;
	GError *global_error = NULL;
	GPtrArray *sparql_array_errors;
	UpdateArrayData *update_data;
	guint i;

	/* Get arrays of errors and queries */
	update_data = user_data;
	priv = TRACKER_SPARQL_BUFFER (update_data->buffer)->priv;
	priv->n_updates--;

	g_message ("(Sparql buffer) Finished array-update with %u tasks",
	           update_data->tasks->len);

	sparql_array_errors = tracker_sparql_connection_update_array_finish (priv->connection,
	                                                                     result,
	                                                                     &global_error);
	if (global_error) {
		g_critical ("  (Sparql buffer) Error in array-update: %s",
		            global_error->message);
	}

	/* Report status on each task of the batch update */
	for (i = 0; i < update_data->tasks->len; i++) {
		TrackerTask *task;
		SparqlTaskData *task_data;
		GError *error = NULL;

		task = g_ptr_array_index (update_data->tasks, i);
		task_data = tracker_task_get_data (task);

		if (global_error) {
			error = global_error;
		} else {
			gint error_pos;

			error_pos = g_array_index (update_data->error_map, gint, i);

			/* Find the corresponing error according to the passed map,
			 * numbers >= 0 are non-bulk tasks, and < 0 are bulk tasks,
			 * so the number of bulk operations must be added, as these
			 * tasks are prepended.
			 */
			error_pos += update_data->n_bulk_operations;
			error = g_ptr_array_index (sparql_array_errors, error_pos);
		}

		/* Call finished handler with the error, if any */
		g_simple_async_result_set_op_res_gpointer (task_data->result,
		                                           task, NULL);
		if (error) {
			g_simple_async_result_set_from_error (task_data->result, error);
		}

		g_simple_async_result_complete (task_data->result);

		/* No need to deallocate the task here, it will be done when
		 * unref-ing the UpdateArrayData below */
	}

	/* Unref the arrays of errors and queries */
	if (sparql_array_errors) {
		g_ptr_array_unref (sparql_array_errors);
	}

	/* Note that tasks are actually deallocated here */
	update_array_data_free (update_data);

	if (global_error) {
		g_error_free (global_error);
	}
}

static void
bulk_operation_merge_finish (BulkOperationMerge *merge)
{
	if (merge->sparql) {
		g_free (merge->sparql);
		merge->sparql = NULL;
	}

	if (merge->bulk_operation && merge->tasks) {
		GString *equals_string = NULL, *children_string = NULL, *sparql;
		guint n_equals = 0;
		GList *l;

		for (l = merge->tasks; l; l = l->next) {
			SparqlTaskData *task_data;
			TrackerTask *task = l->data;
			gchar *uri;

			task_data = tracker_task_get_data (task);
			uri = g_file_get_uri (tracker_task_get_file (task));

			if (task_data->data.bulk.flags & TRACKER_BULK_MATCH_EQUALS) {
				if (!equals_string) {
					equals_string = g_string_new ("");
				} else {
					g_string_append_c (equals_string, ',');
				}

				g_string_append_printf (equals_string, "\"%s\"", uri);
				n_equals++;
			}

			if (task_data->data.bulk.flags & TRACKER_BULK_MATCH_CHILDREN) {
				if (!children_string) {
					children_string = g_string_new ("");
				} else {
					g_string_append_c (children_string, ',');
				}

				g_string_append_printf (children_string, "\"%s\"", uri);
			}

			g_free (uri);
		}

		sparql = g_string_new ("");

		if (equals_string) {
			g_string_append (sparql, merge->bulk_operation);

			if (n_equals == 1) {
				g_string_append_printf (sparql,
				                        " WHERE { "
				                        "  ?f nie:url %s"
				                        "} ",
				                        equals_string->str);
			} else {
				g_string_append_printf (sparql,
				                        " WHERE { "
				                        "  ?f nie:url ?u ."
				                        "  FILTER (?u IN (%s))"
				                        "} ",
				                        equals_string->str);
			}

			g_string_free (equals_string, TRUE);
		}

		if (children_string) {
			g_string_append (sparql, merge->bulk_operation);
			g_string_append_printf (sparql,
			                        " WHERE { "
			                        "  ?f nie:url ?u ."
			                        "  FILTER (tracker:uri-is-descendant (%s, ?u))"
			                        "} ",
			                        children_string->str);
			g_string_free (children_string, TRUE);
		}

		merge->sparql = g_string_free (sparql, FALSE);
	}
}

static BulkOperationMerge *
bulk_operation_merge_new (const gchar *bulk_operation)
{
	BulkOperationMerge *operation;

	operation = g_slice_new0 (BulkOperationMerge);
	operation->bulk_operation = bulk_operation;

	return operation;
}

static void
bulk_operation_merge_free (BulkOperationMerge *operation)
{
	g_list_foreach (operation->tasks,
	                (GFunc) tracker_task_unref,
	                NULL);
	g_list_free (operation->tasks);
	g_free (operation->sparql);
	g_slice_free (BulkOperationMerge, operation);
}

gboolean
tracker_sparql_buffer_flush (TrackerSparqlBuffer *buffer,
                             const gchar         *reason)
{
	TrackerSparqlBufferPrivate *priv;
	GPtrArray *bulk_ops = NULL;
	GArray *sparql_array, *error_map;
	UpdateArrayData *update_data;
	guint i, j;

	priv = buffer->priv;

	if (!priv->tasks ||
	    priv->tasks->len == 0) {
		return FALSE;
	}

	g_message ("Flushing SPARQL buffer, reason: %s", reason);

	if (priv->flush_timeout_id != 0) {
		g_source_remove (priv->flush_timeout_id);
		priv->flush_timeout_id = 0;
	}

	/* Loop buffer and construct array of strings */
	sparql_array = g_array_new (FALSE, TRUE, sizeof (gchar *));
	error_map = g_array_new (TRUE, TRUE, sizeof (gint));

	for (i = 0; i < priv->tasks->len; i++) {
		SparqlTaskData *task_data;
		TrackerTask *task;
		gint pos;

		task = g_ptr_array_index (priv->tasks, i);
		task_data = tracker_task_get_data (task);

		if (task_data->type == TASK_TYPE_SPARQL_STR) {
			g_array_append_val (sparql_array, task_data->data.str);
			pos = sparql_array->len - 1;
		} else if (task_data->type == TASK_TYPE_SPARQL) {
			const gchar *str;

			str = tracker_sparql_builder_get_result (task_data->data.builder);
			g_array_append_val (sparql_array, str);
			pos = sparql_array->len - 1;
		} else if (task_data->type == TASK_TYPE_BULK) {
			BulkOperationMerge *bulk = NULL;
			gint j;

			if (G_UNLIKELY (!bulk_ops)) {
				bulk_ops = g_ptr_array_new_with_free_func ((GDestroyNotify) bulk_operation_merge_free);
			}

			for (j = 0; j < bulk_ops->len; j++) {
				BulkOperationMerge *cur;

				cur = g_ptr_array_index (bulk_ops, j);

				/* This is a comparison of intern strings */
				if (cur->bulk_operation == task_data->data.bulk.str) {
					bulk = cur;
					pos = - 1 - j;
					break;
				}
			}

			if (!bulk) {
				bulk = bulk_operation_merge_new (task_data->data.bulk.str);
				g_ptr_array_add (bulk_ops, bulk);
				pos = - bulk_ops->len;
			}

			bulk->tasks = g_list_prepend (bulk->tasks,
			                              tracker_task_ref (task));
		}

		g_array_append_val (error_map, pos);
	}

	if (bulk_ops) {
		for (j = 0; j < bulk_ops->len; j++) {
			BulkOperationMerge *bulk;

			bulk = g_ptr_array_index (bulk_ops, j);
			bulk_operation_merge_finish (bulk);

			if (bulk->sparql) {
				g_array_prepend_val (sparql_array,
				                     bulk->sparql);
			}
		}
	}

	update_data = g_slice_new0 (UpdateArrayData);
	update_data->buffer = buffer;
	update_data->tasks = g_ptr_array_ref (priv->tasks);
	update_data->bulk_ops = bulk_ops;
	update_data->n_bulk_operations = bulk_ops ? bulk_ops->len : 0;
	update_data->error_map = error_map;
	update_data->sparql_array = sparql_array;

	/* Empty pool, update_data will keep
	 * references to the tasks to keep
	 * these alive.
	 */
	g_ptr_array_unref (priv->tasks);
	priv->tasks = NULL;
	priv->n_updates++;

	/* Start the update */
	tracker_sparql_connection_update_array_async (priv->connection,
	                                              (gchar **) update_data->sparql_array->data,
	                                              update_data->sparql_array->len,
	                                              G_PRIORITY_DEFAULT,
	                                              NULL,
	                                              tracker_sparql_buffer_update_array_cb,
	                                              update_data);

	return TRUE;
}

void
tracker_sparql_buffer_push (TrackerSparqlBuffer *buffer,
                            TrackerTask         *task,
                            GAsyncReadyCallback  cb,
                            gpointer             user_data)
{
	TrackerSparqlBufferPrivate *priv;
	SparqlTaskData *data;

	g_return_if_fail (TRACKER_IS_SPARQL_BUFFER (buffer));
	g_return_if_fail (task != NULL);

	priv = buffer->priv;

	if (tracker_task_pool_get_size (TRACKER_TASK_POOL (buffer)) == 0) {
		reset_flush_timeout (buffer);
	}

	tracker_task_pool_add (TRACKER_TASK_POOL (buffer), task);

	if (!priv->tasks) {
		priv->tasks = g_ptr_array_new_with_free_func ((GDestroyNotify) tracker_task_unref);
	}

	g_ptr_array_add (priv->tasks, task);

	data = tracker_task_get_data (task);
	data->result = g_simple_async_result_new (G_OBJECT (buffer),
	                                          cb, user_data, NULL);

	if (tracker_task_pool_limit_reached (TRACKER_TASK_POOL (buffer))) {
		tracker_sparql_buffer_flush (buffer, "SPARQL buffer limit reached");
	} else if (priv->tasks->len > tracker_task_pool_get_limit (TRACKER_TASK_POOL (buffer)) / 2) {
		/* We've filled half of the buffer, flush it as we receive more tasks */
		tracker_sparql_buffer_flush (buffer, "SPARQL buffer half-full");
	}
}

static SparqlTaskData *
sparql_task_data_new (guint    type,
                      gpointer data,
                      guint    flags)
{
	SparqlTaskData *task_data;

	task_data = g_slice_new0 (SparqlTaskData);
	task_data->type = type;

	switch (type) {
	case TASK_TYPE_SPARQL_STR:
		task_data->data.str = data;
		break;
	case TASK_TYPE_SPARQL:
		task_data->data.builder = g_object_ref (data);
		break;
	case TASK_TYPE_BULK:
		task_data->data.bulk.str = data;
		task_data->data.bulk.flags = flags;
		break;
	}

	return task_data;
}

static void
sparql_task_data_free (SparqlTaskData *data)
{
	switch (data->type) {
	case TASK_TYPE_SPARQL_STR:
		g_free (data->data.str);
		break;
	case TASK_TYPE_SPARQL:
		g_object_unref (data->data.builder);
		break;
	case TASK_TYPE_BULK:
		/* nothing to free, the string is interned */
		break;
	}

	if (data->result) {
		g_object_unref (data->result);
	}

	g_slice_free (SparqlTaskData, data);
}

TrackerTask *
tracker_sparql_task_new_take_sparql_str (GFile *file,
                                         gchar *sparql_str)
{
	SparqlTaskData *data;

	data = sparql_task_data_new (TASK_TYPE_SPARQL_STR, sparql_str, 0);
	return tracker_task_new (file, data,
	                         (GDestroyNotify) sparql_task_data_free);
}

TrackerTask *
tracker_sparql_task_new_with_sparql_str (GFile       *file,
                                         const gchar *sparql_str)
{
	SparqlTaskData *data;

	data = sparql_task_data_new (TASK_TYPE_SPARQL_STR,
	                             g_strdup (sparql_str), 0);
	return tracker_task_new (file, data,
	                         (GDestroyNotify) sparql_task_data_free);
}

TrackerTask *
tracker_sparql_task_new_with_sparql (GFile                *file,
                                     TrackerSparqlBuilder *builder)
{
	SparqlTaskData *data;

	data = sparql_task_data_new (TASK_TYPE_SPARQL, builder, 0);
	return tracker_task_new (file, data,
	                         (GDestroyNotify) sparql_task_data_free);
}

TrackerTask *
tracker_sparql_task_new_bulk (GFile                *file,
                              const gchar          *sparql_str,
                              TrackerBulkTaskFlags  flags)
{
	SparqlTaskData *data;

	data = sparql_task_data_new (TASK_TYPE_BULK,
	                             (gchar *) g_intern_string (sparql_str),
	                             flags);
	return tracker_task_new (file, data,
	                         (GDestroyNotify) sparql_task_data_free);
}
