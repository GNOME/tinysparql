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

static GStaticPrivate private_key = G_STATIC_PRIVATE_INIT;

static GStrv
copy_rdf_types (GPtrArray *rdf_types)
{
	GStrv new_types;
	guint n;

	new_types = g_new0 (gchar *, rdf_types->len + 1);

	for (n = 0; n < rdf_types->len; n++) {
		new_types[n] = g_strdup (tracker_class_get_uri (rdf_types->pdata[n]));
	}

	return new_types;
}

void
tracker_writeback_check (const gchar *graph,
                         const gchar *subject,
                         const gchar *predicate,
                         const gchar *object,
                         GPtrArray   *rdf_types)
{
	WritebackPrivate *private;

	/* When graph is NULL, the graph is the default one. We only do
	 * writeback reporting in the default graph (update queries that
	 * aren't coming from the miner)
	 */

	if (graph != NULL) {
		/* g_debug ("Not doing writeback check, no graph"); */
		return;
	}

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	if (g_hash_table_lookup (private->allowances, predicate)) {
		if (!private->events) {
			private->events = g_hash_table_new_full (g_str_hash, g_str_equal,
			                                         (GDestroyNotify) g_free,
			                                         (GDestroyNotify) g_strfreev);
		}

		g_hash_table_insert (private->events,
		                     g_strdup (subject),
		                     copy_rdf_types (rdf_types));
	}
}

void
tracker_writeback_reset (void)
{
	WritebackPrivate *private;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	if (private->events) {
		g_hash_table_unref (private->events);
		private->events = NULL;
	}
}

GHashTable *
tracker_writeback_get_pending (void)
{
	WritebackPrivate *private;

	private = g_static_private_get (&private_key);
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
	WritebackPrivate *private;
	GStrv predicates_to_signal;
	gint i, count;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private == NULL);

	private = g_new0 (WritebackPrivate, 1);

	g_static_private_set (&private_key, private, free_private);

	private->allowances = g_hash_table_new_full (g_str_hash,
	                                             g_str_equal,
	                                             (GDestroyNotify) g_free,
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
		g_message ("  Adding:'%s'", predicates_to_signal[i]);
		g_hash_table_insert (private->allowances,
		                     g_strdup (predicates_to_signal[i]),
		                     GINT_TO_POINTER (TRUE));
	}

	g_strfreev (predicates_to_signal);
}

void
tracker_writeback_shutdown (void)
{
	WritebackPrivate *private;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	tracker_writeback_reset ();
	g_static_private_set (&private_key, NULL, NULL);
}
