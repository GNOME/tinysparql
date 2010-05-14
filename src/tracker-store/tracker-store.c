/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 * Author: Philip Van Hoof <philip@codeminded.be>
 */

#include "config.h"

#include <unistd.h>
#include <sys/types.h>

#include <libtracker-common/tracker-dbus.h>
#include <libtracker-db/tracker-db-dbus.h>
#include <libtracker-db/tracker-db-interface-sqlite.h>

#include <libtracker-data/tracker-data-manager.h>
#include <libtracker-data/tracker-data-update.h>
#include <libtracker-data/tracker-data-query.h>
#include <libtracker-data/tracker-sparql-query.h>

#include "tracker-store.h"

#define TRACKER_STORE_TRANSACTION_MAX                   4000
#define TRACKER_STORE_MAX_CONCURRENT_QUERIES               2

#define TRACKER_STORE_QUERY_WATCHDOG_TIMEOUT 10
#define TRACKER_STORE_MAX_TASK_TIME          30

typedef struct {
	gboolean     have_handler, have_sync_handler;
	gboolean     batch_mode, start_log;
	guint        batch_count;
	GQueue      *queues[TRACKER_STORE_N_PRIORITIES];
	guint        handler, sync_handler;
	guint        n_queries_running;
	gboolean     update_running;
	GThreadPool *main_pool;
	GThreadPool *global_pool;
	GSList	    *running_tasks;
	guint	     watchdog_id;
} TrackerStorePrivate;

typedef enum {
	TRACKER_STORE_TASK_TYPE_QUERY,
	TRACKER_STORE_TASK_TYPE_UPDATE,
	TRACKER_STORE_TASK_TYPE_UPDATE_BLANK,
	TRACKER_STORE_TASK_TYPE_COMMIT,
	TRACKER_STORE_TASK_TYPE_TURTLE,
} TrackerStoreTaskType;

typedef struct {
	TrackerStoreTaskType  type;
	union {
		struct {
			gchar       *query;
			GThread     *running_thread;
			GTimer      *timer;
			gpointer     thread_data;
		} query;
		struct {
			gchar        *query;
			gboolean      batch;
			GPtrArray    *blank_nodes;
		} update;
		struct {
			TrackerTurtleReader *reader;
			gboolean             in_progress;
			gchar               *path;
		} turtle;
	} data;
	gchar                *client_id;
	GError               *error;
	gpointer              user_data;
	GDestroyNotify        destroy;
	union {
		struct {
			TrackerStoreSparqlQueryCallback   query_callback;
			TrackerStoreSparqlQueryInThread   in_thread;
		} query;
		TrackerStoreSparqlUpdateCallback      update_callback;
		TrackerStoreSparqlUpdateBlankCallback update_blank_callback;
		TrackerStoreCommitCallback            commit_callback;
		TrackerStoreTurtleCallback            turtle_callback;
	} callback;
} TrackerStoreTask;

static GStaticPrivate private_key = G_STATIC_PRIVATE_INIT;

static void start_handler (TrackerStorePrivate *private);

static void
private_free (gpointer data)
{
	TrackerStorePrivate *private = data;
	gint i;

	for (i = 0; i < TRACKER_STORE_N_PRIORITIES; i++) {
		g_queue_free (private->queues[i]);
	}
	g_free (private);
}

static void
store_task_free (TrackerStoreTask *task)
{
	if (task->type == TRACKER_STORE_TASK_TYPE_TURTLE) {
		g_object_unref (task->data.turtle.reader);
		g_free (task->data.turtle.path);
	} else if (task->type == TRACKER_STORE_TASK_TYPE_QUERY) {
		g_free (task->data.query.query);
		if (task->data.query.timer) {
			g_timer_destroy (task->data.query.timer);
		}
	} else {
		g_free (task->data.update.query);
	}

	g_free (task->client_id);
	g_slice_free (TrackerStoreTask, task);
}

