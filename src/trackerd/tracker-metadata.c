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
#include <libtracker-common/tracker-field-data.h>
#include <libtracker-common/tracker-utils.h>
#include <libtracker-common/tracker-type-utils.h>

#include <libtracker-db/tracker-db-dbus.h>
#include <libtracker-db/tracker-db-index.h>
#include <libtracker-db/tracker-db-manager.h>

#include "tracker-indexer-client.h"
#include "tracker-dbus.h"
#include "tracker-metadata.h"
#include "tracker-db.h"
#include "tracker-marshal.h"

#include "tracker-rdf-query.h"

#define DEFAULT_METADATA_MAX_HITS 1024

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

static gint
metadata_sanity_check_max_hits (gint max_hits)
{
	if (max_hits < 1) {
		return DEFAULT_METADATA_MAX_HITS;
	}

	return max_hits;
}

static TrackerFieldData *
tracker_metadata_add_metadata_field (TrackerDBInterface *iface,
		    const gchar        *service,
		    GSList	      **fields,
		    const gchar        *field_name,
		    gboolean		is_select,
		    gboolean		is_condition)
{
	TrackerFieldData *field_data;
	gboolean	  field_exists;
	GSList		 *l;

	field_exists = FALSE;
	field_data = NULL;

	/* Check if field is already in list */
	for (l = *fields; l; l = l->next) {
		const gchar *this_field_name;

		this_field_name = tracker_field_data_get_field_name (l->data);
		if (!this_field_name) {
			continue;
		}

		if (strcasecmp (this_field_name, field_name) == 0) {
			field_data = l->data;
			field_exists = TRUE;

			if (is_condition) {
				tracker_field_data_set_is_condition (field_data, TRUE);
			}

			break;
		}
	}

	if (!field_exists) {
		field_data = tracker_db_get_metadata_field (iface,
							    service,
							    field_name,
							    g_slist_length (*fields),
							    is_select,
							    is_condition);
		if (field_data) {
			*fields = g_slist_prepend (*fields, field_data);
		}
	}

	return field_data;
}

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
	gchar		    *service_id, *service_result;
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

	iface = tracker_db_manager_get_db_interface_by_service (service_type);

	service_id = tracker_db_file_get_id_as_string (iface, service_type, uri);
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

		if (tracker_ontology_get_field_by_name (keys[i]) == NULL) {
			tracker_dbus_request_failed (request_id,
						     &actual_error,
						     "Metadata field '%s' not registered in the system",
						     uri);
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
	service_result = tracker_db_service_get_by_entity (iface, service_id);
	if (!service_result) {
	       g_free (service_id);
	       tracker_dbus_request_failed (request_id,
					    &actual_error,
					    "Service type can not be found for entity '%s'",
					    uri);
	       dbus_g_method_return_error (context, actual_error);
	       g_error_free (actual_error);
	       return;
	}

	result_set = tracker_db_metadata_get_array (iface, service_result, service_id, keys);
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
tracker_metadata_set (TrackerMetadata	     *object,
		      const gchar	     *service_type,
		      const gchar	     *uri,
		      gchar		    **keys,
		      gchar		    **values,
		      DBusGMethodInvocation  *context,
		      GError		    **error)
{
	TrackerDBInterface *iface;
	guint		    request_id;
	gchar		   *service_id;
	guint		    i;
	GError		   *actual_error = NULL;

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

	iface = tracker_db_manager_get_db_interface_by_service (service_type);

	/* Check the uri exists, so we dont start the indexer in vain */
	service_id = tracker_db_file_get_id_as_string (iface, service_type, uri);
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
		TrackerField *field_def;
		gchar	    **value;

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

		if (tracker_field_get_embedded (field_def)) {
			tracker_dbus_request_failed (request_id,
						     &actual_error,
						     "Metadata field '%s' cannot be overwritten (is embedded)",
						     keys[i]);
			dbus_g_method_return_error (context, actual_error);
			g_error_free (actual_error);
			return;
		}

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
	TrackerDBInterface *iface;
	TrackerDBResultSet *result_set = NULL;
	guint		    request_id;

	GPtrArray	   *values = NULL;
	GSList		   *field_list = NULL;
	gchar		   *str_offset, *str_limit;

	GString		   *sql_select;
	GString		   *sql_from;
	GString		   *sql_where;
	GString		   *sql_order;
	gchar		   *sql;

	char		   *rdf_where;
	char		   *rdf_from;
	GError		   *actual_error = NULL;

	guint		    i;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (service_type != NULL, context);
	tracker_dbus_async_return_if_fail (fields != NULL, context);
	tracker_dbus_async_return_if_fail (query_condition != NULL, context);

	tracker_dbus_request_new (request_id,
				  "DBus request to get unique values, "
				  "service type:'%s', query '%s'",
				  service_type,
				  query_condition);

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

	sql_select = g_string_new ("SELECT DISTINCT ");
	sql_from   = g_string_new ("\nFROM Services S ");
	sql_where  = g_string_new ("\nWHERE ");
	sql_order  = g_string_new ("\nORDER BY ");

	for (i=0;i<g_strv_length(fields);i++) {
		TrackerFieldData   *def = NULL;
		def = tracker_metadata_add_metadata_field (iface, service_type, &field_list, fields[i], FALSE, TRUE);

		if (!def) {
			g_string_free (sql_select, TRUE);
			g_string_free (sql_from, TRUE);
			g_string_free (sql_where, TRUE);
			g_string_free (sql_order, TRUE);

			tracker_dbus_request_failed (request_id,
						     &actual_error,
						     "Invalid or non-existant metadata type '%s' specified",
						     fields[i]);
			dbus_g_method_return_error (context, actual_error);
			g_error_free (actual_error);
			return;
		}

		if (i) {
			g_string_append_printf (sql_select, ",");
			g_string_append_printf (sql_order, ",");
		}

		g_string_append_printf (sql_select, "%s", tracker_field_data_get_select_field (def));
		g_string_append_printf (sql_order, " %s %s",
					tracker_field_data_get_select_field (def),
					order_desc ? "DESC" : "ASC" );

	}

	tracker_rdf_filter_to_sql (iface, query_condition, service_type,
				   &field_list, &rdf_from, &rdf_where, &actual_error);

	if (actual_error) {

		g_string_free (sql_select, TRUE);
		g_string_free (sql_from, TRUE);
		g_string_free (sql_where, TRUE);
		g_string_free (sql_order, TRUE);


		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     NULL);

		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	}

	g_string_append_printf (sql_from, " %s ", rdf_from);
	g_string_append_printf (sql_where, " %s ", rdf_where);

	g_free (rdf_from);
	g_free (rdf_where);

	str_offset = tracker_gint_to_string (offset);
	str_limit = tracker_gint_to_string (metadata_sanity_check_max_hits (max_hits));

	g_string_append_printf (sql_order, " LIMIT %s,%s", str_offset, str_limit);

	sql = g_strconcat (sql_select->str, " ", sql_from->str, " ", sql_where->str, " ", sql_order->str, NULL);

	g_free (str_offset);
	g_free (str_limit);

	g_string_free (sql_select, TRUE);
	g_string_free (sql_from, TRUE);
	g_string_free (sql_where, TRUE);
	g_string_free (sql_order, TRUE);

	g_slist_foreach (field_list, (GFunc) g_object_unref, NULL);
	g_slist_free (field_list);

	g_message ("Unique values query executed:\n%s", sql);

	result_set =  tracker_db_interface_execute_query (iface, NULL, sql);

	g_free (sql);

	values = tracker_dbus_query_result_to_ptr_array (result_set);

	dbus_g_method_return (context, values);

	tracker_dbus_results_ptr_array_free (&values);

	if (result_set) {
		g_object_unref (result_set);
	}

	tracker_dbus_request_success (request_id);

	return;
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
	TrackerDBInterface *iface;
	TrackerDBResultSet *result_set = NULL;
	guint		    request_id;

	GPtrArray	   *values = NULL;
	GSList		   *field_list = NULL;
	gchar		   *str_offset, *str_limit;

	GString		   *sql_select;
	GString		   *sql_from;
	GString		   *sql_where;
	GString		   *sql_order;
	GString		   *sql_group;
	gchar		   *sql;

	char		   *rdf_where;
	char		   *rdf_from;
	GError		   *actual_error = NULL;

	guint		    i;

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

	sql_select = g_string_new ("SELECT DISTINCT ");
	sql_from   = g_string_new ("\nFROM Services S ");
	sql_where  = g_string_new ("\nWHERE ");
	sql_order  = g_string_new ("\nORDER BY ");
	sql_group  = g_string_new ("\nGROUP BY ");


	for (i=0;i<g_strv_length(fields);i++) {
		TrackerFieldData   *def = NULL;
		def = tracker_metadata_add_metadata_field (iface, service_type, &field_list, fields[i], FALSE, TRUE);

		if (!def) {
			g_string_free (sql_select, TRUE);
			g_string_free (sql_from, TRUE);
			g_string_free (sql_where, TRUE);
			g_string_free (sql_order, TRUE);
			g_string_free (sql_group, TRUE);

			tracker_dbus_request_failed (request_id,
						     &actual_error,
						     "Invalid or non-existant metadata type '%s' specified",
						     fields[i]);
			dbus_g_method_return_error (context, actual_error);
			g_error_free (actual_error);
			return;
		}

		if (i) {
			g_string_append_printf (sql_select, ",");
			g_string_append_printf (sql_order, ",");
			g_string_append_printf (sql_group, ",");
		}

		g_string_append_printf (sql_select, "%s", tracker_field_data_get_select_field (def));
		g_string_append_printf (sql_order, " %s %s",
					tracker_field_data_get_select_field (def),
					order_desc ? "DESC" : "ASC" );
		g_string_append_printf (sql_group, "%s", tracker_field_data_get_select_field (def));

	}

	if (count_field && !(tracker_is_empty_string (count_field))) {
		TrackerFieldData   *def = NULL;

		def = tracker_metadata_add_metadata_field (iface, service_type, &field_list, count_field, FALSE, TRUE);

		if (!def) {
			g_string_free (sql_select, TRUE);
			g_string_free (sql_from, TRUE);
			g_string_free (sql_where, TRUE);
			g_string_free (sql_order, TRUE);
			g_string_free (sql_group, TRUE);

			tracker_dbus_request_failed (request_id,
						     &actual_error,
						     "Invalid or non-existant metadata type '%s' specified",
						     count_field);
			dbus_g_method_return_error (context, actual_error);
			g_error_free (actual_error);
			return;
		}

		g_string_append_printf (sql_select, ", COUNT (DISTINCT %s)", tracker_field_data_get_select_field (def));
	}

	tracker_rdf_filter_to_sql (iface, query_condition, service_type,
				   &field_list, &rdf_from, &rdf_where, &actual_error);

	if (actual_error) {

		g_string_free (sql_select, TRUE);
		g_string_free (sql_from, TRUE);
		g_string_free (sql_where, TRUE);
		g_string_free (sql_order, TRUE);
		g_string_free (sql_group, TRUE);


		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     NULL);

		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	}

	g_string_append_printf (sql_from, " %s ", rdf_from);
	g_string_append_printf (sql_where, " %s ", rdf_where);

	g_free (rdf_from);
	g_free (rdf_where);

	str_offset = tracker_gint_to_string (offset);
	str_limit = tracker_gint_to_string (metadata_sanity_check_max_hits (max_hits));

	g_string_append_printf (sql_order, " LIMIT %s,%s", str_offset, str_limit);

	sql = g_strconcat (sql_select->str, " ",
			   sql_from->str, " ",
			   sql_where->str, " ",
			   sql_group->str, " ",
			   sql_order->str, NULL);

	g_free (str_offset);
	g_free (str_limit);

	g_string_free (sql_select, TRUE);
	g_string_free (sql_from, TRUE);
	g_string_free (sql_where, TRUE);
	g_string_free (sql_order, TRUE);
	g_string_free (sql_group, TRUE);

	g_slist_foreach (field_list, (GFunc) g_object_unref, NULL);
	g_slist_free (field_list);

	g_message ("Unique values query executed:\n%s", sql);

	result_set =  tracker_db_interface_execute_query (iface, NULL, sql);

	g_free (sql);

	values = tracker_dbus_query_result_to_ptr_array (result_set);

	dbus_g_method_return (context, values);

	tracker_dbus_results_ptr_array_free (&values);

	if (result_set) {
		g_object_unref (result_set);
	}

	tracker_dbus_request_success (request_id);

	return;
}

