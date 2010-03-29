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

#include <libtracker-data/tracker-data-update.h>
#include <libtracker-data/tracker-data-query.h>
#include <libtracker-data/tracker-sparql-query.h>

#include "tracker-store.h"

#define TRACKER_STORE_TRANSACTION_MAX                   4000

typedef struct {
	gboolean  have_handler, have_sync_handler;
	gboolean  batch_mode, start_log;
	guint     batch_count;
	GQueue   *queue;
	guint     handler, sync_handler;
} TrackerStorePrivate;

typedef enum {
	TRACKER_STORE_TASK_TYPE_UPDATE = 0,
	TRACKER_STORE_TASK_TYPE_COMMIT = 1,
	TRACKER_STORE_TASK_TYPE_TURTLE = 2,
} TrackerStoreTaskType;

typedef struct {
	TrackerStoreTaskType  type;
	union {
		struct {
			gchar                   *query;
			gchar                   *client_id;
		} update;
		struct {
			gboolean           in_progress;
			gchar             *path;
		} turtle;
	} data;
	gpointer                   user_data;
	GDestroyNotify             destroy;
	union {
		TrackerStoreSparqlUpdateCallback update_callback;
		TrackerStoreCommitCallback       commit_callback;
		TrackerStoreTurtleCallback       turtle_callback;
	} callback;
} TrackerStoreTask;

static GStaticPrivate private_key = G_STATIC_PRIVATE_INIT;

static void
private_free (gpointer data)
{
	TrackerStorePrivate *private = data;

	g_queue_free (private->queue);
	g_free (private);
}

static void
store_task_free (TrackerStoreTask *task)
{
	if (task->type == TRACKER_STORE_TASK_TYPE_TURTLE) {
		g_free (task->data.turtle.path);
	} else {
		g_free (task->data.update.query);
		g_free (task->data.update.client_id);
	}
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

		private->batch_mode = FALSE;
		private->batch_count = 0;
	}
}


static gboolean
queue_idle_handler (gpointer user_data)
{
	TrackerStorePrivate *private = user_data;
	TrackerStoreTask    *task;

	task = g_queue_peek_head (private->queue);
	g_return_val_if_fail (task != NULL, FALSE);

	if (task->type == TRACKER_STORE_TASK_TYPE_UPDATE) {
		GError *error = NULL;

		begin_batch (private);

		tracker_data_update_sparql (task->data.update.query, &error);

		if (!error) {
			private->batch_count++;
			if (private->batch_count >= TRACKER_STORE_TRANSACTION_MAX) {
				end_batch (private);
			}
		}

		if (task->callback.update_callback) {
			task->callback.update_callback (error, task->user_data);
		}

		if (error) {
			g_clear_error (&error);
		}
	} else if (task->type == TRACKER_STORE_TASK_TYPE_COMMIT) {
		end_batch (private);

		if (task->callback.commit_callback) {
			task->callback.commit_callback (task->user_data);
		}
	} else if (task->type == TRACKER_STORE_TASK_TYPE_TURTLE) {
		GError *error = NULL;
		static TrackerTurtleReader *turtle_reader = NULL;

		if (!task->data.turtle.in_progress) {
			turtle_reader = tracker_turtle_reader_new (task->data.turtle.path, &error);
			if (error) {
				if (task->callback.turtle_callback) {
					task->callback.turtle_callback (error, task->user_data);
				}

				turtle_reader = NULL;
				g_clear_error (&error);

				goto out;
			}
			task->data.turtle.in_progress = TRUE;
		}

		begin_batch (private);

		if (process_turtle_file_part (turtle_reader, &error)) {
			/* import still in progress */
			private->batch_count++;
			if (private->batch_count >= TRACKER_STORE_TRANSACTION_MAX) {
				end_batch (private);
			}

			/* Process function wont return true in case of error */

			return TRUE;
		} else {
			/* import finished */
			task->data.turtle.in_progress = FALSE;

			end_batch (private);

			if (task->callback.turtle_callback) {
				task->callback.turtle_callback (error, task->user_data);
			}

			g_object_unref (turtle_reader);
			turtle_reader = NULL;
			if (error) {
				g_clear_error (&error);
			}
		}
	}

 out:
	g_queue_pop_head (private->queue);

	if (task->destroy) {
		task->destroy (task->user_data);
	}

	store_task_free (task);

	return !g_queue_is_empty (private->queue);
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

	private = g_new0 (TrackerStorePrivate, 1);

	private->queue = g_queue_new ();

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
	task->data.update.client_id = g_strdup (client_id);
	task->data.update.query = NULL;

	g_queue_push_tail (private->queue, task);

	if (!private->have_handler) {
		start_handler (private);
	}
}


