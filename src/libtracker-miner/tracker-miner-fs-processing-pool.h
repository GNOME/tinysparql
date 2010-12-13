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


typedef struct _TrackerProcessingTask TrackerProcessingTask;
typedef struct _TrackerProcessingPool TrackerProcessingPool;
typedef void  (* TrackerProcessingPoolTaskFinishedCallback) (TrackerProcessingTask *task,
                                                             gpointer               user_data,
                                                             const GError          *error);


TrackerProcessingTask *tracker_processing_task_new               (GFile          *file);
void                   tracker_processing_task_free              (TrackerProcessingTask *task);
GFile                 *tracker_processing_task_get_file          (TrackerProcessingTask *task);
gpointer               tracker_processing_task_get_context       (TrackerProcessingTask *task);
void                   tracker_processing_task_set_context       (TrackerProcessingTask *task,
                                                                  gpointer               context,
                                                                  GFreeFunc              context_free_func);
void                   tracker_processing_task_set_sparql        (TrackerProcessingTask *task,
                                                                  TrackerSparqlBuilder  *sparql);
void                   tracker_processing_task_set_sparql_string (TrackerProcessingTask *task,
                                                                  gchar                 *sparql_string);


TrackerProcessingPool *tracker_processing_pool_new                   (TrackerSparqlConnection *connection,
                                                                      guint                    limit_wait,
                                                                      guint                    limit_process);
void                   tracker_processing_pool_free                  (TrackerProcessingPool   *pool);
void                   tracker_processing_pool_set_wait_limit        (TrackerProcessingPool   *pool,
                                                                      guint                    limit);
void                   tracker_processing_pool_set_ready_limit       (TrackerProcessingPool   *pool,
                                                                      guint                    limit);
guint                  tracker_processing_pool_get_wait_limit        (TrackerProcessingPool   *pool);
guint                  tracker_processing_pool_get_ready_limit       (TrackerProcessingPool   *pool);
TrackerProcessingTask *tracker_processing_pool_find_task             (TrackerProcessingPool   *pool,
                                                                      GFile                   *file,
                                                                      gboolean                 path_search);
gboolean               tracker_processing_pool_wait_limit_reached    (TrackerProcessingPool   *pool);
gboolean               tracker_processing_pool_ready_limit_reached   (TrackerProcessingPool   *pool);

void                   tracker_processing_pool_remove_task           (TrackerProcessingPool   *pool,
                                                                      TrackerProcessingTask   *task);
void                   tracker_processing_pool_push_wait_task        (TrackerProcessingPool   *pool,
                                                                      TrackerProcessingTask   *task);
gboolean               tracker_processing_pool_push_ready_task       (TrackerProcessingPool   *pool,
                                                                      TrackerProcessingTask   *task,
                                                                      gboolean                 buffer,
                                                                      TrackerProcessingPoolTaskFinishedCallback finished_handler,
                                                                      gpointer                 user_data);
guint                  tracker_processing_pool_get_wait_task_count   (TrackerProcessingPool   *pool);
guint                  tracker_processing_pool_get_ready_task_count  (TrackerProcessingPool   *pool);
guint                  tracker_processing_pool_get_total_task_count  (TrackerProcessingPool   *pool);
TrackerProcessingTask *tracker_processing_pool_get_last_wait         (TrackerProcessingPool   *pool);
void                   tracker_processing_pool_foreach               (TrackerProcessingPool   *pool,
                                                                      GFunc                    func,
                                                                      gpointer                 user_data);
void                   tracker_processing_pool_buffer_flush          (TrackerProcessingPool   *pool,
                                                                      const gchar             *reason);

G_END_DECLS

#endif /* __LIBTRACKER_MINER_MINER_FS_PROCESSING_POOL_H__ */