static gboolean
is_data_type_numeric (TrackerFieldType type) {
	return (type == TRACKER_FIELD_TYPE_INTEGER
		|| type == TRACKER_FIELD_TYPE_DOUBLE);
}


void
tracker_metadata_get_sum (TrackerMetadata	 *object,
			  const gchar		 *service_type,
			  const gchar		 *field,
			  const gchar		 *query_condition,
			  DBusGMethodInvocation  *context,
			  GError		**error)
{
	TrackerDBInterface *iface;
	TrackerDBResultSet *result_set = NULL;
	guint		    request_id;

	gint		    sum;
	GSList		   *fields = NULL;
	TrackerFieldData   *def = NULL;
	TrackerFieldType    data_type;
	GString		   *sql_select;
	GString		   *sql_from;
	GString		   *sql_where;
	gchar		   *sql;

	char		   *rdf_where;
	char		   *rdf_from;
	GError		   *actual_error = NULL;

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

	if (!tracker_ontology_service_is_valid (service_type)) {
		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     "Service '%s' is invalid or has not been implemented yet",
					     service_type);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	}

	iface = tracker_db_manager_get_db_interface_by_service (service_type);

	sql_select = g_string_new ("SELECT ");
	sql_from   = g_string_new ("\nFROM Services S ");
	sql_where  = g_string_new ("\nWHERE ");

	def = tracker_metadata_add_metadata_field (iface, service_type, &fields, field, FALSE, TRUE);

	data_type = tracker_field_data_get_data_type (def);
	if (!is_data_type_numeric (data_type)) {
		g_string_free (sql_select, TRUE);
		g_string_free (sql_from, TRUE);
		g_string_free (sql_where, TRUE);
		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     "Cannot sum '%s': this metadata type is not numeric",
					     field);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	}


	if (!def) {
		g_string_free (sql_select, TRUE);
		g_string_free (sql_from, TRUE);
		g_string_free (sql_where, TRUE);
		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     "Invalid or non-existant metadata type '%s' specified",
					     field);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	}

	g_string_append_printf (sql_select, "SUM (%s)", tracker_field_data_get_select_field (def));

	tracker_rdf_filter_to_sql (iface, query_condition, service_type,
				   &fields, &rdf_from, &rdf_where, &actual_error);

	if (actual_error) {

		g_string_free (sql_select, TRUE);
		g_string_free (sql_from, TRUE);
		g_string_free (sql_where, TRUE);

		tracker_dbus_request_failed (request_id, &actual_error, NULL);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	}

	g_string_append_printf (sql_from, " %s ", rdf_from);
	g_string_append_printf (sql_where, " %s ", rdf_where);

	g_free (rdf_from);
	g_free (rdf_where);

	sql = g_strconcat (sql_select->str, " ", sql_from->str, " ", sql_where->str, NULL);

	g_string_free (sql_select, TRUE);
	g_string_free (sql_from, TRUE);
	g_string_free (sql_where, TRUE);

	g_slist_foreach (fields, (GFunc) g_object_unref, NULL);
	g_slist_free (fields);

	g_debug ("Sum query executed:\n%s", sql);

	result_set =  tracker_db_interface_execute_query (iface, NULL, sql);

	g_free (sql);

	tracker_db_result_set_get (result_set, 0, &sum, -1);

	if (result_set) {
		g_object_unref (result_set);
	}

	dbus_g_method_return (context, sum);

	tracker_dbus_request_success (request_id);

	return;
}


