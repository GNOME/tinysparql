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
#include <libtracker-db/tracker-db-index.h>
#include <libtracker-db/tracker-db-manager.h>

#include <libtracker-data/tracker-data-manager.h>
#include <libtracker-data/tracker-data-query.h>
#include <libtracker-data/tracker-data-schema.h>
#include <libtracker-data/tracker-data-search.h>
#include <libtracker-data/tracker-rdf-query.h>

#include "tracker-indexer-client.h"
#include "tracker-dbus.h"
#include "tracker-metadata.h"
#include "tracker-marshal.h"

G_DEFINE_TYPE(TrackerMetadata, tracker_metadata, G_TYPE_OBJECT)

static void
tracker_metadata_class_init (TrackerMetadataClass *klass)
{
}

static void
tracker_metadata_init (TrackerMetadata *object)
{
}

TrackerMetadata *
tracker_metadata_new (void)
{
	return g_object_new (TRACKER_TYPE_METADATA, NULL);
}

/*
 * Functions
 */

void
tracker_metadata_get (TrackerMetadata	     *object,
		      const gchar	     *service_type,
		      const gchar	     *uri,
		      gchar		    **keys,
		      DBusGMethodInvocation  *context,
		      GError		    **error)
{
	TrackerDBInterface  *iface;
	TrackerDBResultSet  *result_set;
	guint		     request_id;
	const gchar         *service_result;
	guint32              service_id;
	gchar		    *service_id_str;
	guint		     i;
	gchar		   **values;
	GError		    *actual_error = NULL;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (service_type != NULL, context);
	tracker_dbus_async_return_if_fail (uri != NULL, context);
	tracker_dbus_async_return_if_fail (keys != NULL, context);
	tracker_dbus_async_return_if_fail (g_strv_length (keys) > 0, context);

	tracker_dbus_request_new (request_id,
				  "DBus request to get metadata values, "
				  "service type:'%s'",
				  service_type);

	if (!tracker_ontology_service_is_valid (service_type)) {
		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     "Service '%s' is invalid or has not been implemented yet",
					     service_type);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	}

	service_id = tracker_data_query_file_id (service_type, uri);

	if (service_id <= 0) {
		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     "Service URI '%s' not found",
					     uri);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	}

	/* Checking keys */
	for (i = 0; i < g_strv_length (keys); i++) {

		if (tracker_ontology_get_field_by_name (keys[i]) == NULL) {
			tracker_dbus_request_failed (request_id,
						     &actual_error,
						     "Metadata field '%s' not registered in the system",
						     keys[i]);
			dbus_g_method_return_error (context, actual_error);
			g_error_free (actual_error);
			return;
		}
	}

	/* The parameter service_type can be "Files"
	 * and the actual service type of the uri "Video"
	 *
	 * Note: Does this matter?
	 */
	iface = tracker_db_manager_get_db_interface_by_service (service_type);

	service_result = tracker_data_query_service_type_by_id (iface, service_id);
	if (!service_result) {
	       tracker_dbus_request_failed (request_id,
					    &actual_error,
					    "Service type can not be found for entity '%s'",
					    uri);
	       dbus_g_method_return_error (context, actual_error);
	       g_error_free (actual_error);
	       return;
	}

	service_id_str = tracker_guint_to_string (service_id);
	result_set = tracker_data_query_metadata_fields (iface, service_result, service_id_str, keys);
	g_free (service_id_str);

	if (result_set) {
		values = tracker_dbus_query_result_columns_to_strv (result_set, -1, -1, TRUE);
		g_object_unref (result_set);
	} else {
		values = NULL;
	}

	if (!values) {
		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     "No metadata information was available");
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
	}

	dbus_g_method_return (context, values);
	g_strfreev (values);

	tracker_dbus_request_success (request_id);
}

void
tracker_metadata_get_all (TrackerMetadata	     *object,
			  const gchar	             *service_type,
			  const gchar	             *uri,
			  DBusGMethodInvocation      *context,
			  GError		    **error)
{
	guint		     request_id;
	gchar		    *service_id;
	GPtrArray *          values;
	GError		    *actual_error = NULL;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (service_type != NULL, context);
	tracker_dbus_async_return_if_fail (uri != NULL, context);

	tracker_dbus_request_new (request_id,
				  "DBus request to get all metadata values, "
				  "service type:'%s' uri:'%s'",
				  service_type, uri);

	if (!tracker_ontology_service_is_valid (service_type)) {
		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     "Service '%s' is invalid or has not been implemented yet",
					     service_type);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	}

	service_id = tracker_data_query_file_id_as_string (service_type, uri);
	if (!service_id) {
		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     "Service URI '%s' not found",
					     uri);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	}

	values = tracker_data_query_all_metadata (service_type, service_id);

	dbus_g_method_return (context, values);
	g_ptr_array_foreach (values, (GFunc)g_strfreev, NULL);
	g_ptr_array_free (values, TRUE);

	tracker_dbus_request_success (request_id);
}

