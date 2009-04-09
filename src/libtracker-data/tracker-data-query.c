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
#include "tracker-sparql-query.h"

static gchar *
get_string_for_value (GValue *value)
{
	switch (G_VALUE_TYPE (value)) {
	case G_TYPE_INT:
		return g_strdup_printf ("%d", g_value_get_int (value));
	case G_TYPE_DOUBLE:
		return g_strdup_printf ("%f", g_value_get_double (value));
	case G_TYPE_STRING:
		return g_strdup (g_value_get_string (value));
	default:
		return NULL;
	}
}

GPtrArray *
tracker_data_query_all_metadata (guint32 resource_id) 
{
	TrackerDBInterface *iface;
	TrackerDBStatement *stmt;
	TrackerDBResultSet *result_set, *single_result_set, *multi_result_set;
	TrackerClass	   *class;
	GString		   *sql;
	GPtrArray          *result;
	TrackerProperty	  **properties, **property;
	gchar		   *class_uri;
	int		    i;
	gboolean            first;
	gchar		  **item;
	GValue		    value = { 0 };

	result = g_ptr_array_new ();

	iface = tracker_db_manager_get_db_interface ();

	properties = tracker_ontology_get_properties ();

	stmt = tracker_db_interface_create_statement (iface, "SELECT (SELECT Uri FROM \"rdfs:Resource\" WHERE ID = \"rdf:type\") FROM \"rdfs:Resource_rdf:type\" WHERE ID = ?");
	tracker_db_statement_bind_int (stmt, 0, resource_id);
	result_set = tracker_db_statement_execute (stmt, NULL);
	g_object_unref (stmt);

	if (result_set) {
		do {
			tracker_db_result_set_get (result_set, 0, &class_uri, -1);

			class = tracker_ontology_get_class_by_uri (class_uri);
			if (class == NULL) {
				g_warning ("Class '%s' not found in the ontology", class_uri);
				g_free (class_uri);
				continue;
			}

			/* retrieve single value properties for current class */

			sql = g_string_new ("SELECT ");

			first = TRUE;
			for (property = properties; *property; property++) {
				if (tracker_property_get_domain (*property) == class) {
					if (!tracker_property_get_multiple_values (*property)) {
						if (!first) {
							g_string_append (sql, ", ");
						}
						first = FALSE;

						if (tracker_property_get_data_type (*property) == TRACKER_PROPERTY_TYPE_RESOURCE) {
							g_string_append_printf (sql, "(SELECT Uri FROM \"rdfs:Resource\" WHERE ID = \"%s\")", tracker_property_get_name (*property));
						} else {
							g_string_append_printf (sql, "\"%s\"", tracker_property_get_name (*property));
						}
					}
				}
			}

			if (!first) {
				g_string_append_printf (sql, " FROM \"%s\" WHERE ID = ?", tracker_class_get_name (class));
				stmt = tracker_db_interface_create_statement (iface, "%s", sql->str);
				tracker_db_statement_bind_int (stmt, 0, resource_id);
				single_result_set = tracker_db_statement_execute (stmt, NULL);
				g_object_unref (stmt);
			}

			g_string_free (sql, TRUE);

			i = 0;
			for (property = properties; *property; property++) {
				if (tracker_property_get_domain (*property) != class) {
					continue;
				}

				if (!tracker_property_get_multiple_values (*property)) {
					/* single value property, value in single_result_set */

					_tracker_db_result_set_get_value (single_result_set, i++, &value);
					if (G_VALUE_TYPE (&value) == 0) {
						/* NULL, property not set */
						continue;
					}

					/* Item is a pair (property_name, value) */
					item = g_new0 (gchar *, 2);

					item[0] = g_strdup (tracker_property_get_name (*property));
					item[1] = get_string_for_value (&value);

					g_value_unset (&value);

					g_ptr_array_add (result, item);
				} else {
					/* multi value property, retrieve values from DB */

					sql = g_string_new ("SELECT ");

					if (tracker_property_get_data_type (*property) == TRACKER_PROPERTY_TYPE_RESOURCE) {
						g_string_append_printf (sql, "(SELECT Uri FROM \"rdfs:Resource\" WHERE ID = \"%s\")", tracker_property_get_name (*property));
					} else {
						g_string_append_printf (sql, "\"%s\"", tracker_property_get_name (*property));
					}

					g_string_append_printf (sql,
								" FROM \"%s_%s\" WHERE ID = ?",
								tracker_class_get_name (tracker_property_get_domain (*property)),
								tracker_property_get_name (*property));

					stmt = tracker_db_interface_create_statement (iface, "%s", sql->str);
					tracker_db_statement_bind_int (stmt, 0, resource_id);
					multi_result_set = tracker_db_statement_execute (stmt, NULL);
					g_object_unref (stmt);

					if (multi_result_set) {
						do {

							/* Item is a pair (property_name, value) */
							item = g_new0 (gchar *, 2);

							item[0] = g_strdup (tracker_property_get_name (*property));

							_tracker_db_result_set_get_value (multi_result_set, 0, &value);
							item[1] = get_string_for_value (&value);
							g_value_unset (&value);

							g_ptr_array_add (result, item);
						} while (tracker_db_result_set_iter_next (multi_result_set));

						g_object_unref (multi_result_set);
					}

					g_string_free (sql, TRUE);
				}
			}

			if (!first) {
				g_object_unref (single_result_set);
			}

			g_free (class_uri);
		} while (tracker_db_result_set_iter_next (result_set));

		g_object_unref (result_set);
	}

	g_free (properties);

	return result;

}

