/*
 * Copyright (C) 2011, Nokia <ivan.frade@nokia.com>
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
 * Author: Carlos Garnacho <carlos@lanedo.com>
 */

#include "config.h"

#include <libtracker-sparql/tracker-sparql.h>

#include "tracker-sparql-buffer.h"

/* Maximum time (seconds) before forcing a sparql buffer flush */
#define MAX_SPARQL_BUFFER_TIME  15

typedef struct _TrackerSparqlBufferPrivate TrackerSparqlBufferPrivate;
typedef struct _SparqlTaskData SparqlTaskData;
typedef struct _UpdateArrayData UpdateArrayData;
typedef struct _UpdateData UpdateData;
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
	gint n_updates;
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

	GTask *async_task;
};

struct _UpdateData {
	TrackerSparqlBuffer *buffer;
	TrackerTask *task;
};

struct _UpdateArrayData {
	TrackerSparqlBuffer *buffer;
	GPtrArray *tasks;
	GArray *sparql_array;
};

struct _BulkOperationMerge {
	const gchar *bulk_operation;
	GList *tasks;
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

	g_ptr_array_foreach (update_data->tasks,
	                     (GFunc) remove_task_foreach,
	                     update_data->buffer);
	g_ptr_array_free (update_data->tasks, TRUE);

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
	gint i;

	/* Get arrays of errors and queries */
	update_data = user_data;
	priv = TRACKER_SPARQL_BUFFER (update_data->buffer)->priv;
	priv->n_updates--;

	g_debug ("(Sparql buffer) Finished array-update with %u tasks",
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
			error = g_ptr_array_index (sparql_array_errors, i);
			if (error) {
				const gchar *sparql = NULL;
				GFile *file;
				gchar *uri;

				file = tracker_task_get_file (task);
				uri = g_file_get_uri (file);
				g_critical ("  (Sparql buffer) Error in task %u (%s) of the array-update: %s",
				            i, uri, error->message);
				g_free (uri);

				uri = g_file_get_uri (tracker_task_get_file (task));
				g_debug ("    Affected file: %s", uri);
				g_free (uri);

				switch (task_data->type) {
				case TASK_TYPE_SPARQL_STR:
					sparql = task_data->data.str;
					break;
				case TASK_TYPE_SPARQL:
					sparql = tracker_sparql_builder_get_result (task_data->data.builder);
					break;
				case TASK_TYPE_BULK:
					sparql = task_data->data.bulk.str;
					break;
				default:
					break;
				}

				if (sparql) {
					g_debug ("    Sparql: %s", sparql);
				}
			}
		}

		/* Call finished handler with the error, if any */
		if (error) {
			g_task_return_error (task_data->async_task,
			                     g_error_copy (error));
		} else {
			g_task_return_pointer (task_data->async_task, task, NULL);
		}

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

static gchar *
bulk_operation_merge_finish (BulkOperationMerge *merge)
{
	if (merge->bulk_operation && merge->tasks) {
		GString *equals_string = NULL, *children_string = NULL, *sparql;
		gint n_equals = 0;
		gboolean include_logical_resources = FALSE;
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
				gchar *dir_uri;

				if (!children_string) {
					children_string = g_string_new (NULL);
				} else {
					g_string_append (children_string, "||");
				}

				if (uri[strlen (uri) - 1] == '/')
					dir_uri = g_strdup (uri);
				else
					dir_uri = g_strdup_printf ("%s/", uri);

				g_string_append_printf (children_string,
				                        "STRSTARTS (?u, \"%s\")",
				                        dir_uri);
				g_free (dir_uri);
			}

			if (task_data->data.bulk.flags & TRACKER_BULK_MATCH_LOGICAL_RESOURCES) {
				include_logical_resources = TRUE;
			}

			g_free (uri);
		}

		sparql = g_string_new ("");

