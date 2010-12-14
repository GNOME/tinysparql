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
 *         SPARQL string ready to be pushed to tracker-store. If
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
 *    currently is set to 1 for TrackerMinerApplications and to 10 to
 *    TrackerMinerFiles. In the case of TrackerMinerFiles, this number specifies
 *    the maximum number of extraction requests that can be managed in parallel.
 *
 * 4. The number of tasks pushed to the pull as "READY" tasks is limited to
 *    the number set while creating the pool. This value corresponds to the
 *    "processing-pool-ready-limit" property in the TrackerMinerFS object, and
 *    currently is set to 1 for TrackerMinerApplications and to 100 to
 *    TrackerMinerFiles. In the case of TrackerMinerFiles, this number specifies
 *    the maximum number of SPARQL updates that can be merged into a single
 *    multi-insert SPARQL connection.
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

struct _TrackerProcessingTask {
	/* The file being processed */
	GFile *file;
	/* The FULL sparql to be updated in the store */
	TrackerSparqlBuilder *sparql;
	gchar *sparql_string;

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
	GObject *owner;
	/* Connection to the Store */
	TrackerSparqlConnection *connection;

	/* The tasks currently in WAIT or PROCESS status */
	GQueue *tasks[TRACKER_PROCESSING_TASK_STATUS_LAST];
	/* The processing pool limits */
	guint limit[TRACKER_PROCESSING_TASK_STATUS_LAST];

	/* SPARQL buffer to pile up several UPDATEs */
	GPtrArray *sparql_buffer;
	GFile *sparql_buffer_current_parent;
	time_t sparql_buffer_start_time;

	/* Timeout to notify status of the queues, if traces
	 * enabled only. */
#ifdef PROCESSING_POOL_ENABLE_TRACE
	guint timeout_id;
#endif /* PROCESSING_POOL_ENABLE_TRACE */
};

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
	if (task->sparql) {
		g_object_unref (task->sparql);
	}
	g_free (task->sparql_string);
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
	if (task->sparql) {
		g_object_unref (task->sparql);
	}
	if (task->sparql_string) {
		g_free (task->sparql_string);
		task->sparql_string = NULL;
	}
	task->sparql = g_object_ref (sparql);
}

void
tracker_processing_task_set_sparql_string (TrackerProcessingTask *task,
                                           gchar                 *sparql_string)
{
	if (task->sparql) {
		g_object_unref (task->sparql);
		task->sparql = NULL;
	}
	if (task->sparql_string) {
		g_free (task->sparql_string);
	}
	/* We take ownership of the input string! */
	task->sparql_string = sparql_string;
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
	       G_OBJECT_TYPE_NAME (pool->owner));
	for (i = TRACKER_PROCESSING_TASK_STATUS_WAIT;
	     i < TRACKER_PROCESSING_TASK_STATUS_LAST;
	     i++) {
		GList *l;

		l = g_queue_peek_head_link (pool->tasks[i]);
		trace ("(Processing Pool %s) Queue %s has %u tasks",
		       G_OBJECT_TYPE_NAME (pool->owner),
		       queue_names[i],
		       g_list_length (l));
		while (l) {
			trace ("(Processing Pool %s)     Task %p in queue %s",
			       G_OBJECT_TYPE_NAME (pool->owner),
			       l->data,
			       queue_names[i]);
			l = g_list_next (l);
		}
	}
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

	g_object_unref (pool->connection);
	g_free (pool);
}

