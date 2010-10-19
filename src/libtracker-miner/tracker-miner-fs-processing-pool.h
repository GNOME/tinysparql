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

#ifndef __LIBTRACKER_MINER_MINER_FS_PROCESSING_POOL_H__
#define __LIBTRACKER_MINER_MINER_FS_PROCESSING_POOL_H__

#include <glib-object.h>
#include <gio/gio.h>

#include <libtracker-sparql/tracker-sparql.h>
#include "tracker-miner-fs.h"

G_BEGIN_DECLS


typedef struct _ProcessingTask ProcessingTask;
typedef struct _ProcessingPool ProcessingPool;
typedef void  (* ProcessingPoolTaskFinishedCallback) (ProcessingTask *task,
                                                      gpointer        user_data,
                                                      const GError   *error);


ProcessingTask *processing_task_new         (GFile          *file);
void            processing_task_free        (ProcessingTask *task);
GFile          *processing_task_get_file    (ProcessingTask *task);
gpointer        processing_task_get_context (ProcessingTask *task);
void            processing_task_set_context (ProcessingTask *task,
                                             gpointer        context,
                                             GFreeFunc       context_free_func);
void            processing_task_set_sparql  (ProcessingTask *task,
                                             gchar          *sparql);


ProcessingPool *processing_pool_new                   (TrackerSparqlConnection *connection,
                                                       guint                    limit_wait,
                                                       guint                    limit_process);
void            processing_pool_free                  (ProcessingPool          *pool);
void            processing_pool_set_wait_limit        (ProcessingPool          *pool,
                                                       guint                    limit);
void            processing_pool_set_ready_limit       (ProcessingPool          *pool,
                                                       guint                    limit);
guint           processing_pool_get_wait_limit        (ProcessingPool          *pool);
guint           processing_pool_get_ready_limit       (ProcessingPool          *pool);
ProcessingTask *processing_pool_find_task             (ProcessingPool          *pool,
                                                       GFile                   *file,
                                                       gboolean                 path_search);
gboolean        processing_pool_wait_limit_reached    (ProcessingPool          *pool);
gboolean        processing_pool_ready_limit_reached   (ProcessingPool          *pool);

void            processing_pool_remove_task           (ProcessingPool          *pool,
                                                       ProcessingTask          *task);
void            processing_pool_push_wait_task        (ProcessingPool          *pool,
                                                       ProcessingTask          *task);
gboolean        processing_pool_push_ready_task       (ProcessingPool          *pool,
                                                       ProcessingTask          *task,
                                                       gboolean                 buffer,
                                                       ProcessingPoolTaskFinishedCallback  finished_handler,
                                                       gpointer                 user_data);
guint           processing_pool_get_wait_task_count    (ProcessingPool         *pool);
guint           processing_pool_get_ready_task_count   (ProcessingPool         *pool);
guint           processing_pool_get_total_task_count   (ProcessingPool         *pool);
ProcessingTask *processing_pool_get_last_wait          (ProcessingPool         *pool);
void            processing_pool_foreach                (ProcessingPool         *pool,
                                                        GFunc                   func,
                                                        gpointer                user_data);
void            processing_pool_buffer_flush           (ProcessingPool         *pool);

G_END_DECLS

#endif /* __LIBTRACKER_MINER_MINER_FS_PROCESSING_POOL_H__ */
