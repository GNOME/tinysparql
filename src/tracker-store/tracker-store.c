/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2009, Nokia
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

#include <libtracker-common/tracker-dbus.h>
#include <libtracker-db/tracker-db-dbus.h>

#include <libtracker-data/tracker-data-update.h>
#include <libtracker-data/tracker-data-query.h>
#include <libtracker-data/tracker-turtle.h>

#include "tracker-store.h"

#define TRACKER_STORE_TRANSACTION_MAX	4000

typedef struct {
	gboolean  have_handler;
	gboolean  batch_mode;
	guint     batch_count;
	GQueue   *queue;
} TrackerStorePrivate;

typedef enum {
	TRACKER_STORE_TASK_TYPE_UPDATE = 0,
	TRACKER_STORE_TASK_TYPE_COMMIT = 1,
	TRACKER_STORE_TASK_TYPE_TURTLE = 2,
	/* To be removed when query builder is available */
	TRACKER_STORE_TASK_TYPE_STATEMENT = 3
} TrackerStoreTaskType;

typedef struct {
	TrackerStoreTaskType  type;
	union {
	  gchar                   *query;
	  struct {
		gboolean           in_progress;
		gchar             *path;
	  } turtle;
	/* To be removed when query builder is available */
	  struct {
		gchar             *subject;
		gchar             *predicate;
		gchar             *object;
	  } statement;
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
tracker_store_task_free (TrackerStoreTask *task)
{
	if (task->type == TRACKER_STORE_TASK_TYPE_TURTLE) {
		g_free (task->data.turtle.path);
	} else 	if (task->type == TRACKER_STORE_TASK_TYPE_STATEMENT) {
		g_free (task->data.statement.subject);
		g_free (task->data.statement.predicate);
		g_free (task->data.statement.object);
	} else {
		g_free (task->data.query);
	}
	g_slice_free (TrackerStoreTask, task);
}

static gboolean
process_turtle_file_part (void)
{
	int i;

	/* process 10 statements at once before returning to main loop */

	i = 0;

	while (tracker_turtle_reader_next ()) {
		/* insert statement */
		tracker_data_insert_statement (
			tracker_turtle_reader_get_subject (),
			tracker_turtle_reader_get_predicate (),
			tracker_turtle_reader_get_object ());

		i++;
		if (i >= 10) {
			/* return to main loop */
			return TRUE;
		}
	}

	return FALSE;
}

static void
begin_batch (TrackerStorePrivate *private)
{
	if (!private->batch_mode) {
		/* switch to batch mode
		   delays database commits to improve performance */
		tracker_data_begin_transaction ();
		private->batch_mode = TRUE;
		private->batch_count = 0;
	}
}

static void
end_batch (TrackerStorePrivate *private)
{
	if (private->batch_mode) {
		/* commit pending batch items */
		tracker_data_commit_transaction ();
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

		tracker_data_update_sparql (task->data.query, &error);
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
	} else if (task->type == TRACKER_STORE_TASK_TYPE_STATEMENT) {

		/* To be removed when query builder is available */
		tracker_data_insert_statement (task->data.statement.subject,
		                               task->data.statement.predicate,
		                               task->data.statement.object);

		if (task->callback.update_callback) {
			task->callback.update_callback (NULL, task->user_data);
		}

	} else if (task->type == TRACKER_STORE_TASK_TYPE_TURTLE) {
		begin_batch (private);

		if (!task->data.turtle.in_progress) {
			tracker_turtle_reader_init (task->data.turtle.path, NULL);
			task->data.turtle.in_progress = TRUE;
		}

		if (process_turtle_file_part ()) {
			/* import still in progress */
			private->batch_count++;
			if (private->batch_count >= TRACKER_STORE_TRANSACTION_MAX) {
				end_batch (private);
			}

			return TRUE;
		} else {
			/* import finished */
			task->data.turtle.in_progress = FALSE;

			end_batch (private);

			if (task->callback.turtle_callback) {
				task->callback.turtle_callback (NULL, task->user_data);
			}
		}

	}

	g_queue_pop_head (private->queue);

	if (task->destroy) {
		task->destroy (task->user_data);
	}

	tracker_store_task_free (task);

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
		g_debug ("Can't exit until store-queue is finished ...");
		while (private->have_handler) {
			g_main_context_iteration (NULL, TRUE);
		}
		g_debug ("Store-queue finished");
	}

	g_static_private_set (&private_key, NULL, NULL);
}

static void
start_handler (TrackerStorePrivate *private)
{
	private->have_handler = TRUE;

	g_idle_add_full (G_PRIORITY_LOW,
	                 queue_idle_handler,
	                 private,
	                 queue_idle_destroy);
}

void
tracker_store_queue_commit (TrackerStoreCommitCallback callback,
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

	g_queue_push_tail (private->queue, task);

	if (!private->have_handler) {
		start_handler (private);
	}
}


void
tracker_store_queue_sparql_update (const gchar *sparql,
                                   TrackerStoreSparqlUpdateCallback callback,
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
	task->data.query = g_strdup (sparql);
	task->user_data = user_data;
	task->callback.update_callback = callback;
	task->destroy = destroy;

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
		tracker_data_commit_transaction ();
		private->batch_mode = FALSE;
		private->batch_count = 0;
	}

	tracker_data_update_sparql (sparql, error);
}

TrackerDBResultSet*
tracker_store_sparql_query (const gchar *sparql,
                            GError     **error)
{
	return tracker_data_query_sparql (sparql, error);
}

void
tracker_store_insert_statement (const gchar   *subject,
                                const gchar   *predicate,
                                const gchar   *object)
{
	TrackerStorePrivate *private;

	g_return_if_fail (subject != NULL);
	g_return_if_fail (predicate != NULL);
	g_return_if_fail (object != NULL);

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	if (private->batch_mode) {
		/* commit pending batch items */
		tracker_data_commit_transaction ();
		private->batch_mode = FALSE;
		private->batch_count = 0;
	}

	tracker_data_insert_statement (subject, predicate, object);
}

void
tracker_store_delete_statement (const gchar   *subject,
                                const gchar   *predicate,
                                const gchar   *object)
{
	TrackerStorePrivate *private;

	g_return_if_fail (subject != NULL);
	g_return_if_fail (predicate != NULL);
	g_return_if_fail (object != NULL);

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	if (private->batch_mode) {
		/* commit pending batch items */
		tracker_data_commit_transaction ();
		private->batch_mode = FALSE;
		private->batch_count = 0;
	}

	tracker_data_delete_statement (subject, predicate, object);
}

/* To be removed when query builder is available */
void
tracker_store_queue_insert_statement (const gchar *subject,
                                      const gchar *predicate,
                                      const gchar *object,
                                      TrackerStoreSparqlUpdateCallback callback,
                                      gpointer user_data,
                                      GDestroyNotify destroy)
{
	TrackerStorePrivate *private;
	TrackerStoreTask *task;

	g_assert (subject != NULL);
	g_assert (predicate != NULL);
	g_assert (object != NULL);

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);
	task = g_slice_new0 (TrackerStoreTask);
	task->type = TRACKER_STORE_TASK_TYPE_STATEMENT;
	task->data.statement.subject = g_strdup (subject);
	task->data.statement.predicate = g_strdup (predicate);
	task->data.statement.object = g_strdup (object);
	task->user_data = user_data;
	task->callback.update_callback = callback;
	task->destroy = destroy;

	g_queue_push_tail (private->queue, task);

	if (!private->have_handler) {
		start_handler (private);
	}
}
