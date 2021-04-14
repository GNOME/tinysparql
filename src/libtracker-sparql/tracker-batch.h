/*
 * Copyright (C) 2020 Red Hat Ltd
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
 * Author: Carlos Garnacho
 */
#ifndef __TRACKER_BATCH_H__
#define __TRACKER_BATCH_H__

#if !defined (__LIBTRACKER_SPARQL_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "only <libtracker-sparql/tracker-sparql.h> must be included directly."
#endif

#include <libtracker-sparql/tracker-version.h>
#include <libtracker-sparql/tracker-resource.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_BATCH tracker_batch_get_type ()

TRACKER_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (TrackerBatch,
                          tracker_batch,
                          TRACKER, BATCH,
                          GObject)

#include "tracker-connection.h"

TRACKER_AVAILABLE_IN_3_1
TrackerSparqlConnection * tracker_batch_get_connection (TrackerBatch *batch);

TRACKER_AVAILABLE_IN_3_1
void tracker_batch_add_sparql (TrackerBatch *batch,
                               const gchar  *sparql);

TRACKER_AVAILABLE_IN_3_1
void tracker_batch_add_resource (TrackerBatch    *batch,
                                 const gchar     *graph,
                                 TrackerResource *resource);

TRACKER_AVAILABLE_IN_3_1
gboolean tracker_batch_execute (TrackerBatch  *batch,
                                GCancellable  *cancellable,
                                GError       **error);

TRACKER_AVAILABLE_IN_3_1
void tracker_batch_execute_async (TrackerBatch        *batch,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data);

TRACKER_AVAILABLE_IN_3_1
gboolean tracker_batch_execute_finish (TrackerBatch  *batch,
                                       GAsyncResult  *res,
                                       GError       **error);

G_END_DECLS

#endif /* __TRACKER_BATCH_H__ */
