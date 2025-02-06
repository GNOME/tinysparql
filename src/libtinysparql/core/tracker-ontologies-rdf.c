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

#include "tracker-ontologies-rdf.h"

#include "tracker-deserializer-rdf.h"
#include "tracker-ontologies.h"

#define RDF_PROPERTY                    TRACKER_PREFIX_RDF "Property"
#define RDF_TYPE                        TRACKER_PREFIX_RDF "type"

#define RDFS_CLASS                      TRACKER_PREFIX_RDFS "Class"
#define RDFS_DOMAIN                     TRACKER_PREFIX_RDFS "domain"
#define RDFS_RANGE                      TRACKER_PREFIX_RDFS "range"
#define RDFS_SUB_CLASS_OF               TRACKER_PREFIX_RDFS "subClassOf"
#define RDFS_SUB_PROPERTY_OF            TRACKER_PREFIX_RDFS "subPropertyOf"

#define NRL_ONTOLOGY                    TRACKER_PREFIX_NRL "Ontology"
#define NRL_NAMESPACE                   TRACKER_PREFIX_NRL "Namespace"
#define NRL_INVERSE_FUNCTIONAL_PROPERTY TRACKER_PREFIX_NRL "InverseFunctionalProperty"

#define NRL_PREFIX                      TRACKER_PREFIX_NRL "prefix"
#define NRL_MAX_CARDINALITY             TRACKER_PREFIX_NRL "maxCardinality"
#define NRL_LAST_MODIFIED               TRACKER_PREFIX_NRL "lastModified"
#define NRL_NOTIFY                      TRACKER_PREFIX_NRL "notify"
#define NRL_INDEXED                     TRACKER_PREFIX_NRL "indexed"
#define NRL_DOMAIN_INDEX                TRACKER_PREFIX_NRL "domainIndex"
#define NRL_SECONDARY_INDEX             TRACKER_PREFIX_NRL "secondaryIndex"
#define NRL_FULL_TEXT_INDEXED           TRACKER_PREFIX_NRL "fulltextIndexed"
#define NRL_WEIGHT                      TRACKER_PREFIX_NRL "weight"

static void
print_parsing_error (TrackerDeserializer *deserializer,
                     GFile               *file,
                     const gchar         *format,
                     ...)
{
	goffset line_no = 0, column_no = 0;
	gchar *prefix, *uri, *msg;
	va_list va_args;

	uri = g_file_get_uri (file);

	if (tracker_deserializer_get_parser_location (TRACKER_DESERIALIZER (deserializer),
	                                              &line_no, &column_no))
		prefix = g_strdup_printf ("%s:%d:%d: ", uri, (int) line_no, (int) column_no);
	else
		prefix = g_strdup_printf ("%s: ", uri);

	va_start (va_args, format);
	msg = g_strdup_vprintf (format, va_args);
	va_end (va_args);

	g_printerr ("%s%s\n", prefix, msg);

	g_free (prefix);
	g_free (msg);
	g_free (uri);
}

static TrackerProperty *
get_property (TrackerOntologies   *ontologies,
              const gchar         *property_uri,
              TrackerDeserializer *rdf,
              GFile               *file)
{
	TrackerProperty *property;

	property = tracker_ontologies_get_property_by_uri (ontologies, property_uri);
	if (!property)
		print_parsing_error (rdf, file, "Unknown property %s", property_uri);

	return property;
}

static TrackerClass *
get_class (TrackerOntologies   *ontologies,
           const gchar         *class_uri,
           TrackerDeserializer *rdf,
           GFile               *file)
{
	TrackerClass *class;

	class = tracker_ontologies_get_class_by_uri (ontologies, class_uri);
	if (!class)
		print_parsing_error (rdf, file, "Unknown class %s", class_uri);

	return class;
}

