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

#include <libtracker-common/tracker-file-utils.h>
#include <libtracker-common/tracker-nfs-lock.h>
#include <libtracker-common/tracker-parser.h>
#include <libtracker-common/tracker-type-utils.h>
#include <libtracker-common/tracker-utils.h>

#include <libtracker-db/tracker-db-dbus.h>
#include <libtracker-db/tracker-db-index.h>
#include <libtracker-db/tracker-db-interface-sqlite.h>
#include <libtracker-db/tracker-db-index-manager.h>
#include <libtracker-db/tracker-db-manager.h>

#include "tracker-data-manager.h"
#include "tracker-data-query.h"
#include "tracker-data-schema.h"

TrackerDBResultSet *
tracker_data_query_metadata_field (TrackerDBInterface *iface,
				   const gchar	      *id,
				   const gchar	      *field)
{
	TrackerField *def;
	const gchar  *proc = NULL;

	g_return_val_if_fail (TRACKER_IS_DB_INTERFACE (iface), NULL);
	g_return_val_if_fail (id != NULL, NULL);
	g_return_val_if_fail (field != NULL, NULL);

	def = tracker_ontology_get_field_by_name (field);

	if (!def) {
		g_warning ("Metadata not found for id:'%s' and type:'%s'", id, field);
		return NULL;
	}

	switch (tracker_field_get_data_type (def)) {
	case TRACKER_FIELD_TYPE_INDEX:
	case TRACKER_FIELD_TYPE_STRING:
	case TRACKER_FIELD_TYPE_DOUBLE:
		proc = "GetMetadata";
		break;

	case TRACKER_FIELD_TYPE_INTEGER:
	case TRACKER_FIELD_TYPE_DATE:
		proc = "GetMetadataNumeric";
		break;

	case TRACKER_FIELD_TYPE_FULLTEXT:
		proc = "GetContents";
		break;

	case TRACKER_FIELD_TYPE_KEYWORD:
		proc = "GetMetadataKeyword";
		break;

	default:
	case TRACKER_FIELD_TYPE_BLOB:
	case TRACKER_FIELD_TYPE_STRUCT:
	case TRACKER_FIELD_TYPE_LINK:
		g_warning ("Metadata could not be retrieved as type:%d is not supported",
			   tracker_field_get_data_type (def));
		return NULL;
	}

	return tracker_data_manager_exec_proc (iface,
				     proc,
				     id,
				     tracker_field_get_id (def),
				     NULL);
}

static void
db_result_set_to_ptr_array (TrackerDBResultSet *result_set,
			    GPtrArray         **previous)
{
	gchar        *prop_id_str;
	gchar        *value;
	TrackerField *field;
	gboolean      valid = result_set != NULL;

	while (valid) {
		/* Item is a pair (property_name, value) */
		gchar **item = g_new0 (gchar *, 2);

		tracker_db_result_set_get (result_set, 0, &prop_id_str, 1, &value, -1);
		item[1] = g_strdup (value);

		field = tracker_ontology_get_field_by_id (GPOINTER_TO_UINT (prop_id_str));

		item[0] = g_strdup (tracker_field_get_name (field));

		g_ptr_array_add (*previous, item);
		
		valid = tracker_db_result_set_iter_next (result_set);
	}
}

GPtrArray *
tracker_data_query_all_metadata (const gchar *service_type,
				 const gchar *service_id) 
{
	TrackerDBInterface *iface;
	TrackerDBResultSet *result_set;
	GPtrArray          *result;

	result = g_ptr_array_new ();

	iface = tracker_db_manager_get_db_interface_by_service (service_type);
	if (!iface) {
		g_warning ("Unable to obtain a DB connection for service type '%s'",
			   service_type);
		return result;
	}

	result_set = tracker_data_manager_exec_proc (iface, 
						     "GetAllMetadata", 
						     service_id, 
						     service_id, 
						     service_id, 
						     NULL);

	if (result_set) {
		db_result_set_to_ptr_array (result_set, &result);
		g_object_unref (result_set);
	}

	return result;

}

