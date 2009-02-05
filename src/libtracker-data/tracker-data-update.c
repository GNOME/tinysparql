/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia (urho.konttori@nokia.com)
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

#include <libtracker-common/tracker-type-utils.h>
#include <libtracker-common/tracker-file-utils.h>

#include <libtracker-db/tracker-db-index-manager.h>
#include <libtracker-db/tracker-db-manager.h>
#include <libtracker-db/tracker-db-dbus.h>

#include "tracker-data-manager.h"
#include "tracker-data-update.h"
#include "tracker-data-query.h"

typedef struct {
	TrackerService *service;
	guint32 iid_value;
	TrackerLanguage *language;
	TrackerConfig *config;
} ForeachInMetadataInfo;


guint32
tracker_data_update_get_new_service_id (TrackerDBInterface *iface)
{
	guint32		    files_max;
	TrackerDBResultSet *result_set;
	TrackerDBInterface *temp_iface;
	static guint32	    max = 0;

	if (G_LIKELY (max != 0)) {
		return ++max;
	}

	temp_iface = tracker_db_manager_get_db_interface (TRACKER_DB_FILE_METADATA);

	result_set = tracker_db_interface_execute_query (temp_iface, NULL,
							 "SELECT MAX(ID) AS A FROM Services");

	if (result_set) {
		GValue val = {0, };
		_tracker_db_result_set_get_value (result_set, 0, &val);
		if (G_VALUE_TYPE (&val) == G_TYPE_INT) {
			max = g_value_get_int (&val);
		}
		g_value_unset (&val);
		g_object_unref (result_set);
	}

	temp_iface = tracker_db_manager_get_db_interface (TRACKER_DB_EMAIL_METADATA);

	result_set = tracker_db_interface_execute_query (temp_iface, NULL,
							 "SELECT MAX(ID) AS A FROM Services");

	if (result_set) {
		GValue val = {0, };
		_tracker_db_result_set_get_value (result_set, 0, &val);
		if (G_VALUE_TYPE (&val) == G_TYPE_INT) {
			files_max = g_value_get_int (&val);
			max = MAX (files_max, max);
		}
		g_value_unset (&val);
		g_object_unref (result_set);
	}

	return ++max;
}

void
tracker_data_update_increment_stats (TrackerDBInterface *iface,
				     TrackerService     *service)
{
	const gchar *service_type, *parent;

	service_type = tracker_service_get_name (service);
	parent = tracker_service_get_parent (service);

	tracker_db_interface_execute_procedure (iface,
						NULL,
						"IncStat",
						service_type,
						NULL);

	if (parent) {
		tracker_db_interface_execute_procedure (iface,
							NULL,
							"IncStat",
							parent,
							NULL);
	}
}

void
tracker_data_update_decrement_stats (TrackerDBInterface *iface,
				     TrackerService     *service)
{
	const gchar *service_type, *parent;

	service_type = tracker_service_get_name (service);
	parent = tracker_service_get_parent (service);

	tracker_db_interface_execute_procedure (iface,
						NULL,
						"DecStat",
						service_type,
						NULL);

	if (parent) {
		tracker_db_interface_execute_procedure (iface,
							NULL,
							"DecStat",
							parent,
							NULL);
	}
}

void
tracker_data_update_create_event (TrackerDBInterface *iface,
				  guint32 service_id,
				  const gchar *type)
{
	gchar *service_id_str;

	service_id_str = tracker_guint32_to_string (service_id);

	tracker_db_interface_execute_procedure (iface, NULL, "CreateEvent",
						service_id_str,
						type,
						NULL);

	g_free (service_id_str);
}

