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
 *    processed by tracker-miner-fs, and there are currently 2 kind of tasks
 *    considered in this pool:
 *   1.1. "WAIT" tasks are those used to specify tasks which still do not have a
 *        full SPARQL built. Currently, tasks in the WAIT status could be:
 *         o Tasks added while checking if the upper layer needs to process the
 *           given file (checked using the 'process-file' or
 *           'process-file-attributes' signals in TrackerMinerFS).
 *         o Tasks added while the upper layer is actually processing the given
 *           files (until the tracker_miner_fs_file_notify() is called by the
 *           upper layer).
 *   1.2. "PROCESS" tasks are those used to specify tasks which have a proper
 *         SPARQL string ready to be pushed to tracker-store.
 *
 * 2. The current possible flows for tasks added to the processing pool are:
 *   2.1. Full SPARQL is ready before pushing the task to the pool. This is
 *        currently the case for DELETED or MOVED events, and the flow would be
 *        like this:
 *         - processing_task_new() to create a new task
 *         - processing_task_set_sparql() to set the full SPARQL in the task.
 *         - processing_pool_process_task() to push the newly created task
 *           into the processing pool as a "PROCESS" task.
 *
 *   2.2. The full SPARQL is still not available, as the upper layers need to
 *        process the file (like extracting metadata using tracker-extract
 *        in the case of TrackerMinerFiles). This case would correspond to
 *        CREATED or UPDATED events:
 *         - processing_task_new() to create a new task
 *         - processing_pool_wait_task() to push the newly created task into
 *           the processing pool as a "WAIT" task.
 *         - processing_task_set_sparql() to set the full SPARQL in the task
 *           (when the upper layers finished building it).
 *         - processing_pool_process_task() to push the newly created task
 *           into the processing pool as a "PROCESS" task.
 *
 * 3. The number of tasks pushed to the pull as "WAIT" tasks is limited to the
 *    number set while creating the pool. This value corresponds to the
 *    "wait-pool-limit" property in the TrackerMinerFS object, and currently is
 *    set to 1 for TrackerMinerApplications and to 10 to TrackerMinerFiles. In
 *    the case of TrackerMinerFiles, this number specifies the maximum number of
 *    extraction requests that can be managed in parallel.
 *
 * 4. The number of tasks pushed to the pull as "PROCESS" tasks is limited to
 *    the number set while creating the pool. This value corresponds to the
 *    "process-pool-limit" property in the TrackerMinerFS object, and currently
 *    is set to 1 for TrackerMinerApplications and to 100 to TrackerMinerFiles.
 *    In the case of TrackerMinerFiles, this number specifies the maximum number
 *    of SPARQL updates that can be merged into a single multi-insert SPARQL
 *    connection.
 *
 * 5. When a task is pushed to the pool as a "PROCESS" task, the pool will be in
 *    charge of executing the SPARQL update into the store.
 *
 * 6. If buffering was requested when processing_pool_process_task() was used to
 *    push the new task in the pool as a "PROCESS" task, this task will be added
 *    internally into a SPARQL buffer. This SPARQL buffer will be flushed
 *    (pushing all collected SPARQL updates into the store) if one of these
 *    conditions is met:
 *      (a) The file corresponding to the task pushed doesn't have a parent.
 *      (b) The parent of the file corresponding to the task pushed is different
 *          to the parent of the last file pushed to the buffer.
 *      (c) The limit for "PROCESS" tasks in the pool was reached.
 *      (d) The buffer was not flushed in the last MAX_SPARQL_BUFFER_TIME (=15)
 *          seconds.
 *    The buffer is flushed using a single multi-insert SPARQL connection. This
 *    means that an array of SPARQLs is sent to tracker-store, which replies
 *    with an array of GErrors specifying which update failed, if any.
 *
 * 7. If buffering is not requested when processing_pool_process_task() is
 *    called, first the previous buffer is flushed (if any) and then the current
 *    task is updated in the store.
 *
 * 8. May the gods be with you if you need to fix a bug in here.
 *
 */

#include "config.h"
#include "tracker-miner-fs-processing-pool.h"

/* Maximum time (seconds) before forcing a sparql buffer flush */
#define MAX_SPARQL_BUFFER_TIME  15

/*------------------- PROCESSING TASK ----------------------*/

typedef enum {
	PROCESSING_TASK_STATUS_NO_POOL,
	PROCESSING_TASK_STATUS_WAIT = 0,
	PROCESSING_TASK_STATUS_PROCESS,
	PROCESSING_TASK_STATUS_LAST
} ProcessingTaskStatus;

struct _ProcessingTask {
	/* The file being processed */
	GFile *file;
	/* The FULL sparql to be updated in the store */
	gchar *sparql;
	/* The context of the task */
	gpointer context;
	/* The context deallocation method, if any */
	GFreeFunc context_free_func;

