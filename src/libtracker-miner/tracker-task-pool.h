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

#ifndef __LIBTRACKER_MINER_TASK_POOL_H__
#define __LIBTRACKER_MINER_TASK_POOL_H__

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

/* Task pool */
#define TRACKER_TYPE_TASK_POOL         (tracker_task_pool_get_type())
#define TRACKER_TASK_POOL(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_TASK_POOL, TrackerTaskPool))
#define TRACKER_TASK_POOL_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    TRACKER_TYPE_TASK_POOL, TrackerTaskPoolClass))
#define TRACKER_IS_TASK_POOL(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_TASK_POOL))
#define TRACKER_IS_TASK_POOL_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    TRACKER_TYPE_TASK_POOL))
#define TRACKER_TASK_POOL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  TRACKER_TYPE_TASK_POOL, TrackerTaskPoolClass))

typedef struct _TrackerTaskPool TrackerTaskPool;
typedef struct _TrackerTaskPoolClass TrackerTaskPoolClass;
typedef struct _TrackerTask TrackerTask;

struct _TrackerTaskPool
{
	GObject parent_instance;
	gpointer priv;
};

struct _TrackerTaskPoolClass
{
	GObjectClass parent_class;
};

GType    tracker_task_pool_get_type      (void) G_GNUC_CONST;

TrackerTaskPool *tracker_task_pool_new   (guint limit);

void     tracker_task_pool_set_limit     (TrackerTaskPool *pool,
                                          guint            limit);
guint    tracker_task_pool_get_limit     (TrackerTaskPool *pool);
guint    tracker_task_pool_get_size      (TrackerTaskPool *pool);

gboolean tracker_task_pool_limit_reached (TrackerTaskPool *pool);

void     tracker_task_pool_add           (TrackerTaskPool *pool,
                                          TrackerTask     *task);

gboolean tracker_task_pool_remove        (TrackerTaskPool *pool,
                                          TrackerTask     *task);

void     tracker_task_pool_foreach       (TrackerTaskPool *pool,
                                          GFunc            func,
                                          gpointer         user_data);

TrackerTask * tracker_task_pool_find     (TrackerTaskPool *pool,
                                          GFile           *file);

/* Task */
TrackerTask * tracker_task_new         (GFile          *file,
                                        gpointer        data,
                                        GDestroyNotify  destroy_notify);

GFile *       tracker_task_get_file    (TrackerTask    *task);

TrackerTask * tracker_task_ref         (TrackerTask    *task);
void          tracker_task_unref       (TrackerTask    *task);

gpointer      tracker_task_get_data    (TrackerTask    *task);


G_END_DECLS

#endif /* __LIBTRACKER_MINER_TASK_POOL_H__ */
