/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
 * Copyright (C) 2007, Jason Kivlighn <jkivlighn@gmail.com>
 * Copyright (C) 2007, Creative Commons <http://creativecommons.org>
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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
 */

#include "config.h"

#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <zlib.h>
#include <inttypes.h>

#include <glib/gstdio.h>

#include <libtracker-common/tracker-common.h>

#if HAVE_TRACKER_FTS
#include <libtracker-fts/tracker-fts.h>
#endif

#include <libtracker-db/tracker-db-interface-sqlite.h>
#include <libtracker-db/tracker-db-manager.h>
#include <libtracker-db/tracker-db-journal.h>

#include "tracker-class.h"
#include "tracker-data-manager.h"
#include "tracker-data-update.h"
#include "tracker-namespace.h"
#include "tracker-ontologies.h"
#include "tracker-ontology.h"
#include "tracker-property.h"
#include "tracker-sparql-query.h"
#include "tracker-data-query.h"

#define XSD_PREFIX TRACKER_XSD_PREFIX
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
#define NRL_INVERSE_FUNCTIONAL_PROPERTY TRACKER_NRL_PREFIX "InverseFunctionalProperty"
#define NRL_MAX_CARDINALITY NRL_PREFIX "maxCardinality"

#define NAO_PREFIX TRACKER_NAO_PREFIX
#define NAO_LAST_MODIFIED NAO_PREFIX "lastModified"

#define TRACKER_PREFIX TRACKER_TRACKER_PREFIX

#define ZLIBBUFSIZ 8192

static gchar    *ontologies_dir;
static gboolean  initialized;
static gboolean  in_journal_replay;


typedef struct {
	const gchar *from;
	const gchar *to;
} Conversion;

static Conversion allowed_boolean_conversions[] = {
	{ "false", "true" },
	{ "true", "false" },
	{ NULL, NULL }
};

static Conversion allowed_range_conversions[] = {
	{ XSD_PREFIX "integer", XSD_PREFIX "string" },
	{ XSD_PREFIX "integer", XSD_PREFIX "double" },
	{ XSD_PREFIX "integer", XSD_PREFIX "boolean" },

	{ XSD_PREFIX "string", XSD_PREFIX "integer" },
	{ XSD_PREFIX "string", XSD_PREFIX "double" },
	{ XSD_PREFIX "string", XSD_PREFIX "boolean" },

	{ XSD_PREFIX "double", XSD_PREFIX "integer" },
	{ XSD_PREFIX "double", XSD_PREFIX "string" },
	{ XSD_PREFIX "double", XSD_PREFIX "boolean" },

	{ NULL, NULL }
};

static void
set_secondary_index_for_single_value_property (TrackerDBInterface *iface,
                                               const gchar        *service_name,
                                               const gchar        *field_name,
                                               const gchar        *second_field_name,
                                               gboolean            enabled)
{
	g_debug ("Dropping index:  DROP INDEX IF EXISTS \"%s_%s\"",
	         service_name, field_name);

	tracker_db_interface_execute_query (iface, NULL,
	                                    "DROP INDEX IF EXISTS \"%s_%s\"",
	                                    service_name,
	                                    field_name);

	if (enabled) {
		g_debug ("Creating index: CREATE INDEX \"%s_%s\" ON \"%s\" (\"%s\", \"%s\")",
		         service_name, field_name, service_name, field_name, second_field_name);

		tracker_db_interface_execute_query (iface, NULL,
		                                    "CREATE INDEX \"%s_%s\" ON \"%s\" (\"%s\", \"%s\")",
		                                    service_name,
		                                    field_name,
		                                    service_name,
		                                    field_name,
		                                    second_field_name);
	}
}

static void
set_index_for_single_value_property (TrackerDBInterface *iface,
                                     const gchar        *service_name,
                                     const gchar        *field_name,
                                     gboolean            enabled)
{
	g_debug ("Dropping index: DROP INDEX IF EXISTS \"%s_%s\"",
	         service_name, field_name);

	tracker_db_interface_execute_query (iface, NULL,
	                                    "DROP INDEX IF EXISTS \"%s_%s\"",
	                                    service_name,
	                                    field_name);

	if (enabled) {
		g_debug ("Creating index: CREATE INDEX \"%s_%s\" ON \"%s\" (\"%s\")",
		         service_name, field_name, service_name, field_name);

		tracker_db_interface_execute_query (iface, NULL,
		                                    "CREATE INDEX \"%s_%s\" ON \"%s\" (\"%s\")",
		                                    service_name,
		                                    field_name,
		                                    service_name,
		                                    field_name);
	}
}

static void
set_index_for_multi_value_property (TrackerDBInterface *iface,
                                    const gchar        *service_name,
                                    const gchar        *field_name,
                                    gboolean            enabled)
{
	tracker_db_interface_execute_query (iface, NULL,
	                                    "DROP INDEX IF EXISTS \"%s_%s_ID_ID\"",
	                                    service_name,
	                                    field_name);

	if (enabled) {
		tracker_db_interface_execute_query (iface, NULL,
		                                    "CREATE INDEX \"%s_%s_ID\" ON \"%s_%s\" (ID)",
		                                    service_name,
		                                    field_name,
		                                    service_name,
		                                    field_name);
		tracker_db_interface_execute_query (iface, NULL,
		                                    "CREATE UNIQUE INDEX \"%s_%s_ID_ID\" ON \"%s_%s\" (\"%s\", ID)",
		                                    service_name,
		                                    field_name,
		                                    service_name,
		                                    field_name,
		                                    field_name);
	} else {
		tracker_db_interface_execute_query (iface, NULL,
		                                    "DROP INDEX IF EXISTS \"%s_%s_ID\"",
		                                    service_name,
		                                    field_name);
		tracker_db_interface_execute_query (iface, NULL,
		                                    "CREATE UNIQUE INDEX \"%s_%s_ID_ID\" ON \"%s_%s\" (ID, \"%s\")",
		                                    service_name,
		                                    field_name,
		                                    service_name,
		                                    field_name,
		                                    field_name);
	}
}

static gboolean
is_allowed_conversion (const gchar *oldv,
                       const gchar *newv,
                       Conversion   allowed[])
{
	guint i;

	for (i = 0; allowed[i].from != NULL; i++) {
		if (g_strcmp0 (allowed[i].from, oldv) == 0) {
			if (g_strcmp0 (allowed[i].to, newv) == 0) {
				return TRUE;
			}
		}
	}

	return FALSE;
}

static gboolean
update_property_value (const gchar     *kind,
                       const gchar     *subject,
                       const gchar     *predicate,
                       const gchar     *object,
                       Conversion       allowed[],
                       TrackerClass    *class,
                       TrackerProperty *property)
{
	GError *error = NULL;
	gboolean needed = TRUE;
	gboolean is_new = FALSE;

	if (class) {
		is_new = tracker_class_get_is_new (class);
	} else if (property) {
		is_new = tracker_property_get_is_new (property);
	}

	if (!is_new) {
		gchar *query = NULL;
		TrackerDBResultSet *result_set;

		query = g_strdup_printf ("SELECT ?old_value WHERE { "
		                           "<%s> %s ?old_value "
		                         "}", subject, kind);

		result_set = tracker_data_query_sparql (query, &error);

		if (!error && result_set) {
			gchar *str = NULL;

			tracker_db_result_set_get (result_set, 0, &str, -1);

			if (g_strcmp0 (object, str) == 0) {
				needed = FALSE;
			} else {

				if (allowed && !is_allowed_conversion (str, object, allowed)) {
					g_error ("Ontology change conversion not allowed '%s' -> '%s' in '%s' of '%s'",
					         str, object, predicate, subject);
				}

				tracker_data_delete_statement (NULL, subject, predicate, str, &error);
				if (!error)
					tracker_data_update_buffer_flush (&error);
			}

			g_free (str);
		} else {
			if (object && (g_strcmp0 (object, "false") == 0)) {
				needed = FALSE;
			} else {
				needed = (object != NULL);
			}
		}
		g_free (query);
		if (result_set) {
			g_object_unref (result_set);
		}
	} else {
		needed = FALSE;
	}


	if (!error && needed) {
		tracker_data_insert_statement (NULL, subject,
		                               predicate, object,
		                               &error);
		if (!error)
			tracker_data_update_buffer_flush (&error);
	}

	if (error) {
		g_critical ("Ontology change, %s", error->message);
		g_clear_error (&error);
	}

	return needed;
}


static void
check_range_conversion_is_allowed (const gchar *subject,
                                   const gchar *predicate,
                                   const gchar *object)
{
	TrackerDBResultSet *result_set;
	gchar *query;

	query = g_strdup_printf ("SELECT ?old_value WHERE { "
	                           "<%s> rdfs:range ?old_value "
	                         "}", subject);

	result_set = tracker_data_query_sparql (query, NULL);

	g_free (query);

	if (result_set) {
		gchar *str = NULL;
		tracker_db_result_set_get (result_set, 0, &str, -1);

		if (g_strcmp0 (object, str) != 0) {
			if (!is_allowed_conversion (str, object, allowed_range_conversions)) {
				g_error ("Ontology change conversion not allowed '%s' -> '%s' in '%s' of '%s'",
				         str, object, predicate, subject);
			}
		}
		g_free (str);
	}

	if (result_set) {
		g_object_unref (result_set);
	}
}

static void
fix_indexed (TrackerProperty *property)
{
	TrackerDBInterface *iface;
	TrackerClass *class;
	const gchar *service_name;
	const gchar *field_name;

	iface = tracker_db_manager_get_db_interface ();

	class = tracker_property_get_domain (property);
	field_name = tracker_property_get_name (property);
	service_name = tracker_class_get_name (class);

	if (tracker_property_get_multiple_values (property)) {
		set_index_for_multi_value_property (iface, service_name, field_name,
		                                    tracker_property_get_indexed (property));
	} else {
		TrackerProperty *secondary_index;

		secondary_index = tracker_property_get_secondary_index (property);
		if (secondary_index == NULL) {
			set_index_for_single_value_property (iface, service_name, field_name,
			                                     tracker_property_get_indexed (property));
		} else {
			set_secondary_index_for_single_value_property (iface, service_name, field_name,
			                                               tracker_property_get_name (secondary_index),
			                                               tracker_property_get_indexed (property));
		}
	}
}

