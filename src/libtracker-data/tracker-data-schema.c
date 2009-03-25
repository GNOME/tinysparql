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
#include <fcntl.h>

#include <libtracker-common/tracker-file-utils.h>
#include <libtracker-common/tracker-nfs-lock.h>
#include <libtracker-common/tracker-parser.h>
#include <libtracker-common/tracker-type-utils.h>
#include <libtracker-common/tracker-utils.h>

#include <libtracker-db/tracker-db-index.h>
#include <libtracker-db/tracker-db-interface-sqlite.h>
#include <libtracker-db/tracker-db-index-manager.h>
#include <libtracker-db/tracker-db-manager.h>

#include "tracker-data-manager.h"
#include "tracker-data-schema.h"

GArray *
tracker_data_schema_create_service_array (const gchar *service,
					  gboolean     basic_services)
{
	GArray	 *array;

	if (service) {
		array = tracker_ontology_get_subcategory_ids (service);
	} else if (basic_services) {
		array = tracker_ontology_get_subcategory_ids ("Files");
	} else {
		array = tracker_ontology_get_subcategory_ids ("*");
	}

	return array;
}

gchar *
tracker_data_schema_get_field_name (const gchar *service,
				    const gchar *meta_name)
{
	gint key_field;

	/* Replace with tracker_ontology_get_field_name_by_service_name */
	key_field = tracker_ontology_service_get_key_metadata (service, meta_name);

	if (key_field > 0) {
		return g_strdup_printf ("KeyMetadata%d", key_field);
	}

	if (strcasecmp (meta_name, "File:Path") == 0)	  return g_strdup ("Path");
	if (strcasecmp (meta_name, "File:Name") == 0)	  return g_strdup ("Name");
	if (strcasecmp (meta_name, "File:Mime") == 0)	  return g_strdup ("Mime");
	if (strcasecmp (meta_name, "File:Size") == 0)	  return g_strdup ("Size");
	if (strcasecmp (meta_name, "File:Rank") == 0)	  return g_strdup ("Rank");
	if (strcasecmp (meta_name, "File:Modified") == 0) return g_strdup ("IndexTime");

	return NULL;
}

