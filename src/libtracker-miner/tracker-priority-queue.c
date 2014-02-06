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

#include "config.h"

#include "tracker-priority-queue.h"

typedef struct PrioritySegment PrioritySegment;

struct PrioritySegment
{
	gint priority;
	GList *first_elem;
	GList *last_elem;
};

struct _TrackerPriorityQueue
{
	GQueue queue;
	GArray *segments;

	gint ref_count;
};

TrackerPriorityQueue *
tracker_priority_queue_new (void)
{
	TrackerPriorityQueue *queue;

	queue = g_slice_new (TrackerPriorityQueue);
	g_queue_init (&queue->queue);
	queue->segments = g_array_new (FALSE, FALSE,
	                               sizeof (PrioritySegment));

	queue->ref_count = 1;

	return queue;
}

TrackerPriorityQueue *
tracker_priority_queue_ref (TrackerPriorityQueue *queue)
{
	g_atomic_int_inc (&queue->ref_count);
	return queue;
}

void
tracker_priority_queue_unref (TrackerPriorityQueue *queue)
{
	if (g_atomic_int_dec_and_test (&queue->ref_count)) {
		g_queue_clear (&queue->queue);
		g_array_free (queue->segments, TRUE);
		g_slice_free (TrackerPriorityQueue, queue);
	}
}

static void
queue_insert_before_link (GQueue *queue,
                          GList  *sibling,
                          GList  *link_)
{
	if (sibling == queue->head)
		g_queue_push_head_link (queue, link_);
	else {
		link_->next = sibling;
		link_->prev = sibling->prev;
		sibling->prev->next = link_;
		sibling->prev = link_;
		queue->length++;
	}
}

static void
queue_insert_after_link (GQueue *queue,
                         GList  *sibling,
                         GList  *link_)
{
	if (sibling == queue->tail)
		g_queue_push_tail_link (queue, link_);
	else
		queue_insert_before_link (queue, sibling->next, link_);
}

static void
insert_node (TrackerPriorityQueue *queue,
             gint                  priority,
             GList                *node)
{
	PrioritySegment *segment = NULL;
	gboolean found = FALSE;
	gint l, r, c;

	/* Perform binary search to find out the segment for
	 * the given priority, create one if it isn't found.
	 */
	l = 0;
	r = queue->segments->len - 1;

	while (queue->segments->len > 0 && !found) {
		c = (r + l) / 2;
		segment = &g_array_index (queue->segments, PrioritySegment, c);

		if (segment->priority == priority) {
			found = TRUE;
			break;
		} else if (segment->priority > priority) {
			r = c - 1;
		} else if (segment->priority < priority) {
			l = c + 1;
		}

		if (l > r) {
			break;
		}
	}

	if (found) {
		/* Element found, append at the end of segment */
		g_assert (segment != NULL);
		g_assert (segment->priority == priority);

		queue_insert_after_link (&queue->queue, segment->last_elem, node);
		segment->last_elem = node;
	} else {
		PrioritySegment new_segment = { 0 };

		new_segment.priority = priority;

		if (segment) {
			g_assert (segment->priority != priority);

			/* Binary search got to one of the closest results,
			 * but we may have come from either of both sides,
			 * so check whether we have to insert after the
			 * segment we got.
			 */
			if (segment->priority > priority) {
				/* We have to insert to the left of this element */
				queue_insert_before_link (&queue->queue, segment->first_elem, node);
			} else {
				/* We have to insert to the right of this element */
				queue_insert_after_link (&queue->queue, segment->last_elem, node);
				c++;
			}

			new_segment.first_elem = new_segment.last_elem = node;
			g_array_insert_val (queue->segments, c, new_segment);
		} else {
			/* Segments list has 0 elements */
			g_assert (queue->segments->len == 0);
			g_assert (g_queue_get_length (&queue->queue) == 0);

			g_queue_push_head_link (&queue->queue, node);
			new_segment.first_elem = new_segment.last_elem = node;

			g_array_append_val (queue->segments, new_segment);
		}
	}
}

void
tracker_priority_queue_foreach (TrackerPriorityQueue *queue,
                                GFunc                 func,
                                gpointer              user_data)
{
	g_return_if_fail (queue != NULL);
	g_return_if_fail (func != NULL);

	g_queue_foreach (&queue->queue, func, user_data);
}

gboolean
tracker_priority_queue_foreach_remove (TrackerPriorityQueue *queue,
                                       GEqualFunc            compare_func,
                                       gpointer              compare_user_data,
                                       GDestroyNotify        destroy_notify)
{
	PrioritySegment *segment;
	gint n_segment = 0;
	gboolean updated = FALSE;
	GList *list;

	g_return_val_if_fail (queue != NULL, FALSE);
	g_return_val_if_fail (compare_func != NULL, FALSE);

	list = queue->queue.head;

	if (!list) {
		return FALSE;
	}

	segment = &g_array_index (queue->segments, PrioritySegment, n_segment);

	while (list) {
		GList *elem;

		elem = list;
		list = list->next;

		if ((compare_func) (elem->data, compare_user_data)) {
			/* Update segment limits */
			if (elem == segment->first_elem &&
			    elem == segment->last_elem) {
				/* Last element of segment, remove it */
				g_array_remove_index (queue->segments,
				                      n_segment);

				if (list) {
					/* Fetch the next one */
					segment = &g_array_index (queue->segments,
					                          PrioritySegment,
					                          n_segment);
				}
			} else if (elem == segment->first_elem) {
				/* First elemen in segment */
				segment->first_elem = elem->next;
			} else if (elem == segment->last_elem) {
				segment->last_elem = elem->prev;
			}

			if (destroy_notify) {
				(destroy_notify) (elem->data);
			}

			g_queue_delete_link (&queue->queue, elem);
			updated = TRUE;
		} else {
			if (list != NULL &&
			    elem == segment->last_elem) {
				/* Move on to the next segment */
				n_segment++;
				g_assert (n_segment < queue->segments->len);

				segment = &g_array_index (queue->segments,
				                          PrioritySegment,
				                          n_segment);
			}
		}
	}

	return updated;
}