void
tracker_store_queue_sparql_update (const gchar *sparql,
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
	task->user_data = user_data;
	task->callback.update_callback = callback;
	task->destroy = destroy;
	task->data.update.client_id = g_strdup (client_id);

	g_queue_push_tail (private->queue, task);

	if (!private->have_handler) {
		start_handler (private);
	}
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

	g_queue_push_tail (private->queue, task);

	if (!private->have_handler) {
		start_handler (private);
	}
}

void
tracker_store_sparql_update (const gchar *sparql,
                             GError     **error)
{
	TrackerStorePrivate *private;

	g_return_if_fail (sparql != NULL);

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	if (private->batch_mode) {
		/* commit pending batch items */
		tracker_data_commit_db_transaction ();
		private->batch_mode = FALSE;
		private->batch_count = 0;
	}

	tracker_data_begin_db_transaction ();
	tracker_data_update_sparql (sparql, error);
	tracker_data_commit_db_transaction ();

}

GPtrArray *
tracker_store_sparql_update_blank (const gchar *sparql,
                                   GError     **error)
{
	TrackerStorePrivate *private;
	GPtrArray *blank_nodes;

	g_return_val_if_fail (sparql != NULL, NULL);

	private = g_static_private_get (&private_key);
	g_return_val_if_fail (private != NULL, NULL);

	if (private->batch_mode) {
		/* commit pending batch items */
		tracker_data_commit_db_transaction ();
		private->batch_mode = FALSE;
		private->batch_count = 0;
	}

	tracker_data_begin_db_transaction ();
	blank_nodes = tracker_data_update_sparql_blank (sparql, error);
	tracker_data_commit_db_transaction ();

	return blank_nodes;
}

TrackerDBResultSet*
tracker_store_sparql_query (const gchar *sparql,
                            GError     **error)
{
	return tracker_data_query_sparql (sparql, error);
}

guint
tracker_store_get_queue_size (void)
{
	TrackerStorePrivate *private;

	private = g_static_private_get (&private_key);
	g_return_val_if_fail (private != NULL, 0);

	return g_queue_get_length (private->queue);
}

void
tracker_store_unreg_batches (const gchar *client_id)
{
	TrackerStorePrivate *private;
	static GError *error = NULL;
	GList *list, *cur;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	list = private->queue->head;

	while (list) {
		TrackerStoreTask *task;

		cur = list;
		list = list->next;
		task = cur->data;

		if (task && task->type != TRACKER_STORE_TASK_TYPE_TURTLE) {
			if (g_strcmp0 (task->data.update.client_id, client_id) == 0) {
				if (task->type == TRACKER_STORE_TASK_TYPE_UPDATE) {
					if (!error) {
						g_set_error (&error, TRACKER_DBUS_ERROR, 0,
						             "Client disappeared");
					}
					task->callback.update_callback (error, task->user_data);
				} else {
					task->callback.commit_callback (task->user_data);
				}
				task->destroy (task->user_data);

				g_queue_delete_link (private->queue, cur);

				store_task_free (task);
			}
		}
	}

	if (error) {
		g_clear_error (&error);
	}
}
