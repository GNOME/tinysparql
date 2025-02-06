/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
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

#pragma once

#include <glib-object.h>

#include "tracker-class.h"
#include "tracker-namespace.h"
#include "tracker-ontology.h"
#include "tracker-property.h"

G_BEGIN_DECLS

#define TRACKER_TYPE_ONTOLOGIES         (tracker_ontologies_get_type ())
#define TRACKER_ONTOLOGIES(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_ONTOLOGIES, TrackerOntologies))
#define TRACKER_ONTOLOGIES_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), TRACKER_TYPE_ONTOLOGIES, TrackerOntologiesClass))
#define TRACKER_IS_ONTOLOGIES(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_ONTOLOGIES))
#define TRACKER_IS_ONTOLOGIES_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), TRACKER_TYPE_ONTOLOGIES))
#define TRACKER_ONTOLOGIES_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TRACKER_TYPE_ONTOLOGIES, TrackerOntologiesClass))

typedef struct _TrackerOntologiesClass TrackerOntologiesClass;

struct _TrackerOntologies {
	GObject parent;
};

struct _TrackerOntologiesClass {
	GObjectClass parent_class;
};

TrackerOntologies *tracker_ontologies_new                  (void);

/* Service mechanics */
void               tracker_ontologies_add_class            (TrackerOntologies *ontologies,
                                                            TrackerClass      *service);
TrackerClass *     tracker_ontologies_get_class_by_uri     (TrackerOntologies *ontologies,
                                                            const gchar       *service_uri);
TrackerNamespace **tracker_ontologies_get_namespaces       (TrackerOntologies *ontologies,
                                                            guint             *length);
TrackerClass  **   tracker_ontologies_get_classes          (TrackerOntologies *ontologies,
                                                            guint             *length);
TrackerProperty ** tracker_ontologies_get_properties       (TrackerOntologies *ontologies,
                                                            guint             *length);
TrackerProperty *  tracker_ontologies_get_rdf_type         (TrackerOntologies *ontologies);
TrackerProperty *  tracker_ontologies_get_nrl_added        (TrackerOntologies *ontologies);
TrackerProperty *  tracker_ontologies_get_nrl_modified     (TrackerOntologies *ontologies);

/* Field mechanics */
void               tracker_ontologies_add_property         (TrackerOntologies *ontologies,
                                                            TrackerProperty   *field);
TrackerProperty *  tracker_ontologies_get_property_by_uri  (TrackerOntologies *ontologies,
                                                            const gchar       *uri);
void               tracker_ontologies_add_namespace        (TrackerOntologies *ontologies,
                                                            TrackerNamespace  *namespace_);
void               tracker_ontologies_add_ontology         (TrackerOntologies *ontologies,
                                                            TrackerOntology   *ontology);
TrackerNamespace * tracker_ontologies_get_namespace_by_uri (TrackerOntologies *ontologies,
                                                            const gchar       *namespace_uri);
TrackerOntology  * tracker_ontologies_get_ontology_by_uri  (TrackerOntologies *ontologies,
                                                            const gchar       *namespace_uri);
const gchar*       tracker_ontologies_get_uri_by_id        (TrackerOntologies *ontologies,
                                                            TrackerRowid       id);
void               tracker_ontologies_add_id_uri_pair      (TrackerOntologies *ontologies,
                                                            TrackerRowid       id,
                                                            const gchar       *uri);

G_END_DECLS