gchar *
tracker_data_schema_metadata_field_get_related_names (TrackerDBInterface *iface,
						      const gchar	 *name)
{
	TrackerDBResultSet *result_set;

	g_return_val_if_fail (TRACKER_IS_DB_INTERFACE (iface), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	result_set = tracker_data_manager_exec_proc (iface,
					   "GetMetadataAliasesForName",
					   name,
					   name,
					   NULL);

	if (result_set) {
		GString  *s = NULL;
		gboolean  valid = TRUE;
		gint	  id;

		while (valid) {
			tracker_db_result_set_get (result_set, 1, &id, -1);

			if (s) {
				g_string_append_printf (s, ", %d", id);
			} else {
				s = g_string_new ("");
				g_string_append_printf (s, "%d", id);
			}

			valid = tracker_db_result_set_iter_next (result_set);
		}

		g_object_unref (result_set);

		return g_string_free (s, FALSE);
	}

	return NULL;
}

const gchar *
tracker_data_schema_metadata_field_get_db_table (TrackerFieldType type)
{
	switch (type) {
	case TRACKER_FIELD_TYPE_INDEX:
	case TRACKER_FIELD_TYPE_STRING:
	case TRACKER_FIELD_TYPE_DOUBLE:
		return "ServiceMetaData";

	case TRACKER_FIELD_TYPE_INTEGER:
	case TRACKER_FIELD_TYPE_DATE:
		return "ServiceNumericMetaData";

	case TRACKER_FIELD_TYPE_BLOB:
		return "ServiceBlobMetaData";

	case TRACKER_FIELD_TYPE_KEYWORD:
		return "ServiceKeywordMetaData";

	default:
	case TRACKER_FIELD_TYPE_FULLTEXT:
	case TRACKER_FIELD_TYPE_STRUCT:
	case TRACKER_FIELD_TYPE_LINK:
		break;
	}

	return NULL;
}

TrackerFieldData *
tracker_data_schema_get_metadata_field (TrackerDBInterface *iface,
				        const gchar	   *service,
				        const gchar	   *field_name,
				        gint		    field_count,
				        gboolean	    is_select,
				        gboolean	    is_condition)
{
	TrackerFieldData *field_data = NULL;
	TrackerField	 *def;

	g_return_val_if_fail (TRACKER_IS_DB_INTERFACE (iface), NULL);
	g_return_val_if_fail (service != NULL, NULL);
	g_return_val_if_fail (field_name != NULL, NULL);

	def = tracker_ontology_get_field_by_name (field_name);

	if (def) {
		gchar	    *alias;
		const gchar *table_name;
		gchar	    *this_field_name;
		gchar	    *where_field;
		gchar       *order_field;
		gint         key_collate;

		field_data = g_object_new (TRACKER_TYPE_FIELD_DATA,
					   "is-select", is_select,
					   "is-condition", is_condition,
					   "field-name", field_name,
					   NULL);

		alias = g_strdup_printf ("M%d", field_count);
		table_name = tracker_data_schema_metadata_field_get_db_table (tracker_field_get_data_type (def));

		g_debug ("Field_name: %s :table_name is: %s for data_type: %i",
			 field_name,
			 table_name,
			 tracker_field_get_data_type(def));

		tracker_field_data_set_alias (field_data, alias);
		tracker_field_data_set_table_name (field_data, table_name);
		tracker_field_data_set_id_field (field_data, tracker_field_get_id (def));
		tracker_field_data_set_data_type (field_data, tracker_field_get_data_type (def));
		tracker_field_data_set_multiple_values (field_data, tracker_field_get_multiple_values (def));

		this_field_name = tracker_data_schema_get_field_name (service, field_name);

		if (this_field_name) {
			gchar *str;

			str = g_strdup_printf (" S.%s ", this_field_name);
			tracker_field_data_set_select_field (field_data, str);
			tracker_field_data_set_needs_join (field_data, FALSE);
			g_free (str);
			g_free (this_field_name);
		} else {
			gchar *str;
			gchar *display_field;

			display_field = tracker_ontology_field_get_display_name (def);
			str = g_strdup_printf ("M%d.%s", field_count, display_field);
			tracker_field_data_set_select_field (field_data, str);
			tracker_field_data_set_needs_join (field_data, TRUE);
			g_free (str);
			g_free (display_field);
		}

		if ((tracker_field_get_data_type (def) == TRACKER_FIELD_TYPE_DOUBLE) ||
		    (tracker_field_get_data_type (def) == TRACKER_FIELD_TYPE_INDEX)  ||
		    (tracker_field_get_data_type (def) == TRACKER_FIELD_TYPE_STRING)) {
			where_field = g_strdup_printf ("M%d.MetaDataDisplay", field_count);
		} else {
			where_field = g_strdup_printf ("M%d.MetaDataValue", field_count);
		}

		tracker_field_data_set_where_field (field_data, where_field);

		key_collate = tracker_ontology_service_get_key_metadata (service, field_name);

		if (key_collate > 0 && key_collate <= 5) {
			gchar *str;

			str = g_strdup_printf (" S.KeyMetadataCollation%d", key_collate);
			tracker_field_data_set_order_field (field_data, str);
			tracker_field_data_set_needs_collate (field_data, FALSE);
			g_free (str);
		} else if (key_collate >= 6 && key_collate <= 8) {
			gchar *str;
			
			str = g_strdup_printf (" S.KeyMetadata%d", key_collate);
			tracker_field_data_set_order_field (field_data, str);
			tracker_field_data_set_needs_collate (field_data, FALSE);
			g_free (str);
		} else {
			if ((tracker_field_get_data_type (def) == TRACKER_FIELD_TYPE_DOUBLE) ||
			    (tracker_field_get_data_type (def) == TRACKER_FIELD_TYPE_INDEX)  ||
			    (tracker_field_get_data_type (def) == TRACKER_FIELD_TYPE_STRING)) {
				order_field = g_strdup_printf ("M%d.MetaDataCollation", field_count);
			} else {
				order_field = g_strdup_printf ("M%d.MetaDataValue", field_count);
			}
			tracker_field_data_set_needs_collate (field_data, TRUE);
			tracker_field_data_set_order_field (field_data, order_field);
			g_free (order_field);
		}
		
		tracker_field_data_set_needs_null (field_data, FALSE);
		g_free (where_field);
		g_free (alias);
	}

	return field_data;
}

