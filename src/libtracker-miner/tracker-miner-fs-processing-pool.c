/*
 * Copyright (C) 2009, 2010 Nokia <ivan.frade@nokia.com>
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


/*
 * How the processing pool works.
 *
 * 1. This processing pool is used to determine which files are being currently
 *    processed by tracker-miner-fs, and there are currently 3 kind of tasks
 *    considered in this pool:
 *   1.1. "WAIT" tasks are those used to specify tasks which still do not have a
 *        full SPARQL built. Currently, tasks in the WAIT status could be:
 *         o Tasks added while checking if the upper layer needs to process the
 *           given file (checked using the 'process-file' or
 *           'process-file-attributes' signals in TrackerMinerFS).
 *         o Tasks added while the upper layer is actually processing the given
 *           files (until the tracker_miner_fs_file_notify() is called by the
 *           upper layer).
 *   1.2. "READY" tasks are those used to specify tasks which have a proper
 *         SPARQL string ready to be pushed to tracker-store.
 *   1.3. "PROCESSING" tasks are those used to specify tasks that are currently
 *         being pushed to the store.
 *
 * 2. The current possible flows for tasks added to the processing pool are:
 *   2.1. Full SPARQL is ready before pushing the task to the pool. This is
 *        currently the case for DELETED or MOVED events, and the flow would be
 *        like this:
 *         - processing_task_new() to create a new task
 *         - processing_task_set_sparql() to set the full SPARQL in the task.
 *         - tracker_processing_pool_push_ready_task() to push the newly created
 *           task into the processing pool as a "READY" task.
 *
 *   2.2. The full SPARQL is still not available, as the upper layers need to
 *        process the file (like extracting metadata using tracker-extract
 *        in the case of TrackerMinerFiles). This case would correspond to
 *        CREATED or UPDATED events:
 *         - processing_task_new() to create a new task
 *         - tracker_processing_pool_push_wait_task() to push the newly created
 *           task into the processing pool as a "WAIT" task.
 *         - processing_task_set_sparql() to set the full SPARQL in the task
 *           (when the upper layers finished building it).
 *         - tracker_processing_pool_push_ready_task() to push the newly created
 *           task into the processing pool as a "READY" task.
 *
 *   2.3. Note that "PROCESSING" tasks are an internal status of the pool, the
 *        user of the processing pool cannot push a task with this status.
 *
 * 3. The number of tasks pushed to the pull as "WAIT" tasks is limited to the
 *    number set while creating the pool. This value corresponds to the
 *    "processing-pool-wait-limit" property in the TrackerMinerFS object, and
 *    currently is set to 10 for both TrackerMinerApplications and
 *    TrackerMinerFiles. In the case of TrackerMinerFiles, this number specifies
 *    the maximum number of extraction requests that can be managed in parallel.
 *
 * 4. The number of tasks pushed to the pull as "READY" tasks is limited to
 *    the number set while creating the pool. This value corresponds to the
 *    "processing-pool-ready-limit" property in the TrackerMinerFS object, and
 *    currently is set to 100 for both TrackerMinerApplications and
 *    TrackerMinerFiles. This number specifies the maximum number of SPARQL
 *    updates that can be merged into a single multi-insert SPARQL connection.
 *
 * 5. When a task is pushed to the pool as a "READY" task, the pool will be in
 *    charge of executing the SPARQL update into the store.
 *
 * 6. If buffering was requested when tracker_processing_pool_push_ready_task()
 *    was used to push the new task in the pool as a "READY" task, this task
 *    will be added internally into a SPARQL buffer. This SPARQL buffer will be
 *    flushed (pushing all collected SPARQL updates into the store) if one of
 *    these conditions is met:
 *      (a) The file corresponding to the task pushed doesn't have a parent.
 *      (b) The parent of the file corresponding to the task pushed is different
 *          to the parent of the last file pushed to the buffer.
 *      (c) The limit for "READY" tasks in the pool was reached.
 *      (d) The buffer was not flushed in the last MAX_SPARQL_BUFFER_TIME (=15)
 *          seconds.
 *    The buffer is flushed using a single multi-insert SPARQL connection. This
 *    means that an array of SPARQLs is sent to tracker-store, which replies
 *    with an array of GErrors specifying which update failed, if any.
 *    Once the flushing operation in the buffer is started, the tasks are then
 *    converted to "PROCESSING" state, until the reply from the store is
 *    received.
 *
 * 7. If buffering is not requested when
 *    tracker_processing_pool_push_ready_task() is called, first the previous
 *    buffer is flushed (if any) and then the current task is updated in the
 *    store, so this task goes directly from "READY" to "PROCESSING" state
 *    without going through the intermediate buffer.
 *
 * 8. May the gods be with you if you need to fix a bug in here. So say we all.
 *
 */