void
tracker_metadata_set (TrackerMetadata	     *object,
		      const gchar	     *service_type,
		      const gchar	     *uri,
		      gchar		    **keys,
		      gchar		    **values,
		      DBusGMethodInvocation  *context,
		      GError		    **error)
{
	guint		    request_id;
	gchar		   *service_id;
	guint		    i;
	GError		   *actual_error = NULL;
	TrackerField       *field_def;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (service_type != NULL, context);
	tracker_dbus_async_return_if_fail (keys != NULL, context);
	tracker_dbus_async_return_if_fail (values != NULL, context);
	tracker_dbus_async_return_if_fail (g_strv_length (keys) > 0, context);
	tracker_dbus_async_return_if_fail (g_strv_length (values) > 0, context);
	tracker_dbus_async_return_if_fail (g_strv_length (keys) == g_strv_length (values), context);

	tracker_dbus_request_new (request_id,
				  "DBus request to set metadata keys, "
				  "service type:'%s' uri:'%s'",
				  service_type, uri);

	if (!tracker_ontology_service_is_valid (service_type)) {
		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     "Service_Type '%s' is invalid or has not been implemented yet",
					     service_type);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	}

	/* Check the uri exists, so we dont start the indexer in vain */
	service_id = tracker_data_query_file_id_as_string (service_type, uri);
	if (!service_id) {
		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     "Service URI '%s' not found",
					     uri);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	}

	/* Checking keys */
	for (i = 0; i < g_strv_length (keys); i++) {
		gchar **tmp_values;
		gint    len;

		field_def = tracker_ontology_get_field_by_name (keys[i]);

		if (field_def == NULL) {
			tracker_dbus_request_failed (request_id,
						     &actual_error,
						     "Metadata field '%s' not registered in the system",
						     keys[i]);
			dbus_g_method_return_error (context, actual_error);
			g_error_free (actual_error);
			return;
		}

		tmp_values = tracker_string_to_string_list (values[i]);
		len = g_strv_length (tmp_values);
		g_strfreev (tmp_values);

		if (!tracker_field_get_multiple_values (field_def) && len > 1) {
			tracker_dbus_request_failed (request_id,
						     &actual_error,
						     "Field type: '%s' doesnt support multiple values (trying to set %d)",
						     tracker_field_get_name (field_def),
						     len);
			dbus_g_method_return_error (context, actual_error);
			g_error_free (actual_error);
			return;
		}
	}

	/* Real insertion */
	for (i = 0; i < g_strv_length (keys); i++) {
		gchar	    **value;

		value = tracker_string_to_string_list (values[i]);
		org_freedesktop_Tracker_Indexer_property_set (tracker_dbus_indexer_get_proxy (),
							      service_type,
							      uri,
							      keys[i],
							      (const gchar **)value,
							      &actual_error);
		g_strfreev (value);
		if (actual_error) {
			tracker_dbus_request_failed (request_id, &actual_error, NULL);
			dbus_g_method_return_error (context, actual_error);
			g_error_free (actual_error);
			return;
		}
	}

	g_free (service_id);

	/* FIXME: Check return value? */

	dbus_g_method_return (context);

	tracker_dbus_request_success (request_id);
}

void
tracker_metadata_get_type_details (TrackerMetadata	  *object,
				   const gchar		  *metadata,
				   DBusGMethodInvocation  *context,
				   GError		 **error)
{
	guint		  request_id;
	TrackerField	 *def = NULL;
	TrackerFieldType  field_type;
	gchar		 *type;
	gboolean	  is_embedded;
	gboolean	  is_writable;
	GError		 *actual_error = NULL;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (metadata != NULL, context);

	tracker_dbus_request_new (request_id,
				  "DBus request to get metadata details, "
				  "metadata type:'%s'",
				  metadata);

	def = tracker_ontology_get_field_by_name (metadata);
	if (!def) {
		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     "Metadata name '%s' is invalid or unrecognized",
					     metadata);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	}

	field_type = tracker_field_get_data_type (def);

	type = g_strdup (tracker_field_type_to_string (field_type));
	is_embedded = tracker_field_get_embedded (def);
	is_writable = !tracker_field_get_embedded (def);

	dbus_g_method_return (context, type, is_embedded, is_writable);
	g_free (type);

	tracker_dbus_request_success (request_id);
}