static gboolean
tracker_ontologies_rdf_load_triple (TrackerOntologies    *ontologies,
                                    TrackerDeserializer  *rdf,
                                    GFile                *file)
{
	const gchar *subject, *predicate;
	goffset line, column;
	gboolean had_error = FALSE;

	subject = tracker_sparql_cursor_get_string (TRACKER_SPARQL_CURSOR (rdf),
	                                            TRACKER_RDF_COL_SUBJECT,
	                                            NULL);
	predicate = tracker_sparql_cursor_get_string (TRACKER_SPARQL_CURSOR (rdf),
	                                              TRACKER_RDF_COL_PREDICATE,
	                                              NULL);

	tracker_deserializer_get_parser_location (TRACKER_DESERIALIZER (rdf),
	                                          &line, &column);

	if (g_strcmp0 (predicate, RDF_TYPE) == 0) {
		const gchar *object;

		object = tracker_sparql_cursor_get_string (TRACKER_SPARQL_CURSOR (rdf),
		                                           TRACKER_RDF_COL_OBJECT,
		                                           NULL);

		if (g_strcmp0 (object, RDFS_CLASS) == 0) {
			TrackerClass *class;
			gchar *uri;

			class = tracker_ontologies_get_class_by_uri (ontologies, subject);

			if (class != NULL) {
				print_parsing_error (rdf, file, "Duplicate definition of class %s", subject);
				return TRUE;
			}

			uri = g_file_get_uri (file);

			class = tracker_class_new ();
			tracker_class_set_ontologies (class, ontologies);
			tracker_class_set_uri (class, subject);
			tracker_class_set_ontology_path (class, uri);
			tracker_class_set_definition_line_no (class, line);
			tracker_class_set_definition_column_no (class, column);
			tracker_ontologies_add_class (ontologies, class);
			g_object_unref (class);
			g_free (uri);
		} else if (g_strcmp0 (object, RDF_PROPERTY) == 0) {
			TrackerProperty *property;
			gchar *uri;

			property = tracker_ontologies_get_property_by_uri (ontologies, subject);
			if (property != NULL) {
				print_parsing_error (rdf, file, "Duplicate definition of property %s", subject);
				return TRUE;
			}

			uri = g_file_get_uri (file);

			property = tracker_property_new ();
			tracker_property_set_ontologies (property, ontologies);
			tracker_property_set_uri (property, subject);
			tracker_property_set_multiple_values (property, TRUE);
			tracker_property_set_ontology_path (property, uri);
			tracker_property_set_definition_line_no (property, line);
			tracker_property_set_definition_column_no (property, column);
			tracker_ontologies_add_property (ontologies, property);
			g_object_unref (property);
			g_free (uri);
		} else if (g_strcmp0 (object, NRL_INVERSE_FUNCTIONAL_PROPERTY) == 0) {
			TrackerProperty *property;

			property = get_property (ontologies, subject, rdf, file);
			had_error |= property == NULL;

			if (property)
				tracker_property_set_is_inverse_functional_property (property, TRUE);
		} else if (g_strcmp0 (object, NRL_NAMESPACE) == 0) {
			TrackerNamespace *namespace;

			if (tracker_ontologies_get_namespace_by_uri (ontologies, subject) != NULL) {
				print_parsing_error (rdf, file, "Duplicate definition of namespace %s", subject);
				return TRUE;
			}

			namespace = tracker_namespace_new ();
			tracker_namespace_set_ontologies (namespace, ontologies);
			tracker_namespace_set_uri (namespace, subject);
			tracker_ontologies_add_namespace (ontologies, namespace);
			g_object_unref (namespace);
		} else if (g_strcmp0 (object, NRL_ONTOLOGY) == 0) {
			TrackerOntology *ontology;

			if (tracker_ontologies_get_ontology_by_uri (ontologies, subject) != NULL) {
				print_parsing_error (rdf, file, "Duplicate definition of ontology %s", subject);
				return TRUE;
			}

			ontology = tracker_ontology_new ();
			tracker_ontology_set_ontologies (ontology, ontologies);
			tracker_ontology_set_uri (ontology, subject);
			tracker_ontologies_add_ontology (ontologies, ontology);
			g_object_unref (ontology);

		}
	} else if (g_strcmp0 (predicate, RDFS_SUB_CLASS_OF) == 0) {
		TrackerClass *class, *super_class;
		const gchar *object;

		class = get_class (ontologies, subject, rdf, file);
		had_error |= class == NULL;

		object = tracker_sparql_cursor_get_string (TRACKER_SPARQL_CURSOR (rdf),
		                                           TRACKER_RDF_COL_OBJECT,
		                                           NULL);
		super_class = get_class (ontologies, object, rdf, file);
		had_error |= super_class == NULL;

		if (class && super_class)
			tracker_class_add_super_class (class, super_class);
	} else if (g_strcmp0 (predicate, NRL_NOTIFY) == 0) {
		TrackerClass *class;
		gboolean notify;

		class = get_class (ontologies, subject, rdf, file);
		had_error |= class == NULL;

		notify = tracker_sparql_cursor_get_boolean (TRACKER_SPARQL_CURSOR (rdf),
		                                            TRACKER_RDF_COL_OBJECT);
		if (class)
			tracker_class_set_notify (class, notify);
	} else if (g_strcmp0 (predicate, NRL_DOMAIN_INDEX) == 0) {
		TrackerClass *class;
		TrackerProperty *property;
		TrackerProperty **properties;
		guint n_props, i;
		const gchar *object;

		class = get_class (ontologies, subject, rdf, file);
		had_error |= class == NULL;

		object = tracker_sparql_cursor_get_string (TRACKER_SPARQL_CURSOR (rdf),
		                                           TRACKER_RDF_COL_OBJECT,
		                                           NULL);

		property = get_property (ontologies, object, rdf, file);
		had_error |= property == NULL;

		if (class && property) {
			properties = tracker_ontologies_get_properties (ontologies, &n_props);
			for (i = 0; i < n_props; i++) {
				if (tracker_property_get_domain (properties[i]) == class &&
				    properties[i] == property) {
					print_parsing_error (rdf, file,
					                     "Property %s is already a first-class property of %s while trying to add it as nrl:domainIndex",
					                     object, subject);
					had_error |= TRUE;
				}
			}

			tracker_class_add_domain_index (class, property);
			tracker_property_add_domain_index (property, class);
		}
	} else if (g_strcmp0 (predicate, RDFS_SUB_PROPERTY_OF) == 0) {
		TrackerProperty *property, *super_property;
		const gchar *object;

		property = get_property (ontologies, subject, rdf, file);
		had_error |= property == NULL;

		object = tracker_sparql_cursor_get_string (TRACKER_SPARQL_CURSOR (rdf),
		                                           TRACKER_RDF_COL_OBJECT,
		                                           NULL);
		super_property = get_property (ontologies, object, rdf, file);
		had_error |= super_property == NULL;

		if (property && super_property)
			tracker_property_add_super_property (property, super_property);
	} else if (g_strcmp0 (predicate, RDFS_DOMAIN) == 0) {
		TrackerProperty *property;
		TrackerClass *domain;
		const gchar *object;

		property = get_property (ontologies, subject, rdf, file);
		had_error |= property == NULL;

		object = tracker_sparql_cursor_get_string (TRACKER_SPARQL_CURSOR (rdf),
		                                           TRACKER_RDF_COL_OBJECT,
		                                           NULL);
		domain = get_class (ontologies, object, rdf, file);
		had_error |= domain == NULL;

		if (property && domain)
			tracker_property_set_domain (property, domain);
	} else if (g_strcmp0 (predicate, RDFS_RANGE) == 0) {
		TrackerProperty *property;
		TrackerClass *range;
		const gchar *object;

		property = get_property (ontologies, subject, rdf, file);
		had_error |= property == NULL;

		object = tracker_sparql_cursor_get_string (TRACKER_SPARQL_CURSOR (rdf),
		                                           TRACKER_RDF_COL_OBJECT,
		                                           NULL);
		range = get_class (ontologies, object, rdf, file);
		had_error |= range == NULL;

		if (property && range)
			tracker_property_set_range (property, range);
	} else if (g_strcmp0 (predicate, NRL_MAX_CARDINALITY) == 0) {
		TrackerProperty *property;
		gint64 cardinality;

		property = get_property (ontologies, subject, rdf, file);
		had_error |= property == NULL;

		cardinality = tracker_sparql_cursor_get_integer (TRACKER_SPARQL_CURSOR (rdf),
		                                                 TRACKER_RDF_COL_OBJECT);

		if (cardinality == 0) {
			print_parsing_error (rdf, file, "Property nrl:maxCardinality only accepts integers greater than 0");
			had_error |= TRUE;
		}

		tracker_property_set_multiple_values (property, cardinality != 1);
	} else if (g_strcmp0 (predicate, NRL_INDEXED) == 0) {
		TrackerProperty *property;
		gboolean indexed;

		property = get_property (ontologies, subject, rdf, file);
		had_error |= property == NULL;

		indexed = tracker_sparql_cursor_get_boolean (TRACKER_SPARQL_CURSOR (rdf),
		                                             TRACKER_RDF_COL_OBJECT);

		if (property)
			tracker_property_set_indexed (property, indexed);
	} else if (g_strcmp0 (predicate, NRL_SECONDARY_INDEX) == 0) {
		TrackerProperty *property, *secondary_index;
		const gchar *object;

		object = tracker_sparql_cursor_get_string (TRACKER_SPARQL_CURSOR (rdf),
		                                           TRACKER_RDF_COL_OBJECT,
		                                           NULL);

		property = get_property (ontologies, subject, rdf, file);
		had_error |= property == NULL;

		secondary_index = get_property (ontologies, object, rdf, file);
		had_error |= secondary_index == NULL;

		if (property && secondary_index) {
			if (!tracker_property_get_indexed (property)) {
				print_parsing_error (rdf, file, "nrl:secondaryindex only applies to nrl:indexed properties");
				had_error |= TRUE;
			}

			if (tracker_property_get_multiple_values (property) ||
			    tracker_property_get_multiple_values (secondary_index)) {
				print_parsing_error (rdf, file, "nrl:secondaryindex cannot be applied to properties with nrl:maxCardinality higher than one");
				had_error |= TRUE;
			}

			tracker_property_set_secondary_index (property, secondary_index);
		}
	} else if (g_strcmp0 (predicate, NRL_FULL_TEXT_INDEXED) == 0) {
		TrackerProperty *property;
		gboolean indexed;

		property = get_property (ontologies, subject, rdf, file);
		had_error |= property == NULL;

		indexed = tracker_sparql_cursor_get_boolean (TRACKER_SPARQL_CURSOR (rdf),
		                                             TRACKER_RDF_COL_OBJECT);

		if (property)
			tracker_property_set_fulltext_indexed (property, indexed);
	} else if (g_strcmp0 (predicate, NRL_WEIGHT) == 0) {
		TrackerProperty *property;
		gint64 weight;

		property = get_property (ontologies, subject, rdf, file);
		had_error |= property == NULL;

		weight = tracker_sparql_cursor_get_integer (TRACKER_SPARQL_CURSOR (rdf),
		                                            TRACKER_RDF_COL_OBJECT);

		if (property)
			tracker_property_set_weight (property, weight);
	} else if (g_strcmp0 (predicate, NRL_PREFIX) == 0) {
		TrackerNamespace *namespace;
		const gchar *prefix;

		namespace = tracker_ontologies_get_namespace_by_uri (ontologies, subject);
		if (namespace == NULL) {
			print_parsing_error (rdf, file, "Unknown namespace %s", subject);
			return TRUE;
		}

		prefix = tracker_sparql_cursor_get_string (TRACKER_SPARQL_CURSOR (rdf),
		                                           TRACKER_RDF_COL_OBJECT,
		                                           NULL);
		tracker_namespace_set_prefix (namespace, prefix);
	}

	return !had_error;
}

