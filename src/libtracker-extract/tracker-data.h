/*
 * Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
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

#ifndef __LIBTRACKER_EXTRACT_DATA_H__
#define __LIBTRACKER_EXTRACT_DATA_H__

#if !defined (__LIBTRACKER_EXTRACT_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "only <libtracker-extract/tracker-extract.h> must be included directly."
#endif

#include <libtracker-sparql/tracker-resource.h>
#include <libtracker-sparql/tracker-sparql.h>
#include "tracker-module-manager.h"
#include "tracker-extract-info.h"

G_BEGIN_DECLS

/**
 * SECTION:tracker-extract
 * @title: libtracker-extract
 * @stability: Unstable
 * @include: libtracker-extract/tracker-extract.h
 *
 * The libtracker-extract library is an internal, private library for Tracker
 * metadata extraction modules. Third party applications should not link
 * against libtracker-extract, because its API may change at any time.
 *
 * If you want to insert metadata into the Tracker store from a 3rd party
 * application, look at the TrackerDecorator class from libtracker-miner.
 */

gboolean tracker_extract_module_init     (GError **error);
gboolean tracker_extract_module_shutdown (void);

/**
 * tracker_extract_get_metadata:
 * @info: a #TrackerExtractInfo object
 *
 * This function must be provided by ALL extractors. This is merely
 * the declaration of the function which must be written by each
 * extractor.
 *
 * This is checked by tracker-extract by looking up the symbols for
 * each started plugin and making sure this function exists.
 *
 * The @info parameter contains required information for the
 * extraction and a location to store the results. The
 * tracker_extract_info_get_metadata_builder() function returns a
 * #TrackerSparqlBuilder constructed through
 * tracker_sparql_builder_new_embedded_insert(). The subject
 * is already set to be the file URN, so implementations of this
 * function should just provide predicate/object(s) pairs. The
 * triples contained in this object at the end of the function will be
 * merged with further file information from miners.
 *
 * Whenever any of the inserted triples rely on entities that
 * should also be provided by this extractor (for example, album
 * or artist information from a song), such insertions should be
 * added to the preupdate object returned by
 * tracker_extract_info_get_preupdate_builder(). This is a
 * #TrackerSparqlBuilder created through
 * tracker_sparql_builder_new_update().
 *
 * NOTE: If upgrading from 0.10, this function replaces the old
 * function named tracker_extract_get_data() and has a few subtle
 * differences. First, there is a return value for success and the
 * parameters are contained in @info instead of being passed
 * individually. Second, the extractor is passed the detected
 * MIME type of the file being extracted.
 *
 * Returns: %TRUE if the extraction succeeded, %FALSE otherwise.
 *
 * Since: 0.12
 */
gboolean tracker_extract_get_metadata (TrackerExtractInfo *info);

G_END_DECLS

#endif /* __LIBTRACKER_EXTRACT_DATA_H__ */
