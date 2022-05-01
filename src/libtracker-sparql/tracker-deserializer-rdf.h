/*
 * Copyright (C) 2022, Red Hat, Inc
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

#ifndef TRACKER_DESERIALIZER_RDF_H
#define TRACKER_DESERIALIZER_RDF_H

#include <libtracker-sparql/tracker-deserializer.h>

#include "tracker-private.h"

typedef enum {
	TRACKER_RDF_COL_SUBJECT,
	TRACKER_RDF_COL_PREDICATE,
	TRACKER_RDF_COL_OBJECT,
	TRACKER_RDF_COL_GRAPH,
	TRACKER_RDF_N_COLS
} TrackerRdfColumn;

#define TRACKER_TYPE_DESERIALIZER_RDF (tracker_deserializer_rdf_get_type())

struct _TrackerDeserializerRdfClass {
	TrackerDeserializerClass parent_class;
};

G_DECLARE_DERIVABLE_TYPE (TrackerDeserializerRdf,
                          tracker_deserializer_rdf,
                          TRACKER, DESERIALIZER_RDF,
                          TrackerDeserializer)

#endif /* TRACKER_DESERIALIZER_RDF_H */