static gboolean
load_ontology_rdf (TrackerOntologies    *ontologies,
                   TrackerDeserializer  *rdf,
                   GFile                *file,
                   GError              **error)
{
	GError *inner_error = NULL;
	gboolean had_errors = FALSE;

	while (tracker_sparql_cursor_next (TRACKER_SPARQL_CURSOR (rdf), NULL, &inner_error))
		had_errors |= !tracker_ontologies_rdf_load_triple (ontologies, rdf, file);

	if (inner_error) {
		g_propagate_error (error, inner_error);
		return FALSE;
	} else if (had_errors) {
		g_set_error (error,
		             TRACKER_SPARQL_ERROR,
		             TRACKER_SPARQL_ERROR_PARSE,
		             "Syntax errors found while parsing ontology");
		return FALSE;
	}

	return TRUE;
}

static gboolean
has_superclass (TrackerClass *class,
                TrackerClass *superclass)
{
	TrackerClass **super;

	if (class == superclass)
		return TRUE;

	super = tracker_class_get_super_classes (class);
	while (super && *super) {
		if (superclass == *super || has_superclass (*super, superclass))
			return TRUE;
		super++;
	}

	return FALSE;
}

static gboolean
check_property_inheritance (TrackerProperty *property,
                            const gchar     *error_prefix)
{
	TrackerProperty **super;
	TrackerClass *domain;
	gboolean had_errors = FALSE;

	super = tracker_property_get_super_properties (property);
	domain = tracker_property_get_domain (property);

	while (super && *super) {
		TrackerClass *super_domain;

		super_domain = tracker_property_get_domain (*super);

		if (super_domain && !has_superclass (domain, super_domain)) {
			g_printerr ("%s: Property %s is not in the domain of a subclass of %s, to be a subproperty of %s\n",
			            error_prefix, tracker_property_get_name (property),
			            tracker_class_get_name (super_domain),
			            tracker_property_get_name (*super));
			had_errors |= TRUE;
		}

		super++;
	}

	return had_errors;
}

