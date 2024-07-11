/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <tinysparql.h>

#include "tracker-ontology-model.h"

struct _TrackerOntologyModel
{
	TrackerSparqlConnection *ontology_conn;
	TrackerSparqlConnection *desc_conn;

	TrackerSparqlStatement *classes_stmt;
	TrackerSparqlStatement *props_stmt;
	GHashTable *stmts;

	GHashTable *classes;
	GHashTable *properties;
	GHashTable *descriptions;
};

TrackerOntologyClass *
ttl_model_class_new (const gchar *classname)
{
	TrackerOntologyClass *def = NULL;

	def = g_new0 (TrackerOntologyClass, 1);
	def->classname = g_strdup (classname);

	return def;
}

void
ttl_model_class_free (TrackerOntologyClass *def)
{
	g_free (def->classname);
	g_free (def->shortname);
	g_free (def->basename);

	g_list_free_full (def->superclasses, (GDestroyNotify) g_free);
	g_list_free_full (def->subclasses, (GDestroyNotify) g_free);
	g_list_free_full (def->in_domain_of, (GDestroyNotify) g_free);
	g_list_free_full (def->in_range_of, (GDestroyNotify) g_free);

	g_free (def->description);
	g_free (def->specification);

	g_list_free_full (def->instances, (GDestroyNotify) g_free);

	g_free (def);
}

TrackerOntologyProperty *
ttl_model_property_new (const gchar *propname)
{
	TrackerOntologyProperty *prop;

	prop = g_new0 (TrackerOntologyProperty, 1);
	prop->propertyname = g_strdup (propname);

	return prop;
}

void
ttl_model_property_free (TrackerOntologyProperty *def)
{
	g_free (def->propertyname);
	g_free (def->shortname);
	g_free (def->basename);

	g_list_free_full (def->domain, (GDestroyNotify) g_free);
	g_list_free_full (def->range, (GDestroyNotify) g_free);
	g_list_free_full (def->superproperties, (GDestroyNotify) g_free);
	g_list_free_full (def->subproperties, (GDestroyNotify) g_free);

	g_free (def->max_cardinality);
	g_free (def->description);
	g_free (def->weight);
	g_free (def->specification);
	g_free (def);
}

TrackerOntologyDescription *
ttl_model_description_new (void)
{
	TrackerOntologyDescription *desc;

	desc = g_new0 (TrackerOntologyDescription, 1);

	return desc;
}

void
ttl_model_description_free (TrackerOntologyDescription *desc)
{
	g_free (desc->title);
	g_free (desc->description);

	g_list_free_full (desc->authors, (GDestroyNotify) g_free);
	g_list_free_full (desc->editors, (GDestroyNotify) g_free);
	g_list_free_full (desc->contributors, (GDestroyNotify) g_free);

	g_free (desc->gitlog);
	g_free (desc->upstream);
	g_free (desc->copyright);

	g_free (desc->baseUrl);
	g_free (desc->relativePath);
	g_free (desc->localPrefix);

	g_free (desc);
}

static void
fill_in_list (TrackerSparqlConnection  *conn,
              TrackerOntologyModel     *model,
              GList                   **list,
              const gchar              *value,
              const gchar              *query)
{
	TrackerSparqlStatement *stmt;
	TrackerSparqlCursor *cursor;
	GError *error = NULL;

	stmt = g_hash_table_lookup (model->stmts, query);
	if (!stmt) {
		stmt = tracker_sparql_connection_query_statement (conn, query, NULL, &error);
		g_assert_no_error (error);
		g_hash_table_insert (model->stmts, g_strdup (query), stmt);
	}

	tracker_sparql_statement_bind_string (stmt, "var", value);
	cursor = tracker_sparql_statement_execute (stmt, NULL, &error);
	g_assert_no_error (error);

	while (tracker_sparql_cursor_next (cursor, NULL, &error)) {
		const gchar *editor = tracker_sparql_cursor_get_string (cursor, 0, NULL);
		*list = g_list_prepend (*list, g_strdup (editor));
	}

	g_assert_no_error (error);
	g_object_unref (cursor);
}