TrackerDBResultSet *
tracker_data_query_metadata_fields (TrackerDBInterface *iface,
				    const gchar	       *service_type,
				    const gchar	       *service_id,
				    gchar	      **fields)
{
	TrackerDBResultSet *result_set;
	GString		   *sql, *sql_join;
	gchar		   *query;
	guint		    i;

	/* Build SQL select clause */
	sql = g_string_new (" SELECT DISTINCT ");
	sql_join = g_string_new (" FROM Services S ");

	for (i = 0; i < g_strv_length (fields); i++) {
		TrackerFieldData *field_data;

		field_data = tracker_data_schema_get_metadata_field (iface,
							    service_type,
							    fields[i],
							    i,
							    TRUE,
							    FALSE);

		if (!field_data) {
			g_string_free (sql_join, TRUE);
			g_string_free (sql, TRUE);
			return NULL;
		}

		if (i == 0) {
			g_string_append_printf (sql, " %s",
						tracker_field_data_get_select_field (field_data));
		} else {
			g_string_append_printf (sql, ", %s",
						tracker_field_data_get_select_field (field_data));
		}

		if (tracker_field_data_get_needs_join (field_data)) {
			g_string_append_printf (sql_join,
						"\n LEFT OUTER JOIN %s %s ON (S.ID = %s.ServiceID and %s.MetaDataID = %s) ",
						tracker_field_data_get_table_name (field_data),
						tracker_field_data_get_alias (field_data),
						tracker_field_data_get_alias (field_data),
						tracker_field_data_get_alias (field_data),
						tracker_field_data_get_id_field (field_data));
		}

		g_object_unref (field_data);
	}

	g_string_append (sql, sql_join->str);
	g_string_free (sql_join, TRUE);

	/* Build SQL where clause */
	g_string_append_printf (sql, " WHERE S.ID = %s", service_id);

	query = g_string_free (sql, FALSE);

	g_debug ("%s", query);

	result_set = tracker_db_interface_execute_query (iface, NULL, "%s", query);

	g_free (query);

	return result_set;
}

/*
 * Obtain the concrete service type name for the file id.
 */
G_CONST_RETURN gchar *
tracker_data_query_service_type_by_id (TrackerDBInterface *iface,
				       guint32             service_id)
{
	TrackerDBResultSet *result_set;
	gint		    service_type_id;
	gchar              *service_id_str;
	const gchar	   *result = NULL;

	g_return_val_if_fail (TRACKER_IS_DB_INTERFACE (iface), NULL);
	g_return_val_if_fail (service_id > 0, NULL);

	service_id_str = tracker_guint32_to_string (service_id);

	result_set = tracker_data_manager_exec_proc (iface,
					   "GetFileByID",
					   service_id_str,
					   NULL);

	g_free (service_id_str);

	if (result_set) {
		tracker_db_result_set_get (result_set, 3, &service_type_id, -1);
		g_object_unref (result_set);

		result = tracker_ontology_get_service_by_id (service_type_id);
	}

	return result;
}

guint32
tracker_data_query_file_id (const gchar *service_type,
			    const gchar *path)
{
	TrackerDBResultSet *result_set;
	TrackerDBInterface *iface;
	gchar		   *dir, *name;
	guint32		    id = 0;
	gboolean            enabled;

	g_return_val_if_fail (path != NULL, 0);

	iface = tracker_db_manager_get_db_interface_by_service (service_type);
	
	if (!iface) {
		g_warning ("Unable to obtain interface for service type '%s'",
			   service_type);
		return 0;
	}

	tracker_file_get_path_and_name (path, &dir, &name);

	result_set = tracker_data_manager_exec_proc (iface,
					   "GetServiceID",
					   dir,
					   name,
					   NULL);

	g_free (dir);
	g_free (name);

	if (result_set) {
		tracker_db_result_set_get (result_set,
					   0, &id,
					   4, &enabled,
					   -1);
		g_object_unref (result_set);

		if (!enabled) {
			id = 0;
		}
	}

	return id;
}

