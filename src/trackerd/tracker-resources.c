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

#include <libtracker-common/tracker-dbus.h>
#include <libtracker-common/tracker-log.h>
#include <libtracker-common/tracker-utils.h>
#include <libtracker-common/tracker-type-utils.h>

#include <libtracker-db/tracker-db-dbus.h>

#include <libtracker-data/tracker-data-manager.h>
#include <libtracker-data/tracker-data-query.h>
#include <libtracker-data/tracker-data-update.h>

#include "tracker-indexer-client.h"
#include "tracker-dbus.h"
#include "tracker-marshal.h"
#include "tracker-resources.h"
#include "tracker-resource-class.h"

G_DEFINE_TYPE(TrackerResources, tracker_resources, G_TYPE_OBJECT)

#define TRACKER_RESOURCES_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_RESOURCES, TrackerResourcesPrivate))


typedef struct {
	GSList     *event_sources;
	DBusGProxy *indexer_proxy;
} TrackerResourcesPrivate;

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

	g_object_unref (priv->indexer_proxy);

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
event_happened_cb (DBusGProxy *proxy,
		   GPtrArray  *events,
		   gpointer    user_data)
{
	TrackerResources        *object = user_data;
	TrackerResourcesPrivate *priv;
	GSList                  *event_sources, *l, *to_emit = NULL;
	guint                    i;

	priv = TRACKER_RESOURCES_GET_PRIVATE (object);

	event_sources = priv->event_sources;

	for (i = 0; i < events->len; i++) {
		GValueArray *event = events->pdata[i];
		const gchar *uri = g_value_get_string (g_value_array_get_nth (event, 0));
		const gchar *rdf_class = g_value_get_string (g_value_array_get_nth (event, 1));
		TrackerDBusEventsType type = g_value_get_int (g_value_array_get_nth (event, 2));

		for (l = event_sources; l; l = l->next) {
			TrackerResourceClass *class_ = l->data;
			if (g_strcmp0 (rdf_class, tracker_resource_class_get_rdf_class (class_)) == 0) {
				tracker_resource_class_add_event (class_, uri, type);
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

static void
tracker_resources_init (TrackerResources *object)
{
	TrackerResourcesPrivate *priv;
	DBusGProxy *proxy = tracker_dbus_indexer_get_proxy ();

	priv = TRACKER_RESOURCES_GET_PRIVATE (object);

	priv->indexer_proxy = g_object_ref (proxy);

	dbus_g_proxy_connect_signal (proxy, "EventHappened",
				     G_CALLBACK (event_happened_cb),
				     object,
				     NULL);

}

TrackerResources *
tracker_resources_new (void)
{
	return g_object_new (TRACKER_TYPE_RESOURCES, NULL);
}

/*
 * Functions
 */

void
tracker_resources_insert (TrackerResources	     *self,
			  const gchar                *subject,
			  const gchar                *predicate,
			  const gchar                *object,
			  DBusGMethodInvocation      *context,
			  GError		    **error)
{
	guint		    request_id;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (subject != NULL, context);
	tracker_dbus_async_return_if_fail (predicate != NULL, context);
	tracker_dbus_async_return_if_fail (object != NULL, context);

	tracker_dbus_request_new (request_id,
				  "DBus request to insert statement: "
				  "'%s' '%s' '%s'",
				  subject, predicate, object);

	tracker_data_insert_statement (subject, predicate, object);

	dbus_g_method_return (context);

	tracker_dbus_request_success (request_id);
}

void
tracker_resources_delete (TrackerResources	     *self,
			  const gchar                *subject,
			  const gchar                *predicate,
			  const gchar                *object,
			  DBusGMethodInvocation      *context,
			  GError		    **error)
{
	guint		    request_id;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (subject != NULL, context);
	tracker_dbus_async_return_if_fail (predicate != NULL, context);
	tracker_dbus_async_return_if_fail (object != NULL, context);

	tracker_dbus_request_new (request_id,
				  "DBus request to delete statement: "
				  "'%s' '%s' '%s'",
				  subject, predicate, object);

	tracker_data_delete_statement (subject, predicate, object);

	dbus_g_method_return (context);

	tracker_dbus_request_success (request_id);
}

void
tracker_resources_load (TrackerResources	 *object,
			const gchar		 *uri,
			DBusGMethodInvocation	 *context,
			GError			**error)
{
	guint		    request_id;
	GFile  *file;
	gchar  *path;
	GError		   *actual_error = NULL;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (uri != NULL, context);

	tracker_dbus_request_new (request_id,
				  "DBus request to load turtle file "
				  "'%s'",
				  uri);

	file = g_file_new_for_uri (uri);
	path = g_file_get_path (file);

	org_freedesktop_Tracker_Indexer_turtle_add (tracker_dbus_indexer_get_proxy (),
						    path,
						    &actual_error);

	g_free (path);
	g_object_unref (file);

	if (actual_error) {
		tracker_dbus_request_failed (request_id, &actual_error, NULL);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	}

	dbus_g_method_return (context);

	tracker_dbus_request_success (request_id);
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
				  "DBus request for SPARQL Query, "
				  "query:'%s'",
				  query);

	result_set = tracker_data_query_sparql (query, &actual_error);

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
	GError 		     *actual_error = NULL;
	guint		      request_id;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (update != NULL, context);

	tracker_dbus_request_new (request_id,
				  "DBus request for SPARQL Update, "
				  "update:'%s'",
				  update);

	tracker_data_update_sparql (update, &actual_error);

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

void 
tracker_resources_set_event_sources (TrackerResources *object,
				     GSList           *event_sources)
{
	TrackerResourcesPrivate *priv;

	priv = TRACKER_RESOURCES_GET_PRIVATE (object);

	free_event_sources (priv);

	priv->event_sources = event_sources;
}