#include "config.h"
#include "tracker-miner-fs-processing-pool.h"

/* If defined, will dump additional traces */
#ifdef PROCESSING_POOL_ENABLE_TRACE
#warning Processing pool traces are enabled
#define POOL_STATUS_TRACE_TIMEOUT_SECS 10
#define trace(message, ...) g_debug (message, ##__VA_ARGS__)
#else
#define trace(...)
#endif /* PROCESSING_POOL_ENABLE_TRACE */

/* Maximum time (seconds) before forcing a sparql buffer flush */
#define MAX_SPARQL_BUFFER_TIME  15

typedef enum {
	TRACKER_PROCESSING_TASK_STATUS_NO_POOL = -1,
	TRACKER_PROCESSING_TASK_STATUS_WAIT = 0,
	TRACKER_PROCESSING_TASK_STATUS_READY,
	TRACKER_PROCESSING_TASK_STATUS_PROCESSING,
	TRACKER_PROCESSING_TASK_STATUS_LAST
} TrackerProcessingTaskStatus;

typedef struct {
	const gchar *bulk_operation;
	GList *tasks;
	gchar *sparql;
} BulkOperationMerge;

typedef enum {
	CONTENT_NONE,
	CONTENT_SPARQL_STRING,
	CONTENT_SPARQL_BUILDER,
	CONTENT_BULK_OPERATION
} TaskContentType;

struct _TrackerProcessingTask {
	/* The file being processed */
	GFile *file;

	TaskContentType content;

	union {
		TrackerSparqlBuilder *builder;
		gchar *string;
		struct {
			const gchar *bulk_operation;
			TrackerBulkMatchType match;
		} bulk;
	} data;

	/* The context of the task */
	gpointer context;
	/* The context deallocation method, if any */
	GFreeFunc context_free_func;

	/* Internal status of the task */
	TrackerProcessingTaskStatus status;
	/* The pool where the task was added */
	TrackerProcessingPool *pool;

	/* Handler and user_data to use when task is fully processed */
	TrackerProcessingPoolTaskFinishedCallback finished_handler;
	gpointer finished_user_data;

#ifdef PROCESSING_POOL_ENABLE_TRACE
	/* File URI, useful for logs */
	gchar *file_uri;
#endif /* PROCESSING_POOL_ENABLE_TRACE */
};

struct _TrackerProcessingPool {
	/* Owner of the pool */
	TrackerMinerFS *miner;

	/* The tasks currently in WAIT or PROCESS status */
	GQueue *tasks[TRACKER_PROCESSING_TASK_STATUS_LAST];
	/* The processing pool limits */
	guint limit[TRACKER_PROCESSING_TASK_STATUS_LAST];

	/* The current number of requests sent to the store */
	guint n_requests;
	/* The limit for number of requests sent to the store */
	guint limit_n_requests;
	/* The list of UpdateArrayData items pending to be flushed, blocked
	 * because the maximum number of requests was reached */
	GQueue *pending_requests;

	/* SPARQL buffer to pile up several UPDATEs */
	GPtrArray *sparql_buffer;
	GFile *sparql_buffer_current_parent;
	time_t sparql_buffer_start_time;

#ifdef PROCESSING_POOL_ENABLE_TRACE
	/* Timeout to notify status of the queues, if traces
	 * enabled only. */
	guint timeout_id;
#endif /* PROCESSING_POOL_ENABLE_TRACE */
};

typedef struct {
	TrackerProcessingPool *pool;
	GPtrArray *tasks;
	GArray *sparql_array;
	GArray *error_map;
	GPtrArray *bulk_ops;
	guint n_bulk_operations;
} UpdateArrayData;

/*------------------- PROCESSING TASK ----------------------*/

TrackerProcessingTask *
tracker_processing_task_new (GFile *file)
{
	TrackerProcessingTask *task;

	task = g_slice_new0 (TrackerProcessingTask);
	task->file = g_object_ref (file);
	task->status = TRACKER_PROCESSING_TASK_STATUS_NO_POOL;

#ifdef PROCESSING_POOL_ENABLE_TRACE
	task->file_uri = g_file_get_uri (task->file);
#endif /* PROCESSING_POOL_ENABLE_TRACE */

	return task;
}

