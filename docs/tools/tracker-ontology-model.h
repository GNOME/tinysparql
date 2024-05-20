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

#pragma once

#include <glib.h>

G_BEGIN_DECLS

typedef struct _TrackerOntologyModel TrackerOntologyModel;

typedef struct {
	gchar *classname;
	gchar *shortname;
	gchar *basename;
	gchar *specification;
	GList *superclasses;
	GList *subclasses;
	GList *in_domain_of;
	GList *in_range_of;
	gchar *description;
	GList *instances;
	gboolean notify;
	gboolean deprecated;
} TrackerOntologyClass;

typedef struct {
	gchar *propertyname;
	gchar *shortname;
	gchar *basename;
	gchar *specification;
	GList *domain;
	GList *range;
	GList *superproperties;
	GList *subproperties;
	gchar *max_cardinality;
	gchar *description;
	gboolean deprecated;
        gboolean fulltextIndexed;
        gchar *weight;
} TrackerOntologyProperty;

typedef struct {
	gchar *title;
        gchar *description;
	GList *authors;
	GList *editors;
	GList *contributors;
	gchar *gitlog;
	gchar *upstream;
	gchar *copyright;
	gchar *baseUrl;
	gchar *localPrefix;
	gchar *relativePath;
} TrackerOntologyDescription;

TrackerOntologyModel * tracker_ontology_model_new (GFile   *ontology_location,
                                                   GFile   *description_location,
                                                   GError **error);
void tracker_ontology_model_free (TrackerOntologyModel *model);

GStrv tracker_ontology_model_get_prefixes (TrackerOntologyModel *model);

TrackerOntologyDescription * tracker_ontology_model_get_description (TrackerOntologyModel *model,
                                                                     const gchar          *prefix);

GList * tracker_ontology_model_list_classes (TrackerOntologyModel *model,
                                             const gchar          *prefix);
GList * tracker_ontology_model_list_properties (TrackerOntologyModel *model,
                                                const gchar          *prefix);

TrackerOntologyClass * tracker_ontology_model_get_class (TrackerOntologyModel *model,
                                                         const gchar          *class_name);
TrackerOntologyProperty * tracker_ontology_model_get_property (TrackerOntologyModel *model,
                                                               const gchar          *prop_name);

G_END_DECLS
