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

#pragma once

#include "tracker-serializer.h"

#include <tinysparql.h>

#define TRACKER_TYPE_DESERIALIZER (tracker_deserializer_get_type())

G_DECLARE_DERIVABLE_TYPE (TrackerDeserializer,
                          tracker_deserializer,
                          TRACKER, DESERIALIZER,
                          TrackerSparqlCursor)

TrackerSparqlCursor * tracker_deserializer_new (GInputStream            *stream,
                                                TrackerNamespaceManager *manager,
                                                TrackerSerializerFormat  format);
TrackerSparqlCursor * tracker_deserializer_new_for_file (GFile                    *file,
                                                         TrackerNamespaceManager  *manager,
                                                         GError                  **error);

gboolean tracker_deserializer_get_parser_location (TrackerDeserializer *deserializer,
                                                   goffset             *line_no,
                                                   goffset             *column_no);
GInputStream * tracker_deserializer_get_stream (TrackerDeserializer *deserializer);

TrackerNamespaceManager * tracker_deserializer_get_namespaces (TrackerDeserializer *deserializer);
