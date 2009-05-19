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
#include <zlib.h>

#include <glib/gstdio.h>

#include <libtracker-common/tracker-file-utils.h>
#include <libtracker-common/tracker-nfs-lock.h>
#include <libtracker-common/tracker-parser.h>
#include <libtracker-common/tracker-type-utils.h>
#include <libtracker-common/tracker-utils.h>
#include <libtracker-common/tracker-ontology.h>

#include <libtracker-db/tracker-db-interface-sqlite.h>
#include <libtracker-db/tracker-db-manager.h>

#include "tracker-data-manager.h"
#include "tracker-data-update.h"
#include "tracker-turtle.h"

#define RDF_PREFIX TRACKER_RDF_PREFIX
#define RDF_PROPERTY RDF_PREFIX "Property"
#define RDF_TYPE RDF_PREFIX "type"

#define RDFS_PREFIX TRACKER_RDFS_PREFIX
#define RDFS_CLASS RDFS_PREFIX "Class"
#define RDFS_DOMAIN RDFS_PREFIX "domain"
#define RDFS_RANGE RDFS_PREFIX "range"
#define RDFS_SUB_CLASS_OF RDFS_PREFIX "subClassOf"
#define RDFS_SUB_PROPERTY_OF RDFS_PREFIX "subPropertyOf"

#define NRL_PREFIX TRACKER_NRL_PREFIX
#define NRL_MAX_CARDINALITY NRL_PREFIX "maxCardinality"

#define TRACKER_PREFIX TRACKER_TRACKER_PREFIX

#define ZLIBBUFSIZ 8192

typedef struct {
	TrackerConfig	*config;
	TrackerLanguage *language;
} TrackerDBPrivate;

/* Private */
static GStaticPrivate private_key = G_STATIC_PRIVATE_INIT;

static gchar		  *ontologies_dir;

static void
private_free (gpointer data)
{
	TrackerDBPrivate *private;

	private = data;

	if (private->config) {
		g_object_unref (private->config);
	}

	if (private->language) {
		g_object_unref (private->language);
	}

	g_free (private);
}


