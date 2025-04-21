/*
 * Copyright (C) 2023, Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#include "config.h"

#include "tracker-ontologies-introspect.h"

#include "tracker-ontologies.h"

#define NRL_INVERSE_FUNCTIONAL_PROPERTY TRACKER_PREFIX_NRL "InverseFunctionalProperty"

TrackerOntologies *
tracker_ontologies_load_from_database (TrackerDataManager  *manager,
                                       GError             **error)
{
	TrackerOntologies *ontologies = NULL;
	TrackerDBInterface *iface;
	TrackerDBStatement *stmt;
	TrackerSparqlCursor *cursor = NULL;
	GError *internal_error = NULL;

	iface = tracker_data_manager_get_writable_db_interface (manager);

	ontologies = tracker_ontologies_new ();

	/* Get nrl:Ontology */
	stmt = tracker_db_interface_create_statement (iface,
	                                              TRACKER_DB_STATEMENT_CACHE_TYPE_NONE,
	                                              &internal_error,
	                                              "SELECT (SELECT Uri FROM Resource WHERE ID = O.ID) "
	                                              "FROM \"nrl:Ontology\" AS O");

	if (stmt) {
		cursor = TRACKER_SPARQL_CURSOR (tracker_db_statement_start_cursor (stmt, &internal_error));
		g_object_unref (stmt);
	}

	if (cursor) {
		while (tracker_sparql_cursor_next (cursor, NULL, &internal_error)) {
			TrackerOntology *ontology;
			const gchar *uri;

			ontology = tracker_ontology_new ();
			tracker_ontology_set_ontologies (ontology, ontologies);

			uri = tracker_sparql_cursor_get_string (cursor, 0, NULL);

			tracker_ontology_set_uri (ontology, uri);
			tracker_ontologies_add_ontology (ontologies, ontology);

			g_object_unref (ontology);
		}

		g_clear_object (&cursor);
	}

	if (internal_error)
		goto error;

	/* Get nrl:Namespace */
	stmt = tracker_db_interface_create_statement (iface,
	                                              TRACKER_DB_STATEMENT_CACHE_TYPE_NONE,
	                                              &internal_error,
	                                              "SELECT (SELECT Uri FROM Resource WHERE ID = N.ID), "
	                                              "\"nrl:prefix\" "
	                                              "FROM \"nrl:Namespace\" AS N");

	if (stmt) {
		cursor = TRACKER_SPARQL_CURSOR (tracker_db_statement_start_cursor (stmt, &internal_error));
		g_object_unref (stmt);
	}

	if (cursor) {
		while (tracker_sparql_cursor_next (cursor, NULL, &internal_error)) {
			TrackerNamespace *namespace;
			const gchar *uri, *prefix;

			namespace = tracker_namespace_new ();

			uri = tracker_sparql_cursor_get_string (cursor, 0, NULL);
			prefix = tracker_sparql_cursor_get_string (cursor, 1, NULL);

			tracker_namespace_set_ontologies (namespace, ontologies);
			tracker_namespace_set_uri (namespace, uri);
			tracker_namespace_set_prefix (namespace, prefix);
			tracker_ontologies_add_namespace (ontologies, namespace);

			g_object_unref (namespace);

		}

		g_clear_object (&cursor);
	}

	if (internal_error)
		goto error;

	/* Get rdfs:Class */
	stmt = tracker_db_interface_create_statement (iface,
	                                              TRACKER_DB_STATEMENT_CACHE_TYPE_NONE,
	                                              &internal_error,
	                                              "SELECT ID, "
	                                              "(SELECT Uri FROM Resource WHERE ID = C.ID), "
	                                              "\"nrl:notify\" "
	                                              "FROM \"rdfs:Class\" AS C ORDER BY ID");

	if (stmt) {
		cursor = TRACKER_SPARQL_CURSOR (tracker_db_statement_start_cursor (stmt, &internal_error));
		g_object_unref (stmt);
	}

	if (cursor) {
		while (tracker_sparql_cursor_next (cursor, NULL, &internal_error)) {
			TrackerClass *class;
			TrackerRowid id;
			const gchar  *uri;
			gboolean notify;

			class = tracker_class_new ();

			id = tracker_sparql_cursor_get_integer (cursor, 0);
			uri = tracker_sparql_cursor_get_string (cursor, 1, NULL);
			notify = tracker_sparql_cursor_get_integer (cursor, 2);

			tracker_class_set_ontologies (class, ontologies);
			tracker_class_set_id (class, id);
			tracker_class_set_uri (class, uri);
			tracker_class_set_notify (class, notify);

			/* We add domain indexes later , we first need to load the properties */

			tracker_ontologies_add_class (ontologies, class);
			tracker_ontologies_add_id_uri_pair (ontologies, id, uri);
			g_object_unref (class);
		}

		g_clear_object (&cursor);
	}

	if (internal_error)
		goto error;

	/* Get rdf:Property */
	stmt = tracker_db_interface_create_statement (iface,
	                                              TRACKER_DB_STATEMENT_CACHE_TYPE_NONE,
	                                              &internal_error,
	                                              "SELECT P.ID, (SELECT Uri FROM Resource WHERE ID = P.ID), "
	                                              "(SELECT Uri FROM Resource WHERE ID = \"rdfs:domain\"), "
	                                              "(SELECT Uri FROM Resource WHERE ID = \"rdfs:range\"), "
	                                              "\"nrl:maxCardinality\", "
	                                              "\"nrl:indexed\", "
	                                              "(SELECT Uri FROM Resource WHERE ID = \"nrl:secondaryIndex\"), "
	                                              "\"nrl:fulltextIndexed\", "
	                                              "(SELECT 1 FROM \"rdfs:Resource_rdf:type\" WHERE ID = P.ID AND "
	                                              "\"rdf:type\" = (SELECT ID FROM Resource WHERE Uri = '" NRL_INVERSE_FUNCTIONAL_PROPERTY "')) "
	                                              "FROM \"rdf:Property\" AS P ORDER BY ID");

	if (stmt) {
		cursor = TRACKER_SPARQL_CURSOR (tracker_db_statement_start_cursor (stmt, &internal_error));
		g_object_unref (stmt);
	}

	if (cursor) {
		while (tracker_sparql_cursor_next (cursor, NULL, &internal_error)) {
			TrackerProperty *property;
			const gchar *uri, *domain_uri, *range_uri, *secondary_index_uri;
			gboolean indexed, fulltext_indexed;
			gboolean is_inverse_functional_property;
			gint64 max_cardinality;
			TrackerRowid id;

			property = tracker_property_new ();

			id = tracker_sparql_cursor_get_integer (cursor, 0);
			uri = tracker_sparql_cursor_get_string (cursor, 1, NULL);
			domain_uri = tracker_sparql_cursor_get_string (cursor, 2, NULL);
			range_uri = tracker_sparql_cursor_get_string (cursor, 3, NULL);
			max_cardinality = tracker_sparql_cursor_get_integer (cursor, 4);
			indexed = tracker_sparql_cursor_get_boolean (cursor, 5);
			secondary_index_uri = tracker_sparql_cursor_get_string (cursor, 6, NULL);
			fulltext_indexed = tracker_sparql_cursor_get_boolean (cursor, 7);
			is_inverse_functional_property = tracker_sparql_cursor_get_boolean (cursor, 8);

			tracker_property_set_ontologies (property, ontologies);
			tracker_property_set_id (property, id);
			tracker_property_set_uri (property, uri);
			tracker_property_set_domain (property, tracker_ontologies_get_class_by_uri (ontologies, domain_uri));
			tracker_property_set_range (property, tracker_ontologies_get_class_by_uri (ontologies, range_uri));
			tracker_property_set_multiple_values (property, max_cardinality != 1);
			tracker_property_set_indexed (property, indexed);

			if (secondary_index_uri) {
				tracker_property_set_secondary_index (property, tracker_ontologies_get_property_by_uri (ontologies, secondary_index_uri));
			}

			tracker_property_set_fulltext_indexed (property, fulltext_indexed);
			tracker_property_set_is_inverse_functional_property (property, is_inverse_functional_property);

			tracker_ontologies_add_property (ontologies, property);
			tracker_ontologies_add_id_uri_pair (ontologies, id, uri);
			g_object_unref (property);
		}

		g_clear_object (&cursor);
	}

	if (internal_error)
		goto error;

	stmt = tracker_db_interface_create_statement (iface,
	                                              TRACKER_DB_STATEMENT_CACHE_TYPE_NONE, &internal_error,
	                                              "SELECT (SELECT Uri FROM Resource WHERE ID = A.ID),"
	                                              " (SELECT Uri FROM Resource WHERE ID = A.\"rdfs:subClassOf\") "
	                                              "FROM \"rdfs:Class_rdfs:subClassOf\" AS A ");

	if (stmt) {
		cursor = TRACKER_SPARQL_CURSOR (tracker_db_statement_start_cursor (stmt, &internal_error));
		g_object_unref (stmt);
	}

	if (cursor) {
		while (tracker_sparql_cursor_next (cursor, NULL, &internal_error)) {
			TrackerClass *class, *super_class;
			const gchar *class_uri, *super_class_uri;

			class_uri = tracker_sparql_cursor_get_string (cursor, 0, NULL);
			class = tracker_ontologies_get_class_by_uri (ontologies, class_uri);
			super_class_uri = tracker_sparql_cursor_get_string (cursor, 1, NULL);
			super_class = tracker_ontologies_get_class_by_uri (ontologies, super_class_uri);
			tracker_class_add_super_class (class, super_class);
		}

		g_clear_object (&cursor);
	}

	if (internal_error)
		goto error;

	stmt = tracker_db_interface_create_statement (iface,
	                                              TRACKER_DB_STATEMENT_CACHE_TYPE_NONE, &internal_error,
	                                              "SELECT (SELECT Uri FROM Resource WHERE ID = A.ID),"
	                                              " (SELECT Uri FROM Resource WHERE ID = A.\"nrl:domainIndex\") "
	                                              "FROM \"rdfs:Class_nrl:domainIndex\" AS A ");
	if (stmt) {
		cursor = TRACKER_SPARQL_CURSOR (tracker_db_statement_start_cursor (stmt, &internal_error));
		g_object_unref (stmt);
	}

	if (cursor) {
		while (tracker_sparql_cursor_next (cursor, NULL, &internal_error)) {
			TrackerProperty *domain_index;
			TrackerClass *class;
			const gchar *class_uri, *domain_index_uri;

			class_uri = tracker_sparql_cursor_get_string (cursor, 0, NULL);
			class = tracker_ontologies_get_class_by_uri (ontologies, class_uri);
			domain_index_uri = tracker_sparql_cursor_get_string (cursor, 1, NULL);
			domain_index = tracker_ontologies_get_property_by_uri (ontologies, domain_index_uri);

			tracker_class_add_domain_index (class, domain_index);
			tracker_property_add_domain_index (domain_index, class);
		}

		g_clear_object (&cursor);
	}

	if (internal_error)
		goto error;

	stmt = tracker_db_interface_create_statement (iface,
	                                              TRACKER_DB_STATEMENT_CACHE_TYPE_NONE, &internal_error,
	                                              "SELECT (SELECT Uri FROM Resource WHERE ID = A.ID),"
	                                              " (SELECT Uri FROM Resource WHERE ID = A.\"rdfs:subPropertyOf\") "
	                                              "FROM \"rdf:Property_rdfs:subPropertyOf\" AS A ");
	if (stmt) {
		cursor = TRACKER_SPARQL_CURSOR (tracker_db_statement_start_cursor (stmt, &internal_error));
		g_object_unref (stmt);
	}

	if (cursor) {
		while (tracker_sparql_cursor_next (cursor, NULL, &internal_error)) {
			TrackerProperty *property, *super_property;
			const gchar *property_uri, *super_property_uri;

			property_uri = tracker_sparql_cursor_get_string (cursor, 0, NULL);
			property = tracker_ontologies_get_property_by_uri (ontologies, property_uri);
			super_property_uri = tracker_sparql_cursor_get_string (cursor, 1, NULL);
			super_property = tracker_ontologies_get_property_by_uri (ontologies, super_property_uri);
			tracker_property_add_super_property (property, super_property);
		}

		g_clear_object (&cursor);
	}

 error:
	if (internal_error) {
		g_clear_object (&ontologies);
		g_propagate_error (error, internal_error);
		return NULL;
	}

	return ontologies;
}
