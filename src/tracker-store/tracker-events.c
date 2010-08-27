/*
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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
 * Authors:
 *  Philip Van Hoof <philip@codeminded.be>
 */

#include "config.h"

#include <libtracker-data/tracker-data.h>

#include "tracker-events.h"

typedef struct {
	GHashTable *allowances_id;
	GHashTable *allowances;
	gboolean frozen;
	guint total;
} EventsPrivate;

static EventsPrivate *private;

guint
tracker_events_get_total (gboolean and_reset)
{
	guint total;

	g_return_val_if_fail (private != NULL, 0);

	total = private->total;

	if (and_reset) {
		private->total = 0;
	}

	return total;
}

static gboolean
is_allowed (EventsPrivate *private, TrackerClass *rdf_class, gint class_id)
{
	gboolean ret;

	if (rdf_class != NULL) {
		ret = (g_hash_table_lookup (private->allowances, rdf_class) != NULL) ? TRUE : FALSE;
	} else {
		ret = (g_hash_table_lookup (private->allowances_id, GINT_TO_POINTER (class_id)) != NULL) ? TRUE : FALSE;
	}
	return ret;
}

void
tracker_events_add_insert (gint         graph_id,
                           gint         subject_id,
                           const gchar *subject,
                           gint         pred_id,
                           gint         object_id,
                           const gchar *object,
                           GPtrArray   *rdf_types)
{
	TrackerProperty *rdf_type;

	g_return_if_fail (rdf_types != NULL);
	g_return_if_fail (private != NULL);

	if (private->frozen) {
		return;
	}

	rdf_type = tracker_ontologies_get_rdf_type ();

	if (object_id != 0 && pred_id == tracker_property_get_id (rdf_type)) {
		/* Resource create
		 * In case of create, object is the rdf:type */
		if (is_allowed (private, NULL, object_id)) {
			TrackerClass *class = NULL;

			if (rdf_types->len == 1 && tracker_class_get_id (rdf_types->pdata[0]) == object_id) {
				class = rdf_types->pdata[0];
			} else {
				if (object == NULL) {
					const gchar *uri = tracker_ontologies_get_uri_by_id (object_id);
					if (uri != NULL)
						class = tracker_ontologies_get_class_by_uri (uri);
				} else {
					class = tracker_ontologies_get_class_by_uri (object);
				}
			}

			if (class) {
				tracker_class_add_insert_event (class,
				                                subject_id,
				                                pred_id,
				                                object_id);
				private->total++;
			}
		}
	} else {
		guint i;

		for (i = 0; i < rdf_types->len; i++) {
			if (is_allowed (private, rdf_types->pdata[i], 0)) {
				tracker_class_add_insert_event (rdf_types->pdata[i],
				                                subject_id,
				                                pred_id,
				                                object_id);
				private->total++;
			}
		}
	}
}

void
tracker_events_add_delete (gint         graph_id,
                           gint         subject_id,
                           const gchar *subject,
                           gint         pred_id,
                           gint         object_id,
                           const gchar *object,
                           GPtrArray   *rdf_types)
{
	TrackerProperty *rdf_type;

	g_return_if_fail (rdf_types != NULL);
	g_return_if_fail (private != NULL);

	if (private->frozen) {
		return;
	}

	rdf_type = tracker_ontologies_get_rdf_type ();

	if (object_id != 0 && pred_id == tracker_property_get_id (rdf_type)) {
		/* Resource delete
		 * In case of delete, object is the rdf:type */
		if (is_allowed (private, NULL, object_id)) {
			TrackerClass *class = NULL;

			if (rdf_types->len == 1 && tracker_class_get_id (rdf_types->pdata[0]) == object_id) {
				class = rdf_types->pdata[0];
			} else {
				if (object == NULL) {
					const gchar *uri = tracker_ontologies_get_uri_by_id (object_id);
					if (uri != NULL)
						class = tracker_ontologies_get_class_by_uri (uri);
				} else {
					class = tracker_ontologies_get_class_by_uri (object);
				}
			}

			if (class) {
				tracker_class_add_delete_event (class,
				                                subject_id,
				                                pred_id,
				                                object_id);
				private->total++;
			}
		}
	} else {
		guint i;

		for (i = 0; i < rdf_types->len; i++) {
			if (is_allowed (private, rdf_types->pdata[i], 0)) {
				tracker_class_add_delete_event (rdf_types->pdata[i],
				                                subject_id,
				                                pred_id,
				                                object_id);
				private->total++;
			}
		}
	}

}

void
tracker_events_reset_pending (void)
{
	GHashTableIter iter;
	gpointer key, value;

	g_return_if_fail (private != NULL);

	g_hash_table_iter_init (&iter, private->allowances);

	while (g_hash_table_iter_next (&iter, &key, &value)) {
		TrackerClass *class = key;

		tracker_class_reset_pending_events (class);
	}

	private->frozen = FALSE;
}

void
tracker_events_freeze (void)
{
	g_return_if_fail (private != NULL);

	private->frozen = TRUE;
}

static void
free_private (EventsPrivate *private)
{
	GHashTableIter iter;
	gpointer key, value;

	g_hash_table_iter_init (&iter, private->allowances);

	while (g_hash_table_iter_next (&iter, &key, &value)) {
		TrackerClass *class = key;

		tracker_class_reset_pending_events (class);

		/* Perhaps hurry an emit of the ready events here? We're shutting down,
		 * so I guess we're not required to do that here ... ? */
		tracker_class_reset_ready_events (class);

	}

	g_hash_table_unref (private->allowances);
	g_hash_table_unref (private->allowances_id);

	g_free (private);
}

void
tracker_events_init (TrackerNotifyClassGetter callback)
{
	GStrv classes_to_signal;
	gint  i, count;

	if (!callback) {
		return;
	}

	private = g_new0 (EventsPrivate, 1);

	private->allowances = g_hash_table_new (g_direct_hash, g_direct_equal);
	private->allowances_id = g_hash_table_new (g_direct_hash, g_direct_equal);

	classes_to_signal = (*callback)();

	if (!classes_to_signal)
		return;

	count = g_strv_length (classes_to_signal);
	for (i = 0; i < count; i++) {
		TrackerClass *class = tracker_ontologies_get_class_by_uri (classes_to_signal[i]);
		if (class != NULL) {
			g_hash_table_insert (private->allowances,
			                     class,
			                     GINT_TO_POINTER (TRUE));
			g_hash_table_insert (private->allowances_id,
			                     GINT_TO_POINTER (tracker_class_get_id (class)),
			                     GINT_TO_POINTER (TRUE));
			g_debug ("ClassSignal allowance: %s has ID %d",
			         tracker_class_get_name (class),
			         tracker_class_get_id (class));
		}
	}
	g_strfreev (classes_to_signal);
}

void
tracker_events_classes_iter (GHashTableIter *iter)
{
	g_return_if_fail (private != NULL);

	g_hash_table_iter_init (iter, private->allowances);
}

void
tracker_events_shutdown (void)
{
	if (private != NULL) {
		free_private (private);
		private = NULL;
	} else {
		g_warning ("tracker_events already shutdown");
	}
}