void
tracker_data_ontology_load_statement (const gchar *ontology_path,
                                      gint         subject_id,
                                      const gchar *subject,
                                      const gchar *predicate,
                                      const gchar *object,
                                      gint        *max_id,
                                      gboolean     in_update,
                                      GHashTable  *classes,
                                      GHashTable  *properties,
                                      GPtrArray   *seen_classes,
                                      GPtrArray   *seen_properties)
{
	if (g_strcmp0 (predicate, RDF_TYPE) == 0) {
		if (g_strcmp0 (object, RDFS_CLASS) == 0) {
			TrackerClass *class;
			class = tracker_ontologies_get_class_by_uri (subject);

			if (class != NULL) {
				if (seen_classes)
					g_ptr_array_add (seen_classes, g_object_ref (class));
				if (!in_update) {
					g_critical ("%s: Duplicate definition of class %s", ontology_path, subject);
				} else {
					/* Reset for a correct post-check */
					tracker_class_reset_domain_indexes (class);
					tracker_class_set_notify (class, FALSE);
				}
				return;
			}

			if (max_id) {
				subject_id = ++(*max_id);
			}

			class = tracker_class_new ();
			tracker_class_set_is_new (class, in_update);
			tracker_class_set_uri (class, subject);
			tracker_class_set_id (class, subject_id);
			tracker_ontologies_add_class (class);
			tracker_ontologies_add_id_uri_pair (subject_id, subject);

			if (seen_classes)
				g_ptr_array_add (seen_classes, g_object_ref (class));

			if (classes) {
				g_hash_table_insert (classes, GINT_TO_POINTER (subject_id), class);
			} else {
				g_object_unref (class);
			}

		} else if (g_strcmp0 (object, RDF_PROPERTY) == 0) {
			TrackerProperty *property;

			property = tracker_ontologies_get_property_by_uri (subject);
			if (property != NULL) {
				if (seen_properties)
					g_ptr_array_add (seen_properties, g_object_ref (property));
				if (!in_update) {
					g_critical ("%s: Duplicate definition of property %s", ontology_path, subject);
				} else {
					/* Reset for a correct post-check */
					tracker_property_reset_domain_indexes (property);
					tracker_property_set_indexed (property, FALSE);
					tracker_property_set_secondary_index (property, NULL);
					tracker_property_set_writeback (property, FALSE);
					tracker_property_set_default_value (property, NULL);
				}
				return;
			}

			if (max_id) {
				subject_id = ++(*max_id);
			}

			property = tracker_property_new ();
			tracker_property_set_is_new (property, in_update);
			tracker_property_set_uri (property, subject);
			tracker_property_set_id (property, subject_id);
			tracker_ontologies_add_property (property);
			tracker_ontologies_add_id_uri_pair (subject_id, subject);

			if (seen_properties)
				g_ptr_array_add (seen_properties, g_object_ref (property));

			if (properties) {
				g_hash_table_insert (properties, GINT_TO_POINTER (subject_id), property);
			} else {
				g_object_unref (property);
			}

		} else if (g_strcmp0 (object, NRL_INVERSE_FUNCTIONAL_PROPERTY) == 0) {
			TrackerProperty *property;

			property = tracker_ontologies_get_property_by_uri (subject);
			if (property == NULL) {
				g_critical ("%s: Unknown property %s", ontology_path, subject);
				return;
			}

			tracker_property_set_is_inverse_functional_property (property, TRUE);
		} else if (g_strcmp0 (object, TRACKER_PREFIX "Namespace") == 0) {
			TrackerNamespace *namespace;

			if (tracker_ontologies_get_namespace_by_uri (subject) != NULL) {
				if (!in_update)
					g_critical ("%s: Duplicate definition of namespace %s", ontology_path, subject);
				return;
			}

			namespace = tracker_namespace_new ();
			tracker_namespace_set_is_new (namespace, in_update);
			tracker_namespace_set_uri (namespace, subject);
			tracker_ontologies_add_namespace (namespace);
			g_object_unref (namespace);

		} else if (g_strcmp0 (object, TRACKER_PREFIX "Ontology") == 0) {
			TrackerOntology *ontology;

			if (tracker_ontologies_get_ontology_by_uri (subject) != NULL) {
				if (!in_update)
					g_critical ("%s: Duplicate definition of ontology %s", ontology_path, subject);
				return;
			}

			ontology = tracker_ontology_new ();
			tracker_ontology_set_is_new (ontology, in_update);
			tracker_ontology_set_uri (ontology, subject);
			tracker_ontologies_add_ontology (ontology);
			g_object_unref (ontology);

		}
	} else if (g_strcmp0 (predicate, RDFS_SUB_CLASS_OF) == 0) {
		TrackerClass *class, *super_class;

		class = tracker_ontologies_get_class_by_uri (subject);
		if (class == NULL) {
			g_critical ("%s: Unknown class %s", ontology_path, subject);
			return;
		}

		if (tracker_class_get_is_new (class) != in_update) {
			return;
		}

		super_class = tracker_ontologies_get_class_by_uri (object);
		if (super_class == NULL) {
			g_critical ("%s: Unknown class %s", ontology_path, object);
			return;
		}

		tracker_class_add_super_class (class, super_class);
	} else if (g_strcmp0 (predicate, TRACKER_PREFIX "notify") == 0) {
		TrackerClass *class;

		class = tracker_ontologies_get_class_by_uri (subject);

		if (class == NULL) {
			g_critical ("%s: Unknown class %s", ontology_path, subject);
			return;
		}

		tracker_class_set_notify (class, (strcmp (object, "true") == 0));
	} else if (g_strcmp0 (predicate, TRACKER_PREFIX "domainIndex") == 0) {
		TrackerClass *class;
		TrackerProperty *property;
		TrackerProperty **properties;
		gboolean ignore = FALSE;
		gboolean had = FALSE;

		class = tracker_ontologies_get_class_by_uri (subject);

		if (class == NULL) {
			g_critical ("%s: Unknown class %s", ontology_path, subject);
			return;
		}

		property = tracker_ontologies_get_property_by_uri (object);

		if (property == NULL) {
			g_critical ("%s: Unknown property %s for tracker:domainIndex in %s",
			            ontology_path, object, subject);
			return;
		}

		if (tracker_property_get_multiple_values (property)) {
			g_critical ("%s: Property %s has multiple values while trying to add it as tracker:domainIndex in %s, this isn't supported",
			            ontology_path, object, subject);
			return;
		}

		properties = tracker_class_get_domain_indexes (class);
		while (*properties) {
			if (property == *properties) {
				g_debug ("%s: Property %s already a tracker:domainIndex in %s",
				         ontology_path, object, subject);
				ignore = TRUE;
			}
			properties++;
		}

		properties = tracker_class_get_last_domain_indexes (class);
		if (properties) {
			while (*properties) {
				if (property == *properties) {
					had = TRUE;
				}
				properties++;
			}
		}

		if (!ignore) {
			if (!had) {
				tracker_property_set_is_new_domain_index (property, in_update);
			}
			tracker_class_add_domain_index (class, property);
			tracker_property_add_domain_index (property, class);
		}

	} else if (g_strcmp0 (predicate, TRACKER_PREFIX "writeback") == 0) {
		TrackerProperty *property;

		property = tracker_ontologies_get_property_by_uri (subject);

		if (property == NULL) {
			g_critical ("%s: Unknown property %s", ontology_path, subject);
			return;
		}

		tracker_property_set_writeback (property, (strcmp (object, "true") == 0));
	} else if (g_strcmp0 (predicate, RDFS_SUB_PROPERTY_OF) == 0) {
		TrackerProperty *property, *super_property;

		property = tracker_ontologies_get_property_by_uri (subject);
		if (property == NULL) {
			g_critical ("%s: Unknown property %s", ontology_path, subject);
			return;
		}

		if (tracker_property_get_is_new (property) != in_update) {
			return;
		}

		super_property = tracker_ontologies_get_property_by_uri (object);
		if (super_property == NULL) {
			g_critical ("%s: Unknown property %s", ontology_path, object);
			return;
		}

		tracker_property_add_super_property (property, super_property);
	} else if (g_strcmp0 (predicate, RDFS_DOMAIN) == 0) {
		TrackerProperty *property;
		TrackerClass *domain;

		property = tracker_ontologies_get_property_by_uri (subject);
		if (property == NULL) {
			g_critical ("%s: Unknown property %s", ontology_path, subject);
			return;
		}

		if (tracker_property_get_is_new (property) != in_update) {
			return;
		}

		domain = tracker_ontologies_get_class_by_uri (object);
		if (domain == NULL) {
			g_critical ("%s: Unknown class %s", ontology_path, object);
			return;
		}

		tracker_property_set_domain (property, domain);
	} else if (g_strcmp0 (predicate, RDFS_RANGE) == 0) {
		TrackerProperty *property;
		TrackerClass *range;

		property = tracker_ontologies_get_property_by_uri (subject);
		if (property == NULL) {
			g_critical ("%s: Unknown property %s", ontology_path, subject);
			return;
		}

		if (tracker_property_get_is_new (property) != in_update) {
			check_range_conversion_is_allowed (subject, predicate, object);
		}

		range = tracker_ontologies_get_class_by_uri (object);
		if (range == NULL) {
			g_critical ("%s: Unknown class %s", ontology_path, object);
			return;
		}

		tracker_property_set_range (property, range);
	} else if (g_strcmp0 (predicate, NRL_MAX_CARDINALITY) == 0) {
		TrackerProperty *property;

		property = tracker_ontologies_get_property_by_uri (subject);
		if (property == NULL) {
			g_critical ("%s: Unknown property %s", ontology_path, subject);
			return;
		}

		if (tracker_property_get_is_new (property) != in_update) {
			return;
		}

		if (atoi (object) == 1) {
			tracker_property_set_multiple_values (property, FALSE);
		}
	} else if (g_strcmp0 (predicate, TRACKER_PREFIX "indexed") == 0) {
		TrackerProperty *property;

		property = tracker_ontologies_get_property_by_uri (subject);
		if (property == NULL) {
			g_critical ("%s: Unknown property %s", ontology_path, subject);
			return;
		}

		tracker_property_set_indexed (property, (strcmp (object, "true") == 0));
	} else if (g_strcmp0 (predicate, TRACKER_PREFIX "secondaryIndex") == 0) {
		TrackerProperty *property, *secondary_index;

		property = tracker_ontologies_get_property_by_uri (subject);
		if (property == NULL) {
			g_critical ("%s: Unknown property %s", ontology_path, subject);
			return;
		}

		secondary_index = tracker_ontologies_get_property_by_uri (object);
		if (secondary_index == NULL) {
			g_critical ("%s: Unknown property %s", ontology_path, object);
			return;
		}

		tracker_property_set_secondary_index (property, secondary_index);
	} else if (g_strcmp0 (predicate, TRACKER_PREFIX "transient") == 0) {
		TrackerProperty *property;

		property = tracker_ontologies_get_property_by_uri (subject);
		if (property == NULL) {
			g_critical ("%s: Unknown property %s", ontology_path, subject);
			return;
		}

		if (tracker_property_get_is_new (property) != in_update) {
			return;
		}

		if (g_strcmp0 (object, "true") == 0) {
			tracker_property_set_transient (property, TRUE);
		}
	} else if (g_strcmp0 (predicate, TRACKER_PREFIX "isAnnotation") == 0) {
		TrackerProperty *property;

		property = tracker_ontologies_get_property_by_uri (subject);
		if (property == NULL) {
			g_critical ("%s: Unknown property %s", ontology_path, subject);
			return;
		}

		if (tracker_property_get_is_new (property) != in_update) {
			return;
		}

		if (g_strcmp0 (object, "true") == 0) {
			tracker_property_set_embedded (property, FALSE);
		}
	} else if (g_strcmp0 (predicate, TRACKER_PREFIX "fulltextIndexed") == 0) {
		TrackerProperty *property;

		property = tracker_ontologies_get_property_by_uri (subject);
		if (property == NULL) {
			g_critical ("%s: Unknown property %s", ontology_path, subject);
			return;
		}

		if (tracker_property_get_is_new (property) != in_update) {
			return;
		}

		if (strcmp (object, "true") == 0) {
			tracker_property_set_fulltext_indexed (property, TRUE);
		}
	} else if (g_strcmp0 (predicate, TRACKER_PREFIX "fulltextNoLimit") == 0) {
		TrackerProperty *property;

		property = tracker_ontologies_get_property_by_uri (subject);
		if (property == NULL) {
			g_critical ("%s: Unknown property %s", ontology_path, subject);
			return;
		}

		if (tracker_property_get_is_new (property) != in_update) {
			return;
		}

		if (strcmp (object, "true") == 0) {
			tracker_property_set_fulltext_no_limit (property, TRUE);
		}
	} else if (g_strcmp0 (predicate, TRACKER_PREFIX "defaultValue") == 0) {
		TrackerProperty *property;

		property = tracker_ontologies_get_property_by_uri (subject);
		if (property == NULL) {
			g_critical ("%s: Unknown property %s", ontology_path, subject);
			return;
		}

		tracker_property_set_default_value (property, object);
	} else if (g_strcmp0 (predicate, TRACKER_PREFIX "prefix") == 0) {
		TrackerNamespace *namespace;

		namespace = tracker_ontologies_get_namespace_by_uri (subject);
		if (namespace == NULL) {
			g_critical ("%s: Unknown namespace %s", ontology_path, subject);
			return;
		}

		if (tracker_namespace_get_is_new (namespace) != in_update) {
			return;
		}

		tracker_namespace_set_prefix (namespace, object);
	} else if (g_strcmp0 (predicate, NAO_LAST_MODIFIED) == 0) {
		TrackerOntology *ontology;

		ontology = tracker_ontologies_get_ontology_by_uri (subject);
		if (ontology == NULL) {
			g_critical ("%s: Unknown ontology %s", ontology_path, subject);
			return;
		}

		if (tracker_ontology_get_is_new (ontology) != in_update) {
			return;
		}

		tracker_ontology_set_last_modified (ontology, tracker_string_to_date (object, NULL, NULL));
	}
}