static void
tracker_processing_task_data_unset (TrackerProcessingTask *task)
{
	if (task->content == CONTENT_SPARQL_STRING) {
		g_free (task->data.string);
	} else if (task->content == CONTENT_SPARQL_BUILDER) {
		if (task->data.builder) {
			g_object_unref (task->data.builder);
		}
	}

	task->content = CONTENT_NONE;
}

void
tracker_processing_task_free (TrackerProcessingTask *task)
{
	if (!task)
		return;

#ifdef PROCESSING_POOL_ENABLE_TRACE
	g_free (task->file_uri);
#endif /* PROCESSING_POOL_ENABLE_TRACE */

	/* Free context if requested to do so */
	if (task->context &&
	    task->context_free_func) {
		task->context_free_func (task->context);
	}

	tracker_processing_task_data_unset (task);

	g_object_unref (task->file);
	g_slice_free (TrackerProcessingTask, task);
}

GFile *
tracker_processing_task_get_file (TrackerProcessingTask *task)
{
	return task->file;
}

gpointer
tracker_processing_task_get_context (TrackerProcessingTask *task)
{
	return task->context;
}

void
tracker_processing_task_set_context (TrackerProcessingTask *task,
                                     gpointer               context,
                                     GFreeFunc              context_free_func)
{
	/* Free previous context if any and if requested to do so */
	if (task->context &&
	    task->context_free_func) {
		task->context_free_func (task->context);
	}

	task->context = context;
	task->context_free_func = context_free_func;
}

void
tracker_processing_task_set_sparql (TrackerProcessingTask *task,
                                    TrackerSparqlBuilder  *sparql)
{
	tracker_processing_task_data_unset (task);

	if (sparql) {
		task->data.builder = g_object_ref (sparql);
		task->content = CONTENT_SPARQL_BUILDER;
	}
}

void
tracker_processing_task_set_sparql_string (TrackerProcessingTask *task,
                                           gchar                 *sparql_string)
{
	tracker_processing_task_data_unset (task);

	if (sparql_string) {
		/* We take ownership of the input string! */
		task->data.string = sparql_string;
		task->content = CONTENT_SPARQL_STRING;
	}
}

void
tracker_processing_task_set_bulk_operation (TrackerProcessingTask *task,
                                            const gchar           *sparql,
                                            TrackerBulkMatchType   match)
{
	tracker_processing_task_data_unset (task);

	if (sparql) {
		/* This string is expected to remain constant */
		task->data.bulk.bulk_operation = sparql;
		task->data.bulk.match = match;
		task->content = CONTENT_BULK_OPERATION;
	}
}

/*------------------- PROCESSING POOL ----------------------*/

#ifdef PROCESSING_POOL_ENABLE_TRACE
static const gchar *queue_names [TRACKER_PROCESSING_TASK_STATUS_LAST] = {
	"WAIT",
	"READY",
	"PROCESSING"
};

static gboolean
pool_status_trace_timeout_cb (gpointer data)
{
	TrackerProcessingPool *pool = data;
	guint i;

	trace ("(Processing Pool %s) ------------",
	       G_OBJECT_TYPE_NAME (pool->miner));
	for (i = TRACKER_PROCESSING_TASK_STATUS_WAIT;
	     i < TRACKER_PROCESSING_TASK_STATUS_LAST;
	     i++) {
		GList *l;

		l = g_queue_peek_head_link (pool->tasks[i]);
		trace ("(Processing Pool %s) Queue %s has %u tasks",
		       G_OBJECT_TYPE_NAME (pool->miner),
		       queue_names[i],
		       g_list_length (l));
		while (l) {
			trace ("(Processing Pool %s)     Task %p in queue %s",
			       G_OBJECT_TYPE_NAME (pool->miner),
			       l->data,
			       queue_names[i]);
			l = g_list_next (l);
		}
	}
	trace ("(Processing Pool %s) Requests being currently processed: %u "
	       "(max: %u, pending: %u)",
	       G_OBJECT_TYPE_NAME (pool->miner),
	       pool->n_requests,
	       pool->limit_n_requests,
	       pool->pending_requests->length);
	return TRUE;
}
#endif /* PROCESSING_POOL_ENABLE_TRACE */

