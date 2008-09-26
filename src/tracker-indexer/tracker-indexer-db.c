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

#include <stdlib.h>

#include <libtracker-common/tracker-type-utils.h>
#include <libtracker-common/tracker-file-utils.h>

#include <libtracker-db/tracker-db-manager.h>
#include <libtracker-db/tracker-db-dbus.h>

#include "tracker-indexer-db.h"

guint32
tracker_db_get_new_service_id (TrackerDBInterface *iface)
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
tracker_db_increment_stats (TrackerDBInterface *iface,
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
tracker_db_decrement_stats (TrackerDBInterface *iface,
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
tracker_db_create_event (TrackerDBInterface *iface,
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
tracker_db_check_service (TrackerService *service,
			  const gchar	 *dirname,
			  const gchar	 *basename,
			  guint32	 *id,
			  time_t	 *mtime)
{
	TrackerDBInterface *iface;
	TrackerDBResultSet *result_set;
	gchar *db_mtime_str;
	guint db_id;
	guint db_mtime;
	gboolean found = FALSE;

	db_id = db_mtime = 0;

	iface = tracker_db_manager_get_db_interface_by_type (tracker_service_get_name (service),
							     TRACKER_DB_CONTENT_TYPE_METADATA);

	result_set = tracker_db_interface_execute_procedure (iface, NULL,
							     "GetServiceID",
							     dirname,
							     basename,
							     NULL);
	if (result_set) {
		tracker_db_result_set_get (result_set,
					   0, &db_id,
					   1, &db_mtime_str,
					   -1);
		g_object_unref (result_set);
		found = TRUE;

		if (db_mtime_str) {
			db_mtime = tracker_string_to_date (db_mtime_str);
			g_free (db_mtime_str);
		}
	}

	if (id) {
		*id = (guint32) db_id;
	}

	if (mtime) {
		*mtime = (time_t) db_mtime;
	}

	return found;
}

guint
tracker_db_get_service_type (const gchar *dirname,
			     const gchar *basename)
{
	TrackerDBInterface *iface;
	TrackerDBResultSet *result_set;
	guint service_type_id;

	/* We are asking this because the module cannot assign service_type -> probably it is files */
	iface = tracker_db_manager_get_db_interface_by_type ("Files",
							     TRACKER_DB_CONTENT_TYPE_METADATA);

	result_set = tracker_db_interface_execute_procedure (iface, NULL,
							     "GetServiceID",
							     dirname,
							     basename,
							     NULL);
	if (!result_set) {
		return 0;
	}

	tracker_db_result_set_get (result_set, 3, &service_type_id, -1);
	g_object_unref (result_set);

	return service_type_id;
}

gboolean
tracker_db_create_service (TrackerService  *service,
			   guint32	    id,
			   const gchar	   *dirname,
			   const gchar	   *basename,
			   TrackerMetadata *metadata)
{
	TrackerDBInterface *iface;
	gchar *id_str, *service_type_id_str, *path;
	gboolean is_dir, is_symlink, enabled;

	if (!service) {
		return FALSE;
	}

	iface = tracker_db_manager_get_db_interface_by_type (tracker_service_get_name (service),
							     TRACKER_DB_CONTENT_TYPE_METADATA);

	id_str = tracker_guint32_to_string (id);
	service_type_id_str = tracker_gint_to_string (tracker_service_get_id (service));

	path = g_build_filename (dirname, basename, NULL);

	is_dir = g_file_test (path, G_FILE_TEST_IS_DIR);
	is_symlink = g_file_test (path, G_FILE_TEST_IS_SYMLINK);

	/* FIXME: do not hardcode arguments */
	tracker_db_interface_execute_procedure (iface, NULL, "CreateService",
						id_str,
						dirname,
						basename,
						service_type_id_str,
						is_dir ? "Folder" : tracker_metadata_lookup (metadata, "File:Mime"),
						tracker_metadata_lookup (metadata, "File:Size"),
						is_dir ? "1" : "0",
						is_symlink ? "1" : "0",
						"0", /* Offset */
						tracker_metadata_lookup (metadata, "File:Modified"),
						"0", /* Aux ID */
						NULL);

	enabled = is_dir ?
		tracker_service_get_show_service_directories (service) :
		tracker_service_get_show_service_files (service);

	if (!enabled) {
		tracker_db_interface_execute_query (iface, NULL,
						    "Update services set Enabled = 0 where ID = %d",
						    id);
	}

	g_free (id_str);
	g_free (service_type_id_str);
	g_free (path);

	return TRUE;
}

static gchar *
db_get_metadata (TrackerService *service,
		 guint		 service_id,
		 gboolean	 keywords)
{
	TrackerDBInterface *iface;
	TrackerDBResultSet *result_set;
	gchar		   *query;
	GString		   *result;
	gchar		   *str = NULL;

	iface = tracker_db_manager_get_db_interface_by_type (tracker_service_get_name (service),
							     TRACKER_DB_CONTENT_TYPE_METADATA);

	result = g_string_new ("");

	if (service_id < 1) {
		return g_string_free (result, FALSE);
	}

	if (keywords) {
		query = g_strdup_printf ("Select MetadataValue From ServiceKeywordMetadata WHERE serviceID = %d",
					 service_id);
	} else {
		query = g_strdup_printf ("Select MetadataValue From ServiceMetadata WHERE serviceID = %d",
					 service_id);
	}

	result_set = tracker_db_interface_execute_query (iface, NULL, query);
	g_free (query);

	if (result_set) {
		gboolean valid = TRUE;

		while (valid) {
			tracker_db_result_set_get (result_set, 0, &str, -1);
			result = g_string_append (result, str);
			result = g_string_append (result, " ");
			valid = tracker_db_result_set_iter_next (result_set);
			g_free (str);
		}

		g_object_unref (result_set);
	}

	return g_string_free (result, FALSE);
}

static void
result_set_to_metadata (TrackerDBResultSet *result_set,
			TrackerMetadata    *metadata,
			gboolean	    numeric,
			gboolean	    only_embedded)
{
	TrackerField *field;
	gchar	     *value;
	gint	      numeric_value;
	gint	      metadata_id;
	gboolean      valid = TRUE;

	while (valid) {
		if (numeric) {
			tracker_db_result_set_get (result_set,
						   0, &metadata_id,
						   1, &numeric_value,
						   -1);
			value = g_strdup_printf ("%d", numeric_value);
		} else {
			tracker_db_result_set_get (result_set,
						   0, &metadata_id,
						   1, &value,
						   -1);
		}

		field = tracker_ontology_get_field_by_id (metadata_id);
		if (!field) {
			g_critical ("Field id %d in database but not in tracker-ontology",
				    metadata_id);
			g_free (value);
			return;
		}

		if (tracker_field_get_embedded (field) || !only_embedded) {
			if (tracker_field_get_multiple_values (field)) {
				GList *new_values;
				const GList *old_values;

				new_values = NULL;
				old_values = tracker_metadata_lookup_multiple_values (metadata,
										      tracker_field_get_name (field));
				if (old_values) {
					new_values = g_list_copy ((GList *) old_values);
				}

				new_values = g_list_prepend (new_values, value);
				tracker_metadata_insert_multiple_values (metadata,
									 tracker_field_get_name (field),
									 new_values);
			} else {
				tracker_metadata_insert (metadata,
							 tracker_field_get_name (field),
							 value);
			}
		} else {
			g_free (value);
		}

		valid = tracker_db_result_set_iter_next (result_set);
	}
}

TrackerMetadata *
tracker_db_get_all_metadata (TrackerService *service,
			     guint32	     service_id,
			     gboolean	     only_embedded)
{
	TrackerDBInterface *iface;
	TrackerDBResultSet *result_set = NULL;
	gchar		   *service_id_str;
	TrackerMetadata    *metadata;

	metadata = tracker_metadata_new ();

	service_id_str = g_strdup_printf ("%d", service_id);
	iface = tracker_db_manager_get_db_interface_by_type (tracker_service_get_name (service),
							     TRACKER_DB_CONTENT_TYPE_METADATA);

	result_set = tracker_db_interface_execute_procedure (iface, NULL, "GetMetadataIDValue", service_id_str, NULL);
	if (result_set) {
		result_set_to_metadata (result_set, metadata, FALSE, only_embedded);
		g_object_unref (result_set);
	}

	result_set = tracker_db_interface_execute_procedure (iface, NULL, "GetMetadataIDValueKeyword", service_id_str, NULL);
	if (result_set) {
		result_set_to_metadata (result_set, metadata, FALSE, only_embedded);
		g_object_unref (result_set);
	}

	result_set = tracker_db_interface_execute_procedure (iface, NULL, "GetMetadataIDValueNumeric", service_id_str, NULL);
	if (result_set) {
		result_set_to_metadata (result_set, metadata, TRUE, only_embedded);
		g_object_unref (result_set);
	}

	g_free (service_id_str);

	return metadata;
}

void
tracker_db_delete_service (TrackerService *service,
			   guint32	   service_id)
{

	TrackerDBInterface *iface;
	gchar *service_id_str;

	if (service_id < 1) {
		return;
	}

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
tracker_db_move_service (TrackerService *service,
			 const gchar	*from,
			 const gchar	*to)
{
	TrackerDBInterface *iface;
	GError *error = NULL;
	gchar *from_dirname;
	gchar *from_basename;
	gchar *to_dirname;
	gchar *to_basename;

	iface = tracker_db_manager_get_db_interface_by_type (tracker_service_get_name (service),
							     TRACKER_DB_CONTENT_TYPE_METADATA);

	tracker_file_get_path_and_name (from,
					&from_dirname,
					&from_basename);
	tracker_file_get_path_and_name (to,
					&to_dirname,
					&to_basename);

	tracker_db_interface_execute_procedure (iface,
						NULL,
						"MoveService",
						to_dirname, to_basename,
						from_dirname, from_basename,
						NULL);

	/* FIXME: This procedure should use LIKE statement */
	tracker_db_interface_execute_procedure (iface,
						&error,
						"MoveServiceChildren",
						from,
						to,
						from,
						NULL);

	g_free (to_dirname);
	g_free (to_basename);
	g_free (from_dirname);
	g_free (from_basename);
}

void
tracker_db_delete_all_metadata (TrackerService *service,
				guint32		service_id)
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
}

gchar *
tracker_db_get_unparsed_metadata (TrackerService *service,
				  guint		  service_id)
{
	return db_get_metadata (service, service_id, TRUE);
}

gchar *
tracker_db_get_parsed_metadata (TrackerService *service,
				guint		service_id)
{
	return db_get_metadata (service, service_id, FALSE);
}

gchar **
tracker_db_get_property_values (TrackerService *service_def,
				guint32		id,
				TrackerField   *field)
{
	TrackerDBInterface *iface;
	TrackerDBResultSet *result_set = NULL;
	gint		    metadata_key;
	gchar		  **final_result = NULL;
	gboolean	    is_numeric = FALSE;

	iface = tracker_db_manager_get_db_interface_by_type (tracker_service_get_name (service_def),
							     TRACKER_DB_CONTENT_TYPE_METADATA);
	metadata_key = tracker_ontology_service_get_key_metadata (tracker_service_get_name (service_def),
								  tracker_field_get_name (field));

	if (metadata_key > 0) {
		gchar *query;

		query = g_strdup_printf ("SELECT KeyMetadata%d FROM Services WHERE id = '%d'",
					 metadata_key,
					 id);
		result_set = tracker_db_interface_execute_query (iface,
								 NULL,
								 query,
								 NULL);
		g_free (query);
	} else {
		gchar *id_str;

		id_str = tracker_guint32_to_string (id);

		switch (tracker_field_get_data_type (field)) {
		case TRACKER_FIELD_TYPE_KEYWORD:
			result_set = tracker_db_interface_execute_procedure (iface, NULL,
									     "GetMetadataKeyword",
									     id_str,
									     tracker_field_get_id (field),
									     NULL);
			break;
		case TRACKER_FIELD_TYPE_INDEX:
		case TRACKER_FIELD_TYPE_STRING:
		case TRACKER_FIELD_TYPE_DOUBLE:
			result_set = tracker_db_interface_execute_procedure (iface, NULL,
									     "GetMetadata",
									     id_str,
									     tracker_field_get_id (field),
									     NULL);
			break;
		case TRACKER_FIELD_TYPE_INTEGER:
		case TRACKER_FIELD_TYPE_DATE:
			result_set = tracker_db_interface_execute_procedure (iface, NULL,
									     "GetMetadataNumeric",
									     id_str,
									     tracker_field_get_id (field),
									     NULL);
			is_numeric = TRUE;
			break;
		case TRACKER_FIELD_TYPE_FULLTEXT:
			tracker_db_get_text (service_def, id);
			break;
		case TRACKER_FIELD_TYPE_BLOB:
		case TRACKER_FIELD_TYPE_STRUCT:
		case TRACKER_FIELD_TYPE_LINK:
			/* not handled */
		default:
			break;
		}
		g_free (id_str);
	}

	if (result_set) {
		if (tracker_db_result_set_get_n_rows (result_set) > 1) {
			g_warning ("More than one result in tracker_db_get_property_value");
		}

		if (!is_numeric) {
			final_result = tracker_dbus_query_result_to_strv (result_set, 0, NULL);
		} else {
			final_result = tracker_dbus_query_result_numeric_to_strv (result_set, 0, NULL);
		}

		g_object_unref (result_set);
	}

	return final_result;
}

void
tracker_db_set_metadata (TrackerService *service,
			 guint32	 id,
			 TrackerField	*field,
			 const gchar	*value,
			 const gchar	*parsed_value)
{
	TrackerDBInterface *iface;
	gint metadata_key;
	gchar *id_str;
	gchar *time_string;

	id_str = tracker_guint32_to_string (id);
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
		tracker_db_interface_execute_procedure (iface, NULL,
							"SetMetadataNumeric",
							id_str,
							tracker_field_get_id (field),
							value,
							NULL);
		break;

	case TRACKER_FIELD_TYPE_DATE:

		time_string = tracker_date_to_time_string (value);

		if (time_string) {
			tracker_db_interface_execute_procedure (iface, NULL,
								"SetMetadataNumeric",
								id_str,
								tracker_field_get_id (field),
								time_string,
								NULL);
			g_free (time_string);
		}
		break;

	case TRACKER_FIELD_TYPE_FULLTEXT:
		tracker_db_set_text (service, id, value);
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
						    metadata_key,
						    value,
						    id);
	}

	g_free (id_str);
}

void
tracker_db_delete_metadata (TrackerService *service,
			    guint32	    id,
			    TrackerField   *field,
			    const gchar    *value)
{
	TrackerDBInterface *iface;
	gint metadata_key;
	gchar *id_str;

	id_str = tracker_guint32_to_string (id);
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
		tracker_db_delete_text (service, id);
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
						    metadata_key, "", id);
	}

	g_free (id_str);
}