static void
load_ontology_file_from_path (const gchar	 *ontology_file)
{
	tracker_turtle_reader_init (ontology_file, NULL);
	while (tracker_turtle_reader_next ()) {
		const gchar *subject, *predicate, *object;

		subject = tracker_turtle_reader_get_subject ();
		predicate = tracker_turtle_reader_get_predicate ();
		object = tracker_turtle_reader_get_object ();

		if (strcmp (predicate, RDF_TYPE) == 0) {
			if (strcmp (object, RDFS_CLASS) == 0) {
				TrackerClass *class;

				if (tracker_ontology_get_class_by_uri (subject) != NULL) {
					g_critical ("%s: Duplicate definition of class %s", ontology_file, subject);
					continue;
				}

				class = tracker_class_new ();
				tracker_class_set_uri (class, subject);
				tracker_ontology_add_class (class);
				g_object_unref (class);
			} else if (strcmp (object, RDF_PROPERTY) == 0) {
				TrackerProperty *property;

				if (tracker_ontology_get_property_by_uri (subject) != NULL) {
					g_critical ("%s: Duplicate definition of property %s", ontology_file, subject);
					continue;
				}

				property = tracker_property_new ();
				tracker_property_set_uri (property, subject);
				tracker_ontology_add_property (property);
				g_object_unref (property);
			} else if (strcmp (object, TRACKER_PREFIX "Namespace") == 0) {
				TrackerNamespace *namespace;

				if (tracker_ontology_get_namespace_by_uri (subject) != NULL) {
					g_critical ("%s: Duplicate definition of namespace %s", ontology_file, subject);
					continue;
				}

				namespace = tracker_namespace_new ();
				tracker_namespace_set_uri (namespace, subject);
				tracker_ontology_add_namespace (namespace);
				g_object_unref (namespace);
			}
		} else if (strcmp (predicate, RDFS_SUB_CLASS_OF) == 0) {
			TrackerClass *class, *super_class;

			class = tracker_ontology_get_class_by_uri (subject);
			if (class == NULL) {
				g_critical ("%s: Unknown class %s", ontology_file, subject);
				continue;
			}

			super_class = tracker_ontology_get_class_by_uri (object);
			if (super_class == NULL) {
				g_critical ("%s: Unknown class %s", ontology_file, object);
				continue;
			}

			tracker_class_add_super_class (class, super_class);
		} else if (strcmp (predicate, RDFS_SUB_PROPERTY_OF) == 0) {
			TrackerProperty *property, *super_property;

			property = tracker_ontology_get_property_by_uri (subject);
			if (property == NULL) {
				g_critical ("%s: Unknown property %s", ontology_file, subject);
				continue;
			}

			super_property = tracker_ontology_get_property_by_uri (object);
			if (super_property == NULL) {
				g_critical ("%s: Unknown property %s", ontology_file, object);
				continue;
			}

			tracker_property_add_super_property (property, super_property);
		} else if (strcmp (predicate, RDFS_DOMAIN) == 0) {
			TrackerProperty *property;
			TrackerClass *domain;

			property = tracker_ontology_get_property_by_uri (subject);
			if (property == NULL) {
				g_critical ("%s: Unknown property %s", ontology_file, subject);
				continue;
			}

			domain = tracker_ontology_get_class_by_uri (object);
			if (domain == NULL) {
				g_critical ("%s: Unknown class %s", ontology_file, object);
				continue;
			}

			tracker_property_set_domain (property, domain);
		} else if (strcmp (predicate, RDFS_RANGE) == 0) {
			TrackerProperty *property;
			TrackerClass *range;

			property = tracker_ontology_get_property_by_uri (subject);
			if (property == NULL) {
				g_critical ("%s: Unknown property %s", ontology_file, subject);
				continue;
			}

			range = tracker_ontology_get_class_by_uri (object);
			if (range == NULL) {
				g_critical ("%s: Unknown class %s", ontology_file, object);
				continue;
			}

			tracker_property_set_range (property, range);
		} else if (strcmp (predicate, NRL_MAX_CARDINALITY) == 0) {
			TrackerProperty *property;

			property = tracker_ontology_get_property_by_uri (subject);
			if (property == NULL) {
				g_critical ("%s: Unknown property %s", ontology_file, subject);
				continue;
			}

			if (atoi (object) == 1) {
				tracker_property_set_multiple_values (property, FALSE);
			}
		} else if (strcmp (predicate, TRACKER_PREFIX "indexed") == 0) {
			TrackerProperty *property;

			property = tracker_ontology_get_property_by_uri (subject);
			if (property == NULL) {
				g_critical ("%s: Unknown property %s", ontology_file, subject);
				continue;
			}

			if (strcmp (object, "true") == 0) {
				tracker_property_set_indexed (property, TRUE);
			}
		} else if (strcmp (predicate, TRACKER_PREFIX "transient") == 0) {
			TrackerProperty *property;

			property = tracker_ontology_get_property_by_uri (subject);
			if (property == NULL) {
				g_critical ("%s: Unknown property %s", ontology_file, subject);
				continue;
			}

			if (strcmp (object, "true") == 0) {
				tracker_property_set_transient (property, TRUE);
			}
		} else if (strcmp (predicate, TRACKER_PREFIX "fulltextIndexed") == 0) {
			TrackerProperty *property;

			property = tracker_ontology_get_property_by_uri (subject);
			if (property == NULL) {
				g_critical ("%s: Unknown property %s", ontology_file, subject);
				continue;
			}

			if (strcmp (object, "true") == 0) {
				tracker_property_set_fulltext_indexed (property, TRUE);
			}
		} else if (strcmp (predicate, TRACKER_PREFIX "prefix") == 0) {
			TrackerNamespace *namespace;

			namespace = tracker_ontology_get_namespace_by_uri (subject);
			if (namespace == NULL) {
				g_critical ("%s: Unknown namespace %s", ontology_file, subject);
				continue;
			}

			tracker_namespace_set_prefix (namespace, object);
		}
	}
}

static void
load_ontology_file (const gchar	      *filename)
{
	gchar		*ontology_file;

	ontology_file = g_build_filename (ontologies_dir, filename, NULL);
	load_ontology_file_from_path (ontology_file);
	g_free (ontology_file);
}

static void
import_ontology_file_from_path (const gchar	 *ontology_file)
{
	tracker_turtle_reader_init (ontology_file, NULL);
	while (tracker_turtle_reader_next ()) {
		tracker_data_insert_statement (
			tracker_turtle_reader_get_subject (),
			tracker_turtle_reader_get_predicate (),
			tracker_turtle_reader_get_object ());
	}
}