void
tracker_metadata_get_count (TrackerMetadata	   *object,
			    const gchar		   *service_type,
			    const gchar		   *field,
			    const gchar		   *query_condition,
			    DBusGMethodInvocation  *context,
			    GError		  **error)
{
	TrackerDBInterface *iface;
	TrackerDBResultSet *result_set = NULL;
	guint		    request_id;
	gint		    count;
	GSList		   *fields = NULL;
	TrackerFieldData   *def = NULL;

	GString		   *sql_select;
	GString		   *sql_from;
	GString		   *sql_where;
	gchar		   *sql;

	char		   *rdf_where;
	char		   *rdf_from;
	GError		   *actual_error = NULL;

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

	if (!tracker_ontology_service_is_valid (service_type)) {
		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     "Service '%s' is invalid or has not been implemented yet",
					     service_type);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	}

	iface = tracker_db_manager_get_db_interface_by_service (service_type);

	sql_select = g_string_new ("SELECT ");
	sql_from   = g_string_new ("\nFROM Services S ");
	sql_where  = g_string_new ("\nWHERE ");

	def = tracker_metadata_add_metadata_field (iface, service_type, &fields, field, FALSE, TRUE);

	if (!def) {
		g_string_free (sql_select, TRUE);
		g_string_free (sql_from, TRUE);
		g_string_free (sql_where, TRUE);

		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     "Invalid or non-existant metadata type '%s' specified",
					     field);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	}

	g_string_append_printf (sql_select, "COUNT (DISTINCT %s)", tracker_field_data_get_select_field (def));

	tracker_rdf_filter_to_sql (iface, query_condition, service_type,
				   &fields, &rdf_from, &rdf_where, &actual_error);

	if (actual_error) {

		g_string_free (sql_select, TRUE);
		g_string_free (sql_from, TRUE);
		g_string_free (sql_where, TRUE);

		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     NULL);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	}

	g_string_append_printf (sql_from, " %s ", rdf_from);
	g_string_append_printf (sql_where, " %s ", rdf_where);

	g_free (rdf_from);
	g_free (rdf_where);

	sql = g_strconcat (sql_select->str, " ", sql_from->str, " ", sql_where->str, NULL);

	g_string_free (sql_select, TRUE);
	g_string_free (sql_from, TRUE);
	g_string_free (sql_where, TRUE);

	g_slist_foreach (fields, (GFunc) g_object_unref, NULL);
	g_slist_free (fields);

	g_message ("Count query executed:\n%s", sql);

	result_set =  tracker_db_interface_execute_query (iface, NULL, sql);

	g_free (sql);

	tracker_db_result_set_get (result_set, 0, &count, -1);

	if (result_set) {
		g_object_unref (result_set);
	}

	dbus_g_method_return (context, count);

	tracker_dbus_request_success (request_id);

	return;
}