static void
pool_queue_free_foreach (gpointer data,
                         gpointer user_data)
{
	GPtrArray *sparql_buffer = user_data;

	/* If found in the SPARQL buffer, remove it (will call task_free itself) */
	if (!g_ptr_array_remove (sparql_buffer, data)) {
		/* If not removed from the array, free it ourselves */
		tracker_processing_task_free (data);
	}
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

	g_ptr_array_free (update_data->tasks, TRUE);
	g_array_free (update_data->error_map, TRUE);
	g_slice_free (UpdateArrayData, update_data);
}

void
tracker_processing_pool_free (TrackerProcessingPool *pool)
{
	guint i;

	if (!pool)
		return;

#ifdef PROCESSING_POOL_ENABLE_TRACE
	if (pool->timeout_id)
		g_source_remove (pool->timeout_id);
#endif /* PROCESSING_POOL_ENABLE_TRACE */

	g_queue_foreach (pool->pending_requests,
	                 (GFunc)update_array_data_free,
	                 NULL);
	g_queue_free (pool->pending_requests);

	/* Free any pending task here... shouldn't really
	 * be any */
	for (i = TRACKER_PROCESSING_TASK_STATUS_WAIT;
	     i < TRACKER_PROCESSING_TASK_STATUS_LAST;
	     i++) {
		g_queue_foreach (pool->tasks[i],
		                 pool_queue_free_foreach,
		                 pool->sparql_buffer);
		g_queue_free (pool->tasks[i]);
	}

	if (pool->sparql_buffer_current_parent) {
		g_object_unref (pool->sparql_buffer_current_parent);
	}

	if (pool->sparql_buffer) {
		g_ptr_array_free (pool->sparql_buffer, TRUE);
	}

	g_free (pool);
	                 }

TrackerProcessingPool *
tracker_processing_pool_new (TrackerMinerFS *miner,
                             guint           limit_wait,
                             guint           limit_ready,
                             guint           limit_n_requests)
{
	TrackerProcessingPool *pool;

	pool = g_new0 (TrackerProcessingPool, 1);

	pool->miner = miner;
	pool->limit[TRACKER_PROCESSING_TASK_STATUS_WAIT] = limit_wait;
	pool->limit[TRACKER_PROCESSING_TASK_STATUS_READY] = limit_ready;
	/* convenience limit, not really used currently */
	pool->limit[TRACKER_PROCESSING_TASK_STATUS_PROCESSING] = G_MAXUINT;
	pool->limit_n_requests = limit_n_requests;

	pool->tasks[TRACKER_PROCESSING_TASK_STATUS_WAIT] = g_queue_new ();
	pool->tasks[TRACKER_PROCESSING_TASK_STATUS_READY] = g_queue_new ();
	pool->tasks[TRACKER_PROCESSING_TASK_STATUS_PROCESSING] = g_queue_new ();

	pool->pending_requests = g_queue_new ();

	g_debug ("Processing pool created with a limit of "
	         "%u tasks in WAIT status, "
	         "%u tasks in READY status and "
	         "%u requests",
	         limit_wait,
	         limit_ready,
	         limit_n_requests);

#ifdef PROCESSING_POOL_ENABLE_TRACE
	pool->timeout_id = g_timeout_add_seconds (POOL_STATUS_TRACE_TIMEOUT_SECS,
	                                          pool_status_trace_timeout_cb,
	                                          pool);
#endif /* PROCESSING_POOL_ENABLE_TRACE */

	return pool;
}

void
tracker_processing_pool_set_wait_limit (TrackerProcessingPool *pool,
                                        guint                  limit)
{
	g_message ("Processing pool limit for WAIT tasks set to %u",
	           limit);
	pool->limit[TRACKER_PROCESSING_TASK_STATUS_WAIT] = limit;
}

void
tracker_processing_pool_set_ready_limit (TrackerProcessingPool *pool,
                                         guint                  limit)
{
	g_message ("Processing pool limit for READY tasks set to %u",
	           limit);
	pool->limit[TRACKER_PROCESSING_TASK_STATUS_READY] = limit;
}

void
tracker_processing_pool_set_n_requests_limit (TrackerProcessingPool *pool,
                                              guint                  limit)
{
	g_message ("Processing pool limit for number of requests set to %u",
	           limit);
	pool->limit_n_requests = limit;
}

guint
tracker_processing_pool_get_wait_limit (TrackerProcessingPool *pool)
{
	return pool->limit[TRACKER_PROCESSING_TASK_STATUS_WAIT];
}

guint
tracker_processing_pool_get_ready_limit (TrackerProcessingPool *pool)
{
	return pool->limit[TRACKER_PROCESSING_TASK_STATUS_READY];
}

