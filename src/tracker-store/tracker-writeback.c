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
 * Authors:
 *  Philip Van Hoof <philip@codeminded.be>
 */

#include "config.h"

#include <libtracker-data/tracker-data.h>

#include "tracker-writeback.h"

typedef struct {
	GHashTable *allowances;
	GHashTable *events;
} WritebackPrivate;

static WritebackPrivate *private;

static GArray*
rdf_types_to_array (GPtrArray *rdf_types)
{
	GArray *new_types;
	guint n;

	new_types =  g_array_sized_new (FALSE, FALSE, sizeof (gint), rdf_types->len);

	for (n = 0; n < rdf_types->len; n++) {
		gint id = tracker_class_get_id (rdf_types->pdata[n]);
		g_array_append_val (new_types, id);
	}

	return new_types;
}

static void
array_free (GArray *array)
{
	g_array_free (array, TRUE);
}

void
tracker_writeback_check (gint         graph_id,
                         gint         subject_id,
                         const gchar *subject,
                         gint         pred_id,
                         gint         object_id,
                         const gchar *object,
                         GPtrArray   *rdf_types)
{
	/* When graph is NULL, the graph is the default one. We only do
	 * writeback reporting in the default graph (update queries that
	 * aren't coming from the miner)
	 */

	if (graph_id != 0) {
		/* g_debug ("Not doing writeback check, no graph"); */
		return;
	}

	g_return_if_fail (private != NULL);

	if (g_hash_table_lookup (private->allowances, GINT_TO_POINTER (pred_id))) {
		if (!private->events) {
			private->events = g_hash_table_new_full (g_direct_hash, g_direct_equal,
			                                         (GDestroyNotify) NULL,
			                                         (GDestroyNotify) array_free);
		}

		g_hash_table_insert (private->events,
		                     GINT_TO_POINTER (subject_id),
		                     rdf_types_to_array (rdf_types));
	}
}

void
tracker_writeback_reset (void)
{
	g_return_if_fail (private != NULL);

	if (private->events) {
		g_hash_table_unref (private->events);
		private->events = NULL;
	}
}

GHashTable *
tracker_writeback_get_pending (void)
{
	g_return_val_if_fail (private != NULL, NULL);

	return private->events;
}

static void
free_private (gpointer user_data)
{
	WritebackPrivate *private;

	private = user_data;
	g_hash_table_unref (private->allowances);
	g_free (private);
}

void
tracker_writeback_init (TrackerWritebackGetPredicatesFunc func)
{
	GStrv predicates_to_signal;
	gint i, count;

	g_return_if_fail (private == NULL);

	private = g_new0 (WritebackPrivate, 1);

	private->allowances = g_hash_table_new_full (g_direct_hash,
	                                             g_direct_equal,
	                                             NULL,
	                                             NULL);

	g_message ("Setting up predicates for writeback notification...");

	if (!func) {
		g_message ("  No predicates set, no TrackerWritebackGetPredicatesFunc");
		return;
	}

	predicates_to_signal = (*func)();

	if (!predicates_to_signal) {
		g_message ("  No predicates set, none are configured in ontology");
		return;
	}

	count = g_strv_length (predicates_to_signal);

	for (i = 0; i < count; i++) {
		TrackerProperty *predicate = tracker_ontologies_get_property_by_uri (predicates_to_signal[i]);
		if (predicate) {
			gint id = tracker_property_get_id (predicate);
			g_message ("  Adding:'%s'", predicates_to_signal[i]);
			g_hash_table_insert (private->allowances,
			                     GINT_TO_POINTER (id),
			                     GINT_TO_POINTER (TRUE));
		}
	}

	g_strfreev (predicates_to_signal);
}

void
tracker_writeback_shutdown (void)
{
	g_return_if_fail (private != NULL);

	tracker_writeback_reset ();
	free_private (private);
	private = NULL;
}
