/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
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
 * Author: Philip Van Hoof <philip@codeminded.be>
 */

#pragma once

#include <glib-object.h>
#include "tracker-class.h"

G_BEGIN_DECLS

#define TRACKER_TYPE_ONTOLOGY         (tracker_ontology_get_type ())
#define TRACKER_ONTOLOGY(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_ONTOLOGY, TrackerOntology))
#define TRACKER_ONTOLOGY_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), TRACKER_TYPE_ONTOLOGY, TrackerOntologyClass))
#define TRACKER_IS_ONTOLOGY(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_ONTOLOGY))
#define TRACKER_IS_ONTOLOGY_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), TRACKER_TYPE_ONTOLOGY))
#define TRACKER_ONTOLOGY_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TRACKER_TYPE_ONTOLOGY, TrackerOntologyClass))

typedef struct _TrackerOntology TrackerOntology;
typedef struct _TrackerOntologyClass TrackerOntologyClass;

struct _TrackerOntology {
	GObject parent;
};

struct _TrackerOntologyClass {
	GObjectClass parent_class;
};

GType             tracker_ontology_get_type          (void) G_GNUC_CONST;
TrackerOntology  *tracker_ontology_new               (void);
const gchar *     tracker_ontology_get_uri           (TrackerOntology *ontology);

void              tracker_ontology_set_uri           (TrackerOntology *ontology,
                                                      const gchar      *value);
void              tracker_ontology_set_ontologies    (TrackerOntology   *ontology,
                                                      TrackerOntologies *ontologies);

G_END_DECLS