		if (equals_string) {
			g_string_append (sparql, merge->bulk_operation);
			g_string_append_printf (sparql, " WHERE { ");

			if (n_equals == 1) {
				g_string_append_printf (sparql,
				                        "  ?f nie:url %s .",
				                        equals_string->str);
			} else {
				g_string_append_printf (sparql,
				                        "  ?f nie:url ?u ."
				                        "  FILTER (?u IN (%s))",
				                        equals_string->str);
			}
			g_string_free (equals_string, TRUE);

			if (include_logical_resources) {
				g_string_append (sparql, "  ?ie nie:isStoredAs ?f .");
			}
			g_string_append_printf (sparql, " } ");
		}

		if (children_string) {
			g_string_append (sparql, merge->bulk_operation);
			g_string_append_printf (sparql,
			                        " WHERE { "
			                        "  ?f nie:url ?u ."
			                        "  FILTER (%s)",
			                        children_string->str);
			g_string_free (children_string, TRUE);

			if (include_logical_resources) {
				g_string_append (sparql, "  ?ie nie:isStoredAs ?f .");
			}
			g_string_append_printf (sparql, "} ");
		}

		return g_string_free (sparql, FALSE);
	}

	return NULL;
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
	g_slice_free (BulkOperationMerge, operation);
}

gboolean
tracker_sparql_buffer_flush (TrackerSparqlBuffer *buffer,
                             const gchar         *reason)
{
	TrackerSparqlBufferPrivate *priv;
	GArray *sparql_array;
	GPtrArray *bulk_sparql;
	UpdateArrayData *update_data;
	gint i;

	priv = buffer->priv;

	if (priv->n_updates > 0) {
		return FALSE;
	}

	if (!priv->tasks ||
	    priv->tasks->len == 0) {
		return FALSE;
	}

	g_debug ("Flushing SPARQL buffer, reason: %s", reason);

	if (priv->flush_timeout_id != 0) {
		g_source_remove (priv->flush_timeout_id);
		priv->flush_timeout_id = 0;
	}

	/* Loop buffer and construct array of strings */
	sparql_array = g_array_new (FALSE, TRUE, sizeof (gchar *));
	bulk_sparql = g_ptr_array_new_with_free_func ((GDestroyNotify) g_free);

	for (i = 0; i < priv->tasks->len; i++) {
		SparqlTaskData *task_data;
		TrackerTask *task;

		task = g_ptr_array_index (priv->tasks, i);
		task_data = tracker_task_get_data (task);

		if (task_data->type == TASK_TYPE_SPARQL_STR) {
			g_array_append_val (sparql_array, task_data->data.str);
		} else if (task_data->type == TASK_TYPE_SPARQL) {
			const gchar *str;

			str = tracker_sparql_builder_get_result (task_data->data.builder);
			g_array_append_val (sparql_array, str);
		} else if (task_data->type == TASK_TYPE_BULK) {
			BulkOperationMerge *bulk = NULL;
			gchar *str;

			bulk = bulk_operation_merge_new (task_data->data.bulk.str);
			bulk->tasks = g_list_prepend (bulk->tasks,
			                              tracker_task_ref (task));

			str = bulk_operation_merge_finish (bulk);
			g_ptr_array_add (bulk_sparql, str);
			g_array_append_val (sparql_array, str);

			bulk_operation_merge_free (bulk);
		}
	}

	update_data = g_slice_new0 (UpdateArrayData);
	update_data->buffer = buffer;
	update_data->tasks = g_ptr_array_ref (priv->tasks);
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

	/* These strings we generated here can be freed now */
	g_ptr_array_free (bulk_sparql, TRUE);

	return TRUE;
}

static void
tracker_sparql_buffer_update_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
	UpdateData *update_data = user_data;
	SparqlTaskData *task_data;
	GError *error = NULL;

	tracker_sparql_connection_update_finish (TRACKER_SPARQL_CONNECTION (object),
	                                         result, &error);

	task_data = tracker_task_get_data (update_data->task);

	/* Call finished handler with the error, if any */
	if (error) {
		g_task_return_error (task_data->async_task, error);
	} else {
		g_task_return_pointer (task_data->async_task,
		                       update_data->task, NULL);
	}

	tracker_task_pool_remove (TRACKER_TASK_POOL (update_data->buffer),
	                          update_data->task);
	g_slice_free (UpdateData, update_data);
}