static gboolean
process_turtle_file_part (TrackerTurtleReader *reader, GError **error)
{
	int i;
	GError *new_error = NULL;

	/* process 10 statements at once before returning to main loop */

	i = 0;

	/* There is no logical structure in turtle files, so we have no choice
	 * but fallback to fixed number of statements per transaction to avoid
	 * blocking tracker-store.
	 * Real applications should all use SPARQL update instead of turtle
	 * import to avoid this issue.
	 */
	tracker_data_begin_transaction (&new_error);
	if (new_error) {
		g_propagate_error (error, new_error);
		return FALSE;
	}

	while (new_error == NULL && tracker_turtle_reader_next (reader, &new_error)) {
		/* insert statement */
		if (tracker_turtle_reader_get_object_is_uri (reader)) {
			tracker_data_insert_statement_with_uri (
			                                        tracker_turtle_reader_get_graph (reader),
			                                        tracker_turtle_reader_get_subject (reader),
			                                        tracker_turtle_reader_get_predicate (reader),
			                                        tracker_turtle_reader_get_object (reader),
			                                        &new_error);
		} else {
			tracker_data_insert_statement_with_string (
			                                           tracker_turtle_reader_get_graph (reader),
			                                           tracker_turtle_reader_get_subject (reader),
			                                           tracker_turtle_reader_get_predicate (reader),
			                                           tracker_turtle_reader_get_object (reader),
			                                           &new_error);
		}

		i++;
		if (!new_error && i >= 10) {
			tracker_data_commit_transaction (&new_error);
			if (new_error) {
				tracker_data_rollback_transaction ();
				g_propagate_error (error, new_error);
				return FALSE;
			}
			/* return to main loop */
			return TRUE;
		}
	}

	if (new_error) {
		tracker_data_rollback_transaction ();
		g_propagate_error (error, new_error);
		return FALSE;
	}

	tracker_data_commit_transaction (&new_error);
	if (new_error) {
		tracker_data_rollback_transaction ();
		g_propagate_error (error, new_error);
		return FALSE;
	}

	return FALSE;
}

static void
begin_batch (TrackerStorePrivate *private)
{
	if (!private->batch_mode) {
		/* switch to batch mode
		   delays database commits to improve performance */
		tracker_data_begin_db_transaction ();
		private->batch_mode = TRUE;
		private->batch_count = 0;
	}
}

static void
end_batch (TrackerStorePrivate *private)
{
	if (private->batch_mode) {
		/* commit pending batch items */
		tracker_data_commit_db_transaction ();
		tracker_data_notify_db_transaction ();

		private->batch_mode = FALSE;
		private->batch_count = 0;
	}
}

static gboolean
task_ready (TrackerStorePrivate *private)
{
	TrackerStoreTask *task;
	gint i;

	/* return TRUE if at least one queue is not empty (to keep idle handler running) */

	if (private->n_queries_running >= TRACKER_STORE_MAX_CONCURRENT_QUERIES) {
		/* maximum number of queries running already, cannot schedule anything else */
		return FALSE;
	} else if (private->update_running) {
		/* update running already, cannot schedule anything else */
		return FALSE;
	}

	for (i = 0; i < TRACKER_STORE_N_PRIORITIES; i++) {
		/* check next task of highest priority */
		task = g_queue_peek_head (private->queues[i]);
		if (task != NULL) {
			if (task->type == TRACKER_STORE_TASK_TYPE_QUERY) {
				/* we know that the maximum number of concurrent queries has not been reached yet,
				   query can be scheduled */
				return TRUE;
			} else if (private->n_queries_running == 0) {
				/* no queries running, updates can be scheduled */
				return TRUE;
			} else {
				/* queries running, wait for them to finish before scheduling updates */
				return FALSE;
			}
		}
	}

	return FALSE;
}

static void
check_handler (TrackerStorePrivate *private)
{
	if (task_ready (private)) {
		/* handler should be running */
		if (!private->have_handler) {
			start_handler (private);
		}
	} else {
		/* handler should not be running */
		if (private->have_handler) {
			g_source_remove (private->handler);
		}
	}
}

static gboolean
watchdog_cb (gpointer user_data)
{
	TrackerStorePrivate *private = user_data;
	GSList *running;

	private = user_data;
	running = private->running_tasks;

	if (!running) {
		private->watchdog_id = 0;
		return FALSE;
	}

	while (running) {
		TrackerStoreTask *task;
		GThread *thread;

		task = running->data;
		running = running->next;
		thread = task->data.query.running_thread;

		if (thread && g_timer_elapsed (task->data.query.timer, NULL) > TRACKER_STORE_MAX_TASK_TIME) {
			tracker_data_manager_interrupt_thread (task->data.query.running_thread);
		}
	}

	return TRUE;
}