void
tracker_db_set_text (TrackerService *service,
		     guint32	     id,
		     const gchar    *text)
{
	TrackerDBInterface *iface;
	TrackerField *field;
	gchar *id_str;

	id_str = tracker_guint32_to_string (id);
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

gchar *
tracker_db_get_text (TrackerService *service,
		     guint32	     id)
{
	TrackerDBInterface *iface;
	TrackerField	   *field;
	gchar		   *service_id_str, *contents = NULL;
	TrackerDBResultSet *result_set;

	service_id_str = tracker_guint32_to_string (id);
	field = tracker_ontology_get_field_by_name ("File:Contents");
	iface = tracker_db_manager_get_db_interface_by_type (tracker_service_get_name (service),
							     TRACKER_DB_CONTENT_TYPE_CONTENTS);

	/* Delete contents if it has! */
	result_set = tracker_db_interface_execute_procedure (iface, NULL,
							     "GetContents",
							     service_id_str,
							     tracker_field_get_id (field),
							     NULL);

	if (result_set) {
		tracker_db_result_set_get (result_set, 0, &contents, -1);
		g_object_unref (result_set);
	}

	g_free (service_id_str);

	return contents;
}

void
tracker_db_delete_text (TrackerService *service,
			guint32		id)
{
	TrackerDBInterface *iface;
	TrackerField *field;
	gchar *service_id_str;

	service_id_str = tracker_guint32_to_string (id);
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