static void
import_ontology_file (const gchar	      *filename)
{
	gchar		*ontology_file;

	ontology_file = g_build_filename (ontologies_dir, filename, NULL);
	import_ontology_file_from_path (ontology_file);
	g_free (ontology_file);
}

static void
class_add_super_classes_from_db (TrackerDBInterface *iface, TrackerClass *class)
{
	TrackerDBStatement *stmt;
	TrackerDBResultSet *result_set;

	stmt = tracker_db_interface_create_statement (iface,
						      "SELECT (SELECT Uri FROM \"rdfs:Resource\" WHERE ID = \"rdfs:subClassOf\") "
						      "FROM \"rdfs:Class_rdfs:subClassOf\" "
						      "WHERE ID = (SELECT ID FROM \"rdfs:Resource\" WHERE Uri = ?)");
	tracker_db_statement_bind_text (stmt, 0, tracker_class_get_uri (class));
	result_set = tracker_db_statement_execute (stmt, NULL);
	g_object_unref (stmt);

	if (result_set) {
		gboolean valid = TRUE;

		while (valid) {
			TrackerClass *super_class;
			gchar *super_class_uri;

			tracker_db_result_set_get (result_set, 0, &super_class_uri, -1);
			super_class = tracker_ontology_get_class_by_uri (super_class_uri);
			tracker_class_add_super_class (class, super_class);

			g_free (super_class_uri);

			valid = tracker_db_result_set_iter_next (result_set);
		}

		g_object_unref (result_set);
	}
}

static void
property_add_super_properties_from_db (TrackerDBInterface *iface, TrackerProperty *property)
{
	TrackerDBStatement *stmt;
	TrackerDBResultSet *result_set;

	stmt = tracker_db_interface_create_statement (iface,
						      "SELECT (SELECT Uri FROM \"rdfs:Resource\" WHERE ID = \"rdfs:subPropertyOf\") "
						      "FROM \"rdf:Property_rdfs:subPropertyOf\" "
						      "WHERE ID = (SELECT ID FROM \"rdfs:Resource\" WHERE Uri = ?)");
	tracker_db_statement_bind_text (stmt, 0, tracker_property_get_uri (property));
	result_set = tracker_db_statement_execute (stmt, NULL);
	g_object_unref (stmt);

	if (result_set) {
		gboolean valid = TRUE;

		while (valid) {
			TrackerProperty *super_property;
			gchar *super_property_uri;

			tracker_db_result_set_get (result_set, 0, &super_property_uri, -1);
			super_property = tracker_ontology_get_property_by_uri (super_property_uri);
			tracker_property_add_super_property (property, super_property);

			g_free (super_property_uri);

			valid = tracker_db_result_set_iter_next (result_set);
		}

		g_object_unref (result_set);
	}
}