static void
ensure_running_tasks_watchdog (TrackerStorePrivate *private)
{
	if (private->watchdog_id == 0) {
		private->watchdog_id = g_timeout_add_seconds (TRACKER_STORE_QUERY_WATCHDOG_TIMEOUT,
		                                              watchdog_cb, private);
	}
}

static void
check_running_tasks_watchdog (TrackerStorePrivate *private)
{
	if (private->running_tasks == NULL &&
	    private->watchdog_id != 0) {
		g_source_remove (private->watchdog_id);
		private->watchdog_id = 0;
	}
}

static gboolean
task_finish_cb (gpointer data)
{
	TrackerStorePrivate *private;
	TrackerStoreTask *task;

	private = g_static_private_get (&private_key);
	task = data;

	if (task->type == TRACKER_STORE_TASK_TYPE_QUERY) {
		if (task->callback.query.query_callback) {
			task->callback.query.query_callback (task->data.query.thread_data, task->error, task->user_data);
		}

		if (task->error) {
			g_clear_error (&task->error);
		}

		private->running_tasks = g_slist_remove (private->running_tasks, task);
		check_running_tasks_watchdog (private);
		private->n_queries_running--;
	} else if (task->type == TRACKER_STORE_TASK_TYPE_UPDATE) {
		if (!task->data.update.batch && !task->error) {
			tracker_data_notify_db_transaction ();
		}

		if (task->callback.update_callback) {
			task->callback.update_callback (task->error, task->user_data);
		}

		if (task->error) {
			g_clear_error (&task->error);
		}

		private->update_running = FALSE;
	} else if (task->type == TRACKER_STORE_TASK_TYPE_UPDATE_BLANK) {
		if (!task->data.update.batch && !task->error) {
			tracker_data_notify_db_transaction ();
		}

		if (task->callback.update_blank_callback) {
			if (!task->data.update.blank_nodes) {
				/* Create empty GPtrArray for dbus-glib to be happy */
				task->data.update.blank_nodes = g_ptr_array_new ();
			}

			task->callback.update_blank_callback (task->data.update.blank_nodes, task->error, task->user_data);
		}

		if (task->data.update.blank_nodes) {
			gint i;

			for (i = 0; i < task->data.update.blank_nodes->len; i++) {
				g_ptr_array_foreach (task->data.update.blank_nodes->pdata[i], (GFunc) g_hash_table_unref, NULL);
				g_ptr_array_free (task->data.update.blank_nodes->pdata[i], TRUE);
			}
			g_ptr_array_free (task->data.update.blank_nodes, TRUE);
		}

		if (task->error) {
			g_clear_error (&task->error);
		}

		private->update_running = FALSE;
	} else if (task->type == TRACKER_STORE_TASK_TYPE_TURTLE) {
		private->update_running = FALSE;

		if (task->data.turtle.in_progress) {
			/* Task still in progress */
			check_handler (private);
			return FALSE;
		} else {
			if (task->callback.turtle_callback) {
				task->callback.turtle_callback (task->error, task->user_data);
			}

			if (task->error) {
				g_clear_error (&task->error);
			}

			/* Remove the task now that we're done with it */
			g_queue_pop_head (private->queues[TRACKER_STORE_PRIORITY_TURTLE]);
		}
	} else if (task->type == TRACKER_STORE_TASK_TYPE_COMMIT) {
		tracker_data_notify_db_transaction ();

		if (task->callback.commit_callback) {
			task->callback.commit_callback (task->user_data);
		}

		private->update_running = FALSE;
	}

	if (task->destroy) {
		task->destroy (task->user_data);
	}

	store_task_free (task);

	check_handler (private);

	return FALSE;
}