static void
tracker_ontology_model_init_classes (TrackerOntologyModel *model)
{
	TrackerOntologyClass *klass;
	TrackerSparqlCursor *cursor;
	GError *error = NULL;

	cursor = tracker_sparql_connection_query (model->ontology_conn,
	                                          "SELECT "
	                                          "  ?c"
	                                          "  nrl:classSpecification(?c)"
	                                          "  rdfs:comment(?c)"
	                                          "  nrl:notify(?c)"
	                                          "  nrl:deprecated(?c)"
	                                          "  (SUBSTR(str(?c), strlen(?o) + 1) AS ?basename)"
	                                          "  (CONCAT(?prefix, ':', SUBSTR(str(?c), strlen(?o) + 1)) AS ?shortname)"
	                                          "{"
	                                          "  ?c a rdfs:Class ."
	                                          "  ?o a nrl:Namespace ;"
	                                          "     nrl:prefix ?prefix ."
	                                          "  FILTER (STRSTARTS(?c, ?o))"
	                                          "}",
	                                          NULL,
	                                          &error);
	g_assert_no_error (error);

	while (tracker_sparql_cursor_next (cursor, NULL, &error)) {
		const gchar *class_name;

		class_name = tracker_sparql_cursor_get_string (cursor, 0, NULL);

		klass = ttl_model_class_new (class_name);
		klass->specification = g_strdup (tracker_sparql_cursor_get_string (cursor, 1, NULL));
		klass->description = g_strdup (tracker_sparql_cursor_get_string (cursor, 2, NULL));
		klass->notify = tracker_sparql_cursor_get_boolean (cursor, 3);
		klass->deprecated = tracker_sparql_cursor_get_boolean (cursor, 4);
		klass->basename = g_strdup (tracker_sparql_cursor_get_string (cursor, 5, NULL));
		klass->shortname = g_strdup (tracker_sparql_cursor_get_string (cursor, 6, NULL));

		fill_in_list (model->ontology_conn, model, &klass->superclasses, class_name,
			      "SELECT ?super {"
			      "  ~var rdfs:subClassOf ?super"
			      "} ORDER BY DESC ?super");
		fill_in_list (model->ontology_conn, model, &klass->subclasses, class_name,
			      "SELECT ?sub {"
			      "  ?sub rdfs:subClassOf ~var"
			      "} ORDER BY DESC ?sub");
		fill_in_list (model->ontology_conn, model, &klass->in_domain_of, class_name,
			      "SELECT ?prop {"
			      "  ?prop rdfs:domain ~var"
			      "} ORDER BY DESC ?prop");
		fill_in_list (model->ontology_conn, model, &klass->in_range_of, class_name,
			      "SELECT ?prop {"
			      "  ?prop rdfs:range ~var"
			      "} ORDER BY DESC ?prop");
		fill_in_list (model->ontology_conn, model, &klass->instances, class_name,
		              "SELECT (CONCAT(?prefix, ':', SUBSTR(str(?c), strlen(?o) + 1)) AS ?shortname) {"
			      "  ?c a ~var ."
		              "  ?o a nrl:Namespace ;"
		              "     nrl:prefix ?prefix ."
		              "  FILTER (STRSTARTS(?c, ?o))"
			      "} ORDER BY DESC ?shortname");

		g_hash_table_insert (model->classes, klass->classname, klass);
	}

	g_assert_no_error (error);
	g_object_unref (cursor);
}