static void
db_get_static_data (TrackerDBInterface *iface)
{
	TrackerDBStatement *stmt;
	TrackerDBResultSet *result_set;

	stmt = tracker_db_interface_create_statement (iface,
						      "SELECT (SELECT Uri FROM \"rdfs:Resource\" WHERE ID = \"tracker:Namespace\".ID), "
						      "\"tracker:prefix\" "
						      "FROM \"tracker:Namespace\"");
	result_set = tracker_db_statement_execute (stmt, NULL);
	g_object_unref (stmt);

	if (result_set) {
		gboolean valid = TRUE;

		while (valid) {
			TrackerNamespace *namespace;
			gchar	         *uri, *prefix;

			namespace = tracker_namespace_new ();

			tracker_db_result_set_get (result_set,
						   0, &uri,
						   1, &prefix,
						   -1);

			tracker_namespace_set_uri (namespace, uri);
			tracker_namespace_set_prefix (namespace, prefix);
			tracker_ontology_add_namespace (namespace);

			g_object_unref (namespace);
			g_free (uri);
			g_free (prefix);

			valid = tracker_db_result_set_iter_next (result_set);
		}

		g_object_unref (result_set);
	}

	stmt = tracker_db_interface_create_statement (iface,
						      "SELECT (SELECT Uri FROM \"rdfs:Resource\" WHERE ID = \"rdfs:Class\".ID) "
						      "FROM \"rdfs:Class\" ORDER BY ID");
	result_set = tracker_db_statement_execute (stmt, NULL);
	g_object_unref (stmt);

	if (result_set) {
		gboolean valid = TRUE;

		while (valid) {
			TrackerClass *class;
			gchar	     *uri;

			class = tracker_class_new ();

			tracker_db_result_set_get (result_set,
						   0, &uri,
						   -1);

			tracker_class_set_uri (class, uri);
			class_add_super_classes_from_db (iface, class);
			tracker_ontology_add_class (class);

			g_object_unref (class);
			g_free (uri);

			valid = tracker_db_result_set_iter_next (result_set);
		}

		g_object_unref (result_set);
	}

	stmt = tracker_db_interface_create_statement (iface,
						      "SELECT (SELECT Uri FROM \"rdfs:Resource\" WHERE ID = \"rdf:Property\".ID), "
						      "(SELECT Uri FROM \"rdfs:Resource\" WHERE ID = \"rdfs:domain\"), "
						      "(SELECT Uri FROM \"rdfs:Resource\" WHERE ID = \"rdfs:range\"), "
						      "\"nrl:maxCardinality\", "
						      "\"tracker:indexed\", "
						      "\"tracker:fulltextIndexed\", "
						      "\"tracker:transient\" "
						      "FROM \"rdf:Property\" ORDER BY ID");
	result_set = tracker_db_statement_execute (stmt, NULL);
	g_object_unref (stmt);

	if (result_set) {
		gboolean valid = TRUE;

		while (valid) {
			GValue value = { 0 };
			TrackerProperty *property;
			gchar	        *uri, *domain_uri, *range_uri;
			gboolean         multi_valued, indexed, fulltext_indexed;
			gboolean         transient = FALSE;

			property = tracker_property_new ();

			tracker_db_result_set_get (result_set,
						   0, &uri,
						   1, &domain_uri,
						   2, &range_uri,
						   -1);

			_tracker_db_result_set_get_value (result_set, 3, &value);
			if (G_VALUE_TYPE (&value) != 0) {
				multi_valued = (g_value_get_int (&value) > 1);
				g_value_unset (&value);
			} else {
				/* nrl:maxCardinality not set
				   not limited to single value */
				multi_valued = TRUE;
			}

			_tracker_db_result_set_get_value (result_set, 4, &value);
			if (G_VALUE_TYPE (&value) != 0) {
				indexed = (g_value_get_int (&value) == 1);
				g_value_unset (&value);
			} else {
				/* NULL */
				indexed = FALSE;
			}

			_tracker_db_result_set_get_value (result_set, 5, &value);
			if (G_VALUE_TYPE (&value) != 0) {
				fulltext_indexed = (g_value_get_int (&value) == 1);
				g_value_unset (&value);
			} else {
				/* NULL */
				fulltext_indexed = FALSE;
			}

			_tracker_db_result_set_get_value (result_set, 6, &value);
			if (G_VALUE_TYPE (&value) != 0) {
				transient = (g_value_get_int (&value) == 1);
				g_value_unset (&value);
			} else {
				/* NULL */
				transient = FALSE;
			}

			tracker_property_set_transient (property, transient);
			tracker_property_set_uri (property, uri);
			tracker_property_set_domain (property, tracker_ontology_get_class_by_uri (domain_uri));
			tracker_property_set_range (property, tracker_ontology_get_class_by_uri (range_uri));
			tracker_property_set_multiple_values (property, multi_valued);
			tracker_property_set_indexed (property, indexed);
			tracker_property_set_fulltext_indexed (property, fulltext_indexed);
			property_add_super_properties_from_db (iface, property);
			tracker_ontology_add_property (property);

			g_object_unref (property);
			g_free (uri);
			g_free (domain_uri);
			g_free (range_uri);

			valid = tracker_db_result_set_iter_next (result_set);
		}

		g_object_unref (result_set);
	}
}