static void
pool_dispatch_cb (gpointer data,
                  gpointer user_data)
{
	TrackerStorePrivate *private;
	TrackerStoreTask *task;

	private = user_data;
	task = data;

	if (task->type == TRACKER_STORE_TASK_TYPE_QUERY) {
		TrackerDBCursor *cursor;

		task->data.query.running_thread = g_thread_self ();
		cursor = tracker_data_query_sparql_cursor (task->data.query.query, &task->error);

		task->data.query.thread_data = task->callback.query.in_thread (cursor, task->error, task->user_data);

		if (cursor)
			g_object_unref (cursor);
		task->data.query.running_thread = NULL;

	} else if (task->type == TRACKER_STORE_TASK_TYPE_UPDATE) {
		if (task->data.update.batch) {
			begin_batch (private);
		} else {
			end_batch (private);
			tracker_data_begin_db_transaction ();
		}

		tracker_data_update_sparql (task->data.update.query, &task->error);

		if (task->data.update.batch) {
			if (!task->error) {
				private->batch_count++;
				if (private->batch_count >= TRACKER_STORE_TRANSACTION_MAX) {
					end_batch (private);
				}
			}
		} else {
			tracker_data_commit_db_transaction ();
		}
	} else if (task->type == TRACKER_STORE_TASK_TYPE_UPDATE_BLANK) {
		if (task->data.update.batch) {
			begin_batch (private);
		} else {
			end_batch (private);
			tracker_data_begin_db_transaction ();
		}

		task->data.update.blank_nodes = tracker_data_update_sparql_blank (task->data.update.query, &task->error);

		if (task->data.update.batch) {
			if (!task->error) {
				private->batch_count++;
				if (private->batch_count >= TRACKER_STORE_TRANSACTION_MAX) {
					end_batch (private);
				}
			}
		} else {
			tracker_data_commit_db_transaction ();
		}
	} else if (task->type == TRACKER_STORE_TASK_TYPE_TURTLE) {
		if (!task->data.turtle.in_progress) {
			task->data.turtle.reader = tracker_turtle_reader_new (task->data.turtle.path, &task->error);

			if (task->error) {
				g_idle_add (task_finish_cb, task);
				return;
			}

			task->data.turtle.in_progress = TRUE;
		}

		begin_batch (private);

		if (process_turtle_file_part (task->data.turtle.reader, &task->error)) {
			/* import still in progress */
			private->batch_count++;
			if (private->batch_count >= TRACKER_STORE_TRANSACTION_MAX) {
				end_batch (private);
			}
		} else {
			/* import finished */
			task->data.turtle.in_progress = FALSE;
			end_batch (private);
		}
	} else if (task->type == TRACKER_STORE_TASK_TYPE_COMMIT) {
		end_batch (private);
	}

	g_idle_add (task_finish_cb, task);
}

static void
task_run_async (TrackerStorePrivate *private,
                TrackerStoreTask    *task)
{
	if (private->n_queries_running > 1) {
		/* use global pool if main pool might already be occupied */
		g_thread_pool_push (private->global_pool, task, NULL);
	} else {
		/* use main pool for updates and non-parallel queries */
		g_thread_pool_push (private->main_pool, task, NULL);
	}
}

static gboolean
queue_idle_handler (gpointer user_data)
{
	TrackerStorePrivate *private = user_data;
	GQueue              *queue;
	TrackerStoreTask    *task = NULL;
	gint                 i;

	for (i = 0; task == NULL && i < TRACKER_STORE_N_PRIORITIES; i++) {
		queue = private->queues[i];
		task = g_queue_peek_head (queue);
	}
	g_return_val_if_fail (task != NULL, FALSE);

	if (task->type == TRACKER_STORE_TASK_TYPE_QUERY) {
		/* pop task now, otherwise further queries won't be scheduled */
		g_queue_pop_head (queue);

		private->running_tasks = g_slist_prepend (private->running_tasks, task);
		ensure_running_tasks_watchdog (private);
		private->n_queries_running++;

		task->data.query.timer = g_timer_new ();

		task_run_async (private, task);
	} else if (task->type == TRACKER_STORE_TASK_TYPE_UPDATE ||
	           task->type == TRACKER_STORE_TASK_TYPE_UPDATE_BLANK ||
	           task->type == TRACKER_STORE_TASK_TYPE_COMMIT) {
		g_queue_pop_head (queue);

		private->update_running = TRUE;

		task_run_async (private, task);
	} else if (task->type == TRACKER_STORE_TASK_TYPE_TURTLE) {
		private->update_running = TRUE;
		task_run_async (private, task);
	}

	return task_ready (private);
}