gboolean
tracker_data_update_create_service (TrackerService *service,
				    guint32	    service_id,
				    const gchar	   *dirname,
				    const gchar	   *basename,
				    GHashTable     *metadata)
{
	TrackerDBInterface *iface;
	TrackerDBResultSet *result_set;
	guint32	volume_id = 0;
	gchar *id_str, *service_type_id_str, *path, *volume_id_str;
	gboolean is_dir, is_symlink;

	if (!service) {
		return FALSE;
	}

	/* retrieve VolumeID */
	iface = tracker_db_manager_get_db_interface (TRACKER_DB_COMMON);
	result_set = tracker_db_interface_execute_procedure (iface, NULL,
							     "GetVolumeByPath",
							     dirname,
							     dirname,
							     NULL);
	if (result_set) {
		tracker_db_result_set_get (result_set, 0, &volume_id, -1);
		g_object_unref (result_set);
	}
	volume_id_str = tracker_guint32_to_string (volume_id);

	iface = tracker_db_manager_get_db_interface_by_type (tracker_service_get_name (service),
							     TRACKER_DB_CONTENT_TYPE_METADATA);

	id_str = tracker_guint32_to_string (service_id);
	service_type_id_str = tracker_gint_to_string (tracker_service_get_id (service));

	path = g_build_filename (dirname, basename, NULL);

	is_dir = g_file_test (path, G_FILE_TEST_IS_DIR);
	is_symlink = g_file_test (path, G_FILE_TEST_IS_SYMLINK);

	tracker_db_interface_execute_procedure (iface, NULL, "CreateService",
						id_str,
						dirname,
						basename,
						service_type_id_str,
						is_dir ? "Folder" : g_hash_table_lookup (metadata, "File:Mime"),
						g_hash_table_lookup (metadata, "File:Size"),
						is_dir ? "1" : "0",
						is_symlink ? "1" : "0",
						"0", /* Offset */
						g_hash_table_lookup (metadata, "File:Modified"),
						volume_id_str, /* Aux ID */
						NULL);

	g_free (id_str);
	g_free (service_type_id_str);
	g_free (volume_id_str);
	g_free (path);

	return TRUE;
}

void
tracker_data_update_delete_service (TrackerService *service,
				    guint32	    service_id)
{

	TrackerDBInterface *iface;
	gchar *service_id_str;

	g_return_if_fail (TRACKER_IS_SERVICE (service));
	g_return_if_fail (service_id >= 1);

	iface = tracker_db_manager_get_db_interface_by_type (tracker_service_get_name (service),
							     TRACKER_DB_CONTENT_TYPE_METADATA);

	service_id_str = tracker_guint32_to_string (service_id);

	/* Delete from services table */
	tracker_db_interface_execute_procedure (iface,
						NULL,
						"DeleteService1",
						service_id_str,
						NULL);

	g_free (service_id_str);
}

void
tracker_data_update_delete_service_recursively (TrackerService *service,
						const gchar    *service_path)
{
	TrackerDBInterface *iface;

	g_return_if_fail (TRACKER_IS_SERVICE (service));
	g_return_if_fail (service_path != NULL);

	iface = tracker_db_manager_get_db_interface_by_type (tracker_service_get_name (service),
							     TRACKER_DB_CONTENT_TYPE_METADATA);

	/* We have to give two arguments. One is the actual path and
	 * the second is a string representing the likeness to match
	 * sub paths. Not sure how to do this in the .sql file
	 * instead.
	 */
	tracker_db_interface_execute_procedure (iface,
						NULL,
						"DeleteServiceRecursively",
						service_path,
						service_path,
						NULL);
}

gboolean
tracker_data_update_move_service (TrackerService *service,
				  const gchar	*from,
				  const gchar	*to)
{
	TrackerDBInterface *iface;
	GError *error = NULL;
	gchar *from_dirname;
	gchar *from_basename;
	gchar *to_dirname;
	gchar *to_basename;
	gboolean retval = TRUE;

	g_return_val_if_fail (TRACKER_IS_SERVICE (service), FALSE);
	g_return_val_if_fail (from != NULL, FALSE);
	g_return_val_if_fail (to != NULL, FALSE);

	iface = tracker_db_manager_get_db_interface_by_type (tracker_service_get_name (service),
							     TRACKER_DB_CONTENT_TYPE_METADATA);

	tracker_file_get_path_and_name (from,
					&from_dirname,
					&from_basename);
	tracker_file_get_path_and_name (to,
					&to_dirname,
					&to_basename);

	tracker_db_interface_execute_procedure (iface,
						&error,
						"MoveService",
						to_dirname, to_basename,
						from_dirname, from_basename,
						NULL);

	if (error) {
		g_warning ("%s", error->message);
		g_error_free (error);
		retval = FALSE;
	}

	g_free (to_dirname);
	g_free (to_basename);
	g_free (from_dirname);
	g_free (from_basename);

	return retval;
}