TrackerOntologyProperty *
tracker_ontology_model_init_properties (TrackerOntologyModel *model)
{
	TrackerOntologyProperty *prop = NULL;
	TrackerSparqlCursor *cursor;
	GError *error = NULL;

	cursor = tracker_sparql_connection_query (model->ontology_conn,
	                                          "SELECT "
	                                          "  ?p"
	                                          "  nrl:propertySpecification(?p)"
	                                          "  rdfs:comment(?p)"
	                                          "  nrl:maxCardinality(?p)"
	                                          "  nrl:deprecated(?p)"
	                                          "  nrl:fulltextIndexed(?p)"
	                                          "  nrl:weight(?p)"
	                                          "  (SUBSTR(str(?p), strlen(?o) + 1) AS ?basename)"
	                                          "  (CONCAT(?prefix, ':', SUBSTR(str(?p), strlen(?o) + 1)) AS ?shortname)"
	                                          "{"
	                                          "  ?p a rdf:Property ."
	                                          "  ?o a nrl:Namespace ;"
	                                          "     nrl:prefix ?prefix ."
	                                          "  FILTER (STRSTARTS(?p, ?o))"
	                                          "}",
	                                          NULL,
	                                          &error);
	g_assert_no_error (error);

	while (tracker_sparql_cursor_next (cursor, NULL, &error)) {
		const gchar *prop_name;

		prop_name = tracker_sparql_cursor_get_string (cursor, 0, NULL);

		prop = ttl_model_property_new (prop_name);
		prop->specification = g_strdup (tracker_sparql_cursor_get_string (cursor, 1, NULL));
		prop->description = g_strdup (tracker_sparql_cursor_get_string (cursor, 2, NULL));
		prop->max_cardinality = g_strdup (tracker_sparql_cursor_get_string (cursor, 3, NULL));
		prop->deprecated = tracker_sparql_cursor_get_boolean (cursor, 4);
		prop->fulltextIndexed = tracker_sparql_cursor_get_boolean (cursor, 5);
		prop->weight = g_strdup (tracker_sparql_cursor_get_string (cursor, 6, NULL));
		prop->basename = g_strdup (tracker_sparql_cursor_get_string (cursor, 7, NULL));
		prop->shortname = g_strdup (tracker_sparql_cursor_get_string (cursor, 8, NULL));

		fill_in_list (model->ontology_conn, model, &prop->domain, prop_name,
			      "SELECT ?class {"
			      "  ~var rdfs:domain ?class"
			      "} ORDER BY DESC ?class");
		fill_in_list (model->ontology_conn, model, &prop->range, prop_name,
			      "SELECT ?class {"
			      "  ~var rdfs:range ?class"
			      "} ORDER BY DESC ?class");
		fill_in_list (model->ontology_conn, model, &prop->superproperties, prop_name,
			      "SELECT ?prop {"
			      "  ~var rdfs:subPropertyOf ?prop"
			      "} ORDER BY DESC ?prop");
		fill_in_list (model->ontology_conn, model, &prop->subproperties, prop_name,
			      "SELECT ?prop {"
			      "  ?prop rdfs:subPropertyOf ~var"
			      "} ORDER BY DESC ?prop");

		g_hash_table_insert (model->properties, prop->propertyname, prop);
	}

	g_assert_no_error (error);
	g_object_unref (cursor);

	return prop;
}

TrackerOntologyModel *
tracker_ontology_model_new (GFile   *ontology_location,
                            GFile   *description_location,
                            GError **error)
{
	TrackerOntologyModel *model;
	TrackerSparqlConnection *ontology_conn, *desc_conn = NULL;
	GFileEnumerator *enumerator = NULL;
	GFileInfo *info;
	GFile *dsc_ontology;

	ontology_conn = tracker_sparql_connection_new (TRACKER_SPARQL_CONNECTION_FLAGS_NONE,
						       NULL, ontology_location, NULL,
						       error);
	if (!ontology_conn)
		goto error;

	dsc_ontology = g_file_new_for_uri ("resource:///org/freedesktop/tracker/doctool/ontology");
	desc_conn = tracker_sparql_connection_new (TRACKER_SPARQL_CONNECTION_FLAGS_NONE,
						   NULL,
						   dsc_ontology,
						   NULL,
						   error);
	g_clear_object (&dsc_ontology);

	if (!desc_conn)
		goto error;

	/* Load all .description files into desc_conn */
	enumerator = g_file_enumerate_children (description_location,
						G_FILE_ATTRIBUTE_STANDARD_NAME,
						G_FILE_QUERY_INFO_NONE,
						NULL, error);
	if (!enumerator)
		goto error;

	while (g_file_enumerator_iterate (enumerator, &info, NULL, NULL, error)) {
		GError *inner_error = NULL;
		gchar *uri, *query;
		GFile *child;

		if (!info)
			break;
		if (!g_str_has_suffix (g_file_info_get_name (info), ".description"))
			continue;

		child = g_file_enumerator_get_child (enumerator, info);
		uri = g_file_get_uri (child);
		query = g_strdup_printf ("LOAD <%s>", uri);
		tracker_sparql_connection_update (desc_conn,
						  query,
						  NULL,
						  &inner_error);
		g_assert_no_error (inner_error);
		g_object_unref (child);
		g_free (uri);
		g_free (query);
	}

	model = g_new0 (TrackerOntologyModel, 1);
	model->ontology_conn = ontology_conn;
	model->desc_conn = desc_conn;
	model->stmts = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
						(GDestroyNotify) g_object_unref);
	model->classes = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
						(GDestroyNotify) ttl_model_class_free);
	model->properties = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
						   (GDestroyNotify) ttl_model_property_free);
	model->descriptions = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
						     (GDestroyNotify) ttl_model_description_free);

	tracker_ontology_model_init_classes (model);
	tracker_ontology_model_init_properties (model);

	return model;