static void
queue_idle_destroy (gpointer user_data)
{
	TrackerStorePrivate *private = user_data;

	private->have_handler = FALSE;
}


void
tracker_store_init (void)
{
	TrackerStorePrivate *private;
	gint i;

	private = g_new0 (TrackerStorePrivate, 1);

	for (i = 0; i < TRACKER_STORE_N_PRIORITIES; i++) {
		private->queues[i] = g_queue_new ();
	}

	private->main_pool = g_thread_pool_new (pool_dispatch_cb,
	                                        private, 1,
	                                        TRUE, NULL);
	private->global_pool = g_thread_pool_new (pool_dispatch_cb,
	                                          private, TRACKER_STORE_MAX_CONCURRENT_QUERIES,
	                                          FALSE, NULL);

	/* as the following settings are global for unknown reasons,
	   let's use the same settings as gio, otherwise the used settings
	   are rather random */
	g_thread_pool_set_max_idle_time (15 * 1000);
	g_thread_pool_set_max_unused_threads (2);

	g_static_private_set (&private_key,
	                      private,
	                      private_free);

}

void
tracker_store_shutdown (void)
{
	TrackerStorePrivate *private;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	g_thread_pool_free (private->global_pool, FALSE, TRUE);
	g_thread_pool_free (private->main_pool, FALSE, TRUE);

	if (private->have_handler) {
		g_source_remove (private->handler);
		private->have_handler = FALSE;
	}

	if (private->have_sync_handler) {
		g_source_remove (private->sync_handler);
		private->have_sync_handler = FALSE;
	}

	g_static_private_set (&private_key, NULL, NULL);
}

static void
start_handler (TrackerStorePrivate *private)
{
	private->have_handler = TRUE;

	private->handler = g_idle_add_full (G_PRIORITY_LOW,
	                                    queue_idle_handler,
	                                    private,
	                                    queue_idle_destroy);
}

void
tracker_store_queue_commit (TrackerStoreCommitCallback callback,
                            const gchar *client_id,
                            gpointer user_data,
                            GDestroyNotify destroy)
{
	TrackerStorePrivate *private;
	TrackerStoreTask    *task;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	task = g_slice_new0 (TrackerStoreTask);
	task->type = TRACKER_STORE_TASK_TYPE_COMMIT;
	task->user_data = user_data;
	task->callback.commit_callback = callback;
	task->destroy = destroy;
	task->client_id = g_strdup (client_id);
	task->data.update.query = NULL;

	g_queue_push_tail (private->queues[TRACKER_STORE_PRIORITY_LOW], task);

	check_handler (private);
}

void
tracker_store_sparql_query (const gchar *sparql,
                            TrackerStorePriority priority,
                            TrackerStoreSparqlQueryInThread in_thread,
                            TrackerStoreSparqlQueryCallback callback,
                            const gchar *client_id,
                            gpointer user_data,
                            GDestroyNotify destroy)
{
	TrackerStorePrivate *private;
	TrackerStoreTask    *task;

	g_return_if_fail (sparql != NULL);
	g_return_if_fail (in_thread != NULL);

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	task = g_slice_new0 (TrackerStoreTask);
	task->type = TRACKER_STORE_TASK_TYPE_QUERY;
	task->data.query.query = g_strdup (sparql);
	task->user_data = user_data;
	task->callback.query.query_callback = callback;
	task->callback.query.in_thread = in_thread;
	task->destroy = destroy;
	task->client_id = g_strdup (client_id);

	g_queue_push_tail (private->queues[priority], task);

	check_handler (private);
}

void
tracker_store_sparql_update (const gchar *sparql,
                             TrackerStorePriority priority,
                             gboolean batch,
                             TrackerStoreSparqlUpdateCallback callback,
                             const gchar *client_id,
                             gpointer user_data,
                             GDestroyNotify destroy)
{
	TrackerStorePrivate *private;
	TrackerStoreTask    *task;

	g_return_if_fail (sparql != NULL);

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	task = g_slice_new0 (TrackerStoreTask);
	task->type = TRACKER_STORE_TASK_TYPE_UPDATE;
	task->data.update.query = g_strdup (sparql);
	task->data.update.batch = batch;
	task->user_data = user_data;
	task->callback.update_callback = callback;
	task->destroy = destroy;
	task->client_id = g_strdup (client_id);

	g_queue_push_tail (private->queues[priority], task);

	check_handler (private);
}

