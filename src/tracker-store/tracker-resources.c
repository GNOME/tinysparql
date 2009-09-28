/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
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
 */

#include "config.h"

#include <string.h>
#include <gio/gio.h>

#include <libtracker-common/tracker-dbus.h>
#include <libtracker-common/tracker-log.h>
#include <libtracker-common/tracker-utils.h>
#include <libtracker-common/tracker-type-utils.h>
#include <libtracker-common/tracker-ontology.h>

#include <libtracker-db/tracker-db-dbus.h>

#include <libtracker-data/tracker-data-manager.h>
#include <libtracker-data/tracker-data-query.h>
#include <libtracker-data/tracker-data-update.h>

#include "tracker-dbus.h"
#include "tracker-marshal.h"
#include "tracker-resources.h"
#include "tracker-resource-class.h"
#include "tracker-events.h"
#include "tracker-store.h"

#define RDF_PREFIX TRACKER_RDF_PREFIX
#define RDF_TYPE RDF_PREFIX "type"


G_DEFINE_TYPE(TrackerResources, tracker_resources, G_TYPE_OBJECT)

#define TRACKER_RESOURCES_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_RESOURCES, TrackerResourcesPrivate))


typedef struct {
	GSList     *event_sources;
} TrackerResourcesPrivate;

typedef struct {
	DBusGMethodInvocation *context;
	guint request_id;
} TrackerDBusMethodInfo;

static void
free_event_sources (TrackerResourcesPrivate *priv)
{
	if (priv->event_sources) {
		g_slist_foreach (priv->event_sources, 
				 (GFunc) g_object_unref, NULL);
		g_slist_free (priv->event_sources);

		priv->event_sources = NULL;
	}
}

static void 
tracker_resources_finalize (GObject	 *object)
{
	TrackerResourcesPrivate *priv;

	priv = TRACKER_RESOURCES_GET_PRIVATE (object);

	free_event_sources (priv);

	G_OBJECT_CLASS (tracker_resources_parent_class)->finalize (object);
}

static void
tracker_resources_class_init (TrackerResourcesClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_resources_finalize;

	g_type_class_add_private (object_class, sizeof (TrackerResourcesPrivate));
}


static void
tracker_resources_init (TrackerResources *object)
{
}

TrackerResources *
tracker_resources_new (void)
{
	return g_object_new (TRACKER_TYPE_RESOURCES, NULL);
}

/*
 * Functions
 */

static void
destroy_method_info (gpointer user_data)
{
	g_slice_free (TrackerDBusMethodInfo, user_data);
}

static void
turtle_import_callback (GError *error, gpointer user_data)
{
	TrackerDBusMethodInfo *info = user_data;

	if (error) {
		tracker_dbus_request_failed (info->request_id,
					     &error,
					     NULL);
		dbus_g_method_return_error (info->context, error);
		return;
	}

	dbus_g_method_return (info->context);

	tracker_dbus_request_success (info->request_id);
}

void
tracker_resources_load (TrackerResources	 *object,
			const gchar		 *uri,
			DBusGMethodInvocation	 *context,
			GError			**error)
{
	TrackerDBusMethodInfo   *info;
	guint		    request_id;
	GFile  *file;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (uri != NULL, context);

	tracker_dbus_request_new (request_id,
				  "D-Bus request to load turtle file "
				  "'%s'",
				  uri);

	file = g_file_new_for_uri (uri);

	info = g_slice_new (TrackerDBusMethodInfo);

	info->request_id = request_id;
	info->context = context;

	tracker_store_queue_turtle_import (file, turtle_import_callback,
	                                   info, destroy_method_info);

	g_object_unref (file);
}

void
tracker_resources_sparql_query (TrackerResources	 *self,
				const gchar	         *query,
				DBusGMethodInvocation	 *context,
				GError			**error)
{
	TrackerDBResultSet   *result_set;
	GError 		     *actual_error = NULL;
	guint		      request_id;
	GPtrArray            *values;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (query != NULL, context);

	tracker_dbus_request_new (request_id,
				  "D-Bus request for SPARQL Query, "
				  "query:'%s'",
				  query);

	result_set = tracker_store_sparql_query (query, &actual_error);

	if (actual_error) {
		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     NULL);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	}

	values = tracker_dbus_query_result_to_ptr_array (result_set);

	dbus_g_method_return (context, values);

	tracker_dbus_results_ptr_array_free (&values);

	if (result_set) {
		g_object_unref (result_set);
	}

	tracker_dbus_request_success (request_id);
}

void
tracker_resources_sparql_update (TrackerResources	 *self,
				 const gchar	         *update,
				 DBusGMethodInvocation	 *context,
				 GError			**error)
{
	TrackerResourcesPrivate *priv;
	GError 		     *actual_error = NULL;
	guint		      request_id;

	priv = TRACKER_RESOURCES_GET_PRIVATE (self);

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (update != NULL, context);

	tracker_dbus_request_new (request_id,
				  "D-Bus request for SPARQL Update, "
				  "update:'%s'",
				  update);

	tracker_store_sparql_update (update, &actual_error);

	if (actual_error) {
		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     NULL);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	}

	dbus_g_method_return (context);

	tracker_dbus_request_success (request_id);
}