void
tracker_metadata_get_registered_types (TrackerMetadata	      *object,
				       const gchar	      *service_type,
				       DBusGMethodInvocation  *context,
				       GError		     **error)
{
	guint		     request_id;
	gchar		   **values = NULL;
	const gchar	    *requested = NULL;
	GSList		    *registered = NULL;
	GError		    *actual_error = NULL;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (service_type != NULL, context);

	tracker_dbus_request_new (request_id,
				  "DBus request to get registered metadata types, "
				  "service_type:'%s'",
				  service_type);

	if (strcmp (service_type, "*") != 0 &&
	    !tracker_ontology_service_is_valid (service_type)) {
		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     "Service_Type '%s' is invalid or has not been implemented yet",
					     service_type);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	}

	requested = (strcmp (service_type, "*") == 0 ? NULL : service_type);

	registered = tracker_ontology_get_field_names_registered (requested);

	values = tracker_gslist_to_string_list (registered);

	g_slist_foreach (registered, (GFunc) g_free, NULL);
	g_slist_free (registered);

	dbus_g_method_return (context, values);

	if (values) {
		g_strfreev (values);
	}

	tracker_dbus_request_success (request_id);
}

void
tracker_metadata_get_registered_classes (TrackerMetadata	*object,
					 DBusGMethodInvocation	*context,
					 GError		       **error)
{
	guint		     request_id;
	gchar		   **values = NULL;
	GSList		    *registered = NULL;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_request_new (request_id,
				  "DBus request to get registered classes");

	registered = tracker_ontology_get_service_names_registered ();

	values = tracker_gslist_to_string_list (registered);

	g_slist_foreach (registered, (GFunc) g_free, NULL);
	g_slist_free (registered);

	dbus_g_method_return (context, values);

	if (values) {
		g_strfreev (values);
	}

	tracker_dbus_request_success (request_id);
}