TrackerProcessingPool *
tracker_processing_pool_new (GObject                 *owner,
                             TrackerSparqlConnection *connection,
                             guint                    limit_wait,
                             guint                    limit_ready)
{
	TrackerProcessingPool *pool;

	pool = g_new0 (TrackerProcessingPool, 1);

	pool->owner = owner;
	pool->connection = g_object_ref (connection);
	pool->limit[TRACKER_PROCESSING_TASK_STATUS_WAIT] = limit_wait;
	pool->limit[TRACKER_PROCESSING_TASK_STATUS_READY] = limit_ready;
	/* convenience limit, not really used currently */
	pool->limit[TRACKER_PROCESSING_TASK_STATUS_PROCESSING] = G_MAXUINT;

	pool->tasks[TRACKER_PROCESSING_TASK_STATUS_WAIT] = g_queue_new ();
	pool->tasks[TRACKER_PROCESSING_TASK_STATUS_READY] = g_queue_new ();
	pool->tasks[TRACKER_PROCESSING_TASK_STATUS_PROCESSING] = g_queue_new ();

	g_debug ("Processing pool created with a limit of "
	         "%u tasks in WAIT status and "
	         "%u tasks in READY status",
	         limit_wait,
	         limit_ready);

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

gboolean
tracker_processing_pool_wait_limit_reached (TrackerProcessingPool *pool)
{
	return ((g_queue_get_length (pool->tasks[TRACKER_PROCESSING_TASK_STATUS_WAIT]) >=
	         pool->limit[TRACKER_PROCESSING_TASK_STATUS_WAIT]) ?
	        TRUE : FALSE);
}

gboolean
tracker_processing_pool_ready_limit_reached (TrackerProcessingPool *pool)
{
	return ((g_queue_get_length (pool->tasks[TRACKER_PROCESSING_TASK_STATUS_READY]) >=
	         pool->limit[TRACKER_PROCESSING_TASK_STATUS_READY]) ?
	        TRUE : FALSE);
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
	       G_OBJECT_TYPE_NAME (pool->owner),
	       task,
	       task->file_uri);

	/* Push a new task in WAIT status (so just add it to the tasks queue,
	 * and don't process it. */
	g_queue_push_head (pool->tasks[TRACKER_PROCESSING_TASK_STATUS_WAIT], task);
	task->pool = pool;
}

static void
tracker_processing_pool_sparql_update_cb (GObject      *object,
                                          GAsyncResult *result,
                                          gpointer      user_data)
{
	TrackerProcessingTask *task;
	GError *error = NULL;

	tracker_sparql_connection_update_finish (TRACKER_SPARQL_CONNECTION (object), result, &error);

	task = user_data;

	trace ("(Processing Pool) Finished update of task %p for file '%s'",
	       task,
	       task->file_uri);

	/* Before calling user-provided callback, REMOVE the task from the pool;
	 * as the user-provided callback may actually modify the pool again */
	tracker_processing_pool_remove_task (task->pool, task);

	/* Call finished handler with the error, if any */
	task->finished_handler (task, task->finished_user_data, error);

	/* Deallocate unneeded stuff */
	tracker_processing_task_free (task);
	g_clear_error (&error);
}

static void
tracker_processing_pool_sparql_update_array_cb (GObject      *object,
                                                GAsyncResult *result,
                                                gpointer      user_data)
{
	GError *global_error = NULL;
	GPtrArray *sparql_array_errors;
	GPtrArray *sparql_array;
	guint i;

	/* Get arrays of errors and queries */
	sparql_array = user_data;

	trace ("(Processing Pool) Finished array-update of tasks %p",
	       sparql_array);

	sparql_array_errors = tracker_sparql_connection_update_array_finish (TRACKER_SPARQL_CONNECTION (object),
	                                                                     result,
	                                                                     &global_error);
	if (global_error) {
		g_critical ("(Processing Pool) Could not execute array-update of tasks %p with '%u' items: %s",
		            sparql_array,
		            sparql_array->len,
		            global_error->message);
	}

	/* Report status on each task of the batch update */
	for (i = 0; i < sparql_array->len; i++) {
		TrackerProcessingTask *task;

		task = g_ptr_array_index (sparql_array, i);

		/* Before calling user-provided callback, REMOVE the task from the pool;
		 * as the user-provided callback may actually modify the pool again */
		tracker_processing_pool_remove_task (task->pool, task);

		/* Call finished handler with the error, if any */
		task->finished_handler (task, task->finished_user_data,
		                        (global_error ?
		                         global_error :
		                         g_ptr_array_index (sparql_array_errors, i)));

		/* No need to deallocate the task here, it will be done when
		 * unref-ing the GPtrArray below */
	}

	/* Unref the arrays of errors and queries */
	if (sparql_array_errors)
		g_ptr_array_unref (sparql_array_errors);
	/* Note that tasks are actually deallocated here */
	g_ptr_array_unref (sparql_array);
	g_clear_error (&global_error);
}