error:
	g_clear_object (&ontology_conn);
	g_clear_object (&desc_conn);
	g_clear_object (&enumerator);

	return NULL;
}

void
tracker_ontology_model_free (TrackerOntologyModel *model)
{
	g_object_unref (model->ontology_conn);
	g_object_unref (model->desc_conn);
	g_clear_object (&model->classes_stmt);
	g_clear_object (&model->props_stmt);
	g_hash_table_unref (model->stmts);
	g_hash_table_unref (model->classes);
	g_hash_table_unref (model->properties);
	g_hash_table_unref (model->descriptions);
	g_free (model);
}

GStrv
tracker_ontology_model_get_prefixes (TrackerOntologyModel *model)
{
	TrackerSparqlCursor *cursor;
	GError *error = NULL;
	GPtrArray *prefixes;

	cursor = tracker_sparql_connection_query (model->ontology_conn,
	                                          "SELECT ?p { ?u a nrl:Namespace ; nrl:prefix ?p }",
	                                          NULL,
	                                          &error);
	g_assert_no_error (error);

	prefixes = g_ptr_array_new ();

	while (tracker_sparql_cursor_next (cursor, NULL, &error)) {
		const gchar *prefix;

		prefix = tracker_sparql_cursor_get_string (cursor, 0, NULL);
		g_ptr_array_add (prefixes, g_strdup (prefix));
	}

	g_object_unref (cursor);
	g_ptr_array_add (prefixes, NULL);
	g_assert_no_error (error);

	return (GStrv) g_ptr_array_free (prefixes, FALSE);
}

TrackerOntologyDescription *
tracker_ontology_model_get_description (TrackerOntologyModel *model,
                                        const gchar          *prefix)
{
	TrackerSparqlStatement *stmt;
	TrackerSparqlCursor *cursor;
	TrackerOntologyDescription *desc;
	GError *error = NULL;

	desc = g_hash_table_lookup (model->descriptions, prefix);

	if (!desc) {
		stmt = tracker_sparql_connection_query_statement (model->desc_conn,
		                                                  "SELECT "
		                                                  "  dsc:title(?d) "
		                                                  "  dsc:description(?d) "
		                                                  "  dsc:gitlog(?d) "
		                                                  "  dsc:localPrefix(?d) "
		                                                  "  dsc:baseUrl(?d) "
		                                                  "  dsc:relativePath(?d) "
		                                                  "  dsc:copyright(?d) "
		                                                  "  dsc:upstream(?d) "
		                                                  "{"
		                                                  "  ?d a dsc:Ontology ;"
		                                                  "     dsc:localPrefix ~prefix"
		                                                  "}",
		                                                  NULL,
		                                                  &error);
		g_assert_no_error (error);

		tracker_sparql_statement_bind_string (stmt, "prefix", prefix);
		cursor = tracker_sparql_statement_execute (stmt, NULL, &error);
		g_object_unref (stmt);
		g_assert_no_error (error);

		if (!tracker_sparql_cursor_next (cursor, NULL, &error))
			return NULL;

		desc = ttl_model_description_new ();
		desc->title = g_strdup (tracker_sparql_cursor_get_string (cursor, 0, NULL));
		desc->description = g_strdup (tracker_sparql_cursor_get_string (cursor, 1, NULL));
		desc->gitlog = g_strdup (tracker_sparql_cursor_get_string (cursor, 2, NULL));
		desc->localPrefix = g_strdup (tracker_sparql_cursor_get_string (cursor, 3, NULL));
		desc->baseUrl = g_strdup (tracker_sparql_cursor_get_string (cursor, 4, NULL));
		desc->relativePath = g_strdup (tracker_sparql_cursor_get_string (cursor, 5, NULL));
		desc->copyright = g_strdup (tracker_sparql_cursor_get_string (cursor, 6, NULL));
		desc->upstream = g_strdup (tracker_sparql_cursor_get_string (cursor, 7, NULL));

		g_object_unref (cursor);

		fill_in_list (model->desc_conn, model, &desc->authors, prefix,
			      "SELECT ?author {"
			      "  ?d a dsc:Ontology ;"
			      "     dsc:localPrefix ~var ;"
			      "     dsc:author ?author ."
			      "}"
			      "ORDER BY DESC ?author");

		fill_in_list (model->desc_conn, model, &desc->editors, prefix,
			      "SELECT ?editor {"
			      "  ?d a dsc:Ontology ;"
			      "     dsc:localPrefix ~var ;"
			      "     dsc:editor ?editor ."
			      "}"
			      "ORDER BY DESC ?editor");

		fill_in_list (model->desc_conn, model, &desc->contributors, prefix,
			      "SELECT ?contributor {"
			      "  ?d a dsc:Ontology ;"
			      "     dsc:localPrefix ~var ;"
			      "     dsc:contributor ?contributor ."
			      "}"
			      "ORDER BY DESC ?contributor");

		g_hash_table_insert (model->descriptions, desc->localPrefix, desc);
	}

	return desc;
}

