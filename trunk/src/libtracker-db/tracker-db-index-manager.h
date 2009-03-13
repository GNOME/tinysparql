/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
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

#ifndef __TRACKERD_INDEX_MANAGER_H__
#define __TRACKERD_INDEX_MANAGER_H__

#include <glib.h>

G_BEGIN_DECLS

#include "tracker-db-index.h"

typedef enum {
	TRACKER_DB_INDEX_UNKNOWN,
	TRACKER_DB_INDEX_FILE,
	TRACKER_DB_INDEX_FILE_UPDATE,
	TRACKER_DB_INDEX_EMAIL
} TrackerDBIndexType;

typedef enum {
	TRACKER_DB_INDEX_MANAGER_FORCE_REINDEX = 1 << 1,
	TRACKER_DB_INDEX_MANAGER_READONLY      = 1 << 2
} TrackerDBIndexManagerFlags;

gboolean	tracker_db_index_manager_init			 (TrackerDBIndexManagerFlags  flags,
								  gint			      min_bucket,
								  gint			      max_bucket);
void		tracker_db_index_manager_shutdown		 (void);
TrackerDBIndex *tracker_db_index_manager_get_index		 (TrackerDBIndexType	      index);
TrackerDBIndex *tracker_db_index_manager_get_index_by_service	 (const gchar		     *service);
TrackerDBIndex *tracker_db_index_manager_get_index_by_service_id (gint			      id);
const gchar *	tracker_db_index_manager_get_filename		 (TrackerDBIndexType	      index);
gboolean	tracker_db_index_manager_are_indexes_too_big	 (void);

G_END_DECLS

#endif /* __TRACKERD_INDEX_MANAGER_H__ */