static void
sparql_buffer_push_high_priority (TrackerSparqlBuffer *buffer,
                                  TrackerTask         *task,
                                  SparqlTaskData      *data)
{
	TrackerSparqlBufferPrivate *priv;
	UpdateData *update_data;
	const gchar *sparql = NULL;

	priv = buffer->priv;

	/* Task pool addition adds a reference (below) */
	update_data = g_slice_new0 (UpdateData);
	update_data->buffer = buffer;
	update_data->task = task;

	if (data->type == TASK_TYPE_SPARQL_STR) {
		sparql = data->data.str;
	} else if (data->type == TASK_TYPE_SPARQL) {
		sparql = tracker_sparql_builder_get_result (data->data.builder);
	}

	tracker_task_pool_add (TRACKER_TASK_POOL (buffer), task);
	tracker_sparql_connection_update_async (priv->connection,
	                                        sparql,
	                                        G_PRIORITY_HIGH,
	                                        NULL,
	                                        tracker_sparql_buffer_update_cb,
	                                        update_data);
}

static void
sparql_buffer_push_to_pool (TrackerSparqlBuffer *buffer,
                            TrackerTask         *task)
{
	TrackerSparqlBufferPrivate *priv;

	priv = buffer->priv;

	if (tracker_task_pool_get_size (TRACKER_TASK_POOL (buffer)) == 0) {
		reset_flush_timeout (buffer);
	}

	/* Task pool addition increments reference */
	tracker_task_pool_add (TRACKER_TASK_POOL (buffer), task);

	if (!priv->tasks) {
		priv->tasks = g_ptr_array_new_with_free_func ((GDestroyNotify) tracker_task_unref);
	}

	/* We add a reference here because we unref when removed from
	 * the GPtrArray. */
	g_ptr_array_add (priv->tasks, tracker_task_ref (task));

	if (tracker_task_pool_limit_reached (TRACKER_TASK_POOL (buffer))) {
		tracker_sparql_buffer_flush (buffer, "SPARQL buffer limit reached");
	} else if (priv->tasks->len > tracker_task_pool_get_limit (TRACKER_TASK_POOL (buffer)) / 2) {
		/* We've filled half of the buffer, flush it as we receive more tasks */
		tracker_sparql_buffer_flush (buffer, "SPARQL buffer half-full");
	}
}

void
tracker_sparql_buffer_push (TrackerSparqlBuffer *buffer,
                            TrackerTask         *task,
                            gint                 priority,
                            GAsyncReadyCallback  cb,
                            gpointer             user_data)
{
	SparqlTaskData *data;

	g_return_if_fail (TRACKER_IS_SPARQL_BUFFER (buffer));
	g_return_if_fail (task != NULL);

	/* NOTE: We don't own the task and if we want it we have to
	 * reference it, each function below references task in
	 * different ways.
	 */
	data = tracker_task_get_data (task);

	if (!data->async_task) {
		data->async_task = g_task_new (buffer, NULL, cb, user_data);
		g_task_set_task_data (data->async_task,
		                      tracker_task_ref (task),
		                      (GDestroyNotify) tracker_task_unref);
	}

	if (priority <= G_PRIORITY_HIGH &&
	    data->type != TASK_TYPE_BULK) {
		sparql_buffer_push_high_priority (buffer, task, data);
	} else {
		sparql_buffer_push_to_pool (buffer, task);
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

	if (data->async_task) {
		g_object_unref (data->async_task);
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

TrackerTask *
tracker_sparql_buffer_push_finish (TrackerSparqlBuffer  *buffer,
                                   GAsyncResult         *res,
                                   GError              **error)
{
	TrackerTask *task;

	g_return_val_if_fail (TRACKER_IS_SPARQL_BUFFER (buffer), NULL);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (res), NULL);
	g_return_val_if_fail (!error || !*error, NULL);

	task = g_task_propagate_pointer (G_TASK (res), error);

	if (!task)
		task = g_task_get_task_data (G_TASK (res));

	return task;
}
