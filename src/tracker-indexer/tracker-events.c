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

#include "tracker-events.h"


typedef struct {
	GPtrArray *allowances;
	GPtrArray *events;
} EventsPrivate;

static GStaticPrivate private_key = G_STATIC_PRIVATE_INIT;

static void 
tracker_events_add_allow (const gchar *rdf_class)
{
	EventsPrivate *private;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	g_ptr_array_add (private->allowances, g_strdup (rdf_class));
}

static gboolean
is_allowed (EventsPrivate *private, const gchar *rdf_class)
{
	guint i;
	gboolean found = FALSE;

	for (i = 0; i < private->allowances->len;  i++) {
		if (g_strcmp0 (rdf_class, private->allowances->pdata[i]) == 0) {
			found = TRUE;
			break;
		}
	}

	return found;
}

typedef struct {
	const gchar *uri;
	TrackerDBusEventsType type;
} PreparableEvent;

static void 
prepare_event_for_rdf_types (gpointer data, gpointer user_data)
{
	const gchar *rdf_class = data;
	PreparableEvent *info = user_data;
	const gchar *uri = info->uri;
	TrackerDBusEventsType type = info->type;

	EventsPrivate *private;
	GValueArray *event;
	GValue uri_value = { 0 , };
	GValue rdfclass_value = { 0 , };
	GValue type_value = { 0 , };

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	if (!is_allowed (private, rdf_class))
		return;

	if (!private->events) {
		private->events = g_ptr_array_new ();
	}

	g_value_init (&uri_value, G_TYPE_STRING);
	g_value_init (&rdfclass_value, G_TYPE_STRING);
	g_value_init (&type_value, G_TYPE_INT);

	event = g_value_array_new (3);

	g_value_set_string (&uri_value, uri);
	g_value_set_string (&rdfclass_value, rdf_class);
	g_value_set_int (&type_value, type);

	g_value_array_append (event, &uri_value);
	g_value_array_append (event, &rdfclass_value);
	g_value_array_append (event, &type_value);

	g_ptr_array_add (private->events, event);

	g_value_unset (&uri_value);
	g_value_unset (&rdfclass_value);
	g_value_unset (&type_value);
}

void 
tracker_events_insert (const gchar *uri, 
		       const gchar *object, 
		       GPtrArray *rdf_types, 
		       TrackerDBusEventsType type)
{
	PreparableEvent info;

	info.uri = uri;
	info.type = type;

	if (rdf_types && type == TRACKER_DBUS_EVENTS_TYPE_UPDATE) {
		/* object is not very important for updates (we don't expose
		 * the value being set to the user's DBus API in trackerd) */
		g_ptr_array_foreach (rdf_types, prepare_event_for_rdf_types, &info);
	} else if (type == TRACKER_DBUS_EVENTS_TYPE_UPDATE) {
		/* In this case we had an INSERT for a resource that didn't exist 
		 * yet, but it was not the rdf:type predicate being inserted */
		prepare_event_for_rdf_types ((gpointer) TRACKER_RDFS_PREFIX "Resource", &info);
	} else {
		/* In case of delete and create, object is the rdf:type */
		prepare_event_for_rdf_types ((gpointer) object, &info);
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
	g_ptr_array_foreach (private->allowances, (GFunc)g_free, NULL);
	g_ptr_array_free (private->allowances, TRUE);
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

	private->allowances = g_ptr_array_new ();
	private->events = NULL;

	if (!callback) {
		return;
	}

	classes_to_signal = (*callback)();
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
		/* Shutdown with pending events = ERROR */
		g_return_if_fail (private->events == NULL);
		g_static_private_set (&private_key, NULL, NULL);
	} else {
		g_warning ("tracker_events already shutdown");
	}
}
