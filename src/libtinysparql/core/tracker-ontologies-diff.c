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

#include "tracker-ontologies-diff.h"

static gboolean
compare_classes (TrackerClass *a,
                 TrackerClass *b)
{
	if (a == b || (a && b && g_strcmp0 (tracker_class_get_uri (a), tracker_class_get_uri (b)) == 0))
		return TRUE;
	return FALSE;
}

static gboolean
compare_properties (TrackerProperty *a,
                    TrackerProperty *b)
{
	if (a == b || (a && b && g_strcmp0 (tracker_property_get_uri (a), tracker_property_get_uri (b)) == 0))
		return TRUE;
	return FALSE;
}

static gboolean
array_contains (gconstpointer *array,
                gconstpointer  elem,
                GEqualFunc     func)
{
	while (array && *array) {
		if (func (*array, elem))
			return TRUE;
		array++;
	}

	return FALSE;
}

static void
add_property_change (TrackerOntologyChangeType  type,
                     TrackerProperty           *property,
                     GArray                    *array)
{
	TrackerOntologyChange change;

	g_assert (type == TRACKER_CHANGE_PROPERTY_NEW ||
	          type == TRACKER_CHANGE_PROPERTY_DELETE ||
	          type == TRACKER_CHANGE_PROPERTY_INDEX_NEW ||
	          type == TRACKER_CHANGE_PROPERTY_INDEX_DELETE ||
	          type == TRACKER_CHANGE_PROPERTY_SECONDARY_INDEX_NEW ||
	          type == TRACKER_CHANGE_PROPERTY_SECONDARY_INDEX_DELETE ||
	          type == TRACKER_CHANGE_PROPERTY_INVERSE_FUNCTIONAL_NEW ||
	          type == TRACKER_CHANGE_PROPERTY_INVERSE_FUNCTIONAL_DELETE ||
	          type == TRACKER_CHANGE_PROPERTY_FTS_INDEX ||
	          type == TRACKER_CHANGE_PROPERTY_RANGE ||
	          type == TRACKER_CHANGE_PROPERTY_DOMAIN ||
	          type == TRACKER_CHANGE_PROPERTY_CARDINALITY);

	change = (TrackerOntologyChange) {
		.type = type,
		.d.property = property,
	};
	g_array_append_val (array, change);
}

static void
add_class_change (TrackerOntologyChangeType  type,
                  TrackerClass              *class,
                  GArray                    *array)
{
	TrackerOntologyChange change;

	g_assert (type == TRACKER_CHANGE_CLASS_NEW ||
	          type == TRACKER_CHANGE_CLASS_DELETE);

	change = (TrackerOntologyChange) {
		.type = type,
		.d.class = class,
	};
	g_array_append_val (array, change);
}

static void
add_superclass_change (TrackerOntologyChangeType  type,
                       TrackerClass              *class,
                       TrackerClass              *superclass,
                       GArray                    *array)
{
	TrackerOntologyChange change;

	g_assert (type == TRACKER_CHANGE_CLASS_SUPERCLASS_NEW ||
	          type == TRACKER_CHANGE_CLASS_SUPERCLASS_DELETE);

	change = (TrackerOntologyChange) {
		.type = type,
		.d.superclass.class1 = class,
		.d.superclass.class2 = superclass,
	};
	g_array_append_val (array, change);
}

static void
add_superproperty_change (TrackerOntologyChangeType  type,
                          TrackerProperty           *property,
                          TrackerProperty           *superproperty,
                          GArray                    *array)
{
	TrackerOntologyChange change;

	g_assert (type == TRACKER_CHANGE_PROPERTY_SUPERPROPERTY_NEW ||
	          type == TRACKER_CHANGE_PROPERTY_SUPERPROPERTY_DELETE);

	change = (TrackerOntologyChange) {
		.type = type,
		.d.superproperty.property1 = property,
		.d.superproperty.property2 = superproperty,
	};
	g_array_append_val (array, change);
}

static void
add_domain_index_change (TrackerOntologyChangeType  type,
                         TrackerClass              *class,
                         TrackerProperty           *property,
                         GArray                    *array)
{
	TrackerOntologyChange change;

	g_assert (type == TRACKER_CHANGE_CLASS_DOMAIN_INDEX_NEW ||
	          type == TRACKER_CHANGE_CLASS_DOMAIN_INDEX_DELETE);

	change = (TrackerOntologyChange) {
		.type = type,
		.d.domain_index.class = class,
		.d.domain_index.property = property,
	};
	g_array_append_val (array, change);
}