static void
create_decomposed_metadata_property_table (TrackerDBInterface *iface, 
					   TrackerProperty   **property, 
					   const gchar        *service_name,
					   const gchar       **sql_type_for_single_value)
{
	const char *field_name;
	const char *sql_type;
	gboolean    transient;

	field_name = tracker_property_get_name (*property);

	transient = !sql_type_for_single_value;

	if (!transient) {
		transient = tracker_property_get_transient (*property);
	}

	switch (tracker_property_get_data_type (*property)) {
	case TRACKER_PROPERTY_TYPE_STRING:
		sql_type = "TEXT";
		break;
	case TRACKER_PROPERTY_TYPE_INTEGER:
	case TRACKER_PROPERTY_TYPE_BOOLEAN:
	case TRACKER_PROPERTY_TYPE_DATE:
	case TRACKER_PROPERTY_TYPE_DATETIME:
	case TRACKER_PROPERTY_TYPE_RESOURCE:
		sql_type = "INTEGER";
		break;
	case TRACKER_PROPERTY_TYPE_DOUBLE:
		sql_type = "REAL";
		break;
	default:
		sql_type = "";
		break;
	}

	/* TODO: When we refactor to having writes in trackerd, we can use 
	 * TEMPORARY tables instead of deleting and storing physically */

	if (transient || tracker_property_get_multiple_values (*property)) {
		/* multiple values */
		if (tracker_property_get_indexed (*property)) {
			/* use different UNIQUE index for properties whose
			 * value should be indexed to minimize index size */
			tracker_db_interface_execute_query (iface, NULL,
				"CREATE %sTABLE \"%s_%s\" ("
				"ID INTEGER NOT NULL, "
				"\"%s\" %s NOT NULL, "
				"UNIQUE (\"%s\", ID))",
				transient ? "" /*"TEMPORARY "*/ : "",
				service_name,
				field_name,
				field_name,
				sql_type,
				field_name);

			tracker_db_interface_execute_query (iface, NULL,
				"CREATE INDEX \"%s_%s_ID\" ON \"%s_%s\" (ID)",
				service_name,
				field_name,
				service_name,
				field_name);
		} else {
			/* we still have to include the property value in
			 * the unique index for proper constraints */
			tracker_db_interface_execute_query (iface, NULL,
				"CREATE %sTABLE \"%s_%s\" ("
				"ID INTEGER NOT NULL, "
				"\"%s\" %s NOT NULL, "
				"UNIQUE (ID, \"%s\"))",
				transient ? "" /*"TEMPORARY "*/ : "",
				service_name,
				field_name,
				field_name,
				sql_type,
				field_name);
		}
	} else if (sql_type_for_single_value) {
		*sql_type_for_single_value = sql_type;
	}

}

static void
create_decomposed_metadata_tables (TrackerDBInterface *iface,
				   TrackerClass       *service,
				   gint               *max_id)
{
	const char *service_name;
	GString    *sql;
	TrackerProperty	  **properties, **property;
	GSList      *class_properties, *field_it;
	gboolean    main_class;

	service_name = tracker_class_get_name (service);
	main_class = (strcmp (service_name, "rdfs:Resource") == 0);

	if (g_str_has_prefix (service_name, "xsd:")) {
		/* xsd classes do not derive from rdfs:Resource and do not need separate tables */
		return;
	}

	sql = g_string_new ("");
	g_string_append_printf (sql, "CREATE TABLE \"%s\" (ID INTEGER NOT NULL PRIMARY KEY", service_name);
	if (main_class) {
		g_string_append (sql, ", Uri TEXT NOT NULL, Available INTEGER NOT NULL");
	}

	properties = tracker_ontology_get_properties ();
	class_properties = NULL;
	for (property = properties; *property; property++) {
		if (tracker_property_get_domain (*property) == service) {
			const gchar *sql_type_for_single_value = NULL;

			create_decomposed_metadata_property_table (iface, property, 
								   service_name, 
								   &sql_type_for_single_value);

			if (sql_type_for_single_value) {
				/* single value */

				class_properties = g_slist_prepend (class_properties, *property);

				g_string_append_printf (sql, ", \"%s\" %s", 
							tracker_property_get_name (*property), 
							sql_type_for_single_value);
			}
		}
	}

	if (main_class) {
		g_string_append (sql, ", UNIQUE (Uri)");
	}
	g_string_append (sql, ")");
	tracker_db_interface_execute_query (iface, NULL, "%s", sql->str);

	g_free (properties);
	g_string_free (sql, TRUE);

	/* create index for single-valued fields */
	for (field_it = class_properties; field_it != NULL; field_it = field_it->next) {
		TrackerProperty *field;
		const char   *field_name;

		field = field_it->data;

		if (!tracker_property_get_multiple_values (field)
		    && tracker_property_get_indexed (field)) {
			field_name = tracker_property_get_name (field);
			tracker_db_interface_execute_query (iface, NULL,
							    "CREATE INDEX \"%s_%s\" ON \"%s\" (\"%s\")",
							    service_name,
							    field_name,
							    service_name,
							    field_name);
		}
	}

	g_slist_free (class_properties);

	/* insert class uri in rdfs:Resource table */
	if (tracker_class_get_uri (service) != NULL) {
		TrackerDBStatement *stmt;

		stmt = tracker_db_interface_create_statement (iface,
							      "INSERT OR IGNORE INTO \"rdfs:Resource\" (ID, Uri, \"tracker:modified\") VALUES (?, ?, ?)");
		tracker_db_statement_bind_int (stmt, 0, ++(*max_id));
		tracker_db_statement_bind_text (stmt, 1, tracker_class_get_uri (service));
		tracker_db_statement_bind_int64 (stmt, 2, (gint64) time (NULL));
		tracker_db_statement_execute (stmt, NULL);
		g_object_unref (stmt);
	}
}