static void
check_for_deleted_domain_index (TrackerClass *class)
{
	TrackerProperty **last_domain_indexes;
	GSList *hfound = NULL, *deleted = NULL;

	last_domain_indexes = tracker_class_get_last_domain_indexes (class);

	while (*last_domain_indexes) {
		TrackerProperty *last_domain_index = *last_domain_indexes;
		gboolean found = FALSE;
		TrackerProperty **domain_indexes;

		domain_indexes = tracker_class_get_domain_indexes (class);

		while (*domain_indexes) {
			TrackerProperty *domain_index = *domain_indexes;
			if (last_domain_index == domain_index) {
				found = TRUE;
				hfound = g_slist_prepend (hfound, domain_index);
				break;
			}
			domain_indexes++;
		}

		if (!found) {
			deleted = g_slist_prepend (deleted, last_domain_index);
		}

		last_domain_indexes++;
	}


	if (deleted) {
		GSList *l;

		tracker_class_set_db_schema_changed (class, TRUE);

		for (l = hfound; l != NULL; l = l->next) {
			TrackerProperty *prop = l->data;
			g_debug ("Ontology change: keeping tracker:domainIndex: %s",
			         tracker_property_get_name (prop));
			tracker_property_set_is_new_domain_index (prop, TRUE);
		}

		for (l = deleted; l != NULL; l = l->next) {
			TrackerProperty *prop = l->data;
			/* TODO: delete from introspection too */
			g_debug ("Ontology change: deleting tracker:domainIndex: %s",
			         tracker_property_get_name (prop));
			tracker_property_del_domain_index (prop, class);
			tracker_class_del_domain_index (class, prop);
		}

		g_slist_free (deleted);
	}

	g_slist_free (hfound);
}

void
tracker_data_ontology_process_changes_pre_db (GPtrArray *seen_classes,
                                               GPtrArray *seen_properties)
{
	gint i;
	if (seen_classes) {
		for (i = 0; i < seen_classes->len; i++) {
			TrackerClass *class = g_ptr_array_index (seen_classes, i);
			check_for_deleted_domain_index (class);

		}
	}
}

void
tracker_data_ontology_process_changes_post_db (GPtrArray *seen_classes,
                                               GPtrArray *seen_properties)
{
	gint i;

	/* This updates property-property changes and marks classes for necessity
	 * of having their tables recreated later. There's support for
	 * tracker:notify, tracker:writeback and tracker:indexed */

	if (seen_classes) {
		for (i = 0; i < seen_classes->len; i++) {
			TrackerClass *class = g_ptr_array_index (seen_classes, i);
			const gchar *subject;

			subject = tracker_class_get_uri (class);

			if (tracker_class_get_notify (class)) {
				update_property_value ("tracker:notify",
				                       subject,
				                       TRACKER_PREFIX "notify",
				                       "true", allowed_boolean_conversions,
				                       class, NULL);
			} else {
				update_property_value ("tracker:notify",
				                       subject,
				                       TRACKER_PREFIX "notify",
				                       "false", allowed_boolean_conversions,
				                       class, NULL);
			}
		}
	}

	if (seen_properties) {
		for (i = 0; i < seen_properties->len; i++) {
			TrackerProperty *property = g_ptr_array_index (seen_properties, i);
			const gchar *subject;
			TrackerProperty *secondary_index;
			gboolean indexed_set = FALSE;

			subject = tracker_property_get_uri (property);

			if (tracker_property_get_writeback (property)) {
				update_property_value ("tracker:writeback",
				                       subject,
				                       TRACKER_PREFIX "writeback",
				                       "true", allowed_boolean_conversions,
				                       NULL, property);
			} else {
				update_property_value ("tracker:writeback",
				                       subject,
				                       TRACKER_PREFIX "writeback",
				                       "false", allowed_boolean_conversions,
				                       NULL, property);
			}

			if (tracker_property_get_indexed (property)) {
				if (update_property_value ("tracker:indexed",
				                           subject,
				                           TRACKER_PREFIX "indexed",
				                           "true", allowed_boolean_conversions,
				                           NULL, property)) {
					fix_indexed (property);
					indexed_set = TRUE;
				}
			} else {
				if (update_property_value ("tracker:indexed",
				                           subject,
				                           TRACKER_PREFIX "indexed",
				                           "false", allowed_boolean_conversions,
				                           NULL, property)) {
					fix_indexed (property);
					indexed_set = TRUE;
				}
			}

			secondary_index = tracker_property_get_secondary_index (property);

			if (secondary_index) {
				if (update_property_value ("tracker:secondaryIndex",
				                           subject,
				                           TRACKER_PREFIX "secondaryIndex",
				                           tracker_property_get_uri (secondary_index), NULL,
				                           NULL, property)) {
					if (!indexed_set)
						fix_indexed (property);
				}
			} else {
				if (update_property_value ("tracker:secondaryIndex",
				                           subject,
				                           TRACKER_PREFIX "secondaryIndex",
				                           NULL, NULL,
				                           NULL, property)) {
					if (!indexed_set)
						fix_indexed (property);
				}
			}

			if (update_property_value ("rdfs:range", subject, RDFS_PREFIX "range",
			                           tracker_class_get_uri (tracker_property_get_range (property)),
			                           allowed_range_conversions,
			                           NULL, property)) {
				TrackerClass *class;

				class = tracker_property_get_domain (property);
				tracker_class_set_db_schema_changed (class, TRUE);
				tracker_property_set_db_schema_changed (property, TRUE);
			}

			if (update_property_value ("tracker:defaultValue", subject, TRACKER_PREFIX "defaultValue",
			                           tracker_property_get_default_value (property),
			                           NULL, NULL, property)) {
				TrackerClass *class;

				class = tracker_property_get_domain (property);
				tracker_class_set_db_schema_changed (class, TRUE);
				tracker_property_set_db_schema_changed (property, TRUE);
			}
		}
	}
}

void
tracker_data_ontology_process_changes_post_import (GPtrArray *seen_classes,
                                                   GPtrArray *seen_properties)
{
	return;
}

void
tracker_data_ontology_free_seen (GPtrArray *seen)
{
	if (seen) {
		g_ptr_array_foreach (seen, (GFunc) g_object_unref, NULL);
		g_ptr_array_free (seen, TRUE);
	}
}

static void
load_ontology_file_from_path (const gchar        *ontology_path,
                              gint               *max_id,
                              gboolean            in_update,
                              GPtrArray          *seen_classes,
                              GPtrArray          *seen_properties)
{
	TrackerTurtleReader *reader;
	GError              *error = NULL;

	reader = tracker_turtle_reader_new (ontology_path, &error);
	if (error) {
		g_critical ("Turtle parse error: %s", error->message);
		g_error_free (error);
		return;
	}

	/* Post checks are only needed for ontology updates, not the initial
	 * ontology */

	while (error == NULL && tracker_turtle_reader_next (reader, &error)) {
		const gchar *subject, *predicate, *object;

		subject = tracker_turtle_reader_get_subject (reader);
		predicate = tracker_turtle_reader_get_predicate (reader);
		object = tracker_turtle_reader_get_object (reader);

		tracker_data_ontology_load_statement (ontology_path, 0, subject, predicate, object,
		                                      max_id, in_update, NULL, NULL,
		                                      seen_classes, seen_properties);
	}

	g_object_unref (reader);

	if (error) {
		g_critical ("Turtle parse error: %s", error->message);
		g_error_free (error);
	}
}


static TrackerOntology*
get_ontology_from_path (const gchar *ontology_path)
{
	TrackerTurtleReader *reader;
	GError *error = NULL;
	GHashTable *ontology_uris;
	TrackerOntology *ret = NULL;

	reader = tracker_turtle_reader_new (ontology_path, &error);

	if (error) {
		g_critical ("Turtle parse error: %s", error->message);
		g_error_free (error);
		return NULL;
	}

	ontology_uris = g_hash_table_new_full (g_str_hash,
	                                       g_str_equal,
	                                       g_free,
	                                       g_object_unref);

	while (error == NULL && tracker_turtle_reader_next (reader, &error)) {
		const gchar *subject, *predicate, *object;

		subject = tracker_turtle_reader_get_subject (reader);
		predicate = tracker_turtle_reader_get_predicate (reader);
		object = tracker_turtle_reader_get_object (reader);

		if (g_strcmp0 (predicate, RDF_TYPE) == 0) {
			if (g_strcmp0 (object, TRACKER_PREFIX "Ontology") == 0) {
				TrackerOntology *ontology;

				ontology = tracker_ontology_new ();
				tracker_ontology_set_uri (ontology, subject);

				/* Passes ownership */
				g_hash_table_insert (ontology_uris,
				                     g_strdup (subject),
				                     ontology);
			}
		} else if (g_strcmp0 (predicate, NAO_LAST_MODIFIED) == 0) {
			TrackerOntology *ontology;

			ontology = g_hash_table_lookup (ontology_uris, subject);
			if (ontology == NULL) {
				g_critical ("%s: Unknown ontology %s", ontology_path, subject);
				return NULL;
			}

			tracker_ontology_set_last_modified (ontology, tracker_string_to_date (object, NULL, NULL));

			/* This one is here because lower ontology_uris is destroyed, and
			 * else would this one's reference also be destroyed with it */
			ret = g_object_ref (ontology);

			break;
		}
	}

	g_hash_table_unref (ontology_uris);
	g_object_unref (reader);

	if (error) {
		g_critical ("Turtle parse error: %s", error->message);
		g_error_free (error);
	}

	return ret;
}

static void
load_ontology_from_journal (GHashTable **classes_out,
                            GHashTable **properties_out,
                            GHashTable **id_uri_map_out)
{
	GHashTable *id_uri_map;
	GHashTable *classes, *properties;

	classes = g_hash_table_new_full (g_direct_hash, g_direct_equal,
	                                 NULL, (GDestroyNotify) g_object_unref);

	properties = g_hash_table_new_full (g_direct_hash, g_direct_equal,
	                                    NULL, (GDestroyNotify) g_object_unref);

	id_uri_map = g_hash_table_new (g_direct_hash, g_direct_equal);

	while (tracker_db_journal_reader_next (NULL)) {
		TrackerDBJournalEntryType type;

		type = tracker_db_journal_reader_get_type ();
		if (type == TRACKER_DB_JOURNAL_RESOURCE) {
			gint id;
			const gchar *uri;

			tracker_db_journal_reader_get_resource (&id, &uri);
			g_hash_table_insert (id_uri_map, GINT_TO_POINTER (id), (gpointer) uri);
		} else if (type == TRACKER_DB_JOURNAL_END_TRANSACTION) {
			/* end of initial transaction => end of ontology */
			break;
		} else {
			const gchar *subject, *predicate, *object;
			gint subject_id, predicate_id, object_id;

			if (type == TRACKER_DB_JOURNAL_INSERT_STATEMENT) {
				tracker_db_journal_reader_get_statement (NULL, &subject_id, &predicate_id, &object);
			} else if (type == TRACKER_DB_JOURNAL_INSERT_STATEMENT_ID) {
				tracker_db_journal_reader_get_statement_id (NULL, &subject_id, &predicate_id, &object_id);
				object = g_hash_table_lookup (id_uri_map, GINT_TO_POINTER (object_id));
			} else {
				continue;
			}

			subject = g_hash_table_lookup (id_uri_map, GINT_TO_POINTER (subject_id));
			predicate = g_hash_table_lookup (id_uri_map, GINT_TO_POINTER (predicate_id));

			/* Post checks are only needed for ontology updates, not the initial
			 * ontology */

			tracker_data_ontology_load_statement ("journal", subject_id, subject, predicate,
			                                      object, NULL, FALSE, classes, properties,
			                                      NULL, NULL);
		}
	}

	*classes_out = classes;
	*properties_out = properties;
	*id_uri_map_out = id_uri_map;
}