void
tracker_processing_pool_buffer_flush (TrackerProcessingPool *pool,
                                      const gchar           *reason)
{
	guint i;
	gchar **sparql_array;

	if (!pool->sparql_buffer)
		return;

	/* Loop buffer and construct array of strings */
	sparql_array = g_new (gchar *, pool->sparql_buffer->len);
	for (i = 0; i < pool->sparql_buffer->len; i++) {
		TrackerProcessingTask *task;

		task = g_ptr_array_index (pool->sparql_buffer, i);

		/* Make sure it was a READY task */
		g_assert (task->status == TRACKER_PROCESSING_TASK_STATUS_READY);

		/* Remove the task from the READY queue and add it to the
		 * PROCESSING one. */
		tracker_processing_pool_remove_task (pool, task);
		task->status = TRACKER_PROCESSING_TASK_STATUS_PROCESSING;
		task->pool = pool;
		g_queue_push_head (pool->tasks[TRACKER_PROCESSING_TASK_STATUS_PROCESSING], task);

		/* Add original string, not a duplicate */
		sparql_array[i] = (task->sparql ?
		                   (gchar *) tracker_sparql_builder_get_result (task->sparql) :
		                   task->sparql_string);
	}

	trace ("(Processing Pool %s) Flushing array-update of tasks %p with %u items (%s)",
	       G_OBJECT_TYPE_NAME (pool->owner),
	       pool->sparql_buffer,
	       pool->sparql_buffer->len,
	       reason ? reason : "Unknown reason");

	tracker_sparql_connection_update_array_async (pool->connection,
	                                              sparql_array,
	                                              pool->sparql_buffer->len,
	                                              G_PRIORITY_DEFAULT,
	                                              NULL,
	                                              tracker_processing_pool_sparql_update_array_cb,
	                                              pool->sparql_buffer);

	/* Clear current parent */
	if (pool->sparql_buffer_current_parent) {
		g_object_unref (pool->sparql_buffer_current_parent);
		pool->sparql_buffer_current_parent = NULL;
	}

	/* Clear temp buffer */
	g_free (sparql_array);
	pool->sparql_buffer_start_time = 0;
	/* Note the whole buffer is passed to the update_array callback,
	 * so no need to free it. */
	pool->sparql_buffer = NULL;
}

gboolean
tracker_processing_pool_push_ready_task (TrackerProcessingPool                     *pool,
                                         TrackerProcessingTask                     *task,
                                         gboolean                                   buffer,
                                         TrackerProcessingPoolTaskFinishedCallback  finished_handler,
                                         gpointer                                   user_data)
{
	GList *previous;

	/* The task MUST have a proper SPARQL here */
	g_assert (task->sparql != NULL || task->sparql_string != NULL);

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

	/* If buffering not requested, OR the limit of READY tasks is actually 1,
	 * flush previous buffer (if any) and then the new update */
	if (!buffer || pool->limit[TRACKER_PROCESSING_TASK_STATUS_READY] == 1) {
		trace ("(Processing Pool %s) Pushed READY/PROCESSING task %p for file '%s'",
		       G_OBJECT_TYPE_NAME (pool->owner),
		       task,
		       task->file_uri);

		/* Flush previous */
		tracker_processing_pool_buffer_flush (pool,
		                                      "Before unbuffered task");

		/* Set status of the task as PROCESSING (No READY status here!) */
		task->status = TRACKER_PROCESSING_TASK_STATUS_PROCESSING;
		g_queue_push_head (pool->tasks[TRACKER_PROCESSING_TASK_STATUS_PROCESSING], task);

		trace ("(Processing Pool %s) Flushing single task %p",
		       G_OBJECT_TYPE_NAME (pool->owner),
		       task);

		/* And update the new one */
		tracker_sparql_connection_update_async (pool->connection,
		                                        (task->sparql ?
		                                         tracker_sparql_builder_get_result (task->sparql) :
		                                         task->sparql_string),
		                                        G_PRIORITY_DEFAULT,
		                                        NULL,
		                                        tracker_processing_pool_sparql_update_cb,
		                                        task);

		return TRUE;
	} else {
		GFile *parent;
		gboolean flushed = FALSE;

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
		       G_OBJECT_TYPE_NAME (pool->owner),
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
tracker_processing_pool_get_ready_task_count (TrackerProcessingPool *pool)
{
	return g_queue_get_length (pool->tasks[TRACKER_PROCESSING_TASK_STATUS_READY]);
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
