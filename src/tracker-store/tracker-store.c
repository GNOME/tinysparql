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

#define _GNU_SOURCE
#include <sched.h>
#include <pthread.h>

#include <unistd.h>
#include <sys/types.h>

#include <libtracker-common/tracker-dbus.h>

#include <libtracker-data/tracker-data-manager.h>
#include <libtracker-data/tracker-data-update.h>
#include <libtracker-data/tracker-data-query.h>
#include <libtracker-data/tracker-db-dbus.h>
#include <libtracker-data/tracker-db-interface-sqlite.h>
#include <libtracker-data/tracker-sparql-query.h>

#include "tracker-store.h"
#include "tracker-events.h"

#define TRACKER_STORE_MAX_CONCURRENT_QUERIES               2

#define TRACKER_STORE_MAX_TASK_TIME          30

typedef struct {
	gboolean       start_log;
	GQueue        *query_queues[TRACKER_STORE_N_PRIORITIES];
	GQueue        *update_queues[TRACKER_STORE_N_PRIORITIES];
	guint          n_queries_running;
	gboolean       update_running;
	GThreadPool   *update_pool;
	GThreadPool   *query_pool;
	GSList        *running_tasks;
	guint          max_task_time;
	gboolean       active;
	GDestroyNotify active_callback;
	gpointer       active_user_data;
} TrackerStorePrivate;

typedef enum {
	TRACKER_STORE_TASK_TYPE_QUERY,
	TRACKER_STORE_TASK_TYPE_UPDATE,
	TRACKER_STORE_TASK_TYPE_UPDATE_BLANK,
	TRACKER_STORE_TASK_TYPE_TURTLE,
} TrackerStoreTaskType;

