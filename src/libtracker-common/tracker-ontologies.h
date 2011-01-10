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

#ifndef __LIBTRACKER_COMMON_ONTOLOGIES_H__
#define __LIBTRACKER_COMMON_ONTOLOGIES_H__

G_BEGIN_DECLS

#if !defined (__LIBTRACKER_COMMON_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "only <libtracker-common/tracker-common.h> must be included directly."
#endif

/* Core ontologies */
#define TRACKER_RDF_PREFIX      "http://www.w3.org/1999/02/22-rdf-syntax-ns#"
#define TRACKER_RDFS_PREFIX     "http://www.w3.org/2000/01/rdf-schema#"
#define TRACKER_XSD_PREFIX      "http://www.w3.org/2001/XMLSchema#"
#define TRACKER_TRACKER_PREFIX  "http://www.tracker-project.org/ontologies/tracker#"
#define TRACKER_DC_PREFIX       "http://purl.org/dc/elements/1.1/"
#define TRACKER_MAEMO_PREFIX    "http://maemo.org/ontologies/tracker#"

/* Our Nepomuk selection */
#define TRACKER_NRL_PREFIX      "http://www.semanticdesktop.org/ontologies/2007/08/15/nrl#"
#define TRACKER_NMO_PREFIX      "http://www.semanticdesktop.org/ontologies/2007/03/22/nmo#"
#define TRACKER_NIE_PREFIX      "http://www.semanticdesktop.org/ontologies/2007/01/19/nie#"
#define TRACKER_NCO_PREFIX      "http://www.semanticdesktop.org/ontologies/2007/03/22/nco#"
#define TRACKER_NAO_PREFIX      "http://www.semanticdesktop.org/ontologies/2007/08/15/nao#"
#define TRACKER_NID3_PREFIX     "http://www.semanticdesktop.org/ontologies/2007/05/10/nid3#"
#define TRACKER_NFO_PREFIX      "http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#"

/* Temporary */
#define TRACKER_SLO_PREFIX      "http://www.tracker-project.org/temp/slo#"
#define TRACKER_NMM_PREFIX      "http://www.tracker-project.org/temp/nmm#"
#define TRACKER_MLO_PREFIX      "http://www.tracker-project.org/temp/mlo#"

#define TRACKER_DATASOURCE_URN_PREFIX	  \
	"urn:nepomuk:datasource:"
#define TRACKER_NON_REMOVABLE_MEDIA_DATASOURCE_URN	  \
	TRACKER_DATASOURCE_URN_PREFIX "9291a450-1d49-11de-8c30-0800200c9a66"

G_END_DECLS

#endif /* __LIBTRACKER_COMMON_ONTOLOGY_H__ */