void
tracker_data_ontology_process_statement (const gchar *graph,
                                         const gchar *subject,
                                         const gchar *predicate,
                                         const gchar *object,
                                         gboolean     is_uri,
                                         gboolean     in_update,
                                         gboolean     ignore_nao_last_modified)
{
	GError *error = NULL;

	if (g_strcmp0 (predicate, RDF_TYPE) == 0) {
		if (g_strcmp0 (object, RDFS_CLASS) == 0) {
			TrackerClass *class;

			class = tracker_ontologies_get_class_by_uri (subject);

			if (class && tracker_class_get_is_new (class) != in_update) {
				return;
			}
		} else if (g_strcmp0 (object, RDF_PROPERTY) == 0) {
			TrackerProperty *prop;

			prop = tracker_ontologies_get_property_by_uri (subject);

			if (prop && tracker_property_get_is_new (prop) != in_update) {
				return;
			}
		} else if (g_strcmp0 (object, TRACKER_PREFIX "Namespace") == 0) {
			TrackerNamespace *namespace;

			namespace = tracker_ontologies_get_namespace_by_uri (subject);

			if (namespace && tracker_namespace_get_is_new (namespace) != in_update) {
				return;
			}
		} else if (g_strcmp0 (object, TRACKER_PREFIX "Ontology") == 0) {
			TrackerOntology *ontology;

			ontology = tracker_ontologies_get_ontology_by_uri (subject);

			if (ontology && tracker_ontology_get_is_new (ontology) != in_update) {
				return;
			}
		}
	} else if (g_strcmp0 (predicate, RDFS_SUB_CLASS_OF) == 0) {
		TrackerClass *class;

		class = tracker_ontologies_get_class_by_uri (subject);

		if (class && tracker_class_get_is_new (class) != in_update) {
			return;
		}
	} else if (g_strcmp0 (predicate, RDFS_SUB_PROPERTY_OF) == 0          ||
	           g_strcmp0 (predicate, RDFS_DOMAIN) == 0                   ||
	           g_strcmp0 (predicate, RDFS_RANGE) == 0                    ||
	           g_strcmp0 (predicate, NRL_MAX_CARDINALITY) == 0           ||
	           g_strcmp0 (predicate, TRACKER_PREFIX "indexed") == 0      ||
	           g_strcmp0 (predicate, TRACKER_PREFIX "transient") == 0    ||
	           g_strcmp0 (predicate, TRACKER_PREFIX "isAnnotation") == 0 ||
	           g_strcmp0 (predicate, TRACKER_PREFIX "fulltextIndexed") == 0) {
		TrackerProperty *prop;

		prop = tracker_ontologies_get_property_by_uri (subject);

		if (prop && tracker_property_get_is_new (prop) != in_update) {
			return;
		}
	} else if (g_strcmp0 (predicate, TRACKER_PREFIX "prefix") == 0) {
		TrackerNamespace *namespace;

		namespace = tracker_ontologies_get_namespace_by_uri (subject);

		if (namespace && tracker_namespace_get_is_new (namespace) != in_update) {
			return;
		}
	} else if (g_strcmp0 (predicate, NAO_LAST_MODIFIED) == 0) {
		TrackerOntology *ontology;

		ontology = tracker_ontologies_get_ontology_by_uri (subject);

		if (ontology && tracker_ontology_get_is_new (ontology) != in_update) {
			return;
		}

		if (ignore_nao_last_modified) {
			return;
		}
	}

	if (is_uri) {
		tracker_data_insert_statement_with_uri (graph, subject,
												predicate, object,
												&error);

		if (error != NULL) {
			g_critical ("%s", error->message);
			g_error_free (error);
			return;
		}

	} else {
		tracker_data_insert_statement_with_string (graph, subject,
												   predicate, object,
												   &error);

		if (error != NULL) {
			g_critical ("%s", error->message);
			g_error_free (error);
			return;
		}
	}
}

static void
import_ontology_path (const gchar *ontology_path,
                      gboolean in_update,
                      gboolean ignore_nao_last_modified)
{
	GError          *error = NULL;

	TrackerTurtleReader* reader;

	reader = tracker_turtle_reader_new (ontology_path, &error);

	if (error != NULL) {
		g_critical ("%s", error->message);
		g_error_free (error);
		return;
	}

	while (tracker_turtle_reader_next (reader, &error)) {

		const gchar *graph = tracker_turtle_reader_get_graph (reader);
		const gchar *subject = tracker_turtle_reader_get_subject (reader);
		const gchar *predicate = tracker_turtle_reader_get_predicate (reader);
		const gchar *object  = tracker_turtle_reader_get_object (reader);

		tracker_data_ontology_process_statement (graph, subject, predicate, object,
		                                         tracker_turtle_reader_get_object_is_uri (reader),
		                                         in_update, ignore_nao_last_modified);

	}

	g_object_unref (reader);

	if (error) {
		g_critical ("%s", error->message);
		g_error_free (error);
	}
}

static void
class_add_super_classes_from_db (TrackerDBInterface *iface,
                                 TrackerClass       *class)
{
	TrackerDBStatement *stmt;
	TrackerDBCursor *cursor;
	GError *error = NULL;

	stmt = tracker_db_interface_create_statement (iface, &error,
	                                              "SELECT (SELECT Uri FROM Resource WHERE ID = \"rdfs:subClassOf\") "
	                                              "FROM \"rdfs:Class_rdfs:subClassOf\" "
	                                              "WHERE ID = (SELECT ID FROM Resource WHERE Uri = ?)");

	if (!stmt) {
		g_critical ("%s", error->message);
		g_error_free (error);
		return;
	}

	tracker_db_statement_bind_text (stmt, 0, tracker_class_get_uri (class));
	cursor = tracker_db_statement_start_cursor (stmt, NULL);
	g_object_unref (stmt);

	if (cursor) {
		while (tracker_db_cursor_iter_next (cursor, NULL)) {
			TrackerClass *super_class;
			const gchar *super_class_uri;

			super_class_uri = tracker_db_cursor_get_string (cursor, 0, NULL);
			super_class = tracker_ontologies_get_class_by_uri (super_class_uri);
			tracker_class_add_super_class (class, super_class);
		}

		g_object_unref (cursor);
	}
}


static void
class_add_domain_indexes_from_db (TrackerDBInterface *iface,
                                  TrackerClass       *class)
{
	TrackerDBStatement *stmt;
	TrackerDBCursor *cursor;
	GError *error = NULL;

	stmt = tracker_db_interface_create_statement (iface, &error,
	                                              "SELECT (SELECT Uri FROM Resource WHERE ID = \"tracker:domainIndex\") "
	                                              "FROM \"rdfs:Class_tracker:domainIndex\" "
	                                              "WHERE ID = (SELECT ID FROM Resource WHERE Uri = ?)");

	if (!stmt) {
		g_critical ("%s", error->message);
		g_error_free (error);
		return;
	}

	tracker_db_statement_bind_text (stmt, 0, tracker_class_get_uri (class));
	cursor = tracker_db_statement_start_cursor (stmt, NULL);
	g_object_unref (stmt);

	if (cursor) {
		while (tracker_db_cursor_iter_next (cursor, NULL)) {
			TrackerProperty *domain_index;
			const gchar *domain_index_uri;

			domain_index_uri = tracker_db_cursor_get_string (cursor, 0, NULL);
			domain_index = tracker_ontologies_get_property_by_uri (domain_index_uri);
			tracker_class_add_domain_index (class, domain_index);
			tracker_property_add_domain_index (domain_index, class);
		}

		g_object_unref (cursor);
	}
}

static void
property_add_super_properties_from_db (TrackerDBInterface *iface,
                                       TrackerProperty *property)
{
	TrackerDBStatement *stmt;
	TrackerDBCursor *cursor;
	GError *error = NULL;

	stmt = tracker_db_interface_create_statement (iface, &error,
	                                              "SELECT (SELECT Uri FROM Resource WHERE ID = \"rdfs:subPropertyOf\") "
	                                              "FROM \"rdf:Property_rdfs:subPropertyOf\" "
	                                              "WHERE ID = (SELECT ID FROM Resource WHERE Uri = ?)");

	if (!stmt) {
		g_critical ("%s", error->message);
		g_error_free (error);
		return;
	}

	tracker_db_statement_bind_text (stmt, 0, tracker_property_get_uri (property));
	cursor = tracker_db_statement_start_cursor (stmt, NULL);
	g_object_unref (stmt);

	if (cursor) {
		while (tracker_db_cursor_iter_next (cursor, NULL)) {
			TrackerProperty *super_property;
			const gchar *super_property_uri;

			super_property_uri = tracker_db_cursor_get_string (cursor, 0, NULL);
			super_property = tracker_ontologies_get_property_by_uri (super_property_uri);
			tracker_property_add_super_property (property, super_property);
		}

		g_object_unref (cursor);
	}
}

