/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2007, Jason Kivlighn (jkivlighn@gmail.com)
 * Copyright (C) 2007, Creative Commons (http://creativecommons.org)
 * Copyright (C) 2008, Nokia
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

#ifndef __TRACKER_DATA_QUERY_H__
#define __TRACKER_DATA_QUERY_H__

#include <glib.h>

#include <libtracker-common/tracker-language.h>
#include <libtracker-common/tracker-ontology.h>
#include <libtracker-common/tracker-property.h>

#include <libtracker-db/tracker-db-interface.h>

G_BEGIN_DECLS

gboolean             tracker_data_query_resource_exists  (const gchar   *uri,
                                                          guint32       *resource_id);
guint32              tracker_data_query_resource_id      (const gchar   *uri);
TrackerDBResultSet  *tracker_data_query_sparql           (const gchar   *query,
                                                          GError       **error);
GPtrArray*           tracker_data_query_rdf_type         (guint32        id);

G_END_DECLS

#endif /* __TRACKER_DATA_QUERY_H__ */