static gboolean
check_class_inheritance (TrackerClass *class,
                         TrackerClass *cur,
                         GPtrArray    *visited,
                         const gchar  *error_prefix)
{
	TrackerClass **super;
	gboolean had_errors = FALSE;

	if (!cur)
		cur = class;

	super = tracker_class_get_super_classes (cur);
	while (super && *super) {
		if (!g_ptr_array_find (visited, *super, NULL)) {
			g_ptr_array_add (visited, *super);

			if (class == *super) {
				g_printerr ("%s: Class %s has cycles in rdfs:subClassOf hierarchy\n",
				            error_prefix, tracker_class_get_name (class));
				had_errors |= TRUE;
			} else {
				had_errors |= check_class_inheritance (class, *super, visited, error_prefix);
			}
		}
		super++;
	}

	return had_errors;
}

static gboolean
check_class_domain_indexes (TrackerClass *class,
                            const gchar  *error_prefix)
{
	TrackerProperty **domain_indexes;
	gboolean had_errors = FALSE;

	domain_indexes = tracker_class_get_domain_indexes (class);

	while (domain_indexes && *domain_indexes) {
		TrackerClass *domain;

		domain = tracker_property_get_domain (*domain_indexes);
		if (domain && !has_superclass (class, domain)) {
			g_printerr ("%s: Class %s wants domain index on property %s, but is not a subclass of %s\n",
			            error_prefix, tracker_class_get_name (class),
			            tracker_property_get_name (*domain_indexes),
			            tracker_class_get_name (domain));
			had_errors |= TRUE;
		}

		domain_indexes++;
	}

	return had_errors;
}

