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

#ifndef __LIBTRACKER_DATA_ONTOLOGIES_H__
#define __LIBTRACKER_DATA_ONTOLOGIES_H__

#include <glib-object.h>

#include "tracker-class.h"
#include "tracker-namespace.h"
#include "tracker-ontology.h"
#include "tracker-property.h"

G_BEGIN_DECLS

#if !defined (__LIBTRACKER_DATA_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "only <libtracker-data/tracker-data.h> must be included directly."
#endif

#define TRACKER_ONTOLOGIES_MAX_ID 100000

void               tracker_ontologies_init                 (void);
void               tracker_ontologies_shutdown             (void);

/* Service mechanics */
void               tracker_ontologies_add_class            (TrackerClass     *service);
TrackerClass *     tracker_ontologies_get_class_by_uri     (const gchar      *service_uri);
TrackerNamespace **tracker_ontologies_get_namespaces       (guint *length);
TrackerOntology  **tracker_ontologies_get_ontologies       (guint *length);
TrackerClass  **   tracker_ontologies_get_classes          (guint *length);
TrackerProperty ** tracker_ontologies_get_properties       (guint *length);
TrackerProperty *  tracker_ontologies_get_rdf_type         (void);

/* Field mechanics */
void               tracker_ontologies_add_property         (TrackerProperty  *field);
TrackerProperty *  tracker_ontologies_get_property_by_uri  (const gchar      *uri);
void               tracker_ontologies_add_namespace        (TrackerNamespace *namespace_);
void               tracker_ontologies_add_ontology         (TrackerOntology  *ontology);
TrackerNamespace * tracker_ontologies_get_namespace_by_uri (const gchar      *namespace_uri);
TrackerOntology  * tracker_ontologies_get_ontology_by_uri  (const gchar      *namespace_uri);
const gchar*       tracker_ontologies_get_uri_by_id        (gint              id);
void               tracker_ontologies_add_id_uri_pair      (gint              id,
                                                            const gchar      *uri);

G_END_DECLS

#endif /* __LIBTRACKER_DATA_ONTOLOGY_H__ */