void
tracker_store_sparql_update_blank (const gchar *sparql,
                                   TrackerStorePriority priority,
                                   TrackerStoreSparqlUpdateBlankCallback callback,
                                   const gchar *client_id,
                                   gpointer user_data,
                                   GDestroyNotify destroy)
{
	TrackerStorePrivate *private;
	TrackerStoreTask    *task;

	g_return_if_fail (sparql != NULL);

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	task = g_slice_new0 (TrackerStoreTask);
	task->type = TRACKER_STORE_TASK_TYPE_UPDATE_BLANK;
	task->data.update.query = g_strdup (sparql);
	task->user_data = user_data;
	task->callback.update_blank_callback = callback;
	task->destroy = destroy;
	task->client_id = g_strdup (client_id);

	g_queue_push_tail (private->queues[priority], task);

	check_handler (private);
}

void
tracker_store_queue_turtle_import (GFile                      *file,
                                   TrackerStoreTurtleCallback  callback,
                                   gpointer                    user_data,
                                   GDestroyNotify              destroy)
{
	TrackerStorePrivate *private;
	TrackerStoreTask    *task;

	g_return_if_fail (G_IS_FILE (file));

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	task = g_slice_new0 (TrackerStoreTask);
	task->type = TRACKER_STORE_TASK_TYPE_TURTLE;
	task->data.turtle.path = g_file_get_path (file);
	task->user_data = user_data;
	task->callback.update_callback = callback;
	task->destroy = destroy;

	g_queue_push_tail (private->queues[TRACKER_STORE_PRIORITY_TURTLE], task);

	check_handler (private);
}

guint
tracker_store_get_queue_size (void)
{
	TrackerStorePrivate *private;
	gint i;
	guint result = 0;

	private = g_static_private_get (&private_key);
	g_return_val_if_fail (private != NULL, 0);

	for (i = 0; i < TRACKER_STORE_N_PRIORITIES; i++) {
		result += g_queue_get_length (private->queues[i]);
	}
	return result;
}

void
tracker_store_unreg_batches (const gchar *client_id)
{
	TrackerStorePrivate *private;
	static GError *error = NULL;
	GList *list, *cur;
	GSList *running;
	GQueue *queue;
	gint i;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	for (running = private->running_tasks; running; running = running->next) {
		TrackerStoreTask *task;

		task = running->data;

		if (task->data.query.running_thread &&
                    g_strcmp0 (task->client_id, client_id) == 0) {
			tracker_data_manager_interrupt_thread (task->data.query.running_thread);
		}
	}

	for (i = 0; i < TRACKER_STORE_N_PRIORITIES; i++) {
		queue = private->queues[i];

		list = queue->head;

		while (list) {
			TrackerStoreTask *task;

			cur = list;
			list = list->next;
			task = cur->data;

			if (task && task->type != TRACKER_STORE_TASK_TYPE_TURTLE) {
				if (g_strcmp0 (task->client_id, client_id) == 0) {
					if (!error) {
						g_set_error (&error, TRACKER_DBUS_ERROR, 0,
							     "Client disappeared");
					}

					if (task->type == TRACKER_STORE_TASK_TYPE_QUERY) {
						task->callback.query.query_callback (NULL, error, task->user_data);
					} else if (task->type == TRACKER_STORE_TASK_TYPE_UPDATE) {
						task->callback.update_callback (error, task->user_data);
					} else if (task->type == TRACKER_STORE_TASK_TYPE_UPDATE_BLANK) {
						task->callback.update_blank_callback (NULL, error, task->user_data);
					} else if (task->type == TRACKER_STORE_TASK_TYPE_COMMIT) {
						task->callback.commit_callback (task->user_data);
					}
					task->destroy (task->user_data);

					g_queue_delete_link (queue, cur);

					store_task_free (task);
				}
			}
		}
	}

	if (error) {
		g_clear_error (&error);
	}

	check_handler (private);
}
