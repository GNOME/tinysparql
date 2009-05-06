/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2007, Jason Kivlighn (jkivlighn@gmail.com)
 * Copyright (C) 2007, Creative Commons (http://creativecommons.org)
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
#include <stdlib.h>

#include <libtracker-common/tracker-dbus.h>
#include <libtracker-common/tracker-type-utils.h>
#include <libtracker-common/tracker-utils.h>

#include <libtracker-db/tracker-db-index.h>
#include <libtracker-db/tracker-db-interface-sqlite.h>
#include <libtracker-db/tracker-db-index-manager.h>
#include <libtracker-db/tracker-db-manager.h>

#include "tracker-data-manager.h"
#include "tracker-data-query.h"
#include "tracker-data-schema.h"
#include "tracker-data-search.h"
#include "tracker-query-tree.h"
#include "tracker-rdf-query.h"

#define DEFAULT_METADATA_MAX_HITS 1024

TrackerDBResultSet *
tracker_data_search_text (TrackerDBInterface *iface,
			  const gchar	     *service,
			  const gchar	     *search_string,
			  gint		      offset,
			  gint		      limit,
			  gboolean	      save_results,
			  gboolean	      detailed)
{
	TrackerQueryTree    *tree;
	TrackerDBResultSet  *result_set, *result;
	GArray		    *hits;
	gint		     count;
	const gchar	    *procedure;
	GArray		    *services = NULL;
	guint		     i = 0;

	g_return_val_if_fail (TRACKER_IS_DB_INTERFACE (iface), NULL);
	g_return_val_if_fail (service != NULL, NULL);
	g_return_val_if_fail (search_string != NULL, NULL);
	g_return_val_if_fail (offset >= 0, NULL);

	services = tracker_data_schema_create_service_array (service, FALSE);
	/* FIXME: Do we need both index and services here? We used to have it */
	tree = tracker_query_tree_new (search_string,
				       tracker_data_manager_get_config (),
				       tracker_data_manager_get_language (),
				       services);
	hits = tracker_query_tree_get_hits (tree, offset, limit);
	result = NULL;

	if (save_results) {
		tracker_db_interface_start_transaction (iface);
		tracker_data_manager_exec_proc (iface,
				      "DeleteSearchResults1",
				      NULL);
	}

	count = 0;

	for (i = 0; i < hits->len; i++) {
		TrackerDBIndexItemRank	rank;
		gchar		       *str_id;

		if (count >= limit) {
			break;
		}

		rank = g_array_index (hits, TrackerDBIndexItemRank, i);
		str_id = tracker_guint_to_string (rank.service_id);

		/* We save results into SearchResults table instead of
		 * returing an array of array of strings
		 */
		if (save_results) {
			gchar *str_score;

			str_score = tracker_gint_to_string (rank.score);
			tracker_data_manager_exec_proc (iface,
					      "InsertSearchResult1",
					      str_id,
					      str_score,
					      NULL);
			g_free (str_id);
			g_free (str_score);

			continue;
		}

		if (detailed) {
			if (strcmp (service, "Emails") == 0) {
				procedure = "GetEmailByID";
			} else if (strcmp (service, "Applications") == 0) {
				procedure = "GetApplicationByID";
			} else {
				procedure = "GetFileByID2";
			}
		} else {
			procedure = "GetFileByID";
		}

		result_set = tracker_data_manager_exec_proc (iface,
						   procedure,
						   str_id,
						   NULL);
		g_free (str_id);

		if (result_set) {
			gchar *path;
			guint  columns, t;

			tracker_db_result_set_get (result_set, 0, &path, -1);

				columns = tracker_db_result_set_get_n_columns (result_set);

				if (G_UNLIKELY (!result)) {
					guint lcolumns;

					lcolumns = tracker_db_result_set_get_n_columns (result_set);
					result = _tracker_db_result_set_new (lcolumns);
				}

				_tracker_db_result_set_append (result);

				for (t = 0; t < columns; t++) {
					GValue value = { 0, };

					_tracker_db_result_set_get_value (result_set, t, &value);
					_tracker_db_result_set_set_value (result, t, &value);
					g_value_unset (&value);
				}

			g_free (path);
			g_object_unref (result_set);
		}
	}

	if (save_results) {
		tracker_db_interface_end_transaction (iface);
	}

	g_object_unref (tree);
	g_array_free (hits, TRUE);

	if (!result) {
		return NULL;
	}

	if (tracker_db_result_set_get_n_rows (result) == 0) {
		g_object_unref (result);
		return NULL;
	}

	tracker_db_result_set_rewind (result);

	return result;
}

TrackerDBResultSet *
tracker_data_search_text_and_mime (TrackerDBInterface  *iface,
				   const gchar	       *text,
				   gchar	      **mime_array)
{
	TrackerQueryTree   *tree;
	TrackerDBResultSet *result_set1;
	GArray		   *hits;
	GArray		   *services;
	gint		    count = 0;
	guint		    i;

	g_return_val_if_fail (TRACKER_IS_DB_INTERFACE (iface), NULL);
	g_return_val_if_fail (text != NULL, NULL);
	g_return_val_if_fail (mime_array != NULL, NULL);

	result_set1 = NULL;
	services = tracker_data_schema_create_service_array (NULL, TRUE);

	tree = tracker_query_tree_new (text,
				       tracker_data_manager_get_config (),
				       tracker_data_manager_get_language (),
				       services);
	hits = tracker_query_tree_get_hits (tree, 0, 0);

	for (i = 0, count = 0; i < hits->len; i++) {
		TrackerDBResultSet     *result_set2;
		TrackerDBIndexItemRank	rank;
		gchar		       *str_id, *mimetype;

		rank = g_array_index (hits, TrackerDBIndexItemRank, i);

		str_id = tracker_guint_to_string (rank.service_id);
		result_set2 = tracker_data_manager_exec_proc (iface,
						    "GetFileByID",
						    str_id,
						    NULL);
		g_free (str_id);

		if (result_set2) {
			tracker_db_result_set_get (result_set2, 2, &mimetype, -1);

			if (tracker_string_in_string_list (mimetype, mime_array) != -1) {
				GValue value = { 0, };

				if (G_UNLIKELY (!result_set1)) {
					result_set1 = _tracker_db_result_set_new (2);
				}

				_tracker_db_result_set_append (result_set1);

				/* copy value in column 0 */
				_tracker_db_result_set_get_value (result_set2, 0, &value);
				_tracker_db_result_set_set_value (result_set1, 0, &value);
				g_value_unset (&value);

				/* copy value in column 1 */
				_tracker_db_result_set_get_value (result_set2, 1, &value);
				_tracker_db_result_set_set_value (result_set1, 1, &value);
				g_value_unset (&value);

				count++;
			}

			g_free (mimetype);
			g_object_unref (result_set2);
		}

		if (count > 2047) {
			g_warning ("Count is > 2047? Breaking for loop in %s, why?",
				   __FUNCTION__);
			break;
		}
	}

	g_object_unref (tree);
	g_array_free (hits, TRUE);

	if (!result_set1) {
		return NULL;
	}

	if (tracker_db_result_set_get_n_rows (result_set1) == 0) {
		g_object_unref (result_set1);
		return NULL;
	}

	tracker_db_result_set_rewind (result_set1);

	return result_set1;
}

TrackerDBResultSet *
tracker_data_search_text_and_location (TrackerDBInterface *iface,
				       const gchar	  *text,
				       const gchar	  *location)
{
	TrackerDBResultSet *result_set1;
	TrackerQueryTree   *tree;
	GArray		   *hits;
	GArray		   *services;
	gchar		   *location_prefix;
	gint		    count;
	guint		    i;

	g_return_val_if_fail (TRACKER_IS_DB_INTERFACE (iface), NULL);
	g_return_val_if_fail (text != NULL, NULL);
	g_return_val_if_fail (location != NULL, NULL);

	result_set1 = NULL;
	location_prefix = g_strconcat (location, G_DIR_SEPARATOR_S, NULL);
	services = tracker_data_schema_create_service_array (NULL, TRUE);

	tree = tracker_query_tree_new (text,
				       tracker_data_manager_get_config (),
				       tracker_data_manager_get_language (),
				       services);
	hits = tracker_query_tree_get_hits (tree, 0, 0);

	for (i = 0, count = 0; i < hits->len; i++) {
		TrackerDBResultSet     *result_set2;
		TrackerDBIndexItemRank	rank;
		gchar		       *str_id, *path;

		rank = g_array_index (hits, TrackerDBIndexItemRank, i);

		str_id = tracker_guint_to_string (rank.service_id);
		result_set2 = tracker_data_manager_exec_proc (iface,
						    "GetFileByID",
						    str_id,
						    NULL);
		g_free (str_id);

		if (result_set2) {
			tracker_db_result_set_get (result_set2, 0, &path, -1);

			if (g_str_has_prefix (path, location_prefix) ||
			    strcmp (path, location) == 0) {
				GValue value = { 0, };

				if (G_UNLIKELY (!result_set1)) {
					result_set1 = _tracker_db_result_set_new (2);
				}

				_tracker_db_result_set_append (result_set1);

				/* copy value in column 0 */
				_tracker_db_result_set_get_value (result_set2, 0, &value);
				_tracker_db_result_set_set_value (result_set1, 0, &value);
				g_value_unset (&value);

				/* copy value in column 1 */
				_tracker_db_result_set_get_value (result_set2, 1, &value);
				_tracker_db_result_set_set_value (result_set1, 1, &value);
				g_value_unset (&value);

				count++;
			}

			g_object_unref (result_set2);
		}

		if (count > 2047) {
			g_warning ("Count is > 2047? Breaking for loop in %s, why?",
				   __FUNCTION__);
			break;
		}
	}

	g_free (location_prefix);
	g_object_unref (tree);
	g_array_free (hits, TRUE);

	if (!result_set1) {
		return NULL;
	}

	if (tracker_db_result_set_get_n_rows (result_set1) == 0) {
		g_object_unref (result_set1);
		return NULL;
	}

	tracker_db_result_set_rewind (result_set1);

	return result_set1;
}

TrackerDBResultSet *
tracker_data_search_text_and_mime_and_location (TrackerDBInterface  *iface,
						const gchar	    *text,
						gchar		   **mime_array,
						const gchar	    *location)
{
	TrackerDBResultSet *result_set1;
	TrackerQueryTree   *tree;
	GArray		   *hits;
	GArray		   *services;
	gchar		   *location_prefix;
	gint		    count;
	guint		    i;

	g_return_val_if_fail (TRACKER_IS_DB_INTERFACE (iface), NULL);
	g_return_val_if_fail (text != NULL, NULL);
	g_return_val_if_fail (location != NULL, NULL);

	result_set1 = NULL;
	location_prefix = g_strconcat (location, G_DIR_SEPARATOR_S, NULL);
	services = tracker_data_schema_create_service_array (NULL, TRUE);

	tree = tracker_query_tree_new (text,
				       tracker_data_manager_get_config (),
				       tracker_data_manager_get_language (),
				       services);
	hits = tracker_query_tree_get_hits (tree, 0, 0);

	for (i = 0, count = 0; i < hits->len; i++) {
		TrackerDBResultSet     *result_set2;
		TrackerDBIndexItemRank	rank;
		gchar		       *str_id, *path, *mimetype;

		rank = g_array_index (hits, TrackerDBIndexItemRank, i);

		str_id = tracker_guint_to_string (rank.service_id);
		result_set2 = tracker_data_manager_exec_proc (iface,
						    "GetFileByID",
						    str_id,
						    NULL);
		g_free (str_id);

		if (result_set2) {
			tracker_db_result_set_get (result_set2,
						   0, &path,
						   2, &mimetype,
						   -1);

			if ((g_str_has_prefix (path, location_prefix) ||
			     strcmp (path, location) == 0) &&
			    tracker_string_in_string_list (mimetype, mime_array) != -1) {
				GValue value = { 0, };

				if (G_UNLIKELY (!result_set1)) {
					result_set1 = _tracker_db_result_set_new (2);
				}

				_tracker_db_result_set_append (result_set1);

				/* copy value in column 0 */
				_tracker_db_result_set_get_value (result_set2, 0, &value);
				_tracker_db_result_set_set_value (result_set1, 0, &value);
				g_value_unset (&value);

				/* copy value in column 1 */
				_tracker_db_result_set_get_value (result_set2, 1, &value);
				_tracker_db_result_set_set_value (result_set1, 1, &value);
				g_value_unset (&value);

				count++;
			}

			g_free (path);
			g_free (mimetype);
			g_object_unref (result_set2);
		}

		if (count > 2047) {
			g_warning ("Count is > 2047? Breaking for loop in %s, why?",
				   __FUNCTION__);
			break;
		}
	}

	g_free (location_prefix);
	g_object_unref (tree);
	g_array_free (hits, TRUE);

	if (!result_set1) {
		return NULL;
	}

	if (tracker_db_result_set_get_n_rows (result_set1) == 0) {
		g_object_unref (result_set1);
		return NULL;
	}

	tracker_db_result_set_rewind (result_set1);

	return result_set1;
}

gchar **
tracker_data_search_files_get (TrackerDBInterface *iface,
			       const gchar	  *folder_path)
{
	TrackerDBResultSet *result_set;
	GPtrArray	   *array;

	g_return_val_if_fail (TRACKER_IS_DB_INTERFACE (iface), NULL);
	g_return_val_if_fail (folder_path != NULL, NULL);

	result_set = tracker_data_manager_exec_proc (iface,
						     "GetFileChildren",
						     folder_path,
						     NULL);
	array = g_ptr_array_new ();

	if (result_set) {
		gchar	 *name, *prefix;
		gboolean  valid = TRUE;

		while (valid) {
			tracker_db_result_set_get (result_set,
						   1, &prefix,
						   2, &name,
						   -1);

			g_ptr_array_add (array, g_build_filename (prefix, name, NULL));

			g_free (prefix);
			g_free (name);
			valid = tracker_db_result_set_iter_next (result_set);
		}

		g_object_unref (result_set);
	}

	g_ptr_array_add (array, NULL);

	return (gchar**) g_ptr_array_free (array, FALSE);
}

TrackerDBResultSet *
tracker_data_search_files_get_by_service (TrackerDBInterface *iface,
					  const gchar	     *service,
					  gint		      offset,
					  gint		      limit)
{
	TrackerDBResultSet *result_set;
	gchar		   *str_limit;
	gchar		   *str_offset;

	g_return_val_if_fail (TRACKER_IS_DB_INTERFACE (iface), NULL);
	g_return_val_if_fail (service != NULL, NULL);

	str_limit = tracker_gint_to_string (limit);
	str_offset = tracker_gint_to_string (offset);

	result_set = tracker_data_manager_exec_proc (iface,
					   "GetByServiceType",
					   service,
					   service,
					   str_offset,
					   str_limit,
					   NULL);

	g_free (str_offset);
	g_free (str_limit);

	return result_set;
}

TrackerDBResultSet *
tracker_data_search_files_get_by_mime (TrackerDBInterface  *iface,
				       gchar		  **mimes,
				       gint		    n,
				       gint		    offset,
				       gint		    limit,
				       gboolean		    vfs)
{
	TrackerDBResultSet *result_set;
	gint		    i;
	const gchar	   *service;
	gchar		   *query;
	GString		   *str;

	g_return_val_if_fail (TRACKER_IS_DB_INTERFACE (iface), NULL);
	g_return_val_if_fail (mimes != NULL, NULL);
	g_return_val_if_fail (offset >= 0, NULL);

	if (vfs) {
		service = "VFS";
	} else {
		service = "Files";
	}

	str = g_string_new ("SELECT DISTINCT S.Path || '/' || S.Name AS uri "
			    "FROM Services AS S "
			    "INNER JOIN ServiceKeywordMetaData AS M "
			    "ON "
			    "S.ID = M.ServiceID "
			    "WHERE "
			    "S.Enabled = 1 "
			    "AND "
			    "(S.AuxilaryID = 0 OR S.AuxilaryID IN (SELECT VolumeID FROM Volumes WHERE Enabled = 1)) "
			    "AND "
			    "(M.MetaDataID = (SELECT ID FROM MetaDataTypes WHERE MetaName ='File:Mime')) "
			    "AND "
			    "(M.MetaDataValue IN ");

	g_string_append_printf (str, "('%s'", mimes[0]);

	for (i = 1; i < n; i++) {
		g_string_append_printf (str, ", '%s'", mimes[i]);
	}

	g_string_append (str, ")) ");

	g_string_append_printf (str,
				"AND "
				"(S.ServiceTypeID IN (SELECT TypeId FROM ServiceTypes WHERE TypeName = '%s' OR Parent = '%s')) "
				"LIMIT %d,%d",
				service,
				service,
				offset,
				limit);

	query = g_string_free (str, FALSE);
	result_set = tracker_db_interface_execute_query (iface, NULL, "%s", query);
	g_free (query);

	return result_set;
}

TrackerDBResultSet *
tracker_data_search_keywords_get_list (TrackerDBInterface *iface,
				       const gchar	  *service)
{
	g_return_val_if_fail (TRACKER_IS_DB_INTERFACE (iface), NULL);
	g_return_val_if_fail (service != NULL, NULL);

	return tracker_data_manager_exec_proc (iface,
				     "GetKeywordList",
				     service,
				     service,
				     NULL);
}


static gint
metadata_sanity_check_max_hits (gint max_hits)
{
	if (max_hits < 1) {
		return DEFAULT_METADATA_MAX_HITS;
	}

	return max_hits;
}

static gboolean
is_data_type_numeric (TrackerFieldType type) 
{
	return 
		type == TRACKER_FIELD_TYPE_INTEGER ||
		type == TRACKER_FIELD_TYPE_DOUBLE;
}

static gboolean
is_data_type_text (TrackerFieldType type) 
{
	return 
		type == TRACKER_FIELD_TYPE_STRING ||
		type == TRACKER_FIELD_TYPE_INDEX;
}

static TrackerFieldData *
tracker_metadata_add_metadata_field (TrackerDBInterface *iface,
				     const gchar        *service,
				     GSList	       **fields,
				     const gchar        *field_name,
				     gboolean		 is_select,
				     gboolean		 is_condition,
				     gboolean            is_order)
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

			if (is_select) {
				tracker_field_data_set_is_select (field_data, TRUE);
			}

			if (is_order) {
				tracker_field_data_set_is_order (field_data, TRUE);
			}
			
			break;			
		}
	}

	if (!field_exists) {
		field_data = tracker_data_schema_get_metadata_field (iface,
								     service,
								     field_name,
								     g_slist_length (*fields),
								     is_select,
								     is_condition);
		if (field_data) {
			*fields = g_slist_prepend (*fields, field_data);
		}

		if (is_order) {
			tracker_field_data_set_is_order (field_data, TRUE);
		}
	}

	return field_data;
}