gchar *
tracker_data_query_file_id_as_string (const gchar	*service_type,
				      const gchar	*path)
{
	guint32	id;

	g_return_val_if_fail (path != NULL, NULL);

	id = tracker_data_query_file_id (service_type, path);

	if (id > 0) {
		return tracker_guint_to_string (id);
	}

	return NULL;
}

gboolean
tracker_data_query_service_exists (TrackerService *service,
				   const gchar	  *dirname,
				   const gchar	  *basename,
				   guint32	  *service_id,
				   time_t	  *mtime,
				   gboolean       *enabled)
{
	TrackerDBInterface *iface;
	TrackerDBResultSet *result_set;
	guint db_id, db_mtime;
	gboolean db_enabled, found = FALSE;

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
					   1, &db_mtime,
					   4, &db_enabled,
					   -1);
		g_object_unref (result_set);
		found = TRUE;
	}

	if (service_id) {
		*service_id = (guint32) db_id;
	}

	if (mtime) {
		*mtime = (time_t) db_mtime;
	}

	if (enabled) {
		*enabled = db_enabled;
	}

	return found;
}

guint
tracker_data_query_service_type_id (const gchar *dirname,
				    const gchar *basename)
{
	TrackerDBInterface *iface;
	TrackerDBResultSet *result_set;
	guint service_type_id;
	gboolean enabled;

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

	tracker_db_result_set_get (result_set,
				   3, &service_type_id,
				   4, &enabled,
				   -1);
	g_object_unref (result_set);

	return (enabled) ? service_type_id : 0;
}

GHashTable *
tracker_data_query_service_children (TrackerService *service,
				     const gchar    *dirname)
{
	TrackerDBInterface *iface;
	TrackerDBResultSet *result_set;
	gboolean valid = TRUE;
	GHashTable *children;

	iface = tracker_db_manager_get_db_interface_by_type (tracker_service_get_name (service),
							     TRACKER_DB_CONTENT_TYPE_METADATA);

	result_set = tracker_db_interface_execute_procedure (iface, NULL,
							     "GetFileChildren",
							     dirname,
							     NULL);

	if (!result_set) {
		return NULL;
	}

	children = g_hash_table_new_full (g_direct_hash,
					  g_direct_equal,
					  NULL,
					  (GDestroyNotify) g_free);

	while (valid) {
		guint32 id;
		gchar *child_name;

		tracker_db_result_set_get (result_set,
					   0, &id,
					   2, &child_name,
					   -1);

		g_hash_table_insert (children, GUINT_TO_POINTER (id), child_name);

		valid = tracker_db_result_set_iter_next (result_set);
	}

	g_object_unref (result_set);

	return children;
}

/*
 * Result set with (metadataID, value) per row
 */
static void
result_set_to_metadata (TrackerDBResultSet  *result_set,
			TrackerDataMetadata *metadata,
			gboolean	     embedded)
{
	TrackerField *field;
	gint	      metadata_id;
	gboolean      valid = TRUE;

	while (valid) {
		GValue transform = {0, };
		GValue value = {0, };
		gchar *str;

		g_value_init (&transform, G_TYPE_STRING);
		tracker_db_result_set_get (result_set, 0, &metadata_id, -1);
		_tracker_db_result_set_get_value (result_set, 1, &value);

		if (g_value_transform (&value, &transform)) {
			str = g_value_dup_string (&transform);
			
			if (!str) {
				str = g_strdup ("");
			} else if (!g_utf8_validate (str, -1, NULL)) {
				g_warning ("Could not add string:'%s' to GStrv, invalid UTF-8", str);
				g_free (str);
				str = g_strdup ("");
			}

			g_value_unset (&transform);
		} else {
			str = g_strdup ("");
		}

		g_value_unset (&value);
		field = tracker_ontology_get_field_by_id (metadata_id);
		if (!field) {
			g_critical ("Field id %d in database but not in tracker-ontology",
				    metadata_id);
			g_free (str);
			return;
		}

		if (tracker_field_get_embedded (field) == embedded) {
			if (tracker_field_get_multiple_values (field)) {
				GList *new_values;
				const GList *old_values;

				new_values = NULL;
				old_values = tracker_data_metadata_lookup_values (metadata,
										  tracker_field_get_name (field));
				if (old_values) {
					new_values = g_list_copy ((GList*) old_values);
				}

				new_values = g_list_prepend (new_values, str);
				tracker_data_metadata_insert_values (metadata,
								     tracker_field_get_name (field),
								     new_values);

				g_list_free (new_values);
			} else {
				tracker_data_metadata_insert (metadata,
							      tracker_field_get_name (field),
							      str);
			}
		}

		g_free (str);

		valid = tracker_db_result_set_iter_next (result_set);
	}
}