static void
batch_update_callback (GError *error, gpointer user_data)
{
	TrackerDBusMethodInfo *info = user_data;

	if (error) {
		tracker_dbus_request_failed (info->request_id,
					     &error,
					     NULL);
		dbus_g_method_return_error (info->context, error);
		return;
	}

	dbus_g_method_return (info->context);

	tracker_dbus_request_success (info->request_id);
}

void
tracker_resources_batch_sparql_update (TrackerResources          *self,
				       const gchar	         *update,
				       DBusGMethodInvocation	 *context,
				       GError			**error)
{
	TrackerDBusMethodInfo   *info;
	TrackerResourcesPrivate *priv;
	guint		      request_id;
	gchar		      *sender;

	priv = TRACKER_RESOURCES_GET_PRIVATE (self);

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (update != NULL, context);

	tracker_dbus_request_new (request_id,
				  "D-Bus request for batch SPARQL Update, "
				  "update:'%s'",
				  update);

	info = g_slice_new (TrackerDBusMethodInfo);

	info->request_id = request_id;
	info->context = context;

	sender = dbus_g_method_get_sender (context);

	tracker_store_queue_sparql_update (update, batch_update_callback,
	                                   sender, info, destroy_method_info);

	g_free (sender);
}

static void
batch_commit_callback (gpointer user_data)
{
	TrackerDBusMethodInfo *info = user_data;

	dbus_g_method_return (info->context);
	tracker_dbus_request_success (info->request_id);
}

void
tracker_resources_batch_commit (TrackerResources	 *self,
				DBusGMethodInvocation	 *context,
				GError			**error)
{
	TrackerDBusMethodInfo *info;
	TrackerResourcesPrivate *priv;
	guint		      request_id;
	gchar		      *sender;

	priv = TRACKER_RESOURCES_GET_PRIVATE (self);

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_request_new (request_id,
				  "D-Bus request for batch commit");

	info = g_slice_new (TrackerDBusMethodInfo);

	info->request_id = request_id;
	info->context = context;

	sender = dbus_g_method_get_sender (context);

	tracker_store_queue_commit (batch_commit_callback, sender, info,
	                            destroy_method_info);

	g_free (sender);
}


static void
on_statements_committed (gpointer user_data)
{
	GPtrArray *events;
	TrackerResourcesPrivate *priv;

	priv = TRACKER_RESOURCES_GET_PRIVATE (user_data);

	/* For more information about this call, look at the function end_batch
	 * of tracker-store.c */
	tracker_store_flush_journal ();

	events = tracker_events_get_pending ();

	if (events) {
		GSList *event_sources, *l, *to_emit = NULL;
		guint i;

		event_sources =priv->event_sources;

		for (i = 0; i < events->len; i++) {
			GValueArray *event = events->pdata[i];
			const gchar *uri = g_value_get_string (g_value_array_get_nth (event, 0));
			const gchar *predicate = g_value_get_string (g_value_array_get_nth (event, 1));
			const gchar *rdf_class = g_value_get_string (g_value_array_get_nth (event, 2));
			TrackerDBusEventsType type = g_value_get_int (g_value_array_get_nth (event, 3));

			for (l = event_sources; l; l = l->next) {
				TrackerResourceClass *class_ = l->data;
				if (g_strcmp0 (rdf_class, tracker_resource_class_get_rdf_class (class_)) == 0) {
					tracker_resource_class_add_event (class_, uri, predicate, type);
					to_emit = g_slist_prepend (to_emit, class_);
				}
			}
		}

		if (to_emit) {
			for (l = to_emit; l; l = l->next) {
				TrackerResourceClass *class_ = l->data;
				tracker_resource_class_emit_events (class_);
			}

			g_slist_free (to_emit);
		}
	}

	tracker_events_reset ();
}

static void
on_statement_inserted (const gchar *subject,
		       const gchar *predicate,
		       const gchar *object,
		       GPtrArray   *rdf_types,
		       gpointer user_data)
{
	if (g_strcmp0 (predicate, RDF_PREFIX "type") == 0) {
		tracker_events_insert (subject, predicate, object, rdf_types, TRACKER_DBUS_EVENTS_TYPE_ADD);
	} else {
		tracker_events_insert (subject, predicate, object, rdf_types, TRACKER_DBUS_EVENTS_TYPE_UPDATE);
	}
}

static void
on_statement_deleted (const gchar *subject,
		      const gchar *predicate,
		      const gchar *object,
		      GPtrArray   *rdf_types,
		      gpointer user_data)
{
	if (g_strcmp0 (predicate, RDF_PREFIX "type") == 0) {
		tracker_events_insert (subject, predicate, object, rdf_types, TRACKER_DBUS_EVENTS_TYPE_DELETE);
	} else {
		tracker_events_insert (subject, predicate, object, rdf_types, TRACKER_DBUS_EVENTS_TYPE_UPDATE);
	}
}


void 
tracker_resources_prepare (TrackerResources *object,
			   GSList           *event_sources)
{
	TrackerResourcesPrivate *priv;

	priv = TRACKER_RESOURCES_GET_PRIVATE (object);

	free_event_sources (priv);

	tracker_data_set_insert_statement_callback (on_statement_inserted, object);
	tracker_data_set_delete_statement_callback (on_statement_deleted, object);
	tracker_data_set_commit_statement_callback (on_statements_committed, object);

	priv->event_sources = event_sources;
}

void
tracker_resources_unreg_batches (TrackerResources *object,
                                 const gchar      *old_owner)
{
	tracker_store_unreg_batches (old_owner);
}
