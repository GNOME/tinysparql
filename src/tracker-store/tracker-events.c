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

#define MAX_EVENTS 1000000

typedef struct {
	GHashTable *allowances;
	GArray *events;
	GStringChunk *chunk;
	gboolean frozen;
} EventsPrivate;

static EventsPrivate *private;

static void
tracker_events_add_allow (const gchar *rdf_class)
{
	TrackerClass *cl;

	g_return_if_fail (private != NULL);

	cl = tracker_ontologies_get_class_by_uri (rdf_class);
	if (!cl) {
		g_critical ("Unknown class %s", rdf_class);
		return;
	}

	g_hash_table_insert (private->allowances, cl,
	                     GINT_TO_POINTER (TRUE));
}

static gboolean
is_allowed (EventsPrivate *private, TrackerClass *rdf_class)
{
	return (g_hash_table_lookup (private->allowances, rdf_class) != NULL) ? TRUE : FALSE;
}

static void
prepare_event_for_rdf_type (EventsPrivate *private,
                            TrackerClass  *rdf_class ,
                            const gchar *uri,
                            TrackerDBusEventsType type,
                            const gchar *predicate)
{
	TrackerEvent event;

	if (!private->events) {
		private->events = g_array_new (TRUE, FALSE, sizeof (TrackerEvent));
	}

	if (!private->chunk) {
		private->chunk = g_string_chunk_new (4096);
	}

	event.type = type;
	event.class = rdf_class;
	event.predicate = tracker_ontologies_get_property_by_uri (predicate);
	event.subject = g_string_chunk_insert_const (private->chunk, uri);

	g_array_append_val (private->events, event);
}

void
tracker_events_insert (const gchar *uri,
                       const gchar *predicate,
                       const gchar *object,
                       GPtrArray *rdf_types,
                       TrackerDBusEventsType type)
{
	g_return_if_fail (rdf_types || type != TRACKER_DBUS_EVENTS_TYPE_UPDATE);
	g_return_if_fail (private != NULL);

	if (private->frozen) {
		return;
	}

	if (private->events && private->events->len > MAX_EVENTS) {
		/* too many events would lead to high memory usage and to dbus signal messages
		   larger than maximum dbus message size, which results in dbus disconnect and tracker-store crash */
		g_warning ("Too many events pending, disabling events for current transaction");
		tracker_events_reset ();
		private->frozen = TRUE;
	}

	if (rdf_types && type == TRACKER_DBUS_EVENTS_TYPE_UPDATE) {
		guint i;

		for (i = 0; i < rdf_types->len; i++) {

			/* object is not very important for updates (we don't expose
			 * the value being set to the user's DBus API in tracker-store) */
			if (is_allowed (private, rdf_types->pdata[i])) {

				prepare_event_for_rdf_type (private, rdf_types->pdata[i],
				                            uri, type, predicate);
			}
		}
	} else {
		TrackerClass *class = tracker_ontologies_get_class_by_uri (object);
		/* In case of delete and create, object is the rdf:type */
		if (is_allowed (private, class)) {
			prepare_event_for_rdf_type (private, class,
			                            uri, type, predicate);
		}
	}
}

void
tracker_events_reset (void)
{
	g_return_if_fail (private != NULL);

	if (private->events) {
		g_array_free (private->events, TRUE);
		g_string_chunk_free (private->chunk);
		private->chunk = NULL;
		private->events = NULL;
	}
	private->frozen = FALSE;
}

void
tracker_events_freeze (void)
{
	g_return_if_fail (private != NULL);

	private->frozen = TRUE;
}

GArray *
tracker_events_get_pending (void)
{
	g_return_val_if_fail (private != NULL, NULL);

	return private->events;
}

static void
free_private (EventsPrivate *private)
{
	g_hash_table_unref (private->allowances);
	g_free (private);
}

void
tracker_events_init (TrackerNotifyClassGetter callback)
{
	GStrv          classes_to_signal;
	gint           i, count;

	private = g_new0 (EventsPrivate, 1);

	private->allowances = g_hash_table_new (g_direct_hash, g_direct_equal);

	private->events = NULL;

	if (!callback) {
		return;
	}

	classes_to_signal = (*callback)();

	if (!classes_to_signal)
		return;

	count = g_strv_length (classes_to_signal);
	for (i = 0; i < count; i++) {
		tracker_events_add_allow (classes_to_signal[i]);
	}

	g_strfreev (classes_to_signal);
}

void
tracker_events_shutdown (void)
{
	if (private != NULL) {
		tracker_events_reset ();
		free_private (private);
		private = NULL;
	} else {
		g_warning ("tracker_events already shutdown");
	}
}
