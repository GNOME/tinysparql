/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2007, Jason Kivlighn (jkivlighn@gmail.com)
 * Copyright (C) 2007, Creative Commons (http://creativecommons.org)
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
 */

#ifndef __TRACKER_DATA_LIVE_SEARCH_H__
#define __TRACKER_DATA_LIVE_SEARCH_H__

#include <glib.h>

#include <libtracker-db/tracker-db-interface.h>

G_BEGIN_DECLS

void                tracker_data_live_search_start           (TrackerDBInterface *iface,
							      const gchar        *from_query,
							      const gchar        *join_query,
							      const gchar        *where_query,
							      const gchar        *search_id);
void                tracker_data_live_search_stop            (TrackerDBInterface *iface,
							      const gchar        *search_id);
TrackerDBResultSet *tracker_data_live_search_get_ids         (TrackerDBInterface *iface,
							      const gchar        *search_id);
TrackerDBResultSet *tracker_data_live_search_get_new_ids     (TrackerDBInterface *iface,
							      const gchar        *search_id,
							      const gchar        *from_query,
							      const gchar        *query_joins,
							      const gchar        *where_query);
TrackerDBResultSet *tracker_data_live_search_get_deleted_ids (TrackerDBInterface *iface,
							      const gchar        *search_id);
TrackerDBResultSet *tracker_data_live_search_get_hit_data    (TrackerDBInterface *iface,
							      const gchar        *search_id,
							      GStrv               fields);
TrackerDBResultSet *tracker_data_live_search_get_hit_count   (TrackerDBInterface *iface,
							      const gchar        *search_id);

G_END_DECLS

#endif /* __TRACKER_DATA_LIVE_SEARCH_H__ */