static void
create_decomposed_transient_metadata_tables (TrackerDBInterface *iface)
{
	TrackerProperty **properties;
	TrackerProperty **property;

	properties = tracker_ontology_get_properties ();

	for (property = properties; *property; property++) {
		if (tracker_property_get_transient (*property)) {

			TrackerClass *domain;
			const gchar *service_name;
			const char *field_name;

			field_name = tracker_property_get_name (*property);

			domain = tracker_property_get_domain (*property);
			service_name = tracker_class_get_name (domain);

			/* TODO: When we refactor to having writes in trackerd, we can use 
	 		 * TEMPORARY tables instead of deleting and storing physically 

			 * create_decomposed_metadata_property_table (iface, property,
			 * 					   service_name,
			 *					   NULL);
			 */

			tracker_db_interface_execute_query (iface, NULL,
				"DELETE FROM \"%s_%s\"",
				service_name,
				field_name);
		}
	}

	g_free (properties);
}

static void
create_fts_table (TrackerDBInterface *iface)
{
	GString    *sql;
	TrackerProperty	  **properties, **property;
	gboolean first;

	sql = g_string_new ("CREATE VIRTUAL TABLE fulltext.fts USING trackerfts (");

	first = TRUE;
	properties = tracker_ontology_get_properties ();
	for (property = properties; *property; property++) {
		if (tracker_property_get_data_type (*property) == TRACKER_PROPERTY_TYPE_STRING &&
		    tracker_property_get_fulltext_indexed (*property)) {
			if (first) {
				first = FALSE;
			} else {
				g_string_append (sql, ", ");
			}
			g_string_append_printf (sql, "\"%s\"", tracker_property_get_name (*property));
		}
	}
	g_free (properties);

	g_string_append (sql, ")");
	tracker_db_interface_execute_query (iface, NULL, "%s", sql->str);

	g_string_free (sql, TRUE);
}