TrackerDataMetadata *
tracker_data_query_metadata (TrackerService *service,
			     guint32	     service_id,
			     gboolean        embedded)
{
	TrackerDBInterface  *iface;
	TrackerDBResultSet  *result_set = NULL;
	gchar		    *service_id_str;
	TrackerDataMetadata *metadata;

	metadata = tracker_data_metadata_new ();

	g_return_val_if_fail (TRACKER_IS_SERVICE (service), metadata);

	service_id_str = g_strdup_printf ("%d", service_id);
	iface = tracker_db_manager_get_db_interface_by_type (tracker_service_get_name (service),
							     TRACKER_DB_CONTENT_TYPE_METADATA);


	result_set = tracker_data_manager_exec_proc (iface,
						     "GetAllMetadata", 
						     service_id_str,
						     service_id_str,
						     service_id_str, NULL);
	if (result_set) {
		result_set_to_metadata (result_set, metadata, embedded);
		g_object_unref (result_set);
	}

	g_free (service_id_str);

	return metadata;
}

TrackerDBResultSet *
tracker_data_query_backup_metadata (TrackerService *service)
{
	TrackerDBInterface *iface;
	TrackerDBResultSet *result_set;

	g_return_val_if_fail (TRACKER_IS_SERVICE (service), NULL);

	iface = tracker_db_manager_get_db_interface_by_service (tracker_service_get_name (service));

	result_set = tracker_data_manager_exec_proc (iface,
						     "GetUserMetadataBackup", 
						     NULL);
	return result_set;
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

	result_set = tracker_db_interface_execute_query (iface, NULL, "%s", query);
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

gchar *
tracker_data_query_unparsed_metadata (TrackerService *service,
				      guint	      service_id)
{
	return db_get_metadata (service, service_id, TRUE);
}

gchar *
tracker_data_query_parsed_metadata (TrackerService *service,
				    guint	    service_id)
{
	return db_get_metadata (service, service_id, FALSE);
}

gchar **
tracker_data_query_metadata_field_values (TrackerService *service_def,
					  guint32	  service_id,
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
					 service_id);
		result_set = tracker_db_interface_execute_query (iface,
								 NULL,
								 query,
								 NULL);
		g_free (query);
	} else {
		gchar *id_str;

		id_str = tracker_guint32_to_string (service_id);

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
			tracker_data_query_content (service_def, service_id);
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

gchar *
tracker_data_query_content (TrackerService *service,
			    guint32	    service_id)
{
	TrackerDBInterface *iface;
	TrackerField	   *field;
	gchar		   *service_id_str, *contents = NULL;
	TrackerDBResultSet *result_set;

	service_id_str = tracker_guint32_to_string (service_id);
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

gboolean
tracker_data_query_first_removed_service (TrackerDBInterface *iface,
					  guint32            *service_id)
{
	TrackerDBResultSet *result_set;

	result_set = tracker_db_interface_execute_procedure (iface, NULL,
							     "GetFirstRemovedFile",
							     NULL);

	if (result_set) {
		guint32 id;

		tracker_db_result_set_get (result_set, 0, &id, -1);
		g_object_unref (result_set);

		if (service_id) {
			*service_id = id;
		}

		return TRUE;
	}

	return FALSE;
}
