/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008, Nokia
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 * Author: Philip Van Hoof <philip@codeminded.be>
 */

#ifndef __TRACKER_TURTLE_H__
#define __TRACKER_TURTLE_H__

#include <stdio.h>

#include <libtracker-data/tracker-data-metadata.h>

#ifdef HAVE_RAPTOR
#include <raptor.h>
#endif

/*
 * TODO: Is it possible to do this in the .c file? Dont expose raptor here. 
*/

#ifdef HAVE_RAPTOR
typedef raptor_statement TrackerRaptorStatement;
#else
typedef struct {
  gconstpointer subject;
  gint subject_type;
  gconstpointer predicate;
  gint predicate_type;
  gconstpointer object;
  gint object_type;
  gpointer object_literal_datatype;
  const guchar *object_literal_language;
} TrackerRaptorStatement;
#endif

G_BEGIN_DECLS

typedef void (* TurtleTripleCallback) (void *user_data, const TrackerRaptorStatement *triple);

typedef struct TurtleFile TurtleFile;



typedef struct {
	gchar                *about_uri;
	TrackerDataMetadata  *metadata;
	TurtleFile           *turtle; /* For internal use only */
} TrackerTurtleMetadataItem;

/* Initialization (use in both cases) */
void        tracker_turtle_init            (void);
void        tracker_turtle_shutdown        (void);

/* Transactions style */
TurtleFile *tracker_turtle_open            (const gchar         *turtle_file);
void        tracker_turtle_add_triple      (TurtleFile          *turtle,
					    const gchar         *uri,
					    TrackerField        *property,
					    const gchar         *value);
void        tracker_turtle_add_metadata    (TurtleFile          *turtle,
					    const gchar         *uri,
					    TrackerDataMetadata *metadata);

/* @metadata_items contains TrackerTurtleMetadataItem instances */
void        tracker_turtle_add_metadatas   (TurtleFile          *turtle,
					    GPtrArray           *metadata_items);
void        tracker_turtle_close           (TurtleFile          *turtle);

/* Reading functions */
void        tracker_turtle_process         (const gchar          *turtle_file,
					    const gchar          *base_uri,
					    TurtleTripleCallback  callback,
					    void                 *user_data);
					    

/* Optimizer, reparser */
void        tracker_turtle_optimize        (const gchar          *turtle_file);

G_END_DECLS

#endif /* __TRACKER_TURTLE_H__ */
