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

#ifndef __TRACKER_ONTOLOGIES_DIFF_H__
#define __TRACKER_ONTOLOGIES_DIFF_H__

#include "tracker-ontologies.h"

typedef enum
{
	TRACKER_CHANGE_CLASS_NEW,
	TRACKER_CHANGE_CLASS_DELETE,
	TRACKER_CHANGE_CLASS_SUPERCLASS_NEW,
	TRACKER_CHANGE_CLASS_SUPERCLASS_DELETE,
	TRACKER_CHANGE_CLASS_DOMAIN_INDEX_NEW,
	TRACKER_CHANGE_CLASS_DOMAIN_INDEX_DELETE,
	TRACKER_CHANGE_PROPERTY_NEW,
	TRACKER_CHANGE_PROPERTY_DELETE,
	TRACKER_CHANGE_PROPERTY_SUPERPROPERTY_NEW,
	TRACKER_CHANGE_PROPERTY_SUPERPROPERTY_DELETE,
	TRACKER_CHANGE_PROPERTY_INDEX_NEW,
	TRACKER_CHANGE_PROPERTY_INDEX_DELETE,
	TRACKER_CHANGE_PROPERTY_SECONDARY_INDEX_NEW,
	TRACKER_CHANGE_PROPERTY_SECONDARY_INDEX_DELETE,
	TRACKER_CHANGE_PROPERTY_INVERSE_FUNCTIONAL_NEW,
	TRACKER_CHANGE_PROPERTY_INVERSE_FUNCTIONAL_DELETE,
	TRACKER_CHANGE_PROPERTY_FTS_INDEX,
	TRACKER_CHANGE_PROPERTY_RANGE,
	TRACKER_CHANGE_PROPERTY_DOMAIN,
	TRACKER_CHANGE_PROPERTY_CARDINALITY,
} TrackerOntologyChangeType;

typedef struct _TrackerOntologyChange TrackerOntologyChange;

struct _TrackerOntologyChange
{
	TrackerOntologyChangeType type;
	union {
		TrackerClass *class;
		TrackerProperty *property;
		TrackerNamespace *namespace;
		TrackerOntology *ontology;
		struct {
			TrackerClass *class;
			TrackerProperty *property;
		} domain_index;
		struct {
			TrackerProperty *property1;
			TrackerProperty *property2;
		} superproperty;
		struct {
			TrackerClass *class1;
			TrackerClass *class2;
		} superclass;
	} d;
};

TrackerOntologies * tracker_ontologies_diff (TrackerOntologies      *db_ontology,
                                             TrackerOntologies      *current_ontology,
                                             TrackerOntologyChange **changes,
                                             gint                   *n_changes);

#endif /* __TRACKER_ONTOLOGIES_DIFF_H__ */