void
tracker_data_update_delete_all_metadata (TrackerService *service,
					 guint32	 service_id)
{
	TrackerDBInterface *iface;
	gchar *service_id_str;

	iface = tracker_db_manager_get_db_interface_by_type (tracker_service_get_name (service),
							     TRACKER_DB_CONTENT_TYPE_METADATA);

	service_id_str = tracker_guint32_to_string (service_id);

	/* Delete from ServiceMetadata, ServiceKeywordMetadata,
	 * ServiceNumberMetadata.
	 */
	tracker_db_interface_execute_procedure (iface,
						NULL,
						"DeleteServiceMetadata",
						service_id_str,
						NULL);
	tracker_db_interface_execute_procedure (iface,
						NULL,
						"DeleteServiceKeywordMetadata",
						service_id_str,
						NULL);
	tracker_db_interface_execute_procedure (iface,
						NULL,
						"DeleteServiceNumericMetadata",
						service_id_str,
						NULL);
	g_free (service_id_str);
}

void
tracker_data_update_set_metadata (TrackerService *service,
				  guint32	  service_id,
				  TrackerField	 *field,
				  const gchar	 *value,
				  const gchar	 *parsed_value)
{
	TrackerDBInterface *iface;
	gint metadata_key;
	gchar *id_str;

	id_str = tracker_guint32_to_string (service_id);
	iface = tracker_db_manager_get_db_interface_by_type (tracker_service_get_name (service),
							     TRACKER_DB_CONTENT_TYPE_METADATA);

	switch (tracker_field_get_data_type (field)) {
	case TRACKER_FIELD_TYPE_KEYWORD:
		tracker_db_interface_execute_procedure (iface, NULL,
							"SetMetadataKeyword",
							id_str,
							tracker_field_get_id (field),
							value,
							NULL);
		break;

	case TRACKER_FIELD_TYPE_INDEX:
	case TRACKER_FIELD_TYPE_STRING:
	case TRACKER_FIELD_TYPE_DOUBLE:
		tracker_db_interface_execute_procedure (iface, NULL,
							"SetMetadata",
							id_str,
							tracker_field_get_id (field),
							parsed_value,
							value,
							NULL);
		break;

	case TRACKER_FIELD_TYPE_INTEGER:
	case TRACKER_FIELD_TYPE_DATE:
		tracker_db_interface_execute_procedure (iface, NULL,
							"SetMetadataNumeric",
							id_str,
							tracker_field_get_id (field),
							value,
							NULL);
		break;

	case TRACKER_FIELD_TYPE_FULLTEXT:
		tracker_data_update_set_content (service, service_id, value);
		break;

	case TRACKER_FIELD_TYPE_BLOB:
	case TRACKER_FIELD_TYPE_STRUCT:
	case TRACKER_FIELD_TYPE_LINK:
		/* not handled */
	default:
		break;
	}

	metadata_key = tracker_ontology_service_get_key_metadata (tracker_service_get_name (service),
								  tracker_field_get_name (field));
	if (metadata_key > 0) {
		gchar *val;

		val = tracker_escape_string (value);

		tracker_db_interface_execute_query (iface, NULL,
						    "update Services set KeyMetadata%d = '%s' where id = %d",
						    metadata_key,
						    val,
						    service_id);
		g_free (val);
	} else if (tracker_field_get_data_type (field) == TRACKER_FIELD_TYPE_DATE &&
		   (strcmp (tracker_field_get_name (field), "File:Modified") == 0)) {
		/* Handle mtime */
		tracker_db_interface_execute_query (iface, NULL,
						    "update Services set IndexTime = '%s' where ID = %d",
						    value,
						    service_id);
	}

	g_free (id_str);
}