	/* Internal status of the task */
	ProcessingTaskStatus status;
	/* The pool where the task was added */
	ProcessingPool *pool;

	/* Handler and user_data to use when task is fully processed */
	ProcessingPoolTaskFinishedCallback finished_handler;
	gpointer                           finished_user_data;
};

ProcessingTask *
processing_task_new (GFile *file)
{
	ProcessingTask *task;

	task = g_slice_new0 (ProcessingTask);
	task->file = g_object_ref (file);
	task->status = PROCESSING_TASK_STATUS_NO_POOL;
	return task;
}

void
processing_task_free (ProcessingTask *task)
{
	if (!task)
		return;

	/* Free context if requested to do so */
	if (task->context &&
	    task->context_free_func) {
		task->context_free_func (task->context);
	}
	g_free (task->sparql);
	g_object_unref (task->file);
	g_slice_free (ProcessingTask, task);
}

GFile *
processing_task_get_file (ProcessingTask *task)
{
	return task->file;
}

gpointer
processing_task_get_context (ProcessingTask *task)
{
	return task->context;
}

void
processing_task_set_context (ProcessingTask *task,
                             gpointer        context,
                             GFreeFunc       context_free_func)
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
processing_task_set_sparql (ProcessingTask *task,
                            gchar          *sparql)
{
	g_free (task->sparql);
	task->sparql = g_strdup (sparql);
}


/*------------------- PROCESSING POOL ----------------------*/

struct _ProcessingPool {
	/* Connection to the Store */
	TrackerSparqlConnection *connection;

	/* The tasks currently in WAIT or PROCESS status */
	GQueue *tasks[PROCESSING_TASK_STATUS_LAST];
	/* The processing pool limits */
	guint  limit[PROCESSING_TASK_STATUS_LAST];

	/* SPARQL buffer to pile up several UPDATEs */
	GPtrArray      *sparql_buffer;
	GFile          *sparql_buffer_current_parent;
	time_t          sparql_buffer_start_time;
};

static void
pool_queue_free_foreach (gpointer data,
                         gpointer user_data)
{
	processing_task_free (data);
}

