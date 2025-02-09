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

G_BEGIN_DECLS

#if !defined (__LIBTRACKER_SPARQL_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "only <tinysparql.h> must be included directly."
#endif

/* Core: resources, data types */

/**
 * TRACKER_PREFIX_RDF:
 *
 * The Prefix of the RDF namespace
 */
#define TRACKER_PREFIX_RDF      "http://www.w3.org/1999/02/22-rdf-syntax-ns#"

/**
 * TRACKER_PREFIX_RDFS:
 *
 * The Prefix of the RDFS namespace
 */
#define TRACKER_PREFIX_RDFS     "http://www.w3.org/2000/01/rdf-schema#"

/**
 * TRACKER_PREFIX_XSD:
 *
 * The Prefix of the XSD namespace
 */
#define TRACKER_PREFIX_XSD      "http://www.w3.org/2001/XMLSchema#"

/**
 * TRACKER_PREFIX_TRACKER:
 *
 * The Prefix of the Tracker namespace
 */
#define TRACKER_PREFIX_TRACKER  "http://tracker.api.gnome.org/ontology/v3/tracker#"

/**
 * TRACKER_PREFIX_DC:
 *
 * The Prefix of the DC (Dublin Core) namespace
 */
#define TRACKER_PREFIX_DC       "http://purl.org/dc/elements/1.1/"

/* Our Nepomuk selection */

/**
 * TRACKER_PREFIX_NRL:
 *
 * The Prefix of the NRL namespace
 */
#define TRACKER_PREFIX_NRL      "http://tracker.api.gnome.org/ontology/v3/nrl#"

/**
 * TRACKER_PREFIX_NIE:
 *
 * The Prefix of the NIE namespace
 */
#define TRACKER_PREFIX_NIE      "http://tracker.api.gnome.org/ontology/v3/nie#"

/**
 * TRACKER_PREFIX_NCO:
 *
 * The Prefix of the NCO namespace
 */
#define TRACKER_PREFIX_NCO      "http://tracker.api.gnome.org/ontology/v3/nco#"

/**
 * TRACKER_PREFIX_NAO:
 *
 * The Prefix of the NAO namespace
 */
#define TRACKER_PREFIX_NAO      "http://tracker.api.gnome.org/ontology/v3/nao#"

/**
 * TRACKER_PREFIX_NFO:
 *
 * The Prefix of the NFO namespace
 */
#define TRACKER_PREFIX_NFO      "http://tracker.api.gnome.org/ontology/v3/nfo#"

/**
 * TRACKER_PREFIX_NMM:
 *
 * The Prefix of the RDF namespace
 */
#define TRACKER_PREFIX_NMM      "http://tracker.api.gnome.org/ontology/v3/nmm#"

/* Additional ontologies used by tracker-miners */

/**
 * TRACKER_PREFIX_MFO:
 *
 * The Prefix of the MFO namespace
 */
#define TRACKER_PREFIX_MFO      "http://tracker.api.gnome.org/ontology/v3/mfo#"

/**
 * TRACKER_PREFIX_SLO:
 *
 * The Prefix of the SLO namespace
 */
#define TRACKER_PREFIX_SLO      "http://tracker.api.gnome.org/ontology/v3/slo#"

/**
 * TRACKER_PREFIX_OSINFO:
 *
 * The Prefix of the Osinfo namespace
 */
#define TRACKER_PREFIX_OSINFO   "http://tracker.api.gnome.org/ontology/v3/osinfo#"

G_END_DECLS
