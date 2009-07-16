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

#include <libtracker-common/tracker-common.h>

#include <raptor.h>

G_BEGIN_DECLS

typedef void (* TurtleTripleCallback) (const gchar *subject, const gchar *predicate, const gchar *object, void *user_data);

typedef struct TurtleFile TurtleFile;

/* Initialization (use in both cases) */
void        tracker_turtle_init          (void);
void        tracker_turtle_shutdown      (void);

/* Transactions style */
TurtleFile *tracker_turtle_open            (const gchar         *turtle_file);
void        tracker_turtle_add_triple      (TurtleFile          *turtle,
					    const gchar         *uri,
					    TrackerProperty        *property,
					    const gchar         *value);
void        tracker_turtle_close           (TurtleFile          *turtle);

/* Reading functions */
void        tracker_turtle_process         (const gchar          *turtle_file,
					    const gchar          *base_uri,
					    TurtleTripleCallback  callback,
					    void                 *user_data);

void         tracker_turtle_reader_init          (const gchar    *turtle_file,
					          const gchar    *base_uri);
gboolean     tracker_turtle_reader_next          (void);
const gchar *tracker_turtle_reader_get_subject   (void);
const gchar *tracker_turtle_reader_get_predicate (void);
const gchar *tracker_turtle_reader_get_object    (void);
gboolean     tracker_turtle_reader_object_is_uri (void);
void         tracker_turtle_reader_cancel        (void);
GError*      tracker_turtle_get_error           (void);

/* Optimizer, reparser */
void        tracker_turtle_optimize      (const gchar          *turtle_file);

G_END_DECLS

#endif /* __TRACKER_TURTLE_H__ */
