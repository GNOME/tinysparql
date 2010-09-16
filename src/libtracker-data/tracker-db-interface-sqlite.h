/*
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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

#ifndef __LIBTRACKER_DB_INTERFACE_SQLITE_H__
#define __LIBTRACKER_DB_INTERFACE_SQLITE_H__

#include "config.h"

#include "tracker-db-interface.h"

G_BEGIN_DECLS

#if !defined (__LIBTRACKER_DATA_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "only <libtracker-data/tracker-data.h> must be included directly."
#endif

#define TRACKER_COLLATION_NAME "TRACKER"

TrackerDBInterface *tracker_db_interface_sqlite_new                    (const gchar              *filename);
TrackerDBInterface *tracker_db_interface_sqlite_new_ro                 (const gchar              *filename);
gint64              tracker_db_interface_sqlite_get_last_insert_id     (TrackerDBInterface       *interface);
void                tracker_db_interface_sqlite_enable_shared_cache    (void);
void                tracker_db_interface_sqlite_fts_init               (TrackerDBInterface       *interface,
                                                                        gboolean                  create);

#if HAVE_TRACKER_FTS
int                 tracker_db_interface_sqlite_fts_update_init        (TrackerDBInterface       *interface,
                                                                        int                       id);
int                 tracker_db_interface_sqlite_fts_update_text        (TrackerDBInterface       *interface,
                                                                        int                       id,
                                                                        int                       column_id,
                                                                        const char               *text,
                                                                        gboolean                  limit_word_length);
void                tracker_db_interface_sqlite_fts_update_commit      (TrackerDBInterface       *interface);
void                tracker_db_interface_sqlite_fts_update_rollback    (TrackerDBInterface       *interface);
#endif

G_END_DECLS

#endif /* __LIBTRACKER_DB_INTERFACE_SQLITE_H__ */