TrackerDBResultSet *
tracker_data_search_get_unique_values (const gchar  *service_type,
				       gchar	   **fields,
				       const gchar  *query_condition,
				       gboolean	     order_desc,
				       gint	     offset,
				       gint	     max_hits,
				       GError	   **error)
{
	return tracker_data_search_get_unique_values_with_concat_count_and_sum (service_type,
										fields,
										query_condition,
										NULL,
										NULL,
										NULL,
										order_desc,
										offset,
										max_hits,
										error);
}

TrackerDBResultSet *
tracker_data_search_get_unique_values_with_count (const gchar  *service_type,
						  gchar	      **fields,
						  const gchar  *query_condition,
						  const gchar  *count_field,
						  gboolean      order_desc,
						  gint	        offset,
						  gint	        max_hits,
						  GError      **error)
{
	return tracker_data_search_get_unique_values_with_concat_count_and_sum (service_type,
										fields,
										query_condition,
										NULL,
										count_field,
										NULL,
										order_desc,
										offset,
										max_hits,
										error);	
}

TrackerDBResultSet *
tracker_data_search_get_unique_values_with_count_and_sum (const gchar	      *service_type,
							  gchar		     **fields,
							  const gchar	      *query_condition,
							  const gchar	      *count_field,
							  const gchar         *sum_field,
							  gboolean	       order_desc,
							  gint		       offset,
							  gint		       max_hits,
							  GError	     **error)
{
	return tracker_data_search_get_unique_values_with_concat_count_and_sum (service_type,
										fields,
										query_condition,
										NULL,
										count_field,
										sum_field,
										order_desc,
										offset,
										max_hits,
										error);
}

