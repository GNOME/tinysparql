/*
 * Copyright (C) 2017, Red Hat, Inc.
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
 * Authors: Carlos Garnacho <carlosg@gnome.org>
 */

#ifndef __TRACKER_DOMAIN_ONTOLOGY_H__
#define __TRACKER_DOMAIN_ONTOLOGY_H__

#if !defined (__LIBTRACKER_COMMON_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "only <libtracker-common/tracker-common.h> must be included directly."
#endif

#include <glib-object.h>
#include <gio/gio.h>

#define TRACKER_TYPE_DOMAIN_ONTOLOGY         (tracker_domain_ontology_get_type())
#define TRACKER_DOMAIN_ONTOLOGY(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_DOMAIN_ONTOLOGY, TrackerDomainOntology))
#define TRACKER_DOMAIN_ONTOLOGY_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    TRACKER_TYPE_DOMAIN_ONTOLOGY, TrackerDomainOntologyClass))
#define TRACKER_IS_DOMAIN_ONTOLOGY(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_DOMAIN_ONTOLOGY))
#define TRACKER_IS_DOMAIN_ONTOLOGY_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    TRACKER_TYPE_DOMAIN_ONTOLOGY))
#define TRACKER_DOMAIN_ONTOLOGY_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  TRACKER_TYPE_DOMAIN_ONTOLOGY, TrackerDomainOntologyClass))

typedef struct _TrackerDomainOntology TrackerDomainOntology;
typedef struct _TrackerDomainOntologyClass TrackerDomainOntologyClass;

struct _TrackerDomainOntology {
	GObject parent_instance;
};

struct _TrackerDomainOntologyClass {
	GObjectClass parent_class;
	/*<private>*/
	gpointer padding[10];
};

GType                   tracker_domain_ontology_get_type (void) G_GNUC_CONST;

TrackerDomainOntology * tracker_domain_ontology_new      (const gchar   *name,
                                                          GCancellable  *cancellable,
                                                          GError       **error);

GFile * tracker_domain_ontology_get_cache    (TrackerDomainOntology *domain_ontology);
GFile * tracker_domain_ontology_get_journal  (TrackerDomainOntology *domain_ontology);
GFile * tracker_domain_ontology_get_ontology (TrackerDomainOntology *domain_ontology);

gchar * tracker_domain_ontology_get_domain   (TrackerDomainOntology *domain_ontology,
                                              const gchar           *suffix);

gboolean tracker_domain_ontology_uses_miner  (TrackerDomainOntology *domain_ontology,
                                              const gchar           *suffix);


#endif /* __TRACKER_MINER_PROXY_H__ */
