/* Tracker
 * routines for cacheing 
 * Copyright (C) 2007, Jamie McCracken 
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

#ifndef _TRACKER_CACHE_H_
#define _TRACKER_CACHE_H_

#include "tracker-db-sqlite.h"
#include "tracker-indexer.h"


void		tracker_cache_add 		(const gchar *word, guint32 service_id, gint service_type, gint score, gboolean is_new);
void            tracker_cache_flush_all       	();
gboolean	tracker_cache_process_events	(DBConnection *db_con, gboolean check_flush);

#endif
