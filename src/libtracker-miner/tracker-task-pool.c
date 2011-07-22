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

#include "tracker-task-pool.h"

enum {
	PROP_0,
	PROP_LIMIT,
	PROP_LIMIT_REACHED
};

G_DEFINE_TYPE (TrackerTaskPool, tracker_task_pool, G_TYPE_OBJECT)

typedef struct _TrackerTaskPoolPrivate TrackerTaskPoolPrivate;

struct _TrackerTaskPoolPrivate
{
	GHashTable *tasks;
	guint limit;
};

struct _TrackerTask
{
	GFile *file;
	gpointer data;
	GDestroyNotify destroy_notify;
	gint ref_count;
};

static void
tracker_task_pool_finalize (GObject *object)
{
	TrackerTaskPoolPrivate *priv;

	priv = TRACKER_TASK_POOL (object)->priv;
	g_hash_table_destroy (priv->tasks);

	G_OBJECT_CLASS (tracker_task_pool_parent_class)->finalize (object);
}

static void
tracker_task_pool_set_property (GObject      *object,
                                guint         param_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
	TrackerTaskPool *pool = TRACKER_TASK_POOL (object);


	switch (param_id) {
	case PROP_LIMIT:
		tracker_task_pool_set_limit (pool,
		                             g_value_get_uint (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
	}
}

static void
tracker_task_pool_get_property (GObject    *object,
                                guint       param_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
	TrackerTaskPool *pool = TRACKER_TASK_POOL (object);

	switch (param_id) {
	case PROP_LIMIT:
		g_value_set_uint (value,
		                  tracker_task_pool_get_limit (pool));
		break;
	case PROP_LIMIT_REACHED:
		g_value_set_boolean (value,
		                     tracker_task_pool_limit_reached (pool));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
	}
}

static void
tracker_task_pool_class_init (TrackerTaskPoolClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_task_pool_finalize;
	object_class->set_property = tracker_task_pool_set_property;
	object_class->get_property = tracker_task_pool_get_property;

	g_object_class_install_property (object_class,
	                                 PROP_LIMIT,
	                                 g_param_spec_uint ("limit",
	                                                    "Limit",
	                                                    "Task limit",
	                                                    1, G_MAXUINT, 1,
	                                                    G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_LIMIT_REACHED,
	                                 g_param_spec_boolean ("limit-reached",
	                                                       "Limit reached",
	                                                       "Task limit reached",
	                                                       FALSE,
	                                                       G_PARAM_READABLE));

	g_type_class_add_private (klass, sizeof (TrackerTaskPoolPrivate));
}

static gboolean
file_equal (GFile *file1,
            GFile *file2)
{
	if (file1 == file2) {
		return TRUE;
	} else {
		return g_file_equal (file1, file2);
	}
}

static void
tracker_task_pool_init (TrackerTaskPool *pool)
{
	TrackerTaskPoolPrivate *priv;

	priv = pool->priv = G_TYPE_INSTANCE_GET_PRIVATE (pool,
	                                                 TRACKER_TYPE_TASK_POOL,
	                                                 TrackerTaskPoolPrivate);
	priv->tasks = g_hash_table_new_full (g_file_hash,
	                                     (GEqualFunc) file_equal, NULL,
	                                     (GDestroyNotify) tracker_task_unref);
	priv->limit = 0;
}

TrackerTaskPool *
tracker_task_pool_new (guint limit)
{
	return g_object_new (TRACKER_TYPE_TASK_POOL,
	                     "limit", limit,
	                     NULL);
}

void
tracker_task_pool_set_limit (TrackerTaskPool *pool,
                             guint            limit)
{
	TrackerTaskPoolPrivate *priv;
	gboolean old_limit_reached;

	g_return_if_fail (TRACKER_IS_TASK_POOL (pool));

	old_limit_reached = tracker_task_pool_limit_reached (pool);

	priv = pool->priv;
	priv->limit = limit;

	if (old_limit_reached !=
	    tracker_task_pool_limit_reached (pool)) {
		g_object_notify (G_OBJECT (pool), "limit-reached");
	}
}

guint
tracker_task_pool_get_limit (TrackerTaskPool *pool)
{
	TrackerTaskPoolPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_TASK_POOL (pool), 0);

	priv = pool->priv;
	return priv->limit;
}

guint
tracker_task_pool_get_size (TrackerTaskPool *pool)
{
	TrackerTaskPoolPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_TASK_POOL (pool), 0);

	priv = pool->priv;
	return g_hash_table_size (priv->tasks);
}

gboolean
tracker_task_pool_limit_reached (TrackerTaskPool *pool)
{
	TrackerTaskPoolPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_TASK_POOL (pool), FALSE);

	priv = pool->priv;
	return (g_hash_table_size (priv->tasks) >= priv->limit);
}

