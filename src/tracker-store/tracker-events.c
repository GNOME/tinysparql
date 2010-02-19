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

#include "tracker-events.h"


typedef struct {
	GHashTable *allowances;
	GPtrArray *events;
} EventsPrivate;

static GStaticPrivate private_key = G_STATIC_PRIVATE_INIT;

static void
tracker_events_add_allow (const gchar *rdf_class)
{
	EventsPrivate *private;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	g_hash_table_insert (private->allowances, g_strdup (rdf_class),
	                     GINT_TO_POINTER (TRUE));
}

static gboolean
is_allowed (EventsPrivate *private, const gchar *rdf_class)
{
	return (g_hash_table_lookup (private->allowances, rdf_class) != NULL) ? TRUE : FALSE;
}

static void
prepare_event_for_rdf_type (EventsPrivate *private,
                            const gchar *rdf_class ,
                            const gchar *uri,
                            TrackerDBusEventsType type,
                            const gchar *predicate)
{
	GValueArray *event;
	GValue uri_value = { 0 , };
	GValue rdfclass_value = { 0 , };
	GValue type_value = { 0 , };
	GValue predicate_value = { 0 , };

	if (!private->events) {
		private->events = g_ptr_array_new ();
	}

	g_value_init (&uri_value, G_TYPE_STRING);
	g_value_init (&predicate_value, G_TYPE_STRING);
	g_value_init (&rdfclass_value, G_TYPE_STRING);
	g_value_init (&type_value, G_TYPE_INT);

	event = g_value_array_new (4);

	g_value_set_string (&uri_value, uri);
	g_value_set_string (&predicate_value, predicate);
	g_value_set_string (&rdfclass_value, rdf_class);
	g_value_set_int (&type_value, type);

	g_value_array_append (event, &uri_value);
	g_value_array_append (event, &predicate_value);
	g_value_array_append (event, &rdfclass_value);
	g_value_array_append (event, &type_value);

	g_ptr_array_add (private->events, event);

	g_value_unset (&uri_value);
	g_value_unset (&rdfclass_value);
	g_value_unset (&type_value);
	g_value_unset (&predicate_value);
}

void
tracker_events_insert (const gchar *uri,
                       const gchar *predicate,
                       const gchar *object,
                       GPtrArray *rdf_types,
                       TrackerDBusEventsType type)
{
	EventsPrivate *private;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

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
	} else if (type == TRACKER_DBUS_EVENTS_TYPE_UPDATE) {
		/* In this case we had an INSERT for a resource that didn't exist
		 * yet, but it was not the rdf:type predicate being inserted */
		if (is_allowed (private, (gpointer) TRACKER_RDFS_PREFIX "Resource")) {
			prepare_event_for_rdf_type (private,
			                            (gpointer) TRACKER_RDFS_PREFIX "Resource",
			                            uri, type, predicate);
		}
	} else {
		/* In case of delete and create, object is the rdf:type */
		if (is_allowed (private, (gpointer) object)) {
			prepare_event_for_rdf_type (private, (gpointer) object,
			                            uri, type, predicate);
		}
	}
}

void
tracker_events_reset (void)
{
	EventsPrivate *private;
	guint i;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	if (private->events) {
		for (i = 0; i < private->events->len; i++) {
			g_value_array_free (private->events->pdata[i]);
		}
		g_ptr_array_free (private->events, TRUE);

		private->events = NULL;
	}
}

GPtrArray *
tracker_events_get_pending (void)
{
	EventsPrivate *private;

	private = g_static_private_get (&private_key);
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
	EventsPrivate *private;
	GStrv          classes_to_signal;
	gint           i, count;

	private = g_new0 (EventsPrivate, 1);

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
	EventsPrivate *private;

	private = g_static_private_get (&private_key);
	if (private != NULL) {
		tracker_events_reset ();
		g_static_private_set (&private_key, NULL, NULL);
	} else {
		g_warning ("tracker_events already shutdown");
	}
}