guint32
tracker_data_query_resource_id (const gchar	   *uri)
{
	TrackerDBResultSet *result_set;
	TrackerDBInterface *iface;
	TrackerDBStatement *stmt;
	guint32		    id = 0;

	g_return_val_if_fail (uri != NULL, 0);

	iface = tracker_db_manager_get_db_interface ();

	stmt = tracker_db_interface_create_statement (iface,
		"SELECT ID FROM \"rdfs:Resource\" WHERE Uri = ?");
	tracker_db_statement_bind_text (stmt, 0, uri);
	result_set = tracker_db_statement_execute (stmt, NULL);
	g_object_unref (stmt);

	if (result_set) {
		tracker_db_result_set_get (result_set, 0, &id, -1);
		g_object_unref (result_set);
	}

	return id;
}

gboolean
tracker_data_query_resource_exists (const gchar	  *uri,
				   guint32	  *service_id,
				   time_t	  *mtime)
{
	TrackerDBInterface *iface;
	TrackerDBStatement *stmt;
	TrackerDBResultSet *result_set;
	guint db_id;
	guint db_mtime;
	gboolean found = FALSE;

	db_id = db_mtime = 0;

	iface = tracker_db_manager_get_db_interface ();

	stmt = tracker_db_interface_create_statement (iface,
		"SELECT ID, Modified FROM \"rdfs:Resource\" WHERE Uri = ?");
	tracker_db_statement_bind_text (stmt, 0, uri);
	result_set = tracker_db_statement_execute (stmt, NULL);
	g_object_unref (stmt);

	if (result_set) {
		tracker_db_result_set_get (result_set,
					   0, &db_id,
					   1, &db_mtime,
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

	return found;
}


/* TODO */
#if 0
TrackerDBResultSet *
tracker_data_query_backup_metadata (TrackerClass *service)
{
	TrackerDBInterface *iface;
	TrackerDBResultSet *result_set;

	g_return_val_if_fail (TRACKER_IS_CLASS (service), NULL);

	iface = tracker_db_manager_get_db_interface_by_service (tracker_class_get_name (service));

	result_set = tracker_data_manager_exec_proc (iface,
						     "GetUserMetadataBackup", 
						     NULL);
	return result_set;
}
#endif

gchar *
tracker_data_query_property_value (const gchar *subject,
				   const gchar *predicate)
{
	TrackerDBInterface  *iface;
	TrackerDBStatement  *stmt;
	TrackerDBResultSet  *result_set;
	TrackerProperty *property;
	guint32		    subject_id;
	const gchar *table_name, *field_name;
	gchar *result = NULL;

	property = tracker_ontology_get_property_by_uri (predicate);

	/* only single-value field supported */
	g_return_val_if_fail (!tracker_property_get_multiple_values (property), NULL);

	iface = tracker_db_manager_get_db_interface ();

	table_name = tracker_class_get_name (tracker_property_get_domain (property));
	field_name = tracker_property_get_name (property);

	subject_id = tracker_data_query_resource_id (subject);

	if (tracker_property_get_data_type (property) == TRACKER_PROPERTY_TYPE_RESOURCE) {
		/* retrieve object URI */
		stmt = tracker_db_interface_create_statement (iface,
			"SELECT "
			"(SELECT Uri FROM \"rdfs:Resource\" WHERE ID = \"%s\") "
			"FROM \"%s\" WHERE ID = ?",
			field_name, table_name);
	} else {
		/* retrieve literal */
		stmt = tracker_db_interface_create_statement (iface,
			"SELECT \"%s\" FROM \"%s\" WHERE ID = ?",
			field_name, table_name);
	}

	tracker_db_statement_bind_int (stmt, 0, subject_id);
	result_set = tracker_db_statement_execute (stmt, NULL);

	if (result_set) {
		GValue value = { 0 };

		_tracker_db_result_set_get_value (result_set, 0, &value);
		result = get_string_for_value (&value);

		g_object_unref (result_set);
	}

	g_object_unref (stmt);

	return result;
}

gchar **
tracker_data_query_property_values (const gchar *subject,
				    const gchar *predicate)
{
	TrackerDBInterface *iface;
	TrackerDBStatement  *stmt;
	TrackerDBResultSet *result_set;
	TrackerProperty *property;
	gchar		  **result = NULL;
	guint32             subject_id;
	gboolean            multiple_values;
	gchar              *table_name;
	const gchar        *field_name;

	property = tracker_ontology_get_property_by_uri (predicate);

	/* multi or single-value field supported */

	iface = tracker_db_manager_get_db_interface ();

	multiple_values = tracker_property_get_multiple_values (property);
	if (multiple_values) {
		table_name = g_strdup_printf ("%s_%s",
			tracker_class_get_name (tracker_property_get_domain (property)),
			tracker_property_get_name (property));
	} else {
		table_name = g_strdup (tracker_class_get_name (tracker_property_get_domain (property)));
	}
	field_name = tracker_property_get_name (property);

	subject_id = tracker_data_query_resource_id (subject);

	if (tracker_property_get_data_type (property) == TRACKER_PROPERTY_TYPE_RESOURCE) {
		/* retrieve object URI */
		stmt = tracker_db_interface_create_statement (iface,
			"SELECT "
			"(SELECT Uri FROM \"rdfs:Resource\" WHERE ID = \"%s\") "
			"FROM \"%s\" WHERE ID = ?",
			field_name, table_name);
	} else {
		/* retrieve literal */
		stmt = tracker_db_interface_create_statement (iface,
			"SELECT \"%s\" FROM \"%s\" WHERE ID = ?",
			field_name, table_name);
	}

	tracker_db_statement_bind_int (stmt, 0, subject_id);
	result_set = tracker_db_statement_execute (stmt, NULL);

	if (result_set) {
		gint i = 0;

		result = g_new0 (gchar *, tracker_db_result_set_get_n_rows (result_set + 1));

		do {
			GValue value = { 0 };

			_tracker_db_result_set_get_value (result_set, 0, &value);
			result[i++] = get_string_for_value (&value);
		} while (tracker_db_result_set_iter_next (result_set));

		g_object_unref (result_set);
	} else {
		result = g_new0 (gchar *, 1);
	}

	g_object_unref (stmt);
	g_free (table_name);

	return result;
}


TrackerDBResultSet *
tracker_data_query_sparql (const gchar  *query,
			   GError      **error)
{
	TrackerSparqlQuery *sparql_query;
	TrackerDBResultSet *result_set;

	g_return_val_if_fail (query != NULL, NULL);

	sparql_query = tracker_sparql_query_new (query);

	result_set = tracker_sparql_query_execute (sparql_query, error);

	g_object_unref (sparql_query);

	return result_set;
}