void
tracker_metadata_get_unique_values (TrackerMetadata	   *object,
				    const gchar		   *service_type,
				    gchar		  **fields,
				    const gchar		   *query_condition,
				    gboolean		    order_desc,
				    gint		    offset,
				    gint		    max_hits,
				    DBusGMethodInvocation  *context,
				    GError		  **error)
{
	TrackerDBResultSet *result_set = NULL;
	guint		    request_id;

	GPtrArray	   *values = NULL;

	GError		   *actual_error = NULL;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (service_type != NULL, context);
	tracker_dbus_async_return_if_fail (fields != NULL, context);
	tracker_dbus_async_return_if_fail (query_condition != NULL, context);

	tracker_dbus_request_new (request_id,
				  "DBus request to get unique values, "
				  "service type:'%s', query '%s'",
				  service_type,
				  query_condition);

	result_set = tracker_data_search_get_unique_values (service_type, fields,
							    query_condition,
							    order_desc,
							    offset,
							    max_hits,
							    &actual_error);

	if (actual_error) {
		tracker_dbus_request_failed (request_id, &actual_error, NULL);
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
tracker_metadata_get_unique_values_with_count (TrackerMetadata	      *object,
					       const gchar	      *service_type,
					       gchar		     **fields,
					       const gchar	      *query_condition,
					       const gchar	      *count_field,
					       gboolean		       order_desc,
					       gint		       offset,
					       gint		       max_hits,
					       DBusGMethodInvocation  *context,
					       GError		     **error)
{
	TrackerDBResultSet *result_set = NULL;
	guint		    request_id;
	GPtrArray	   *values = NULL;
	GError		   *actual_error = NULL;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (service_type != NULL, context);
	tracker_dbus_async_return_if_fail (fields != NULL, context);
	tracker_dbus_async_return_if_fail (query_condition != NULL, context);

	tracker_dbus_request_new (request_id,
				  "DBus request to get unique values, "
				  "service type:'%s', query '%s'"
				  "count field :'%s'",
				  service_type,
				  query_condition,
				  count_field);

	result_set = 
		tracker_data_search_get_unique_values_with_count (service_type,
								  fields,
								  query_condition,
								  count_field,
								  order_desc,
								  offset,
								  max_hits,
								  &actual_error);
	
	if (actual_error) {
		tracker_dbus_request_failed (request_id, &actual_error, NULL);
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
tracker_metadata_get_unique_values_with_count_and_sum (TrackerMetadata	      *object,
						       const gchar	      *service_type,
						       gchar		     **fields,
						       const gchar	      *query_condition,
						       const gchar	      *count_field,
						       const gchar            *sum_field,
						       gboolean		       order_desc,
						       gint		       offset,
						       gint		       max_hits,
						       DBusGMethodInvocation  *context,
						       GError		     **error)
{
	TrackerDBResultSet *result_set = NULL;
	guint		    request_id;
	GPtrArray	   *values = NULL;
	GError		   *actual_error = NULL;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (service_type != NULL, context);
	tracker_dbus_async_return_if_fail (fields != NULL, context);
	tracker_dbus_async_return_if_fail (query_condition != NULL, context);

	tracker_dbus_request_new (request_id,
				  "DBus request to get unique values with count and sum, "
				  "service type:'%s', query '%s', "
				  "count field :'%s', "
				  "sum field :'%s'",
				  service_type,
				  query_condition,
				  count_field,
				  sum_field);

	result_set = 
		tracker_data_search_get_unique_values_with_count_and_sum (service_type,
									  fields,
									  query_condition,
									  count_field,
									  sum_field,
									  order_desc,
									  offset,
									  max_hits,
									  &actual_error);

	if (actual_error) {
		tracker_dbus_request_failed (request_id, &actual_error, NULL);
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
tracker_metadata_get_unique_values_with_concat_count_and_sum (TrackerMetadata	      *object,
							      const gchar	      *service_type,
							      gchar		     **fields,
							      const gchar	      *query_condition,
							      const gchar             *concat_field,
							      const gchar	      *count_field,
							      const gchar             *sum_field,
							      gboolean		       order_desc,
							      gint		       offset,
							      gint		       max_hits,
							      DBusGMethodInvocation  *context,
							      GError		     **error)
{
	TrackerDBResultSet *result_set = NULL;
	guint		    request_id;
	GPtrArray	   *values = NULL;
	GError		   *actual_error = NULL;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (service_type != NULL, context);
	tracker_dbus_async_return_if_fail (fields != NULL, context);
	tracker_dbus_async_return_if_fail (query_condition != NULL, context);

	tracker_dbus_request_new (request_id,
				  "DBus request to get unique values with count and sum, "
				  "service type:'%s', query '%s', "
				  "concat field :'%s' "
				  "count field :'%s', "
				  "sum field :'%s'",
				  service_type,
				  query_condition,
				  concat_field,
				  count_field,
				  sum_field);

	result_set = 
		tracker_data_search_get_unique_values_with_concat_count_and_sum (service_type,
										 fields,
										 query_condition,
										 concat_field,
										 count_field,
										 sum_field,
										 order_desc,
										 offset,
										 max_hits,
										 &actual_error);

	if (actual_error) {
		tracker_dbus_request_failed (request_id, &actual_error, NULL);
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
tracker_metadata_get_sum (TrackerMetadata	 *object,
			  const gchar		 *service_type,
			  const gchar		 *field,
			  const gchar		 *query_condition,
			  DBusGMethodInvocation  *context,
			  GError		**error)
{
	guint   request_id;
	gint    sum;
	GError *actual_error = NULL;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (service_type != NULL, context);
	tracker_dbus_async_return_if_fail (field != NULL, context);
	tracker_dbus_async_return_if_fail (query_condition != NULL, context);

	tracker_dbus_request_new (request_id,
				  "DBus request to get sum, "
				  "service type:'%s', field '%s', query '%s'",
				  service_type,
				  field,
				  query_condition);

	sum = tracker_data_search_get_sum (service_type,
					   field,
					   query_condition,
					   &actual_error);

	if (actual_error) {
		tracker_dbus_request_failed (request_id, &actual_error, NULL);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	}

	dbus_g_method_return (context, sum);

	tracker_dbus_request_success (request_id);
}

void
tracker_metadata_get_count (TrackerMetadata	   *object,
			    const gchar		   *service_type,
			    const gchar		   *field,
			    const gchar		   *query_condition,
			    DBusGMethodInvocation  *context,
			    GError		  **error)
{
	guint	request_id;
	gint    count;

	GError *actual_error = NULL;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (service_type != NULL, context);
	tracker_dbus_async_return_if_fail (field != NULL, context);
	tracker_dbus_async_return_if_fail (query_condition != NULL, context);

	tracker_dbus_request_new (request_id,
				  "DBus request to get count, "
				  "service type:'%s', field '%s', query '%s'",
				  service_type,
				  field,
				  query_condition);

	count = tracker_data_search_get_count (service_type,
					       field,
					       query_condition,
					       &actual_error);

	if (actual_error) {
		tracker_dbus_request_failed (request_id, &actual_error, NULL);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	}

	dbus_g_method_return (context, count);

	tracker_dbus_request_success (request_id);
}

