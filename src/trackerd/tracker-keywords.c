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

#include <libtracker-common/tracker-dbus.h>
#include <libtracker-common/tracker-log.h>
#include <libtracker-common/tracker-utils.h>

#include <libtracker-db/tracker-db-dbus.h>
#include <libtracker-db/tracker-db-index.h>
#include <libtracker-db/tracker-db-manager.h>

#include <libtracker-data/tracker-data-manager.h>
#include <libtracker-data/tracker-data-query.h>
#include <libtracker-data/tracker-data-schema.h>
#include <libtracker-data/tracker-data-search.h>

#include "tracker-dbus.h"
#include "tracker-keywords.h"
#include "tracker-marshal.h"
#include "tracker-indexer-client.h"

#define TRACKER_KEYWORDS_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_KEYWORDS, TrackerKeywordsPrivate))

typedef struct {
	DBusGProxy *fd_proxy;
} TrackerKeywordsPrivate;

enum {
	KEYWORD_ADDED,
	KEYWORD_REMOVED,
	LAST_SIGNAL
};

static void tracker_keywords_finalize (GObject	    *object);

static guint signals[LAST_SIGNAL] = {0};

G_DEFINE_TYPE(TrackerKeywords, tracker_keywords, G_TYPE_OBJECT)

static void
tracker_keywords_class_init (TrackerKeywordsClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_keywords_finalize;

	signals[KEYWORD_ADDED] =
		g_signal_new ("keyword-added",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      tracker_marshal_VOID__STRING_STRING_STRING,
			      G_TYPE_NONE,
			      3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
	signals[KEYWORD_REMOVED] =
		g_signal_new ("keyword-removed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      tracker_marshal_VOID__STRING_STRING_STRING,
			      G_TYPE_NONE,
			      3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

	g_type_class_add_private (object_class, sizeof (TrackerKeywordsPrivate));
}

static void
tracker_keywords_init (TrackerKeywords *object)
{
}

static void
tracker_keywords_finalize (GObject *object)
{
	TrackerKeywordsPrivate *priv;

	priv = TRACKER_KEYWORDS_GET_PRIVATE (object);

	if (priv->fd_proxy) {
		g_object_unref (priv->fd_proxy);
	}

	G_OBJECT_CLASS (tracker_keywords_parent_class)->finalize (object);
}

TrackerKeywords *
tracker_keywords_new (void)
{
	return g_object_new (TRACKER_TYPE_KEYWORDS, NULL);
}

/*
 * Functions
 */
void
tracker_keywords_get_list (TrackerKeywords  *object,
			   const gchar	    *service_type,
			   DBusGMethodInvocation *context,
			   GError	   **error)
{
	TrackerDBInterface *iface;
	TrackerDBResultSet *result_set;
	guint		    request_id;
	GError		   *actual_error = NULL;
	GPtrArray	   *values;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (service_type != NULL, context);

	tracker_dbus_request_new (request_id,
				  "DBus request to get keywords list, "
				  "service type:'%s'",
				  service_type);

	if (!tracker_ontology_service_is_valid (service_type)) {
		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     "Service type '%s' is invalid or has not been implemented yet",
					     service_type);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	}

	iface = tracker_db_manager_get_db_interface_by_service (service_type);
	result_set = tracker_data_search_keywords_get_list (iface, service_type);
	values = tracker_dbus_query_result_to_ptr_array (result_set);

	if (result_set) {
		g_object_unref (result_set);
	}

	dbus_g_method_return (context, values);
	tracker_dbus_results_ptr_array_free (&values);

	tracker_dbus_request_success (request_id);
}

void
tracker_keywords_get (TrackerKeywords	     *object,
		      const gchar	     *service_type,
		      const gchar	     *uri,
		      DBusGMethodInvocation  *context,
		      GError		    **error)
{
	TrackerDBInterface *iface;
	TrackerDBResultSet *result_set;
	guint		    request_id;
	gchar		   *id;
	GError		   *actual_error = NULL;
	gchar		  **values;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (service_type != NULL, context);
	tracker_dbus_async_return_if_fail (uri != NULL, context);

	tracker_dbus_request_new (request_id,
				  "DBus request to get keywords, "
				  "service type:'%s', uri:'%s'",
				  service_type,
				  uri);

	if (!tracker_ontology_service_is_valid (service_type)) {
		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     "Service type '%s' is invalid or has not been implemented yet",
					     service_type);
	}

	if (!actual_error && tracker_is_empty_string (uri)) {
		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     "URI is empty");
	}

	if (actual_error) {
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	}

	id = tracker_data_query_file_id_as_string (service_type, uri);
	if (!id) {
		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     "Entity '%s' was not found",
					     uri);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	}

	iface = tracker_db_manager_get_db_interface_by_service (service_type);
	result_set = tracker_data_query_metadata_field (iface,
					      id,
					      "User:Keywords");
	values = tracker_dbus_query_result_to_strv (result_set, 0, NULL);

	if (result_set) {
		g_object_unref (result_set);
	}

	g_free (id);

	dbus_g_method_return (context, values);

	if (values) {
		g_strfreev (values);
	}

	tracker_dbus_request_success (request_id);
}

