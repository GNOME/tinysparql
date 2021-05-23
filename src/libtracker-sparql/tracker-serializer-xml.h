/*
 * Copyright (C) 2020, Red Hat, Inc
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

#ifndef TRACKER_SERIALIZER_XML_H
#define TRACKER_SERIALIZER_XML_H

#include <libtracker-sparql/tracker-sparql.h>
#include <libtracker-sparql/tracker-private.h>
#include <libtracker-sparql/tracker-serializer.h>

#define TRACKER_TYPE_SERIALIZER_XML (tracker_serializer_xml_get_type())

G_DECLARE_FINAL_TYPE (TrackerSerializerXml,
                      tracker_serializer_xml,
                      TRACKER, SERIALIZER_XML,
                      TrackerSerializer)

#endif /* TRACKER_SERIALIZER_XML_H */