void
processing_pool_free (ProcessingPool *pool)
{
	guint i;

	if (!pool)
		return;

	/* Free any pending task here... shouldn't really
	 * be any */
	for (i = PROCESSING_TASK_STATUS_WAIT;
	     i < PROCESSING_TASK_STATUS_LAST;
	     i++) {
		g_queue_foreach (pool->tasks[i],
		                 pool_queue_free_foreach,
		                 NULL);
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

ProcessingPool *
processing_pool_new (TrackerSparqlConnection *connection,
                     guint                    limit_wait,
                     guint                    limit_process)
{
	ProcessingPool *pool;

	pool = g_new0 (ProcessingPool, 1);

	pool->connection = g_object_ref (connection);
	pool->limit[PROCESSING_TASK_STATUS_WAIT] = limit_wait;
	pool->limit[PROCESSING_TASK_STATUS_PROCESS] = limit_process;

	pool->tasks[PROCESSING_TASK_STATUS_WAIT] = g_queue_new ();
	pool->tasks[PROCESSING_TASK_STATUS_PROCESS] = g_queue_new ();

	g_debug ("Processing pool created with a limit of "
	         "%u tasks in WAIT status and "
	         "%u tasks in PROCESS status",
	         limit_wait,
	         limit_process);

	return pool;
}

void
processing_pool_set_wait_limit (ProcessingPool *pool,
                                guint           limit)
{
	g_message ("Processing pool limit for WAIT tasks set to %u", limit);
	pool->limit[PROCESSING_TASK_STATUS_WAIT] = limit;
}

void
processing_pool_set_process_limit (ProcessingPool *pool,
                                   guint           limit)
{
	g_message ("Processing pool limit for PROCESS tasks set to %u", limit);
	pool->limit[PROCESSING_TASK_STATUS_PROCESS] = limit;
}

guint
processing_pool_get_wait_limit (ProcessingPool *pool)
{
	return pool->limit[PROCESSING_TASK_STATUS_WAIT];
}

guint
processing_pool_get_process_limit (ProcessingPool *pool)
{
	return pool->limit[PROCESSING_TASK_STATUS_PROCESS];
}

gboolean
processing_pool_wait_limit_reached (ProcessingPool *pool)
{
	return ((g_queue_get_length (pool->tasks[PROCESSING_TASK_STATUS_WAIT]) >=
	         pool->limit[PROCESSING_TASK_STATUS_WAIT]) ?
	        TRUE : FALSE);
}

gboolean
processing_pool_process_limit_reached (ProcessingPool *pool)
{
	return ((g_queue_get_length (pool->tasks[PROCESSING_TASK_STATUS_PROCESS]) >=
	         pool->limit[PROCESSING_TASK_STATUS_PROCESS]) ?
	        TRUE : FALSE);
}

ProcessingTask *
processing_pool_find_task (ProcessingPool *pool,
                           GFile          *file,
                           gboolean        path_search)
{
	guint i;

	for (i = PROCESSING_TASK_STATUS_WAIT;
	     i < PROCESSING_TASK_STATUS_PROCESS;
	     i++) {
		GList *l;

		for (l = pool->tasks[i]->head; l; l = g_list_next (l)) {
			ProcessingTask *task = l->data;

			if (!path_search) {
				/* Different operations for the same file URI could be
				 * piled up here, each being a different GFile object.
				 * Miner implementations should really notify on the
				 * same GFile object that's being passed, so we check for
				 * pointer equality here, rather than doing path comparisons
				 */
				if(task->file == file)
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
processing_pool_wait_task (ProcessingPool *pool,
                           ProcessingTask *task)
{
	g_assert (task->status == PROCESSING_TASK_STATUS_NO_POOL);

	/* Set status of the task as WAIT */
	task->status = PROCESSING_TASK_STATUS_WAIT;

	/* Push a new task in WAIT status (so just add it to the tasks queue,
	 * and don't process it. */
	g_queue_push_head (pool->tasks[PROCESSING_TASK_STATUS_WAIT], task);
	task->pool = pool;
}

static void
processing_pool_sparql_update_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
	ProcessingTask *task;
	GError *error = NULL;

	tracker_sparql_connection_update_finish (TRACKER_SPARQL_CONNECTION (object), result, &error);

	/* If update was done when crawling finished, no task will be given */
	if (!user_data)
		return;

	task = user_data;

	/* Before calling user-provided callback, REMOVE the task from the pool;
	 * as the user-provided callback may actually modify the pool again */
	processing_pool_remove_task (task->pool, task);

	/* Call finished handler with the error, if any */
	task->finished_handler (task, task->finished_user_data, error);

	/* Deallocate unneeded stuff */
	processing_task_free (task);
	g_clear_error (&error);
}

static void
processing_pool_sparql_update_array_cb (GObject      *object,
                                        GAsyncResult *result,
                                        gpointer      user_data)
{
	GError *global_error = NULL;
	GPtrArray *sparql_array_errors;
	GPtrArray *sparql_array;
	guint i;

	/* Get arrays of errors and queries */
	sparql_array = user_data;
	sparql_array_errors = tracker_sparql_connection_update_array_finish (TRACKER_SPARQL_CONNECTION (object),
	                                                                     result,
	                                                                     &global_error);
	if (global_error) {
		g_critical ("(Sparql buffer) Could not execute array-update with '%u' items: %s",
		            sparql_array->len,
		            global_error->message);
	}

	/* Report status on each task of the batch update */
	for (i = 0; i < sparql_array->len; i++) {
		ProcessingTask *task;

		task = g_ptr_array_index (sparql_array, i);

		/* Before calling user-provided callback, REMOVE the task from the pool;
		 * as the user-provided callback may actually modify the pool again */
		processing_pool_remove_task (task->pool, task);

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
processing_pool_buffer_flush (ProcessingPool *pool)
{
	guint i;
	GPtrArray *sparql_array;

	if (!pool->sparql_buffer)
		return;

	/* Loop buffer and construct array of strings */
	sparql_array = g_ptr_array_new ();
	for (i = 0; i < pool->sparql_buffer->len; i++) {
		ProcessingTask *task;

		task = g_ptr_array_index (pool->sparql_buffer, i);
		/* Add original string, not a duplicate */
		g_ptr_array_add (sparql_array, task->sparql);
	}

	g_debug ("(Sparql buffer) Flushing buffer with '%u' items",
	         pool->sparql_buffer->len);
	tracker_sparql_connection_update_array_async (pool->connection,
	                                              (gchar **)(sparql_array->pdata),
	                                              sparql_array->len,
	                                              G_PRIORITY_DEFAULT,
	                                              NULL,
	                                              processing_pool_sparql_update_array_cb,
	                                              pool->sparql_buffer);

	/* Clear current parent */
	if (pool->sparql_buffer_current_parent) {
		g_object_unref (pool->sparql_buffer_current_parent);
		pool->sparql_buffer_current_parent = NULL;
	}

	/* Clear temp buffer */
	g_ptr_array_free (sparql_array, TRUE);
	pool->sparql_buffer_start_time = 0;
	/* Note the whole buffer is passed to the update_array callback,
	 * so no need to free it. */
	pool->sparql_buffer = NULL;
}

gboolean
processing_pool_process_task (ProcessingPool                     *pool,
                              ProcessingTask                     *task,
                              gboolean                            buffer,
                              ProcessingPoolTaskFinishedCallback  finished_handler,
                              gpointer                            user_data)
{
	GList *previous;

	/* The task MUST have a proper SPARQL here */
	g_assert (task->sparql != NULL);

	/* First, check if the task was already added as being WAITING */
	previous = g_queue_find (pool->tasks[PROCESSING_TASK_STATUS_WAIT], task);
	if (!previous) {
		/* Add it to the PROCESS queue */
		g_queue_push_head (pool->tasks[PROCESSING_TASK_STATUS_PROCESS], task);
		task->pool = pool;
	} else {
		/* Make sure it was a WAIT task */
		g_assert (task->status == PROCESSING_TASK_STATUS_WAIT);
		/* Move task from WAIT queue to PROCESS queue */
		g_queue_delete_link (pool->tasks[PROCESSING_TASK_STATUS_WAIT], previous);
		g_queue_push_head (pool->tasks[PROCESSING_TASK_STATUS_PROCESS], task);
	}

	/* Set status of the task as PROCESS */
	task->status = PROCESSING_TASK_STATUS_PROCESS;

	task->finished_handler = finished_handler;
	task->finished_user_data = user_data;

	/* If buffering not requested, flush previous buffer and then the new update */
	if (!buffer) {
		/* Flush previous */
		processing_pool_buffer_flush (pool);
		/* And update the new one */
		tracker_sparql_connection_update_async (pool->connection,
		                                        task->sparql,
		                                        G_PRIORITY_DEFAULT,
		                                        NULL,
		                                        processing_pool_sparql_update_cb,
		                                        task);

		return TRUE;
	} else {
		GFile *parent;
		gboolean flushed = FALSE;

		/* Get parent of this file we're updating/creating */
		parent = g_file_get_parent (task->file);

		/* Start buffer if not already done */
		if (!pool->sparql_buffer) {
			pool->sparql_buffer = g_ptr_array_new_with_free_func ((GDestroyNotify)processing_task_free);
			pool->sparql_buffer_start_time = time (NULL);
		}

		/* Set current parent if not set already */
		if (!pool->sparql_buffer_current_parent && parent) {
			pool->sparql_buffer_current_parent = g_object_ref (parent);
		}

		/* Add task to array */
		g_ptr_array_add (pool->sparql_buffer, task);

		/* Flush buffer if:
		 *  - Last item has no parent
		 *  - Parent change was detected
		 *  - 'limit_process' items reached
		 *  - Not flushed in the last MAX_SPARQL_BUFFER_TIME seconds
		 */
		if (!parent ||
		    !g_file_equal (parent, pool->sparql_buffer_current_parent) ||
		    processing_pool_process_limit_reached (pool) ||
		    (time (NULL) - pool->sparql_buffer_start_time > MAX_SPARQL_BUFFER_TIME)) {
			/* Flush! */
			processing_pool_buffer_flush (pool);
			flushed = TRUE;
		}

		if (parent)
			g_object_unref (parent);

		return flushed;
	}
}

void
processing_pool_remove_task (ProcessingPool *pool,
                             ProcessingTask *task)
{
	/* Remove from pool without freeing it */
	GList *in_pool;

	g_assert (pool == task->pool);

	/* Make sure the task was in the pool */
	in_pool = g_queue_find (pool->tasks[task->status], task);
	g_assert (in_pool != NULL);

	g_queue_delete_link (pool->tasks[task->status], in_pool);
	task->pool = NULL;
	task->status = PROCESSING_TASK_STATUS_NO_POOL;
}

guint
processing_pool_get_wait_task_count (ProcessingPool *pool)
{
	return g_queue_get_length (pool->tasks[PROCESSING_TASK_STATUS_WAIT]);
}

guint
processing_pool_get_process_task_count (ProcessingPool *pool)
{
	return g_queue_get_length (pool->tasks[PROCESSING_TASK_STATUS_PROCESS]);
}

ProcessingTask *
processing_pool_get_last_wait (ProcessingPool *pool)
{
	GList *li;

	for (li = pool->tasks[PROCESSING_TASK_STATUS_WAIT]->tail; li; li = g_list_previous (li)) {
		ProcessingTask *task = li->data;

		if (task->status == PROCESSING_TASK_STATUS_WAIT) {
			return task;
		}
	}
	return NULL;
}

void
processing_pool_foreach (ProcessingPool *pool,
                         GFunc           func,
                         gpointer        user_data)
{
	guint i;

	for (i = PROCESSING_TASK_STATUS_WAIT;
	     i < PROCESSING_TASK_STATUS_PROCESS;
	     i++) {
		g_queue_foreach (pool->tasks[i], func, user_data);
	}
}
