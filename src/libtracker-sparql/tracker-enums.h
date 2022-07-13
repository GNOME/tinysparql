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

#ifndef TRACKER_ENUMS_H
#define TRACKER_ENUMS_H

/**
 * TrackerRdfFormat:
 * @TRACKER_RDF_FORMAT_TURTLE: Turtle format
 *   ([http://www.w3.org/ns/formats/Turtle](http://www.w3.org/ns/formats/Turtle))
 * @TRACKER_RDF_FORMAT_TRIG: Trig format
 *   ([http://www.w3.org/ns/formats/Trig](http://www.w3.org/ns/formats/Trig))
 * @TRACKER_N_RDF_FORMATS: The total number of RDF formats
 *
 * Describes a RDF format to be used in data exchange.
 */
typedef enum {
	TRACKER_RDF_FORMAT_TURTLE,
	TRACKER_RDF_FORMAT_TRIG,
	TRACKER_N_RDF_FORMATS
} TrackerRdfFormat;

#endif /* TRACKER_ENUMS_H */