static gboolean
check_ontology_completeness (TrackerOntologies  *ontologies,
                             GError            **error)
{
	guint i, n_properties, n_classes;
	TrackerProperty **properties;
	TrackerClass **classes;
	gboolean had_errors = FALSE;

	properties = tracker_ontologies_get_properties (ontologies, &n_properties);

	for (i = 0; i < n_properties; i++) {
		TrackerProperty *property = properties[i];
		const gchar *ontology_path = tracker_property_get_ontology_path (property);
		goffset line_no = tracker_property_get_definition_line_no (property);
		goffset column_no = tracker_property_get_definition_column_no (property);
		gchar *error_prefix = g_strdup_printf ("%s:%" G_GOFFSET_FORMAT ":%" G_GOFFSET_FORMAT,
		                                       ontology_path, line_no, column_no);

		if (!tracker_property_get_domain (property)) {
			g_printerr ("%s: Property %s has no defined domain",
			            error_prefix, tracker_property_get_name (property));
			had_errors |= TRUE;
		}

		if (!tracker_property_get_range (property)) {
			g_printerr ("%s: Property %s has no defined range",
			            error_prefix, tracker_property_get_name (property));
			had_errors |= TRUE;
		}

		had_errors |= check_property_inheritance (property, error_prefix);
		g_free (error_prefix);
	}

	classes = tracker_ontologies_get_classes (ontologies, &n_classes);

	for (i = 0; i < n_classes; i++) {
		TrackerClass *class = classes[i];
		const gchar *ontology_path = tracker_class_get_ontology_path (class);
		goffset line_no = tracker_class_get_definition_line_no (class);
		goffset column_no = tracker_class_get_definition_column_no (class);
		gchar *error_prefix = g_strdup_printf ("%s:%" G_GOFFSET_FORMAT ":%" G_GOFFSET_FORMAT,
		                                       ontology_path, line_no, column_no);
		GPtrArray *visited = g_ptr_array_new ();

		had_errors |= check_class_inheritance (class, NULL, visited, error_prefix);
		had_errors |= check_class_domain_indexes (class, error_prefix);
		g_ptr_array_unref (visited);
		g_free (error_prefix);
	}

	if (had_errors) {
		g_set_error (error,
		             TRACKER_SPARQL_ERROR,
		             TRACKER_SPARQL_ERROR_INCOMPLETE_PROPERTY_DEFINITION,
		             "Semantic errors found during ontology parsing");
		return FALSE;
	}

	return TRUE;
}

TrackerOntologies *
tracker_ontologies_load_from_rdf (GList   *files,
                                  GError **error)
{
	TrackerOntologies *ontologies;
	GError *inner_error = NULL;
	TrackerSparqlCursor *rdf = NULL;
	GList *l;

	ontologies = tracker_ontologies_new ();

	for (l = files; l; l = l->next) {
		rdf = tracker_deserializer_new_for_file (l->data,
		                                         NULL,
		                                         &inner_error);
		if (!rdf)
			break;

		if (!load_ontology_rdf (ontologies,
		                        TRACKER_DESERIALIZER (rdf),
		                        l->data, &inner_error))
			break;

		g_clear_object (&rdf);
	}

	g_clear_object (&rdf);

	if (!inner_error)
		check_ontology_completeness (ontologies, &inner_error);

	if (inner_error) {
		g_propagate_error (error, inner_error);
		g_clear_object (&ontologies);
		return NULL;
	}

	return ontologies;
}