guint
tracker_processing_pool_get_n_requests_limit (TrackerProcessingPool *pool)
{
	return pool->limit_n_requests;
}

gboolean
tracker_processing_pool_wait_limit_reached (TrackerProcessingPool *pool)
{
	return ((g_queue_get_length (pool->tasks[TRACKER_PROCESSING_TASK_STATUS_WAIT]) >=
	         pool->limit[TRACKER_PROCESSING_TASK_STATUS_WAIT]) ?
	        TRUE : FALSE);
}

static gboolean
tracker_processing_pool_ready_limit_reached (TrackerProcessingPool *pool)
{
	return ((g_queue_get_length (pool->tasks[TRACKER_PROCESSING_TASK_STATUS_READY]) >=
	         pool->limit[TRACKER_PROCESSING_TASK_STATUS_READY]) ?
	        TRUE : FALSE);
}

static gboolean
tracker_processing_pool_n_requests_limit_reached (TrackerProcessingPool *pool)
{
	return (pool->n_requests >= pool->limit_n_requests ? TRUE : FALSE);
}

TrackerProcessingTask *
tracker_processing_pool_find_task (TrackerProcessingPool *pool,
                                   GFile                 *file,
                                   gboolean               path_search)
{
	guint i;

	for (i = TRACKER_PROCESSING_TASK_STATUS_WAIT;
	     i < TRACKER_PROCESSING_TASK_STATUS_LAST;
	     i++) {
		GList *l;

		for (l = pool->tasks[i]->head; l; l = g_list_next (l)) {
			TrackerProcessingTask *task = l->data;

			if (!path_search) {
				/* Different operations for the same file URI could be
				 * piled up here, each being a different GFile object.
				 * Miner implementations should really notify on the
				 * same GFile object that's being passed, so we check for
				 * pointer equality here, rather than doing path comparisons
				 */
				if (task->file == file)
					return task;
			} else {
				/* Note that if there are different GFiles being
				 * processed for the same file path, we are actually
				 * returning the first one found, If you want exactly
				 * the same GFile as the one as input, use the
				 * process_data_find() method instead */
				if (g_file_equal (task->file, file))
					return task;
			}
		}
	}

	/* Not found... */
	return NULL;
}

void
tracker_processing_pool_push_wait_task (TrackerProcessingPool *pool,
                                        TrackerProcessingTask *task)
{
	g_assert (task->status == TRACKER_PROCESSING_TASK_STATUS_NO_POOL);

	/* Set status of the task as WAIT */
	task->status = TRACKER_PROCESSING_TASK_STATUS_WAIT;

	trace ("(Processing Pool %s) Pushed WAIT task %p for file '%s'",
	       G_OBJECT_TYPE_NAME (pool->miner),
	       task,
	       task->file_uri);

	/* Push a new task in WAIT status (so just add it to the tasks queue,
	 * and don't process it. */
	g_queue_push_head (pool->tasks[TRACKER_PROCESSING_TASK_STATUS_WAIT], task);
	task->pool = pool;
}