TrackerOntologies *
tracker_ontologies_diff (TrackerOntologies      *db_ontology,
                         TrackerOntologies      *current_ontology,
                         TrackerOntologyChange **changes,
                         gint                   *n_changes)
{
	TrackerClass **classes;
	TrackerProperty **props;
	guint len, i;
	GArray *array;

	g_assert (changes != NULL);
	g_assert (n_changes != NULL);

	array = g_array_new (FALSE, FALSE, sizeof (TrackerOntologyChange));

	if (current_ontology) {
		/* Handle new class additions */
		classes = tracker_ontologies_get_classes (current_ontology, &len);
		for (i = 0; i < len; i++) {
			if (db_ontology &&
			    tracker_ontologies_get_class_by_uri (db_ontology,
			                                         tracker_class_get_uri (classes[i])))
				continue;

			/* Class is is new */
			add_class_change (TRACKER_CHANGE_CLASS_NEW,
			                  classes[i], array);
		}

		/* Handle new property additions */
		props = tracker_ontologies_get_properties (current_ontology, &len);
		for (i = 0; i < len; i++) {
			TrackerProperty **super;

			/* Updated properties are handled elsewhere */
			if (db_ontology &&
			    tracker_ontologies_get_property_by_uri (db_ontology,
			                                            tracker_property_get_uri (props[i])))
				continue;

			/* Property changes are only handled individually if:
			 * - The property is multi-valued
			 * - The class of its domain existed previously
			 */
			if (tracker_property_get_multiple_values (props[i]) ||
			    (db_ontology &&
			     tracker_ontologies_get_class_by_uri (db_ontology,
			                                          tracker_class_get_uri (tracker_property_get_domain (props[i]))))) {
				add_property_change (TRACKER_CHANGE_PROPERTY_NEW,
				                     props[i], array);
			}

			super = tracker_property_get_super_properties (props[i]);
			while (*super) {
				add_superproperty_change (TRACKER_CHANGE_PROPERTY_SUPERPROPERTY_NEW,
				                          props[i], *super, array);
				super++;
			}

			if (tracker_property_get_is_inverse_functional_property (props[i])) {
				add_property_change (TRACKER_CHANGE_PROPERTY_INVERSE_FUNCTIONAL_NEW,
				                     props[i], array);
			}

			if (tracker_property_get_indexed (props[i])) {
				add_property_change (TRACKER_CHANGE_PROPERTY_INDEX_NEW,
				                     props[i], array);
			}

			if (tracker_property_get_secondary_index (props[i])) {
				add_property_change (TRACKER_CHANGE_PROPERTY_SECONDARY_INDEX_NEW,
				                     props[i], array);
			}

			if (tracker_property_get_fulltext_indexed (props[i])) {
				add_property_change (TRACKER_CHANGE_PROPERTY_FTS_INDEX,
				                     props[i], array);
			}
		}

		classes = tracker_ontologies_get_classes (current_ontology, &len);
		for (i = 0; i < len; i++) {
			TrackerProperty **domain_indexes;
			TrackerClass **super;

			if (db_ontology &&
			    tracker_ontologies_get_class_by_uri (db_ontology,
			                                         tracker_class_get_uri (classes[i])))
				continue;

			super = tracker_class_get_super_classes (classes[i]);
			while (*super) {
				add_superclass_change (TRACKER_CHANGE_CLASS_SUPERCLASS_NEW,
				                       classes[i], *super, array);
				super++;
			}

			domain_indexes = tracker_class_get_domain_indexes (classes[i]);
			while (*domain_indexes) {
				add_domain_index_change (TRACKER_CHANGE_CLASS_DOMAIN_INDEX_NEW,
				                         classes[i], *domain_indexes, array);
				domain_indexes++;
			}
		}
	}

	/* Handle changed/deleted changes */
	if (db_ontology) {
		classes = tracker_ontologies_get_classes (db_ontology, &len);
		for (i = 0; i < len; i++) {
			TrackerClass *cur_class = NULL;
			TrackerProperty **domain_indexes;

			if (current_ontology) {
				cur_class = tracker_ontologies_get_class_by_uri (current_ontology,
				                                                 tracker_class_get_uri (classes[i]));
			}

			if (cur_class) {
				TrackerClass **super;

				/* Class exists in both */

				tracker_class_set_id (cur_class, tracker_class_get_id (classes[i]));
				tracker_ontologies_add_id_uri_pair (current_ontology,
				                                    tracker_class_get_id (cur_class),
				                                    tracker_class_get_uri (cur_class));

				super = tracker_class_get_super_classes (classes[i]);
				while (*super) {
					if (!array_contains ((gconstpointer*) tracker_class_get_super_classes (cur_class),
					                     (gconstpointer) *super,
					                     (GEqualFunc) compare_classes)) {
						add_superclass_change (TRACKER_CHANGE_CLASS_SUPERCLASS_DELETE,
						                       cur_class, *super, array);
					}
					super++;
				}

				super = tracker_class_get_super_classes (cur_class);
				while (*super) {
					if (!array_contains ((gconstpointer*) tracker_class_get_super_classes (classes[i]),
					                     (gconstpointer) *super,
					                     (GEqualFunc) compare_classes)) {
						add_superclass_change (TRACKER_CHANGE_CLASS_SUPERCLASS_NEW,
						                       cur_class, *super, array);
					}
					super++;
				}

				domain_indexes = tracker_class_get_domain_indexes (classes[i]);
				while (*domain_indexes) {
					if (!array_contains ((gconstpointer*) tracker_class_get_domain_indexes (cur_class),
					                     (gconstpointer) *domain_indexes,
					                     (GEqualFunc) compare_properties)) {
						add_domain_index_change (TRACKER_CHANGE_CLASS_DOMAIN_INDEX_DELETE,
						                         cur_class, *domain_indexes, array);
					}
					domain_indexes++;
				}

				domain_indexes = tracker_class_get_domain_indexes (cur_class);
				while (*domain_indexes) {
					if (!array_contains ((gconstpointer*) tracker_class_get_domain_indexes (classes[i]),
					                     (gconstpointer) *domain_indexes,
					                     (GEqualFunc) compare_properties)) {
						add_domain_index_change (TRACKER_CHANGE_CLASS_DOMAIN_INDEX_NEW,
						                         cur_class, *domain_indexes, array);
					}
					domain_indexes++;
				}
			}
		}

		props = tracker_ontologies_get_properties (db_ontology, &len);
		for (i = 0; i < len; i++) {
			TrackerProperty *cur_property = NULL;

			if (current_ontology) {
				cur_property = tracker_ontologies_get_property_by_uri (current_ontology,
				                                                       tracker_property_get_uri (props[i]));
			}

			if (cur_property) {
				TrackerProperty **super;
				gboolean cardinality_change;

				/* Property exists in both */
				cardinality_change = tracker_property_get_multiple_values (props[i]) !=
					tracker_property_get_multiple_values (cur_property);

				tracker_property_set_id (cur_property, tracker_property_get_id (props[i]));
				tracker_ontologies_add_id_uri_pair (current_ontology,
				                                    tracker_property_get_id (cur_property),
				                                    tracker_property_get_uri (cur_property));

				super = tracker_property_get_super_properties (props[i]);
				while (*super) {
					if (!array_contains ((gconstpointer*) tracker_property_get_super_properties (cur_property),
					                     (gconstpointer) *super,
					                     (GEqualFunc) compare_properties)) {
						add_superproperty_change (TRACKER_CHANGE_PROPERTY_SUPERPROPERTY_DELETE,
						                          cur_property, *super, array);
					}
					super++;
				}

				if (tracker_property_get_indexed (props[i]) &&
				    (cardinality_change || !tracker_property_get_indexed (cur_property))) {
					add_property_change (TRACKER_CHANGE_PROPERTY_INDEX_DELETE,
					                     cur_property, array);
				}

				if (tracker_property_get_secondary_index (props[i]) &&
				    (cardinality_change ||
				     !compare_properties (tracker_property_get_secondary_index (props[i]),
				                          tracker_property_get_secondary_index (cur_property)))) {
					add_property_change (TRACKER_CHANGE_PROPERTY_SECONDARY_INDEX_DELETE,
					                     props[i], array);
				}

				if (tracker_property_get_is_inverse_functional_property (props[i]) &&
				    (cardinality_change || !tracker_property_get_is_inverse_functional_property (cur_property))) {
					add_property_change (TRACKER_CHANGE_PROPERTY_INVERSE_FUNCTIONAL_DELETE,
					                     cur_property, array);
				}

				if ((tracker_property_get_fulltext_indexed (cur_property) !=
				     tracker_property_get_fulltext_indexed (props[i])) ||
				    (cardinality_change && tracker_property_get_fulltext_indexed (cur_property))) {
					add_property_change (TRACKER_CHANGE_PROPERTY_FTS_INDEX,
					                     cur_property, array);
				}

				if (!compare_classes (tracker_property_get_range (props[i]),
				                      tracker_property_get_range (cur_property))) {
					add_property_change (TRACKER_CHANGE_PROPERTY_RANGE,
					                     cur_property, array);
				}

				if (!compare_classes (tracker_property_get_domain (props[i]),
				                      tracker_property_get_domain (cur_property))) {
					add_property_change (TRACKER_CHANGE_PROPERTY_DOMAIN,
					                     cur_property, array);
				}

				if (cardinality_change) {
					add_property_change (TRACKER_CHANGE_PROPERTY_CARDINALITY,
					                     cur_property, array);
				}

				if (tracker_property_get_is_inverse_functional_property (cur_property) &&
				    (cardinality_change || !tracker_property_get_is_inverse_functional_property (props[i]))) {
					add_property_change (TRACKER_CHANGE_PROPERTY_INVERSE_FUNCTIONAL_NEW,
					                     cur_property, array);
				}

				if (tracker_property_get_secondary_index (cur_property) &&
				    (cardinality_change ||
				     !compare_properties (tracker_property_get_secondary_index (props[i]),
				                          tracker_property_get_secondary_index (cur_property)))) {
					add_property_change (TRACKER_CHANGE_PROPERTY_SECONDARY_INDEX_NEW,
					                     cur_property, array);
				}

				if (tracker_property_get_indexed (cur_property) &&
				    (cardinality_change || !tracker_property_get_indexed (props[i]))) {
					add_property_change (TRACKER_CHANGE_PROPERTY_INDEX_NEW,
					                     cur_property, array);
				}

				super = tracker_property_get_super_properties (cur_property);
				while (*super) {
					if (!array_contains ((gconstpointer*) tracker_property_get_super_properties (props[i]),
					                     (gconstpointer) *super,
					                     (GEqualFunc) compare_properties)) {
						add_superproperty_change (TRACKER_CHANGE_PROPERTY_SUPERPROPERTY_NEW,
						                          cur_property, *super, array);
					}
					super++;
				}
			} else {
				TrackerProperty **super;

				/* Property is deleted */
				if (tracker_property_get_fulltext_indexed (props[i])) {
					add_property_change (TRACKER_CHANGE_PROPERTY_FTS_INDEX,
					                     props[i], array);
				}

				if (tracker_property_get_secondary_index (props[i])) {
					add_property_change (TRACKER_CHANGE_PROPERTY_SECONDARY_INDEX_DELETE,
					                     props[i], array);
				}

				if (tracker_property_get_indexed (props[i])) {
					add_property_change (TRACKER_CHANGE_PROPERTY_INDEX_DELETE,
					                     props[i], array);
				}

				if (tracker_property_get_is_inverse_functional_property (props[i])) {
					add_property_change (TRACKER_CHANGE_PROPERTY_INVERSE_FUNCTIONAL_DELETE,
					                     props[i], array);
				}

				super = tracker_property_get_super_properties (props[i]);
				while (*super) {
					add_superproperty_change (TRACKER_CHANGE_PROPERTY_SUPERPROPERTY_DELETE,
					                          props[i], *super, array);
					super++;
				}

				add_property_change (TRACKER_CHANGE_PROPERTY_DELETE,
				                     props[i], array);
			}
		}

		classes = tracker_ontologies_get_classes (db_ontology, &len);
		for (i = 0; i < len; i++) {
			TrackerClass *cur_class = NULL;

			if (current_ontology) {
				cur_class = tracker_ontologies_get_class_by_uri (current_ontology,
				                                                 tracker_class_get_uri (classes[i]));
			}

			if (!cur_class) {
				TrackerProperty **domain_indexes;
				TrackerClass **super;

				/* Class is deleted */
				domain_indexes = tracker_class_get_domain_indexes (classes[i]);
				while (*domain_indexes) {
					add_domain_index_change (TRACKER_CHANGE_CLASS_DOMAIN_INDEX_DELETE,
					                         classes[i], *domain_indexes, array);
					domain_indexes++;
				}

				super = tracker_class_get_super_classes (classes[i]);
				while (*super) {
					add_superclass_change (TRACKER_CHANGE_CLASS_SUPERCLASS_DELETE,
					                       cur_class, *super, array);
					super++;
				}

				add_class_change (TRACKER_CHANGE_CLASS_DELETE,
				                  classes[i], array);
			}
		}
	}

	*n_changes = array->len;
	*changes = (TrackerOntologyChange*) g_array_free (array, FALSE);

	return current_ontology;
}