void
tracker_data_update_delete_metadata (TrackerService *service,
				     guint32	     service_id,
				     TrackerField   *field,
				     const gchar    *value)
{
	TrackerDBInterface *iface;
	gint metadata_key;
	gchar *id_str;

	id_str = tracker_guint32_to_string (service_id);
	iface = tracker_db_manager_get_db_interface_by_type (tracker_service_get_name (service),
							     TRACKER_DB_CONTENT_TYPE_METADATA);

	switch (tracker_field_get_data_type (field)) {
	case TRACKER_FIELD_TYPE_KEYWORD:
		if (!value) {
			g_debug ("Trying to remove keyword field with no specific value");
			tracker_db_interface_execute_procedure (iface, NULL,
								"DeleteMetadataKeyword",
								id_str,
								tracker_field_get_id (field),
								NULL);
		} else {
			tracker_db_interface_execute_procedure (iface, NULL,
								"DeleteMetadataKeywordValue",
								id_str,
								tracker_field_get_id (field),
								value,
								NULL);
		}
		break;

	case TRACKER_FIELD_TYPE_INDEX:
	case TRACKER_FIELD_TYPE_STRING:
	case TRACKER_FIELD_TYPE_DOUBLE:
		tracker_db_interface_execute_procedure (iface, NULL,
							"DeleteMetadata",
							id_str,
							tracker_field_get_id (field),
							NULL);
		break;

	case TRACKER_FIELD_TYPE_INTEGER:
	case TRACKER_FIELD_TYPE_DATE:
		tracker_db_interface_execute_procedure (iface, NULL,
							"DeleteMetadataNumeric",
							id_str,
							tracker_field_get_id (field),
							NULL);
		break;

	case TRACKER_FIELD_TYPE_FULLTEXT:
		tracker_data_update_delete_content (service, service_id);
		break;

	case TRACKER_FIELD_TYPE_BLOB:
	case TRACKER_FIELD_TYPE_STRUCT:
	case TRACKER_FIELD_TYPE_LINK:
		/* not handled */
	default:
		break;
	}

	metadata_key = tracker_ontology_service_get_key_metadata (tracker_service_get_name (service),
								  tracker_field_get_name (field));
	if (metadata_key > 0) {
		tracker_db_interface_execute_query (iface, NULL,
						    "update Services set KeyMetadata%d = '%s' where id = %d",
						    metadata_key, "", service_id);
	}

	g_free (id_str);
}

void
tracker_data_update_set_content (TrackerService *service,
				  guint32	 service_id,
				  const gchar   *text)
{
	TrackerDBInterface *iface;
	TrackerField *field;
	gchar *id_str;

	id_str = tracker_guint32_to_string (service_id);
	field = tracker_ontology_get_field_by_name ("File:Contents");
	iface = tracker_db_manager_get_db_interface_by_type (tracker_service_get_name (service),
							     TRACKER_DB_CONTENT_TYPE_CONTENTS);

	tracker_db_interface_execute_procedure (iface, NULL,
						"SaveServiceContents",
						id_str,
						tracker_field_get_id (field),
						text,
						NULL);
	g_free (id_str);
}

void
tracker_data_update_delete_content (TrackerService *service,
				     guint32	    service_id)
{
	TrackerDBInterface *iface;
	TrackerField *field;
	gchar *service_id_str;

	service_id_str = tracker_guint32_to_string (service_id);
	field = tracker_ontology_get_field_by_name ("File:Contents");
	iface = tracker_db_manager_get_db_interface_by_type (tracker_service_get_name (service),
							     TRACKER_DB_CONTENT_TYPE_CONTENTS);

	/* Delete contents if it has! */
	tracker_db_interface_execute_procedure (iface, NULL,
						"DeleteContent",
						service_id_str,
						tracker_field_get_id (field),
						NULL);

	g_free (service_id_str);
}

void
tracker_data_update_delete_handled_events (TrackerDBInterface *iface)
{
	g_return_if_fail (TRACKER_IS_DB_INTERFACE (iface));

	tracker_data_manager_exec (iface, "DELETE FROM Events WHERE BeingHandled = 1");
}