TrackerDBResultSet *
tracker_data_search_get_unique_values_with_concat_count_and_sum (const gchar	      *service_type,
								 gchar		     **fields,
								 const gchar	      *query_condition,
								 const gchar          *concat_field,
								 const gchar	      *count_field,
								 const gchar          *sum_field,
								 gboolean	       order_desc,
								 gint		       offset,
								 gint		       max_hits,
								 GError	             **error)
{
	TrackerDBInterface *iface;
	TrackerDBResultSet *result_set = NULL;

	GSList		   *field_list = NULL;
	gchar		   *str_offset, *str_limit;

	GString		   *sql_select;
	GString		   *sql_from;
	GString		   *sql_where;
	GString		   *sql_order;
	GString		   *sql_group;
	gchar		   *sql;

	gchar		   *rdf_where;
	gchar		   *rdf_from;
	GError		   *actual_error = NULL;

	guint		    i;

	g_return_val_if_fail (service_type != NULL, NULL);
	g_return_val_if_fail (fields != NULL, NULL);
	g_return_val_if_fail (query_condition != NULL, NULL);

	if (!tracker_ontology_service_is_valid (service_type)) {
		g_set_error (error, TRACKER_DBUS_ERROR, 0,
			     "Service_Type '%s' is invalid or has not been implemented yet",
			     service_type);
		return NULL;
	}

	iface = tracker_db_manager_get_db_interface_by_service (service_type);

	sql_select = g_string_new ("SELECT DISTINCT ");
	sql_from   = g_string_new ("\nFROM Services AS S ");
	sql_where  = g_string_new ("\nWHERE ");
	sql_order  = g_string_new ("");
	sql_group  = g_string_new ("\nGROUP BY ");


	for (i = 0; i < g_strv_length (fields); i++) {
		TrackerFieldData *fd;

		fd = tracker_metadata_add_metadata_field (iface, service_type, &field_list, fields[i], TRUE, FALSE, TRUE);

		if (!fd) {
			g_string_free (sql_select, TRUE);
			g_string_free (sql_from, TRUE);
			g_string_free (sql_where, TRUE);
			g_string_free (sql_order, TRUE);
			g_string_free (sql_group, TRUE);

			g_set_error (error, TRACKER_DBUS_ERROR, 0,
				     "Invalid or non-existant metadata type '%s' specified",
				     fields[i]);
			return NULL;
		}

		if (i) {
			g_string_append_printf (sql_select, ",");
			g_string_append_printf (sql_group, ",");
		}

		g_string_append_printf (sql_select, "%s", tracker_field_data_get_select_field (fd));
		if (order_desc) {
			if (i) {
				g_string_append_printf (sql_order, ",");
			}
			g_string_append_printf (sql_order, "\nORDER BY %s DESC ",
						tracker_field_data_get_order_field (fd));
		}
		g_string_append_printf (sql_group, "%s", tracker_field_data_get_order_field (fd));

	}

	if (concat_field && !(tracker_is_empty_string (concat_field))) {
		TrackerFieldData *fd;
		TrackerFieldType  data_type;

		fd = tracker_metadata_add_metadata_field (iface, service_type, &field_list, concat_field, TRUE, FALSE, FALSE);

		if (!fd) {
			g_string_free (sql_select, TRUE);
			g_string_free (sql_from, TRUE);
			g_string_free (sql_where, TRUE);
			g_string_free (sql_order, TRUE);
			g_string_free (sql_group, TRUE);

			g_set_error (error, TRACKER_DBUS_ERROR, 0,
				     "Invalid or non-existant metadata type '%s' specified",
				     sum_field);
			return NULL;
		}

		data_type = tracker_field_data_get_data_type (fd);

		if (!is_data_type_text (data_type)) {
			g_string_free (sql_select, TRUE);
			g_string_free (sql_from, TRUE);
			g_string_free (sql_where, TRUE);
			g_string_free (sql_order, TRUE);
			g_string_free (sql_group, TRUE);

			g_set_error (error, TRACKER_DBUS_ERROR, 0,
				     "Cannot concatenate '%s': this metadata type is not text",
				     sum_field);
			return NULL;
		}

		g_string_append_printf (sql_select, ", GROUP_CONCAT (DISTINCT %s)", tracker_field_data_get_select_field (fd));
	}

	if (count_field && !(tracker_is_empty_string (count_field))) {
		TrackerFieldData *fd;

		if (strcmp (count_field, "*")) {
			fd = tracker_metadata_add_metadata_field (iface, service_type, &field_list, count_field,
								  TRUE, FALSE, FALSE);
			
			if (!fd) {
				g_string_free (sql_select, TRUE);
				g_string_free (sql_from, TRUE);
				g_string_free (sql_where, TRUE);
				g_string_free (sql_order, TRUE);
				g_string_free (sql_group, TRUE);
				
				g_set_error (error, TRACKER_DBUS_ERROR, 0,
					     "Invalid or non-existant metadata type '%s' specified",
					     count_field);
				return NULL;
			}
			
			g_string_append_printf (sql_select, ", COUNT (DISTINCT %s)", tracker_field_data_get_select_field (fd));
		} else {
				g_string_append_printf (sql_select, ", COUNT (DISTINCT S.ID)");		
		}
	}

	if (sum_field && !(tracker_is_empty_string (sum_field))) {
		TrackerFieldData *fd;
		TrackerFieldType  data_type;

		fd = tracker_metadata_add_metadata_field (iface, service_type, &field_list, sum_field, TRUE, FALSE, FALSE);

		if (!fd) {
			g_string_free (sql_select, TRUE);
			g_string_free (sql_from, TRUE);
			g_string_free (sql_where, TRUE);
			g_string_free (sql_order, TRUE);
			g_string_free (sql_group, TRUE);

			g_set_error (error, TRACKER_DBUS_ERROR, 0,
				     "Invalid or non-existant metadata type '%s' specified",
				     sum_field);
			return NULL;
		}

		data_type = tracker_field_data_get_data_type (fd);

		if (!is_data_type_numeric (data_type)) {
			g_string_free (sql_select, TRUE);
			g_string_free (sql_from, TRUE);
			g_string_free (sql_where, TRUE);
			g_string_free (sql_order, TRUE);
			g_string_free (sql_group, TRUE);

			g_set_error (error, TRACKER_DBUS_ERROR, 0,
				     "Cannot sum '%s': this metadata type is not numeric",
				     sum_field);
			return NULL;
		}

		g_string_append_printf (sql_select, ", SUM (%s)", tracker_field_data_get_select_field (fd));
	}

	tracker_rdf_filter_to_sql (iface, query_condition, service_type,
				   &field_list, &rdf_from, &rdf_where, &actual_error);

	if (actual_error) {
		g_string_free (sql_select, TRUE);
		g_string_free (sql_from, TRUE);
		g_string_free (sql_where, TRUE);
		g_string_free (sql_order, TRUE);
		g_string_free (sql_group, TRUE);

		g_propagate_error (error, actual_error);

		return NULL;
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

	result_set =  tracker_db_interface_execute_query (iface, NULL, "%s", sql);

	g_free (sql);

	return result_set;
}

gint
tracker_data_search_get_sum (const gchar	 *service_type,
			     const gchar	 *field,
			     const gchar	 *query_condition,
			     GError		**error)
{
	TrackerDBInterface *iface;
	TrackerDBResultSet *result_set = NULL;

	gint		    sum;
	GSList		   *fields = NULL;
	TrackerFieldData   *fd = NULL;
	TrackerFieldType    data_type;
	GString		   *sql_select;
	GString		   *sql_from;
	GString		   *sql_where;
	gchar		   *sql;

	char		   *rdf_where;
	char		   *rdf_from;
	GError		   *actual_error = NULL;

	g_return_val_if_fail (service_type != NULL, 0);
	g_return_val_if_fail (field != NULL, 0);
	g_return_val_if_fail (query_condition != NULL, 0);

	if (!tracker_ontology_service_is_valid (service_type)) {
		g_set_error (error, TRACKER_DBUS_ERROR, 0,
			     "Service_Type '%s' is invalid or has not been implemented yet",
			     service_type);
		return 0;
	}

	iface = tracker_db_manager_get_db_interface_by_service (service_type);

	sql_select = g_string_new ("SELECT ");
	sql_from   = g_string_new ("\nFROM Services AS S ");
	sql_where  = g_string_new ("\nWHERE ");

	fd = tracker_metadata_add_metadata_field (iface, service_type, &fields, field, TRUE, FALSE, FALSE);

	if (!fd) {
		g_string_free (sql_select, TRUE);
		g_string_free (sql_from, TRUE);
		g_string_free (sql_where, TRUE);

		g_set_error (error, TRACKER_DBUS_ERROR, 0,
			     "Invalid or non-existant metadata type '%s' specified",
			     field);
		return 0;
	}

	data_type = tracker_field_data_get_data_type (fd);
	if (!is_data_type_numeric (data_type)) {
		g_string_free (sql_select, TRUE);
		g_string_free (sql_from, TRUE);
		g_string_free (sql_where, TRUE);

		g_set_error (error, TRACKER_DBUS_ERROR, 0,
			     "Cannot sum '%s': this metadata type is not numeric",
			     field);
		return 0;
	}

	g_string_append_printf (sql_select, "SUM (%s)", tracker_field_data_get_select_field (fd));

	tracker_rdf_filter_to_sql (iface, query_condition, service_type,
				   &fields, &rdf_from, &rdf_where, &actual_error);

	if (actual_error) {

		g_string_free (sql_select, TRUE);
		g_string_free (sql_from, TRUE);
		g_string_free (sql_where, TRUE);

		g_propagate_error (error, actual_error);
		return 0;
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

	result_set =  tracker_db_interface_execute_query (iface, NULL, "%s", sql);

	g_free (sql);

	tracker_db_result_set_get (result_set, 0, &sum, -1);

	if (result_set) {
		g_object_unref (result_set);
	}

	return sum;
}


gint
tracker_data_search_get_count (const gchar	   *service_type,
			       const gchar	   *field,
			       const gchar	   *query_condition,
			       GError		  **error)
{
	TrackerDBInterface *iface;
	TrackerDBResultSet *result_set = NULL;
	gint		    count;
	GSList		   *fields = NULL;
	TrackerFieldData   *fd;

	GString		   *sql_select;
	GString		   *sql_from;
	GString		   *sql_where;
	gchar		   *sql;

	char		   *rdf_where;
	char		   *rdf_from;
	GError		   *actual_error = NULL;

	g_return_val_if_fail (service_type != NULL, 0);
	g_return_val_if_fail (field != NULL, 0);
	g_return_val_if_fail (query_condition != NULL, 0);

	if (!tracker_ontology_service_is_valid (service_type)) {
		g_set_error (error, TRACKER_DBUS_ERROR, 0,
			     "Service_Type '%s' is invalid or has not been implemented yet",
			     service_type);
		return 0;
	}

	iface = tracker_db_manager_get_db_interface_by_service (service_type);

	sql_select = g_string_new ("SELECT ");
	sql_from   = g_string_new ("\nFROM Services AS S ");
	sql_where  = g_string_new ("\nWHERE ");

	if (strcmp (field, "*")) {
		fd = tracker_metadata_add_metadata_field (iface, service_type, &fields, field, TRUE, FALSE, FALSE);
		
		if (!fd) {
			g_string_free (sql_select, TRUE);
			g_string_free (sql_from, TRUE);
			g_string_free (sql_where, TRUE);
			
			g_set_error (error, TRACKER_DBUS_ERROR, 0,
				     "Invalid or non-existant metadata type '%s' specified",
				     field);
			return 0;
		}

		g_string_append_printf (sql_select, "COUNT (DISTINCT %s)", tracker_field_data_get_select_field (fd));
	} else {
		g_string_append_printf (sql_select, "COUNT (DISTINCT S.ID)");
	}

	tracker_rdf_filter_to_sql (iface, query_condition, service_type,
				   &fields, &rdf_from, &rdf_where, &actual_error);

	if (actual_error) {

		g_string_free (sql_select, TRUE);
		g_string_free (sql_from, TRUE);
		g_string_free (sql_where, TRUE);

		g_propagate_error (error, actual_error);
		return 0;
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

	result_set =  tracker_db_interface_execute_query (iface, NULL, "%s", sql);

	g_free (sql);

	tracker_db_result_set_get (result_set, 0, &count, -1);

	if (result_set) {
		g_object_unref (result_set);
	}

	return count;
}

TrackerDBResultSet *
tracker_data_search_get_unique_values_with_aggregates (const gchar	      *service_type,
						       gchar		     **fields,
						       const gchar	      *query_condition,
						       gchar                 **aggregates,
						       gchar	             **aggregate_fields,
						       gboolean	               order_desc,
						       gint		       offset,
						       gint		       max_hits,
						       GError	             **error)
{
	TrackerDBInterface *iface;
	TrackerDBResultSet *result_set = NULL;

	GSList		   *field_list = NULL;
	gchar		   *str_offset, *str_limit;

	GString		   *sql_select;
	GString		   *sql_from;
	GString		   *sql_where;
	GString		   *sql_order;
	GString		   *sql_group;
	gchar		   *sql;

	gchar		   *rdf_where;
	gchar		   *rdf_from;
	GError		   *actual_error = NULL;

	guint		    i;

	g_return_val_if_fail (service_type != NULL, NULL);
	g_return_val_if_fail (fields != NULL, NULL);
	g_return_val_if_fail (query_condition != NULL, NULL);

	if (!tracker_ontology_service_is_valid (service_type)) {
		g_set_error (error, TRACKER_DBUS_ERROR, 0,
			     "Service_Type '%s' is invalid or has not been implemented yet",
			     service_type);
		return NULL;
	}

	if (g_strv_length (aggregates) != g_strv_length (aggregate_fields)) {
		g_set_error (error, TRACKER_DBUS_ERROR, 0,
			     "The number of aggregates and aggregate fields do not match");
		return NULL;	
	}

	iface = tracker_db_manager_get_db_interface_by_service (service_type);

	sql_select = g_string_new ("SELECT DISTINCT ");
	sql_from   = g_string_new ("\nFROM Services AS S ");
	sql_where  = g_string_new ("\nWHERE ");
	sql_order  = g_string_new ("");
	sql_group  = g_string_new ("\nGROUP BY ");


	for (i = 0; i < g_strv_length (fields); i++) {
		TrackerFieldData *fd;

		fd = tracker_metadata_add_metadata_field (iface, service_type, &field_list, fields[i], TRUE, FALSE, TRUE);

		if (!fd) {
			g_string_free (sql_select, TRUE);
			g_string_free (sql_from, TRUE);
			g_string_free (sql_where, TRUE);
			g_string_free (sql_order, TRUE);
			g_string_free (sql_group, TRUE);

			g_slist_foreach (field_list, (GFunc) g_object_unref, NULL);
			g_slist_free (field_list);

			g_set_error (error, TRACKER_DBUS_ERROR, 0,
				     "Invalid or non-existant metadata type '%s' specified",
				     fields[i]);
			return NULL;
		}

		if (i) {
			g_string_append_printf (sql_select, ",");
			g_string_append_printf (sql_group, ",");
		}

		g_string_append_printf (sql_select, "%s", tracker_field_data_get_select_field (fd));
		if (order_desc) {
			if (i) {
				g_string_append_printf (sql_order, ",");
			}
			g_string_append_printf (sql_order, "\nORDER BY %s DESC ",
						tracker_field_data_get_order_field (fd));
		}
		g_string_append_printf (sql_group, "%s", tracker_field_data_get_order_field (fd));

	}

	for (i = 0; i < g_strv_length (aggregates); i++) {
		if (strcmp (aggregates[i],"COUNT") == 0) {
			if (!(tracker_is_empty_string (aggregate_fields[i]))) {
				TrackerFieldData *fd;
				
				if (strcmp (aggregate_fields[i], "*")) {
					fd = tracker_metadata_add_metadata_field (iface, service_type,
										  &field_list, aggregate_fields[i],
										  TRUE, FALSE, FALSE);
					
					if (!fd) {
						g_string_free (sql_select, TRUE);
						g_string_free (sql_from, TRUE);
						g_string_free (sql_where, TRUE);
						g_string_free (sql_order, TRUE);
						g_string_free (sql_group, TRUE);

						g_slist_foreach (field_list, (GFunc) g_object_unref, NULL);
						g_slist_free (field_list);
						
						g_set_error (error, TRACKER_DBUS_ERROR, 0,
							     "Invalid or non-existant metadata type '%s' specified",
							     aggregate_fields[i]);
						return NULL;
					}
					
					g_string_append_printf (sql_select, ", COUNT (DISTINCT %s)",
								tracker_field_data_get_select_field (fd));
				} else {
					g_string_append_printf (sql_select, ", COUNT (DISTINCT S.ID)");		
				}
			}
		} else if (strcmp (aggregates[i], "SUM") == 0) {
			if ( !(tracker_is_empty_string (aggregate_fields[i])) ) {
				TrackerFieldData *fd;
				TrackerFieldType  data_type;
				
				fd = tracker_metadata_add_metadata_field (iface, service_type, 
									  &field_list, aggregate_fields[i],
									  TRUE, FALSE, FALSE);
				
				if (!fd) {
					g_string_free (sql_select, TRUE);
					g_string_free (sql_from, TRUE);
					g_string_free (sql_where, TRUE);
					g_string_free (sql_order, TRUE);
					g_string_free (sql_group, TRUE);
					
					g_slist_foreach (field_list, (GFunc) g_object_unref, NULL);
					g_slist_free (field_list);

					g_set_error (error, TRACKER_DBUS_ERROR, 0,
						     "Invalid or non-existant metadata type '%s' specified",
						     aggregate_fields[i]);
					return NULL;
				}
				
				data_type = tracker_field_data_get_data_type (fd);
				
				if (!is_data_type_numeric (data_type)) {
					g_string_free (sql_select, TRUE);
					g_string_free (sql_from, TRUE);
					g_string_free (sql_where, TRUE);
					g_string_free (sql_order, TRUE);
					g_string_free (sql_group, TRUE);
					
					g_slist_foreach (field_list, (GFunc) g_object_unref, NULL);
					g_slist_free (field_list);

					g_set_error (error, TRACKER_DBUS_ERROR, 0,
						     "Cannot sum '%s': this metadata type is not numeric",
						     aggregate_fields[i]);
					return NULL;
				}
				
				g_string_append_printf (sql_select, ", SUM (%s)",
							tracker_field_data_get_select_field (fd));
			}
		} else if (strcmp (aggregates[i], "CONCAT") == 0 ) { 
			if (!(tracker_is_empty_string (aggregate_fields[i]))) {
				TrackerFieldData *fd;
				TrackerFieldType  data_type;
				
				fd = tracker_metadata_add_metadata_field (iface, service_type,
									  &field_list, aggregate_fields[i],
									  TRUE, FALSE, FALSE);
				
				if (!fd) {
					g_string_free (sql_select, TRUE);
					g_string_free (sql_from, TRUE);
					g_string_free (sql_where, TRUE);
					g_string_free (sql_order, TRUE);
					g_string_free (sql_group, TRUE);
					
					g_slist_foreach (field_list, (GFunc) g_object_unref, NULL);
					g_slist_free (field_list);

					g_set_error (error, TRACKER_DBUS_ERROR, 0,
						     "Invalid or non-existant metadata type '%s' specified",
						     aggregate_fields[i]);
					return NULL;
				}
				
				data_type = tracker_field_data_get_data_type (fd);
				
				if (!is_data_type_text (data_type)) {
					g_string_free (sql_select, TRUE);
					g_string_free (sql_from, TRUE);
					g_string_free (sql_where, TRUE);
					g_string_free (sql_order, TRUE);
					g_string_free (sql_group, TRUE);
					
					g_slist_foreach (field_list, (GFunc) g_object_unref, NULL);
					g_slist_free (field_list);

					g_set_error (error, TRACKER_DBUS_ERROR, 0,
						     "Cannot concatenate '%s': this metadata type is not text",
						     aggregate_fields[i]);
					return NULL;
				}
				
				g_string_append_printf (sql_select, ", GROUP_CONCAT (DISTINCT %s)",
							tracker_field_data_get_select_field (fd));
			}
		} else {
			g_string_free (sql_select, TRUE);
			g_string_free (sql_from, TRUE);
			g_string_free (sql_where, TRUE);
			g_string_free (sql_order, TRUE);
			g_string_free (sql_group, TRUE);

			g_slist_foreach (field_list, (GFunc) g_object_unref, NULL);
			g_slist_free (field_list);

			g_debug ("Got an unknown operation %s", aggregates[i]);
			
			g_set_error (error, TRACKER_DBUS_ERROR, 0,
				     "Aggregate operation %s not found",
				     aggregates[i]);
			return NULL;
		}
	}
	
	tracker_rdf_filter_to_sql (iface, query_condition, service_type,
				   &field_list, &rdf_from, &rdf_where, &actual_error);

	if (actual_error) {
		g_string_free (sql_select, TRUE);
		g_string_free (sql_from, TRUE);
		g_string_free (sql_where, TRUE);
		g_string_free (sql_order, TRUE);
		g_string_free (sql_group, TRUE);

		g_slist_foreach (field_list, (GFunc) g_object_unref, NULL);
		g_slist_free (field_list);

		g_propagate_error (error, actual_error);

		return NULL;
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

	result_set =  tracker_db_interface_execute_query (iface, NULL, "%s", sql);

	g_free (sql);

	return result_set;
}

TrackerDBResultSet *
tracker_data_search_metadata_in_path (const gchar	       *path,
				      gchar		      **fields,
				      GError		      **error)
{
	TrackerDBInterface *iface;
	TrackerDBResultSet *result_set;
	TrackerField	   *fds[255];
	guint		    i;
	gchar		   *uri_filtered;
	guint32		    file_id;
	GString		   *sql;
	gboolean	    needs_join[255];
	gchar		   *query;

	g_return_val_if_fail (path != NULL, NULL);
	g_return_val_if_fail (fields != NULL, NULL);
	g_return_val_if_fail (g_strv_length (fields) > 0, NULL);

	/* Get fields for metadata list provided */
	for (i = 0; i < g_strv_length (fields); i++) {
		fds[i] = tracker_ontology_get_field_by_name (fields[i]);

		if (!fds[i]) {
			g_set_error (error, TRACKER_DBUS_ERROR, 0,
				     "Metadata field '%s' was not found",
				     fields[i]);
			return NULL;
		}

	}
	fds [g_strv_length (fields)] = NULL;


	if (g_str_has_suffix (path, G_DIR_SEPARATOR_S)) {
		/* Remove trailing 'G_DIR_SEPARATOR' */
		uri_filtered = g_strndup (path, strlen (path) - 1);
	} else {
		uri_filtered = g_strdup (path);
	}

	iface = tracker_db_manager_get_db_interface_by_service (TRACKER_DB_FOR_FILE_SERVICE);

	/* Get file ID in database */
	file_id = tracker_data_query_file_id (NULL, uri_filtered);
	if (file_id == 0) {
		g_free (uri_filtered);
		g_set_error (error, TRACKER_DBUS_ERROR, 0,
			     "File or directory was not in database, path:'%s'",
			     path);
		return NULL;
	}

	/* Build SELECT clause */
	sql = g_string_new (" ");
	g_string_append_printf (sql,
				"SELECT (S.Path || '%s' || S.Name) as PathName ",
				G_DIR_SEPARATOR_S);

	for (i = 1; i <= g_strv_length (fields); i++) {
		gchar *field;

		field = tracker_data_schema_get_field_name ("Files", fields[i-1]);

		if (field) {
			g_string_append_printf (sql, ", S.%s ", field);
			g_free (field);
			needs_join[i - 1] = FALSE;
		} else {
			gchar *display_field;

			display_field = tracker_ontology_field_get_display_name (fds[i-1]);
			g_string_append_printf (sql, ", M%d.%s ", i, display_field);
			g_free (display_field);
			needs_join[i - 1] = TRUE;
		}
	}

	/* Build FROM clause */
	g_string_append (sql,
			 " FROM Services AS S ");

	for (i = 0; i < g_strv_length (fields); i++) {
		const gchar *table;

		if (!needs_join[i]) {
			continue;
		}

		table = tracker_data_schema_metadata_field_get_db_table (tracker_field_get_data_type (fds[i]));

		g_string_append_printf (sql,
					" LEFT OUTER JOIN %s M%d ON "
					"S.ID = M%d.ServiceID AND "
					"M%d.MetaDataID = %s ",
					table,
					i + 1,
					i + 1,
					i + 1,
					tracker_field_get_id (fds[i]));
	}

	/* Build WHERE clause */
	g_string_append_printf (sql,
				" "
				"WHERE "
				"S.Path = '%s' "
				"AND "
				"S.Enabled = 1 "
				"AND "
				"(S.AuxilaryID = 0 OR S.AuxilaryID IN (SELECT VolumeID FROM Volumes WHERE Enabled = 1)) ",
				uri_filtered);

	g_free (uri_filtered);

	query = g_string_free (sql, FALSE);
	result_set = tracker_db_interface_execute_query (iface, NULL, "%s", query);

	g_free (query);

	return result_set;
}

TrackerDBResultSet *
tracker_data_search_keywords (const gchar	*service_type,
			      const gchar      **keywords,
			      gint		 offset,
			      gint		 max_hits,
			      GError	       **error)
{
	TrackerDBInterface  *iface;
	TrackerDBResultSet  *result_set;
	const gchar	   **p;
	GString		    *search;
	GString		    *lselect;
	GString		    *where;
	gchar		    *related_metadata;
	gchar		    *query;

	g_return_val_if_fail (service_type != NULL, NULL);
	g_return_val_if_fail (keywords != NULL, NULL);
	g_return_val_if_fail (keywords[0] != NULL, NULL);

	if (!tracker_ontology_service_is_valid (service_type)) {
		g_set_error (error, TRACKER_DBUS_ERROR, 0,
			     "Service_Type '%s' is invalid or has not been implemented yet",
			     service_type);
		return NULL;
	}

	iface = tracker_db_manager_get_db_interface_by_service (service_type);

	/* Sanity check values */
	offset = MAX (offset, 0);

	/* Create keyword search string */
	search = g_string_new ("");
	g_string_append_printf (search, "'%s'", keywords[0]);

	for (p = keywords + 1; *p; p++) {
		g_string_append_printf (search, ", '%s'", *p);
	}

	/* Create select string */
	lselect = g_string_new (" Select distinct S.Path || '");
	lselect = g_string_append (lselect, G_DIR_SEPARATOR_S);
	lselect = g_string_append (lselect,
				   "' || S.Name as EntityName from Services AS S, ServiceKeywordMetaData AS M ");

	/* Create where string */
	related_metadata = tracker_data_schema_metadata_field_get_related_names (iface, "User:Keywords");

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
	query = g_strconcat (lselect->str, where->str, NULL);
	g_string_free (lselect, TRUE);
	g_string_free (where, TRUE);

	g_debug ("%s", query);

	result_set = tracker_db_interface_execute_query (iface, NULL, "%s", query);

	g_free (query);

	return result_set;
}