void
tracker_task_pool_add (TrackerTaskPool *pool,
                       TrackerTask     *task)
{
	TrackerTaskPoolPrivate *priv;

	g_return_if_fail (TRACKER_IS_TASK_POOL (pool));

	priv = pool->priv;

	g_hash_table_insert (priv->tasks,
	                     tracker_task_get_file (task),
	                     tracker_task_ref (task));

	if (g_hash_table_size (priv->tasks) == priv->limit) {
		g_object_notify (G_OBJECT (pool), "limit-reached");
	}
}

gboolean
tracker_task_pool_remove (TrackerTaskPool *pool,
                          TrackerTask     *task)
{
	TrackerTaskPoolPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_TASK_POOL (pool), FALSE);

	priv = pool->priv;

	if (g_hash_table_remove (priv->tasks,
	                         tracker_task_get_file (task))) {
		if (g_hash_table_size (priv->tasks) == priv->limit - 1) {
			/* We've gone below the threshold again */
			g_object_notify (G_OBJECT (pool), "limit-reached");
		}

		return TRUE;
	}

	return FALSE;
}

void
tracker_task_pool_foreach (TrackerTaskPool *pool,
                           GFunc            func,
                           gpointer         user_data)
{
	TrackerTaskPoolPrivate *priv;
	GHashTableIter iter;
	TrackerTask *task;

	g_return_if_fail (TRACKER_IS_TASK_POOL (pool));
	g_return_if_fail (func != NULL);

	priv = pool->priv;
	g_hash_table_iter_init (&iter, priv->tasks);

	while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &task)) {
		(func) (task, user_data);
	}
}

TrackerTask *
tracker_task_pool_find (TrackerTaskPool *pool,
                        GFile           *file)
{
	TrackerTaskPoolPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_TASK_POOL (pool), NULL);
	g_return_val_if_fail (G_IS_FILE (file), NULL);

	priv = pool->priv;
	return g_hash_table_lookup (priv->tasks, file);
}

/* Task */
TrackerTask *
tracker_task_new (GFile          *file,
                  gpointer        data,
                  GDestroyNotify  destroy_notify)
{
	TrackerTask *task;

	task = g_slice_new0 (TrackerTask);
	task->file = g_object_ref (file);
	task->destroy_notify = destroy_notify;
	task->data = data;
	task->ref_count = 1;

	return task;
}

TrackerTask *
tracker_task_ref (TrackerTask *task)
{
	g_return_val_if_fail (task != NULL, NULL);

	g_atomic_int_inc (&task->ref_count);

	return task;
}
void
tracker_task_unref (TrackerTask *task)
{
	g_return_if_fail (task != NULL);

	if (g_atomic_int_dec_and_test (&task->ref_count)) {
		g_object_unref (task->file);

		if (task->data &&
		    task->destroy_notify) {
			(task->destroy_notify) (task->data);
		}

		g_slice_free (TrackerTask, task);
	}
}

GFile *
tracker_task_get_file (TrackerTask *task)
{
	g_return_val_if_fail (task != NULL, NULL);

	return task->file;
}

gpointer
tracker_task_get_data (TrackerTask *task)
{
	g_return_val_if_fail (task != NULL, NULL);

	return task->data;
}