void
tracker_data_update_delete_service_by_path (const gchar *path,
					    const gchar *rdf_type)
{
	TrackerService *service;
	const gchar    *service_type; 
	guint32         service_id;

	g_return_if_fail (path != NULL);

	if (!rdf_type)
		return;

	service = tracker_ontology_get_service_by_name (rdf_type);
	service_type = tracker_service_get_name (service);
	service_id = tracker_data_query_file_id (service_type, path);

	/* When merging from the decomposed branch to trunk then this function
	 * wont exist in the decomposed branch. Create it based on this one. 
	 */
	if (service_id != 0) {
		tracker_data_update_delete_service (service, service_id);
		if (strcmp (service_type, "Folders") == 0) {
			tracker_data_update_delete_service_recursively (service, (gchar *) path);
		}
		tracker_data_update_delete_all_metadata (service, service_id);
	}
}


void
tracker_data_update_delete_service_all (const gchar *rdf_type)
{
	TrackerService     *service;
	gchar              *service_type_id; 
	TrackerDBInterface *iface;

	if (!rdf_type)
		return;

	service = tracker_ontology_get_service_by_name (rdf_type);

	g_return_if_fail (TRACKER_IS_SERVICE (service));

	service_type_id = tracker_gint_to_string (tracker_service_get_id (service));

	iface = tracker_db_manager_get_db_interface_by_type (tracker_service_get_name (service),
							     TRACKER_DB_CONTENT_TYPE_METADATA);

	tracker_db_interface_execute_procedure (iface,
						NULL,
						"DeleteServiceAll",
						service_type_id,
						NULL);

	g_free (service_type_id);
}

static void
set_metadata (TrackerField *field, 
	      gpointer value, 
	      ForeachInMetadataInfo *info)
{
	TrackerDBIndex *lindex;
	gchar *parsed_value;
	gchar **arr;
	gint service_id;
	gint i;
	gint score;

	/* TODO untested and unfinished port that came from the decomposed 
	 * branch of Jürg. When merging from the decomposed branch to trunk
	 * then pick the version in the decomposed branch for this function 
	 */
	parsed_value = tracker_parser_text_to_string (value,
						      info->language,
						      tracker_config_get_max_word_length (info->config),
						      tracker_config_get_min_word_length (info->config),
						      tracker_field_get_filtered (field),
						      tracker_field_get_filtered (field),
						      tracker_field_get_delimited (field));

	if (!parsed_value) {
		return;
	}

	score = tracker_field_get_weight (field);

	arr = g_strsplit (parsed_value, " ", -1);
	service_id = tracker_service_get_id (info->service);
	lindex = tracker_db_index_manager_get_index_by_service_id (service_id);

	for (i = 0; arr[i]; i++) {
		tracker_db_index_add_word (lindex,
					   arr[i],
					   info->iid_value,
					   tracker_service_get_id (info->service),
					   score);
	}

	tracker_data_update_set_metadata (info->service, info->iid_value, field, value, parsed_value);

	g_free (parsed_value);
	g_strfreev (arr);
}

static void
foreach_in_metadata_set_metadata (gpointer   predicate,
				  gpointer   value,
				  gpointer   user_data)
{
	TrackerField *field;
	ForeachInMetadataInfo *info;
	gint throttle;

	info = user_data;
	field = tracker_ontology_get_field_by_name (predicate);

	if (!field)
		return;

	/* Throttle indexer, value 9 is from older code, why 9? */
	throttle = tracker_config_get_throttle (info->config);
	if (throttle > 9) {
		tracker_throttle (info->config, throttle * 100);
	}

	if (!tracker_field_get_multiple_values (field)) {
		set_metadata (field, value, user_data);
	} else {
		GList *list;

		for (list = value; list; list = list->next)
			set_metadata (field, list->data, user_data);
	}
}