typedef struct {
	TrackerStoreTaskType  type;
	union {
		struct {
			gchar        *query;
			GCancellable *cancellable;
			gpointer      thread_data;
			guint         watchdog_id;
		} query;
		struct {
			gchar        *query;
			GPtrArray    *blank_nodes;
			TrackerStorePriority priority;
		} update;
		struct {
			gchar        *path;
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
		TrackerStoreTurtleCallback            turtle_callback;
	} callback;
} TrackerStoreTask;

typedef struct {
	GDestroyNotify callback;
	gpointer user_data;
} SetActiveDelegate;

static GStaticPrivate private_key = G_STATIC_PRIVATE_INIT;

#ifdef __USE_GNU
/* cpu used for mainloop thread and main update/query thread */
static int main_cpu;
#endif /* __USE_GNU */

static void
private_free (gpointer data)
{
	TrackerStorePrivate *private = data;
	gint i;

	for (i = 0; i < TRACKER_STORE_N_PRIORITIES; i++) {
		g_queue_free (private->query_queues[i]);
		g_queue_free (private->update_queues[i]);
	}
	g_free (private);
}

static void
store_task_free (TrackerStoreTask *task)
{
	if (task->type == TRACKER_STORE_TASK_TYPE_TURTLE) {
		g_free (task->data.turtle.path);
	} else if (task->type == TRACKER_STORE_TASK_TYPE_QUERY) {
		g_free (task->data.query.query);
		if (task->data.query.watchdog_id) {
			g_source_remove (task->data.query.watchdog_id);
		}
		if (task->data.query.cancellable) {
			g_object_unref (task->data.query.cancellable);
		}
	} else {
		g_free (task->data.update.query);
	}

	g_free (task->client_id);
	g_slice_free (TrackerStoreTask, task);
}

static gboolean
watchdog_cb (gpointer user_data)
{
	TrackerStoreTask *task = user_data;

	g_cancellable_cancel (task->data.query.cancellable);

	return FALSE;
}

static void
sched (TrackerStorePrivate *private)
{
	GQueue              *queue;
	TrackerStoreTask    *task;
	gint                 i;

	if (!private->active) {
		return;
	}

	while (private->n_queries_running < TRACKER_STORE_MAX_CONCURRENT_QUERIES) {
		for (i = 0; i < TRACKER_STORE_N_PRIORITIES; i++) {
			queue = private->query_queues[i];
			task = g_queue_pop_head (queue);
			if (task != NULL) {
				break;
			}
		}
		if (task == NULL) {
			/* no pending query */
			break;
		}

		private->running_tasks = g_slist_prepend (private->running_tasks, task);

		if (private->max_task_time) {
			task->data.query.watchdog_id = g_timeout_add_seconds (private->max_task_time,
			                                                      watchdog_cb, task);
		}

		private->n_queries_running++;
		g_thread_pool_push (private->query_pool, task, NULL);
	}

	if (!private->update_running) {
		for (i = 0; i < TRACKER_STORE_N_PRIORITIES; i++) {
			queue = private->update_queues[i];
			task = g_queue_pop_head (queue);
			if (task != NULL) {
				break;
			}
		}
		if (task != NULL) {
			private->update_running = TRUE;
			g_thread_pool_push (private->update_pool, task, NULL);
		}
	}
}

static gboolean
start_timer_or_not (TrackerStorePrivate *private,
                    TrackerStoreTask    *task)
{
	gboolean result;

	switch (task->type) {
		case TRACKER_STORE_TASK_TYPE_UPDATE:
		case TRACKER_STORE_TASK_TYPE_UPDATE_BLANK:
			result = !(task->data.update.priority == TRACKER_STORE_PRIORITY_LOW &&
			           g_queue_get_length (private->update_queues[TRACKER_STORE_PRIORITY_LOW]) > 0);
			break;
		case TRACKER_STORE_TASK_TYPE_TURTLE:
			result = g_queue_get_length (private->update_queues[TRACKER_STORE_PRIORITY_TURTLE]) == 0;
			break;
		case TRACKER_STORE_TASK_TYPE_QUERY:
		default:
			result = FALSE;
			break;
	}

	return result;
}

static gboolean
task_finish_cb (gpointer data)
{
	TrackerStorePrivate *private;
	TrackerStoreTask *task;

	private = g_static_private_get (&private_key);
	task = data;

	if (task->type == TRACKER_STORE_TASK_TYPE_QUERY) {
		if (!task->error) {
			g_cancellable_set_error_if_cancelled (task->data.query.cancellable, &task->error);
		}

		if (task->callback.query.query_callback) {
			task->callback.query.query_callback (task->data.query.thread_data, task->error, task->user_data);
		}

		if (task->error) {
			g_clear_error (&task->error);
		}

		private->running_tasks = g_slist_remove (private->running_tasks, task);
		private->n_queries_running--;
	} else if (task->type == TRACKER_STORE_TASK_TYPE_UPDATE) {
		if (!task->error) {
			tracker_data_notify_transaction (start_timer_or_not (private, task));
		}

		if (task->callback.update_callback) {
			task->callback.update_callback (task->error, task->user_data);
		}

		if (task->error) {
			g_clear_error (&task->error);
		}

		private->update_running = FALSE;
	} else if (task->type == TRACKER_STORE_TASK_TYPE_UPDATE_BLANK) {
		if (!task->error) {
			tracker_data_notify_transaction (start_timer_or_not (private, task));
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
		if (!task->error) {
			tracker_data_notify_transaction (start_timer_or_not (private, task));
		}

		if (task->callback.turtle_callback) {
			task->callback.turtle_callback (task->error, task->user_data);
		}

		if (task->error) {
			g_clear_error (&task->error);
		}

		private->update_running = FALSE;
	}

	if (task->destroy) {
		task->destroy (task->user_data);
	}

	store_task_free (task);

	if (private->n_queries_running == 0 &&
	    !private->update_running &&
	    private->active_callback)
	{
		private->active_callback (private->active_user_data);
		private->active_callback = NULL;
	}

	sched (private);

	return FALSE;
}

static void
pool_dispatch_cb (gpointer data,
                  gpointer user_data)
{
	TrackerStorePrivate *private;
	TrackerStoreTask *task;

#ifdef __USE_GNU
        /* special task, only ever sent to main pool */
        if (GPOINTER_TO_INT (data) == 1) {
                if (g_getenv ("TRACKER_STORE_DISABLE_CPU_AFFINITY") == NULL) {
                        cpu_set_t cpuset;
                        CPU_ZERO (&cpuset);
                        CPU_SET (main_cpu, &cpuset);

                        /* avoid cpu hopping which can lead to significantly worse performance */
                        pthread_setaffinity_np (pthread_self (), sizeof (cpu_set_t), &cpuset);
                        return;
                }
        }
#endif /* __USE_GNU */

	private = user_data;
	task = data;

	if (task->type == TRACKER_STORE_TASK_TYPE_QUERY) {
		TrackerDBCursor *cursor;

		cursor = tracker_data_query_sparql_cursor (task->data.query.query, &task->error);

		task->data.query.thread_data = task->callback.query.in_thread (cursor, task->data.query.cancellable, task->error, task->user_data);

		if (cursor)
			g_object_unref (cursor);

	} else if (task->type == TRACKER_STORE_TASK_TYPE_UPDATE) {
		tracker_data_update_sparql (task->data.update.query, &task->error);
	} else if (task->type == TRACKER_STORE_TASK_TYPE_UPDATE_BLANK) {
		task->data.update.blank_nodes = tracker_data_update_sparql_blank (task->data.update.query, &task->error);
	} else if (task->type == TRACKER_STORE_TASK_TYPE_TURTLE) {
		GFile *file;

		file = g_file_new_for_path (task->data.turtle.path);

		tracker_events_freeze ();
		tracker_data_load_turtle_file (file, &task->error);
		tracker_events_reset_pending ();

		g_object_unref (file);
	}

	g_idle_add (task_finish_cb, task);
}

void
tracker_store_init (void)
{
	TrackerStorePrivate *private;
	gint i;
	const char *tmp;
#ifdef __USE_GNU
	cpu_set_t cpuset;
#endif /* __USE_GNU */

	private = g_new0 (TrackerStorePrivate, 1);

	if ((tmp = g_getenv("TRACKER_STORE_MAX_TASK_TIME")) != NULL) {
		private->max_task_time = atoi (tmp);
	} else {
		private->max_task_time = TRACKER_STORE_MAX_TASK_TIME;
	}

	for (i = 0; i < TRACKER_STORE_N_PRIORITIES; i++) {
		private->query_queues[i] = g_queue_new ();
		private->update_queues[i] = g_queue_new ();
	}

	private->update_pool = g_thread_pool_new (pool_dispatch_cb,
	                                          private, 1,
	                                          TRUE, NULL);
	private->query_pool = g_thread_pool_new (pool_dispatch_cb,
	                                         private, TRACKER_STORE_MAX_CONCURRENT_QUERIES,
	                                         TRUE, NULL);

	/* as the following settings are global for unknown reasons,
	   let's use the same settings as gio, otherwise the used settings
	   are rather random */
	g_thread_pool_set_max_idle_time (15 * 1000);
	g_thread_pool_set_max_unused_threads (2);

#ifdef __USE_GNU
        if (g_getenv ("TRACKER_STORE_DISABLE_CPU_AFFINITY") == NULL) {
		sched_getcpu ();
                main_cpu = sched_getcpu ();
                CPU_ZERO (&cpuset);
                CPU_SET (main_cpu, &cpuset);

                /* avoid cpu hopping which can lead to significantly worse performance */
                pthread_setaffinity_np (pthread_self (), sizeof (cpu_set_t), &cpuset);
                /* lock main update/query thread to same cpu to improve overall performance
                   main loop thread is essentially idle during query execution */
                g_thread_pool_push (private->update_pool, GINT_TO_POINTER (1), NULL);
        }
#endif /* __USE_GNU */

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

	g_thread_pool_free (private->query_pool, FALSE, TRUE);
	g_thread_pool_free (private->update_pool, FALSE, TRUE);

	g_static_private_set (&private_key, NULL, NULL);
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
	task->data.query.cancellable = g_cancellable_new ();
	task->user_data = user_data;
	task->callback.query.query_callback = callback;
	task->callback.query.in_thread = in_thread;
	task->destroy = destroy;
	task->client_id = g_strdup (client_id);

	g_queue_push_tail (private->query_queues[priority], task);

	sched (private);
}

void
tracker_store_sparql_update (const gchar *sparql,
                             TrackerStorePriority priority,
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
	task->data.update.priority = priority;
	task->user_data = user_data;
	task->callback.update_callback = callback;
	task->destroy = destroy;
	task->client_id = g_strdup (client_id);

	g_queue_push_tail (private->update_queues[priority], task);

	sched (private);
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

	g_queue_push_tail (private->update_queues[priority], task);

	sched (private);
}

void
tracker_store_queue_turtle_import (GFile                      *file,
                                   TrackerStoreTurtleCallback  callback,
                                   const gchar                *client_id,
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
	task->client_id = g_strdup (client_id);

	g_queue_push_tail (private->update_queues[TRACKER_STORE_PRIORITY_TURTLE], task);

	sched (private);
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
		result += g_queue_get_length (private->query_queues[i]);
		result += g_queue_get_length (private->update_queues[i]);
	}
	return result;
}

static void
unreg_task (TrackerStoreTask *task,
            GError           *error)
{
	if (task->type == TRACKER_STORE_TASK_TYPE_QUERY) {
		task->callback.query.query_callback (NULL, error, task->user_data);
	} else if (task->type == TRACKER_STORE_TASK_TYPE_UPDATE) {
		task->callback.update_callback (error, task->user_data);
	} else if (task->type == TRACKER_STORE_TASK_TYPE_UPDATE_BLANK) {
		task->callback.update_blank_callback (NULL, error, task->user_data);
	} else if (task->type == TRACKER_STORE_TASK_TYPE_TURTLE) {
		task->callback.turtle_callback (error, task->user_data);
	}

	if (task->destroy) {
		task->destroy (task->user_data);
	}

	store_task_free (task);
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

		if (task->data.query.cancellable &&
		    g_strcmp0 (task->client_id, client_id) == 0) {
			g_cancellable_cancel (task->data.query.cancellable);
		}
	}

	for (i = 0; i < TRACKER_STORE_N_PRIORITIES; i++) {
		queue = private->query_queues[i];
		list = queue->head;
		while (list) {
			TrackerStoreTask *task;

			cur = list;
			list = list->next;
			task = cur->data;

			if (task && g_strcmp0 (task->client_id, client_id) == 0) {
				g_queue_delete_link (queue, cur);

				if (!error) {
					g_set_error (&error, TRACKER_DBUS_ERROR, 0,
						     "Client disappeared");
				}

				unreg_task (task, error);
			}
		}

		queue = private->update_queues[i];
		list = queue->head;
		while (list) {
			TrackerStoreTask *task;

			cur = list;
			list = list->next;
			task = cur->data;

			if (task && g_strcmp0 (task->client_id, client_id) == 0) {
				g_queue_delete_link (queue, cur);

				if (!error) {
					g_set_error (&error, TRACKER_DBUS_ERROR, 0,
						     "Client disappeared");
				}

				unreg_task (task, error);
			}
		}
	}

	if (error) {
		g_clear_error (&error);
	}

	sched (private);
}

static gboolean
active_cb_in_idle (gpointer user_data)
{
	SetActiveDelegate *data = user_data;

	data->callback (data->user_data);
	g_free (data);

	return FALSE;
}

void
tracker_store_set_active (gboolean       active,
                          GDestroyNotify callback,
                          gpointer       user_data)
{
	TrackerStorePrivate *private;

	private = g_static_private_get (&private_key);

	private->active = active;

	if (active == FALSE &&
	    private->n_queries_running == 0 &&
	    !private->update_running &&
	    callback)
	{
		SetActiveDelegate *data = g_new0 (SetActiveDelegate, 1);

		data->callback = callback;
		data->user_data = user_data;
		g_idle_add (active_cb_in_idle, data);

		private->active_callback = NULL;
	} else {
		private->active_callback = callback;
		private->active_user_data = user_data;
	}

	if (active) {
		sched (private);
	}
}
