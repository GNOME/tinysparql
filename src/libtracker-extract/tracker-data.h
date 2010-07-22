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

#include <libtracker-sparql/tracker-sparql.h>

G_BEGIN_DECLS

/**
 * SECTION:tracker-data
 * @title: How to use libtracker-extract
 * @short_description: The essentials by example
 * @stability: Stable
 * @include: libtracker-extract/tracker-extract.h
 *
 * The libtracker-extract library is the foundation for Tracker
 * metadata extraction of embedded data in files.
 *
 * Tracker comes with extractors written for the most common file
 * types (like MP3, JPEG, PNG, etc.), however, for more special cases,
 * 3rd party applications may want to write their own plugin to
 * extract their own file formats. This documentation describes how to
 * do that.
 *
 * <example>
 * <title>Basic extractor example</title>
 * An example of how to write an extractor to retrieve PNG embedded
 * metadata.
 * <programlisting>
 *  static void extract_png (const gchar          *filename,
 *                           TrackerSparqlBuilder *preupdate,
 *                           TrackerSparqlBuilder *metadata);
 *
 *  /&ast; Set functions to use to extract different mime types. &ast;/
 *  static TrackerExtractData extract_data[] = {
 *          { "image/png",  extract_png },
 *          { "sketch/png", extract_png },
 *          { NULL, NULL }
 *  };
 *
 *  static void
 *  extract_png (const gchar          *uri,
 *               TrackerSparqlBuilder *preupdate,
 *               TrackerSparqlBuilder *metadata)
 *  {
 *          gint height, width;
 *
 *          /&ast; Do data extraction. &ast;/
 *          height = ...
 *          width = ...
 *
 *          /&ast; Insert data into TrackerSparqlBuilder object. &ast;/
 *          tracker_sparql_builder_predicate (metadata, "a");
 *          tracker_sparql_builder_object (metadata, "nfo:Image");
 *          tracker_sparql_builder_object (metadata, "nmm:Photo");
 *
 *          tracker_sparql_builder_predicate (metadata, "nfo:width");
 *          tracker_sparql_builder_object_int64 (metadata, width);
 *
 *          tracker_sparql_builder_predicate (metadata, "nfo:height");
 *          tracker_sparql_builder_object_int64 (metadata, height);
 *
 *          g_free (filename);
 *  }
 *
 *  TrackerExtractData *
 *  tracker_extract_get_data (void)
 *  {
 *          return extract_data;
 *  }
 * </programlisting>
 * </example>
 *
 */


/**
 * TrackerExtractMimeFunc:
 * @uri: a string representing a URI.
 * @preupdate: used to populate with data updates that
 *             are a prerequisite for the actual file
 *             metadata insertion.
 * @metadata: used to populate with file metadata predicate/object(s).
 *
 * Extracts metadata from a file, and inserts it into @metadata.
 *
 * The @metadata parameter is a #TrackerSparqlBuilder constructed
 * through tracker_sparql_builder_new_embedded_insert(), the subject
 * is already set to be the file URN, so implementations of this
 * function should just provide predicate/object(s) pairs. the data
 * triples contained in there at the end of the function will be
 * merged with further file information from miners.
 *
 * Whenever any of the inserted triples rely on entities that
 * should also be provided by this extractor (for example, album
 * or artist information from a song), such insertions should be
 * added to @preupdate, which is a #TrackerSparqlBuilder constructed.
 * through tracker_sparql_builder_new_update().
 *
 * Since: 0.8
 **/
typedef void (*TrackerExtractMimeFunc) (const gchar          *uri,
                                        TrackerSparqlBuilder *preupdate,
                                        TrackerSparqlBuilder *metadata);

/**
 * TrackerExtractData:
 * @mime: a string pointer representing a mime type.
 * @func: a function to extract extract the data in.
 *
 * The @mime is usually in the format of "image/png" for example.

 * The @func is called by tracker-extract if an extractor plugin
 * matches the @mime.
 *
 * Since: 0.8
 **/
typedef struct {
	const gchar *mime;
	TrackerExtractMimeFunc func;
} TrackerExtractData;

/**
 * TrackerExtractDataFunc:
 *
 * This function is used by by tracker-extract to call into each
 * extractor to get a list of mime type and TrackerExtractMimeFunc
 * combinations.
 *
 * Returns: an array of #TrackerExtractData which must be NULL
 * terminated and must NOT be freed.
 *
 * Since: 0.6
 **/
typedef TrackerExtractData * (*TrackerExtractDataFunc) (void);

/**
 * tracker_extract_get_data:
 *
 *
 * This function must be provided by ALL extractors. This is merely
 * the declaration of the function which must be written by each
 * extractor. 
 * 
 * This is checked by tracker-extract by looking up the symbols for
 * each plugin and making sure this function exists. This is only
 * called by tracker-extract if a mime type in any of the
 * #TrackerExtractData structures returned matches the mime type of
 * the file being handled.
 *
 * Returns: a #TrackerExtractData pointer which should not be freed.
 * This pointer can be an array of #TrackerExtractData structures
 * where multiple mime types are supported.
 *
 * Since: 0.8
 */
TrackerExtractData *tracker_extract_get_data (void);

G_END_DECLS

#endif /* __LIBTRACKER_EXTRACT_DATA_H__ */