gboolean
tracker_data_manager_init (TrackerConfig              *config,
			   TrackerLanguage            *language,
			   TrackerDBManagerFlags       flags,
			   const gchar                *test_schema,
			   gboolean                   *first_time)
{
	TrackerDBPrivate *private;
	TrackerDBInterface *iface;
	gboolean is_first_time_index;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), FALSE);
	g_return_val_if_fail (TRACKER_IS_LANGUAGE (language), FALSE);

	private = g_static_private_get (&private_key);
	if (private) {
		g_warning ("Already initialized (%s)",
			   __FUNCTION__);
		return FALSE;
	}

	private = g_new0 (TrackerDBPrivate, 1);

	private->config = g_object_ref (config);
	private->language = g_object_ref (language);

	g_static_private_set (&private_key,
			      private,
			      private_free);

	tracker_db_manager_init (flags, &is_first_time_index, FALSE);

	if (first_time != NULL) {
		*first_time = is_first_time_index;
	}

	iface = tracker_db_manager_get_db_interface ();

	if (is_first_time_index) {
		TrackerClass **classes;
		TrackerClass **cl;
		gint           max_id = 0;
		GList *sorted = NULL, *l;
		gchar *test_schema_path;


		if (flags & TRACKER_DB_MANAGER_TEST_MODE) {
			ontologies_dir = g_build_filename ("..", "..",
							 "data",
							 "ontologies",
							 NULL);
		} else {
			ontologies_dir = g_build_filename (SHAREDIR,
							 "tracker",
							 "ontologies",
							 NULL);
		}

		if (test_schema) {
			/* load test schema, not used in normal operation */
			test_schema_path = g_strconcat (test_schema, ".ontology", NULL);

			sorted = g_list_prepend (sorted, g_strdup ("12-nrl.ontology"));
			sorted = g_list_prepend (sorted, g_strdup ("11-rdf.ontology"));
			sorted = g_list_prepend (sorted, g_strdup ("10-xsd.ontology"));
		} else {
			GDir        *ontologies;
			const gchar *conf_file;

			ontologies = g_dir_open (ontologies_dir, 0, NULL);

			conf_file = g_dir_read_name (ontologies);

			/* .ontology files */
			while (conf_file) {
				if (g_str_has_suffix (conf_file, ".ontology")) {
					sorted = g_list_insert_sorted (sorted,
					                                   g_strdup (conf_file), 
					                                   (GCompareFunc) strcmp);
				}
				conf_file = g_dir_read_name (ontologies);
			}

			g_dir_close (ontologies);
		}

		/* load ontology from files into memory */
		for (l = sorted; l; l = l->next) {
			g_debug ("Loading ontology %s", (char *) l->data);
			load_ontology_file (l->data);
		}
		if (test_schema) {
			load_ontology_file_from_path (test_schema_path);
		}

		classes = tracker_ontology_get_classes ();

		tracker_data_begin_transaction ();

		/* create tables */
		for (cl = classes; *cl; cl++) {
			create_decomposed_metadata_tables (iface, *cl, &max_id);
		}

		create_fts_table (iface);

		/* store ontology in database */
		for (l = sorted; l; l = l->next) {
			import_ontology_file (l->data);
		}
		if (test_schema) {
			import_ontology_file_from_path (test_schema_path);
			g_free (test_schema_path);
		}

		tracker_data_commit_transaction ();

		g_free (classes);

		g_list_foreach (sorted, (GFunc) g_free, NULL);
		g_list_free (sorted);

		g_free (ontologies_dir);
		ontologies_dir = NULL;
	} else {
		/* load ontology from database into memory */
		db_get_static_data (iface);
		create_decomposed_transient_metadata_tables (iface);
	}

	return TRUE;
}

void
tracker_data_manager_shutdown (void)
{
	TrackerDBPrivate *private;

	tracker_db_manager_shutdown ();

	private = g_static_private_get (&private_key);
	if (!private) {
		g_warning ("Not initialized (%s)",
			   __FUNCTION__);
		return;
	}

	g_static_private_free (&private_key);
}

TrackerConfig *
tracker_data_manager_get_config (void)
{
	TrackerDBPrivate   *private;

	private = g_static_private_get (&private_key);
	g_return_val_if_fail (private != NULL, NULL);

	return private->config;
}

TrackerLanguage *
tracker_data_manager_get_language (void)
{
	TrackerDBPrivate   *private;

	private = g_static_private_get (&private_key);
	g_return_val_if_fail (private != NULL, NULL);

	return private->language;
}

gint
tracker_data_manager_get_db_option_int (const gchar *option)
{
	TrackerDBInterface *iface;
	TrackerDBStatement *stmt;
	TrackerDBResultSet *result_set;
	gchar		   *str;
	gint		    value = 0;

	g_return_val_if_fail (option != NULL, 0);

	iface = tracker_db_manager_get_db_interface ();

	stmt = tracker_db_interface_create_statement (iface, "SELECT OptionValue FROM Options WHERE OptionKey = ?");
	tracker_db_statement_bind_text (stmt, 0, option);
	result_set = tracker_db_statement_execute (stmt, NULL);
	g_object_unref (stmt);

	if (result_set) {
		tracker_db_result_set_get (result_set, 0, &str, -1);

		if (str) {
			value = atoi (str);
			g_free (str);
		}

		g_object_unref (result_set);
	}

	return value;
}

void
tracker_data_manager_set_db_option_int (const gchar *option,
					gint	     value)
{
	TrackerDBInterface *iface;
	TrackerDBStatement *stmt;
	gchar		   *str;

	g_return_if_fail (option != NULL);

	iface = tracker_db_manager_get_db_interface ();

	stmt = tracker_db_interface_create_statement (iface, "REPLACE INTO Options (OptionKey, OptionValue) VALUES (?,?)");
	tracker_db_statement_bind_text (stmt, 0, option);

	str = tracker_gint_to_string (value);
	tracker_db_statement_bind_text (stmt, 1, str);
	g_free (str);

	tracker_db_statement_execute (stmt, NULL);
	g_object_unref (stmt);
}
