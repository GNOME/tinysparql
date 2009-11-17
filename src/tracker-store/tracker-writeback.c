/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008, Nokia
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

#include <libtracker-common/tracker-ontology.h>

#include "tracker-writeback.h"

typedef struct {
	GHashTable *allowances;
	GHashTable *events;
} WritebackPrivate;

static GStaticPrivate private_key = G_STATIC_PRIVATE_INIT;

static void 
tracker_writeback_add_allow (const gchar *predicate)
{
	WritebackPrivate *private;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	g_hash_table_insert (private->allowances, g_strdup (predicate),
	                     GINT_TO_POINTER (TRUE));
}

static gboolean
is_allowed (WritebackPrivate *private, const gchar *rdf_predicate)
{
	return (g_hash_table_lookup (private->allowances, rdf_predicate) != NULL) ? TRUE : FALSE;
}


static GStrv
copy_rdf_types (GPtrArray *rdf_types)
{
	GStrv new_types = g_new0 (gchar*, rdf_types->len + 1);
	guint n;

	for (n = 0; n < rdf_types->len; n++) {
		new_types[n] = g_strdup (rdf_types->pdata[n]);
	}

	return new_types;
}


void 
tracker_writeback_check (const gchar *graph,
                         const gchar *subject, 
                         const gchar *predicate,
                         const gchar *object,
                         GPtrArray *rdf_types)
{
	WritebackPrivate *private;

	/* When graph is NULL, the graph is the default one. We only do 
	 * writeback reporting in the default graph (update queries that
	 * aren't coming from the miner) */

	if (graph != NULL) {
		return;
	}

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	if (is_allowed (private, predicate)) {

		if (!private->events) {
			private->events = g_hash_table_new_full (g_str_hash, g_str_equal,
			                                         (GDestroyNotify) g_free,
			                                         (GDestroyNotify) g_strfreev);
		}

		g_hash_table_insert (private->events, g_strdup (subject),
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

	return private->events ? g_hash_table_ref (private->events) : NULL;
}

static void
free_private (WritebackPrivate *private)
{
	g_hash_table_unref (private->allowances);
	g_free (private);
}

void 
tracker_writeback_init (TrackerWritebackPredicateGetter callback)
{
	WritebackPrivate *private;
	GStrv          predicates_to_signal;
	gint           i, count;

	private = g_new0 (WritebackPrivate, 1);

	g_static_private_set (&private_key,
			      private,
			      (GDestroyNotify) free_private);

	private->allowances = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                             (GDestroyNotify) g_free,
	                                             (GDestroyNotify) NULL);

	private->events = NULL;

	if (!callback) {
		return;
	}

	predicates_to_signal = (*callback)();

	if (!predicates_to_signal)
		return;

	count = g_strv_length (predicates_to_signal);
	for (i = 0; i < count; i++) {
		tracker_writeback_add_allow (predicates_to_signal[i]);
	}

	g_strfreev (predicates_to_signal);
}

void 
tracker_writeback_shutdown (void)
{
	WritebackPrivate *private;

	private = g_static_private_get (&private_key);
	if (private != NULL) {
		tracker_writeback_reset ();
		g_static_private_set (&private_key, NULL, NULL);
	} else {
		g_warning ("tracker_writeback already shutdown");
	}
}
