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

static GList *
priority_segment_alloc_node (TrackerPriorityQueue *queue,
                             gint                  priority)
{
	PrioritySegment *segment = NULL;
	gboolean found = FALSE;
	gint l, r, c;
	GList *node;

	/* Perform binary search to find out the segment for
	 * the given priority, create one if it isn't found.
	 */
	l = 0;
	r = queue->segments->len - 1;

	while (queue->segments->len > 0 && !found) {
		c = r + l / 2;
		segment = &g_array_index (queue->segments, PrioritySegment, c);

		if (segment->priority == priority) {
			found = TRUE;
			break;
		} else if (segment->priority > priority) {
			l = c + 1;
		} else if (segment->priority < priority) {
			r = c - 1;
		}

		if (l > r) {
			break;
		}
	}

	if (found) {
		/* Element found, append at the end of segment */
		g_assert (segment != NULL);
		g_assert (segment->priority == priority);

		g_queue_insert_after (&queue->queue, segment->last_elem, NULL);
		node = segment->last_elem = segment->last_elem->next;
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
				g_queue_insert_before (&queue->queue, segment->first_elem, NULL);
				node = segment->first_elem->prev;
			} else {
				/* We have to insert to the right of this element */
				g_queue_insert_after (&queue->queue, segment->last_elem, NULL);
				node = segment->last_elem->next;
				c++;
			}

			new_segment.first_elem = new_segment.last_elem = node;
			g_array_insert_val (queue->segments, c, new_segment);
		} else {
			/* Segments list has 0 elements */
			g_assert (queue->segments->len == 0);
			g_assert (g_queue_get_length (&queue->queue) == 0);

			node = g_list_alloc ();
			g_queue_push_head_link (&queue->queue, node);
			new_segment.first_elem = new_segment.last_elem = node;

			g_array_append_val (queue->segments, new_segment);
		}
	}

	return node;
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

void
tracker_priority_queue_foreach_remove (TrackerPriorityQueue *queue,
                                       GEqualFunc            compare_func,
                                       gpointer              compare_user_data,
                                       GDestroyNotify        destroy_notify)
{
	PrioritySegment *segment;
	gint n_segment = 0;
	GList *list;

	g_return_if_fail (queue != NULL);
	g_return_if_fail (compare_func != NULL);

	list = queue->queue.head;

	if (!list) {
		return;
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
}

gboolean
tracker_priority_queue_is_empty (TrackerPriorityQueue *queue)
{
	g_return_val_if_fail (queue != NULL, FALSE);

	return g_queue_is_empty (&queue->queue);
}

void
tracker_priority_queue_add (TrackerPriorityQueue *queue,
                            gpointer              data,
                            gint                  priority)
{
	GList *node;

	g_return_if_fail (queue != NULL);
	g_return_if_fail (data != NULL);

	/* clamp on G_PRIORITY_HIGH, to avoid much greediness */
	priority = MAX (priority, G_PRIORITY_HIGH);

	node = priority_segment_alloc_node (queue, priority);
	g_assert (node != NULL);

	node->data = data;
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

	if (priority_out) {
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

	return g_queue_pop_head (&queue->queue);
}