void
tracker_keywords_add (TrackerKeywords	     *object,
		      const gchar	     *service_type,
		      const gchar	     *uri,
		      gchar		    **keywords,
		      DBusGMethodInvocation  *context,
		      GError		     **error)
{
	guint		     request_id;
	gchar		    *id;
	GError		    *actual_error = NULL;
	gchar		   **check;
	gchar		    *stripped_value;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (service_type != NULL, context);
	tracker_dbus_async_return_if_fail (uri != NULL, context);
	tracker_dbus_async_return_if_fail (keywords != NULL && *keywords != NULL, context);

	tracker_dbus_request_new (request_id,
				  "DBus request to add keywords, "
				  "service type:'%s', uri:'%s'",
				  service_type,
				  uri);

	if (!tracker_ontology_service_is_valid (service_type)) {
		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     "Service type '%s' is invalid or has not been implemented yet",
					     service_type);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	}

	if (tracker_is_empty_string (uri)) {
		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     "URI is empty");
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	}

	id = tracker_data_query_file_id_as_string (service_type, uri);
	if (!id) {
		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     "Entity '%s' was not found",
					     uri);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	}

	for (check = keywords; *check != NULL; check++) {
		stripped_value = g_strstrip (g_strdup (*check));

		if (tracker_is_empty_string (stripped_value)) {
			g_free (stripped_value);
			tracker_dbus_request_failed (request_id,
						     &actual_error,
						     "Blank spaces are not allowed as keywords");
			dbus_g_method_return_error (context, actual_error);
			g_free (id);
			g_error_free (actual_error);
			return;
		}
		g_free (stripped_value);
	}

	org_freedesktop_Tracker_Indexer_property_set (tracker_dbus_indexer_get_proxy (),
						      service_type,
						      uri,
						      "User:Keywords",
						      (const gchar **)keywords,
						      &actual_error);

	if (actual_error) {
		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     NULL);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		g_free (id);
		return;
	}

	g_free (id);

	dbus_g_method_return (context);

	tracker_dbus_request_success (request_id);
}

void
tracker_keywords_remove (TrackerKeywords	*object,
			 const gchar		*service_type,
			 const gchar		*uri,
			 gchar		       **keywords,
			 DBusGMethodInvocation	*context,
			 GError		       **error)
{
	guint		     request_id;
	gchar		    *service_id;
	GError		    *actual_error = NULL;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (service_type != NULL, context);
	tracker_dbus_async_return_if_fail (uri != NULL, context);
	tracker_dbus_async_return_if_fail (keywords != NULL && g_strv_length (keywords) > 0, context);

	tracker_dbus_request_new (request_id,
				  "DBus request to remove keywords, "
				  "service type:'%s', uri:'%s'",
				  service_type,
				  uri);

	if (!tracker_ontology_service_is_valid (service_type)) {
		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     "Service type '%s' is invalid or has not been implemented yet",
					     service_type);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	}

	if (tracker_is_empty_string (uri)) {
		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     "URI is empty");
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	}

	/* Check the uri exists, so we dont start the indexer in vain */
	service_id = tracker_data_query_file_id_as_string (service_type, uri);
	if (!service_id) {
		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     "Entity '%s' was not found",
					     uri);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	}

	org_freedesktop_Tracker_Indexer_property_remove (tracker_dbus_indexer_get_proxy (),
							 service_type,
							 uri,
							 "User:Keywords",
							 (const gchar **)keywords,
							 &actual_error);

	if (actual_error) {
		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     NULL);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	}

	g_free (service_id);

	dbus_g_method_return (context);

	tracker_dbus_request_success (request_id);
}

void
tracker_keywords_remove_all (TrackerKeywords	    *object,
			     const gchar	    *service_type,
			     const gchar	    *uri,
			     DBusGMethodInvocation  *context,
			     GError		   **error)
{
	guint		    request_id;
	gchar		  **values;
	gchar		   *service_id;
	GError		   *actual_error = NULL;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (service_type != NULL, context);
	tracker_dbus_async_return_if_fail (uri != NULL, context);

	tracker_dbus_request_new (request_id,
				  "DBus request to remove all keywords, "
				  "service type:'%s', uri:'%s'",
				  service_type,
				  uri);

	if (!tracker_ontology_service_is_valid (service_type)) {
		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     "Service type '%s' is invalid or has not been implemented yet",
					     service_type);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	}

	if (tracker_is_empty_string (uri)) {
		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     "URI is empty");
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	}

	/* Check the uri exists, so we dont start the indexer in vain */
	service_id = tracker_data_query_file_id_as_string (service_type, uri);
	if (!service_id) {
		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     "Entity '%s' was not found",
					     uri);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	}

	values = g_new0 (gchar *, 1);
	values[0] = NULL;
	org_freedesktop_Tracker_Indexer_property_remove (tracker_dbus_indexer_get_proxy (),
							 service_type,
							 uri,
							 "User:Keywords",
							 (const gchar **)values,
							 &actual_error);

	g_strfreev (values);
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
tracker_keywords_search (TrackerKeywords	*object,
			 gint			 live_query_id,
			 const gchar		*service_type,
			 const gchar	       **keywords,
			 gint			 offset,
			 gint			 max_hits,
			 DBusGMethodInvocation	*context,
			 GError		       **error)
{
	TrackerDBResultSet  *result_set;
	guint		     request_id;
	gchar		   **values;
	GError		    *actual_error = NULL;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (service_type != NULL, context);
	tracker_dbus_async_return_if_fail (keywords != NULL, context);
	tracker_dbus_async_return_if_fail (keywords[0] != NULL, context);

	tracker_dbus_request_new (request_id,
				  "DBus request to search keywords, "
				  "query id:%d, service type:'%s', offset:%d, "
				  "max hits:%d",
				  live_query_id,
				  service_type,
				  offset,
				  max_hits);

	result_set = tracker_data_search_keywords (service_type,
						   keywords,
						   offset,
						   max_hits,
						   &actual_error);

	if (actual_error) {
		tracker_dbus_request_failed (request_id, &actual_error, NULL);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	}

	values = tracker_dbus_query_result_to_strv (result_set, 0, NULL);

	if (result_set) {
		g_object_unref (result_set);
	}

	dbus_g_method_return (context, values);

	if (values) {
		g_strfreev (values);
	}

	tracker_dbus_request_success (request_id);
}