void 
tracker_data_update_replace_service (const gchar *path,
				     const gchar *rdf_type,
				     GHashTable  *metadata)
{
	TrackerDBInterface  *iface;
	TrackerDBResultSet  *result_set;
	const gchar         *modified;
	GError              *error = NULL;
	TrackerService      *service;
	gchar               *escaped_path;
	gchar               *dirname;
	gchar               *basename;

	g_return_if_fail (path != NULL);
	g_return_if_fail (metadata != NULL);

	/* When merging from the decomposed branch to trunk then pick the version
	 * in the decomposed branch for this function. However, carefully 
	 * compare the features, as this version is more recent and has 
	 * implemented a few significant items, whereas the version in the
	 * decomposed branch was a proof of concept implementation, and might
	 * not have these needed features. 
	 */

	if (!rdf_type)
		return;

	service = tracker_ontology_get_service_by_name (rdf_type);

	iface = tracker_db_manager_get_db_interface_by_type (tracker_service_get_name (service),
							     TRACKER_DB_CONTENT_TYPE_METADATA);

	modified = g_hash_table_lookup (metadata, "File:Modified");

	if (!modified) {
		return;
	}

	escaped_path = tracker_escape_string (path);

	basename = g_path_get_basename (escaped_path);
	dirname = g_path_get_dirname (escaped_path);

	/* TODO Warning: comparing Modified against Accessed. Do we have a
	 * better field for this? */

	result_set = tracker_db_interface_execute_query (iface, &error,
							 "SELECT ID, Accessed < '%s' FROM Services "
							 "WHERE Path = '%s' AND "
							 "Name = '%s'",
							 modified,
							 dirname, basename);

	if (error) {
		g_error_free (error);
	}

	if (result_set) {
		GValue id_value = { 0, };
		GValue is_value = { 0, };
		gint   iid_value, iis_value;

		_tracker_db_result_set_get_value (result_set, 0, &id_value);
		iid_value = g_value_get_int (&id_value);

		_tracker_db_result_set_get_value (result_set, 1, &is_value);
		iis_value = g_value_get_int (&is_value);

		if (iis_value) {
			ForeachInMetadataInfo *info = g_slice_new (ForeachInMetadataInfo);
			info->service = service;
			info->iid_value = iid_value;

			info->config = tracker_data_manager_get_config ();
			info->language = tracker_data_manager_get_language ();

			g_hash_table_foreach (metadata, 
					      foreach_in_metadata_set_metadata,
					      info);

			g_slice_free (ForeachInMetadataInfo, info);
		}

		g_value_unset (&id_value);
		g_value_unset (&is_value);

		g_object_unref (result_set);
	} else {
		guint32     id;

		id = tracker_data_update_get_new_service_id (iface);

		if (tracker_data_update_create_service (service, id,
							dirname, basename,
							metadata)) {
			ForeachInMetadataInfo *info;

			info = g_slice_new (ForeachInMetadataInfo);

			info->service = service;
			info->iid_value = id;

			info->config = tracker_data_manager_get_config ();
			info->language = tracker_data_manager_get_language ();

			g_hash_table_foreach (metadata, 
					      foreach_in_metadata_set_metadata,
					      info);

			g_slice_free (ForeachInMetadataInfo, info);
		}
	}

	g_free (dirname);
	g_free (basename);
	g_free (escaped_path);
}

void
tracker_data_update_enable_volume (const gchar *udi,
                                   const gchar *mount_path)
{
	TrackerDBInterface *iface;
	TrackerDBResultSet *result_set;
	guint32		    id = 0;

	g_return_if_fail (udi != NULL);
	g_return_if_fail (mount_path != NULL);

	iface = tracker_db_manager_get_db_interface (TRACKER_DB_COMMON);

	result_set = tracker_db_interface_execute_procedure (iface, NULL,
					   "GetVolumeID",
					   udi,
					   NULL);


	if (result_set) {
		tracker_db_result_set_get (result_set, 0, &id, -1);
		g_object_unref (result_set);
	}

	if (id == 0) {
		tracker_db_interface_execute_procedure (iface, NULL,
							"InsertVolume",
							mount_path,
							udi,
							NULL);
	} else {
		tracker_db_interface_execute_procedure (iface, NULL,
							"EnableVolume",
							mount_path,
							udi,
							NULL);
	}
}

void
tracker_data_update_disable_volume (const gchar *udi)
{
	TrackerDBInterface *iface;

	g_return_if_fail (udi != NULL);

	iface = tracker_db_manager_get_db_interface (TRACKER_DB_COMMON);
	
	tracker_db_interface_execute_procedure (iface, NULL,
						"DisableVolume",
						udi,
						NULL);
}

void
tracker_data_update_disable_all_volumes (void)
{
	TrackerDBInterface *iface;

	iface = tracker_db_manager_get_db_interface (TRACKER_DB_COMMON);
	
	tracker_db_interface_execute_procedure (iface, NULL,
						"DisableAllVolumes",
						NULL);
}