static void
tracker_processing_pool_sparql_update_array_cb (GObject      *object,
                                                GAsyncResult *result,
                                                gpointer      user_data)
{
	TrackerProcessingPool *pool;
	GError *global_error = NULL;
	GPtrArray *sparql_array_errors;
	UpdateArrayData *update_data;
	gboolean flush_next;
	guint i;

	/* Get arrays of errors and queries */
	update_data = user_data;
	pool = update_data->pool;

	/* If we had reached the limit of requests, flush next as this request is
	 * just finished */
	flush_next = tracker_processing_pool_n_requests_limit_reached (pool);

	/* Request finished */
	pool->n_requests--;

	trace ("(Processing Pool) Finished array-update %p with %u tasks "
	       "(%u requests processing, %u requests queued)",
	       update_data->tasks,
	       update_data->tasks->len,
	       pool->n_requests,
	       pool->pending_requests->length);

	sparql_array_errors = tracker_sparql_connection_update_array_finish (TRACKER_SPARQL_CONNECTION (object),
	                                                                     result,
	                                                                     &global_error);
	if (global_error) {
		g_critical ("(Processing Pool) Could not execute array-update of tasks %p with '%u' items: %s",
		            update_data->tasks,
		            update_data->tasks->len,
		            global_error->message);
	}

	/* Report status on each task of the batch update */
	for (i = 0; i < update_data->tasks->len; i++) {
		TrackerProcessingTask *task;
		GError *error = NULL;

		task = g_ptr_array_index (update_data->tasks, i);

		/* Before calling user-provided callback, REMOVE the task from the pool;
		 * as the user-provided callback may actually modify the pool again */
		tracker_processing_pool_remove_task (task->pool, task);

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
		task->finished_handler (task, task->finished_user_data, error);

		/* No need to deallocate the task here, it will be done when
		 * unref-ing the UpdateArrayData below */
	}

	/* Unref the arrays of errors and queries */
	if (sparql_array_errors)
		g_ptr_array_unref (sparql_array_errors);

	/* Note that tasks are actually deallocated here */
	update_array_data_free (update_data);

	if (global_error) {
		g_error_free (global_error);
	}

	/* Flush if needed */
	if (flush_next) {
		tracker_processing_pool_buffer_flush (pool,
		                                      "Pool request limit was reached and "
		                                      "UpdateArrayrequest just finished");
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
			TrackerProcessingTask *task = l->data;
			gchar *uri;

			uri = g_file_get_uri (task->file);

			if (task->data.bulk.match & TRACKER_BULK_MATCH_EQUALS) {
				if (!equals_string) {
					equals_string = g_string_new ("");
				} else {
					g_string_append_c (equals_string, ',');
				}

				g_string_append_printf (equals_string, "\"%s\"", uri);
				n_equals++;
			}

			if (task->data.bulk.match & TRACKER_BULK_MATCH_CHILDREN) {
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
	g_list_free (operation->tasks);
	g_free (operation->sparql);
	g_slice_free (BulkOperationMerge, operation);
}

static void
processing_pool_update_array_flush (TrackerProcessingPool *pool,
                                    UpdateArrayData       *update_data,
                                    const gchar           *reason)
{
	/* This method will flush the UpdateArrayData passed as
	 * argument if:
	 *  - The threshold of requests not reached.
	 *  - There is no other pending request to flush.
	 *
	 * Otherwise, the passed UpdateArrayData will be queued (if any) and the
	 * first one in the pending queue will get flushed.
	 */
	UpdateArrayData *to_flush;

	/* If we cannot flush anything or existing pending requests to flush,
	 * just queue the UpdateArrayData if any */
	if (tracker_processing_pool_n_requests_limit_reached (pool)) {
		/* If we hit the threshold, there's nothing to flush */
		to_flush = NULL;

		if (update_data) {
			trace ("(Processing Pool %s) Queueing array-update of tasks %p with %u items "
			       "(%s, threshold reached)",
			       G_OBJECT_TYPE_NAME (pool->miner),
			       update_data->tasks,
			       update_data->tasks->len,
			       reason ? reason : "Unknown reason");
			g_queue_push_tail (pool->pending_requests, update_data);
		}
	} else if (pool->pending_requests->length > 0) {
		/* There are other pending tasks to be flushed, we need to queue this one if any. */
		to_flush = g_queue_pop_head (pool->pending_requests);

		if (update_data) {
			trace ("(Processing Pool %s) Queueing array-update of tasks %p with %u items "
			       "(%s, pending requests first)",
			       G_OBJECT_TYPE_NAME (pool->miner),
			       update_data->tasks,
			       update_data->tasks->len,
			       reason ? reason : "Unknown reason");
			g_queue_push_tail (pool->pending_requests, update_data);
		}
	} else {
		/* No pending requests, flush the received UpdateArrayData, if any */
		to_flush = update_data;
	}

	/* If nothing to flush, return */
	if (!to_flush)
		return;

	trace ("(Processing Pool %s) Flushing array-update of tasks %p with %u items (%s)",
	       G_OBJECT_TYPE_NAME (pool->miner),
	       to_flush->tasks,
	       to_flush->tasks->len,
	       reason ? reason : "Unknown reason");

	/* New Request */
	pool->n_requests++;

	tracker_sparql_connection_update_array_async (tracker_miner_get_connection (TRACKER_MINER (pool->miner)),
	                                              (gchar **) to_flush->sparql_array->data,
	                                              to_flush->sparql_array->len,
	                                              G_PRIORITY_DEFAULT,
	                                              NULL,
	                                              tracker_processing_pool_sparql_update_array_cb,
	                                              to_flush);
}

void
tracker_processing_pool_buffer_flush (TrackerProcessingPool *pool,
                                      const gchar           *reason)
{
	GPtrArray *bulk_ops = NULL;
	GArray *sparql_array, *error_map;
	UpdateArrayData *update_data;
	guint i, j;

	/* If no sparql buffer, flush any pending request, if any;
	 * or just return otherwise */
	if (!pool->sparql_buffer) {
		processing_pool_update_array_flush (pool, NULL, reason);
		return;
	}

	/* Loop buffer and construct array of strings */
	sparql_array = g_array_new (FALSE, TRUE, sizeof (gchar *));
	error_map = g_array_new (TRUE, TRUE, sizeof (gint));

	for (i = 0; i < pool->sparql_buffer->len; i++) {
		TrackerProcessingTask *task;
		gint pos;

		task = g_ptr_array_index (pool->sparql_buffer, i);

		/* Make sure it was a READY task */
		g_assert (task->status == TRACKER_PROCESSING_TASK_STATUS_READY);

		/* Remove the task from the READY queue and add it to the
		 * PROCESSING one. */
		tracker_processing_pool_remove_task (pool, task);
		task->status = TRACKER_PROCESSING_TASK_STATUS_PROCESSING;
		task->pool = pool;
		g_queue_push_head (pool->tasks[TRACKER_PROCESSING_TASK_STATUS_PROCESSING], task);

		if (task->content == CONTENT_SPARQL_STRING) {
			g_array_append_val (sparql_array, task->data.string);
			pos = sparql_array->len - 1;
		} else if (task->content == CONTENT_SPARQL_BUILDER) {
			const gchar *str = tracker_sparql_builder_get_result (task->data.builder);
			g_array_append_val (sparql_array, str);
			pos = sparql_array->len - 1;
		} else if (task->content == CONTENT_BULK_OPERATION) {
			BulkOperationMerge *bulk = NULL;
			gint j;

			if (G_UNLIKELY (!bulk_ops)) {
				bulk_ops = g_ptr_array_new_with_free_func ((GDestroyNotify) bulk_operation_merge_free);
			}

			for (j = 0; j < bulk_ops->len; j++) {
				BulkOperationMerge *cur;

				cur = g_ptr_array_index (bulk_ops, j);

				if (cur->bulk_operation == task->data.bulk.bulk_operation) {
					bulk = cur;
					pos = - 1 - j;
					break;
				}
			}

			if (!bulk) {
				bulk = bulk_operation_merge_new (task->data.bulk.bulk_operation);
				g_ptr_array_add (bulk_ops, bulk);
				pos = - bulk_ops->len;
			}

			bulk->tasks = g_list_prepend (bulk->tasks, task);
		}

		g_array_append_val (error_map, pos);
	}

	if (bulk_ops) {
		for (j = 0; j < bulk_ops->len; j++) {
			BulkOperationMerge *bulk;

			bulk = g_ptr_array_index (bulk_ops, j);
			bulk_operation_merge_finish (bulk);

			if (bulk->sparql) {
				g_array_prepend_val (sparql_array, bulk->sparql);
			}
		}
	}

	/* Create new UpdateArrayData with the contents, which take ownership
	 * of the SPARQL buffer. */
	update_data = g_slice_new0 (UpdateArrayData);
	update_data->pool = pool;
	update_data->tasks = pool->sparql_buffer;
	update_data->bulk_ops = bulk_ops;
	update_data->n_bulk_operations = bulk_ops ? bulk_ops->len : 0;
	update_data->error_map = error_map;
	update_data->sparql_array = sparql_array;

	/* Reset buffer in the pool */
	pool->sparql_buffer = NULL;
	pool->sparql_buffer_start_time = 0;

	/* Flush or queue... */
	processing_pool_update_array_flush (pool, update_data, reason);

	/* Clear current parent */
	if (pool->sparql_buffer_current_parent) {
		g_object_unref (pool->sparql_buffer_current_parent);
		pool->sparql_buffer_current_parent = NULL;
	}
}

gboolean
tracker_processing_pool_push_ready_task (TrackerProcessingPool                     *pool,
                                         TrackerProcessingTask                     *task,
                                         TrackerProcessingPoolTaskFinishedCallback  finished_handler,
                                         gpointer                                   user_data)
{
	GFile *parent;
	gboolean flushed = FALSE;
	GList *previous;

	/* The task MUST have a proper content here */
	g_assert (task->content != CONTENT_NONE);

	/* First, check if the task was already added as being WAITING */
	previous = g_queue_find (pool->tasks[TRACKER_PROCESSING_TASK_STATUS_WAIT], task);
	if (previous) {
		/* Make sure it was a WAIT task */
		g_assert (task->status == TRACKER_PROCESSING_TASK_STATUS_WAIT);
		/* Remove task from WAIT queue */
		g_queue_delete_link (pool->tasks[TRACKER_PROCESSING_TASK_STATUS_WAIT], previous);
	} else {
		/* Set pool */
		task->pool = pool;
	}

	task->finished_handler = finished_handler;
	task->finished_user_data = user_data;

	/* Set status of the task as READY */
	task->status = TRACKER_PROCESSING_TASK_STATUS_READY;
	g_queue_push_head (pool->tasks[TRACKER_PROCESSING_TASK_STATUS_READY], task);

	/* Get parent of this file we're updating/creating */
	parent = g_file_get_parent (task->file);

	/* Start buffer if not already done */
	if (!pool->sparql_buffer) {
		pool->sparql_buffer =
			g_ptr_array_new_with_free_func ((GDestroyNotify) tracker_processing_task_free);
		pool->sparql_buffer_start_time = time (NULL);
	}

	/* Set current parent if not set already */
	if (!pool->sparql_buffer_current_parent && parent) {
		pool->sparql_buffer_current_parent = g_object_ref (parent);
	}

	trace ("(Processing Pool %s) Pushed READY task %p for file '%s' into array %p",
	       G_OBJECT_TYPE_NAME (pool->miner),
	       task,
	       task->file_uri,
	       pool->sparql_buffer);

	/* Add task to array */
	g_ptr_array_add (pool->sparql_buffer, task);

	/* Flush buffer if:
	 *  - Last item has no parent
	 *  - Parent change was detected
	 *  - Maximum number of READY items reached
	 *  - Not flushed in the last MAX_SPARQL_BUFFER_TIME seconds
	 */
	if (!parent) {
		tracker_processing_pool_buffer_flush (pool,
		                                      "File with no parent");
		flushed = TRUE;
	} else if (!g_file_equal (parent, pool->sparql_buffer_current_parent)) {
		tracker_processing_pool_buffer_flush (pool,
		                                      "Different parent");
		flushed = TRUE;
	} else if (tracker_processing_pool_ready_limit_reached (pool)) {
		tracker_processing_pool_buffer_flush (pool,
		                                      "Ready limit reached");
		flushed = TRUE;
	} else if (time (NULL) - pool->sparql_buffer_start_time > MAX_SPARQL_BUFFER_TIME) {
		tracker_processing_pool_buffer_flush (pool,
		                                      "Buffer time reached");
		flushed = TRUE;
	}

	if (parent)
		g_object_unref (parent);

	return flushed;
}

void
tracker_processing_pool_remove_task (TrackerProcessingPool *pool,
                                     TrackerProcessingTask *task)
{
	/* Remove from pool without freeing it */
	GList *in_pool;

	g_assert (pool == task->pool);

	/* Make sure the task was in the pool */
	in_pool = g_queue_find (pool->tasks[task->status], task);
	g_assert (in_pool != NULL);

	g_queue_delete_link (pool->tasks[task->status], in_pool);
	task->pool = NULL;
	task->status = TRACKER_PROCESSING_TASK_STATUS_NO_POOL;
}

guint
tracker_processing_pool_get_wait_task_count (TrackerProcessingPool *pool)
{
	return g_queue_get_length (pool->tasks[TRACKER_PROCESSING_TASK_STATUS_WAIT]);
}

guint
tracker_processing_pool_get_total_task_count (TrackerProcessingPool *pool)
{
	guint total = 0;
	guint i;

	for (i = TRACKER_PROCESSING_TASK_STATUS_WAIT;
	     i < TRACKER_PROCESSING_TASK_STATUS_LAST;
	     i++) {
		total += g_queue_get_length (pool->tasks[i]);
	}
	return total;
}

TrackerProcessingTask *
tracker_processing_pool_get_last_wait (TrackerProcessingPool *pool)
{
	GList *li;

	for (li = pool->tasks[TRACKER_PROCESSING_TASK_STATUS_WAIT]->tail; li; li = g_list_previous (li)) {
		TrackerProcessingTask *task = li->data;

		if (task->status == TRACKER_PROCESSING_TASK_STATUS_WAIT) {
			return task;
		}
	}
	return NULL;
}

void
tracker_processing_pool_foreach (TrackerProcessingPool *pool,
                                 GFunc                  func,
                                 gpointer               user_data)
{
	guint i;

	for (i = TRACKER_PROCESSING_TASK_STATUS_WAIT;
	     i < TRACKER_PROCESSING_TASK_STATUS_LAST;
	     i++) {
		g_queue_foreach (pool->tasks[i], func, user_data);
	}
}