GList *
tracker_ontology_model_list_classes (TrackerOntologyModel *model,
				     const gchar          *prefix)
{
	TrackerSparqlCursor *cursor;
	GError *error = NULL;
	GList *classes = NULL;

	if (!model->classes_stmt) {
		model->classes_stmt =
			tracker_sparql_connection_query_statement (model->ontology_conn,
			                                           "SELECT ?u {"
			                                           "  ?u a rdfs:Class ."
			                                           "  ?o a nrl:Namespace ;"
			                                           "     nrl:prefix ~prefix ."
			                                           "  FILTER (STRSTARTS(?u, ?o))"
			                                           "} ORDER BY DESC ?u",
			                                           NULL,
			                                           &error);
		g_assert_no_error (error);
	}

	tracker_sparql_statement_bind_string (model->classes_stmt, "prefix", prefix);
	cursor = tracker_sparql_statement_execute (model->classes_stmt, NULL, &error);
	g_assert_no_error (error);

	while (tracker_sparql_cursor_next (cursor, NULL, &error)) {
		classes = g_list_prepend (classes, g_strdup (tracker_sparql_cursor_get_string (cursor, 0, NULL)));
	}

	g_assert_no_error (error);
	g_object_unref (cursor);

	return classes;
}

GList *
tracker_ontology_model_list_properties (TrackerOntologyModel *model,
                                        const gchar          *prefix)
{
	TrackerSparqlCursor *cursor;
	GError *error = NULL;
	GList *props = NULL;

	if (!model->props_stmt) {
		model->props_stmt =
			tracker_sparql_connection_query_statement (model->ontology_conn,
			                                           "SELECT ?u {"
			                                           "  ?u a rdf:Property ."
			                                           "  ?o a nrl:Namespace ;"
			                                           "     nrl:prefix ~prefix ."
			                                           "  FILTER (STRSTARTS(?u, ?o))"
			                                           "} ORDER BY DESC ?u",
			                                           NULL,
			                                           &error);
		g_assert_no_error (error);
	}

	tracker_sparql_statement_bind_string (model->props_stmt, "prefix", prefix);
	cursor = tracker_sparql_statement_execute (model->props_stmt, NULL, &error);
	g_assert_no_error (error);

	while (tracker_sparql_cursor_next (cursor, NULL, &error)) {
		props = g_list_prepend (props, g_strdup (tracker_sparql_cursor_get_string (cursor, 0, NULL)));
	}

	g_assert_no_error (error);
	g_object_unref (cursor);

	return props;
}

TrackerOntologyClass *
tracker_ontology_model_get_class (TrackerOntologyModel *model,
				  const gchar          *class_name)
{
	return g_hash_table_lookup (model->classes, class_name);
}

TrackerOntologyProperty *
tracker_ontology_model_get_property (TrackerOntologyModel *model,
                                     const gchar          *prop_name)
{
	return g_hash_table_lookup (model->properties, prop_name);
}