static void
db_get_static_data (TrackerDBInterface *iface)
{
	TrackerDBStatement *stmt;
	TrackerDBCursor *cursor = NULL;
	TrackerDBResultSet *result_set;
	TrackerClass **classes;
	guint n_classes, i;
	GError *error = NULL;

	stmt = tracker_db_interface_create_statement (iface, &error,
	                                              "SELECT (SELECT Uri FROM Resource WHERE ID = \"tracker:Ontology\".ID), "
	                                              "\"nao:lastModified\" "
	                                              "FROM \"tracker:Ontology\"");

	if (stmt) {
		cursor = tracker_db_statement_start_cursor (stmt, &error);
		g_object_unref (stmt);
	}

	if (cursor) {
		while (tracker_db_cursor_iter_next (cursor, &error)) {
			TrackerOntology *ontology;
			const gchar     *uri;
			time_t           last_mod;

			ontology = tracker_ontology_new ();

			uri = tracker_db_cursor_get_string (cursor, 0, NULL);
			last_mod = (time_t) tracker_db_cursor_get_int (cursor, 1);

			tracker_ontology_set_is_new (ontology, FALSE);
			tracker_ontology_set_uri (ontology, uri);
			tracker_ontology_set_last_modified (ontology, last_mod);
			tracker_ontologies_add_ontology (ontology);

			g_object_unref (ontology);
		}

		g_object_unref (cursor);
		cursor = NULL;
	}

	if (error) {
		g_warning ("%s", error->message);
		g_clear_error (&error);
	}

	stmt = tracker_db_interface_create_statement (iface, &error,
	                                              "SELECT (SELECT Uri FROM Resource WHERE ID = \"tracker:Namespace\".ID), "
	                                              "\"tracker:prefix\" "
	                                              "FROM \"tracker:Namespace\"");

	if (stmt) {
		cursor = tracker_db_statement_start_cursor (stmt, &error);
		g_object_unref (stmt);
	}

	if (cursor) {
		while (tracker_db_cursor_iter_next (cursor, &error)) {
			TrackerNamespace *namespace;
			const gchar      *uri, *prefix;

			namespace = tracker_namespace_new ();

			uri = tracker_db_cursor_get_string (cursor, 0, NULL);
			prefix = tracker_db_cursor_get_string (cursor, 1, NULL);

			tracker_namespace_set_is_new (namespace, FALSE);
			tracker_namespace_set_uri (namespace, uri);
			tracker_namespace_set_prefix (namespace, prefix);
			tracker_ontologies_add_namespace (namespace);

			g_object_unref (namespace);

		}

		g_object_unref (cursor);
		cursor = NULL;
	}

	if (error) {
		g_warning ("%s", error->message);
		g_clear_error (&error);
	}

	stmt = tracker_db_interface_create_statement (iface, &error,
	                                              "SELECT \"rdfs:Class\".ID, "
	                                              "(SELECT Uri FROM Resource WHERE ID = \"rdfs:Class\".ID), "
	                                              "\"tracker:notify\" "
	                                              "FROM \"rdfs:Class\" ORDER BY ID");

	if (stmt) {
		cursor = tracker_db_statement_start_cursor (stmt, &error);
		g_object_unref (stmt);
	}

	if (cursor) {
		while (tracker_db_cursor_iter_next (cursor, &error)) {
			TrackerClass *class;
			const gchar  *uri;
			gint          id;
			gint          count;
			GValue        value = { 0 };
			gboolean      notify;

			class = tracker_class_new ();

			id = tracker_db_cursor_get_int (cursor, 0);
			uri = tracker_db_cursor_get_string (cursor, 1, NULL);

			tracker_db_cursor_get_value (cursor, 2, &value);

			if (G_VALUE_TYPE (&value) != 0) {
				notify = (g_value_get_int64 (&value) == 1);
				g_value_unset (&value);
			} else {
				/* NULL */
				notify = FALSE;
			}

			tracker_class_set_db_schema_changed (class, FALSE);
			tracker_class_set_is_new (class, FALSE);
			tracker_class_set_uri (class, uri);
			tracker_class_set_notify (class, notify);

			class_add_super_classes_from_db (iface, class);

			/* We do this later, we first need to load the properties too
			   class_add_domain_indexes_from_db (iface, class); */

			tracker_ontologies_add_class (class);
			tracker_ontologies_add_id_uri_pair (id, uri);
			tracker_class_set_id (class, id);

			/* xsd classes do not derive from rdfs:Resource and do not use separate tables */
			if (!g_str_has_prefix (tracker_class_get_name (class), "xsd:")) {
				/* update statistics */
				stmt = tracker_db_interface_create_statement (iface, &error, "SELECT COUNT(1) FROM \"%s\"", tracker_class_get_name (class));

				if (error) {
					g_warning ("%s", error->message);
					g_clear_error (&error);
				} else {
					result_set = tracker_db_statement_execute (stmt, NULL);
					tracker_db_result_set_get (result_set, 0, &count, -1);
					tracker_class_set_count (class, count);
					g_object_unref (result_set);
					g_object_unref (stmt);
				}
			}

			g_object_unref (class);
		}

		g_object_unref (cursor);
		cursor = NULL;
	}

	if (error) {
		g_warning ("%s", error->message);
		g_clear_error (&error);
	}

	stmt = tracker_db_interface_create_statement (iface, &error,
	                                              "SELECT \"rdf:Property\".ID, (SELECT Uri FROM Resource WHERE ID = \"rdf:Property\".ID), "
	                                              "(SELECT Uri FROM Resource WHERE ID = \"rdfs:domain\"), "
	                                              "(SELECT Uri FROM Resource WHERE ID = \"rdfs:range\"), "
	                                              "\"nrl:maxCardinality\", "
	                                              "\"tracker:indexed\", "
	                                              "(SELECT Uri FROM Resource WHERE ID = \"tracker:secondaryIndex\"), "
	                                              "\"tracker:fulltextIndexed\", "
	                                              "\"tracker:fulltextNoLimit\", "
	                                              "\"tracker:transient\", "
	                                              "\"tracker:isAnnotation\", "
	                                              "\"tracker:writeback\", "
	                                              "(SELECT 1 FROM \"rdfs:Resource_rdf:type\" WHERE ID = \"rdf:Property\".ID AND "
	                                              "\"rdf:type\" = (SELECT ID FROM Resource WHERE Uri = '" NRL_INVERSE_FUNCTIONAL_PROPERTY "')), "
	                                              "\"tracker:defaultValue\" "
	                                              "FROM \"rdf:Property\" ORDER BY ID");

	if (stmt) {
		cursor = tracker_db_statement_start_cursor (stmt, &error);
		g_object_unref (stmt);
	}

	if (cursor) {
		while (tracker_db_cursor_iter_next (cursor, &error)) {
			GValue value = { 0 };
			TrackerProperty *property;
			const gchar     *uri, *domain_uri, *range_uri, *secondary_index_uri, *default_value;
			gboolean         multi_valued, indexed, fulltext_indexed, fulltext_no_limit;
			gboolean         transient, annotation, is_inverse_functional_property;
			gboolean         writeback;
			gint             id;

			property = tracker_property_new ();

			id = tracker_db_cursor_get_int (cursor, 0);
			uri = tracker_db_cursor_get_string (cursor, 1, NULL);
			domain_uri = tracker_db_cursor_get_string (cursor, 2, NULL);
			range_uri = tracker_db_cursor_get_string (cursor, 3, NULL);

			tracker_db_cursor_get_value (cursor, 4, &value);

			if (G_VALUE_TYPE (&value) != 0) {
				multi_valued = (g_value_get_int64 (&value) > 1);
				g_value_unset (&value);
			} else {
				/* nrl:maxCardinality not set
				   not limited to single value */
				multi_valued = TRUE;
			}

			tracker_db_cursor_get_value (cursor, 5, &value);

			if (G_VALUE_TYPE (&value) != 0) {
				indexed = (g_value_get_int64 (&value) == 1);
				g_value_unset (&value);
			} else {
				/* NULL */
				indexed = FALSE;
			}

			secondary_index_uri = tracker_db_cursor_get_string (cursor, 6, NULL);

			tracker_db_cursor_get_value (cursor, 7, &value);

			if (G_VALUE_TYPE (&value) != 0) {
				fulltext_indexed = (g_value_get_int64 (&value) == 1);
				g_value_unset (&value);
			} else {
				/* NULL */
				fulltext_indexed = FALSE;
			}

			tracker_db_cursor_get_value (cursor, 8, &value);

			if (G_VALUE_TYPE (&value) != 0) {
				fulltext_no_limit = (g_value_get_int64 (&value) == 1);
				g_value_unset (&value);
			} else {
				/* NULL */
				fulltext_no_limit = FALSE;
			}

			tracker_db_cursor_get_value (cursor, 9, &value);

			if (G_VALUE_TYPE (&value) != 0) {
				transient = (g_value_get_int64 (&value) == 1);
				g_value_unset (&value);
			} else {
				/* NULL */
				transient = FALSE;
			}

			tracker_db_cursor_get_value (cursor, 10, &value);

			if (G_VALUE_TYPE (&value) != 0) {
				annotation = (g_value_get_int64 (&value) == 1);
				g_value_unset (&value);
			} else {
				/* NULL */
				annotation = FALSE;
			}


			/* tracker:writeback column */
			tracker_db_cursor_get_value (cursor, 11, &value);

			if (G_VALUE_TYPE (&value) != 0) {
				writeback = (g_value_get_int64 (&value) == 1);
				g_value_unset (&value);
			} else {
				/* NULL */
				writeback = FALSE;
			}

			/* NRL_INVERSE_FUNCTIONAL_PROPERTY column */
			tracker_db_cursor_get_value (cursor, 12, &value);

			if (G_VALUE_TYPE (&value) != 0) {
				is_inverse_functional_property = TRUE;
				g_value_unset (&value);
			} else {
				/* NULL */
				is_inverse_functional_property = FALSE;
			}

			default_value = tracker_db_cursor_get_string (cursor, 13, NULL);

			tracker_property_set_is_new_domain_index (property, FALSE);
			tracker_property_set_is_new (property, FALSE);
			tracker_property_set_transient (property, transient);
			tracker_property_set_uri (property, uri);
			tracker_property_set_id (property, id);
			tracker_property_set_domain (property, tracker_ontologies_get_class_by_uri (domain_uri));
			tracker_property_set_range (property, tracker_ontologies_get_class_by_uri (range_uri));
			tracker_property_set_multiple_values (property, multi_valued);
			tracker_property_set_indexed (property, indexed);
			tracker_property_set_default_value (property, default_value);

			tracker_property_set_db_schema_changed (property, FALSE);
			tracker_property_set_writeback (property, writeback);

			if (secondary_index_uri) {
				tracker_property_set_secondary_index (property, tracker_ontologies_get_property_by_uri (secondary_index_uri));
			}

			tracker_property_set_fulltext_indexed (property, fulltext_indexed);
			tracker_property_set_fulltext_no_limit (property, fulltext_no_limit);
			tracker_property_set_embedded (property, !annotation);
			tracker_property_set_is_inverse_functional_property (property, is_inverse_functional_property);
			property_add_super_properties_from_db (iface, property);

			tracker_ontologies_add_property (property);
			tracker_ontologies_add_id_uri_pair (id, uri);

			g_object_unref (property);

		}

		g_object_unref (cursor);
		cursor = NULL;
	}

	/* Now that the properties are loaded we can do this foreach class */
	classes = tracker_ontologies_get_classes (&n_classes);
	for (i = 0; i < n_classes; i++) {
		class_add_domain_indexes_from_db (iface, classes[i]);
	}

	if (error) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}
}


static void
insert_uri_in_resource_table (TrackerDBInterface *iface,
                              const gchar        *uri,
                              gint                id)
{
	TrackerDBStatement *stmt;
	GError *error = NULL;

	stmt = tracker_db_interface_create_statement (iface, &error,
	                                              "INSERT "
	                                              "INTO Resource "
	                                              "(ID, Uri) "
	                                              "VALUES (?, ?)");
	if (error) {
		g_critical ("%s", error->message);
		g_error_free (error);
		return;
	}

	tracker_db_statement_bind_int (stmt, 0, id);
	tracker_db_statement_bind_text (stmt, 1, uri);
	tracker_db_statement_execute (stmt, &error);

	if (error) {
		g_critical ("%s\n", error->message);
		g_clear_error (&error);
	}

	if (!in_journal_replay) {
		tracker_db_journal_append_resource (id, uri);
	}

	g_object_unref (stmt);

}

static void
range_change_for (TrackerProperty *property,
                  GString         *in_col_sql,
                  GString         *sel_col_sql,
                  const gchar     *field_name)
{
	/* TODO: TYPE_RESOURCE and TYPE_DATETIME are completely unhandled atm, we
	 * should forbid conversion from anything to resource or datetime in error
	 * handling earlier */

	g_string_append_printf (in_col_sql, ", \"%s\", \"%s:graph\"",
	                        field_name, field_name);

	if (tracker_property_get_data_type (property) == TRACKER_PROPERTY_TYPE_INTEGER ||
	    tracker_property_get_data_type (property) == TRACKER_PROPERTY_TYPE_DOUBLE) {
			g_string_append_printf (sel_col_sql, ", \"%s\" + 0, \"%s:graph\"",
			                        field_name, field_name);
	} else if (tracker_property_get_data_type (property) == TRACKER_PROPERTY_TYPE_DATETIME) {

		/* TODO (see above) */

		g_string_append_printf (sel_col_sql, ", \"%s\", \"%s:graph\"",
		                        field_name, field_name);

		g_string_append_printf (in_col_sql, ", \"%s:localDate\", \"%s:localTime\"",
		                        tracker_property_get_name (property),
		                        tracker_property_get_name (property));

		g_string_append_printf (sel_col_sql, ", \"%s:localDate\", \"%s:localTime\"",
		                        tracker_property_get_name (property),
		                        tracker_property_get_name (property));

	} else if (tracker_property_get_data_type (property) == TRACKER_PROPERTY_TYPE_BOOLEAN) {
		g_string_append_printf (sel_col_sql, ", \"%s\" != 0, \"%s:graph\"",
		                        field_name, field_name);
	} else {
		g_string_append_printf (sel_col_sql, ", \"%s\", \"%s:graph\"",
		                        field_name, field_name);
	}
}

