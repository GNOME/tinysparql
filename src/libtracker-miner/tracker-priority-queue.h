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

#ifndef __LIBTRACKER_MINER_PRIORITY_QUEUE_H__
#define __LIBTRACKER_MINER_PRIORITY_QUEUE_H__

#include <glib.h>

G_BEGIN_DECLS

typedef struct _TrackerPriorityQueue TrackerPriorityQueue;

TrackerPriorityQueue *tracker_priority_queue_new   (void);

TrackerPriorityQueue *tracker_priority_queue_ref   (TrackerPriorityQueue *queue);
void                  tracker_priority_queue_unref (TrackerPriorityQueue *queue);

gboolean tracker_priority_queue_is_empty           (TrackerPriorityQueue *queue);

guint    tracker_priority_queue_get_length         (TrackerPriorityQueue *queue);

GList *  tracker_priority_queue_add     (TrackerPriorityQueue *queue,
                                         gpointer              data,
                                         gint                  priority);
void     tracker_priority_queue_foreach (TrackerPriorityQueue *queue,
                                         GFunc                 func,
                                         gpointer              user_data);

gboolean tracker_priority_queue_foreach_remove (TrackerPriorityQueue *queue,
                                                GEqualFunc            compare_func,
                                                gpointer              compare_user_data,
                                                GDestroyNotify        destroy_notify);

gpointer tracker_priority_queue_find           (TrackerPriorityQueue *queue,
                                                gint                 *priority_out,
                                                GEqualFunc            compare_func,
                                                gpointer              data);

gpointer tracker_priority_queue_peek    (TrackerPriorityQueue *queue,
                                         gint                 *priority_out);
gpointer tracker_priority_queue_pop     (TrackerPriorityQueue *queue,
                                         gint                 *priority_out);

GList *  tracker_priority_queue_get_head    (TrackerPriorityQueue *queue);
void     tracker_priority_queue_add_node    (TrackerPriorityQueue *queue,
                                             GList                *node,
                                             gint                  priority);
void     tracker_priority_queue_remove_node (TrackerPriorityQueue  *queue,
                                             GList                 *node);
GList *  tracker_priority_queue_pop_node    (TrackerPriorityQueue *queue,
                                             gint                 *priority_out);


G_END_DECLS

#endif /* __LIBTRACKER_TRACKER_PRIORITY_QUEUE_H__ */
