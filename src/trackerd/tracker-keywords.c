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

#include "tracker-dbus.h"
#include "tracker-keywords.h"
#include "tracker-db.h"
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
	result_set = tracker_db_keywords_get_list (iface, service_type);
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

	iface = tracker_db_manager_get_db_interface_by_service (service_type);
	id = tracker_db_file_get_id_as_string (iface, service_type, uri);
	if (!id) {
		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     "Entity '%s' was not found",
					     uri);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	}

	result_set = tracker_db_metadata_get (iface,
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
	TrackerDBInterface  *iface;
	guint		     request_id;
	gchar		    *id;
	GError		    *actual_error = NULL;

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

	iface = tracker_db_manager_get_db_interface_by_service (service_type);
	id = tracker_db_file_get_id_as_string (iface, service_type, uri);
	if (!id) {
		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     "Entity '%s' was not found",
					     uri);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
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
	TrackerDBInterface  *iface;
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
	iface = tracker_db_manager_get_db_interface_by_service (service_type);
	service_id = tracker_db_file_get_id_as_string (iface, service_type, uri);
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
	TrackerDBInterface *iface;
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
	iface = tracker_db_manager_get_db_interface_by_service (service_type);
	service_id = tracker_db_file_get_id_as_string (iface, service_type, uri);
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
	TrackerDBInterface  *iface;
	TrackerDBResultSet  *result_set;
	guint		     request_id;
	const gchar	   **p;
	GString		    *search;
	GString		    *select;
	GString		    *where;
	gchar		    *related_metadata;
	gchar		    *query;
	gchar		   **values;
	GError		    *actual_error = NULL;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (service_type != NULL, context);
	tracker_dbus_async_return_if_fail (keywords != NULL, context);

	tracker_dbus_request_new (request_id,
				  "DBus request to search keywords, "
				  "query id:%d, service type:'%s', offset:%d, "
				  "max hits:%d",
				  live_query_id,
				  service_type,
				  offset,
				  max_hits);

	if (!tracker_ontology_service_is_valid (service_type)) {
		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     "Service_Type '%s' is invalid or has not been implemented yet",
					     service_type);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	}

	iface = tracker_db_manager_get_db_interface_by_service (service_type);

	/* Sanity check values */
	offset = MAX (offset, 0);

	/* Create keyword search string */
	search = g_string_new ("");
	g_string_append_printf (search,
				"'%s'",
				keywords[0]);

	for (p = keywords + 1; *p; p++) {
		g_string_append_printf (search, ", '%s'", *p);
	}

	tracker_dbus_request_comment (request_id,
				      "Executing keyword search on %s",
				      search->str);

	/* Create select string */
	select = g_string_new (" Select distinct S.Path || '");
	select = g_string_append (select, G_DIR_SEPARATOR_S);
	select = g_string_append (select,
				  "' || S.Name as EntityName from Services S, ServiceKeywordMetaData M ");

	/* Create where string */
	related_metadata = tracker_db_metadata_get_related_names (iface, "User:Keywords");

	where = g_string_new ("");
	g_string_append_printf (where,
				" where S.ID = M.ServiceID and M.MetaDataID in (%s) and M.MetaDataValue in (%s) ",
				related_metadata,
				search->str);
	g_free (related_metadata);
	g_string_free (search, TRUE);

	g_string_append_printf (where,
				"  and	(S.ServiceTypeID in (select TypeId from ServiceTypes where TypeName = '%s' or Parent = '%s')) ",
				service_type,
				service_type);

	/* Add offset and max_hits */
	g_string_append_printf (where,
				" Limit %d,%d",
				offset,
				max_hits);

	/* Finalize query */
	query = g_strconcat (select->str, where->str, NULL);
	g_string_free (select, TRUE);
	g_string_free (where, TRUE);

	g_debug (query);

	result_set = tracker_db_interface_execute_query (iface, NULL, query);
	values = tracker_dbus_query_result_to_strv (result_set, 0, NULL);

	if (result_set) {
		g_object_unref (result_set);
	}

	g_free (query);

	dbus_g_method_return (context, values);

	if (values) {
		g_strfreev (values);
	}

	tracker_dbus_request_success (request_id);
}