static void
create_decomposed_metadata_property_table (TrackerDBInterface *iface,
                                           TrackerProperty    *property,
                                           const gchar        *service_name,
                                           const gchar       **sql_type_for_single_value,
                                           gboolean            in_update,
                                           gboolean            in_change)
{
	const char *field_name;
	const char *sql_type;
	gboolean    transient;

	field_name = tracker_property_get_name (property);

	transient = !sql_type_for_single_value;

	if (!transient) {
		transient = tracker_property_get_transient (property);
	}

	switch (tracker_property_get_data_type (property)) {
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

	if (!in_update || (in_update && (tracker_property_get_is_new (property) ||
	                                 tracker_property_get_is_new_domain_index (property) ||
	                                 tracker_property_get_db_schema_changed (property)))) {
		if (transient || tracker_property_get_multiple_values (property)) {
			GString *sql;
			GString *in_col_sql = NULL;
			GString *sel_col_sql = NULL;
			GError *error = NULL;

			/* multiple values */

			if (in_update) {
				g_debug ("Altering database for class '%s' property '%s': multi value",
				         service_name, field_name);
			}

			if (in_change && !tracker_property_get_is_new (property)) {
				g_debug ("Drop index: DROP INDEX IF EXISTS \"%s_%s_ID\"\nRename: ALTER TABLE \"%s_%s\" RENAME TO \"%s_%s_TEMP\"",
				         service_name, field_name, service_name, field_name,
				         service_name, field_name);

				tracker_db_interface_execute_query (iface, NULL,
				                                    "DROP INDEX IF EXISTS \"%s_%s_ID\"",
				                                    service_name,
				                                    field_name);


				tracker_db_interface_execute_query (iface, &error,
				                                    "ALTER TABLE \"%s_%s\" RENAME TO \"%s_%s_TEMP\"",
				                                    service_name, field_name, service_name, field_name);
				if (error) {
					g_critical ("Ontology change failed while renaming SQL table '%s'", error->message);
					g_clear_error (&error);
				}
			}

			sql = g_string_new ("");
			g_string_append_printf (sql, "CREATE %sTABLE \"%s_%s\" ("
			                             "ID INTEGER NOT NULL, "
			                             "\"%s\" %s NOT NULL, "
			                             "\"%s:graph\" INTEGER",
			                             transient ? "TEMPORARY " : "",
			                             service_name,
			                             field_name,
			                             field_name,
			                             sql_type,
			                             field_name);

			if (in_change && !tracker_property_get_is_new (property)) {
				in_col_sql = g_string_new ("ID");
				sel_col_sql = g_string_new ("ID");

				range_change_for (property, in_col_sql, sel_col_sql, field_name);
			}

			if (tracker_property_get_data_type (property) == TRACKER_PROPERTY_TYPE_DATETIME) {
				/* xsd:dateTime is stored in three columns:
				 * universal time, local date, local time of day */
				g_string_append_printf (sql,
				                        ", \"%s:localDate\" INTEGER NOT NULL"
				                        ", \"%s:localTime\" INTEGER NOT NULL",
				                        tracker_property_get_name (property),
				                        tracker_property_get_name (property));
			}

			tracker_db_interface_execute_query (iface, &error,
			                                    "%s)", sql->str);

			if (error) {
				g_critical ("Creating SQL table failed: %s", error->message);
				g_clear_error (&error);
			}

			/* multiple values */
			if (tracker_property_get_indexed (property)) {
				/* use different UNIQUE index for properties whose
				 * value should be indexed to minimize index size */
				set_index_for_multi_value_property (iface, service_name, field_name, TRUE);
			} else {
				set_index_for_multi_value_property (iface, service_name, field_name, FALSE);
				/* we still have to include the property value in
				 * the unique index for proper constraints */
			}

			g_string_free (sql, TRUE);

			if (in_change && !tracker_property_get_is_new (property) && in_col_sql && sel_col_sql) {
				gchar *query;

				query = g_strdup_printf ("INSERT INTO \"%s_%s\"(%s) "
				                         "SELECT %s FROM \"%s_%s_TEMP\"",
				                         service_name, field_name, in_col_sql->str,
				                         sel_col_sql->str, service_name, field_name);

				tracker_db_interface_execute_query (iface, &error, "%s", query);

				if (error) {
					g_critical ("Ontology change failed while merging data of SQL table '%s'", error->message);
					g_clear_error (&error);
				}

				g_free (query);
				tracker_db_interface_execute_query (iface, &error, "DROP TABLE \"%s_%s_TEMP\"",
				                                    service_name, field_name);

				if (error) {
					g_critical ("Ontology change failed while dropping SQL table '%s'", error->message);
					g_clear_error (&error);
				}

			}

			if (sel_col_sql)
				g_string_free (sel_col_sql, TRUE);
			if (in_col_sql)
				g_string_free (in_col_sql, TRUE);

			/* multiple values */
			if (tracker_property_get_indexed (property)) {
				/* use different UNIQUE index for properties whose
				 * value should be indexed to minimize index size */
				set_index_for_multi_value_property (iface, service_name, field_name, TRUE);
			} else {
				set_index_for_multi_value_property (iface, service_name, field_name, FALSE);
				/* we still have to include the property value in
				 * the unique index for proper constraints */
			}

		} else if (sql_type_for_single_value) {
			*sql_type_for_single_value = sql_type;
		}
	}



}

static gboolean
is_a_domain_index (TrackerProperty **domain_indexes, TrackerProperty *property)
{
	while (*domain_indexes) {

		if (*domain_indexes == property) {
			return TRUE;
		}

		domain_indexes++;
	}

	return FALSE;
}

static void
copy_from_domain_to_domain_index (TrackerDBInterface *iface,
                                  TrackerProperty    *domain_index,
                                  const gchar        *column_name,
                                  const gchar        *column_suffix,
                                  TrackerClass       *dest_domain)
{
	GError *error = NULL;
	TrackerClass *source_domain;
	const gchar *source_name, *dest_name;
	gchar *query;

	source_domain = tracker_property_get_domain (domain_index);
	source_name = tracker_class_get_name (source_domain);
	dest_name = tracker_class_get_name (dest_domain);

	query = g_strdup_printf ("UPDATE \"%s\" SET \"%s%s\"=("
	                         "SELECT \"%s%s\" FROM \"%s\" "
	                         "WHERE \"%s\".ID = \"%s\".ID)",
	                         dest_name,
	                         column_name,
	                         column_suffix ? column_suffix : "",
	                         column_name,
	                         column_suffix ? column_suffix : "",
	                         source_name,
	                         source_name,
	                         dest_name);

	g_debug ("Copying: '%s'", query);

	tracker_db_interface_execute_query (iface, &error, "%s", query);

	if (error) {
		g_critical ("Ontology change failed while altering SQL table '%s'", error->message);
		g_clear_error (&error);
	}

	g_free (query);
}

typedef struct {
	TrackerProperty *prop;
	const gchar *field_name;
	const gchar *suffix;
} ScheduleCopy;

static void
schedule_copy (GPtrArray *schedule,
               TrackerProperty *prop,
               const gchar *field_name,
               const gchar *suffix)
{
	ScheduleCopy *sched = g_new0 (ScheduleCopy, 1);
	sched->prop = prop;
	sched->field_name = field_name,
	sched->suffix = suffix;
	g_ptr_array_add (schedule, sched);
}

static void
copy_from_domain_to_domain_index (TrackerDBInterface *iface,
                                  TrackerProperty    *domain_index,
                                  const gchar        *column_name,
                                  const gchar        *column_suffix,
                                  TrackerClass       *dest_domain)
{
	GError *error = NULL;
	TrackerClass *source_domain;
	const gchar *source_name, *dest_name;
	gchar *query;

	source_domain = tracker_property_get_domain (domain_index);
	source_name = tracker_class_get_name (source_domain);
	dest_name = tracker_class_get_name (dest_domain);

	query = g_strdup_printf ("UPDATE \"%s\" SET \"%s%s\"=("
	                         "SELECT \"%s%s\" FROM \"%s\" "
	                         "WHERE \"%s\".ID = \"%s\".ID)",
	                         dest_name,
	                         column_name,
	                         column_suffix ? column_suffix : "",
	                         column_name,
	                         column_suffix ? column_suffix : "",
	                         source_name,
	                         source_name,
	                         dest_name);

	tracker_db_interface_execute_query (iface, &error, "%s", query);

	if (error) {
		g_critical ("Ontology change failed while altering SQL table '%s'", error->message);
		g_clear_error (&error);
	}

	g_free (query);
}

static void
create_decomposed_metadata_tables (TrackerDBInterface *iface,
                                   TrackerClass       *service,
                                   gboolean            in_update,
                                   gboolean            in_change)
{
	const char       *service_name;
	GString          *create_sql = NULL;
	GString          *in_col_sql = NULL;
	GString          *sel_col_sql = NULL;
	TrackerProperty **properties, *property, **domain_indexes;
	GSList           *class_properties, *field_it;
	gboolean          main_class;
	gint              i, n_props;
	gboolean          in_alter = in_update;
	GError           *error = NULL;
	GPtrArray        *copy_schedule = NULL;

	g_return_if_fail (TRACKER_IS_CLASS (service));

	service_name = tracker_class_get_name (service);

	g_return_if_fail (service_name != NULL);

	main_class = (strcmp (service_name, "rdfs:Resource") == 0);

	if (g_str_has_prefix (service_name, "xsd:")) {
		/* xsd classes do not derive from rdfs:Resource and do not need separate tables */
		return;
	}

	if (in_change) {
		g_debug ("Rename: ALTER TABLE \"%s\" RENAME TO \"%s_TEMP\"", service_name, service_name);
		tracker_db_interface_execute_query (iface, &error, "ALTER TABLE \"%s\" RENAME TO \"%s_TEMP\"", service_name, service_name);
		in_col_sql = g_string_new ("ID");
		sel_col_sql = g_string_new ("ID");
		if (error) {
			g_critical ("Ontology change failed while renaming SQL table '%s'", error->message);
			g_error_free (error);
		}
	}

	if (in_change || !in_update || (in_update && tracker_class_get_is_new (service))) {
		if (in_update)
			g_debug ("Altering database with new class '%s' (create)", service_name);
		in_alter = FALSE;
		create_sql = g_string_new ("");
		g_string_append_printf (create_sql, "CREATE TABLE \"%s\" (ID INTEGER NOT NULL PRIMARY KEY", service_name);
		if (main_class) {
			tracker_db_interface_execute_query (iface, &error, "CREATE TABLE Resource (ID INTEGER NOT NULL PRIMARY KEY, Uri Text NOT NULL, UNIQUE (Uri))");
			if (error) {
				g_critical ("Failed creating Resource SQL table: %s", error->message);
				g_clear_error (&error);
			}
			g_string_append (create_sql, ", Available INTEGER NOT NULL");
		}
	}

	properties = tracker_ontologies_get_properties (&n_props);
	domain_indexes = tracker_class_get_domain_indexes (service);

	class_properties = NULL;

	for (i = 0; i < n_props; i++) {
		gboolean is_domain_index;

		property = properties[i];
		is_domain_index = is_a_domain_index (domain_indexes, property);

		if (tracker_property_get_domain (property) == service || is_domain_index) {
			gboolean put_change;
			const gchar *sql_type_for_single_value = NULL;
			const gchar *field_name;

			create_decomposed_metadata_property_table (iface, property,
			                                           service_name,
			                                           &sql_type_for_single_value,
			                                           in_update,
			                                           in_change);

			field_name = tracker_property_get_name (property);

			if (sql_type_for_single_value) {
				const gchar *default_value;

				/* single value */

				default_value = tracker_property_get_default_value (property);

				if (in_update) {
					g_debug ("%sAltering database for class '%s' property '%s': single value (%s)",
					         in_alter ? "" : "  ",
					         service_name,
					         field_name,
					         in_alter ? "alter" : "create");
				}

				if (!in_alter) {
					put_change = TRUE;
					class_properties = g_slist_prepend (class_properties, property);

					g_string_append_printf (create_sql, ", \"%s\" %s",
					                        field_name,
					                        sql_type_for_single_value);

					if (!copy_schedule) {
						copy_schedule = g_ptr_array_new_with_free_func (g_free);
					}

					if (is_domain_index && tracker_property_get_is_new_domain_index (property)) {
						schedule_copy (copy_schedule, property, field_name, NULL);
					}

					/* add DEFAULT in case that the ontology specifies a default value,
					   assumes that default values never contain quotes */
					if (default_value != NULL) {
						g_string_append_printf (create_sql, " DEFAULT '%s'", default_value);
					}

					if (tracker_property_get_is_inverse_functional_property (property)) {
						g_string_append (create_sql, " UNIQUE");
					}

					g_string_append_printf (create_sql, ", \"%s:graph\" INTEGER",
					                        field_name);

					if (is_domain_index && tracker_property_get_is_new_domain_index (property)) {
						schedule_copy (copy_schedule, property, field_name, ":graph");
					}

					if (tracker_property_get_data_type (property) == TRACKER_PROPERTY_TYPE_DATETIME) {
						/* xsd:dateTime is stored in three columns:
						 * universal time, local date, local time of day */
						g_string_append_printf (create_sql, ", \"%s:localDate\" INTEGER, \"%s:localTime\" INTEGER",
						                        tracker_property_get_name (property),
						                        tracker_property_get_name (property));

						if (is_domain_index && tracker_property_get_is_new_domain_index (property)) {
							schedule_copy (copy_schedule, property, field_name, ":localTime");
							schedule_copy (copy_schedule, property, field_name, ":localDate");
						}

					}

				} else if ((!is_domain_index && tracker_property_get_is_new (property)) ||
				           (is_domain_index && tracker_property_get_is_new_domain_index (property))) {
					GString *alter_sql = NULL;

					put_change = FALSE;
					class_properties = g_slist_prepend (class_properties, property);

					alter_sql = g_string_new ("ALTER TABLE ");
					g_string_append_printf (alter_sql, "\"%s\" ADD COLUMN \"%s\" %s",
					                        service_name,
					                        field_name,
					                        sql_type_for_single_value);

					/* add DEFAULT in case that the ontology specifies a default value,
					   assumes that default values never contain quotes */
					if (default_value != NULL) {
						g_string_append_printf (alter_sql, " DEFAULT '%s'", default_value);
					}
					if (tracker_property_get_is_inverse_functional_property (property)) {
						g_string_append (alter_sql, " UNIQUE");
					}
					g_debug ("Altering: '%s'", alter_sql->str);
					tracker_db_interface_execute_query (iface, &error, "%s", alter_sql->str);
					if (error) {
						g_critical ("Ontology change failed while altering SQL table '%s'", error->message);
						g_clear_error (&error);
					} else if (is_domain_index) {
						copy_from_domain_to_domain_index (iface, property,
						                                  field_name, NULL,
						                                  service);
						/* This is implicit for all domain-specific-indices */
						set_index_for_single_value_property (iface, service_name,
						                                     field_name, TRUE);
					}

					g_string_free (alter_sql, TRUE);


					alter_sql = g_string_new ("ALTER TABLE ");
					g_string_append_printf (alter_sql, "\"%s\" ADD COLUMN \"%s:graph\" INTEGER",
					                        service_name,
					                        field_name);
					g_debug ("Altering: '%s'", alter_sql->str);
					tracker_db_interface_execute_query (iface, &error, "%s", alter_sql->str);
					if (error) {
						g_critical ("Ontology change failed while altering SQL table '%s'", error->message);
						g_clear_error (&error);
					} else if (is_domain_index) {
						copy_from_domain_to_domain_index (iface, property,
						                                  field_name, ":graph",
						                                  service);
					}

					g_string_free (alter_sql, TRUE);

					if (tracker_property_get_data_type (property) == TRACKER_PROPERTY_TYPE_DATETIME) {
						alter_sql = g_string_new ("ALTER TABLE ");
						g_string_append_printf (alter_sql, "\"%s\" ADD COLUMN \"%s:localDate\" INTEGER",
						                        service_name,
						                        field_name);
						g_debug ("Altering: '%s'", alter_sql->str);
						tracker_db_interface_execute_query (iface, &error, "%s", alter_sql->str);
						if (error) {
							g_critical ("Ontology change failed while altering SQL table '%s'", error->message);
							g_clear_error (&error);
						}	else if (is_domain_index) {
							copy_from_domain_to_domain_index (iface, property,
							                                  field_name, ":localDate",
							                                  service);
						}

						g_string_free (alter_sql, TRUE);


						alter_sql = g_string_new ("ALTER TABLE ");
						g_string_append_printf (alter_sql, "\"%s\" ADD COLUMN \"%s:localTime\" INTEGER",
						                        service_name,
						                        field_name);
						g_debug ("Altering: '%s'", alter_sql->str);
						tracker_db_interface_execute_query (iface, &error, "%s", alter_sql->str);
						if (error) {
							g_critical ("Ontology change failed while altering SQL table '%s'", error->message);
							g_clear_error (&error);
						} else if (is_domain_index) {
							copy_from_domain_to_domain_index (iface, property,
							                                  field_name, ":localTime",
							                                  service);
						}

						g_string_free (alter_sql, TRUE);

					}
				} else {
					put_change = TRUE;
				}

				if (in_change && put_change) {
					range_change_for (property, in_col_sql, sel_col_sql, field_name);
				}
			}
		}
	}

	if (create_sql) {
		g_string_append (create_sql, ")");
		g_debug ("Creating: '%s'", create_sql->str);
		tracker_db_interface_execute_query (iface, NULL, "%s", create_sql->str);
		g_string_free (create_sql, TRUE);
	}

	/* create index for single-valued fields */
	for (field_it = class_properties; field_it != NULL; field_it = field_it->next) {
		TrackerProperty *field, *secondary_index;
		const char *field_name;
		gboolean is_domain_index;

		field = field_it->data;

		/* This is implicit for all domain-specific-indices */
		is_domain_index = is_a_domain_index (domain_indexes, field);

		if (!tracker_property_get_multiple_values (field)
		    && (tracker_property_get_indexed (field) || is_domain_index)) {

			field_name = tracker_property_get_name (field);

			secondary_index = tracker_property_get_secondary_index (field);
			if (secondary_index == NULL) {
				set_index_for_single_value_property (iface, service_name, field_name, TRUE);
			} else {
				set_secondary_index_for_single_value_property (iface, service_name, field_name,
				                                               tracker_property_get_name (secondary_index),
				                                               TRUE);
			}
		}
	}

	g_slist_free (class_properties);

	if (in_change && sel_col_sql && in_col_sql) {
		gchar *query;
		GError *error = NULL;

		query = g_strdup_printf ("INSERT INTO \"%s\"(%s) "
		                         "SELECT %s FROM \"%s_TEMP\"",
		                         service_name, in_col_sql->str,
		                         sel_col_sql->str, service_name);

		g_debug ("Copy: %s", query);

		tracker_db_interface_execute_query (iface, &error, "%s", query);
		if (error) {
			g_critical ("Ontology change failed while merging SQL table data '%s'", error->message);
			g_clear_error (&error);
		}
		g_free (query);
		g_debug ("Rename (drop): DROP TABLE \"%s_TEMP\"", service_name);
		tracker_db_interface_execute_query (iface, &error, "DROP TABLE \"%s_TEMP\"", service_name);
		if (error) {
			g_critical ("Ontology change failed while dropping SQL table '%s'", error->message);
			g_error_free (error);
		}
	}

	if (in_col_sql)
		g_string_free (in_col_sql, TRUE);
	if (sel_col_sql)
		g_string_free (sel_col_sql, TRUE);

	if (copy_schedule) {
		guint i;
		for (i = 0; i < copy_schedule->len; i++) {
			ScheduleCopy *sched = g_ptr_array_index (copy_schedule, i);
			copy_from_domain_to_domain_index (iface, sched->prop,
			                                  sched->field_name, sched->suffix,
			                                  service);
		}
		g_ptr_array_free (copy_schedule, TRUE);
	}
}

static void
create_decomposed_transient_metadata_tables (TrackerDBInterface *iface)
{
	TrackerProperty **properties;
	TrackerProperty *property;
	gint i, n_props;

	properties = tracker_ontologies_get_properties (&n_props);

	for (i = 0; i < n_props; i++) {
		property = properties[i];

		if (tracker_property_get_transient (property)) {

			TrackerClass *domain;
			const gchar *service_name;

			domain = tracker_property_get_domain (property);
			service_name = tracker_class_get_name (domain);

			/* create the TEMPORARY table */
			create_decomposed_metadata_property_table (iface, property,
			                                           service_name,
			                                           NULL, FALSE,
			                                           FALSE);
		}
	}
}

void
tracker_data_ontology_import_finished (void)
{
	TrackerClass **classes;
	TrackerProperty **properties;
	gint i, n_props, n_classes;

	classes = tracker_ontologies_get_classes (&n_classes);
	properties = tracker_ontologies_get_properties (&n_props);

	for (i = 0; i < n_classes; i++) {
		tracker_class_set_is_new (classes[i], FALSE);
		tracker_class_set_db_schema_changed (classes[i], FALSE);
	}

	for (i = 0; i < n_props; i++) {
		tracker_property_set_is_new_domain_index (properties[i], FALSE);
		tracker_property_set_is_new (properties[i], FALSE);
		tracker_property_set_db_schema_changed (properties[i], FALSE);
	}
}

void
tracker_data_ontology_import_into_db (gboolean in_update)
{
	TrackerDBInterface *iface;

	TrackerClass **classes;
	TrackerProperty **properties;
	gint i, n_props, n_classes;

	iface = tracker_db_manager_get_db_interface ();

	classes = tracker_ontologies_get_classes (&n_classes);
	properties = tracker_ontologies_get_properties (&n_props);

	/* create tables */
	for (i = 0; i < n_classes; i++) {
		/* Also !is_new classes are processed, they might have new properties */
		create_decomposed_metadata_tables (iface, classes[i], in_update,
		                                   tracker_class_get_db_schema_changed (classes[i]));
	}

	/* insert classes into rdfs:Resource table */
	for (i = 0; i < n_classes; i++) {
		if (tracker_class_get_is_new (classes[i]) == in_update) {
			insert_uri_in_resource_table (iface, tracker_class_get_uri (classes[i]),
			                              tracker_class_get_id (classes[i]));
		}
	}

	/* insert properties into rdfs:Resource table */
	for (i = 0; i < n_props; i++) {
		if (tracker_property_get_is_new (properties[i]) == in_update) {
			insert_uri_in_resource_table (iface, tracker_property_get_uri (properties[i]),
			                              tracker_property_get_id (properties[i]));
		}
	}
}

static GList*
get_ontologies (gboolean     test_schema,
                const gchar *ontologies_dir)
{
	GList *sorted = NULL;

	if (test_schema) {
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

	return sorted;
}


static gint
get_new_service_id (TrackerDBInterface *iface)
{
	TrackerDBCursor    *cursor = NULL;
	TrackerDBStatement *stmt;
	gint max_service_id = 0;
	GError *error = NULL;

	/* Don't intermix this thing with tracker_data_update_get_new_service_id,
	 * if you use this, know what you are doing! */

	iface = tracker_db_manager_get_db_interface ();

	stmt = tracker_db_interface_create_statement (iface, &error,
	                                              "SELECT MAX(ID) AS A FROM Resource");

	if (stmt) {
		cursor = tracker_db_statement_start_cursor (stmt, &error);
		g_object_unref (stmt);
	}

	if (cursor) {
		if (tracker_db_cursor_iter_next (cursor, &error)) {
			max_service_id = tracker_db_cursor_get_int (cursor, 0);
		}
		g_object_unref (cursor);
	}

	if (error) {
		g_error ("Unable to get max ID, aborting: %s", error->message);
	}

	return ++max_service_id;
}

gboolean
tracker_data_manager_init (TrackerDBManagerFlags  flags,
                           const gchar          **test_schemas,
                           gboolean              *first_time,
                           gboolean               journal_check,
                           TrackerBusyCallback    busy_callback,
                           gpointer               busy_user_data,
                           const gchar           *busy_status)
{
	TrackerDBInterface *iface;
	gboolean is_first_time_index, read_journal, check_ontology;
	TrackerDBCursor *cursor;
	TrackerDBStatement *stmt;
	GHashTable *ontos_table;
	GList *sorted = NULL, *l;
	const gchar *env_path;
	gint max_id = 0;

	tracker_data_update_init ();

	/* First set defaults for return values */
	if (first_time) {
		*first_time = FALSE;
	}

	if (initialized) {
		return TRUE;
	}

	/* Make sure we initialize all other modules we depend on */
	tracker_ontologies_init ();

	read_journal = FALSE;

	if (!tracker_db_manager_init (flags, &is_first_time_index, TRUE)) {
		return FALSE;
	}

#if HAVE_TRACKER_FTS
	tracker_fts_set_map_function (tracker_ontologies_get_uri_by_id);
#endif

	if (first_time != NULL) {
		*first_time = is_first_time_index;
	}

	iface = tracker_db_manager_get_db_interface ();

	if (journal_check && is_first_time_index) {
		if (tracker_db_journal_reader_init (NULL)) {
			if (tracker_db_journal_reader_next (NULL)) {
				/* journal with at least one valid transaction
				   is required to trigger journal replay */
				read_journal = TRUE;
			} else {
				tracker_db_journal_reader_shutdown ();
			}
		}
	}

	env_path = g_getenv ("TRACKER_DB_ONTOLOGIES_DIR");

	if (G_LIKELY (!env_path)) {
		ontologies_dir = g_build_filename (SHAREDIR,
		                                   "tracker",
		                                   "ontologies",
		                                   NULL);
	} else {
		ontologies_dir = g_strdup (env_path);
	}

	if (read_journal) {
		GHashTable *classes = NULL, *properties = NULL;
		GHashTable *id_uri_map = NULL;

		in_journal_replay = TRUE;

		/* Load ontology from journal into memory and cache ID v. uri mappings */
		load_ontology_from_journal (&classes, &properties, &id_uri_map);

		/* Read first ontology and commit it into the DB */
		tracker_data_begin_db_transaction_for_replay (tracker_db_journal_reader_get_time ());

		/* This is a no-op when FTS is disabled */
		tracker_db_interface_sqlite_fts_init (iface, TRUE);

		tracker_data_ontology_import_into_db (FALSE);
		tracker_data_commit_db_transaction ();
		tracker_db_journal_reader_shutdown ();

		/* Start replay. Ontology changes might happen during replay of the journal. */

		tracker_data_replay_journal (classes, properties, id_uri_map,
		                             busy_callback, busy_user_data, busy_status);

		in_journal_replay = FALSE;

		/* open journal for writing */
		tracker_db_journal_init (NULL, FALSE);

		check_ontology = TRUE;

		g_hash_table_unref (classes);
		g_hash_table_unref (properties);
		g_hash_table_unref (id_uri_map);

	} else if (is_first_time_index) {
		sorted = get_ontologies (test_schemas != NULL, ontologies_dir);

		/* Truncate journal as it does not even contain a single valid transaction
		 * or is explicitly ignored (journal_check == FALSE, only for test cases) */
		tracker_db_journal_init (NULL, TRUE);

		/* load ontology from files into memory (max_id starts at zero: first-time) */

		for (l = sorted; l; l = l->next) {
			gchar *ontology_path;
			g_debug ("Loading ontology %s", (char *) l->data);
			ontology_path = g_build_filename (ontologies_dir, l->data, NULL);
			load_ontology_file_from_path (ontology_path, &max_id, FALSE, NULL, NULL);
			g_free (ontology_path);
		}

		if (test_schemas) {
			guint p;
			for (p = 0; test_schemas[p] != NULL; p++) {
				gchar *test_schema_path;
				test_schema_path = g_strconcat (test_schemas[p], ".ontology", NULL);

				g_debug ("Loading ontology:'%s' (TEST ONTOLOGY)", test_schema_path);

				load_ontology_file_from_path (test_schema_path, &max_id, FALSE, NULL, NULL);
				g_free (test_schema_path);
			}
		}

		tracker_data_begin_db_transaction ();

		/* Not an ontology transaction: this is the first ontology */
		tracker_db_journal_start_transaction (time (NULL));

		/* This is a no-op when FTS is disabled */
		tracker_db_interface_sqlite_fts_init (iface, TRUE);

		tracker_data_ontology_import_into_db (FALSE);

		/* store ontology in database */
		for (l = sorted; l; l = l->next) {
			gchar *ontology_path = g_build_filename (ontologies_dir, l->data, NULL);
			import_ontology_path (ontology_path, FALSE, !journal_check);
			g_free (ontology_path);
		}

		if (test_schemas) {
			guint p;
			for (p = 0; test_schemas[p] != NULL; p++) {
				gchar *test_schema_path;

				test_schema_path = g_strconcat (test_schemas[p], ".ontology", NULL);
				import_ontology_path (test_schema_path, FALSE, TRUE);
				g_free (test_schema_path);
			}
		}

		tracker_db_journal_commit_db_transaction ();
		tracker_data_commit_db_transaction ();

		g_list_foreach (sorted, (GFunc) g_free, NULL);
		g_list_free (sorted);
		sorted = NULL;

		/* First time, no need to check ontology */
		check_ontology = FALSE;
	} else {
		tracker_db_journal_init (NULL, FALSE);

		/* Load ontology from database into memory */
		db_get_static_data (iface);
		create_decomposed_transient_metadata_tables (iface);
		check_ontology = TRUE;

		/* This is a no-op when FTS is disabled */
		tracker_db_interface_sqlite_fts_init (iface, FALSE);
	}

	if (check_ontology) {
		GList *to_reload = NULL;
		GList *ontos = NULL;
		guint p;
		GPtrArray *seen_classes;
		GPtrArray *seen_properties;
		GError *error = NULL;

		seen_classes = g_ptr_array_new ();
		seen_properties = g_ptr_array_new ();

		/* Get all the ontology files from ontologies_dir */
		sorted = get_ontologies (test_schemas != NULL, ontologies_dir);

		for (l = sorted; l; l = l->next) {
			gchar *ontology_path;
			ontology_path = g_build_filename (ontologies_dir, l->data, NULL);
			ontos = g_list_append (ontos, ontology_path);
		}

		g_list_foreach (sorted, (GFunc) g_free, NULL);
		g_list_free (sorted);

		if (test_schemas) {
			for (p = 0; test_schemas[p] != NULL; p++) {
				gchar *test_schema_path;
				test_schema_path = g_strconcat (test_schemas[p], ".ontology", NULL);
				ontos = g_list_append (ontos, test_schema_path);
			}
		}

		/* check ontology against database */

		tracker_data_begin_db_transaction ();

		/* This _is_ an ontology transaction, it represents a change to the
		 * ontology. We mark it up as such in the journal, so that replay_journal
		 * can recognize it and deal with it properly. */

		tracker_db_journal_start_ontology_transaction (time (NULL));

		/* Get a map of tracker:Ontology v. nao:lastModified so that we can test
		 * for all the ontology files in ontologies_dir whether the last-modified
		 * has changed since we dealt with the file last time. */

		stmt = tracker_db_interface_create_statement (iface, &error,
		        "SELECT Resource.Uri, \"rdfs:Resource\".\"nao:lastModified\" FROM \"tracker:Ontology\""
		        "INNER JOIN Resource ON Resource.ID = \"tracker:Ontology\".ID "
		        "INNER JOIN \"rdfs:Resource\" ON \"tracker:Ontology\".ID = \"rdfs:Resource\".ID");

		if (stmt) {
			cursor = tracker_db_statement_start_cursor (stmt, &error);
			g_object_unref (stmt);
		} else {
			cursor = NULL;
		}

		ontos_table = g_hash_table_new_full (g_str_hash,
		                                     g_str_equal,
		                                     g_free,
		                                     NULL);

		if (cursor) {
			while (tracker_db_cursor_iter_next (cursor, &error)) {
				const gchar *onto_uri = tracker_db_cursor_get_string (cursor, 0, NULL);
				/* It's stored as an int in the db anyway. This is caused by
				 * string_to_gvalue in tracker-data-update.c */
				gint value = tracker_db_cursor_get_int (cursor, 1);

				g_hash_table_insert (ontos_table, g_strdup (onto_uri),
				                     GINT_TO_POINTER (value));
			}

			g_object_unref (cursor);
		}

		if (error) {
			g_warning ("%s", error->message);
			g_clear_error (&error);
		}

		for (l = ontos; l; l = l->next) {
			TrackerOntology *ontology;
			const gchar *ontology_path = l->data;
			const gchar *ontology_uri;
			gboolean found, update_nao = FALSE;
			gpointer value;
			gint last_mod;

			/* Parse a TrackerOntology from ontology_file */
			ontology = get_ontology_from_path (ontology_path);

			if (!ontology) {
				/* TODO: cope with full custom .ontology files: deal with this
				 * error gracefully. App devs might install wrong ontology files
				 * and we shouldn't critical() due to this. */
				g_critical ("Can't get ontology from file: %s", ontology_path);
				continue;
			}

			ontology_uri = tracker_ontology_get_uri (ontology);
			/* We can't do better than this cast, it's stored as an int in the
			 * db. See above comment for more info. */
			last_mod = (gint) tracker_ontology_get_last_modified (ontology);

			found = g_hash_table_lookup_extended (ontos_table,
			                                      ontology_uri,
			                                      NULL, &value);

			if (found) {
				gint val = GPOINTER_TO_INT (value);
				/* When the last-modified in our database isn't the same as the last
				 * modified in the latest version of the file, deal with changes. */
				if (val != last_mod) {
					g_debug ("Ontology file '%s' needs update", ontology_path);
					if (max_id == 0) {
						/* In case of first-time, this wont start at zero */
						max_id = get_new_service_id (iface);
					}
					/* load ontology from files into memory, set all new's
					 * is_new to TRUE */
					load_ontology_file_from_path (ontology_path, &max_id, TRUE,
					                              seen_classes, seen_properties);
					to_reload = g_list_prepend (to_reload, l->data);
					update_nao = TRUE;
				}
			} else {
				g_debug ("Ontology file '%s' got added", ontology_path);
				if (max_id == 0) {
					/* In case of first-time, this wont start at zero */
					max_id = get_new_service_id (iface);
				}
				/* load ontology from files into memory, set all new's
				 * is_new to TRUE */
				load_ontology_file_from_path (ontology_path, &max_id, TRUE,
				                              seen_classes, seen_properties);
				to_reload = g_list_prepend (to_reload, l->data);
				update_nao = TRUE;
			}

			if (update_nao) {
				/* Update the nao:lastModified in the database */
				stmt = tracker_db_interface_create_statement (iface, &error,
				        "UPDATE \"rdfs:Resource\" SET \"nao:lastModified\"= ? "
				        "WHERE \"rdfs:Resource\".ID = "
				        "(SELECT Resource.ID FROM Resource INNER JOIN \"rdfs:Resource\" "
				        "ON \"rdfs:Resource\".ID = Resource.ID WHERE "
				        "Resource.Uri = ?)");

				if (stmt) {
					tracker_db_statement_bind_int (stmt, 0, last_mod);
					tracker_db_statement_bind_text (stmt, 1, ontology_uri);
					tracker_db_statement_execute (stmt, &error);
					g_object_unref (stmt);
				}

				if (error) {
					g_critical ("%s", error->message);
					g_clear_error (&error);
				}
			}

			g_object_unref (ontology);
		}

		if (to_reload) {
			tracker_data_ontology_process_changes_pre_db (seen_classes, seen_properties);

			/* Perform ALTER-TABLE and CREATE-TABLE calls for all that are is_new */
			tracker_data_ontology_import_into_db (TRUE);

			tracker_data_ontology_process_changes_post_db (seen_classes, seen_properties);

			for (l = to_reload; l; l = l->next) {
				const gchar *ontology_path = l->data;
				/* store ontology in database */
				import_ontology_path (ontology_path, TRUE, !journal_check);
			}
			g_list_free (to_reload);

			tracker_data_ontology_process_changes_post_import (seen_classes, seen_properties);
		}


		tracker_data_ontology_free_seen (seen_classes);
		tracker_data_ontology_free_seen (seen_properties);

		/* Reset the is_new flag for all classes and properties */
		tracker_data_ontology_import_finished ();

		tracker_db_journal_commit_db_transaction ();
		tracker_data_commit_db_transaction ();

		g_hash_table_unref (ontos_table);

		g_list_foreach (ontos, (GFunc) g_free, NULL);
		g_list_free (ontos);
	}

	initialized = TRUE;

	g_free (ontologies_dir);

	return TRUE;
}


void
tracker_data_manager_shutdown (void)
{
	g_return_if_fail (initialized == TRUE);

	/* Make sure we shutdown all other modules we depend on */
	tracker_db_journal_shutdown ();
	tracker_db_manager_shutdown ();
	tracker_ontologies_shutdown ();
	tracker_data_update_shutdown ();

	initialized = FALSE;
}

gint64
tracker_data_manager_get_db_option_int64 (const gchar *option)
{
	TrackerDBInterface *iface;
	TrackerDBStatement *stmt;
	TrackerDBResultSet *result_set = NULL;
	gchar              *str;
	gint                value = 0;
	GError             *error = NULL;

	g_return_val_if_fail (option != NULL, 0);

	iface = tracker_db_manager_get_db_interface ();

	stmt = tracker_db_interface_create_statement (iface, &error, "SELECT OptionValue FROM Options WHERE OptionKey = ?");

	if (stmt) {
		tracker_db_statement_bind_text (stmt, 0, option);
		result_set = tracker_db_statement_execute (stmt, &error);
		g_object_unref (stmt);
	}

	if (result_set) {
		tracker_db_result_set_get (result_set, 0, &str, -1);

		if (str) {
			value = g_ascii_strtoull (str, NULL, 10);
			g_free (str);
		}

		g_object_unref (result_set);
	}

	if (error) {
		g_warning ("%s", error->message);
		g_error_free (error);
		return -1;
	}

	return value;
}

void
tracker_data_manager_set_db_option_int64 (const gchar *option,
                                          gint64       value)
{
	TrackerDBInterface *iface;
	TrackerDBStatement *stmt;
	gchar              *str;
	GError             *error = NULL;

	g_return_if_fail (option != NULL);

	iface = tracker_db_manager_get_db_interface ();

	stmt = tracker_db_interface_create_statement (iface, &error, "REPLACE INTO Options (OptionKey, OptionValue) VALUES (?,?)");

	if (stmt) {
		tracker_db_statement_bind_text (stmt, 0, option);

		str = g_strdup_printf ("%"G_GINT64_FORMAT, value);
		tracker_db_statement_bind_text (stmt, 1, str);
		g_free (str);

		tracker_db_statement_execute (stmt, &error);
		g_object_unref (stmt);
	}

	if (error) {
		g_warning ("%s", error->message);
		g_error_free (error);
		return;
	}
}

gboolean
tracker_data_manager_interrupt_thread (GThread *thread)
{
	return tracker_db_manager_interrupt_thread (thread);
}

void
tracker_data_manager_interrupt_thread_reset (GThread *thread)
{
	tracker_db_manager_interrupt_thread_reset (thread);
}
