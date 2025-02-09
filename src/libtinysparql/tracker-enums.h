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

#if !defined (__LIBTRACKER_SPARQL_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "only <tinysparql.h> must be included directly."
#endif

/**
 * TrackerRdfFormat:
 * @TRACKER_RDF_FORMAT_TURTLE: Turtle format
 *   ([http://www.w3.org/ns/formats/Turtle](http://www.w3.org/ns/formats/Turtle))
 * @TRACKER_RDF_FORMAT_TRIG: Trig format
 *   ([http://www.w3.org/ns/formats/Trig](http://www.w3.org/ns/formats/Trig))
 * @TRACKER_RDF_FORMAT_JSON_LD: JSON-LD format
 *   ([http://www.w3.org/ns/formats/JSON-LD](http://www.w3.org/ns/formats/JSON-LD)).
 *   This value was added in version 3.5.
 * @TRACKER_RDF_FORMAT_LAST: The total number of RDF formats
 *
 * Describes a RDF format to be used in data exchange.
 */
typedef enum {
	TRACKER_RDF_FORMAT_TURTLE,
	TRACKER_RDF_FORMAT_TRIG,
	TRACKER_RDF_FORMAT_JSON_LD,
	TRACKER_RDF_FORMAT_LAST
} TrackerRdfFormat;

#define TRACKER_N_RDF_FORMATS TRACKER_RDF_FORMAT_LAST
