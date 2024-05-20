/*
 * Copyright (C) 2008-2010, Nokia
 * Copyright (C) 2018, Red Hat Inc.
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

#include <glib.h>
#include "tracker-data-manager.h"

#define TRACKER_TYPE_SPARQL (tracker_sparql_get_type ())
G_DECLARE_FINAL_TYPE (TrackerSparql, tracker_sparql,
                      TRACKER, SPARQL, GObject)

TrackerSparql *       tracker_sparql_new (TrackerDataManager  *manager,
                                          const gchar         *sparql,
                                          GError             **error);

gboolean              tracker_sparql_is_serializable (TrackerSparql *sparql);

TrackerSparqlCursor * tracker_sparql_execute_cursor (TrackerSparql  *sparql,
                                                     GHashTable     *parameters,
                                                     GError        **error);

TrackerSparql * tracker_sparql_new_update (TrackerDataManager  *manager,
                                           const gchar         *query,
                                           GError             **error);
gboolean tracker_sparql_execute_update (TrackerSparql  *sparql,
                                        GHashTable     *parameters,
                                        GHashTable     *bnode_map,
                                        GVariant      **bnodes_variant,
                                        GError        **error);