gboolean
tracker_priority_queue_is_empty (TrackerPriorityQueue *queue)
{
	g_return_val_if_fail (queue != NULL, FALSE);

	return g_queue_is_empty (&queue->queue);
}

guint
tracker_priority_queue_get_length (TrackerPriorityQueue *queue)
{
	g_return_val_if_fail (queue != NULL, 0);

	return g_queue_get_length (&queue->queue);
}

GList *
tracker_priority_queue_add (TrackerPriorityQueue *queue,
                            gpointer              data,
                            gint                  priority)
{
	GList *node;

	g_return_val_if_fail (queue != NULL, NULL);
	g_return_val_if_fail (data != NULL, NULL);

	node = g_list_alloc ();
	node->data = data;
	insert_node (queue, priority, node);

	return node;
}

void
tracker_priority_queue_add_node (TrackerPriorityQueue *queue,
                                 GList                *node,
                                 gint                  priority)
{
	g_return_if_fail (queue != NULL);
	g_return_if_fail (node != NULL);

	insert_node (queue, priority, node);
}

void
tracker_priority_queue_remove_node (TrackerPriorityQueue *queue,
                                    GList                *node)
{
	guint i;

	g_return_if_fail (queue != NULL);

	/* Check if it is the first or last of a segment */
	for (i = 0; i < queue->segments->len; i++) {
		PrioritySegment *segment;

		segment = &g_array_index (queue->segments, PrioritySegment, i);

		if (segment->first_elem == node) {
			if (segment->last_elem == node)
				g_array_remove_index (queue->segments, i);
			else
				segment->first_elem = node->next;
			break;
		}

		if (segment->last_elem == node) {
			segment->last_elem = node->prev;
			break;
		}
	}

	g_queue_delete_link (&queue->queue, node);
}

gpointer
tracker_priority_queue_find (TrackerPriorityQueue *queue,
                             gint                 *priority_out,
                             GEqualFunc            compare_func,
                             gpointer              user_data)
{
	PrioritySegment *segment;
	gint n_segment = 0;
	GList *list;

	g_return_val_if_fail (queue != NULL, NULL);
	g_return_val_if_fail (compare_func != NULL, NULL);

	list = queue->queue.head;
	segment = &g_array_index (queue->segments, PrioritySegment, n_segment);

	while (list) {
		if ((compare_func) (list->data, user_data)) {
			if (priority_out) {
				*priority_out = segment->priority;
			}

			return list->data;
		}

		if (list->next != NULL &&
		    list == segment->last_elem) {
			/* Move on to the next segment */
			n_segment++;
			g_assert (n_segment < queue->segments->len);
			segment = &g_array_index (queue->segments,
			                          PrioritySegment,
			                          n_segment);
		}

		list = list->next;
	}

	return NULL;
}

gpointer
tracker_priority_queue_peek (TrackerPriorityQueue *queue,
                             gint                 *priority_out)
{
	g_return_val_if_fail (queue != NULL, NULL);

	if (priority_out && queue->segments->len > 0) {
		PrioritySegment *segment;

		segment = &g_array_index (queue->segments, PrioritySegment, 0);
		*priority_out = segment->priority;
	}

	return g_queue_peek_head (&queue->queue);
}

gpointer
tracker_priority_queue_pop (TrackerPriorityQueue *queue,
                            gint                 *priority_out)
{
	GList *node;
	gpointer data;

	node = tracker_priority_queue_pop_node (queue, priority_out);
	if (node == NULL)
		return NULL;

	data = node->data;
	g_list_free_1 (node);

	return data;
}

GList *
tracker_priority_queue_pop_node (TrackerPriorityQueue *queue,
                                 gint                 *priority_out)
{
	PrioritySegment *segment;
	GList *node;

	g_return_val_if_fail (queue != NULL, NULL);

	node = g_queue_peek_head_link (&queue->queue);

	if (!node) {
		/* No elements in queue */
		return NULL;
	}

	segment = &g_array_index (queue->segments, PrioritySegment, 0);
	g_assert (segment->first_elem == node);

	if (priority_out) {
		*priority_out = segment->priority;
	}

	if (segment->last_elem == node) {
		/* It is the last element of the first
		 * segment, remove segment as well.
		 */
		g_array_remove_index (queue->segments, 0);
	} else {

		/* Move segments first element forward */
		segment->first_elem = segment->first_elem->next;
	}

	return g_queue_pop_head_link (&queue->queue);
}

GList *
tracker_priority_queue_get_head (TrackerPriorityQueue *queue)
{
	g_return_val_if_fail (queue != NULL, NULL);

	return queue->queue.head;
}
